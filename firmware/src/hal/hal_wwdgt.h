#ifndef HAL_WWDGT_H
#define HAL_WWDGT_H

#include <stdint.h>

/* Initialize + enable WWDGT with the given timeout (ms). Called by App's
 * board_init. BL does not use WWDGT (reset leaves it off; BL runs unguarded
 * for the ~3 s copy at 8 MHz IRC). */
void hal_wwdgt_init(uint32_t timeout_ms);

void hal_wwdgt_feed(void);

#endif
