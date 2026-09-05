/* components/contiki-ng-esp32c6/platform/contiki-conf.h */
#ifndef CONTIKI_CONF_H_
#define CONTIKI_CONF_H_

#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  CPU / compiler specifics                                          */
/* ------------------------------------------------------------------ */
#define CC_CONF_REGISTER_ARGS         1
#define CC_CONF_FUNCTION_POINTER_ARGS 1
#define CC_CONF_VA_ARGS               1
#define CC_CONF_INLINE                inline
#define CC_CONF_ALIGN_PACK(n)         __attribute__((packed,aligned(n)))

/* ---------- CLOCKS ------------------------------------------------ */
#define CLOCK_CONF_SECOND            1000          /* 1 ms ticks from clock.c */

/* ---------- REAL-TIME TIMER -------------------------------------- */
/* Use a 64-bit counter running at 1 MHz (1 tick = 1 µs) */
#define RTIMER_CONF_CLOCK_TYPE       uint64_t
#define RTIMER_CONF_SECOND           1000000UL

/* ------------------------------------------------------------------ */
/*  Platform drivers you have implemented                             */
/* ------------------------------------------------------------------ */
#define RTIMER_CONF_ARCH_NOW          rtimer_arch_now   /* if you have one */

/* Define the Contiki-NG version and target strings */
#define CONTIKI_VERSION_STRING "Contiki-NG-ESP32C6"
#define CONTIKI_TARGET_STRING "esp32c6"

#ifdef CONTIKI_APP_NULLNET
/* E6 stage 1: nullnet broadcast only, no IPv6, no RPL (see apps/nullnet-xlevel.c) */
#define UIP_CONF_IPV6           0
#define NETSTACK_CONF_WITH_IPV6 0
#define RPL_CONF_ENABLED        0
#define ROUTING_CONF_RPL_LITE   0
#define ROUTING_CONF_NULLROUTING 1
#define NETSTACK_CONF_NETWORK   nullnet_driver
#else
/* Enable IPv6 */
#define UIP_CONF_IPV6 1

/* Network stack configuration */
#define RPL_CONF_ENABLED        1
#define NETSTACK_CONF_WITH_IPV6 1
#define ROUTING_CONF_RPL_LITE   1
#endif
#define NETSTACK_CONF_MAC       csma_driver
#define NETSTACK_CONF_FRAMER    framer_802154
#define NETSTACK_CONF_RADIO     esp32c6_radio_driver

/* One WS2812 RGB pixel (GPIO 8), driven as three Contiki LEDs by arch/leds-arch.c */
#define LEDS_CONF_LEGACY_API 1
#define LEDS_CONF_COUNT      3
#define LEDS_CONF_RED        1
#define LEDS_CONF_GREEN      2
#define LEDS_CONF_BLUE       4
#define LEDS_CONF_ALL        7

#define UIP_CONF_STATISTICS 1      /* let the header create uip_stats_t */
typedef uint32_t uip_stats_t;             /* type expected by uip.h */

/* Compact log prefix, "[I RPL] ", instead of the default "[INFO: RPL       ] "
 * (19 columns of the LCD's 40). */
#define LOG_CONF_OUTPUT_PREFIX(level, levelstr, module) LOG_OUTPUT("[%.1s %s] ", levelstr, module)

/* Log levels: RPL at INFO shows DAG joins and parent changes; the packet-level
 * modules stay at WARN (6LoWPAN at DBG prints several lines per packet). */
#define LOG_CONF_LEVEL_RPL      LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_MAC      LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_IPV6     LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_6LOWPAN  LOG_LEVEL_WARN

/* Application overrides come last */
#ifdef PROJECT_CONF_H
#include PROJECT_CONF_H
#endif

#endif /* CONTIKI_CONF_H_ */
