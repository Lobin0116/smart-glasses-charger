#ifndef CW2017_H
#define CW2017_H

#include <stdint.h>
typedef struct
{
    uint8_t soc;
    uint16_t voltage_mv;
    int8_t temp_c;
} cw2017_status_t;

int cw2017_init(void);

uint8_t cw2017_get_soc(void);

uint16_t cw2017_get_voltage_mv(void);

int8_t cw2017_get_temp_c(void);

int cw2017_get_status(cw2017_status_t *status);

#endif
