#ifndef AT_OPCODE_H
#define AT_OPCODE_H

/* AT command opcodes carried in the frame Opcode field (0x30xx band). */
typedef enum {
    AT_OPCODE_CASE_HEART = 0x3001,
    AT_OPCODE_CASE_SHUTDOWN = 0x3002,
    AT_OPCODE_CASE_PACKET_PREPARE = 0x3003,
    AT_OPCODE_CASE_PACKET_READ = 0x3004,
} at_opcode_e;

#endif /* AT_OPCODE_H */
