/**
 * @file bsp_pins.h
 * @brief Board Support Package — single source of truth for every GPIO.
 *
 * Target board: ESP32-C6-DevKitC-1-N8.
 *
 * ====================  IMPORTANT WIRING NOTE  ============================
 * The display is a MikroElektronika "TFT Proto" board carrying an
 * MI0283QT-9A panel driven by an ILI9341 controller. That controller supports
 * a 4-wire SPI mode in addition to the DB1..DB16 parallel bus. As requested,
 * THIS FIRMWARE DRIVES THE PANEL OVER SPI ONLY — the DB1..DB16 parallel data
 * lines are NOT used.
 *
 * To make the panel speak SPI you must strap its interface-mode pins (IM0..IM3)
 * for "4-wire 8-bit serial" per the ILI9341 datasheet, and route the serial
 * signals to the controller's SDI/SDO/SCL/D-C/CS/RESET pads. On the TFT Proto
 * board these appear on the 2x10 header — confirm against the board silkscreen
 * before wiring, then map them to the ESP32 GPIOs below. The MISO/SDO line is
 * optional (only needed for register read-back / touch).
 * =========================================================================
 *
 * All ESP32-C6 strapping pins (GPIO8, GPIO9, GPIO15) and the USB-Serial-JTAG
 * pins (GPIO12 = D-, GPIO13 = D+) are deliberately avoided for peripherals.
 */
#ifndef BSP_PINS_H
#define BSP_PINS_H

/* ---- SPI bus (display, ILI9341 4-wire serial) ----------------------------*/
#define BSP_SPI_HOST        SPI2_HOST
#define BSP_PIN_SPI_SCLK    6   /**< -> ILI9341 SCL  */
#define BSP_PIN_SPI_MOSI    7   /**< -> ILI9341 SDI  */
#define BSP_PIN_SPI_MISO    2   /**< <- ILI9341 SDO (optional) */
#define BSP_PIN_LCD_CS      10  /**< -> ILI9341 CSX  (active low) */
#define BSP_PIN_LCD_DC      11  /**< -> ILI9341 D/CX (data/command select) */
#define BSP_PIN_LCD_RST     18  /**< -> ILI9341 RESX (active low) */
#define BSP_PIN_LCD_BL      19  /**< -> backlight enable (active high) */

#define BSP_LCD_SPI_HZ      (40 * 1000 * 1000) /**< 40 MHz pixel clock. */
#define BSP_LCD_WIDTH       320
#define BSP_LCD_HEIGHT      240

/* ---- I2C bus (sensors: EPS / ADCS / thermal / RTC) -----------------------*/
#define BSP_I2C_PORT        0
#define BSP_PIN_I2C_SDA     22
#define BSP_PIN_I2C_SCL     23
#define BSP_I2C_HZ          400000  /**< 400 kHz fast-mode. */

/* 7-bit I2C addresses of the modelled subsystem sensors. */
#define BSP_I2C_ADDR_INA219   0x40  /**< EPS: bus V/I monitor. */
#define BSP_I2C_ADDR_MPU6050  0x68  /**< ADCS: 6-axis IMU. */
#define BSP_I2C_ADDR_BME280   0x76  /**< Thermal: T/P/H. */
#define BSP_I2C_ADDR_DS3231   0x68  /**< RTC: mission clock (alt 0x57 EEPROM). */

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
