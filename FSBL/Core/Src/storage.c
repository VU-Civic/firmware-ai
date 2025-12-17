#define TFLAC_IMPLEMENTATION
#define TFLAC_DISABLE_COUNTERS

// Header Inclusions ---------------------------------------------------------------------------------------------------

#include <arm_math.h>
#include <setjmp.h>
#include "comms.h"
#include "ff.h"
#include "diskio.h"
#include "storage.h"
#include "system.h"
#include "tflac.h"


// Storage Data Type Definitions ---------------------------------------------------------------------------------------

#define AUDIO_NUM_ENCODED_CHANNELS            1
#define AUDIO_BITS_PER_SAMPLE                 16

#define FLAC_ENCODER_PARTITION_ORDER          3
#define FLAC_ENCODER_BLOCK_SIZE               4096

#define AUDIO_CLIP_MIN_NUM_SAMPLES            (STORAGE_AUDIO_CLIP_MIN_NUM_SECONDS * AUDIO_PACKET_SAMPLE_RATE)
#define SD_CARD_BUFFER_NUM_BYTES              16384

#define NUM_TASK_CONTEXTS                     2
#define STORAGE_TASK_STACK_SIZE               (2 * 1024)
#define MAX_NUM_SD_CARD_COMMANDS              8

#define SDMMC_CLK                             200000000U
#define SD_MAX_BUS_SPEED_MODE                 SDMMC_SDR50_SWITCH_PATTERN

typedef struct
{
   uint64_t card_size;
   uint32_t card_type, card_version, card_class, card_speed;
   uint32_t address, num_clusters, cluster_size, num_sectors, sector_size;
} sd_card_details_t;

typedef enum {
   SD_CARD_IDLE = 0,
   SD_CARD_OPEN_FILE,
   SD_CARD_WRITE_FILE
} sd_card_operation_t;

typedef struct {
   sd_card_operation_t operation;
   uint32_t operation_data;
} sd_card_command_t;


// Static Storage Variables --------------------------------------------------------------------------------------------

static volatile uint8_t audio_file_open;
static volatile DSTATUS sd_card_status = STA_NOINIT;
static volatile uint32_t sd_xfer_context, sd_result_ready, sd_timed_out;
static volatile uint8_t sd_rx_cplt, sd_tx_cplt, sd_card_initialized, sd_card_state_changed;
static uint32_t pcm_write_index, output_buffer_len, sd_write_index;
static uint32_t samples_written, bytes_written, timeout_num_cycles;
static sd_card_details_t sd_card_details;
static int8_t output_buffer[8219];
static tflac flac_encoder;

#if USE_SETJMP_FOR_SD_STORAGE > 0

static volatile uint32_t sd_card_command_read_index, sd_card_command_write_index;
static sd_card_command_t sd_card_command_queue[MAX_NUM_SD_CARD_COMMANDS];
static uint8_t sd_card_context_stack[STORAGE_TASK_STACK_SIZE];
static jmp_buf task_contexts[NUM_TASK_CONTEXTS];

#endif

__attribute__ ((section(".noncacheable")))
static FATFS file_system;

__attribute__ ((section(".noncacheable")))
static FIL audio_file;

__attribute__ ((section(".noncacheable")))
static uint8_t flac_encoder_mem[81936];

__attribute__ ((section(".noncacheable")))
static int8_t sd_write_buffer[2*SD_CARD_BUFFER_NUM_BYTES];

__attribute__ ((section(".noncacheable")))
static int16_t pcm[AUDIO_NUM_ENCODED_CHANNELS][FLAC_ENCODER_BLOCK_SIZE];

__attribute__ ((section(".noncacheable")))
static int16_t* pcm_channels[AUDIO_NUM_ENCODED_CHANNELS];


// Private Helper Functions --------------------------------------------------------------------------------------------

static void disable_sd_card(void)
{
   // Enable the SDMMC and GPIO clocks
   WRITE_REG(RCC->AHB5ENSR, RCC_AHB5ENR_SDMMC1EN);
   (void)READ_BIT(RCC->AHB5ENR, RCC_AHB5ENR_SDMMC1EN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOEEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOEEN);

   // Power off the SD card and peripheral
   MODIFY_REG(PWR->SVMCR1, PWR_SVMCR1_VDDIO4VRSEL, PWR_VDDIO_RANGE_3V3 << PWR_SVMCR1_VDDIO4VRSEL_Pos);
   CLEAR_BIT(SDMMC1->POWER, SDMMC_POWER_PWRCTRL);
   WRITE_REG(SD_CARD_EN_GPIO_Port->BRR, SD_CARD_EN_Pin);
   WRITE_REG(SD_PWR_SELECT_GPIO_Port->BRR, SD_PWR_SELECT_Pin);

   // Disable all GPIO and SDMMC clocks
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIOCEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIOEEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIOHEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIONEN);
   WRITE_REG(RCC->AHB5ENCR, RCC_AHB5ENR_SDMMC1EN);

   // Clear the SD card initialization flag
   sd_card_state_changed = 0;
   sd_card_initialized = 0;
   sd_result_ready = 0;
}

static uint8_t enable_sd_card(void)
{
   // Enable the SDMMC and GPIO clocks
   sd_card_initialized = 1;
   LL_RCC_SetSDMMCClockSource(RCC_SDMMC1CLKSOURCE_HCLK);
   WRITE_REG(RCC->AHB5ENSR, RCC_AHB5ENR_SDMMC1EN);
   (void)READ_BIT(RCC->AHB5ENR, RCC_AHB5ENR_SDMMC1EN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOCEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOCEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOEEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOEEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOHEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOHEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIONEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIONEN);

   // Allow two voltage initialization attempts
   for (uint32_t voltage_attempt = 0; voltage_attempt < 2; ++voltage_attempt)
   {
      // Power on the SD card
      WRITE_REG(SD_CARD_EN_GPIO_Port->BSRR, SD_CARD_EN_Pin);

      // Initialize and power on the SDMMC peripheral
      MODIFY_REG(SDMMC1->CLKCR, CLKCR_CLEAR_MASK, (SDMMC_CLOCK_EDGE_RISING | SDMMC_CLOCK_POWER_SAVE_DISABLE | SDMMC_BUS_WIDE_1B | (SDMMC_CLK / (2U * 400000U))));
      SET_BIT(SDMMC1->POWER, SDMMC_POWER_PWRCTRL);

      // Wait >75ms for the SD card to power up
      system_delay(100);

      // Put the SD card into its IDLE state
      WRITE_REG(SDMMC1->ARG, 0U);
      MODIFY_REG(SDMMC1->CMD, CMD_CLEAR_MASK, (SDMMC_CMD_GO_IDLE_STATE | SDMMC_RESPONSE_NO | SDMMC_WAIT_NO | SDMMC_CPSM_ENABLE));
      while (!READ_BIT(SDMMC1->STA, (SDMMC_FLAG_CCRCFAIL | SDMMC_FLAG_CTIMEOUT | SDMMC_FLAG_CMDSENT)));
      WRITE_REG(SDMMC1->ICR, SDMMC_STATIC_CMD_FLAGS);
      if (READ_BIT(SDMMC1->STA, (SDMMC_FLAG_CTIMEOUT | SDMMC_FLAG_CCRCFAIL)))  // SD card fatal error
      {
         disable_sd_card();
         return 0;
      }

      // Check the version of the current SD card
      sd_card_details.card_version = CARD_V1_X;
      if (SDMMC_CmdOperCond(SDMMC1))
      {
         // Not a V2 card, put back into the IDLE state
         WRITE_REG(SDMMC1->ARG, 0U);
         MODIFY_REG(SDMMC1->CMD, CMD_CLEAR_MASK, (SDMMC_CMD_GO_IDLE_STATE | SDMMC_RESPONSE_NO | SDMMC_WAIT_NO | SDMMC_CPSM_ENABLE));
         while (!READ_BIT(SDMMC1->STA, (SDMMC_FLAG_CCRCFAIL | SDMMC_FLAG_CTIMEOUT | SDMMC_FLAG_CMDSENT)));
         WRITE_REG(SDMMC1->ICR, SDMMC_STATIC_CMD_FLAGS);
      }
      else
         sd_card_details.card_version = CARD_V2_X;

      // Check the voltage and type of the current SD card
      sd_card_details.card_type = CARD_SDSC;
      for (uint32_t count = 0, valid_voltage = 0; (count <= 1000) && !valid_voltage; ++count)
      {
         SDMMC_CmdAppCommand(SDMMC1, 0);
         SDMMC_CmdAppOperCommand(SDMMC1, SDMMC_VOLTAGE_WINDOW_SD | SDMMC_HIGH_CAPACITY | SD_SWITCH_1_8V_CAPACITY);
         const uint32_t response = SDMMC_GetResponse(SDMMC1, SDMMC_RESP1);
         valid_voltage = ((response >> 31U) == 1U);
         sd_card_details.card_type = (valid_voltage && ((response & SDMMC_HIGH_CAPACITY) == SDMMC_HIGH_CAPACITY)) ? CARD_SDHC_SDXC : CARD_SDSC;
         if (count >= 1000)
         {
            // SD card fatal error
            disable_sd_card();
            return 0;
         }
      }

      // Attempt to lower the signaling voltage for SDXC cards
      if (!voltage_attempt && (sd_card_details.card_type == CARD_SDHC_SDXC))
      {
         // Set the voltage switch-enable flag and send a switch command (CMD11) to the SD card
         SET_BIT(SDMMC1->POWER, SDMMC_POWER_VSWITCHEN);
         WRITE_REG(SDMMC1->ARG, 0U);
         MODIFY_REG(SDMMC1->CMD, CMD_CLEAR_MASK, (SDMMC_CMD_VOLTAGE_SWITCH | SDMMC_RESPONSE_SHORT | SDMMC_WAIT_NO | SDMMC_CPSM_ENABLE));

         // Wait for the command to complete and validate the response
         while (!READ_BIT(SDMMC1->STA, (SDMMC_FLAG_CCRCFAIL | SDMMC_FLAG_CMDREND | SDMMC_FLAG_CTIMEOUT | SDMMC_FLAG_BUSYD0END)) || READ_BIT(SDMMC1->STA, SDMMC_FLAG_CMDACT));
         if (!READ_BIT(SDMMC1->STA, (SDMMC_FLAG_CTIMEOUT | SDMMC_FLAG_CCRCFAIL)) && !READ_BIT(SDMMC1->RESP1, SDMMC_OCR_ERRORBITS))
         {
            // Wait until the clock-stopped flag is set and clear it
            while (!READ_BIT(SDMMC1->STA, SDMMC_FLAG_CKSTOP));
            WRITE_REG(SDMMC1->ICR, SDMMC_FLAG_CKSTOP);

            // Switch the signaling voltage regulator to 1.8V
            WRITE_REG(SD_PWR_SELECT_GPIO_Port->BSRR, SD_PWR_SELECT_Pin);

            // Tell the SD card peripheral to begin the voltage switch and wait for it to complete
            SET_BIT(SDMMC1->POWER, SDMMC_POWER_VSWITCH);
            while (!READ_BIT(SDMMC1->STA, SDMMC_FLAG_VSWEND));
            SET_BIT(SDMMC1->ICR, SDMMC_FLAG_VSWEND);

            // Validate that the switch occurred successfully
            if (READ_BIT(SDMMC1->STA, SDMMC_FLAG_BUSYD0))
            {
               // SD card voltage error
               disable_sd_card();
               continue;
            }

            // Disable the voltage switch flags and clear all status flags
            MODIFY_REG(PWR->SVMCR1, PWR_SVMCR1_VDDIO4VRSEL, PWR_VDDIO_RANGE_1V8 << PWR_SVMCR1_VDDIO4VRSEL_Pos);
            MODIFY_REG(SDMMC1->POWER, (SDMMC_POWER_VSWITCHEN | SDMMC_POWER_VSWITCH), SDMMC_POWER_PWRCTRL);
            WRITE_REG(SDMMC1->ICR, 0xFFFFFFFFU);
            voltage_attempt = 2;
         }
      }
   }

   // Retrieve the SD card's relative address, class, and card-specific data
   uint32_t csd[4] = { 0 };
   sd_card_details.address = 0;
   if (!SDMMC_CmdSendCID(SDMMC1))
   {
      csd[0] = SDMMC_GetResponse(SDMMC1, SDMMC_RESP1);
      csd[1] = SDMMC_GetResponse(SDMMC1, SDMMC_RESP2);
      csd[2] = SDMMC_GetResponse(SDMMC1, SDMMC_RESP3);
      csd[3] = SDMMC_GetResponse(SDMMC1, SDMMC_RESP4);
   }
   uint16_t card_address = (uint16_t)sd_card_details.address;
   SDMMC_CmdSetRelAdd(SDMMC1, &card_address);
   sd_card_details.address = (uint32_t)card_address << 16;
   if (!SDMMC_CmdSendCSD(SDMMC1, sd_card_details.address))
   {
      csd[0] = SDMMC_GetResponse(SDMMC1, SDMMC_RESP1);
      csd[1] = SDMMC_GetResponse(SDMMC1, SDMMC_RESP2);
      csd[2] = SDMMC_GetResponse(SDMMC1, SDMMC_RESP3);
      csd[3] = SDMMC_GetResponse(SDMMC1, SDMMC_RESP4);
   }
   sd_card_details.card_class = (SDMMC_GetResponse(SDMMC1, SDMMC_RESP2) >> 20U);
   if (sd_card_details.card_type == CARD_SDSC)
   {
      sd_card_details.card_size = (((csd[1] & 0x000003FFU) << 2U) | ((csd[2] & 0xC0000000U) >> 30U));
      const uint8_t device_size_mul = (uint8_t)((csd[2] & 0x00038000U) >> 15U);
      const uint8_t read_block_len = (uint8_t)((csd[1] & 0x000F0000U) >> 16U);
      sd_card_details.sector_size = 512;
      sd_card_details.cluster_size = (1UL << (read_block_len & 0x0FU));
      sd_card_details.card_size = (1 + sd_card_details.card_size) * (1ULL << ((device_size_mul & 0x07U) + 2U));
      sd_card_details.num_clusters = (uint32_t)(sd_card_details.card_size / sd_card_details.cluster_size);
      sd_card_details.num_sectors = (uint32_t)(sd_card_details.card_size / sd_card_details.sector_size);
   }
   else
   {
      sd_card_details.card_size = (1 + (((csd[1] & 0x0000003FU) << 16U) | ((csd[2] & 0xFFFF0000U) >> 16U))) * 524288ULL;
      sd_card_details.sector_size = 512;
      sd_card_details.cluster_size = 512;
      sd_card_details.num_clusters = (uint32_t)(sd_card_details.card_size / sd_card_details.cluster_size);
      sd_card_details.num_sectors = (uint32_t)(sd_card_details.card_size / sd_card_details.sector_size);
   }

   // Select the SD card
   if (SDMMC_CmdSelDesel(SDMMC1, sd_card_details.address))
   {
      // SD card fatal error
      disable_sd_card();
      return 0;
   }

   // Force the SD card block size to 64 bytes
   if (!SDMMC_CmdBlockLength(SDMMC1, 64U) && !SDMMC_CmdAppCommand(SDMMC1, sd_card_details.address))
   {
      uint32_t sd_status[16], count = 0;
      const SDMMC_DataInitTypeDef config = {
         .DataTimeOut = SDMMC_DATATIMEOUT,
         .DataLength = 64U,
         .DataBlockSize = SDMMC_DATABLOCK_SIZE_64B,
         .TransferDir = SDMMC_TRANSFER_DIR_TO_SDMMC,
         .TransferMode = SDMMC_TRANSFER_MODE_BLOCK,
         .DPSM = SDMMC_DPSM_ENABLE
      };
      SDMMC_ConfigData(SDMMC1, &config);
      SDMMC_CmdStatusRegister(SDMMC1);
      for (uint32_t i = 0; !__SDMMC_GET_FLAG(SDMMC1, (SDMMC_FLAG_RXOVERR | SDMMC_FLAG_DCRCFAIL | SDMMC_FLAG_DTIMEOUT | SDMMC_FLAG_DATAEND)) && (i <= 100000000); ++i)
      {
         if ((!__SDMMC_GET_FLAG(SDMMC1, SDMMC_FLAG_RXFIFOE)) && (count < 16))
            sd_status[count++] = SDMMC_ReadFIFO(SDMMC1);
         else if (i >= 100000000)
            system_reset();
      }
      for (uint32_t i = 0; __SDMMC_GET_FLAG(SDMMC1, SDMMC_FLAG_DPSMACT) && (count < 16) && (i <= 10000000); ++i)
      {
         sd_status[count++] = SDMMC_ReadFIFO(SDMMC1);
         if (i >= 10000000)
            system_reset();
      }
      WRITE_REG(SDMMC1->ICR, SDMMC_STATIC_DATA_FLAGS);
      const uint8_t uhs_speed_grade = (uint8_t)((sd_status[3] & 0x00F0U) >> 4U);
      const uint8_t uhs_allocation_unit_size = (uint8_t)(sd_status[3] & 0x000FU);

      // Finalize detection of the SD card speed
      if ((sd_card_details.card_type == CARD_SDHC_SDXC) && (uhs_speed_grade || uhs_allocation_unit_size))
         sd_card_details.card_speed = CARD_ULTRA_HIGH_SPEED;
      else if (sd_card_details.card_type == CARD_SDHC_SDXC)
         sd_card_details.card_speed = CARD_HIGH_SPEED;
      else
         sd_card_details.card_speed = CARD_NORMAL_SPEED;

      // Reconfigure the card to use a 4-bit wide bus configuration
      SDMMC_CmdAppCommand(SDMMC1, sd_card_details.address);
      SDMMC_CmdBusWidth(SDMMC1, 2U);
      MODIFY_REG(SDMMC1->CLKCR, CLKCR_CLEAR_MASK, (SDMMC_CLOCK_EDGE_RISING | SDMMC_CLOCK_POWER_SAVE_DISABLE | SDMMC_BUS_WIDE_4B | (SDMMC_CLK / (2U * 25000000U))));

      // Determine the best SD bus clock configuration values given the current card properties and desired speed
      uint32_t sd_max_bus_speed_mode = SD_MAX_BUS_SPEED_MODE, sd_clock_div, sd_high_speed = 0, sd_ddr_mode = 0;
      switch (sd_max_bus_speed_mode)
      {
         case SDMMC_SDR25_SWITCH_PATTERN:
            sd_clock_div = SDMMC_CLK / (2U * 50000000U);
            break;
         case SDMMC_SDR50_SWITCH_PATTERN:
            sd_clock_div = SDMMC_CLK / (2U * 100000000U);
            sd_high_speed = SDMMC_CLKCR_BUSSPEED;
            break;
         case SDMMC_SDR104_SWITCH_PATTERN:
            sd_clock_div = SDMMC_CLK / (2U * 208000000U);
            sd_high_speed = SDMMC_CLKCR_BUSSPEED;
            break;
         case SDMMC_DDR50_SWITCH_PATTERN:
            sd_clock_div = SDMMC_CLK / (2U * 50000000U);
            sd_high_speed = SDMMC_CLKCR_BUSSPEED;
            sd_ddr_mode = SDMMC_CLKCR_DDR;
            break;
         case SDMMC_SDR12_SWITCH_PATTERN:  // Intentional fall-through
         default:
            sd_clock_div = SDMMC_CLK / (2U * 25000000U);
            break;
      }
      if ((sd_card_details.card_speed == CARD_HIGH_SPEED) && (sd_clock_div < (SDMMC_CLK / (2U * 50000000U))))
      {
         sd_clock_div = SDMMC_CLK / (2U * 50000000U);
         if ((sd_max_bus_speed_mode != SDMMC_SDR12_SWITCH_PATTERN) && (sd_max_bus_speed_mode != SDMMC_SDR25_SWITCH_PATTERN))
            sd_max_bus_speed_mode = SDMMC_SDR25_SWITCH_PATTERN;
      }
      else if ((sd_card_details.card_speed == CARD_NORMAL_SPEED) && (sd_clock_div < (SDMMC_CLK / (2U * 25000000U))))
      {
         sd_clock_div = SDMMC_CLK / (2U * 25000000U);
         if (sd_max_bus_speed_mode != SDMMC_SDR12_SWITCH_PATTERN)
            sd_max_bus_speed_mode = SDMMC_SDR12_SWITCH_PATTERN;
      }
      if (sd_card_details.card_speed != CARD_ULTRA_HIGH_SPEED)
         sd_high_speed = sd_ddr_mode = 0;

      // Reconfigure the card to with the specified bus speed configuration
      SDMMC_ConfigData(SDMMC1, &config);
      SDMMC_CmdSwitch(SDMMC1, sd_max_bus_speed_mode);
      for (uint32_t i = 0, count = 0; !__SDMMC_GET_FLAG(SDMMC1, (SDMMC_FLAG_RXOVERR | SDMMC_FLAG_DCRCFAIL | SDMMC_FLAG_DTIMEOUT | SDMMC_FLAG_DBCKEND | SDMMC_FLAG_DATAEND)) && (i <= 100000000); ++i)
      {
         if ((!__SDMMC_GET_FLAG(SDMMC1, SDMMC_FLAG_RXFIFOE)) && (count < 16))
            sd_status[count++] = SDMMC_ReadFIFO(SDMMC1);
         else if (i >= 100000000)
            system_reset();
      }
      WRITE_REG(SDMMC1->ICR, SDMMC_STATIC_DATA_FLAGS);
      if ((((uint8_t*)sd_status)[13] & 2U) == 2U)
         MODIFY_REG(SDMMC1->CLKCR, CLKCR_CLEAR_MASK, (SDMMC_CLOCK_EDGE_RISING | SDMMC_CLOCK_POWER_SAVE_ENABLE | SDMMC_BUS_WIDE_4B | sd_clock_div | sd_high_speed | sd_ddr_mode));

      // Set the SD card block size to 512 bytes
      if (SDMMC_CmdBlockLength(SDMMC1, 512))
         WRITE_REG(SDMMC1->ICR, SDMMC_STATIC_FLAGS);
   }
   else
   {
      // SD card fatal error
      disable_sd_card();
      return 0;
   }

   // Disable GPIO configuration clocks
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIOCEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIOEEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIOHEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIONEN);

   // Verify that SD card is ready to use
   for (uint32_t i = 0; SDMMC_CmdSendStatus(SDMMC1, sd_card_details.address) || (((SDMMC_GetResponse(SDMMC1, SDMMC_RESP1) >> 9U) & 0x0FU) != HAL_SD_CARD_TRANSFER); ++i)
   {
      if (i >= 1000)
      {
         // SD card fatal error
         disable_sd_card();
         return 0;
      }
   }
   sd_card_state_changed = 0;
   return 1;
}

static void sd_card_write_bytes(const int8_t *data, uint32_t data_len)
{
   // Iterate as long as bytes remain to be copied to the write buffer
   UINT data_written;
   uint32_t data_offset = 0;
   uint32_t buffer_bytes_remaining = SD_CARD_BUFFER_NUM_BYTES - (sd_write_index % SD_CARD_BUFFER_NUM_BYTES);
   uint32_t bytes_to_copy = (data_len <= buffer_bytes_remaining) ? data_len : buffer_bytes_remaining;
   while (bytes_to_copy)
   {
      // Copy the requested number of bytes into the write buffer
      arm_copy_q7(data + data_offset, &sd_write_buffer[sd_write_index], bytes_to_copy);
      data_offset += bytes_to_copy;
      data_len -= bytes_to_copy;

      // Check if the write buffer is full and should be flushed to the card
      sd_write_index = (sd_write_index + bytes_to_copy) % sizeof(sd_write_buffer);
      if (audio_file_open && ((sd_write_index == 0) || (sd_write_index == SD_CARD_BUFFER_NUM_BYTES)))
         f_write(&audio_file, &sd_write_buffer[sd_write_index ? 0 : SD_CARD_BUFFER_NUM_BYTES], SD_CARD_BUFFER_NUM_BYTES, &data_written);

      // Recalculate the number of bytes remaining to be copied
      buffer_bytes_remaining = SD_CARD_BUFFER_NUM_BYTES - (sd_write_index % SD_CARD_BUFFER_NUM_BYTES);
      bytes_to_copy = (data_len <= buffer_bytes_remaining) ? data_len : buffer_bytes_remaining;
   }
}

static void sd_card_close_audio_file(void)
{
   // Finalize and close the currently open audio file
   if (audio_file_open)
   {
      // Encode any remaining data as FLAC
      UINT data_written;
      if (pcm_write_index)
      {
         tflac_encode_s16p(&flac_encoder, pcm_write_index, pcm_channels, output_buffer, output_buffer_len, &bytes_written);
         sd_card_write_bytes(output_buffer, bytes_written);
      }
      if (sd_write_index && (sd_write_index != SD_CARD_BUFFER_NUM_BYTES))
         f_write(&audio_file, &sd_write_buffer[(sd_write_index < SD_CARD_BUFFER_NUM_BYTES) ? 0 : SD_CARD_BUFFER_NUM_BYTES], sd_write_index % SD_CARD_BUFFER_NUM_BYTES, &data_written);

      // Finalize the FLAC stream
      tflac_finalize(&flac_encoder);
      tflac_encode_streaminfo(&flac_encoder, 1, output_buffer, output_buffer_len, &bytes_written);
      f_lseek(&audio_file, 4);
      f_write(&audio_file, output_buffer, bytes_written, &data_written);

      // Close the audio file
      f_close(&audio_file);
      audio_file_open = 0;
   }
}

static void sd_card_open_file(uint32_t audio_timestamp)
{
   // Do not continue if the SD card is not initialized
   if (!sd_card_initialized)
      return;

   // Extend the length of an existing audio file if already open
   if (audio_file_open)
   {
      samples_written = 0;
      return;
   }

   // Determine if time to create a new storage directory
   const time_t timestamp = (time_t)audio_timestamp;
   struct tm *curr_time = gmtime(&timestamp);
   static uint32_t audio_directory_timestamp = 0;
   static char time_string[10] = { 0 }, audio_directory[14] = { 0 };
   strftime(time_string, sizeof(time_string), "%H%M%S", curr_time);
   if ((audio_timestamp - audio_directory_timestamp) >= 3600)
   {
      // Generate a new directory name from the current date and time
      memset(audio_directory, 0, sizeof(audio_directory));
      strftime(audio_directory, sizeof(audio_directory), "%Y", curr_time);
      f_mkdir(audio_directory);
      strftime(audio_directory, sizeof(audio_directory), "%Y/%m", curr_time);
      f_mkdir(audio_directory);
      strftime(audio_directory, sizeof(audio_directory), "%Y/%m/%d", curr_time);
      f_mkdir(audio_directory);
      strftime(audio_directory, sizeof(audio_directory), "%Y/%m/%d/%H", curr_time);
      f_mkdir(audio_directory);
      audio_directory_timestamp = (uint32_t)mktime(curr_time);
   }

   // Open the requested file
   static char file_name[32] = { 0 };
   snprintf(file_name, sizeof(file_name), "%s/%s.flac", audio_directory, time_string);
   audio_file_open = (f_open(&audio_file, file_name, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);

   // Initialize a new FLAC encoder
   if (audio_file_open)
   {
      // Reset the FLAC encoder
      sd_write_index = pcm_write_index = samples_written = 0;
      flac_encoder.samplecount = TFLAC_U64_ZERO;
      flac_encoder.frameno = flac_encoder.verbatim_subframe_bits = 0;
      flac_encoder.wasted_bits = flac_encoder.subframe_bitdepth = 0;

      // Write a FLAC header and STREAMINFO structure
      sd_card_write_bytes((int8_t*)"fLaC", 4);
      tflac_encode_streaminfo(&flac_encoder, 1, output_buffer, output_buffer_len, &bytes_written);
      sd_card_write_bytes(output_buffer, bytes_written);
   }
}

static void sd_card_write_audio_file(int16_t *audio_data)
{
   // Only proceed while there is an open file and outstanding audio samples
   uint32_t samples_remaining = AUDIO_PACKET_NUM_SAMPLES, read_index = 0;
   while (sd_card_initialized && audio_file_open && samples_remaining)
   {
      // Cast the audio data into the type expected by the FLAC encoder
      const uint32_t samples_to_copy = ((FLAC_ENCODER_BLOCK_SIZE <= samples_remaining) ? FLAC_ENCODER_BLOCK_SIZE : samples_remaining) - pcm_write_index;
      for (uint32_t ch = 0; ch < AUDIO_NUM_ENCODED_CHANNELS; ++ch)
         arm_copy_q15(audio_data + (ch * AUDIO_PACKET_NUM_SAMPLES) + read_index, pcm_channels[ch] + pcm_write_index, samples_to_copy);
      pcm_write_index = (pcm_write_index + samples_to_copy) % FLAC_ENCODER_BLOCK_SIZE;
      samples_remaining -= samples_to_copy;
      samples_written += samples_to_copy;
      read_index += samples_to_copy;

      // Check if the PCM buffer has been completely filled
      if (!pcm_write_index)
      {
         // Format the raw audio data as FLAC
         tflac_encode_s16p(&flac_encoder, FLAC_ENCODER_BLOCK_SIZE, pcm_channels, output_buffer, output_buffer_len, &bytes_written);
         sd_card_write_bytes(output_buffer, bytes_written);

         // Determine whether the full audio clip has been written
         if (samples_written >= AUDIO_CLIP_MIN_NUM_SAMPLES)
            sd_card_close_audio_file();
      }
   }
}


// Context-Switching Functions -----------------------------------------------------------------------------------------

static void switch_context(void)
{
#if USE_SETJMP_FOR_SD_STORAGE > 0

   // If this call returns 0, we are in the yielding task, otherwise we are in the new task
   static volatile uint32_t current_task_context = 1;
   if (setjmp(task_contexts[current_task_context]) == 0)
   {
      current_task_context = (current_task_context + 1) % NUM_TASK_CONTEXTS;
      longjmp(task_contexts[current_task_context], 1);
   }

#else

   // Ensure that we time out before the next data packet arrives
   if (comms_cycles_since_data_received() >= timeout_num_cycles)
      sd_timed_out = 1;

#endif  // #if USE_SETJMP_FOR_SD_STORAGE > 0
}

#if USE_SETJMP_FOR_SD_STORAGE > 0

static void sd_card_async_process(void)
{
   // Loop forever handling slow SD card operations
   while (1)
   {
      // Check whether an SD card state change has occurred
      if (sd_card_state_changed)
      {
         // Attempt to initialize or disable the SD card based on its detection status
         if ((sd_card_state_changed - 1) && enable_sd_card())
         {
            // Mount the SD card file system
            char sd_card_path[4] = { 0 };
            f_mount(&file_system, (TCHAR const*)sd_card_path, 1);
         }
         else
            disable_sd_card();
      }

      // Check if there are any SD card commands to process
      if (sd_card_command_read_index != sd_card_command_write_index)
      {
         // Determine which SD card operation to execute
         const sd_card_command_t* const cmd = &sd_card_command_queue[sd_card_command_read_index];
         switch (cmd->operation)
         {
            case SD_CARD_OPEN_FILE:
               sd_card_open_file(cmd->operation_data);
               break;
            case SD_CARD_WRITE_FILE:
               sd_card_write_audio_file((int16_t*)cmd->operation_data);
               break;
            case SD_CARD_IDLE:  // Intentional fall-through
            default:
               break;
         }

         // Remove the processed command from the queue
         sd_card_command_read_index = (sd_card_command_read_index + 1) % MAX_NUM_SD_CARD_COMMANDS;
      }
      else  // Yield control back to the main thread
         switch_context();
   }
}

#endif  // #if USE_SETJMP_FOR_SD_STORAGE > 0


// FatFS Required Driver Functions -------------------------------------------------------------------------------------

DSTATUS disk_initialize(BYTE)
{
   // Send a "Send Status" command (CMD13) to the SD card
   WRITE_REG(sd_card_status, STA_NOINIT);
   WRITE_REG(SDMMC1->ARG, sd_card_details.address);
   MODIFY_REG(SDMMC1->CMD, CMD_CLEAR_MASK, (SDMMC_CMD_SEND_STATUS | SDMMC_RESPONSE_SHORT | SDMMC_WAIT_NO | SDMMC_CPSM_ENABLE));

   // Wait for a response or a timeout
   while (!READ_BIT(SDMMC1->STA, (SDMMC_FLAG_CCRCFAIL | SDMMC_FLAG_CMDREND | SDMMC_FLAG_CTIMEOUT | SDMMC_FLAG_BUSYD0END)) || READ_BIT(SDMMC1->STA, SDMMC_FLAG_CMDACT));

   // Validate the response and clear all peripheral flags
   if (!READ_BIT(SDMMC1->STA, (SDMMC_FLAG_CTIMEOUT | SDMMC_FLAG_CCRCFAIL)) && ((uint8_t)SDMMC1->RESPCMD == SDMMC_CMD_SEND_STATUS) && !READ_BIT(SDMMC1->RESP1, SDMMC_OCR_ERRORBITS) && (((READ_REG(SDMMC1->RESP1) >> 9U) & 0x0FU) == HAL_SD_CARD_TRANSFER))
      CLEAR_BIT(sd_card_status, STA_NOINIT);
   WRITE_REG(SDMMC1->ICR, SDMMC_STATIC_CMD_FLAGS);
   return sd_card_status;
}

DSTATUS disk_status(BYTE)
{
   // Simply return the previously determined disk status
   return sd_card_status;
}

DRESULT disk_read(BYTE, BYTE *buff, LBA_t sector, UINT count)
{
   // Verify that the SD card is available and not full
   if (READ_BIT(sd_card_status, STA_NOINIT) || ((sector + count) > sd_card_details.num_sectors))
      return RES_ERROR;

   // Fix the read sector address for SDSC cards
   if (sd_card_details.card_type != CARD_SDHC_SDXC)
      sector *= 512;

   // Try to read up to two times to account for potential CRC errors
   sd_rx_cplt = 0;
   for (uint32_t attempt = 0; !sd_card_state_changed && !sd_timed_out && !sd_rx_cplt && (attempt < 2); ++attempt)
   {
      // Configure the SD DPSM (Data Path State Machine)
      WRITE_REG(SDMMC1->DCTRL, 0U);
      WRITE_REG(SDMMC1->DTIMER, 25000000);
      WRITE_REG(SDMMC1->DLEN, (sd_card_details.sector_size * count));
      MODIFY_REG(SDMMC1->DCTRL, DCTRL_CLEAR_MASK, (SDMMC_DATABLOCK_SIZE_512B | SDMMC_TRANSFER_DIR_TO_SDMMC | SDMMC_TRANSFER_MODE_BLOCK | SDMMC_DPSM_DISABLE));
      WRITE_REG(SDMMC1->IDMABASER, (uint32_t)buff);
      WRITE_REG(SDMMC1->IDMACTRL, SDMMC_ENABLE_IDMA_SINGLE_BUFF);

      // Clear and enable relevant SD card interrupts
      WRITE_REG(SDMMC1->ICR, (SDMMC_STATIC_CMD_FLAGS | SDMMC_STATIC_DATA_FLAGS));
      SET_BIT(SDMMC1->MASK, (SDMMC_FLAG_CTIMEOUT | SDMMC_FLAG_CCRCFAIL | SDMMC_IT_DTIMEOUT | SDMMC_IT_DCRCFAIL | SDMMC_IT_RXOVERR | SDMMC_IT_DATAEND));

      // Send the SD card command to begin reading data blocks
      sd_result_ready = 0;
      sd_xfer_context = (count > 1) ? SDMMC_CMD_READ_MULT_BLOCK : SDMMC_CMD_READ_SINGLE_BLOCK;
      WRITE_REG(SDMMC1->ARG, sector);
      MODIFY_REG(SDMMC1->CMD, CMD_CLEAR_MASK, (sd_xfer_context | SDMMC_CMD_CMDTRANS | SDMMC_RESPONSE_SHORT | SDMMC_WAIT_NO | SDMMC_CPSM_ENABLE));

      // Context switch until a response is received or the SD card changes state
      while (!sd_result_ready && !sd_card_state_changed && !sd_timed_out)
         switch_context();
      sd_result_ready = 0;
   }
   return !sd_rx_cplt;
}

DRESULT disk_write(BYTE, const BYTE *buff, LBA_t sector, UINT count)
{
   // Verify that the SD card is available and not full
   if (READ_BIT(sd_card_status, STA_NOINIT) || ((sector + count) > sd_card_details.num_sectors))
      return RES_ERROR;

   // Fix the write sector address for SDSC cards
   if (sd_card_details.card_type != CARD_SDHC_SDXC)
      sector *= 512;

   // Try to write up to two times to account for potential CRC errors
   sd_tx_cplt = 0;
   for (uint32_t attempt = 0; !sd_card_state_changed && !sd_timed_out && !sd_tx_cplt && (attempt < 2); ++attempt)
   {
      // Configure the SD DPSM (Data Path State Machine)
      WRITE_REG(SDMMC1->DCTRL, 0U);
      WRITE_REG(SDMMC1->DTIMER, 25000000);
      WRITE_REG(SDMMC1->DLEN, (sd_card_details.sector_size * count));
      MODIFY_REG(SDMMC1->DCTRL, DCTRL_CLEAR_MASK, (SDMMC_DATABLOCK_SIZE_512B | SDMMC_TRANSFER_DIR_TO_CARD | SDMMC_TRANSFER_MODE_BLOCK | SDMMC_DPSM_DISABLE));
      WRITE_REG(SDMMC1->IDMABASER, (uint32_t)buff);
      WRITE_REG(SDMMC1->IDMACTRL, SDMMC_ENABLE_IDMA_SINGLE_BUFF);

      // Clear and enable relevant SD card interrupts
      WRITE_REG(SDMMC1->ICR, (SDMMC_STATIC_CMD_FLAGS | SDMMC_STATIC_DATA_FLAGS));
      SET_BIT(SDMMC1->MASK, (SDMMC_FLAG_CTIMEOUT | SDMMC_FLAG_CCRCFAIL | SDMMC_IT_DTIMEOUT | SDMMC_IT_DCRCFAIL | SDMMC_IT_TXUNDERR | SDMMC_IT_DATAEND));

      // Send the SD card command to begin writing data blocks
      sd_result_ready = 0;
      sd_xfer_context = (count > 1) ? SDMMC_CMD_WRITE_MULT_BLOCK : SDMMC_CMD_WRITE_SINGLE_BLOCK;
      WRITE_REG(SDMMC1->ARG, sector);
      MODIFY_REG(SDMMC1->CMD, CMD_CLEAR_MASK, (sd_xfer_context | SDMMC_CMD_CMDTRANS | SDMMC_RESPONSE_SHORT | SDMMC_WAIT_NO | SDMMC_CPSM_ENABLE));

      // Context switch until a response is received or the SD card changes state
      while (!sd_result_ready && !sd_card_state_changed && !sd_timed_out)
         switch_context();
      sd_result_ready = 0;
   }
   return !sd_tx_cplt;
}

DRESULT disk_ioctl(BYTE, BYTE cmd, void *buff)
{
   // Verify that the disk has been initialized
   DRESULT res = RES_ERROR;
   if (READ_BIT(sd_card_status, STA_NOINIT))
      return RES_NOTRDY;

   // Carry out the requested disk ioctl function
   switch (cmd)
   {
      case CTRL_SYNC:           // Ensure there are no pending write processes
         res = RES_OK;
         break;
      case GET_SECTOR_COUNT:    // Return the number of sectors on the disk
         *(DWORD*)buff = sd_card_details.num_sectors;
         res = RES_OK;
         break;
      case GET_SECTOR_SIZE:     // Return the sector size of the disk in bytes
         *(WORD*)buff = sd_card_details.sector_size;
         res = RES_OK;
         break;
      case GET_BLOCK_SIZE:      // Return the block size in units of sectors
         *(DWORD*)buff = sd_card_details.cluster_size / sd_card_details.sector_size;
         res = RES_OK;
         break;
      default:                  // Unknown ioctl
         res = RES_PARERR;
   }
   return res;
}


// Interrupt Service Routines ------------------------------------------------------------------------------------------

void SDMMC1_IRQHandler(void)
{
   // Handle the interrupt based on which flags are set
   if (READ_BIT(SDMMC1->STA, SDMMC_FLAG_DATAEND))
   {
      // Clear flags and disable interrupts
      WRITE_REG(SDMMC1->ICR, SDMMC_STATIC_DATA_FLAGS);
      CLEAR_BIT(SDMMC1->MASK, (SDMMC_IT_DATAEND | SDMMC_IT_DCRCFAIL | SDMMC_IT_DTIMEOUT | SDMMC_IT_TXUNDERR | SDMMC_IT_RXOVERR | SDMMC_IT_TXFIFOHE | SDMMC_IT_RXFIFOHF | SDMMC_IT_IDMABTC));
      CLEAR_BIT(SDMMC1->CMD, SDMMC_CMD_CMDTRANS);
      WRITE_REG(SDMMC1->DLEN, 0);
      WRITE_REG(SDMMC1->DCTRL, 0);
      WRITE_REG(SDMMC1->IDMACTRL, SDMMC_DISABLE_IDMA);

      // Stop the transfer for multi-read or multi-write blocks
      if ((sd_xfer_context == SDMMC_CMD_READ_MULT_BLOCK) || (sd_xfer_context == SDMMC_CMD_WRITE_MULT_BLOCK))
      {
         SET_BIT(SDMMC1->CMD, SDMMC_CMD_CMDSTOP);
         WRITE_REG(SDMMC1->ARG, 0U);
         MODIFY_REG(SDMMC1->CMD, CMD_CLEAR_MASK, (SDMMC_CMD_STOP_TRANSMISSION | SDMMC_RESPONSE_SHORT | SDMMC_WAIT_NO | SDMMC_CPSM_ENABLE));
         while (!READ_BIT(SDMMC1->STA, (SDMMC_FLAG_CCRCFAIL | SDMMC_FLAG_CMDREND | SDMMC_FLAG_CTIMEOUT | SDMMC_FLAG_BUSYD0END)) || READ_BIT(SDMMC1->STA, SDMMC_FLAG_CMDACT));
         if (READ_BIT(SDMMC1->STA, SDMMC_FLAG_BUSYD0))
            while (!READ_BIT(SDMMC1->STA, SDMMC_FLAG_BUSYD0END));
         CLEAR_BIT(SDMMC1->CMD, SDMMC_CMD_CMDSTOP);
      }

      // Set the appropriate transfer complete flags
      if ((sd_xfer_context == SDMMC_CMD_WRITE_SINGLE_BLOCK) || (sd_xfer_context == SDMMC_CMD_WRITE_MULT_BLOCK))
         sd_tx_cplt = 1;
      if ((sd_xfer_context == SDMMC_CMD_READ_SINGLE_BLOCK) || (sd_xfer_context == SDMMC_CMD_READ_MULT_BLOCK))
         sd_rx_cplt = 1;
   }
   else if (READ_BIT(SDMMC1->STA, (SDMMC_FLAG_CTIMEOUT | SDMMC_FLAG_CCRCFAIL | SDMMC_FLAG_DTIMEOUT | SDMMC_FLAG_DCRCFAIL | SDMMC_FLAG_RXOVERR | SDMMC_FLAG_TXUNDERR)))
   {
      // Clear all flags and disable interrupts
      WRITE_REG(SDMMC1->ICR, SDMMC_STATIC_DATA_FLAGS);
      CLEAR_BIT(SDMMC1->MASK, (SDMMC_FLAG_CTIMEOUT | SDMMC_FLAG_CCRCFAIL | SDMMC_FLAG_DTIMEOUT | SDMMC_FLAG_DCRCFAIL | SDMMC_FLAG_RXOVERR | SDMMC_FLAG_TXUNDERR));
      CLEAR_BIT(SDMMC1->CMD, SDMMC_CMD_CMDTRANS);

      // Stop any ongoing transfers
      SET_BIT(SDMMC1->DCTRL, SDMMC_DCTRL_FIFORST);
      SET_BIT(SDMMC1->CMD, SDMMC_CMD_CMDSTOP);
      WRITE_REG(SDMMC1->ARG, 0U);
      MODIFY_REG(SDMMC1->CMD, CMD_CLEAR_MASK, (SDMMC_CMD_STOP_TRANSMISSION | SDMMC_RESPONSE_SHORT | SDMMC_WAIT_NO | SDMMC_CPSM_ENABLE));
      while (!READ_BIT(SDMMC1->STA, (SDMMC_FLAG_CCRCFAIL | SDMMC_FLAG_CMDREND | SDMMC_FLAG_CTIMEOUT | SDMMC_FLAG_BUSYD0END)) || READ_BIT(SDMMC1->STA, SDMMC_FLAG_CMDACT));
      if (READ_BIT(SDMMC1->STA, SDMMC_FLAG_BUSYD0))
         while (!READ_BIT(SDMMC1->STA, SDMMC_FLAG_BUSYD0END));
      CLEAR_BIT(SDMMC1->CMD, SDMMC_CMD_CMDSTOP);
      WRITE_REG(SDMMC1->ICR, SDMMC_FLAG_DABORT);

      // Disable the internal DMA
      CLEAR_BIT(SDMMC1->MASK, SDMMC_IT_IDMABTC);
      WRITE_REG(SDMMC1->IDMACTRL, SDMMC_DISABLE_IDMA);
   }
   sd_xfer_context = SD_CONTEXT_NONE;
   sd_result_ready = 1;
}

void sd_card_detection_isr(uint8_t sd_card_detected)
{
   // Set a flag to alert about a change of state
   if (!sd_card_state_changed)
      sd_card_state_changed = sd_card_detected + 1;
}


// Public API Functions ------------------------------------------------------------------------------------------------

void storage_init(void)
{
   // Initialize all static variables
   sd_card_status = STA_NOINIT;
   output_buffer_len = audio_file_open = 0;
   timeout_num_cycles = SystemCoreClock * AUDIO_PACKET_NUM_SAMPLES / AUDIO_PACKET_SAMPLE_RATE;
   for (uint32_t ch = 0; ch < AUDIO_NUM_ENCODED_CHANNELS; ++ch)
      pcm_channels[ch] = pcm[ch];

   // Enable the GPIO clocks
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOCEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOCEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOEEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOEEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOHEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOHEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIONEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIONEN);

   // Initialize the non-SDMMC GPIO pins
   uint32_t position = 32 - __builtin_clz(SD_PWR_SELECT_Pin) - 1;
   MODIFY_REG(SD_PWR_SELECT_GPIO_Port->OSPEEDR, (GPIO_OSPEEDR_OSPEED0 << (position * 2U)), (GPIO_SPEED_FREQ_LOW << (position * 2U)));
   MODIFY_REG(SD_PWR_SELECT_GPIO_Port->OTYPER, (GPIO_OTYPER_OT0 << position), (((GPIO_MODE_OUTPUT_PP & OUTPUT_TYPE) >> OUTPUT_TYPE_Pos) << position));
   MODIFY_REG(SD_PWR_SELECT_GPIO_Port->PUPDR, (GPIO_PUPDR_PUPD0 << (position * 2U)), (GPIO_NOPULL << (position * 2U)));
   MODIFY_REG(SD_PWR_SELECT_GPIO_Port->AFR[position >> 3U], (0xFU << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)), (GPIO_AF4_I2C3 << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)));
   MODIFY_REG(SD_PWR_SELECT_GPIO_Port->MODER, (GPIO_MODER_MODE0 << (position * 2U)), ((GPIO_MODE_OUTPUT_PP & GPIO_MODE) << (position * 2U)));
   position = 32 - __builtin_clz(SD_CARD_EN_Pin) - 1;
   MODIFY_REG(SD_CARD_EN_GPIO_Port->OSPEEDR, (GPIO_OSPEEDR_OSPEED0 << (position * 2U)), (GPIO_SPEED_FREQ_LOW << (position * 2U)));
   MODIFY_REG(SD_CARD_EN_GPIO_Port->OTYPER, (GPIO_OTYPER_OT0 << position), (((GPIO_MODE_OUTPUT_PP & OUTPUT_TYPE) >> OUTPUT_TYPE_Pos) << position));
   MODIFY_REG(SD_CARD_EN_GPIO_Port->PUPDR, (GPIO_PUPDR_PUPD0 << (position * 2U)), (GPIO_NOPULL << (position * 2U)));
   MODIFY_REG(SD_CARD_EN_GPIO_Port->AFR[position >> 3U], (0xFU << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)), (GPIO_AF4_I2C3 << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)));
   MODIFY_REG(SD_CARD_EN_GPIO_Port->MODER, (GPIO_MODER_MODE0 << (position * 2U)), ((GPIO_MODE_OUTPUT_PP & GPIO_MODE) << (position * 2U)));
   position = 32 - __builtin_clz(SD_CARD_DETECT_Pin) - 1;
   MODIFY_REG(SD_CARD_DETECT_GPIO_Port->OSPEEDR, (GPIO_OSPEEDR_OSPEED0 << (position * 2U)), (GPIO_SPEED_FREQ_LOW << (position * 2U)));
   MODIFY_REG(SD_CARD_DETECT_GPIO_Port->OTYPER, (GPIO_OTYPER_OT0 << position), (((GPIO_MODE_IT_RISING_FALLING & OUTPUT_TYPE) >> OUTPUT_TYPE_Pos) << position));
   MODIFY_REG(SD_CARD_DETECT_GPIO_Port->PUPDR, (GPIO_PUPDR_PUPD0 << (position * 2U)), (GPIO_NOPULL << (position * 2U)));
   MODIFY_REG(SD_CARD_DETECT_GPIO_Port->MODER, (GPIO_MODER_MODE0 << (position * 2U)), ((GPIO_MODE_IT_RISING_FALLING & GPIO_MODE) << (position * 2U)));
   uint32_t iocurrent = SD_CARD_DETECT_Pin & (1UL << position);
   MODIFY_REG(EXTI->EXTICR[position >> 2U], (0x0FUL << ((position & 0x03U) * EXTI_EXTICR1_EXTI1_Pos)), (0x08UL << ((position & 0x03U) * EXTI_EXTICR1_EXTI1_Pos)));
   SET_BIT(EXTI->IMR1, iocurrent);
   CLEAR_BIT(EXTI->EMR1, iocurrent);
   SET_BIT(EXTI->RTSR1, iocurrent);
   SET_BIT(EXTI->FTSR1, iocurrent);
   *(&EXTI->SECCFGR1 + (0x08U * ((EXTI_LINE_12 & EXTI_REG_MASK) >> EXTI_REG_SHIFT))) |= (1UL << (EXTI_LINE_12 & EXTI_PIN_MASK));

   // Ensure that the SD card is powered off
   MODIFY_REG(PWR->SVMCR1, PWR_SVMCR1_VDDIO4VRSEL, PWR_VDDIO_RANGE_3V3 << PWR_SVMCR1_VDDIO4VRSEL_Pos);
   WRITE_REG(SD_PWR_SELECT_GPIO_Port->BRR, SD_PWR_SELECT_Pin);
   WRITE_REG(SD_CARD_EN_GPIO_Port->BRR, SD_CARD_EN_Pin);

   // Initialize the SDMMC GPIO pins
   const gpio_pin_t sdmmc_pins[] = SDMMC_PINS;
   for (uint32_t i = 0; i < (sizeof(sdmmc_pins) / sizeof(sdmmc_pins[0])); ++i)
   {
      const uint32_t position = 32 - __builtin_clz(sdmmc_pins[i].pin) - 1;
      MODIFY_REG(sdmmc_pins[i].port->OSPEEDR, (GPIO_OSPEEDR_OSPEED0 << (position * 2U)), (GPIO_SPEED_FREQ_MEDIUM << (position * 2U)));
      MODIFY_REG(sdmmc_pins[i].port->OTYPER, (GPIO_OTYPER_OT0 << position), (((GPIO_MODE_AF_PP & OUTPUT_TYPE) >> OUTPUT_TYPE_Pos) << position));
      MODIFY_REG(sdmmc_pins[i].port->PUPDR, (GPIO_PUPDR_PUPD0 << (position * 2U)), (GPIO_NOPULL << (position * 2U)));
      MODIFY_REG(sdmmc_pins[i].port->AFR[position >> 3U], (0xFU << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)), (GPIO_AF10_SDMMC1 << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)));
      MODIFY_REG(sdmmc_pins[i].port->MODER, (GPIO_MODER_MODE0 << (position * 2U)), ((GPIO_MODE_AF_PP & GPIO_MODE) << (position * 2U)));
   }

   // Enable interrupts based on the SD_CARD_DETECT pin
   NVIC_SetPriority(EXTI12_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 1, 0));
   NVIC_EnableIRQ(EXTI12_IRQn);
   NVIC_SetPriority(SDMMC1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
   NVIC_EnableIRQ(SDMMC1_IRQn);

   // Initialize the FLAC encoder
   tflac_init(&flac_encoder);
   flac_encoder.samplerate = AUDIO_PACKET_SAMPLE_RATE;
   flac_encoder.channels = AUDIO_NUM_ENCODED_CHANNELS;
   flac_encoder.bitdepth = AUDIO_BITS_PER_SAMPLE;
   flac_encoder.blocksize = FLAC_ENCODER_BLOCK_SIZE;
   flac_encoder.max_partition_order = FLAC_ENCODER_PARTITION_ORDER;
   flac_encoder.enable_md5 = 0;
   if (!tflac_validate(&flac_encoder, flac_encoder_mem, tflac_size_memory(FLAC_ENCODER_BLOCK_SIZE)))
      output_buffer_len = tflac_size_frame(FLAC_ENCODER_BLOCK_SIZE, AUDIO_NUM_ENCODED_CHANNELS, AUDIO_BITS_PER_SAMPLE);

#if USE_SETJMP_FOR_SD_STORAGE > 0

   // Set up an independent SD card processing context for slow operations
   sd_card_command_read_index = sd_card_command_write_index = 0;
   if (setjmp(task_contexts[0]) == 0)
   {
      sd_card_state_changed = READ_BIT(SD_CARD_DETECT_GPIO_Port->IDR, SD_CARD_DETECT_Pin) ? 2 : 1;
      __set_MSPLIM((uint32_t)&sd_card_context_stack);
      __set_MSP((uint32_t)&sd_card_context_stack + STORAGE_TASK_STACK_SIZE);
      sd_card_async_process();
   }

#else

   // Attempt to initialize the SD card
   if (enable_sd_card())
   {
      // Mount the SD card file system
      char sd_card_path[4] = { 0 };
      f_mount(&file_system, (TCHAR const*)sd_card_path, 1);
   }
   else
      disable_sd_card();

#endif  // #if USE_SETJMP_FOR_SD_STORAGE > 0
}

void storage_handle_sd_card_state_change(void)
{
#if USE_SETJMP_FOR_SD_STORAGE > 0

   // Switch to the SD card task if a pending task has completed or the card state has changed
   if (sd_result_ready || sd_card_state_changed)
      switch_context();

#else

   // Check whether an SD card state change has occurred
   if (sd_card_state_changed)
   {
      // Attempt to initialize or disable the SD card based on its detection status
      if ((sd_card_state_changed - 1) && enable_sd_card())
      {
         // Mount the SD card file system
         char sd_card_path[4] = { 0 };
         f_mount(&file_system, (TCHAR const*)sd_card_path, 1);
      }
      else
         disable_sd_card();
   }

#endif  // #if USE_SETJMP_FOR_SD_STORAGE > 0
}

void storage_open_audio_file(uint32_t audio_timestamp)
{
#if USE_SETJMP_FOR_SD_STORAGE > 0

   // Add the open-file command to the SD card command queue
   const uint32_t next_sd_card_command_write_index = (sd_card_command_write_index + 1) % MAX_NUM_SD_CARD_COMMANDS;
   if (sd_card_initialized && !audio_file_open && next_sd_card_command_write_index != sd_card_command_read_index)
   {
      sd_card_command_queue[sd_card_command_write_index].operation = SD_CARD_OPEN_FILE;
      sd_card_command_queue[sd_card_command_write_index].operation_data = audio_timestamp;
      sd_card_command_write_index = next_sd_card_command_write_index;
      switch_context();
   }

#else

   // Open a new file on the SD card
   sd_timed_out = 0;
   sd_card_open_file(audio_timestamp);

#endif  // #if USE_SETJMP_FOR_SD_STORAGE > 0
}

void storage_write_audio_file(volatile int16_t *audio_data)
{
#if USE_SETJMP_FOR_SD_STORAGE > 0

   // Add the write-audio command to the SD card command queue
   const uint32_t next_sd_card_command_write_index = (sd_card_command_write_index + 1) % MAX_NUM_SD_CARD_COMMANDS;
   if (sd_card_initialized && (next_sd_card_command_write_index != sd_card_command_read_index))
   {
      sd_card_command_queue[sd_card_command_write_index].operation = SD_CARD_WRITE_FILE;
      sd_card_command_queue[sd_card_command_write_index].operation_data = (uint32_t)audio_data;
      sd_card_command_write_index = next_sd_card_command_write_index;
      switch_context();
   }

#else

   // Write the audio data to a currently open file
   sd_timed_out = 0;
   sd_card_write_audio_file((int16_t*)audio_data);

#endif  // #if USE_SETJMP_FOR_SD_STORAGE > 0
}
