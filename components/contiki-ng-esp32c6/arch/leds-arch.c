/*
 * Contiki-NG LED driver for the ESP32-C6 boards with one WS2812 RGB pixel
 * (Waveshare ESP32-C6-LCD-1.47, ESP32-C6-DevKitC: GPIO 8).
 *
 * The three colour channels of the single pixel are exposed as three Contiki
 * LEDs, so leds_on(LEDS_RED | LEDS_GREEN) lights it yellow, leds_toggle(LEDS_BLUE)
 * blinks it blue, etc. (legacy LED API, see os/dev/leds.h).
 */
#include "contiki.h"
#include "dev/leds.h"
#include "led_strip.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#ifndef LEDS_ARCH_GPIO
#ifdef CONFIG_BLINK_GPIO
#define LEDS_ARCH_GPIO CONFIG_BLINK_GPIO
#else
#define LEDS_ARCH_GPIO 8
#endif
#endif
/* WS2812 at full scale is blinding; 0..255 per channel when "on". */
#ifndef LEDS_ARCH_LEVEL
#define LEDS_ARCH_LEVEL 24
#endif

static const char *TAG = "leds";
static led_strip_handle_t strip;
static SemaphoreHandle_t lock;
static leds_mask_t current;

/*---------------------------------------------------------------------------*/
void
leds_arch_init(void)
{
  led_strip_config_t strip_config = {
    .strip_gpio_num = LEDS_ARCH_GPIO,
    .max_leds = 1,
    /* The pixel on the Waveshare ESP32-C6-LCD-1.47 takes R,G,B order (the
     * driver's default GRB shows red for green). */
    .color_component_format = (led_color_component_format_t){
      .format = { .r_pos = 0, .g_pos = 1, .b_pos = 2, .w_pos = 3,
                  .bytes_per_color = 1, .num_components = 3 } },
  };
  led_strip_rmt_config_t rmt_config = {
    .resolution_hz = 10 * 1000 * 1000,
    .flags.with_dma = false,
  };
  lock = xSemaphoreCreateMutex();
  if(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip) != ESP_OK) {
    ESP_LOGE(TAG, "WS2812 on GPIO %d: init failed, LEDs disabled", LEDS_ARCH_GPIO);
    strip = NULL;
    return;
  }
  led_strip_clear(strip);
  current = 0;
}
/*---------------------------------------------------------------------------*/
leds_mask_t
leds_arch_get(void)
{
  return current;
}
/*---------------------------------------------------------------------------*/
void
leds_arch_set(leds_mask_t leds)
{
  if(strip == NULL) {
    return;
  }
  if(xSemaphoreTake(lock, pdMS_TO_TICKS(20)) != pdTRUE) {
    return;
  }
  current = leds & LEDS_ALL;
  if(current == 0) {
    led_strip_clear(strip);
  } else {
    led_strip_set_pixel(strip, 0,
                        (current & LEDS_RED) ? LEDS_ARCH_LEVEL : 0,
                        (current & LEDS_GREEN) ? LEDS_ARCH_LEVEL : 0,
                        (current & LEDS_BLUE) ? LEDS_ARCH_LEVEL : 0);
    led_strip_refresh(strip);
  }
  xSemaphoreGive(lock);
}
/*---------------------------------------------------------------------------*/
