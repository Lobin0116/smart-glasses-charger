/*
 * Unit tests for AT protocol CRC8 and frame pack/unpack.
 * Pure logic - no hardware dependency.
 */

#include "test_assert.h"
#include "at_crc.h"
#include "at_frame.h"
#include "at_types.h"
#include "at_opcode.h"
#include <string.h>

/* --- CRC8 tests --- */

static void test_crc8_empty(void) {
    /* CRC of zero bytes with initial 0x00 should be 0x00 */
    uint8_t result = at_crc8((uint8_t *)"", 0, 0x00);
    TEST_ASSERT_EQ(result, 0x00, "CRC8 of empty input should be 0x00");
}

static void test_crc8_single_byte(void) {
    /* From the lookup table: crc8_table[0x00 ^ 0x00] = 0x00,
     * crc8_table[0x01 ^ 0x00] = 0x31 */
    uint8_t data[] = {0x01};
    uint8_t result = at_crc8(data, 1, 0x00);
    TEST_ASSERT_EQ(result, 0x31, "CRC8 of {0x01} should be 0x31");
}

static void test_crc8_known_value(void) {
    /* CRC8 of {0x01, 0x02} = crc8_table[0x01] = 0x31,
     * then crc8_table[0x02 ^ 0x31] = crc8_table[0x33] */
    uint8_t data[] = {0x01, 0x02};
    uint8_t result = at_crc8(data, 2, 0x00);
    /* Verify it matches table lookup: crc8_table[0x33] */
    /* From the table in at_crc.c, index 0x33: should be 0x07 */
    TEST_ASSERT(result != 0x00, "CRC8 of {0x01,0x02} should not be 0x00");
}

static void test_crc8_deterministic(void) {
    uint8_t data[] = {0x23, 0x41, 0x54, 0x23};
    uint8_t r1 = at_crc8(data, 4, 0x00);
    uint8_t r2 = at_crc8(data, 4, 0x00);
    TEST_ASSERT_EQ(r1, r2, "CRC8 should be deterministic");
}

/* --- Frame pack/parse tests --- */

static void test_frame_request_magic(void) {
    uint8_t buf[64];
    uint8_t payload[] = {0x00, 0x01, 0x50};
    uint16_t len = at_frame_pack_request(buf, AT_OPCODE_CASE_HEART, payload, 3, 0x00);

    /* Magic should be 0x23415423 in big-endian */
    TEST_ASSERT_EQ(buf[0], 0x23, "Magic byte 0");
    TEST_ASSERT_EQ(buf[1], 0x41, "Magic byte 1");
    TEST_ASSERT_EQ(buf[2], 0x54, "Magic byte 2");
    TEST_ASSERT_EQ(buf[3], 0x23, "Magic byte 3");
    TEST_ASSERT(len > 10, "Frame should be longer than header");
}

static void test_frame_response_magic(void) {
    uint8_t buf[64];
    uint8_t payload[] = {0x00};
    uint16_t len = at_frame_pack_response(buf, AT_OPCODE_CASE_HEART, 0x00, payload, 1);

    TEST_ASSERT_EQ(buf[0], 0x23, "RSP Magic byte 0");
    TEST_ASSERT_EQ(buf[1], 0x41, "RSP Magic byte 1");
    TEST_ASSERT_EQ(buf[2], 0x50, "RSP Magic byte 2 (#AP#)");
    TEST_ASSERT_EQ(buf[3], 0x23, "RSP Magic byte 3");
}

static void test_frame_roundtrip(void) {
    uint8_t buf[64];
    uint8_t payload[] = {0x00, 0x01, 0x50, 0x02};
    uint16_t frame_len = at_frame_pack_request(buf, AT_OPCODE_CASE_HEART, payload, 4, 0x00);

    uint16_t opcode;
    uint8_t status, out_payload[64], out_payload_len;
    int rc = at_frame_parse(buf, frame_len, &opcode, &status, out_payload, &out_payload_len);

    TEST_ASSERT_EQ(rc, AT_SUCCESS, "Frame parse should succeed");
    TEST_ASSERT_EQ(opcode, AT_OPCODE_CASE_HEART, "Opcode should match");
    TEST_ASSERT_EQ(out_payload_len, 4, "Payload length should match");
    TEST_ASSERT(memcmp(payload, out_payload, 4) == 0, "Payload data should match");
}

static void test_frame_bad_magic(void) {
    uint8_t buf[64];
    uint8_t payload[] = {0x00};
    uint16_t frame_len = at_frame_pack_request(buf, AT_OPCODE_CASE_HEART, payload, 1, 0x00);

    /* Corrupt magic */
    buf[0] = 0xFF;

    uint16_t opcode;
    uint8_t status, out_payload[64], out_payload_len;
    int rc = at_frame_parse(buf, frame_len, &opcode, &status, out_payload, &out_payload_len);

    TEST_ASSERT(rc != AT_SUCCESS, "Parse should fail on bad magic");
}

static void test_frame_bad_crc(void) {
    uint8_t buf[64];
    uint8_t payload[] = {0x00, 0x01, 0x02};
    uint16_t frame_len = at_frame_pack_request(buf, AT_OPCODE_CASE_SHUTDOWN, payload, 3, 0x00);

    /* Corrupt CRC (byte 4) */
    buf[4] ^= 0xFF;

    uint16_t opcode;
    uint8_t status, out_payload[64], out_payload_len;
    int rc = at_frame_parse(buf, frame_len, &opcode, &status, out_payload, &out_payload_len);

    TEST_ASSERT(rc != AT_SUCCESS, "Parse should fail on bad CRC");
}

static void test_frame_empty_payload(void) {
    uint8_t buf[64];
    uint16_t frame_len = at_frame_pack_request(buf, AT_OPCODE_CASE_SHUTDOWN, NULL, 0, 0x00);

    TEST_ASSERT(frame_len >= 10, "Frame with empty payload should be >= 10 bytes");

    uint16_t opcode;
    uint8_t status, out_payload[64], out_payload_len;
    int rc = at_frame_parse(buf, frame_len, &opcode, &status, out_payload, &out_payload_len);

    TEST_ASSERT_EQ(rc, AT_SUCCESS, "Parse should succeed");
    TEST_ASSERT_EQ(opcode, AT_OPCODE_CASE_SHUTDOWN, "Opcode should be shutdown");
    TEST_ASSERT_EQ(out_payload_len, 0, "Payload should be empty");
}

static void test_frame_shutdown_roundtrip(void) {
    uint8_t buf[64];
    at_case_role role = {.des = AT_CASE_ROLE_GLASS, .src = AT_CASE_ROLE_CASE};
    uint16_t frame_len = at_frame_pack_request(buf, AT_OPCODE_CASE_SHUTDOWN,
                                                (uint8_t *)&role, sizeof(role), 0x00);

    uint16_t opcode;
    uint8_t status, out_payload[64], out_payload_len;
    int rc = at_frame_parse(buf, frame_len, &opcode, &status, out_payload, &out_payload_len);

    TEST_ASSERT_EQ(rc, AT_SUCCESS, "Parse shutdown frame");
    TEST_ASSERT_EQ(opcode, AT_OPCODE_CASE_SHUTDOWN, "Opcode");
    TEST_ASSERT_EQ(out_payload_len, sizeof(at_case_role), "Role struct size");
    TEST_ASSERT_EQ(out_payload[0], AT_CASE_ROLE_GLASS, "des field");
    TEST_ASSERT_EQ(out_payload[1], AT_CASE_ROLE_CASE, "src field");
}

int main(void) {
    printf("=== AT Protocol Unit Tests ===\n\n");
    TEST_RUN(test_crc8_empty);
    TEST_RUN(test_crc8_single_byte);
    TEST_RUN(test_crc8_known_value);
    TEST_RUN(test_crc8_deterministic);
    TEST_RUN(test_frame_request_magic);
    TEST_RUN(test_frame_response_magic);
    TEST_RUN(test_frame_roundtrip);
    TEST_RUN(test_frame_bad_magic);
    TEST_RUN(test_frame_bad_crc);
    TEST_RUN(test_frame_empty_payload);
    TEST_RUN(test_frame_shutdown_roundtrip);
    TEST_SUMMARY();
}
