/**
 * @file obc_config.h
 * @brief Central tuning knobs: task priorities/stacks, periods, thresholds.
 *
 * Keeping every scheduling parameter in one place makes the FreeRTOS design
 * reviewable at a glance — which is exactly what matters in a flight-software
 * context, where task priority inversions and starvation must be ruled out by
 * inspection. Priorities are expressed relative to configMAX_PRIORITIES.
 */
#ifndef OBC_CONFIG_H
#define OBC_CONFIG_H

#include "freertos/FreeRTOS.h"

/* ---------------------------------------------------------------------------
 * Task priorities (higher number = higher priority).
 *
 * Rationale (rate-monotonic-ish): the faster a task must react, the higher its
 * priority. FDIR sits at the top so it can always preempt to safe the bird;
 * the GUI sits near the bottom because a late repaint is harmless.
 * ------------------------------------------------------------------------- */
#define PRIO_FDIR          (configMAX_PRIORITIES - 1) /**< Safety watchdog. */
#define PRIO_CDH           (configMAX_PRIORITIES - 2) /**< Command handling. */
#define PRIO_ADCS          (configMAX_PRIORITIES - 3) /**< Fastest control loop. */
#define PRIO_EPS           (configMAX_PRIORITIES - 4)
#define PRIO_THERMAL       (configMAX_PRIORITIES - 5)
#define PRIO_TELEMETRY     (configMAX_PRIORITIES - 5)
#define PRIO_STORAGE       (configMAX_PRIORITIES - 6)
#define PRIO_MODE_MGR      (configMAX_PRIORITIES - 4)
#define PRIO_GUI           (2)
#define PRIO_UART_LINK     (configMAX_PRIORITIES - 3)

/* ---------------------------------------------------------------------------
 * Task stack sizes (bytes). Sized with margin; the FDIR task prints the actual
 * high-water marks at runtime so they can be trimmed with evidence.
 * ------------------------------------------------------------------------- */
#define STACK_FDIR         3072
#define STACK_CDH          4096
#define STACK_ADCS         3072
#define STACK_EPS          3072
#define STACK_THERMAL      3072
#define STACK_TELEMETRY    4096
#define STACK_STORAGE      4096
#define STACK_MODE_MGR     3072
#define STACK_GUI          6144
#define STACK_UART_LINK    4096

/* ---------------------------------------------------------------------------
 * Subsystem loop periods (ms). These set the housekeeping cadence.
 * ------------------------------------------------------------------------- */
#define PERIOD_ADCS_MS         50    /**< 20 Hz attitude sampling. */
#define PERIOD_EPS_MS          250   /**< 4 Hz power monitoring. */
#define PERIOD_THERMAL_MS      1000  /**< 1 Hz thermal. */
#define PERIOD_BEACON_MS       5000  /**< Beacon downlink every 5 s. */
#define PERIOD_GUI_MS          100   /**< 10 fps dashboard refresh. */
#define PERIOD_FDIR_MS         1000  /**< Health sweep every 1 s. */
#define PERIOD_STORAGE_FLUSH_MS 2000 /**< Flush telemetry log every 2 s. */

/* ---------------------------------------------------------------------------
 * Queue depths.
 * ------------------------------------------------------------------------- */
#define Q_DEPTH_TELEMETRY  16   /**< Telemetry samples awaiting log/downlink. */
#define Q_DEPTH_COMMAND    8    /**< Decoded telecommands awaiting dispatch. */
#define Q_DEPTH_EVENT      16   /**< System events for the GUI event log. */

/* ---------------------------------------------------------------------------
 * Physical / autonomy thresholds (drive the FDIR and mode manager).
 * ------------------------------------------------------------------------- */
#define EPS_UV_THRESHOLD_V       3.30f  /**< Below this -> under-voltage fault. */
#define EPS_OC_THRESHOLD_MA      800.0f /**< Above this -> over-current fault. */
#define EPS_SAFE_RECOVER_V       3.70f  /**< Above this -> clear UV, leave SAFE. */
#define THERMAL_HOT_C            60.0f
#define THERMAL_COLD_C           (-10.0f)
#define THERMAL_HEATER_ON_C      (-5.0f)
#define THERMAL_HEATER_OFF_C     5.0f
#define ADCS_TUMBLE_DPS          25.0f  /**< RMS rate above this = tumbling. */
#define ADCS_DETUMBLE_DPS        3.0f   /**< RMS rate below this = detumbled. */

#endif /* OBC_CONFIG_H */
