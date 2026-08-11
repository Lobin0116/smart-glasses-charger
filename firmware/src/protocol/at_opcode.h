#ifndef AT_OPCODE_H
#define AT_OPCODE_H

#include <stdint.h>

/* AT command opcodes carried in the frame Opcode field (0x30xx band). */
typedef enum
{
    AT_OPCODE_CASE_HEART = 0x3001,
    AT_OPCODE_CASE_SHUTDOWN = 0x3002,
    AT_OPCODE_CASE_PACKET_PREPARE = 0x3003,
    AT_OPCODE_CASE_PACKET_READ = 0x3004,
    /* HIL test opcodes (only compiled when HIL_TEST is defined). Same frame
     * format as the production opcodes so they ride the same magic/CRC path.
     * at_frame_recv's expected_opcode filter lets these coexist with heartbeat
     * traffic without charge_poll swallowing them. */
    AT_OPCODE_HIL_RESET = 0x3010,
    AT_OPCODE_HIL_OPEN = 0x3011,
    AT_OPCODE_HIL_CLOSE = 0x3012,
    AT_OPCODE_HIL_KEY = 0x3013,
    AT_OPCODE_HIL_STATUS = 0x3014,
    AT_OPCODE_HIL_SCAN = 0x3015,
    AT_OPCODE_HIL_OTA = 0x3016,
} at_opcode_e;

#endif /* AT_OPCODE_H */
