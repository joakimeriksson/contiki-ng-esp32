/*
 * console_ring: a small line-oriented ring buffer that mirrors everything the
 * node prints (Contiki-NG logs through dbg_putchar, ESP_LOG through the esp_log
 * vprintf hook) so the LCD can show the tail of the console.
 */
#ifndef CONSOLE_RING_H_
#define CONSOLE_RING_H_

#include <stddef.h>
#include <stdint.h>

#define CONSOLE_RING_LINES   24   /* lines kept; the LCD shows the last 21 */
#define CONSOLE_RING_COLS    40   /* 320 px / 8 px unscii_8 glyphs */

void console_ring_init(void);
/* Feed raw console bytes (any task; ISR-safe). ANSI colour escapes and CRs are dropped. */
void console_ring_write(const char *s, size_t len);
void console_ring_putc(char c);
void console_ring_clear(void);
/* Bumps on every committed line and on clear. */
uint32_t console_ring_generation(void);
/* Copy the last `max_lines` lines, newline separated, NUL terminated. Returns strlen. */
size_t console_ring_snapshot(char *out, size_t out_size, unsigned max_lines);

#endif /* CONSOLE_RING_H_ */
