/*
 * LCD console for Contiki-NG on the Waveshare ESP32-C6-LCD-1.47.
 *
 * Two views, toggled with the BOOT button (GPIO 9):
 *   log    - the last 21 console lines, 40 columns, 8x8 font (tail of the UART output)
 *   status - uptime, heap, MAC/channel/PAN, radio frame counters, link-local, RPL state
 * A long press (>0.8 s) clears the log.
 *
 * Console capture: everything the node prints (Contiki logs via the ROM newlib
 * printf and dbg-io puts/putchar, ESP_LOGx via vprintf) ends up in newlib's
 * stdout. stdout is reopened on a tiny VFS device, /dev/lcdcon, whose write()
 * copies the bytes into console_ring and forwards them unchanged to
 * /dev/console (UART0 + USB-Serial-JTAG). ROM-level output (ESP_EARLY_LOG from
 * ISRs, panic dumps) bypasses stdout and is not mirrored.
 */
#include "contiki_display.h"
#include "console_ring.h"
#include "contiki_status.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_console.h"
#include "lvgl.h"
#include "ST7789.h"
#include "LVGL_Driver.h"

#define BOOT_BUTTON_GPIO      9
#define LONG_PRESS_MS         800
#define DEBOUNCE_MS           30
#define LOG_ROWS              21
#define STATUS_REFRESH_MS     500

static const char *TAG = "display";

static SemaphoreHandle_t button_sem;
static volatile int toggle_request;         /* set by button task, consumed by UI task */
static lv_obj_t *scr_log, *scr_status, *log_label, *status_label;
static char textbuf[CONSOLE_RING_LINES * (CONSOLE_RING_COLS + 1) + 64];
static int console_fd = -1;

/*---------------------------------------------------------------------------*/
/* /dev/lcdcon: stdout tap                                                   */
/*---------------------------------------------------------------------------*/
static ssize_t
lcdcon_write(int fd, const void *data, size_t size)
{
  console_ring_write(data, size);
  if(console_fd >= 0) {
    return write(console_fd, data, size);
  }
  return size;
}
/*---------------------------------------------------------------------------*/
static int
lcdcon_open(const char *path, int flags, int mode)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
lcdcon_close(int fd)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
lcdcon_fstat(int fd, struct stat *st)
{
  memset(st, 0, sizeof(*st));
  st->st_mode = S_IFCHR;
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
console_tap_install(void)
{
  static const esp_vfs_t vfs = {
    .flags = ESP_VFS_FLAG_DEFAULT,
    .write = &lcdcon_write,
    .open = &lcdcon_open,
    .close = &lcdcon_close,
    .fstat = &lcdcon_fstat,
  };
  console_fd = open(ESP_VFS_DEV_CONSOLE, O_WRONLY);
  ESP_ERROR_CHECK(esp_vfs_register("/dev/lcdcon", &vfs, NULL));
  fflush(stdout);
  if(freopen("/dev/lcdcon", "w", stdout) == NULL) {
    ESP_LOGE(TAG, "freopen(/dev/lcdcon) failed; LCD log will be empty");
    return;
  }
  setvbuf(stdout, NULL, _IOLBF, 256);
}
/*---------------------------------------------------------------------------*/
/* Button                                                                    */
/*---------------------------------------------------------------------------*/
static void IRAM_ATTR
button_isr(void *arg)
{
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(button_sem, &woken);
  if(woken) {
    portYIELD_FROM_ISR();
  }
}
/*---------------------------------------------------------------------------*/
static void
button_task(void *arg)
{
  for(;;) {
    if(xSemaphoreTake(button_sem, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
    if(gpio_get_level(BOOT_BUTTON_GPIO) != 0) {
      continue;                              /* bounce */
    }
    int held_ms = DEBOUNCE_MS;
    while(gpio_get_level(BOOT_BUTTON_GPIO) == 0 && held_ms < 5000) {
      vTaskDelay(pdMS_TO_TICKS(10));
      held_ms += 10;
    }
    if(held_ms >= LONG_PRESS_MS) {
      console_ring_clear();
      ESP_LOGI(TAG, "log cleared");
    } else {
      toggle_request = 1;
    }
    vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
    xSemaphoreTake(button_sem, 0);           /* drop bounces queued meanwhile */
  }
}
/*---------------------------------------------------------------------------*/
static void
button_init(void)
{
  button_sem = xSemaphoreCreateBinary();
  gpio_config_t io = {
    .intr_type = GPIO_INTR_NEGEDGE,
    .pin_bit_mask = 1ULL << BOOT_BUTTON_GPIO,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
  };
  gpio_config(&io);
  esp_err_t err = gpio_install_isr_service(0);
  if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_ERROR_CHECK(err);
  }
  gpio_isr_handler_add(BOOT_BUTTON_GPIO, button_isr, NULL);
  xTaskCreate(button_task, "lcd_button", 2048, NULL, 10, NULL);
}
/*---------------------------------------------------------------------------*/
/* LVGL views                                                                */
/*---------------------------------------------------------------------------*/
static lv_obj_t *
make_screen(lv_color_t bg)
{
  lv_obj_t *scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, bg, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}
/*---------------------------------------------------------------------------*/
static void
build_views(void)
{
  lv_coord_t w = lv_disp_get_hor_res(NULL);
  lv_coord_t h = lv_disp_get_ver_res(NULL);

  scr_log = make_screen(lv_color_black());
  log_label = lv_label_create(scr_log);
  lv_obj_set_style_text_font(log_label, &lv_font_unscii_8, LV_PART_MAIN);
  lv_obj_set_style_text_color(log_label, lv_color_make(0x50, 0xff, 0x50), LV_PART_MAIN);
  lv_obj_set_style_text_line_space(log_label, 0, LV_PART_MAIN);
  lv_label_set_long_mode(log_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(log_label, w, h);
  lv_obj_set_pos(log_label, 0, h - LOG_ROWS * 8);   /* bottom-aligned tail */
  lv_label_set_text(log_label, "");

  scr_status = make_screen(lv_color_make(0x10, 0x18, 0x30));
  status_label = lv_label_create(scr_status);
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(status_label, lv_color_white(), LV_PART_MAIN);
  lv_label_set_long_mode(status_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(status_label, w - 8, h - 4);
  lv_obj_set_pos(status_label, 4, 2);
  lv_label_set_text(status_label, "");

  lv_scr_load(scr_log);
}
/*---------------------------------------------------------------------------*/
static void
refresh_log(void)
{
  console_ring_snapshot(textbuf, sizeof(textbuf), LOG_ROWS);
  /* Keep the newest line at the bottom: pad short histories from the top. */
  unsigned lines = 1;
  for(const char *p = textbuf; *p; p++) {
    if(*p == '\n') {
      lines++;
    }
  }
  if(lines < LOG_ROWS) {
    lv_obj_set_pos(log_label, 0, (LOG_ROWS - lines) * 8 + (lv_disp_get_ver_res(NULL) - LOG_ROWS * 8));
  } else {
    lv_obj_set_pos(log_label, 0, lv_disp_get_ver_res(NULL) - LOG_ROWS * 8);
  }
  lv_label_set_text(log_label, textbuf);
}
/*---------------------------------------------------------------------------*/
static void
ui_task(void *arg)
{
  uint32_t shown_gen = (uint32_t)-1;
  TickType_t last_status = 0;
  int showing_log = 1;

  build_views();
  for(;;) {
    if(toggle_request) {
      toggle_request = 0;
      showing_log = !showing_log;
      lv_scr_load(showing_log ? scr_log : scr_status);
      shown_gen = (uint32_t)-1;
      last_status = 0;
    }
    if(showing_log) {
      uint32_t gen = console_ring_generation();
      if(gen != shown_gen) {
        shown_gen = gen;
        refresh_log();
      }
    } else {
      TickType_t now = xTaskGetTickCount();
      if(last_status == 0 || now - last_status >= pdMS_TO_TICKS(STATUS_REFRESH_MS)) {
        last_status = now;
        contiki_status_format(textbuf, sizeof(textbuf));
        lv_label_set_text(status_label, textbuf);
      }
    }
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
/*---------------------------------------------------------------------------*/
void
contiki_display_start(void)
{
  console_ring_init();
  console_tap_install();

  LCD_Init();
  BK_Light(60);
  LVGL_Init();
  button_init();
  xTaskCreate(ui_task, "lcd_ui", 6144, NULL, 4, NULL);
  ESP_LOGI(TAG, "LCD console started (BOOT: short=toggle view, long=clear log)");
}
