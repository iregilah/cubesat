#include "telemetry.h"
#include "telecommand.h"   /* reuse crc16 + sync markers */
#include "obc_config.h"
#include "bsp_pins.h"

#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "tm";

/* TM frames use the sync markers swapped relative to TC, so a ground listener
 * can tell uplink from downlink on a shared half-duplex line. */
#define TM_SYNC0 TC_SYNC1
#define TM_SYNC1 TC_SYNC0

static QueueHandle_t s_log_q;

obc_err_t telemetry_init(void)
{
    s_log_q = xQueueCreate(Q_DEPTH_TELEMETRY, sizeof(obc_beacon_t));
    return s_log_q ? OBC_OK : OBC_ERR_NO_MEM;
}

QueueHandle_t telemetry_log_queue(void) { return s_log_q; }

obc_err_t telemetry_downlink(const obc_beacon_t *beacon)
{
    if (beacon == NULL) {
        return OBC_ERR_INVALID_ARG;
    }
    /* Frame: SYNC0 SYNC1 | APID(CDH) | LEN | <beacon bytes> | CRC16. */
    uint8_t frame[4 + sizeof(obc_beacon_t) + 2];
    frame[0] = TM_SYNC0;
    frame[1] = TM_SYNC1;
    frame[2] = OBC_SS_CDH;
    frame[3] = (uint8_t) sizeof(obc_beacon_t);
    memcpy(&frame[4], beacon, sizeof(obc_beacon_t));
    uint16_t crc = telecommand_crc16(&frame[2], 2 + sizeof(obc_beacon_t));
    frame[4 + sizeof(obc_beacon_t)] = crc >> 8;
    frame[5 + sizeof(obc_beacon_t)] = crc & 0xFF;

    uart_write_bytes(BSP_UART_LINK_PORT, (const char *) frame, sizeof frame);

    /* Best-effort enqueue for logging; never block a subsystem task on storage. */
    if (s_log_q && xQueueSend(s_log_q, beacon, 0) != pdTRUE) {
        ESP_LOGD(TAG, "log queue full, beacon not stored");
    }
    return OBC_OK;
}
