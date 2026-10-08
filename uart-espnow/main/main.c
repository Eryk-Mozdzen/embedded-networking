#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <driver/uart.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

// #define UART_PERIPHERAL UART_NUM_2
// #define UART_PIN_TX     17
// #define UART_PIN_RX     16

#define UART_PERIPHERAL UART_NUM_0
#define UART_PIN_TX     1
#define UART_PIN_RX     3

static const char *TAG = "app";

static const uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static void espnow_send_cb(const esp_now_send_info_t *send_info, esp_now_send_status_t status) {
    (void)send_info;

    if(status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "ESP NOW send failed");
    }
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    (void)recv_info;

    ESP_LOGI(TAG, "recv %d bytes", len);

    const int result = uart_write_bytes(UART_PERIPHERAL, data, len);
    if(result != len) {
        ESP_LOGW(TAG, "UART send failed: %d", result);
    }
}

static void espnow_task(void *params) {
    (void)params;

    uint8_t buffer[ESP_NOW_MAX_DATA_LEN];

    while(1) {
        const int len = uart_read_bytes(UART_PERIPHERAL, buffer, sizeof(buffer), 1);

        if(len < 0) {
            ESP_LOGW(TAG, "UART recv failed: %d", len);
        }

        if(len > 0) {
            ESP_LOGI(TAG, "send %d bytes", len);

            const esp_err_t err = esp_now_send(broadcast_mac, buffer, len);
            if(err != ESP_OK) {
                ESP_LOGW(TAG, "ESP NOW send failed: %s", esp_err_to_name(err));
            }
        }
    }
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

    ESP_ERROR_CHECK(uart_driver_install(UART_PERIPHERAL, 8192, 8192, 10, NULL, 0));
    const uart_config_t uart_config = {
        .baud_rate = 921600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PERIPHERAL, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PERIPHERAL, UART_PIN_TX, UART_PIN_RX, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G |
                                                           WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)"pmk1234567890123"));
    ESP_ERROR_CHECK(esp_now_set_user_oui(NULL));
    const esp_now_peer_info_t peer_info = {
        .channel = 1,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
        .peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    };
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

    xTaskCreate(espnow_task, "espnow", 8192, NULL, 4, NULL);
}
