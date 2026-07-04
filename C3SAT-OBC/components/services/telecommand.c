#include "telecommand.h"
#include "obc_state.h"
#include "bsp_pins.h"
#include "obc_config.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "tc";

uint16_t telecommand_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t) data[i] << 8;
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
        }
    }
    return crc;
}

/* ---- streaming frame parser (state machine) -----------------------------*/
typedef enum { S_SYNC0, S_SYNC1, S_HDR, S_PAYLOAD, S_CRC } pstate_t;

static pstate_t s_state;
static uint8_t  s_hdr[4];      /* apid, opcode, seq, len */
static uint8_t  s_hidx;
static uint8_t  s_payload[TC_MAX_PAYLOAD];
static uint8_t  s_pidx;
static uint8_t  s_crc_buf[2];
static uint8_t  s_cidx;

static void reset_parser(void) { s_state = S_SYNC0; s_hidx = s_pidx = s_cidx = 0; }

static void finish_frame(void)
{
    uint8_t apid   = s_hdr[0];
    uint8_t len    = s_hdr[3];

    /* CRC covers header(4) + payload(len). */
    uint8_t crcbuf[4 + TC_MAX_PAYLOAD];
    memcpy(crcbuf, s_hdr, 4);
    memcpy(crcbuf + 4, s_payload, len);
    uint16_t want = ((uint16_t) s_crc_buf[0] << 8) | s_crc_buf[1];
    uint16_t got  = telecommand_crc16(crcbuf, 4 + len);

    if (want != got) {
        ESP_LOGW(TAG, "CRC drop want=%04X got=%04X", want, got);
        obc_state_post_event(OBC_SS_CDH, "TC CRC error");
        return;
    }
    if (apid >= OBC_SS_COUNT) {
        ESP_LOGW(TAG, "bad APID %u", apid);
        return;
    }
    telecommand_t cmd = {
        .apid = (obc_subsystem_t) apid,
        .opcode = (telecommand_op_t) s_hdr[1],
        .seq = s_hdr[2],
        .len = len,
    };
    memcpy(cmd.payload, s_payload, len);
    if (xQueueSend(obc_state_command_queue(), &cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "command queue full, dropping op=0x%02X", cmd.opcode);
    }
}

int telecommand_feed(const uint8_t *data, size_t len)
{
    int frames = 0;
    for (size_t i = 0; i < len; ++i) {
        uint8_t c = data[i];
        switch (s_state) {
            case S_SYNC0:
                if (c == TC_SYNC0) s_state = S_SYNC1;
                break;
            case S_SYNC1:
                s_state = (c == TC_SYNC1) ? S_HDR : S_SYNC0;
                s_hidx = 0;
                break;
            case S_HDR:
                s_hdr[s_hidx++] = c;
                if (s_hidx == 4) {
                    if (s_hdr[3] > TC_MAX_PAYLOAD) { reset_parser(); break; }
                    s_pidx = 0;
                    s_state = (s_hdr[3] == 0) ? S_CRC : S_PAYLOAD;
                }
                break;
            case S_PAYLOAD:
                s_payload[s_pidx++] = c;
                if (s_pidx == s_hdr[3]) { s_cidx = 0; s_state = S_CRC; }
                break;
            case S_CRC:
                s_crc_buf[s_cidx++] = c;
                if (s_cidx == 2) {
                    finish_frame();
                    frames++;
                    reset_parser();
                }
                break;
        }
    }
    return frames;
}

int telecommand_encode(const telecommand_t *cmd, uint8_t *out, size_t out_sz)
{
    if (cmd == NULL || out == NULL || cmd->len > TC_MAX_PAYLOAD) {
        return -1;
    }
    size_t need = 2 + 4 + cmd->len + 2;
    if (out_sz < need) {
        return -1;
    }
    out[0] = TC_SYNC0;
    out[1] = TC_SYNC1;
    out[2] = (uint8_t) cmd->apid;
    out[3] = (uint8_t) cmd->opcode;
    out[4] = cmd->seq;
    out[5] = cmd->len;
    memcpy(&out[6], cmd->payload, cmd->len);
    uint16_t crc = telecommand_crc16(&out[2], 4 + cmd->len);
    out[6 + cmd->len] = crc >> 8;
    out[7 + cmd->len] = crc & 0xFF;
    return (int) need;
}

/* ---- UART RX task -------------------------------------------------------*/
static void uart_link_task(void *arg)
{
    (void) arg;
    uint8_t buf[128];
    for (;;) {
        int n = uart_read_bytes(BSP_UART_LINK_PORT, buf, sizeof buf,
                                pdMS_TO_TICKS(100));
        if (n > 0) {
            telecommand_feed(buf, (size_t) n);
        }
    }
}

obc_err_t telecommand_start(void)
{
    uart_config_t cfg = {
        .baud_rate = BSP_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        /* Use XTAL, not the PLL-derived default. With power management / light
         * sleep enabled (see the "sleep: ..." lines at boot) the PLL_F80M that
         * UART_SCLK_DEFAULT selects can be gated by DFS. uart_hal_init() then
         * spins forever in uart_ll_update() (a reg_update sync that waits on
         * the UART core clock) inside a critical section -> interrupt watchdog
         * timeout -> boot loop. It was non-deterministic: the same image booted
         * only when the PLL happened to be running. The 40 MHz crystal is
         * always on regardless of DFS/light sleep, so the sync always completes
         * (and 40 MHz is plenty for 115200 baud). */
        .source_clk = UART_SCLK_XTAL,
    };
    ESP_ERROR_CHECK(uart_param_config(BSP_UART_LINK_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(BSP_UART_LINK_PORT, BSP_PIN_UART_TX, BSP_PIN_UART_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(BSP_UART_LINK_PORT, 512, 512, 0, NULL, 0));
    reset_parser();
    if (xTaskCreate(uart_link_task, "uart_link", STACK_UART_LINK, NULL,
                    PRIO_UART_LINK, NULL) != pdPASS) {
        return OBC_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "TC link up on UART%d @ %d baud", BSP_UART_LINK_PORT, BSP_UART_BAUD);
    return OBC_OK;
}
