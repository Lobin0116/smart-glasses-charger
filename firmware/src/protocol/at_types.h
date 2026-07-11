#ifndef AT_TYPES_H
#define AT_TYPES_H

#include <stdint.h>

/* Roles on the POGO link. The first two payload bytes of every command carry
 * the destination and source role, selecting the data flow direction. */
typedef enum {
    AT_CASE_ROLE_CASE = 0,
    AT_CASE_ROLE_GLASS = 1,
    AT_CASE_ROLE_MAGBAG = 2,
    AT_CASE_ROLE_COUNT,
} at_case_role_e;

/* Read/write action carried by control commands. */
typedef enum {
    AT_RW_ACTION_R = 0,
    AT_RW_ACTION_W = 1,
    AT_RW_ACTION_COUNT,
} at_rw_action_e;

/* Position of a packet within an OTA transfer. */
typedef enum {
    AT_PACKET_TYPE_MID = 0,
    AT_PACKET_TYPE_END = 1,
    AT_PACKET_TYPE_COUNT,
} at_packet_type_e;

#pragma pack(push, 1)

/* Routing prefix shared by every payload: destination then source role. */
typedef struct {
    uint8_t des; /* destination, see at_case_role_e */
    uint8_t src; /* source, see at_case_role_e */
} at_case_role;

/* Heartbeat REQ payload (opcode 0x3001). */
typedef struct {
    at_case_role role;
    uint8_t case_soc; /* bit7 = charging */
    uint8_t case_sta; /* bit0 = lid open, bit7 = OTA request */
} at_case_data;

/* Heartbeat RSP payload (opcode 0x3001). */
typedef struct {
    at_case_role role;
    uint8_t glass_soc;    /* bit7 = full (0xE4 = charged) */
    uint8_t glass_sta;    /* bit7 = acknowledge */
    uint8_t case_version; /* case firmware version mirrored by the glasses */
} at_glass_data;

/* OTA prepare RSP payload (opcode 0x3003). */
typedef struct {
    at_case_role role;
    uint32_t size; /* total OTA image size in bytes */
} at_case_packet_prepare;

/* OTA data REQ payload (opcode 0x3004). */
typedef struct {
    at_case_role role;
    uint16_t index; /* packet sequence number */
    uint16_t size;  /* packet payload size in bytes */
} at_case_packet_read;

/* OTA data RSP payload (opcode 0x3004). */
typedef struct {
    at_case_role role;
    uint16_t index; /* packet sequence number */
    uint8_t type;   /* see at_packet_type_e */
    uint8_t data[]; /* OTA payload */
} at_case_packet_transfer;

#pragma pack(pop)

/* Status codes exchanged in the response Status field. */
typedef enum {
    AT_SUCCESS = 0x00,
    AT_ERR_IO = 0x01,
    AT_ERR_TIMEOUT = 0x02,
    AT_ERR_LENGTH = 0x03,
    AT_ERR_PARAMS = 0x04,
    AT_ERR_RESULT = 0x05,
    AT_ERR_CRC = 0x06,
    AT_ERR_CHANNEL = 0x07,
    AT_ERR_HARDWARE = 0x08,
    AT_ERR_UNSUPPORT = 0x09,
    AT_ERR_RUNTIME_MODE = 0x0A,
    AT_ERR_DATA_NOT_READY = 0xFC,
    AT_ERR_NULL = 0xFD,
    AT_ERR_MAGIC = 0xFE,
    AT_ERR_UNKNOWN = 0xFF,
} at_status;

#endif /* AT_TYPES_H */
