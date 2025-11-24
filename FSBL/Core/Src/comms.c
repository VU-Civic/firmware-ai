// Header Inclusions ---------------------------------------------------------------------------------------------------

#include "comms.h"


// Host Communication Definitions --------------------------------------------------------------------------------------

#define I2C_DEVICE_ADDRESS                144


// GPS Static Variables ------------------------------------------------------------------------------------------------

__attribute__ ((section (".noncacheable")))
static DMA_NodeTypeDef from_host_spi_dma_nodes[2];

__attribute__ ((section (".noncacheable"), aligned (32)))
static uint8_t spi_buffer[2*sizeof(audio_packet_t)];

static volatile uint8_t *incoming_data = 0;


// Private Helper Functions --------------------------------------------------------------------------------------------

static void spi_dma_setup(void)
{
   // Explicitly configure all SPI DMA registers
   MODIFY_REG(GPDMA1_Channel0->CCR, (DMA_CCR_PRIO | DMA_CCR_LAP | DMA_CCR_LSM), (DMA_HIGH_PRIORITY | DMA_LSM_FULL_EXECUTION | DMA_LINK_ALLOCATED_PORT1));
   WRITE_REG(GPDMA1_Channel0->CLBAR, ((uint32_t)&from_host_spi_dma_nodes[0] & DMA_CLBAR_LBA));
   WRITE_REG(GPDMA1_Channel0->CTR1, from_host_spi_dma_nodes[0].LinkRegisters[NODE_CTR1_DEFAULT_OFFSET]);
   WRITE_REG(GPDMA1_Channel0->CTR2, from_host_spi_dma_nodes[0].LinkRegisters[NODE_CTR2_DEFAULT_OFFSET]);
   WRITE_REG(GPDMA1_Channel0->CSAR, from_host_spi_dma_nodes[0].LinkRegisters[NODE_CSAR_DEFAULT_OFFSET]);
   WRITE_REG(GPDMA1_Channel0->CDAR, from_host_spi_dma_nodes[0].LinkRegisters[NODE_CDAR_DEFAULT_OFFSET]);
   WRITE_REG(GPDMA1_Channel0->CBR1, from_host_spi_dma_nodes[0].LinkRegisters[NODE_CBR1_DEFAULT_OFFSET]);
   WRITE_REG(GPDMA1_Channel0->CLLR, from_host_spi_dma_nodes[0].LinkRegisters[NODE_CLLR_LINEAR_DEFAULT_OFFSET]);
   WRITE_REG(GPDMA1_Channel0->CFCR, (DMA_FLAG_TC | DMA_FLAG_HT | DMA_FLAG_DTE | DMA_FLAG_SUSP));
   SET_BIT(GPDMA1_Channel0->CCR, (DMA_IT_TC | DMA_IT_DTE | DMA_IT_SUSP | DMA_CCR_EN));
}

static void i2c_dma_setup(void)
{
   // Explicitly configure all I2C DMA registers
   MODIFY_REG(GPDMA1_Channel1->CCR, (DMA_CCR_PRIO | DMA_CCR_LAP | DMA_CCR_LSM), (DMA_LOW_PRIORITY_MID_WEIGHT | DMA_LSM_FULL_EXECUTION));
   WRITE_REG(GPDMA1_Channel1->CTR1, (DMA_DINC_FIXED | DMA_DEST_DATAWIDTH_BYTE | DMA_SINC_INCREMENTED | DMA_SRC_DATAWIDTH_BYTE | DMA_CTR1_SSEC | DMA_CTR1_DSEC | DMA_SRC_ALLOCATED_PORT1 | DMA_DEST_ALLOCATED_PORT0));
   WRITE_REG(GPDMA1_Channel1->CTR2, (DMA_TCEM_BLOCK_TRANSFER | GPDMA1_REQUEST_I2C3_TX | DMA_CTR2_DREQ | DMA_NORMAL));
   WRITE_REG(GPDMA1_Channel1->CDAR, (uint32_t)&I2C3->TXDR);
}


// Interrupt Service Routines ------------------------------------------------------------------------------------------

void GPDMA1_Channel0_IRQHandler(void)
{
   // Check if a full data packet transfer has completed
   static const uint8_t packet_delimiter[] = AUDIO_PACKET_START_DELIMITER;
   if (READ_BIT(GPDMA1_Channel0->CSR, DMA_FLAG_TC))
   {
      // Clear the flag and update the pointer to the current incoming data
      WRITE_REG(GPDMA1_Channel0->CFCR, DMA_FLAG_TC);
      incoming_data = (GPDMA1_Channel0->CLLR == ((uint32_t)&from_host_spi_dma_nodes[0] & DMA_CLLR_LA)) ? &spi_buffer[sizeof(spi_buffer) / 2] : &spi_buffer[0];

      // Validate the packet delimiter and trigger automatic re-initialization if there is an error
      if ((incoming_data[0] != packet_delimiter[0]) || (incoming_data[1] != packet_delimiter[1]) || (incoming_data[2] != packet_delimiter[2]) || (incoming_data[3] != packet_delimiter[3]))
      {
         SET_BIT(GPDMA1_Channel0->CCR, DMA_CCR_SUSP);
         incoming_data = 0;
      }
   }

   // Check if a DMA error has occurred or the transfer has been suspended
   if (READ_BIT(GPDMA1_Channel0->CSR, (DMA_FLAG_DTE | DMA_FLAG_SUSP)))
   {
      // Clear the flag and reset the DMA channel and peripheral
      WRITE_REG(GPDMA1_Channel0->CFCR, (DMA_FLAG_DTE | DMA_FLAG_SUSP));
      SET_BIT(GPDMA1_Channel0->CCR, DMA_CCR_RESET);
      CLEAR_BIT(SPI1->CR1, SPI_CR1_SPE);

      // Re-enable SPI CS interrupts to allow read synchronization
      NVIC_EnableIRQ(EXTI15_IRQn);
   }
}

void GPDMA1_Channel1_IRQHandler(void)
{
   // Check if a DMA error has occurred or the transfer has been suspended
   if (READ_BIT(GPDMA1_Channel1->CSR, (DMA_FLAG_DTE | DMA_FLAG_SUSP)))
   {
      // Clear the flag and reset the DMA channel and peripheral
      WRITE_REG(GPDMA1_Channel1->CFCR, (DMA_FLAG_DTE | DMA_FLAG_SUSP));
      SET_BIT(GPDMA1_Channel1->CCR, DMA_CCR_RESET);
      i2c_dma_setup();
   }
}

void I2C3_EV_IRQHandler(void)
{
   // Handle a NACK reception
   if (READ_BIT(I2C3->ISR, I2C_FLAG_AF))
   {
      // Clear the NACK flag and flush the TX register
      WRITE_REG(I2C3->ICR, (I2C_FLAG_AF | I2C_FLAG_STOPF));
      if (READ_BIT(I2C3->ISR, I2C_FLAG_TXIS))
         WRITE_REG(I2C3->TXDR, 0x00U);
      if (READ_BIT(I2C3->ISR, I2C_FLAG_TXE))
         SET_BIT(I2C3->ISR, I2C_FLAG_TXE);
   }
}

void I2C3_ER_IRQHandler(void)
{
   // Clear all error flags
   WRITE_REG(I2C3->ICR, (I2C_FLAG_BERR | I2C_FLAG_OVR | I2C_FLAG_ARLO | I2C_FLAG_STOPF));

   // Flush the TX register
   if (READ_BIT(I2C3->ISR, I2C_FLAG_TXIS))
      WRITE_REG(I2C3->TXDR, 0x00U);
   if (READ_BIT(I2C3->ISR, I2C_FLAG_TXE))
      SET_BIT(I2C3->ISR, I2C_FLAG_TXE);

   // Abort an ongoing DMA transfer
   SET_BIT(GPDMA1_Channel1->CCR, DMA_CCR_SUSP);
}

void comms_spi_cs_isr(void)
{
   // SPI CS has de-asserted, signaling the end of a packet: set up new packet reception using DMA
   spi_dma_setup();
   SET_BIT(SPI1->CR1, SPI_CR1_SPE);
   NVIC_DisableIRQ(EXTI15_IRQn);
}


// Private Helper Functions --------------------------------------------------------------------------------------------

static void from_host_spi_init(void)
{
   // Enable the SPI, GPIO, and DMA clocks
   LL_RCC_SetSPIClockSource(RCC_SPI1CLKSOURCE_PCLK2);
   SET_BIT(RCC->APB2ENSR, RCC_APB2ENR_SPI1EN);
   (void)READ_BIT(RCC->APB2ENR, RCC_APB2ENR_SPI1EN);
   SET_BIT(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOAEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOAEN);
   SET_BIT(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOBEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOBEN);
   SET_BIT(RCC->AHB1ENSR, RCC_AHB1ENR_GPDMA1EN);
   (void)READ_BIT(RCC->AHB1ENR, RCC_AHB1ENR_GPDMA1EN);

   // Initialize the SPI GPIO pins
   uint32_t position = 32 - __builtin_clz(DATA_IN_CS_Pin) - 1;
   MODIFY_REG(DATA_IN_CS_GPIO_Port->OSPEEDR, (GPIO_OSPEEDR_OSPEED0 << (position * 2U)), (GPIO_SPEED_FREQ_VERY_HIGH << (position * 2U)));
   MODIFY_REG(DATA_IN_CS_GPIO_Port->OTYPER, (GPIO_OTYPER_OT0 << position), (((GPIO_MODE_AF_PP & OUTPUT_TYPE) >> OUTPUT_TYPE_Pos) << position));
   MODIFY_REG(DATA_IN_CS_GPIO_Port->PUPDR, (GPIO_PUPDR_PUPD0 << (position * 2U)), (GPIO_NOPULL << (position * 2U)));
   MODIFY_REG(DATA_IN_CS_GPIO_Port->AFR[position >> 3U], (0xFU << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)), (GPIO_AF5_SPI1 << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)));
   MODIFY_REG(DATA_IN_CS_GPIO_Port->MODER, (GPIO_MODER_MODE0 << (position * 2U)), ((GPIO_MODE_AF_PP & GPIO_MODE) << (position * 2U)));
   position = 32 - __builtin_clz(DATA_IN_SCK_Pin) - 1;
   MODIFY_REG(DATA_IN_SCK_GPIO_Port->OSPEEDR, (GPIO_OSPEEDR_OSPEED0 << (position * 2U)), (GPIO_SPEED_FREQ_VERY_HIGH << (position * 2U)));
   MODIFY_REG(DATA_IN_SCK_GPIO_Port->OTYPER, (GPIO_OTYPER_OT0 << position), (((GPIO_MODE_AF_PP & OUTPUT_TYPE) >> OUTPUT_TYPE_Pos) << position));
   MODIFY_REG(DATA_IN_SCK_GPIO_Port->PUPDR, (GPIO_PUPDR_PUPD0 << (position * 2U)), (GPIO_NOPULL << (position * 2U)));
   MODIFY_REG(DATA_IN_SCK_GPIO_Port->AFR[position >> 3U], (0xFU << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)), (GPIO_AF5_SPI1 << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)));
   MODIFY_REG(DATA_IN_SCK_GPIO_Port->MODER, (GPIO_MODER_MODE0 << (position * 2U)), ((GPIO_MODE_AF_PP & GPIO_MODE) << (position * 2U)));
   position = 32 - __builtin_clz(DATA_IN_MOSI_Pin) - 1;
   MODIFY_REG(DATA_IN_MOSI_GPIO_Port->OSPEEDR, (GPIO_OSPEEDR_OSPEED0 << (position * 2U)), (GPIO_SPEED_FREQ_VERY_HIGH << (position * 2U)));
   MODIFY_REG(DATA_IN_MOSI_GPIO_Port->OTYPER, (GPIO_OTYPER_OT0 << position), (((GPIO_MODE_AF_PP & OUTPUT_TYPE) >> OUTPUT_TYPE_Pos) << position));
   MODIFY_REG(DATA_IN_MOSI_GPIO_Port->PUPDR, (GPIO_PUPDR_PUPD0 << (position * 2U)), (GPIO_NOPULL << (position * 2U)));
   MODIFY_REG(DATA_IN_MOSI_GPIO_Port->AFR[position >> 3U], (0xFU << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)), (GPIO_AF5_SPI1 << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)));
   MODIFY_REG(DATA_IN_MOSI_GPIO_Port->MODER, (GPIO_MODER_MODE0 << (position * 2U)), ((GPIO_MODE_AF_PP & GPIO_MODE) << (position * 2U)));
   position = 32 - __builtin_clz(DATA_IN_MISO_Pin) - 1;
   MODIFY_REG(DATA_IN_MISO_GPIO_Port->OSPEEDR, (GPIO_OSPEEDR_OSPEED0 << (position * 2U)), (GPIO_SPEED_FREQ_VERY_HIGH << (position * 2U)));
   MODIFY_REG(DATA_IN_MISO_GPIO_Port->OTYPER, (GPIO_OTYPER_OT0 << position), (((GPIO_MODE_AF_PP & OUTPUT_TYPE) >> OUTPUT_TYPE_Pos) << position));
   MODIFY_REG(DATA_IN_MISO_GPIO_Port->PUPDR, (GPIO_PUPDR_PUPD0 << (position * 2U)), (GPIO_NOPULL << (position * 2U)));
   MODIFY_REG(DATA_IN_MISO_GPIO_Port->AFR[position >> 3U], (0xFU << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)), (GPIO_AF5_SPI1 << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)));
   MODIFY_REG(DATA_IN_MISO_GPIO_Port->MODER, (GPIO_MODER_MODE0 << (position * 2U)), ((GPIO_MODE_AF_PP & GPIO_MODE) << (position * 2U)));

   // Configure the SPI CS pin to interrupt upon de-assertion
   position = 32 - __builtin_clz(DATA_IN_CS_Pin) - 1;
   uint32_t iocurrent = DATA_IN_CS_Pin & (1UL << position);
   MODIFY_REG(EXTI->EXTICR[position >> 2U], (0x0FUL << ((position & 0x03U) * EXTI_EXTICR1_EXTI1_Pos)), (0UL << (4U * (position & 0x03U))));
   SET_BIT(EXTI->IMR1, iocurrent);
   CLEAR_BIT(EXTI->EMR1, iocurrent);
   SET_BIT(EXTI->RTSR1, iocurrent);
   CLEAR_BIT(EXTI->FTSR1, iocurrent);
   *(&EXTI->SECCFGR1 + (0x08U * ((EXTI_LINE_15 & EXTI_REG_MASK) >> EXTI_REG_SHIFT))) |= (1UL << (EXTI_LINE_15 & EXTI_PIN_MASK));

   // Enable secure access for the SPI DMA registers
   const uint32_t channel_idx = 1UL << 0;
   SET_BIT(GPDMA1->PRIVCFGR, channel_idx);
   SET_BIT(GPDMA1->SECCFGR, channel_idx);

   // Set up a DMA linked list to continually receive in a circular buffer
   for (uint32_t i = 0; i < 2; ++i)
   {
      from_host_spi_dma_nodes[i].LinkRegisters[NODE_CTR1_DEFAULT_OFFSET] = DMA_DINC_INCREMENTED | DMA_DEST_DATAWIDTH_WORD | DMA_SINC_FIXED | DMA_SRC_DATAWIDTH_WORD | DMA_CTR1_SSEC | DMA_CTR1_DSEC | DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT1;
      from_host_spi_dma_nodes[i].LinkRegisters[NODE_CTR2_DEFAULT_OFFSET] = DMA_TCEM_EACH_LL_ITEM_TRANSFER | GPDMA1_REQUEST_SPI1_RX | DMA_NORMAL;
      from_host_spi_dma_nodes[i].LinkRegisters[NODE_CBR1_DEFAULT_OFFSET] = sizeof(spi_buffer) / 2;
      from_host_spi_dma_nodes[i].LinkRegisters[NODE_CSAR_DEFAULT_OFFSET] = (uint32_t)&SPI1->RXDR;
      from_host_spi_dma_nodes[i].LinkRegisters[NODE_CDAR_DEFAULT_OFFSET] = (uint32_t)((i == 0) ? &spi_buffer[0] : &spi_buffer[sizeof(spi_buffer) / 2]);
      from_host_spi_dma_nodes[i].LinkRegisters[NODE_CLLR_LINEAR_DEFAULT_OFFSET] = ((uint32_t)&from_host_spi_dma_nodes[(i + 1) % 2] & DMA_CLLR_LA) | DMA_CLLR_UT1 | DMA_CLLR_UT2 | DMA_CLLR_UB1 | DMA_CLLR_UDA | DMA_CLLR_USA | DMA_CLLR_ULL;
      from_host_spi_dma_nodes[i].NodeInfo = DMA_GPDMA_LINEAR_NODE | (NODE_CLLR_LINEAR_DEFAULT_OFFSET << NODE_CLLR_IDX_POS);
   }

   // Reset the SPI DMA peripheral
   SET_BIT(GPDMA1_Channel0->CCR, DMA_CCR_RESET);
   while (READ_BIT(GPDMA1_Channel0->CCR, DMA_CCR_EN));

   // Initialize the SPI host-to-AI communications peripheral
   CLEAR_BIT(SPI1->CR1, SPI_CR1_SPE);
   uint32_t crc_length = SPI1->CFG1 & SPI_CFG1_CRCSIZE;
   CLEAR_BIT(SPI1->CR1, SPI_CR1_MASRX);
   WRITE_REG(SPI1->CFG1, (SPI_BAUDRATEPRESCALER_4 | crc_length | SPI_FIFO_THRESHOLD_04DATA | SPI_DATASIZE_32BIT | SPI_CFG1_RXDMAEN));
   WRITE_REG(SPI1->CFG2, (SPI_NSS_PULSE_ENABLE | SPI_NSS_POLARITY_LOW | SPI_NSS_HARD_INPUT | SPI_MODE_SLAVE | SPI_DIRECTION_2LINES_RXONLY | SPI_CFG2_COMM_1));
   CLEAR_BIT(SPI1->I2SCFGR, SPI_I2SCFGR_I2SMOD);

   // Enable all necessary DMA and SPI CS de-assertion interrupts
   NVIC_SetPriority(GPDMA1_Channel0_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
   NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
   NVIC_SetPriority(EXTI15_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
   NVIC_EnableIRQ(EXTI15_IRQn);
}

static void to_host_i2c_init(void)
{
   // Enable the I2C, GPIO, and DMA clocks
   LL_RCC_SetI2CClockSource(RCC_I2C3CLKSOURCE_PCLK1);
   SET_BIT(RCC->APB1ENSR1, RCC_APB1ENR1_I2C3EN);
   (void)READ_BIT(RCC->APB1ENR1, RCC_APB1ENR1_I2C3EN);
   SET_BIT(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOAEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOAEN);
   SET_BIT(RCC->AHB1ENSR, RCC_AHB1ENR_GPDMA1EN);
   (void)READ_BIT(RCC->AHB1ENR, RCC_AHB1ENR_GPDMA1EN);

   // Initialize the I2C GPIO pins
   uint32_t position = 32 - __builtin_clz(DATA_OUT_SCL_Pin) - 1;
   MODIFY_REG(DATA_OUT_SCL_GPIO_Port->OSPEEDR, (GPIO_OSPEEDR_OSPEED0 << (position * 2U)), (GPIO_SPEED_FREQ_MEDIUM << (position * 2U)));
   MODIFY_REG(DATA_OUT_SCL_GPIO_Port->OTYPER, (GPIO_OTYPER_OT0 << position), (((GPIO_MODE_AF_OD & OUTPUT_TYPE) >> OUTPUT_TYPE_Pos) << position));
   MODIFY_REG(DATA_OUT_SCL_GPIO_Port->PUPDR, (GPIO_PUPDR_PUPD0 << (position * 2U)), (GPIO_NOPULL << (position * 2U)));
   MODIFY_REG(DATA_OUT_SCL_GPIO_Port->AFR[position >> 3U], (0xFU << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)), (GPIO_AF4_I2C3 << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)));
   MODIFY_REG(DATA_OUT_SCL_GPIO_Port->MODER, (GPIO_MODER_MODE0 << (position * 2U)), ((GPIO_MODE_AF_OD & GPIO_MODE) << (position * 2U)));
   position = 32 - __builtin_clz(DATA_OUT_SDA_Pin) - 1;
   MODIFY_REG(DATA_OUT_SDA_GPIO_Port->OSPEEDR, (GPIO_OSPEEDR_OSPEED0 << (position * 2U)), (GPIO_SPEED_FREQ_MEDIUM << (position * 2U)));
   MODIFY_REG(DATA_OUT_SDA_GPIO_Port->OTYPER, (GPIO_OTYPER_OT0 << position), (((GPIO_MODE_AF_OD & OUTPUT_TYPE) >> OUTPUT_TYPE_Pos) << position));
   MODIFY_REG(DATA_OUT_SDA_GPIO_Port->PUPDR, (GPIO_PUPDR_PUPD0 << (position * 2U)), (GPIO_NOPULL << (position * 2U)));
   MODIFY_REG(DATA_OUT_SDA_GPIO_Port->AFR[position >> 3U], (0xFU << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)), (GPIO_AF4_I2C3 << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)));
   MODIFY_REG(DATA_OUT_SDA_GPIO_Port->MODER, (GPIO_MODER_MODE0 << (position * 2U)), ((GPIO_MODE_AF_OD & GPIO_MODE) << (position * 2U)));

   // Enable secure access for the I2C DMA registers
   const uint32_t channel_idx = 1UL << 1;
   SET_BIT(GPDMA1->PRIVCFGR, channel_idx);
   SET_BIT(GPDMA1->SECCFGR, channel_idx);

   // Reset and configure the I2C DMA peripheral
   SET_BIT(GPDMA1_Channel1->CCR, DMA_CCR_RESET);
   while (READ_BIT(GPDMA1_Channel1->CCR, DMA_CCR_EN));
   i2c_dma_setup();

   // Initialize the I2C AI-to-host communications peripheral for Fast Mode Plus (1Mbit/s)
   CLEAR_BIT(I2C3->CR1, I2C_CR1_PE);
   WRITE_REG(I2C3->TIMINGR, 0x1080133B);
   CLEAR_BIT(I2C3->OAR1, I2C_OAR1_OA1EN);
   CLEAR_BIT(I2C3->OAR2, I2C_DUALADDRESS_ENABLE);
   MODIFY_REG(I2C3->CR2, I2C_CR2_ADD10, (I2C_CR2_AUTOEND | I2C_CR2_NACK));
   WRITE_REG(I2C3->CR1, (I2C_CR1_TXDMAEN | I2C_CR1_FMP | I2C_IT_ERRI | I2C_IT_NACKI));
   SET_BIT(I2C3->CR1, I2C_CR1_PE);

   // Enable all necessary DMA and I2C interrupts
   NVIC_SetPriority(GPDMA1_Channel1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
   NVIC_EnableIRQ(GPDMA1_Channel1_IRQn);
   NVIC_SetPriority(I2C3_EV_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
   NVIC_EnableIRQ(I2C3_EV_IRQn);
   NVIC_SetPriority(I2C3_ER_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
   NVIC_EnableIRQ(I2C3_ER_IRQn);
}


// Public API Functions ------------------------------------------------------------------------------------------------

void comms_init(void)
{
   // Initialize MCU host communications in both directions
   to_host_i2c_init();
   from_host_spi_init();
}

void comms_transmit(uint8_t *data, uint8_t data_len)
{
   // Only proceed if the I2C peripheral is not currently busy
   if (!READ_BIT(I2C3->ISR, I2C_FLAG_BUSY))
   {
      // Set up the DMA transfer data source and length
      WRITE_REG(GPDMA1_Channel1->CBR1, data_len);
      WRITE_REG(GPDMA1_Channel1->CSAR, (uint32_t)data);
      WRITE_REG(GPDMA1_Channel1->CFCR, (DMA_FLAG_TC | DMA_FLAG_HT | DMA_FLAG_DTE | DMA_FLAG_SUSP | DMA_FLAG_ULE | DMA_FLAG_USE | DMA_FLAG_TO));
      SET_BIT(GPDMA1_Channel1->CCR, (DMA_IT_DTE | DMA_IT_SUSP | DMA_CCR_EN));

      // Initiate an I2C transmission to the host
      const uint32_t reg_val = ((uint32_t)I2C_DEVICE_ADDRESS | ((uint32_t)data_len << I2C_CR2_NBYTES_Pos) | I2C_CR2_AUTOEND | I2C_CR2_START);
      MODIFY_REG(I2C3->CR2, (I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RELOAD | I2C_CR2_STOP), reg_val);
   }
}

volatile uint8_t* comms_incoming_data(void)
{
   // Return incoming data and reset the pointer for the next packet
  volatile uint8_t *data = incoming_data;
  incoming_data = 0;
  return data;
}
