/**
  ******************************************************************************
  * @file    npu_cache.c
  * @brief   Implementation of NPU-cache-handling functions (CACHEAXI)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
#include <assert.h>

#include "npu_cache.h"

#define USE_HAL_DRIVER  // Define here to fix header issues, must clean projects using only stm32n6xx.h for application code
#include "stm32n6xx.h"

#include "ll_aton_config.h"

#if (LL_ATON_PLATFORM != LL_ATON_PLAT_STM32N6)
#error "LL_ATON_PLATFORM should be equal to LL_ATON_PLAT_STM32N6"
#endif

#define CACHEAXI_COMMAND_CLEAN                   CACHEAXI_CR2_CACHECMD_0
#define CACHEAXI_COMMAND_CLEAN_INVALIDATE        (CACHEAXI_CR2_CACHECMD_0|CACHEAXI_CR2_CACHECMD_1)

void npu_cache_enable(void)
{
   // Wait until the cache is idle then enable it
   while (READ_BIT(CACHEAXI->SR, CACHEAXI_SR_BUSYF));
   SET_BIT(CACHEAXI->CR1, CACHEAXI_CR1_EN);
}

void npu_cache_disable(void)
{
   // Disable the cache then wait for it to go idle
   CLEAR_BIT(CACHEAXI->CR1, CACHEAXI_CR1_EN);
   while (READ_BIT(CACHEAXI->SR, (CACHEAXI_SR_BUSYF | CACHEAXI_SR_BUSYCMDF)));
}

void npu_cache_invalidate(void)
{
   // Ensure there is no ongoing transaction
   if (!READ_BIT(CACHEAXI->SR, (CACHEAXI_SR_BUSYF | CACHEAXI_SR_BUSYCMDF)))
   {
      // Reset all flags and set no operation on the address range
      WRITE_REG(CACHEAXI->FCR, (CACHEAXI_FCR_CBSYENDF | CACHEAXI_FCR_CCMDENDF));
      MODIFY_REG(CACHEAXI->CR2, CACHEAXI_CR2_CACHECMD, 0U);

      // Launch cache invalidation and wait for it to end
      SET_BIT(CACHEAXI->CR1, CACHEAXI_CR1_CACHEINV);
      while (READ_BIT(CACHEAXI->SR, CACHEAXI_SR_BUSYF));
   }
}

void npu_cache_clean_range(uint32_t start_addr, uint32_t end_addr)
{
   // Ensure there is no ongoing transaction
   if (!READ_BIT(CACHEAXI->SR, (CACHEAXI_SR_BUSYF | CACHEAXI_SR_BUSYCMDF)))
   {
      // Reset all flags and set the address range
      WRITE_REG(CACHEAXI->FCR, (CACHEAXI_FCR_CBSYENDF | CACHEAXI_FCR_CCMDENDF));
      WRITE_REG(CACHEAXI->CMDRSADDRR, start_addr);
      WRITE_REG(CACHEAXI->CMDREADDRR, (end_addr - 1U));

      // Set the invalidation command, launch the process, and wait for it to end
      MODIFY_REG(CACHEAXI->CR2, CACHEAXI_CR2_CACHECMD, CACHEAXI_COMMAND_CLEAN);
      CLEAR_BIT(CACHEAXI->IER, CACHEAXI_IER_CMDENDIE);
      SET_BIT(CACHEAXI->CR2, CACHEAXI_CR2_STARTCMD);
      while (!READ_BIT(CACHEAXI->SR, CACHEAXI_SR_CMDENDF));
   }
}

void npu_cache_clean_invalidate_range(uint32_t start_addr, uint32_t end_addr)
{
   // Ensure there is no ongoing transaction
   if (!READ_BIT(CACHEAXI->SR, (CACHEAXI_SR_BUSYF | CACHEAXI_SR_BUSYCMDF)))
   {
      // Reset all flags and set the address range
      WRITE_REG(CACHEAXI->FCR, (CACHEAXI_FCR_CBSYENDF | CACHEAXI_FCR_CCMDENDF));
      WRITE_REG(CACHEAXI->CMDRSADDRR, start_addr);
      WRITE_REG(CACHEAXI->CMDREADDRR, (end_addr - 1U));

      // Set the invalidation command, launch the process, and wait for it to end
      MODIFY_REG(CACHEAXI->CR2, CACHEAXI_CR2_CACHECMD, CACHEAXI_COMMAND_CLEAN_INVALIDATE);
      CLEAR_BIT(CACHEAXI->IER, CACHEAXI_IER_CMDENDIE);
      SET_BIT(CACHEAXI->CR2, CACHEAXI_CR2_STARTCMD);
      while (!READ_BIT(CACHEAXI->SR, CACHEAXI_SR_CMDENDF));
   }
}

/* 
  Weak functions called by HAL_CACHEAXI_Init/DeInit implementations 
*/
void HAL_CACHEAXI_MspInit(CACHEAXI_HandleTypeDef *hcacheaxi)
{
  (void)hcacheaxi;
  npu_cache_enable_clocks_and_reset();
}

void HAL_CACHEAXI_MspDeInit(CACHEAXI_HandleTypeDef *hcacheaxi)
{
  (void)hcacheaxi;
  npu_cache_disable_clocks_and_reset();
}

/*
  Exposed weak functions to be implemented by the user if needed:
  The CACHEAXI IP must be clocked and reset before use.
*/
__weak void npu_cache_enable_clocks_and_reset(void){};
__weak void npu_cache_disable_clocks_and_reset(void){};
