/* Contiki example main file for ESP32 platform 
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <assert.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "dev/leds.h"
#include "sdkconfig.h"

#include "nvs_flash.h"
#include "freertos/queue.h"
#include "driver/uart.h"

#include "contiki.h"
#include "contiki-net.h"
#include "net/ipv6/simple-udp.h"
#include "radio.h"

#ifdef CONTIKI_DISPLAY
#include "contiki_display.h"
#endif

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678
#define SEND_INTERVAL             (10 * CLOCK_SECOND)


extern void contiki_task(void *arg); // Forward declaration for Contiki task

static const char *TAG = "contiki-example";

#define UART_PORT UART_NUM_0
#define UART_RX_BUF_SIZE 256

/* Test reception of UART - if we want to add commands later... */
static void uart_rx_task(void *arg)
{
    uint8_t* data = (uint8_t*) malloc(UART_RX_BUF_SIZE+1);
    while (1) {
        int len = uart_read_bytes(UART_PORT, data, UART_RX_BUF_SIZE, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            data[len] = '\0';
            ESP_LOGI(TAG, "Received: %s", (char*)data);
            // Here you can add your command parsing logic
            // For example, if (strcmp((char*)data, "blink") == 0) { s_led_state = !s_led_state; }
        }
    }
}
static uint8_t s_led_state = 0;

/* Heartbeat on the RGB LED through Contiki's LED API (arch/leds-arch.c owns the
 * WS2812): green blink once the node is reachable in the RPL DAG, red before. */
static void blink_led(void)
{
    leds_off(LEDS_ALL);
    if (s_led_state) {
        leds_on(NETSTACK_ROUTING.node_is_reachable() ? LEDS_GREEN : LEDS_RED);
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();                
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());          
        ret = nvs_flash_init();                      
    }
    ESP_ERROR_CHECK(ret);  // makes warning fatal in debug

    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_PORT, UART_RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreate(uart_rx_task, "uart_rx_task", 2048, NULL, 10, NULL);

    /* Radio driver at INFO: one line per received frame and per unicast TX result.
     * Set to ESP_LOG_DEBUG for SFD events and frame hexdumps. */
    esp_log_level_set("ESP RADIO", ESP_LOG_INFO);

#ifdef CONTIKI_DISPLAY
    /* LCD console: mirrors everything printed from here on; BOOT button toggles views */
    contiki_display_start();
#endif

    /* Start Contiki Task (initialises the LEDs, so wait before using them) */
    xTaskCreate(contiki_task, "contiki", 8192, NULL, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(500));

    while (1) {
        blink_led();
        /* Toggle the LED state */
        s_led_state = !s_led_state;

        vTaskDelay(pdMS_TO_TICKS(1));
        /* Wait for the transmission to complete */
        vTaskDelay(pdMS_TO_TICKS(1));
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}
