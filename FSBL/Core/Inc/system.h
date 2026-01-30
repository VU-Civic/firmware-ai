#ifndef __SYSTEM_HEADER_H__
#define __SYSTEM_HEADER_H__

#include "common.h"

void system_init(void);
void system_reset(void);
void system_finalize(void);
void system_sleep(void);
void system_feed_watchdog(void);
void system_delay(uint32_t ms);
uint32_t system_get_tick(void);

#endif  // #ifndef __SYSTEM_HEADER_H__
