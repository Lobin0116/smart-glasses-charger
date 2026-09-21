#ifndef AT_TYPES_H
#define AT_TYPES_H

#include <stdint.h>

typedef enum
{
    AT_CASE_ROLE_CASE = 0,
    AT_CASE_ROLE_GLASS = 1,
    AT_CASE_ROLE_MAGBAG = 2,
    AT_CASE_ROLE_COUNT,
} at_case_role_e;

typedef enum
{
    AT_RW_ACTION_R = 0,
    AT_RW_ACTION_W = 1,
    AT_RW_ACTION_COUNT,
} at_rw_action_e;

typedef enum
{
    AT_PACKET_TYPE_MID = 0,
    AT_PACKET_TYPE_END = 1,
    AT_PACKET_TYPE_COUNT,
} at_packet_type_e;

#pragma pack(push, 1)

typedef struct
{
    uint8_t des;
    uint8_t src;
} at_case_role;

typedef struct
{
    at_case_role role;
    uint8_t case_soc;
    uint8_t case_sta;
} at_case_data;

typedef struct
{
    at_case_role role;
    uint8_t glass_soc;
    uint8_t glass_sta;
    uint8_t case_version;
} at_glass_data;

typedef struct
{
    at_case_role role;
    uint32_t size;
} at_case_packet_prepare;

typedef struct
{
    at_case_role role;
    uint16_t index;
    uint16_t size;
} at_case_packet_read;

typedef struct
{
    at_case_role role;
    uint16_t index;
    uint8_t type;
    uint8_t data[];
} at_case_packet_transfer;

#pragma pack(pop)

typedef enum
{
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

#endif
