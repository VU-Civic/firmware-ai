#ifndef __SYSTEM_HEADER_H__
#define __SYSTEM_HEADER_H__

#include "common.h"

void chip_reset(void);
void chip_initialize_unused_pins(void);
void cpu_init(void);
void cpu_sleep(void);
uint32_t system_start_execution_timer(void);
uint32_t system_get_execution_time_us(uint32_t start_count);

#endif  // #ifndef __SYSTEM_HEADER_H__
