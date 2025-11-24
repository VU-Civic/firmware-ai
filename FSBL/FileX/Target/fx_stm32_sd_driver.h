#ifndef FX_STM32_SD_DRIVER_H
#define FX_STM32_SD_DRIVER_H

#include "fx_api.h"
#include "stm32n6xx_hal.h"

extern volatile uint8_t sd_rx_cplt;
extern volatile uint8_t sd_tx_cplt;

#define FX_STM32_SD_DEFAULT_TIMEOUT                   (10 * 1000)
#define FX_STM32_SD_INIT                              0
#define FX_STM32_SD_CACHE_MAINTENANCE                 0
#define FX_STM32_SD_DMA_API                           1
#define FX_STM32_SD_INSTANCE                          0
#define FX_STM32_SD_DEFAULT_SECTOR_SIZE               512

#define FX_STM32_SD_CURRENT_TIME()                    HAL_GetTick()
#define FX_STM32_SD_PRE_INIT(_media_ptr)
#define FX_STM32_SD_POST_INIT(_media_ptr)
#define FX_STM32_SD_POST_DEINIT(_media_ptr)
#define FX_STM32_SD_POST_ABORT(_media_ptr)
#define FX_STM32_SD_PRE_READ_TRANSFER(_media_ptr)
#define FX_STM32_SD_POST_READ_TRANSFER(_media_ptr)
#define FX_STM32_SD_READ_TRANSFER_ERROR(_status_)
#define FX_STM32_SD_PRE_WRITE_TRANSFER(_media_ptr)
#define FX_STM32_SD_POST_WRITE_TRANSFER               FX_STM32_SD_POST_READ_TRANSFER
#define FX_STM32_SD_WRITE_TRANSFER_ERROR              FX_STM32_SD_READ_TRANSFER_ERROR

#define FX_STM32_SD_READ_CPLT_NOTIFY() \
  do { \
    uint32_t start = HAL_GetTick(); \
    while (HAL_GetTick() - start < FX_STM32_SD_DEFAULT_TIMEOUT) { \
      if (sd_rx_cplt == 1) break; \
    } \
    if (sd_rx_cplt == 0) return FX_IO_ERROR; \
  } while(0)

#define FX_STM32_SD_WRITE_CPLT_NOTIFY() \
  do { \
    uint32_t start = HAL_GetTick(); \
    while (HAL_GetTick() - start < FX_STM32_SD_DEFAULT_TIMEOUT) { \
      if (sd_tx_cplt == 1) break; \
    } \
    if (sd_tx_cplt == 0) return FX_IO_ERROR; \
  } while(0)

int fx_stm32_sd_init(unsigned int instance);
int fx_stm32_sd_deinit(unsigned int instance);
int fx_stm32_sd_get_status(unsigned int instance);
int fx_stm32_sd_read_blocks(unsigned int instance, UINT *buffer, uint32_t start_block, uint32_t total_blocks);
int fx_stm32_sd_write_blocks(unsigned int instance, const UINT *buffer, uint32_t start_block, uint32_t total_blocks);
void fx_stm32_sd_driver(FX_MEDIA *media_ptr);

#endif  // #ifndef FX_STM32_SD_DRIVER_H
