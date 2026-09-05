# components/contiki-display/CMakeLists.txt
# On-board LCD console for the Waveshare ESP32-C6-LCD-1.47 (ST7789, 172x320, LVGL 8).
# Only linked when the project is configured with -DCONTIKI_DISPLAY=ON (see main/CMakeLists.txt).
idf_component_register(
    SRCS          console_ring.c
                  contiki_status.c
                  contiki_display.c
                  lcd/ST7789.c
                  lcd/Vernon_ST7789T.c
                  lcd/LVGL_Driver.c
    INCLUDE_DIRS  . lcd
    REQUIRES      esp_lcd driver esp_timer vfs esp_vfs_console contiki-ng-esp32c6
)

# contiki_status.c reads Contiki-NG structures, so it must see the same configuration as the core.
target_compile_definitions(${COMPONENT_LIB} PRIVATE PROJECT_CONF_H=\"project-conf.h\")
