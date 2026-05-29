/**
 * @file telecommand.h
 * @brief Ground-to-satellite command link (CCSDS-inspired packet protocol).
 *
 * Frame layout on the UART "radio" link (little-endian payload):
 *
 *   +------+------+--------+---------+======== payload ========+------+
 *   | 0xC3 | 0x5A | APID,L | SEQ,LEN |  LEN bytes of params    | CRC16|
 *   +------+------+--------+---------+=========================+------+
 *     sync1  sync2   1+1B     1+1B          0..32B               2B
 *
 *   - sync:   fixed marker 0xC3 0x5A (a nod to "C3S").
 *   - apid:   application process id == target subsystem (obc_subsystem_t).
 *   - opcode: which command (telecommand_op_t), packed with apid into 2 bytes.
 *   - seq:    rolling sequence counter (for duplicate/gap detection).
 *   - len:    payload length.
 *   - crc16:  CCITT over everything after the sync bytes.
 *
 * The framing is deliberately close to a real CCSDS TC packet so the parser
 * exercises sync hunting, length handling and CRC validation — the bread and
 * butter of a spacecraft comms stack.
 */
#ifndef TELECOMMAND_H
#define TELECOMMAND_H

#include <stdint.h>
#include <stddef.h>
#include "obc_errors.h"
#include "obc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TC_SYNC0        0xC3
#define TC_SYNC1        0x5A
#define TC_MAX_PAYLOAD  32

/** Command opcodes understood by the CDH dispatcher. */
typedef enum {
    TC_OP_NOP            = 0x00,
    TC_OP_PING           = 0x01, /**< Reply with a pong event. */
    TC_OP_SET_MODE       = 0x02, /**< payload[0] = obc_mode_t. */
    TC_OP_REQ_BEACON     = 0x03, /**< Force an immediate beacon downlink. */
    TC_OP_CLEAR_FAULTS   = 0x04,
    TC_OP_SET_HEATER     = 0x05, /**< payload[0] = 0/1 manual heater override. */
    TC_OP_DUMP_LOG       = 0x06, /**< Downlink stored telemetry log. */
    TC_OP_REBOOT         = 0x07,
} telecommand_op_t;

/** A decoded, validated command handed to the CDH task via the queue. */
typedef struct {
    obc_subsystem_t  apid;
    telecommand_op_t opcode;
    uint8_t          seq;
    uint8_t          len;
    uint8_t          payload[TC_MAX_PAYLOAD];
} telecommand_t;

/** Start the UART link RX task (decodes frames, enqueues telecommand_t). */
obc_err_t telecommand_start(void);

/**
 * @brief Feed raw bytes into the frame parser (used by the UART task and by
 *        unit tests). Complete, CRC-valid frames are pushed to the command
 *        queue.
 * @return Number of complete frames accepted from this chunk.
 */
int telecommand_feed(const uint8_t *data, size_t len);

/** CCITT CRC16 (poly 0x1021, init 0xFFFF) — shared by encode/decode + tests. */
uint16_t telecommand_crc16(const uint8_t *data, size_t len);

/** Encode a command into a wire frame. @return frame length or <0 on error. */
int telecommand_encode(const telecommand_t *cmd, uint8_t *out, size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* TELECOMMAND_H */
