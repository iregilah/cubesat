/**
 * @file touch.h
 * @brief 4-wire resistive touch on the MI0283QT-9A panel (no controller IC).
 *
 * The MikroE TFT Proto brings the bare resistive lines (X+, X-, Y+, Y-) to its
 * header with no XPT2046/ADS7843 on board, so we sense the panel directly with
 * the ESP32-C6 ADC. Reading one axis means driving that plate's two rails as
 * digital outputs (one HIGH, one LOW) to set up a voltage gradient, then
 * sampling the opposite plate with the ADC — the sampled voltage is the touch
 * position along the driven axis. A press is confirmed by a separate "pressure"
 * (Z) read so floating pins on an unwired panel do not fire phantom taps.
 *
 * A dedicated FreeRTOS task (touch_start()) samples at a steady cadence,
 * debounces, maps raw ADC to screen pixels via the calibration constants below,
 * and posts one tap per press to a queue. The GUI task drains that queue — this
 * keeps time-sensitive input sampling decoupled from rendering, which is the
 * reason a separate task (rather than polling inside the GUI loop) earns its
 * keep here.
 *
 * Calibration: the TOUCH_CAL_* values are panel/wiring specific. The defaults
 * are reasonable for a 12-bit ADC, but expect to tune them once against your
 * hardware (see the on-screen crosshair note in docs/HARDWARE_HU.md).
 */
#ifndef TOUCH_H
#define TOUCH_H

#include <stdint.h>
#include <stdbool.h>
#include "obc_errors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Calibration (raw 12-bit ADC extents that map to screen edges) --------*/
/* Tune against your panel. If an axis comes out mirrored, swap MIN/MAX. */
#define TOUCH_CAL_X_MIN   250
#define TOUCH_CAL_X_MAX   3800
#define TOUCH_CAL_Y_MIN   250
#define TOUCH_CAL_Y_MAX   3800
/* Orientation fix-ups to match the landscape dashboard (see gfx rotation). */
#define TOUCH_SWAP_XY     1   /**< sampled X plate maps to screen Y and v.v. */
#define TOUCH_INVERT_X    0
#define TOUCH_INVERT_Y    1

/* --- Press detection ------------------------------------------------------*/
#define TOUCH_Z_THRESHOLD 600   /**< min pressure reading to count as a touch. */
#define TOUCH_DEBOUNCE_N  2     /**< consecutive valid samples before a press. */

/* --- Optional self-demo (no touch hardware needed) ------------------------*/
/* Set to 1 to have the sampling task inject a scripted tap tour of the menu
 * every few seconds instead of reading the ADC. Handy to show the UI when no
 * touch panel is wired. Leave 0 for real flight-like behaviour. */
#ifndef TOUCH_SIMULATE
#define TOUCH_SIMULATE    0
#endif

/** A single tap, already mapped to screen pixel coordinates. */
typedef struct {
    int16_t x;   /**< 0 .. width-1  */
    int16_t y;   /**< 0 .. height-1 */
} touch_tap_t;

/** Configure the ADC unit and touch GPIOs. Call once before touch_start(). */
obc_err_t touch_init(void);

/** Create the sampling task that feeds the tap queue. */
obc_err_t touch_start(void);

/** Queue of touch_tap_t taps for the GUI to drain (created by touch_init). */
QueueHandle_t touch_tap_queue(void);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_H */
