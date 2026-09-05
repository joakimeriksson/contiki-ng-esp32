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
| port | `components/contiki-ng-esp32c6/`: `arch/` (radio on `esp_ieee802154`, rtimer on `esp_timer`, clock, dbg-io, watchdog, LEDs on the WS2812 pixel), `platform/` (`contiki-conf.h`, `contiki-main.c` = the Contiki task), `project/project-conf.h` |
| apps | default: `examples/rpl-udp/udp-client.c` from the submodule; `-DCONTIKI_NULLNET=ON`: `apps/nullnet-xlevel.c` (nullnet broadcast probe, no IPv6/RPL) |

## Architecture

Three layers, with Contiki knowing nothing about the ones around it:

1. **ESP-IDF application** (`main/`): a normal IDF app. `app_main` sets up NVS and the UART, optionally
   starts the display, creates one FreeRTOS task for Contiki (priority 5, fed to the task watchdog from
   its event loop) and then runs the LED heartbeat.
2. **The port** (`components/contiki-ng-esp32c6/`): upstream Contiki-NG as a submodule plus the glue.
   `platform/contiki-main.c` is the Contiki task (process init, timers, LEDs, MAC address from eFuse,
   netstack inits, autostart, then the `process_run` loop with a 10 ms yield). `arch/` maps Contiki's
   driver interfaces onto IDF: radio on `esp_ieee802154` (ISR callbacks fill a one-frame buffer and poll
   a Contiki process), rtimer on `esp_timer`, clock on the FreeRTOS tick, watchdog on the task WDT,
   LEDs on the WS2812. Contiki's only output interface is C stdio: `printf` is the ROM newlib one, and
   the port's dbg-io (behind `puts`/`putchar`) also writes to stdout, so everything leaves through the
   IDF VFS console like `ESP_LOG` does.
3. **The display** (`components/contiki-display/`, optional): reopens stdout on a small VFS device that
   mirrors the byte stream into a line ring buffer and forwards it to `/dev/console`; an LVGL task
   renders the ring (log view) or a status page. The status page is the one place that reads Contiki
   state directly (RPL instance, link-local address, radio frame counters), without locking.

## Build

```sh
git clone --recursive <this repo>
. $IDF_PATH/export.sh            # ESP-IDF v5.5.4
idf.py set-target esp32c6
idf.py build                     # rpl-udp client -> build/esp32-blink.bin
idf.py -B build-nullnet -DCONTIKI_NULLNET=ON build
idf.py -B build-display -DSDKCONFIG=build-display/sdkconfig -DIDF_TARGET=esp32c6 -DCONTIKI_DISPLAY=ON build
idf.py -p /dev/cu.usbmodem* flash monitor
```

## Tested on hardware

Waveshare ESP32-C6-LCD-1.47 as rpl-udp client against a Zolertia Firefly (CC2538) running the
upstream `udp-server` as RPL root, channel 26, PAN 0xabcd: the C6 joins the DAG, its DAO is
acknowledged, and every `hello N` request gets the `hello N` reply, with hardware ACKs on the
unicasts (RSSI about -63 dBm across a desk). Occasional no-ACK events are retried by CSMA (the radio
driver maps the ESP TX errors to `RADIO_TX_NOACK` / `RADIO_TX_COLLISION`).

The Firefly's PIC co-processor needs the bootloader tool's active-high option, which Contiki-NG's
Makefile does not pass:

```sh
cd contiki-ng/examples/rpl-udp
make TARGET=zoul BOARD=firefly PORT=/dev/cu.usbserial-XXXX \
     BSL_FLAGS="-e -w -v --bootloader-active-high" udp-server.upload
```

(The upload rule also takes care of the load address, 0x00202000 for Zoul images; flashing the
`.bin` at 0x00200000 by hand leaves the CCA page invalid and the chip parked in the bootloader.)

## LCD console (`-DCONTIKI_DISPLAY=ON`)

On the Waveshare ESP32-C6-LCD-1.47 (ST7789 172x320 over SPI2, BOOT button on GPIO 9) the
`components/contiki-display` component mirrors the console to the screen:

| view | content |
|---|---|
| log (default) | the last 21 console lines, 40 columns, 8x8 font, landscape; Contiki logs, `printf`, and `ESP_LOGx` |
| status | uptime, free heap, MAC / channel / PAN, radio frames tx/rx and last RSSI, link-local address, RPL instance state, rank, preferred parent, DAG root, reachability |

BOOT short press toggles the view, a press longer than 0.8 s clears the log. Orientation is
`DISPLAY_ROTATION` in `components/contiki-display/lcd/LVGL_Driver.h` (`LV_DISP_ROT_90` or `_270`).

How it works: stdout is reopened on a small VFS device (`/dev/lcdcon`) whose `write()` copies the bytes
into a line ring buffer and forwards them to `/dev/console` (UART0 + USB-Serial-JTAG), so the serial
output is unchanged. ROM-level output (`ESP_EARLY_LOG` from ISRs, panic dumps) bypasses stdout and is
not shown. The default build does not link LVGL or any display code (the option is exported as an IDF
build property because component requirements are resolved in a separate cmake pass).

### Resetting over USB from macOS

With the native USB-Serial-JTAG port, opening the port from macOS asserts DTR and RTS in an order
that leaves the boot-mode override latched, so a plain RTS pulse (what `esptool` and `idf.py monitor`
do after flashing) restarts the chip in ROM download mode (`boot:0x66`, "waiting for download").
Either press the RST button, or release the reset with DTR high:

```python
s = serial.Serial(port, 115200)          # opens with DTR=1, RTS=1
s.dtr = False; s.rts = True              # EN low
time.sleep(0.15)
s.dtr = True;  s.rts = True              # release -> normal boot (boot:0x6e)
```

The rpl-udp client joins any Contiki-NG RPL root on channel 26, PAN 0xabcd, and exchanges
`hello N` / `hello N` responses with a `udp-server` root (see the upstream example).

## LEDs

The WS2812 RGB pixel on GPIO 8 is Contiki's LED device (`arch/leds-arch.c`, legacy LED API):
`LEDS_RED`, `LEDS_GREEN`, `LEDS_BLUE` are the pixel's colour channels, so `leds_on(LEDS_RED | LEDS_GREEN)`
is yellow. `app_main` blinks it as a heartbeat: green once the node is reachable in the RPL DAG, red
before. Brightness is `LEDS_ARCH_LEVEL` (default 24 of 255).

## Logging

Contiki logs use a compact prefix, `[I RPL] ...` (`LOG_CONF_OUTPUT_PREFIX` in `contiki-conf.h`), and
default to RPL at INFO and MAC / IPv6 / 6LoWPAN at WARN. The ESP radio driver logs per-frame events
(RX, TX result, SFD) at DEBUG; raise it with `esp_log_level_set("ESP RADIO", ESP_LOG_DEBUG)` in `app_main`.

All Contiki output goes through newlib stdout (the linked `printf` is the ROM newlib one; the port's
`dbg_putchar`/`dbg_send_bytes` behind `puts`/`putchar` write to stdout too), so Contiki and IDF logs
share one line-buffered stream on UART0 and USB-Serial-JTAG. Earlier versions sent `puts`/`putchar`
output to the ROM UART0 only, which dropped e.g. `Starting Contiki-NG` from the USB console.

## Tested under emulation

The same images run under [esp32sim](https://github.com/joakimeriksson/esp32sim) (`esp32sim-c6 --cooja`)
as an external mote of [Cooja-NG](https://github.com/joakimeriksson/cooja-ng): the C6 joins an RPL DAG
rooted at an emulated MSP430 (Sky) node next to a native Contiki-NG node, exchanges UDP, and
acknowledges frames in hardware, deterministically (one trace hash over repeated runs). The PHY blob's
baseband calibration is skipped there with `--stub bb_init=0`.

## Status

Experimental. CSMA + RPL-Lite verified on hardware against a CC2538 root and under emulation; TSCH
unsupported; ESP32-H2 untested.
