/**
 * @file obc_types.h
 * @brief Core data types shared across the C3SAT-OBC firmware.
 *
 * A CubeSat On-Board Computer (OBC) gathers state from several subsystems
 * (power, attitude, thermal) and publishes it both to ground (via the
 * telemetry downlink) and to the local dashboard. These types describe that
 * shared state. They are intentionally POD (plain old data) so they can be
 * copied by value through FreeRTOS queues without ownership concerns.
 */
#ifndef OBC_TYPES_H
#define OBC_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Identifies which subsystem produced a telemetry sample or event. */
typedef enum {
    OBC_SS_CDH = 0,   /**< Command & Data Handling (the OBC core itself). */
    OBC_SS_EPS,       /**< Electrical Power System. */
    OBC_SS_ADCS,      /**< Attitude Determination & Control System. */
    OBC_SS_THERMAL,   /**< Thermal / housekeeping. */
    OBC_SS_PAYLOAD,   /**< Mission payload. */
    OBC_SS_COUNT
} obc_subsystem_t;

/**
 * @brief Top-level satellite operating mode.
 *
 * Real spacecraft run a mode state machine; autonomy rules (in the mode
 * manager) move the satellite between these on faults or ground command.
 */
typedef enum {
    OBC_MODE_BOOT = 0, /**< Just powered on, subsystems initialising. */
    OBC_MODE_SAFE,     /**< Minimal power, sun-pointing, awaiting ground. */
    OBC_MODE_NOMINAL,  /**< Healthy housekeeping, all subsystems on. */
    OBC_MODE_PAYLOAD,  /**< Payload active (e.g. imaging / experiment). */
    OBC_MODE_FAULT,    /**< A fault was latched; recovery in progress. */
    OBC_MODE_COUNT
} obc_mode_t;

/** Electrical Power System telemetry (one snapshot). */
typedef struct {
    float    bus_voltage_v;   /**< Main power bus voltage [V]. */
    float    bus_current_ma;  /**< Bus current draw [mA]. */
    float    power_mw;        /**< Instantaneous power [mW]. */
    float    battery_soc_pct; /**< Estimated battery state of charge [%]. */
    bool     charging;        /**< True if solar input exceeds load. */
    uint8_t  load_shed_level; /**< 0 = all loads on, higher = more shed. */
} eps_telemetry_t;

/** Attitude Determination & Control telemetry (one snapshot). */
typedef struct {
    float gyro_dps[3];   /**< Body angular rate X/Y/Z [deg/s]. */
    float accel_g[3];    /**< Body acceleration X/Y/Z [g]. */
    float euler_deg[3];  /**< Estimated roll/pitch/yaw [deg]. */
    float rate_rms_dps;  /**< RMS of body rates – a "tumbling" indicator. */
    bool  detumbled;     /**< True once rates are below the safe threshold. */
} adcs_telemetry_t;

/** Thermal / housekeeping telemetry (one snapshot). */
typedef struct {
    float temperature_c;    /**< Primary structure temperature [°C]. */
    float pressure_hpa;     /**< Cabin/reference pressure [hPa] (sim/BME280). */
    float humidity_pct;     /**< Relative humidity [%] (if sensor present). */
    float mcu_temp_c;       /**< On-chip temperature [°C]. */
    bool  heater_on;        /**< Survival heater command state. */
} thermal_telemetry_t;

/**
 * @brief Aggregated "beacon" snapshot of the whole spacecraft.
 *
 * The CDH task assembles this from the latest per-subsystem telemetry and it
 * is what gets downlinked as the periodic beacon and shown on the dashboard.
 */
typedef struct {
    uint32_t            uptime_s;       /**< Seconds since boot. */
    uint32_t            mission_time_s; /**< Mission elapsed time (RTC based). */
    uint32_t            boot_count;     /**< Reboots since first flash (NVS). */
    obc_mode_t          mode;           /**< Current satellite mode. */
    uint16_t            fault_flags;    /**< Bitmask of latched fault flags. */
    eps_telemetry_t     eps;
    adcs_telemetry_t    adcs;
    thermal_telemetry_t thermal;
} obc_beacon_t;

/** Fault flag bits stored in obc_beacon_t.fault_flags / FDIR. */
typedef enum {
    OBC_FAULT_NONE         = 0,
    OBC_FAULT_EPS_UV       = 1u << 0, /**< Bus under-voltage. */
    OBC_FAULT_EPS_OC       = 1u << 1, /**< Over-current. */
    OBC_FAULT_THERMAL_HOT  = 1u << 2, /**< Over-temperature. */
    OBC_FAULT_THERMAL_COLD = 1u << 3, /**< Under-temperature. */
    OBC_FAULT_ADCS_TUMBLE  = 1u << 4, /**< Excessive body rates. */
    OBC_FAULT_SENSOR_LOSS  = 1u << 5, /**< A sensor stopped responding. */
    OBC_FAULT_STORAGE      = 1u << 6, /**< Mass-memory write failure. */
    OBC_FAULT_WATCHDOG     = 1u << 7, /**< A task missed its watchdog check-in. */
} obc_fault_t;

#ifdef __cplusplus
}
#endif

#endif /* OBC_TYPES_H */
