#ifndef __MCU_CACHE_HEADER_H__
#define __MCU_CACHE_HEADER_H__

#include "stm32n6xx_hal.h"

__STATIC_FORCEINLINE int mcu_cache_enabled(void) { return (SCB->CCR & SCB_CCR_DC_Msk); }

int mcu_cache_enable(void);
int mcu_cache_disable(void);
int mcu_cache_invalidate(void);
int mcu_cache_clean(void);
int mcu_cache_clean_invalidate(void);
int mcu_cache_invalidate_range(uint32_t start_addr, uint32_t end_addr);
int mcu_cache_clean_range(uint32_t start_addr, uint32_t end_addr);
int mcu_cache_clean_invalidate_range(uint32_t start_addr, uint32_t end_addr);
void set_mcu_cache_state(uint8_t i_cache_state, uint8_t d_cache_state);

#endif   // #ifndef __MCU_CACHE_HEADER_H__
