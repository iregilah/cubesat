/**
 * @file bsp_pins.h
 * @brief Board Support Package — single source of truth for every GPIO.
 *
 * Target board: ESP32-C6-DevKitC-1-N8.
 *
 * ====================  IMPORTANT WIRING NOTE  ============================
 * The display is a MikroElektronika "TFT Proto" board (MIKROE-495) carrying a
 * 2.83" MI0283QT-9A panel driven by an ILI9341 controller. That controller
 * supports a 4-wire SPI mode in addition to the DB0..DB17 parallel bus. As
 * requested, THIS FIRMWARE DRIVES THE PANEL OVER SPI ONLY — the DBx parallel
 * data lines are NOT used.
 *
 * --- Real TFT Proto 2x20 header silkscreen names (NOT the raw ILI9341 names)-
 * The board has NO pin labelled "SCLK"/"SCK". In 4-wire serial mode the serial
 * clock is the "WR" pin (silkscreen: WR, annotated SCL). The serial signals on
 * the 2x20 header map to our GPIOs as:
 *     WR (=SCL)   serial clock      <- our SPI SCLK  (GPIO6)
 *     SDI         serial data in    <- our SPI MOSI  (GPIO7)
 *     SDO         serial data out   -> UNCONNECTED (MISO dropped; GPIO2 is touch Y+)
 *     RS          data/command(D/CX)<- our LCD_DC    (GPIO11)
 *     CS          chip select       <- our LCD_CS    (GPIO10)
 *     RST         reset             <- our LCD_RST   (GPIO18)
 *     LED-A       backlight anode   -> our LCD_BL    (GPIO19, via series R/NPN)
 *     LED-K       backlight cathode -> GND
 *     3.3V / GND  power
 *
 * --- Interface-mode strapping (REQUIRED for SPI) ---------------------------
 * Strap IM[3:0] for "4-wire 8-bit serial interface I" = IM = 0b0110:
 *     IM0 -> GND (0)   IM1 -> 3V3 (1)   IM2 -> 3V3 (1)   IM3 -> GND (0)
 * (ILI9341 datasheet MCU-interface table. Without this the panel stays on the
 * parallel bus and the SPI lines do nothing.)
 *
 * The ESP32-C6 DevKit has only ONE 3V3 pin, which is already needed for the
 * panel/sensor VCC rails. So instead of soldering IM1 and IM2 to a 3V3 rail,
 * this firmware drives them from a GPIO held permanently HIGH for the whole
 * runtime (bsp_display_straps_high(), called first thing in bsp_init(), i.e.
 * BEFORE the panel is released from reset so the strap is valid when the ILI9341
 * latches its interface mode). Because IM1 and IM2 are the SAME logic level (1),
 * both panel pins are tied together to a SINGLE GPIO (BSP_PIN_LCD_IM), which
 * also frees a GPIO for the touch Y- line. IM0/IM3 stay hard-wired to GND.
 *
 * --- Touch panel (NOW DRIVEN) ----------------------------------------------
 * The panel carries a 4-wire RESISTIVE touch (header pins X+, X-, Y+, Y-) with
 * NO on-board controller IC — the raw analog lines come straight to the header.
 * We read it directly with the ESP32-C6 ADC + GPIO drive-switching (no external
 * XPT2046 needed): to read one axis we drive that plate's two rails as digital
 * out (one HIGH, one LOW) and sample the opposite plate with the ADC. Three of
 * the four touch lines therefore sit on ADC1-capable pins (GPIO1/2/3); the 4th
 * is a plain digital drive line. See components/drivers/touch.c.
 *
 * --- Pin budget note: MISO/SDO dropped + IM straps share one GPIO ------------
 * The ESP32-C6-DevKitC-1 breaks out very few free, non-strapping, non-USB GPIOs.
 * NOTE: GPIO14 is NOT on the DevKitC-1 headers (reserved for the internal flash
 * SPI bus), and GPIO24-30 are the module's SPI flash — none are usable. After
 * display control + I2C + UART + status LED + solar ADC, only GPIO1, 2, 3, 20,
 * 21 remain safely free — five pins for what naively looks like six (4 touch +
 * 2 IM). Two independent measures close the gap:
 *   1) MISO/SDO is dropped. This firmware NEVER reads back from the panel, so
 *      the optional SDO line is unnecessary: we free GPIO2 (an ADC1 pin, ideal
 *      for touch) by leaving MISO unconnected (BSP_PIN_SPI_MISO = -1).
 *   2) IM1 and IM2 are both a constant logic HIGH (IM=0b0110), so ONE GPIO
 *      output drives BOTH strap inputs (wire the panel's IM1 and IM2 together to
 *      BSP_PIN_LCD_IM). That frees GPIO21 for the touch Y- drive line, and still
 *      needs no second 3V3 pad.
 * Net: touch X+=1, Y+=2, X-=3, Y-=21; IM1+IM2=20 (shared); SDO unwired.
 * =========================================================================
 *
 * The ESP32-C6 strapping pins (GPIO4, GPIO5, GPIO8, GPIO9, GPIO15) and the
 * USB-Serial-JTAG pins (GPIO12 = D-, GPIO13 = D+) are deliberately kept free of
 * boot-critical wiring. GPIO8 carries the on-board RGB LED (only sampled at
 * reset, harmless as an output afterwards); GPIO4/5 are left entirely unused so
 * their reset-time level can never disturb the boot mode.
 */
#ifndef BSP_PINS_H
#define BSP_PINS_H

/* ---- SPI bus (display, ILI9341 4-wire serial) ----------------------------*/
/* Right-hand label is the TFT Proto SILKSCREEN name (not the raw ILI9341 pad). */
#define BSP_SPI_HOST        SPI2_HOST
#define BSP_PIN_SPI_SCLK    6   /**< -> TFT Proto "WR" pin (=SCL; NO "SCLK" exists) */
#define BSP_PIN_SPI_MOSI    7   /**< -> TFT Proto "SDI" (serial data in) */
#define BSP_PIN_SPI_MISO    (-1)/**< SDO/MISO dropped to free GPIO2 for touch (see note). */
#define BSP_PIN_LCD_CS      10  /**< -> TFT Proto "CS"  (active low) */
#define BSP_PIN_LCD_DC      11  /**< -> TFT Proto "RS"  (D/CX data/command select) */
#define BSP_PIN_LCD_RST     18  /**< -> TFT Proto "RST" (active low) */
#define BSP_PIN_LCD_BL      19  /**< -> TFT Proto "LED-A" backlight (via series R/NPN);
                                 *      "LED-K" -> GND. Active high. */

/* Interface-mode strap, driven HIGH by GPIO for the whole runtime (see the
 * header comment). IM1 and IM2 are both logic 1 for "4-wire serial I", so wire
 * BOTH panel pins (IM1 and IM2) to this single GPIO. IM0/IM3 -> GND. */
#define BSP_PIN_LCD_IM      20  /**< -> TFT Proto "IM1" AND "IM2" (tied together), held HIGH by firmware. */

#define BSP_LCD_SPI_HZ      (40 * 1000 * 1000) /**< 40 MHz pixel clock. */
#define BSP_LCD_WIDTH       320
#define BSP_LCD_HEIGHT      240

/* ---- Resistive touch panel (4-wire, driven directly by the ESP32 ADC) -----*/
/* TFT Proto header lines X+, X-, Y+, Y- -> ESP32-C6 GPIOs. Three sit on ADC1
 * channels so either plate can be sampled; Y- is a plain digital drive line.
 * The ADC_CHANNEL_x values below MUST match the GPIO (ESP32-C6: ADC1_CHn = GPIOn
 * for n=0..6). Read/tuning logic + calibration live in drivers/touch.c. */
#define BSP_PIN_TOUCH_XP    1   /**< TFT Proto "X+"  (ADC1_CH1, analog read). */
#define BSP_PIN_TOUCH_YP    2   /**< TFT Proto "Y+"  (ADC1_CH2, analog read; was MISO). */
#define BSP_PIN_TOUCH_XM    3   /**< TFT Proto "X-"  (ADC1_CH3, analog read + drive). */
#define BSP_PIN_TOUCH_YM    21  /**< TFT Proto "Y-"  (digital drive only; freed by IM sharing). */

#define BSP_TOUCH_ADC_UNIT  ADC_UNIT_1
#define BSP_TOUCH_CH_XP     ADC_CHANNEL_1  /**< must match BSP_PIN_TOUCH_XP. */
#define BSP_TOUCH_CH_YP     ADC_CHANNEL_2  /**< must match BSP_PIN_TOUCH_YP. */
#define BSP_TOUCH_CH_XM     ADC_CHANNEL_3  /**< must match BSP_PIN_TOUCH_XM. */

/* ---- I2C bus (sensors: EPS / ADCS / thermal / RTC) -----------------------*/
#define BSP_I2C_PORT        0
#define BSP_PIN_I2C_SDA     22
#define BSP_PIN_I2C_SCL     23
#define BSP_I2C_HZ          400000  /**< 400 kHz fast-mode. */

/* 7-bit I2C addresses of the modelled subsystem sensors. */
#define BSP_I2C_ADDR_INA219   0x40  /**< EPS: bus V/I monitor (A0=A1=GND). */
#define BSP_I2C_ADDR_MPU6050  0x69  /**< ADCS: 6-axis IMU. NOTE: tie AD0->3V3!
                                     *   Default AD0=GND gives 0x68, which
                                     *   COLLIDES with the DS3231 RTC. Pull AD0
                                     *   high to move the IMU to 0x69. */
#define BSP_I2C_ADDR_BME280   0x76  /**< Thermal: T/P/H (SDO=GND; 0x77 if SDO=VCC). */
#define BSP_I2C_ADDR_DS3231   0x68  /**< RTC: mission clock (fixed 0x68). */

/* ---- UART1: ground-station telemetry/telecommand link --------------------*/
/* UART0 / USB-Serial-JTAG remains the debug console; the "radio" link is a
 * separate UART so the protocol stack is exercised independently of logging. */
#define BSP_UART_LINK_PORT  1
#define BSP_PIN_UART_TX     16
#define BSP_PIN_UART_RX     17
#define BSP_UART_BAUD       115200

/* ---- Status indicator ----------------------------------------------------*/
/* The DevKitC-1 has an addressable RGB LED on GPIO8; we use it as a coarse
 * mode indicator (green=nominal, blue=safe, red=fault, etc.). */
#define BSP_PIN_RGB_LED     8

/* ---- Analog input: simulated solar-array voltage tap ---------------------*/
/* A potentiometer on this ADC pin lets you manually vary the "solar input" to
 * exercise the EPS charge/discharge and load-shedding logic on the bench. */
#define BSP_ADC_SOLAR_CH    ADC_CHANNEL_0  /* GPIO0 on ESP32-C6 */

#endif /* BSP_PINS_H */
