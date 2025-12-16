#ifndef __SYSTEM_HEADER_H__
#define __SYSTEM_HEADER_H__

#include "common.h"

void system_init(void);
void system_reset(void);
void system_finalize(void);
void system_sleep(void);
uint32_t system_start_execution_timer(void);
uint32_t system_get_execution_time_us(uint32_t start_count);
void system_delay(uint32_t ms);

#endif  // #ifndef __SYSTEM_HEADER_H__
