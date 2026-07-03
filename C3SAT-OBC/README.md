# C3SAT-OBC — CubeSat On-Board Computer firmware (FreeRTOS / ESP32-C6)

A from-scratch **satellite On-Board Computer (OBC) / Command & Data Handling
(C&DH)** firmware that runs a realistic CubeSat avionics stack on an
**ESP32-C6-DevKitC-1-N8**. It is built as a portfolio / reference project for an
**embedded software engineer** role in the space domain, and is deliberately
structured to show idiomatic **FreeRTOS** usage end to end.

Every spacecraft subsystem is modelled as an independent FreeRTOS task that
samples sensors, runs local autonomy, and publishes telemetry to a shared,
mutex-protected blackboard. A **mission-control dashboard** is rendered live on
an SPI TFT panel, a **CCSDS-inspired telecommand/telemetry link** runs over
UART, telemetry is logged to onboard **mass memory** (flash), and an **FDIR**
(Fault Detection, Isolation & Recovery) task plus a **mode state machine** keep
the spacecraft safe autonomously.

> Sensors are optional. Every driver falls back to a physically plausible
> **simulation** if its chip is absent, so the full system — dashboard,
> autonomy, downlink — runs on a bare DevKit with nothing but the display
> attached (or even with no display, headless).

---

## Why this project

The target role involves low-level C/C++ on satellite onboard devices,
embedded RTOS (FreeRTOS), the UART/I2C/SPI/CAN protocol family, and hardware
like Flash/EEPROM/RTC/ADC. This project touches all of those on purpose:

| Job requirement              | Where it shows up here                                            |
|------------------------------|-------------------------------------------------------------------|
| FreeRTOS RTOS                | Tasks, queues, mutexes, semaphores, event groups, timers, notifications, task WDT |
| Low-level C                  | Register-level sensor + ILI9341 drivers, bit-banged framing       |
| SPI                          | ILI9341 TFT panel transport (`drivers/ili9341.c`)                 |
| I2C                          | Shared, thread-safe sensor bus (`hal/hal_i2c.c`)                  |
| UART                         | Ground link with a CCSDS-style packet protocol (`services/telecommand.c`, `telemetry.c`) |
| Flash / EEPROM / NVS         | Boot counter in NVS, telemetry log on a SPIFFS "mass memory" partition |
| RTC / ADC                    | DS3231 mission clock; ADC solar-input tap for EPS                |
| Requirements & testing       | Layered architecture, Unity unit tests, simulation for HIL-free testing |
| Domain (satellite)           | EPS / ADCS / Thermal / C&DH / FDIR subsystems and a mode FSM      |

---

## Architecture

Layered components (ESP-IDF components, each independently buildable):

```
                +-----------------------------------------------+
   app_main ->  |  orchestration: bring-up + startup barrier    |
                +-----------------------------------------------+
                          |                       |
        +-----------------+----------+   +---------+-----------------+
        |        subsystems          |   |            ui             |
        |  eps  adcs  thermal  cdh   |   |   gfx  +  gui dashboard    |
        +-------------+--------------+   +---------+-----------------+
                      |   (publish/consume via shared blackboard)   |
        +-------------+---------------------------------------------+
        |                       services                            |
        | obc_state | telemetry | telecommand | storage | clock |   |
        |           | fdir       | mode_manager                  |   |
        +-------------+---------------------------------------------+
                      |                          |
            +---------+---------+      +---------+----------+
            |      drivers      |      |        hal         |
            | ili9341 ina219    |      | i2c_bus  spi_bus   |
            | mpu6050 bme280    |      | (thread-safe)      |
            | ds3231            |      +---------+----------+
            +---------+---------+                |
                      |               +----------+----------+
            +---------+---------------+        bsp           |
            |   obc_common (types,    |  pins, NVS, LED      |
            |   config, errors)       +----------------------+
            +-------------------------+
```

Subsystems never call each other directly. All coupling goes through
`obc_state` (the blackboard), which makes the concurrency model easy to reason
about and review.

### Task set

| Task        | Prio (rel.) | Period   | Core job                                            |
|-------------|-------------|----------|-----------------------------------------------------|
| `fdir`      | highest     | 1 s      | Heartbeat watchdog, HW task-WDT, health stats       |
| `cdh`       | high        | event    | Telecommand dispatch, beacon assembly + downlink    |
| `uart_link` | high        | event    | UART RX, frame parsing → command queue              |
| `adcs`      | high        | 50 ms    | IMU sampling, attitude estimate, detumble detection |
| `mode_mgr`  | high        | 500 ms   | Autonomous BOOT/SAFE/NOMINAL/PAYLOAD/FAULT FSM       |
| `eps`       | mid         | 250 ms   | Power monitoring, SoC, load shedding                |
| `thermal`   | mid         | 1 s      | Temperature, heater hysteresis                      |
| `storage`   | low-mid     | event    | Drains telemetry queue → flash log                  |
| `touch`     | low         | 20 ms    | Samples 4-wire resistive panel → tap queue          |
| `gui`       | low         | 100 ms   | Dashboard + touch menu repaint (never blocks RT tasks) |

### FreeRTOS concept map

This is the part most relevant to the role — where each primitive is used and
*why*:

- **Tasks + priorities** (`obc_config.h`): rate-monotonic-style assignment, FDIR
  on top so it can always preempt to safe the spacecraft.
- **Mutex** (`hal_i2c.c`, `obc_state.c`): serialises the shared I2C bus and
  protects the telemetry blackboard against torn reads.
- **Event group** (`obc_state.c`): doubles as a **startup barrier**
  (`app_main` waits for `EVT_ALL_READY`) and a **change-notification** channel
  (`EVT_MODE_CHANGED` lets the GUI repaint lazily instead of polling).
- **Queues** (`obc_state.c`, `telemetry.c`, `touch.c`): decouple producers from
  consumers — command queue (uplink → CDH), event queue (any task → GUI log),
  telemetry log queue (subsystems → storage), and a **tap queue** (time-sensitive
  50 Hz touch sampling → the lazier 10 Hz GUI), so a slow flash write or a screen
  repaint never stalls a control loop or drops an input.
- **Software watchdog + HW Task WDT** (`fdir.c`): subsystems check in each loop;
  a missed heartbeat latches a fault and forces SAFE mode; a hung FDIR itself is
  caught by the ESP-IDF Task Watchdog and reboots the OBC.
- **`vTaskDelayUntil`**: drift-free periodic scheduling for the sampling tasks.
- **Runtime stats / stack high-water marks** (`fdir.c`): printed periodically so
  stacks can be trimmed with evidence rather than guesswork.

---

## Hardware

### Bill of materials

| Part                               | Role                       | Required? |
|------------------------------------|----------------------------|-----------|
| ESP32-C6-DevKitC-1-N8              | OBC (RISC-V, 8 MB flash)   | yes       |
| MikroE TFT Proto (MI0283QT-9A / ILI9341, 320×240) | Dashboard   | recommended |
| INA219 breakout                    | EPS power monitor          | optional (simulated) |
| MPU6050 breakout                   | ADCS IMU                   | optional (simulated) |
| BME280 **or BMP280** breakout      | Thermal sensor (auto-detected by chip ID) | optional (simulated) |
| DS3231 breakout                    | Mission clock RTC          | optional (simulated) |
| 10 kΩ potentiometer                | Manual "solar input" → ADC | optional  |
| USB-UART adapter                   | Ground-station link        | optional  |

### Pin map (single source of truth: `components/bsp/include/bsp_pins.h`)

**Display — SPI only (the DB0..DB17 parallel bus is NOT used):**

The board is a MikroE **TFT Proto (MIKROE-495)**; the right column below is the
board's **silkscreen** label, which differs from the raw ILI9341 pad names — in
particular there is **no "SCLK" pin: the serial clock is the `WR` pin**.

| ESP32-C6 GPIO | Signal         | TFT Proto silkscreen |
|---------------|----------------|----------------------|
| 6             | SPI SCLK       | `WR` (= SCL)         |
| 7             | SPI MOSI       | `SDI`                |
| —             | SPI MISO       | `SDO` (unconnected — freed GPIO2 for touch) |
| 10            | LCD CS         | `CS`                 |
| 11            | LCD DC         | `RS`                 |
| 18            | LCD RESET      | `RST`                |
| 19            | Backlight (+)  | `LED-A` (via R/NPN); `LED-K`→GND |
| 20            | IM1 strap HIGH | `IM1` (firmware-driven 3V3) |
| 21            | IM2 strap HIGH | `IM2` (firmware-driven 3V3) |

> ⚠️ **Display SPI strapping (required).** The MikroE TFT Proto defaults to the
> parallel bus. The interface-mode pins select **4-wire 8-bit serial I**
> (`IM = 0b0110`): `IM0→GND, IM3→GND`, and **`IM1`, `IM2` are held HIGH by the
> firmware** on GPIO20/21. The DevKit exposes only one `3V3` pad, so rather than
> daisy-chaining, `bsp_display_straps_high()` drives both IM pins high as the very
> first step of `bsp_init()` — before the panel leaves reset — so the straps are
> valid in time. Then wire the serial signals per the table; the clock is `WR`.
>
> **Touch (implemented):** the panel's bare 4-wire resistive touch (`X+ X- Y+ Y-`)
> has **no on-board controller IC**, so the firmware reads it directly via the
> ESP32-C6 ADC + GPIO drive-switching (`drivers/touch.c`). A dedicated 50 Hz
> `touch` task posts taps to a queue drained by the `gui` task. Set
> `TOUCH_SIMULATE 1` in `touch.h` to script a menu tour with no panel wired.

**Touch — 4-wire resistive (read via ADC, no controller IC):**

| ESP32-C6 GPIO | Signal | TFT Proto header | Note                    |
|---------------|--------|------------------|-------------------------|
| 1             | X+     | `X+`             | ADC1_CH1                |
| 2             | Y+     | `Y+`             | ADC1_CH2 (was MISO)     |
| 3             | X-     | `X-`             | ADC1_CH3                |
| 14            | Y-     | `Y-`             | digital drive only      |

**Sensors — shared I2C bus:**

| ESP32-C6 GPIO | Signal | Devices (7-bit addr)                          |
|---------------|--------|-----------------------------------------------|
| 22            | SDA    | INA219 0x40, MPU6050 0x69 (AD0→3V3), BME280 0x76, DS3231 0x68 |
| 23            | SCL    | (add 4.7 kΩ pull-ups to 3V3 if the breakouts lack them) |

**Ground link — UART1** (separate from the USB-Serial-JTAG console):

| ESP32-C6 GPIO | Signal |
|---------------|--------|
| 16            | TX     |
| 17            | RX     |

Strapping pins (GPIO8/9/15) and the USB pins (GPIO12/13) are intentionally left
free of peripherals. GPIO8 (onboard RGB LED) is used as a mode indicator.

---

## Build & flash

Prerequisites: **ESP-IDF v5.2+** installed and exported (`. ./export.sh`).

### Terminal

```bash
cd C3SAT-OBC
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

### CLion

1. Install the **ESP-IDF** support (CLion's CMake will use the IDF toolchain).
2. Open the `C3SAT-OBC` folder. CLion picks up the top-level `CMakeLists.txt`.
3. Point the CMake profile at the IDF toolchain file
   (`$IDF_PATH/tools/cmake/toolchain-esp32c6.cmake`) and ensure `IDF_PATH` is in
   the environment, or use the ESP-IDF plugin's "ESP-IDF Project" wizard which
   wires this up automatically.
4. Use the IDF build/flash/monitor actions, or run `idf.py` in the embedded
   terminal.

On first build the IDF Component Manager pulls the one managed dependency
(`espressif/led_strip`) automatically.

---

## Operating it

On boot you'll see the dashboard come up, the console log subsystems reporting
ready, and the satellite transition `BOOT → SAFE → NOMINAL` once the simulated
battery recovers and the (simulated) tumble damps out. Faults injected by the
simulation (e.g. a low-voltage dip) demote the craft back to SAFE automatically.

### Display & touch menu

The panel is both a live dashboard and an interactive control surface. Tap the
header button (top-right) to toggle between the dashboard and the menu; from the
menu the same button reads **BACK**.

| View          | How you get there        | What it shows                                   |
|---------------|--------------------------|-------------------------------------------------|
| **Dashboard** | default / BACK from menu | Mode banner, EPS/ADCS/thermal panels, last 5 events |
| **Menu**      | tap **MENU**             | SENSORS, MODE CTRL, EVENT LOG, ABOUT, BACKLIGHT |
| **Sensors**   | Menu → SENSORS           | Per-sensor ONLINE/SIMULATED + live values       |
| **Mode ctrl** | Menu → MODE CTRL         | SAFE / NOMINAL / PAYLOAD request buttons        |
| **Event log** | Menu → EVENT LOG         | Last 14 system events                           |
| **About**     | Menu → ABOUT             | Boot count, uptime, free heap, C3S link         |

Mode buttons don't force a transition directly — they enqueue a `SET_MODE`
telecommand onto the same command queue the ground link uses, so the mode
manager and FDIR keep final authority (a touch can request PAYLOAD, but FDIR can
still veto it back to SAFE). This mirrors how a real ground command is handled.

No panel wired? Set `TOUCH_SIMULATE 1` in `components/drivers/include/touch.h`
and the firmware injects a scripted tap tour so the whole menu is demoable
headless.

### Ground station

```bash
pip install pyserial
python tools/groundstation.py --port /dev/ttyUSB0 ping
python tools/groundstation.py --port /dev/ttyUSB0 set-mode PAYLOAD
python tools/groundstation.py --port /dev/ttyUSB0 heater on
python tools/groundstation.py --port /dev/ttyUSB0 listen     # watch beacons
```

The script speaks the exact frame format in `services/telecommand.c`
(sync `0xC3 0x5A`, header, payload, CRC16/CCITT-FALSE).

### Telecommand set

| Opcode | Name          | Payload          | Effect                              |
|--------|---------------|------------------|-------------------------------------|
| 0x01   | PING          | —                | Posts a PONG event                  |
| 0x02   | SET_MODE      | mode byte        | Request mode (FDIR retains veto)    |
| 0x03   | REQ_BEACON    | —                | Immediate beacon downlink           |
| 0x04   | CLEAR_FAULTS  | —                | Clear all latched fault flags       |
| 0x05   | SET_HEATER    | 0/1/0xFF         | Heater off / on / autonomous        |
| 0x06   | DUMP_LOG      | —                | Dump stored telemetry to console    |
| 0x07   | REBOOT        | —                | `esp_restart()`                     |

---

## Tests

Pure codec logic (CRC + framing) is covered by Unity tests in
`components/services/test/`:

```bash
idf.py -T services build flash monitor   # runs the component test app
```

These run without hardware or FreeRTOS objects, making them suitable for CI.

---

## Repository layout

```
C3SAT-OBC/
├── CMakeLists.txt            top-level IDF project
├── sdkconfig.defaults        FreeRTOS tick rate, WDT, runtime stats, 8 MB flash
├── partitions.csv            factory app + NVS + 4 MB "mass memory" (SPIFFS)
├── main/                     app_main: bring-up + startup barrier
├── components/
│   ├── obc_common/           shared types, central config, error codes
│   ├── bsp/                  pin map, NVS boot counter, status LED
│   ├── hal/                  thread-safe I2C + SPI bus wrappers
│   ├── drivers/              ili9341, ina219, mpu6050, bme280/bmp280, ds3231,
│   │                         touch (4-wire resistive via ADC)  (+sim)
│   ├── services/             obc_state, telemetry, telecommand, storage,
│   │   └── test/             clock, fdir, mode_manager  (+ Unity tests)
│   ├── subsystems/           eps, adcs, thermal, cdh tasks
│   └── ui/                   gfx primitives + 5x7 font + dashboard/menu task
└── tools/
    └── groundstation.py      host-side uplink/downlink tool
```

---

## Roadmap / good next steps

This is a working foundation; natural extensions that keep raising its value as
a reference:

- **CAN bus** subsystem link (the role lists CAN) — add a `twai` driver and
  route inter-subsystem telemetry over it instead of the in-RAM blackboard.
- On-screen **touch calibration** routine (persist `TOUCH_CAL_*` to NVS instead
  of the compile-time defaults in `touch.h`).
- Real **CCSDS** Space Packet headers + a small command authentication MAC.
- A proper **B-dot detumble** control law driving simulated magnetorquers.
- Host-target (`linux`) build of the codec + autonomy logic for CI without
  hardware.
- Power profiling with `esp_pm` / light sleep between duty cycles.
