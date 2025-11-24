// Header Inclusions ---------------------------------------------------------------------------------------------------

#include "fx_stm32_sd_driver.h"
#include "storage.h"


// Storage Data Type Definitions ---------------------------------------------------------------------------------------

// TODO:


// Static Storage Variables --------------------------------------------------------------------------------------------
// TODO: MAKE THESE STATIC - REMOVE CALLS FROM OTHER FILES
SD_HandleTypeDef sd_handle;
volatile uint8_t sd_rx_cplt;
volatile uint8_t sd_tx_cplt;


// Interrupt Service Routines ------------------------------------------------------------------------------------------

void SDMMC1_IRQHandler(void) { HAL_SD_IRQHandler(&sd_handle); }
void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd) { sd_tx_cplt = 1; }
void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd) { sd_rx_cplt = 1; }
void sd_card_detection_isr(uint8_t sd_card_detected) {}


// FileX Required Driver Functions -------------------------------------------------------------------------------------

int fx_stm32_sd_init(unsigned int instance) { return 0; }
int fx_stm32_sd_deinit(unsigned int instance) { return 0; }
int fx_stm32_sd_get_status(unsigned int instance) { return (HAL_SD_GetCardState(&sd_handle) != HAL_SD_CARD_TRANSFER); }

int fx_stm32_sd_read_blocks(unsigned int instance, UINT *buffer, uint32_t start_block, uint32_t total_blocks)
{
   sd_rx_cplt = 0;
   return (HAL_SD_ReadBlocks_DMA(&sd_handle, (uint8_t*)buffer, start_block, total_blocks) != HAL_OK);
}

int fx_stm32_sd_write_blocks(unsigned int instance, const UINT *buffer, uint32_t start_block, uint32_t total_blocks)
{
   sd_tx_cplt = 0;
   return (HAL_SD_WriteBlocks_DMA(&sd_handle, (const uint8_t*)buffer, start_block, total_blocks) != HAL_OK);
}


// Public API Functions ------------------------------------------------------------------------------------------------

void HAL_SD_MspInit(SD_HandleTypeDef* hsd)
{
  GPIO_InitTypeDef GPIO_InitStruct = { 0 };
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = { 0 };
  if (hsd->Instance == SDMMC1)
  {
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SDMMC1;
    PeriphClkInitStruct.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
      Error_Handler();

    __HAL_RCC_SDMMC1_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_SDMMC1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    GPIO_InitStruct.Alternate = GPIO_AF10_SDMMC1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    GPIO_InitStruct.Alternate = GPIO_AF10_SDMMC1;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(SDMMC1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(SDMMC1_IRQn);
  }
}

void storage_init(void)
{
   // Turn on power to the SD card
   HAL_GPIO_WritePin(SD_CARD_EN_GPIO_Port, SD_CARD_EN_Pin, GPIO_PIN_SET);

   // Initialize the SDMMC peripheral for high-speed access (25MB/s)
   // TODO: EXPERIMENT WITH SWITCHING TO 50MB/s ACCESS
   sd_handle.Instance = SDMMC1;
   sd_handle.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
   sd_handle.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
   sd_handle.Init.BusWide = SDMMC_BUS_WIDE_4B;
   sd_handle.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
   sd_handle.Init.ClockDiv = SDMMC_HSPEED_CLK_DIV;
   if (HAL_SD_Init(&sd_handle) != HAL_OK)
      Error_Handler();
}
