#include "console_ring.h"
#include <string.h>
#include "freertos/FreeRTOS.h"

static char lines[CONSOLE_RING_LINES][CONSOLE_RING_COLS + 1];
static unsigned head;          /* next line slot to write */
static unsigned count;         /* committed lines, <= CONSOLE_RING_LINES */
static char cur[CONSOLE_RING_COLS + 1];
static unsigned cur_len;
static unsigned cur_overflow;  /* chars beyond COLS on the current line (silently cut) */
static int esc_state;          /* 0 = normal, 1 = got ESC, 2 = inside CSI */
static uint32_t generation;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

void
console_ring_init(void)
{
  portENTER_CRITICAL_SAFE(&lock);
  head = count = cur_len = cur_overflow = 0;
  esc_state = 0;
  generation++;
  portEXIT_CRITICAL_SAFE(&lock);
}
/*---------------------------------------------------------------------------*/
static void
commit_locked(void)
{
  cur[cur_len] = '\0';
  memcpy(lines[head], cur, cur_len + 1);
  head = (head + 1) % CONSOLE_RING_LINES;
  if(count < CONSOLE_RING_LINES) {
    count++;
  }
  cur_len = 0;
  cur_overflow = 0;
  generation++;
}
/*---------------------------------------------------------------------------*/
static void
putc_locked(char c)
{
  /* Strip ANSI CSI sequences (CONFIG_LOG_COLORS) so they don't eat columns. */
  if(esc_state == 1) {
    esc_state = (c == '[') ? 2 : 0;
    return;
  }
  if(esc_state == 2) {
    if(c >= 0x40 && c <= 0x7e) {
      esc_state = 0;
    }
    return;
  }
  if(c == 0x1b) {
    esc_state = 1;
    return;
  }
  if(c == '\r') {
    return;
  }
  if(c == '\n') {
    commit_locked();
    return;
  }
  if(c == '\t') {
    c = ' ';
  }
  if((unsigned char)c < 0x20 || (unsigned char)c > 0x7e) {
    c = '?';
  }
  if(cur_len < CONSOLE_RING_COLS) {
    cur[cur_len++] = c;
  } else {
    cur_overflow++;
  }
}
/*---------------------------------------------------------------------------*/
void
console_ring_putc(char c)
{
  portENTER_CRITICAL_SAFE(&lock);
  putc_locked(c);
  portEXIT_CRITICAL_SAFE(&lock);
}
/*---------------------------------------------------------------------------*/
void
console_ring_write(const char *s, size_t len)
{
  portENTER_CRITICAL_SAFE(&lock);
  while(len--) {
    putc_locked(*s++);
  }
  portEXIT_CRITICAL_SAFE(&lock);
}
/*---------------------------------------------------------------------------*/
void
console_ring_clear(void)
{
  console_ring_init();
}
/*---------------------------------------------------------------------------*/
uint32_t
console_ring_generation(void)
{
  return generation;
}
/*---------------------------------------------------------------------------*/
size_t
console_ring_snapshot(char *out, size_t out_size, unsigned max_lines)
{
  size_t pos = 0;
  unsigned n, i, first;

  if(out_size == 0) {
    return 0;
  }
  portENTER_CRITICAL_SAFE(&lock);
  n = count < max_lines ? count : max_lines;
  first = (head + CONSOLE_RING_LINES - n) % CONSOLE_RING_LINES;
  for(i = 0; i < n; i++) {
    const char *l = lines[(first + i) % CONSOLE_RING_LINES];
    size_t ll = strlen(l);
    if(pos + ll + 2 > out_size) {
      break;
    }
    memcpy(out + pos, l, ll);
    pos += ll;
    out[pos++] = '\n';
  }
  /* Show the partial line being assembled too, so single-printf output is visible. */
  if(cur_len > 0 && pos + cur_len + 1 <= out_size) {
    memcpy(out + pos, cur, cur_len);
    pos += cur_len;
  } else if(pos > 0) {
    pos--; /* drop trailing newline */
  }
  portEXIT_CRITICAL_SAFE(&lock);
  out[pos] = '\0';
  return pos;
}
