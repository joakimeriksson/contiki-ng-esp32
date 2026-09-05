/*
 * Debug-print back-end for Contiki-NG on ESP32-C6
 * ------------------------------------------------
 *  - dbg_putchar()      : one char
 *  - dbg_send_bytes()   : a raw buffer
 *
 * Used by os/lib/dbg-io/puts.c and putchar.c (the linked printf is the ROM
 * newlib one, which already writes to stdout). Everything therefore goes
 * through newlib's stdout -> the IDF VFS console (UART0 + USB-Serial-JTAG),
 * in order, with CRLF conversion, and can be redirected as one stream
 * (components/contiki-display taps it for the LCD).
 */
#include "contiki.h"
#include "dbg.h"
#include <stdio.h>

/*---------------------------------------------------------------------------*/
int
dbg_putchar(int c)
{
  return fputc(c, stdout) == EOF ? EOF : c;
}
/*---------------------------------------------------------------------------*/
unsigned int
dbg_send_bytes(const unsigned char *s, unsigned int len)
{
  if(s == NULL || len == 0) {
    return 0;
  }
  return (unsigned int)fwrite(s, 1, len, stdout);
}
/*---------------------------------------------------------------------------*/
