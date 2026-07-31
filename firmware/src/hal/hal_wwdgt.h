#ifndef HAL_WWDGT_H
#define HAL_WWDGT_H

#include <stdint.h>

void hal_wwdgt_init(uint32_t timeout_ms);
void hal_wwdgt_feed(void);

#endif
