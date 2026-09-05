/* LCD console + status view for the Waveshare ESP32-C6-LCD-1.47.
 * Call contiki_display_start() from app_main before the Contiki task starts so
 * the first log lines are captured. BOOT button (GPIO 9): short press toggles
 * log/status view, long press (>0.8 s) clears the log. */
#ifndef CONTIKI_DISPLAY_H_
#define CONTIKI_DISPLAY_H_

void contiki_display_start(void);

#endif /* CONTIKI_DISPLAY_H_ */
