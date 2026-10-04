/*
 * ESP-IDF: WiFi STA <-> NAPT <-> PPP server over SPI slave (1024-byte framed transactions)
 */
#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <driver/gpio.h>
#include <driver/spi_slave.h>
#include <esp_event.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_log_buffer.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <netif/ppp/ppp.h>
#include <netif/ppp/pppos.h>

#define STA_SSID     "Electric_Boogaloo_2.4G"
#define STA_PASSWORD "Zwek6ssstedj"

#define PPP_SPI_HOST     SPI3_HOST
#define PPP_SPI_PIN_MOSI 23
#define PPP_SPI_PIN_MISO 19
#define PPP_SPI_PIN_CLK  18
#define PPP_SPI_PIN_CS   5

#define BUFFER_SIZE              (32 * 1024)
#define PPP_SPI_TRANSACTION_SIZE 1024

#define PPP_OUR_IP  192, 168, 11, 1
#define PPP_PEER_IP 192, 168, 11, 2

#define IP4_ADDR_X(ipaddr, ...) IP4_ADDR(ipaddr, __VA_ARGS__)

#define PPP_DOWN_BIT BIT0

static const char *TAG = "app";

static esp_netif_t *s_sta_netif;

static struct {
    uint8_t *transaction_rx;
    uint8_t *transaction_tx;

    QueueHandle_t tx_queue;
    QueueHandle_t rx_queue;
    EventGroupHandle_t event;

    struct netif netif;
    ppp_pcb *ppp;
} ppp_spi;

static void ppp_spi_link(ppp_pcb *pcb, int err_code, void *ctx) {
    (void)pcb;
    (void)ctx;
    if(err_code == PPPERR_NONE) {
        ESP_LOGI(TAG, "PPP up:  our=%s", ip4addr_ntoa(netif_ip4_addr(&ppp_spi.netif)));
        ESP_LOGI(TAG, "        peer=%s", ip4addr_ntoa(netif_ip4_gw(&ppp_spi.netif)));
    } else {
        ESP_LOGW(TAG, "PPP down/err %d", err_code);
        xEventGroupSetBits(ppp_spi.event, PPP_DOWN_BIT);
    }
}

static uint32_t ppp_spi_output(ppp_pcb *pcb, const void *data, u32_t len, void *ctx) {
    (void)pcb;
    (void)ctx;
    // ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, len, ESP_LOG_WARN);
    const uint8_t *buffer = data;
    uint32_t total = 0;
    while(total < len) {
        if(xQueueSend(ppp_spi.tx_queue, &buffer[total], 1)) {
            total++;
        }
    }
    return len;
}

static void ppp_spi_task_transaction(void *arg) {
    (void)arg;
    spi_slave_transaction_t transaction = {
        .length = PPP_SPI_TRANSACTION_SIZE * 8,
        .tx_buffer = ppp_spi.transaction_tx,
        .rx_buffer = ppp_spi.transaction_rx,
    };
    uint32_t len;
    while(1) {
        len = 0;
        while(xQueueReceive(ppp_spi.tx_queue, &ppp_spi.transaction_tx[sizeof(len) + len], 0) &&
              (len < (PPP_SPI_TRANSACTION_SIZE - sizeof(len)))) {
            len++;
        }

        ppp_spi.transaction_tx[0] = (len & 0x000000FF) >> 0;
        ppp_spi.transaction_tx[1] = (len & 0x0000FF00) >> 8;
        ppp_spi.transaction_tx[2] = (len & 0x00FF0000) >> 16;
        ppp_spi.transaction_tx[3] = (len & 0xFF000000) >> 24;

        spi_slave_transmit(PPP_SPI_HOST, &transaction, portMAX_DELAY);

        len = (((uint32_t)ppp_spi.transaction_rx[0]) << 0) |
              (((uint32_t)ppp_spi.transaction_rx[1]) << 8) |
              (((uint32_t)ppp_spi.transaction_rx[2]) << 16) |
              (((uint32_t)ppp_spi.transaction_rx[3]) << 24);

        if(len > (PPP_SPI_TRANSACTION_SIZE - sizeof(len))) {
            len = PPP_SPI_TRANSACTION_SIZE - sizeof(len);
        }

        uint32_t total = 0;
        while(total < len) {
            if(xQueueSend(ppp_spi.rx_queue, &ppp_spi.transaction_rx[sizeof(len) + total],
                          portMAX_DELAY)) {
                total++;
            }
        }
    }
}

static void ppp_spi_task_com(void *arg) {
    (void)arg;
    uint8_t byte;
    while(1) {
        if(xQueueReceive(ppp_spi.rx_queue, &byte, portMAX_DELAY)) {
            pppos_input(ppp_spi.ppp, &byte, 1);
        }
    }
}

static void ppp_spi_task_listen(void *arg) {
    (void)arg;
    ip4_addr_t our;
    ip4_addr_t peer;
    IP4_ADDR_X(&our, PPP_OUR_IP);
    IP4_ADDR_X(&peer, PPP_PEER_IP);
    ppp_set_ipcp_ouraddr(ppp_spi.ppp, &our);
    ppp_set_ipcp_hisaddr(ppp_spi.ppp, &peer);
    ESP_LOGI(TAG, "PPP listening");
    err_t status = ppp_listen(ppp_spi.ppp);
    while(1) {
        xEventGroupWaitBits(ppp_spi.event, PPP_DOWN_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        do {
            vTaskDelay(pdMS_TO_TICKS(500));
            ppp_set_ipcp_ouraddr(ppp_spi.ppp, &our);
            ppp_set_ipcp_hisaddr(ppp_spi.ppp, &peer);
            ESP_LOGI(TAG, "PPP listening again");
            status = ppp_listen(ppp_spi.ppp);
        } while(status != ESP_OK);
    }
}

static void ppp_spi_init() {
    ppp_spi.tx_queue = xQueueCreate(BUFFER_SIZE, 1);
    ppp_spi.rx_queue = xQueueCreate(BUFFER_SIZE, 1);
    ppp_spi.event = xEventGroupCreate();

    ppp_spi.transaction_rx = heap_caps_malloc(PPP_SPI_TRANSACTION_SIZE, MALLOC_CAP_DMA);
    ppp_spi.transaction_tx = heap_caps_malloc(PPP_SPI_TRANSACTION_SIZE, MALLOC_CAP_DMA);

    const spi_bus_config_t buscfg = {
        .mosi_io_num = PPP_SPI_PIN_MOSI,
        .miso_io_num = PPP_SPI_PIN_MISO,
        .sclk_io_num = PPP_SPI_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = PPP_SPI_TRANSACTION_SIZE,
    };

    const spi_slave_interface_config_t slvcfg = {
        .mode = 1,
        .spics_io_num = PPP_SPI_PIN_CS,
        .queue_size = 8,
        .flags = 0,
    };

    spi_slave_initialize(PPP_SPI_HOST, &buscfg, &slvcfg, SPI_DMA_CH1);

    ppp_spi.ppp = pppos_create(&ppp_spi.netif, ppp_spi_output, ppp_spi_link, NULL);
    ESP_ERROR_CHECK(ppp_spi.ppp ? ESP_OK : ESP_FAIL);

    xTaskCreate(ppp_spi_task_transaction, "PPP SPI transaction", 8192, NULL, 7, NULL);
    xTaskCreate(ppp_spi_task_com, "PPP SPI com", 8192, NULL, 6, NULL);
    xTaskCreate(ppp_spi_task_listen, "PPP SPI listen", 8192, NULL, 5, NULL);
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if(base == WIFI_EVENT) {
        if(id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if(id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_event_sta_disconnected_t *e = data;
            ESP_LOGW(TAG, "WiFi disconnected (reason %d), retrying", e->reason);
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_wifi_connect();
        } else if(id == WIFI_EVENT_STA_CONNECTED) {
            ESP_LOGI(TAG, "WiFi connected to %s", STA_SSID);
        }
    } else if(base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "STA got IP " IPSTR ", gw " IPSTR, IP2STR(&e->ip_info.ip),
                 IP2STR(&e->ip_info.gw));
        esp_netif_set_default_netif(s_sta_netif);
    }
}

static void wifi_sta_init() {
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    wifi_config_t wc = {0};
    strlcpy((char *)wc.sta.ssid, STA_SSID, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, STA_PASSWORD, sizeof(wc.sta.password));
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main() {
    esp_err_t r = nvs_flash_init();
    if((r == ESP_ERR_NVS_NO_FREE_PAGES) || (r == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    ESP_ERROR_CHECK(r);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ppp_spi_init();
    wifi_sta_init();
}
