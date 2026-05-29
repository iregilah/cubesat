/**
 * @file app_main.c
 * @brief C3SAT-OBC entry point and task orchestration.
 *
 * Boot sequence:
 *   1. Bring up the board (NVS, boot counter, status LED) and the shared
 *      synchronisation hub (obc_state).
 *   2. Initialise the two hardware buses (I2C for sensors, SPI for the panel)
 *      and the services that own peripherals (clock, telemetry, storage).
 *   3. Create every task. Each subsystem signals its EVT_*_READY bit once it
 *      has reached its run loop.
 *   4. Block on the startup barrier until all subsystems are ready (or a
 *      timeout), then leave BOOT mode and hand autonomy to the mode manager.
 *
 * After app_main returns the FreeRTOS scheduler keeps the created tasks
 * running; this function is itself a task that simply exits.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "bsp.h"
#include "obc_state.h"
#include "hal_i2c.h"
#include "hal_spi.h"
#include "clock_svc.h"
#include "telemetry.h"
#include "storage.h"
#include "fdir.h"
#include "mode_manager.h"
#include "telecommand.h"
#include "subsystems.h"
#include "gui.h"

static const char *TAG = "main";

#define CHECK(expr, msg)                                            \
    do {                                                            \
        obc_err_t _rc = (expr);                                     \
        if (_rc != OBC_OK) {                                        \
            ESP_LOGE(TAG, "%s: %s", msg, obc_err_str(_rc));         \
        }                                                           \
    } while (0)

void app_main(void)
{
    ESP_LOGI(TAG, "=== C3SAT-OBC boot ===");

    /* --- 1. board + IPC hub --- */
    CHECK(bsp_init(), "bsp_init");
    CHECK(obc_state_init(), "obc_state_init");
    obc_state_set_mode(OBC_MODE_BOOT);
    ESP_LOGI(TAG, "boot count = %lu", (unsigned long) bsp_boot_count());

    /* --- 2. buses + peripheral-owning services --- */
    CHECK(hal_i2c_init(), "hal_i2c_init");
    CHECK(hal_spi_init(), "hal_spi_init");
    CHECK(clock_svc_init(), "clock_svc_init");
    CHECK(telemetry_init(), "telemetry_init");
    CHECK(storage_init(), "storage_init");
    CHECK(fdir_init(), "fdir_init");
    CHECK(mode_manager_init(), "mode_manager_init");

    /* --- 3. create tasks --- */
    CHECK(fdir_start(), "fdir_start");           /* safety net first */
    CHECK(gui_start(), "gui_start");
    CHECK(eps_start(), "eps_start");
    CHECK(adcs_start(), "adcs_start");
    CHECK(thermal_start(), "thermal_start");
    CHECK(cdh_start(), "cdh_start");
    CHECK(storage_start(), "storage_start");
    CHECK(mode_manager_start(), "mode_manager_start");
    CHECK(telecommand_start(), "telecommand_start");

    /* --- 4. startup barrier --- */
    if (obc_state_wait_all_ready(pdMS_TO_TICKS(10000))) {
        ESP_LOGI(TAG, "all subsystems ready -> leaving BOOT");
        obc_state_post_event(OBC_SS_CDH, "BOOT complete");
        obc_state_set_mode(OBC_MODE_SAFE); /* mode manager takes over from here */
    } else {
        ESP_LOGE(TAG, "startup barrier timed out, faulting");
        obc_state_raise_fault(OBC_FAULT_WATCHDOG);
        obc_state_set_mode(OBC_MODE_FAULT);
    }

    ESP_LOGI(TAG, "init done, free heap = %u B",
             (unsigned) esp_get_free_heap_size());
}
