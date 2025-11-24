#include "ai.h"
#include "app_filex.h"
#include "comms.h"
#include "flash.h"
#include "storage.h"
#include "system.h"

#define HSLV_OTP 124
#define VDDIO2_HSLV_MASK (1U<<16)
#define VDDIO3_HSLV_MASK (1U<<15)
#define VDDIO4_HSLV_MASK (1U<<14)

__attribute__ ((section(".noncacheable")))
static ai_data_t ai_data;

static void SystemClock_Config(void)
{
  // Ensure that the CPU and SYS clocks are sourced from HSI
  __HAL_RCC_CPUCLK_CONFIG(RCC_CPUCLKSOURCE_HSI);
  __HAL_RCC_SYSCLK_CONFIG(RCC_SYSCLKSOURCE_HSI);

  // Configure the system to use an external SMPS power supply
  RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
  RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };
  if (HAL_PWREx_ConfigSupply(PWR_EXTERNAL_SOURCE_SUPPLY) != HAL_OK)
    Error_Handler();

  // Reset all PLLs to start from a known good configuration
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    Error_Handler();

  // Initialize the system to use HSI for the CPU and SYS clocks
  HAL_RCC_GetClockConfig(&RCC_ClkInitStruct);
  if ((RCC_ClkInitStruct.CPUCLKSource == RCC_CPUCLKSOURCE_IC1) || (RCC_ClkInitStruct.SYSCLKSource == RCC_SYSCLKSOURCE_IC2_IC6_IC11))
  {
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK);
    RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_HSI;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
      Error_Handler();
  }

  // Enable PLL1 to output at 2400MHz
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_NONE;
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL1.PLLM = 2;
  RCC_OscInitStruct.PLL1.PLLN = 75;
  RCC_OscInitStruct.PLL1.PLLFractional = 0;
  RCC_OscInitStruct.PLL1.PLLP1 = 1;
  RCC_OscInitStruct.PLL1.PLLP2 = 1;
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    Error_Handler();

  // Configure the various peripheral clock sources
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_CPUCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2|RCC_CLOCKTYPE_PCLK5|RCC_CLOCKTYPE_PCLK4;
  RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
  RCC_ClkInitStruct.APB5CLKDivider = RCC_APB5_DIV1;
  RCC_ClkInitStruct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;     // IC1 used for CPU clock @ 600MHz
  RCC_ClkInitStruct.IC1Selection.ClockDivider = 4;
  RCC_ClkInitStruct.IC2Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;     // IC2 used for SYS clock @ 400MHz
  RCC_ClkInitStruct.IC2Selection.ClockDivider = 6;
  RCC_ClkInitStruct.IC6Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;     // IC6 used for NPU @ 800MHz
  RCC_ClkInitStruct.IC6Selection.ClockDivider = 3;
  RCC_ClkInitStruct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;    // IC11 used for AXISRAM 3-6 @ 800MHz
  RCC_ClkInitStruct.IC11Selection.ClockDivider = 3;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
    Error_Handler();
}

void HAL_MspInit(void)
{
  BSEC_HandleTypeDef hbsec = { .Instance = BSEC, .ErrorCode = 0 };
  uint32_t fuse_data = 0, expected_data = VDDIO2_HSLV_MASK | VDDIO3_HSLV_MASK; // TODO: DO WE NEED TO BLOW THE VDDIO4 FUSE AS WELL

  __HAL_RCC_BSEC_CLK_ENABLE();
  __HAL_RCC_SYSCFG_CLK_ENABLE();

  if (HAL_BSEC_OTP_Read(&hbsec, HSLV_OTP, &fuse_data) != HAL_OK)
    Error_Handler();
  if ((fuse_data & expected_data) != expected_data)
  {
    fuse_data |= expected_data;
    if (HAL_BSEC_OTP_Program(&hbsec, HSLV_OTP, fuse_data, HAL_BSEC_NORMAL_PROG) != HAL_OK)
      Error_Handler();
  }
  __HAL_RCC_PWR_CLK_ENABLE();

  HAL_PWREx_EnableVddIO2();
  HAL_PWREx_ConfigVddIORange(PWR_VDDIO2,PWR_VDDIO_RANGE_1V8);

  HAL_PWREx_EnableVddIO3();
  HAL_PWREx_ConfigVddIORange(PWR_VDDIO3,PWR_VDDIO_RANGE_1V8);

  HAL_PWREx_EnableVddIO4();
  HAL_PWREx_ConfigVddIORange(PWR_VDDIO4,PWR_VDDIO_RANGE_3V3);

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  HAL_SYSCFG_ConfigVDDCompensationCell(SYSCFG_IO_REGISTER_CODE, 0x7, 0x8);
  HAL_SYSCFG_ConfigVDDIOCompensationCell(SYSCFG_IO_VDDIO2_CELL, SYSCFG_IO_REGISTER_CODE, 0x7, 0x8);
  HAL_SYSCFG_ConfigVDDIOCompensationCell(SYSCFG_IO_VDDIO3_CELL, SYSCFG_IO_REGISTER_CODE, 0x7, 0x8);
  HAL_SYSCFG_ConfigVDDIOCompensationCell(SYSCFG_IO_VDDIO4_CELL, SYSCFG_IO_REGISTER_CODE, 0x7, 0x8);

  HAL_PWREx_DisableVddIO2();
}

static void MPU_Config(void)
{
  // Disable global interrupts
  uint32_t primask_bit = __get_PRIMASK();
  __disable_irq();
  HAL_MPU_Disable();

  // Create a non-cacheable attribute configuration for the MPU
  MPU_Attributes_InitTypeDef MPU_AttributesInit = {
    .Number = MPU_ATTRIBUTES_NUMBER0,
    .Attributes = INNER_OUTER(MPU_NOT_CACHEABLE)
  };
  HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);

  // Set up a non-cacheable memory region
  MPU_Region_InitTypeDef MPU_InitStruct = {
    .Enable = MPU_REGION_ENABLE,
    .Number = MPU_REGION_NUMBER0,
    .BaseAddress = __NON_CACHEABLE_SECTION_BEGIN,
    .LimitAddress = __NON_CACHEABLE_SECTION_END,
    .AttributesIndex = MPU_ATTRIBUTES_NUMBER0,
    .AccessPermission = MPU_REGION_ALL_RW,
    .DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE,
    .DisablePrivExec = MPU_PRIV_INSTRUCTION_ACCESS_DISABLE,
    .IsShareable = MPU_ACCESS_OUTER_SHAREABLE
  };
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  // Enable the MPU and re-enable interrupts
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
  __set_PRIMASK(primask_bit);
}

static void GPIO_Init(void)
{
  // Enable all required GPIO clocks
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPION_CLK_ENABLE();

  // Set the initial state for required GPIO lines
  HAL_GPIO_WritePin(GPIOE, SD_PWR_SELECT_Pin|SD_CARD_EN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_MCU_STATUS_GPIO_Port, LED_MCU_STATUS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AI_OVERDRIVE_GPIO_Port, AI_OVERDRIVE_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(HOST_WAKEUP_GPIO_Port, HOST_WAKEUP_Pin, GPIO_PIN_RESET);

  // Initialize the various GPIO pins
  GPIO_InitTypeDef GPIO_InitStruct = { 0 };
  GPIO_InitStruct.Pin = SD_PWR_SELECT_Pin|SD_CARD_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LED_MCU_STATUS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_MCU_STATUS_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = AI_OVERDRIVE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(AI_OVERDRIVE_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = HOST_WAKEUP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(HOST_WAKEUP_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SD_CARD_DETECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SD_CARD_DETECT_GPIO_Port, &GPIO_InitStruct);

  // Enable interrupts based on the SD_CARD_DETECT pin
  HAL_EXTI_ConfigLineAttributes(EXTI_LINE_12, EXTI_LINE_SEC);
  HAL_NVIC_SetPriority(EXTI12_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI12_IRQn);
}

static void RAMCFG_Init(void)
{
  // Enable the necessary peripheral clocks
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_CRC_CLK_ENABLE();
  __HAL_RCC_RAMCFG_CLK_ENABLE();

  // Power on all AXISRAM memory banks and the ICACHE
  RCC->MEMENR |= RCC_MEMENR_AXISRAM2EN | RCC_MEMENR_AXISRAM3EN | RCC_MEMENR_AXISRAM4EN | RCC_MEMENR_AXISRAM5EN | RCC_MEMENR_AXISRAM6EN;
  RCC->MEMENR |= RCC_MEMENR_CACHEAXIRAMEN;

  // Enable all AXISRAM memory banks
  RAMCFG_HandleTypeDef hramcfg = { 0 };
  hramcfg.Instance =  RAMCFG_SRAM2_AXI;
  HAL_RAMCFG_EnableAXISRAM(&hramcfg);
  if (HAL_RAMCFG_Init(&hramcfg) != HAL_OK)
    Error_Handler();
  hramcfg.Instance =  RAMCFG_SRAM3_AXI;
  HAL_RAMCFG_EnableAXISRAM(&hramcfg);
  if (HAL_RAMCFG_Init(&hramcfg) != HAL_OK)
    Error_Handler();
  hramcfg.Instance =  RAMCFG_SRAM4_AXI;
  HAL_RAMCFG_EnableAXISRAM(&hramcfg);
  if (HAL_RAMCFG_Init(&hramcfg) != HAL_OK)
    Error_Handler();
  hramcfg.Instance =  RAMCFG_SRAM5_AXI;
  HAL_RAMCFG_EnableAXISRAM(&hramcfg);
  if (HAL_RAMCFG_Init(&hramcfg) != HAL_OK)
    Error_Handler();
  hramcfg.Instance =  RAMCFG_SRAM6_AXI;
  HAL_RAMCFG_EnableAXISRAM(&hramcfg);
  if (HAL_RAMCFG_Init(&hramcfg) != HAL_OK)
    Error_Handler();

  // Start the clocks for all AXISRAM memory banks
  __HAL_RCC_CACHEAXIRAM_MEM_CLK_ENABLE();
  __HAL_RCC_AXISRAM2_MEM_CLK_ENABLE();
  __HAL_RCC_AXISRAM3_MEM_CLK_ENABLE();
  __HAL_RCC_AXISRAM4_MEM_CLK_ENABLE();
  __HAL_RCC_AXISRAM5_MEM_CLK_ENABLE();
  __HAL_RCC_AXISRAM6_MEM_CLK_ENABLE();

  // Allow instruction and data caches to be activated
  MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_DCACTIVE_Msk | MEMSYSCTL_MSCR_ICACTIVE_Msk;
}

static void CACHEAXI_Init(void)
{
  // Enable the NPU clock and reset the NPU peripheral
  __HAL_RCC_NPU_CLK_ENABLE();
  __HAL_RCC_NPU_FORCE_RESET();
  __HAL_RCC_NPU_RELEASE_RESET();

  // Enable the NPU cache clock and reset the NPU cache
  __HAL_RCC_CACHEAXI_CLK_ENABLE();
  __HAL_RCC_CACHEAXI_FORCE_RESET();
  __HAL_RCC_CACHEAXI_RELEASE_RESET();

  // Initialize the NPU cache
  npu_cache_init();
}

static void SystemIsolation_Config(void)
{
  // Configure the IAC to trap illegal access events
  __HAL_RCC_IAC_CLK_ENABLE();
  __HAL_RCC_IAC_FORCE_RESET();
  __HAL_RCC_IAC_RELEASE_RESET();

  // Enable secure access for the NPU and the SDMMC peripherals
  __HAL_RCC_RIFSC_CLK_ENABLE();
  RIMC_MasterConfig_t master_conf = {
    .MasterCID = RIF_CID_1,
    .SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV
  };
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_NPU, &master_conf);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_NPU, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_SDMMC1, &master_conf);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_SDMMC1 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_SPI1, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_I2C3, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);

  // Enable non-privileged access for the PWR peripheral
  HAL_PWR_ConfigAttributes(PWR_ITEM_0, PWR_SEC_NPRIV);

  // Configure non-privileged access for all GPIO pins
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_7,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_8,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_15,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_4,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_8,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_11,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_12,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_13,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOH,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_12,GPIO_PIN_SEC|GPIO_PIN_NPRIV);

  // Keep all IPs enabled during WFE so they can wake up the CPU (TODO: DO WE NEED THIS)
  LL_BUS_EnableClockLowPower(~0);
  LL_MEM_EnableClockLowPower(~0);
  LL_AHB1_GRP1_EnableClockLowPower(~0);
  LL_AHB2_GRP1_EnableClockLowPower(~0);
  LL_AHB3_GRP1_EnableClockLowPower(~0);
  LL_AHB4_GRP1_EnableClockLowPower(~0);
  LL_AHB5_GRP1_EnableClockLowPower(~0);
  LL_APB1_GRP1_EnableClockLowPower(~0);
  LL_APB1_GRP2_EnableClockLowPower(~0);
  LL_APB2_GRP1_EnableClockLowPower(~0);
  LL_APB4_GRP1_EnableClockLowPower(~0);
  LL_APB4_GRP2_EnableClockLowPower(~0);
  LL_APB5_GRP1_EnableClockLowPower(~0);
  LL_MISC_EnableClockLowPower(~0);
}

int main(void)
{
  // Initialize memory protection, hardware abstraction, the system clock tree, and caching
  MPU_Config();
  HAL_Init();
  SystemClock_Config();
  SCB_EnableICache();
  SCB_EnableDCache();

  // TODO: Remove or fix this
  __HAL_RCC_RIFSC_CLK_ENABLE();
  RISAF3->REG[0].CIDCFGR = 0x00FF00FF;
  RISAF3->REG[0].ENDR = 0xFFFFFFFF;
  RISAF3->REG[0].CFGR = 0x00000101;
  RISAF3->REG[1].CIDCFGR = 0x00FF00FF;
  RISAF3->REG[1].ENDR = 0xFFFFFFFF;
  RISAF3->REG[1].CFGR = 0x00000001;

  // Initialize all required system peripherals
  chip_initialize_unused_pins();
  GPIO_Init();
  RAMCFG_Init();
  flash_init();
  CACHEAXI_Init();
  SystemIsolation_Config();

  // Initialize the user peripherals
  storage_init();
  comms_init();

  // Initialize the SD card file system and AI framework
  FileX_Init();
  ai_init();

  // Illuminate the MCU status LED
  HAL_GPIO_WritePin(LED_MCU_STATUS_GPIO_Port, LED_MCU_STATUS_Pin, GPIO_PIN_SET);

  // Loop forever
  uint32_t count = 0;
  volatile uint8_t *audio_data = 0;
  ai_data.class_probabilities[0] = 3;
  ai_data.class_probabilities[1] = 7;
  while (1)
  {
    audio_data = comms_incoming_data();
    if (audio_data)
    {
      if (++count == 18)
      {
        count = 0;
        HAL_GPIO_TogglePin(LED_MCU_STATUS_GPIO_Port, LED_MCU_STATUS_Pin);
        ai_data.class_probabilities[0]++;
        ai_data.class_probabilities[1]++;
        comms_transmit((uint8_t*)&ai_data, sizeof(ai_data));
      }
      audio_data = 0;
      //ai_process();
    }
  }
}
