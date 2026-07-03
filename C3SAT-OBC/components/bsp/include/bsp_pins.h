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
 *     SDO         serial data out   -> our SPI MISO  (GPIO2, optional)
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
 * --- Touch panel -----------------------------------------------------------
 * The panel carries a 4-wire RESISTIVE touch (header pins X+, X-, Y+, Y-).
 * There is NO on-board touch controller IC — the raw analog lines come straight
 * to the header. To use touch you must either add an external touch controller
 * (XPT2046/ADS7843/TSC2046) on the SPI bus, or read the panel directly with the
 * ESP32 ADC + GPIO drive-switching. Touch is a roadmap item and is NOT wired or
 * driven by this firmware yet (see the BSP_PIN_TOUCH_* placeholders below).
 *
 * The MISO/SDO line is optional (only needed for register read-back).
 * =========================================================================
 *
 * All ESP32-C6 strapping pins (GPIO8, GPIO9, GPIO15) and the USB-Serial-JTAG
 * pins (GPIO12 = D-, GPIO13 = D+) are deliberately avoided for peripherals.
 */
#ifndef BSP_PINS_H
#define BSP_PINS_H

/* ---- SPI bus (display, ILI9341 4-wire serial) ----------------------------*/
/* Right-hand label is the TFT Proto SILKSCREEN name (not the raw ILI9341 pad). */
#define BSP_SPI_HOST        SPI2_HOST
#define BSP_PIN_SPI_SCLK    6   /**< -> TFT Proto "WR" pin (=SCL; NO "SCLK" exists) */
#define BSP_PIN_SPI_MOSI    7   /**< -> TFT Proto "SDI" (serial data in) */
#define BSP_PIN_SPI_MISO    2   /**< <- TFT Proto "SDO" (serial data out, optional) */
#define BSP_PIN_LCD_CS      10  /**< -> TFT Proto "CS"  (active low) */
#define BSP_PIN_LCD_DC      11  /**< -> TFT Proto "RS"  (D/CX data/command select) */
#define BSP_PIN_LCD_RST     18  /**< -> TFT Proto "RST" (active low) */
#define BSP_PIN_LCD_BL      19  /**< -> TFT Proto "LED-A" backlight (via series R/NPN);
                                 *      "LED-K" -> GND. Active high. */

#define BSP_LCD_SPI_HZ      (40 * 1000 * 1000) /**< 40 MHz pixel clock. */
#define BSP_LCD_WIDTH       320
#define BSP_LCD_HEIGHT      240

/* ---- Resistive touch panel (ROADMAP — not yet wired or driven) -----------*/
/* The TFT Proto exposes the bare 4-wire resistive lines (X+, X-, Y+, Y-) on the
 * header; there is NO touch controller IC on the board. Assign real GPIOs here
 * only when implementing touch (external XPT2046 on the SPI bus, or direct ADC
 * sensing). Left undefined on purpose so nothing claims these pins today. */
/* #define BSP_PIN_TOUCH_XP   <gpio>  // TFT Proto "X+" */
/* #define BSP_PIN_TOUCH_XM   <gpio>  // TFT Proto "X-" */
/* #define BSP_PIN_TOUCH_YP   <gpio>  // TFT Proto "Y+" */
/* #define BSP_PIN_TOUCH_YM   <gpio>  // TFT Proto "Y-" */

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
