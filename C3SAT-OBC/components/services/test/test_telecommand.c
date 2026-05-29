/**
 * @file test_telecommand.c
 * @brief Unity tests for the telecommand frame codec (CRC + encode).
 *
 * These exercise the pure, side-effect-free parts of the comms stack — exactly
 * the layer you want under regression test on a spacecraft, because a CRC or
 * framing bug silently corrupts the command link.
 */
#include "unity.h"
#include "telecommand.h"
#include <string.h>

TEST_CASE("crc16 matches known CCITT-FALSE vector", "[telecommand]")
{
    /* CRC16/CCITT-FALSE("123456789") == 0x29B1 (standard check value). */
    const uint8_t v[] = "123456789";
    TEST_ASSERT_EQUAL_HEX16(0x29B1, telecommand_crc16(v, 9));
}

TEST_CASE("crc16 of empty buffer is the init value", "[telecommand]")
{
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, telecommand_crc16((const uint8_t *) "", 0));
}

TEST_CASE("encode produces a well-formed frame", "[telecommand]")
{
    telecommand_t cmd = {
        .apid = OBC_SS_CDH,
        .opcode = TC_OP_SET_MODE,
        .seq = 0x2A,
        .len = 1,
        .payload = { OBC_MODE_NOMINAL },
    };
    uint8_t frame[16];
    int n = telecommand_encode(&cmd, frame, sizeof frame);

    TEST_ASSERT_EQUAL_INT(2 + 4 + 1 + 2, n);   /* sync+hdr+payload+crc */
    TEST_ASSERT_EQUAL_HEX8(TC_SYNC0, frame[0]);
    TEST_ASSERT_EQUAL_HEX8(TC_SYNC1, frame[1]);
    TEST_ASSERT_EQUAL_HEX8(OBC_SS_CDH, frame[2]);
    TEST_ASSERT_EQUAL_HEX8(TC_OP_SET_MODE, frame[3]);
    TEST_ASSERT_EQUAL_HEX8(0x2A, frame[4]);
    TEST_ASSERT_EQUAL_HEX8(1, frame[5]);
    TEST_ASSERT_EQUAL_HEX8(OBC_MODE_NOMINAL, frame[6]);

    /* The trailing CRC must validate over header+payload. */
    uint16_t crc = telecommand_crc16(&frame[2], 4 + 1);
    TEST_ASSERT_EQUAL_HEX8(crc >> 8, frame[7]);
    TEST_ASSERT_EQUAL_HEX8(crc & 0xFF, frame[8]);
}

TEST_CASE("encode rejects oversize payload", "[telecommand]")
{
    telecommand_t cmd = { .len = TC_MAX_PAYLOAD + 1 };
    uint8_t frame[64];
    TEST_ASSERT_LESS_THAN_INT(0, telecommand_encode(&cmd, frame, sizeof frame));
}
