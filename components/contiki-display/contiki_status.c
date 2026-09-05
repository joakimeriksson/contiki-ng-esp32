#include "contiki_status.h"
#include <stdio.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "net/linkaddr.h"
#include "net/mac/framer/frame802154.h"
#include "sys/clock.h"
#include "esp_heap_caps.h"
#include "esp_system.h"

#if NETSTACK_CONF_WITH_IPV6 && ROUTING_CONF_RPL_LITE
#include "net/routing/rpl-lite/rpl.h"
#define WITH_RPL_STATUS 1
#else
#define WITH_RPL_STATUS 0
#endif

/* Frame counters kept by arch/radio-esp32c6.c */
extern volatile uint32_t esp32c6_radio_tx_frames;
extern volatile uint32_t esp32c6_radio_rx_frames;
extern volatile int8_t esp32c6_radio_last_rssi;

#define APPEND(...) do { \
    if(pos < out_size) { pos += snprintf(out + pos, out_size - pos, __VA_ARGS__); } \
  } while(0)

#if NETSTACK_CONF_WITH_IPV6
static void
append_ip6_tail(char *out, size_t out_size, size_t *ppos, const uip_ipaddr_t *a)
{
  size_t pos = *ppos;
  if(a == NULL) {
    APPEND("-");
  } else {
    /* Last two 16-bit groups: enough to tell nodes apart on a small screen. */
    APPEND("..%x:%x", (a->u8[12] << 8) | a->u8[13], (a->u8[14] << 8) | a->u8[15]);
  }
  *ppos = pos;
}
#endif

void
contiki_status_format(char *out, size_t out_size)
{
  size_t pos = 0;
  unsigned long up = clock_seconds();

  if(out_size == 0) {
    return;
  }
  out[0] = '\0';

  APPEND("%s\n", CONTIKI_VERSION_STRING);
  APPEND("up %02lu:%02lu:%02lu  heap %luk\n",
         up / 3600, (up / 60) % 60, up % 60,
         (unsigned long)esp_get_free_heap_size() / 1024);
  APPEND("mac ..%02x%02x:%02x%02x  ch %u pan %04x\n",
         linkaddr_node_addr.u8[4], linkaddr_node_addr.u8[5],
         linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7],
         IEEE802154_DEFAULT_CHANNEL, IEEE802154_PANID);
  APPEND("radio tx %lu  rx %lu  rssi %d\n",
         (unsigned long)esp32c6_radio_tx_frames,
         (unsigned long)esp32c6_radio_rx_frames,
         (int)esp32c6_radio_last_rssi);
  APPEND("mac %s  net %s\n", NETSTACK_MAC.name, NETSTACK_NETWORK.name);

#if NETSTACK_CONF_WITH_IPV6
  {
    uip_ds6_addr_t *ll = uip_ds6_get_link_local(-1);
    APPEND("ll ");
    append_ip6_tail(out, out_size, &pos, ll ? &ll->ipaddr : NULL);
    APPEND("\n");
  }
#endif

#if WITH_RPL_STATUS
  if(!curr_instance.used) {
    APPEND("rpl: no instance\n");
  } else {
    static const char *const states[] = { "init", "joined", "reachable", "poison" };
    unsigned st = curr_instance.dag.state;
    APPEND("rpl %s  rank %u\n",
           st < 4 ? states[st] : "?", (unsigned)curr_instance.dag.rank);
    APPEND("parent ");
    append_ip6_tail(out, out_size, &pos,
                    curr_instance.dag.preferred_parent
                    ? rpl_neighbor_get_ipaddr(curr_instance.dag.preferred_parent) : NULL);
    APPEND("  root ");
    append_ip6_tail(out, out_size, &pos, &curr_instance.dag.dag_id);
    APPEND("\n");
  }
  APPEND("reachable: %s\n", NETSTACK_ROUTING.node_is_reachable() ? "yes" : "no");
#endif
}
