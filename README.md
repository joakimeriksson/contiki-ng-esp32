# Contiki-NG on ESP32-C6 (ESP-IDF component)

Contiki-NG running as a FreeRTOS task inside an unmodified ESP-IDF application on the ESP32-C6, with
IEEE 802.15.4 through Espressif's `esp_ieee802154` driver. CSMA and RPL-Lite work; TSCH is not
supported (the radio driver lacks the timestamp/poll-mode parameters and the rtimer runs from an
`esp_timer` task).

| | |
|---|---|
| Contiki-NG | git submodule `components/contiki-ng-esp32c6/contiki-ng`, pinned to upstream `develop` `5968cdae9` |
| ESP-IDF | v5.5.4 (`sdkconfig.defaults.esp32c6` carries the 802.15.4 driver settings) |
| targets | ESP32-C6 (tested); ESP32-H2 has the same radio and should build, untested |
| port | `components/contiki-ng-esp32c6/`: `arch/` (radio on `esp_ieee802154`, rtimer on `esp_timer`, clock, dbg-io, watchdog), `platform/` (`contiki-conf.h`, `contiki-main.c` = the Contiki task), `project/project-conf.h` |
| apps | default: `examples/rpl-udp/udp-client.c` from the submodule; `-DCONTIKI_NULLNET=ON`: `apps/nullnet-xlevel.c` (nullnet broadcast probe, no IPv6/RPL) |

## Build

```sh
git clone --recursive <this repo>
. $IDF_PATH/export.sh            # ESP-IDF v5.5.4
idf.py set-target esp32c6
idf.py build                     # rpl-udp client -> build/esp32-blink.bin
idf.py -B build-nullnet -DCONTIKI_NULLNET=ON build
idf.py -p /dev/cu.usbmodem* flash monitor
```

The rpl-udp client joins any Contiki-NG RPL root on channel 26, PAN 0xabcd, and exchanges
`hello N` / `hello N` responses with a `udp-server` root (see the upstream example).

## Tested under emulation

The same images run under [esp32sim](https://github.com/joakimeriksson/esp32sim) (`esp32sim-c6 --cooja`)
as an external mote of [Cooja-NG](https://github.com/joakimeriksson/cooja-ng): the C6 joins an RPL DAG
rooted at an emulated MSP430 (Sky) node next to a native Contiki-NG node, exchanges UDP, and
acknowledges frames in hardware, deterministically (one trace hash over repeated runs). The PHY blob's
baseband calibration is skipped there with `--stub bb_init=0`.

## Status

Experimental. Known: the port's `putchar` goes straight to the ROM UART while `printf` is newlib-buffered,
so a Contiki log line without a trailing newline can interleave with IDF output.
