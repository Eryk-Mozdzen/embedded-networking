/*
 * ESP-IDF: WiFi SoftAP <-> NAPT <-> PPP server over UART (921600 8N1)
 */
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "driver/uart.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "iperf.h"
#include "nvs_flash.h"

#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "netif/ppp/pppapi.h"
#include "netif/ppp/pppos.h"

/* ---------------- configuration ---------------- */
#define STA_SSID     "Electric_Boogaloo_2.4G"
#define STA_PASSWORD "Zwek6ssstedj"

#define PPP_UART_NUM    UART_NUM_2
#define PPP_UART_BAUD   921600
#define PPP_UART_TX_PIN 17
#define PPP_UART_RX_PIN 16

#define PPP_OUR_IP  192, 168, 11, 1
#define PPP_PEER_IP 192, 168, 11, 2

#define DHCP_DNS_IP 8, 8, 8, 8 /* DNS offered to WiFi clients */

/* Force expansion of the comma-separated IP macros before the real macro sees them */
#define IP4_ADDR_X(ipaddr, ...) IP4_ADDR(ipaddr, __VA_ARGS__)
#define ESP_IP4TOADDR_X(...)    ESP_IP4TOADDR(__VA_ARGS__)
/* ------------------------------------------------ */

static const char *TAG = "wifi_ppp_nat";

static esp_netif_t *s_sta_netif;
static ppp_pcb *s_ppp;
static struct netif s_ppp_netif;
static EventGroupHandle_t s_ev;
#define PPP_DOWN_BIT BIT0

/* ================= PPP over UART ================= */

static u32_t ppp_output_cb(ppp_pcb *pcb, const void *data, u32_t len, void *ctx) {
    int n = uart_write_bytes(PPP_UART_NUM, data, len);
    return n < 0 ? 0 : (u32_t)n;
}

/* Runs in the tcpip thread: do NOT call pppapi_* from here */
static void ppp_status_cb(ppp_pcb *pcb, int err_code, void *ctx) {
    if(err_code == PPPERR_NONE) {
        ESP_LOGI(TAG, "PPP up:  our=%s", ip4addr_ntoa(netif_ip4_addr(&s_ppp_netif)));
        ESP_LOGI(TAG, "        peer=%s", ip4addr_ntoa(netif_ip4_gw(&s_ppp_netif)));
    } else {
        ESP_LOGW(TAG, "PPP down/err %d", err_code);
        xEventGroupSetBits(s_ev, PPP_DOWN_BIT);
    }
}

static err_t ppp_start_listen(void) {
    ip4_addr_t our, peer;
    IP4_ADDR_X(&our, PPP_OUR_IP);
    IP4_ADDR_X(&peer, PPP_PEER_IP);
    ppp_set_ipcp_ouraddr(s_ppp, &our);
    ppp_set_ipcp_hisaddr(s_ppp, &peer);
    return pppapi_listen(s_ppp);
}

static void uart_rx_task(void *arg) {
    uint8_t *buf = malloc(512);
    for(;;) {
        int n = uart_read_bytes(PPP_UART_NUM, buf, 512, pdMS_TO_TICKS(10));
        if(n > 0 && s_ppp) {
            pppos_input_tcpip(s_ppp, buf, n); /* copies into a pbuf */
        }
    }
}

static void ppp_task(void *arg) {
    for(;;) {
        xEventGroupWaitBits(s_ev, PPP_DOWN_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(500));
        while(ppp_start_listen() != ERR_OK) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        ESP_LOGI(TAG, "PPP listening again");
    }
}

static void ppp_begin(void) {
    uart_config_t uc = {
        .baud_rate = PPP_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(PPP_UART_NUM, 16384, 16384, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(PPP_UART_NUM, &uc));
    ESP_ERROR_CHECK(uart_set_pin(PPP_UART_NUM, PPP_UART_TX_PIN, PPP_UART_RX_PIN, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));

    s_ev = xEventGroupCreate();
    s_ppp = pppapi_pppos_create(&s_ppp_netif, ppp_output_cb, ppp_status_cb, NULL);
    ESP_ERROR_CHECK(s_ppp ? ESP_OK : ESP_FAIL);

    ESP_ERROR_CHECK(ppp_start_listen() == ERR_OK ? ESP_OK : ESP_FAIL);

    xTaskCreate(uart_rx_task, "ppp_rx", 3072, NULL, 12, NULL);
    xTaskCreate(ppp_task, "ppp_ctl", 3072, NULL, 5, NULL);
}

/* ================= WiFi AP + NAPT ================= */

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if(base == WIFI_EVENT) {
        if(id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if(id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_event_sta_disconnected_t *e = data;
            ESP_LOGW(TAG, "WiFi disconnected (reason %d), retrying", e->reason);
            vTaskDelay(pdMS_TO_TICKS(1000)); /* event task, short delay is OK */
            esp_wifi_connect();
        } else if(id == WIFI_EVENT_STA_CONNECTED) {
            ESP_LOGI(TAG, "WiFi connected to %s", STA_SSID);
        }
    } else if(base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "STA got IP " IPSTR ", gw " IPSTR, IP2STR(&e->ip_info.ip),
                 IP2STR(&e->ip_info.gw));
        esp_netif_set_default_netif(s_sta_netif); /* NAT egress = WiFi */
    }
}

static void wifi_sta_init(void) {
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
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK; /* minimum accepted; WPA3 networks also pass */

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* ================= main ================= */

void app_main(void) {
    esp_err_t r = nvs_flash_init();
    if(r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    ESP_ERROR_CHECK(r);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ppp_begin();
    wifi_sta_init();
}
