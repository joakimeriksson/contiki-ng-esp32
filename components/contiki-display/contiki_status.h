/* Text snapshot of the node's state for the LCD status view. */
#ifndef CONTIKI_STATUS_H_
#define CONTIKI_STATUS_H_
#include <stddef.h>

/* Fills `out` with newline-separated status lines. Reads Contiki-NG structures
 * without locking: values may be a tick stale, which is fine for a display. */
void contiki_status_format(char *out, size_t out_size);

#endif /* CONTIKI_STATUS_H_ */
