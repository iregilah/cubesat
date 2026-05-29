#include "storage.h"
#include "telemetry.h"
#include "obc_state.h"
#include "obc_config.h"

#include "esp_spiffs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "storage";

#define MOUNT_POINT   "/mass"
#define LOG_PATH      MOUNT_POINT "/tlm.csv"
#define LOG_PATH_OLD  MOUNT_POINT "/tlm.old"
#define LOG_MAX_BYTES (256 * 1024)   /* rotate at 256 KB */

static bool s_mounted;

obc_err_t storage_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = MOUNT_POINT,
        .partition_label = "massmem",
        .max_files = 4,
        .format_if_mount_failed = true,
    };
    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mount failed: %s", esp_err_to_name(err));
        return OBC_ERR_FAIL;
    }
    s_mounted = true;
    size_t used = 0, total = 0;
    storage_stats(&used, &total);
    ESP_LOGI(TAG, "mass memory mounted: %u/%u KB used",
             (unsigned) (used / 1024), (unsigned) (total / 1024));
    return OBC_OK;
}

void storage_stats(size_t *used, size_t *total)
{
    if (!s_mounted) { if (used) *used = 0; if (total) *total = 0; return; }
    esp_spiffs_info("massmem", total, used);
}

static void rotate_if_needed(void)
{
    FILE *f = fopen(LOG_PATH, "r");
    if (!f) {
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    if (sz >= LOG_MAX_BYTES) {
        remove(LOG_PATH_OLD);
        rename(LOG_PATH, LOG_PATH_OLD);
        obc_state_post_event(OBC_SS_CDH, "TLM log rotated");
    }
}

static void storage_task(void *arg)
{
    (void) arg;
    QueueHandle_t q = telemetry_log_queue();
    obc_beacon_t b;
    uint32_t since_flush = 0;
    FILE *f = NULL;

    for (;;) {
        if (xQueueReceive(q, &b, pdMS_TO_TICKS(PERIOD_STORAGE_FLUSH_MS)) == pdTRUE) {
            rotate_if_needed();
            if (!f) {
                f = fopen(LOG_PATH, "a");
            }
            if (f) {
                /* Compact CSV: time,mode,faults,Vbus,Ibus,SoC,temp,rate */
                fprintf(f, "%lu,%d,0x%02X,%.3f,%.1f,%.1f,%.1f,%.2f\n",
                        (unsigned long) b.uptime_s, b.mode, b.fault_flags,
                        b.eps.bus_voltage_v, b.eps.bus_current_ma,
                        b.eps.battery_soc_pct, b.thermal.temperature_c,
                        b.adcs.rate_rms_dps);
            } else {
                obc_state_raise_fault(OBC_FAULT_STORAGE);
            }
        }
        /* Periodically flush so a reset loses at most PERIOD_STORAGE_FLUSH_MS. */
        if (f && (++since_flush >= 1)) {
            fflush(f);
            fclose(f);
            f = NULL;
            since_flush = 0;
        }
    }
}

obc_err_t storage_start(void)
{
    if (!s_mounted) {
        return OBC_ERR_FAIL;
    }
    if (xTaskCreate(storage_task, "storage", STACK_STORAGE, NULL,
                    PRIO_STORAGE, NULL) != pdPASS) {
        return OBC_ERR_NO_MEM;
    }
    return OBC_OK;
}

void storage_dump(void)
{
    FILE *f = fopen(LOG_PATH, "r");
    if (!f) {
        ESP_LOGW(TAG, "no telemetry log to dump");
        return;
    }
    char line[160];
    ESP_LOGI(TAG, "---- telemetry log dump ----");
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = '\0';
        ESP_LOGI(TAG, "%s", line);
    }
    fclose(f);
    ESP_LOGI(TAG, "---- end of dump ----");
}
