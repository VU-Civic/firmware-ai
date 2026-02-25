#define TFLAC_IMPLEMENTATION
#define TFLAC_DISABLE_COUNTERS

// Header Inclusions ---------------------------------------------------------------------------------------------------

#include <arm_math.h>
#include <arm_mve.h>
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

#define AUDIO_CLIP_HISTORY_NUM_SAMPLES        (STORAGE_AUDIO_CLIP_HISTORY_SECONDS * AUDIO_PACKET_SAMPLE_RATE)
#define SD_CARD_BUFFER_NUM_BYTES              16384

#define SDMMC_CLK                             200000000U
#ifdef SD_LOW_VOLTAGE_SIGNALING
#define SD_MAX_BUS_SPEED_MODE                 SDMMC_SDR50_SWITCH_PATTERN
#else
#define SD_MAX_BUS_SPEED_MODE                 SDMMC_SDR25_SWITCH_PATTERN
#endif

typedef struct
{
   uint64_t card_size;
   uint32_t card_type, card_version, card_class, card_speed;
   uint32_t address, num_clusters, cluster_size, num_sectors, sector_size;
} sd_card_details_t;


// Static Storage Variables --------------------------------------------------------------------------------------------

static volatile uint8_t audio_file_open;
static volatile DSTATUS sd_card_status;
static volatile uint32_t sd_xfer_context, sd_result_ready, sd_timed_out;
static volatile uint8_t sd_rx_cplt, sd_tx_cplt, sd_card_initialized, sd_card_state_changed;
static uint32_t min_clip_samples, samples_written, output_buffer_len, timeout_num_cycles;
static sd_card_details_t sd_card_details;
static double previous_timestamp;

#ifdef USE_FLAC_ENCODER

static uint32_t pcm_write_index, bytes_written, sd_write_index;
static int8_t output_buffer[8219];
static tflac flac_encoder;

#endif  // #ifdef USE_FLAC_ENCODER

__attribute__ ((section(".noncacheable")))
static FATFS file_system;

__attribute__ ((section(".noncacheable")))
static FIL audio_file;

#ifdef USE_FLAC_ENCODER

__attribute__ ((section(".noncacheable")))
static int8_t sd_write_buffer[2*SD_CARD_BUFFER_NUM_BYTES];

__attribute__ ((section(".noncacheable")))
static uint8_t flac_encoder_mem[81936];

__attribute__ ((section(".noncacheable")))
static int16_t pcm[FLAC_ENCODER_BLOCK_SIZE];

//__attribute__ ((section(".nonessential")))
//static int16_t pcm_history[AUDIO_CLIP_HISTORY_NUM_SAMPLES];

#endif  // #ifdef USE_FLAC_ENCODER


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

#ifdef SD_LOW_VOLTAGE_SIGNALING

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

#else
      voltage_attempt = 2;
#endif  // #ifdef SD_LOW_VOLTAGE_SIGNALING
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

static void sd_card_write_audio_metadata(volatile audio_packet_t *audio_data, const ai_data_t *ai_results)
{
   UINT data_written;
   static char line_buffer[32];
   uint32_t whole = (uint32_t)audio_data->timestamp, fraction = (uint32_t)((audio_data->timestamp - whole) * 1000);
   int num_chars = snprintf(line_buffer, sizeof(line_buffer), "Raw Timestamp: %lu.%03lu\n", whole, fraction);
   f_write(&audio_file, line_buffer, num_chars, &data_written);
   num_chars = snprintf(line_buffer, sizeof(line_buffer), "Gunshot Probability: %u%%\n", (unsigned int)ai_results->class_probabilities[0]);
   f_write(&audio_file, line_buffer, num_chars, &data_written);
   int32_t whole_int = (int32_t)audio_data->lat; fraction = (uint32_t)(fabsf(audio_data->lat - whole_int) * 1000000000);
   num_chars = snprintf(line_buffer, sizeof(line_buffer), "Lat: %ld.%09lu\n", whole_int, fraction);
   f_write(&audio_file, line_buffer, num_chars, &data_written);
   whole_int = (int32_t)audio_data->lon; fraction = (uint32_t)(fabsf(audio_data->lon - whole_int) * 1000000000);
   num_chars = snprintf(line_buffer, sizeof(line_buffer), "Lon: %ld.%09lu\n", whole_int, fraction);
   f_write(&audio_file, line_buffer, num_chars, &data_written);
   whole_int = (int32_t)audio_data->ht; fraction = (uint32_t)(fabsf(audio_data->ht - whole_int) * 1000);
   num_chars = snprintf(line_buffer, sizeof(line_buffer), "Ht: %ld.%03lu\n", whole_int, fraction);
   f_write(&audio_file, line_buffer, num_chars, &data_written);
   num_chars = snprintf(line_buffer, sizeof(line_buffer), "Q1: %ld\n", (int32_t)audio_data->q1);
   f_write(&audio_file, line_buffer, num_chars, &data_written);
   num_chars = snprintf(line_buffer, sizeof(line_buffer), "Q2: %ld\n", (int32_t)audio_data->q2);
   f_write(&audio_file, line_buffer, num_chars, &data_written);
   num_chars = snprintf(line_buffer, sizeof(line_buffer), "Q3: %ld\n", (int32_t)audio_data->q3);
   f_write(&audio_file, line_buffer, num_chars, &data_written);
   for (uint32_t retries = 0; audio_file_open && (retries < 1000); ++retries)
   {
      sd_timed_out = 0;
      audio_file_open = (f_close(&audio_file) != FR_OK);
   }
}

#ifdef USE_FLAC_ENCODER

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
         tflac_encode_s16p(&flac_encoder, pcm_write_index, (int16_t**)&pcm, output_buffer, output_buffer_len, &bytes_written);
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
      for (uint32_t retries = 0; audio_file_open && (retries < 1000); ++retries)
      {
         sd_timed_out = 0;
         audio_file_open = (f_close(&audio_file) != FR_OK);
      }
   }
}

static void sd_card_open_file(uint32_t audio_timestamp, volatile audio_packet_t *audio_data, const ai_data_t *ai_results, uint8_t clip_length_seconds)
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
   static uint32_t audio_directory_timestamp;
   static char time_string[10], audio_directory[14];
   strftime(time_string, sizeof(time_string), "%H%M%S", curr_time);
   if ((audio_timestamp - audio_directory_timestamp) >= 3600)
   {
      // Generate a new directory name from the current date and time
      uint8_t success = 1;
      static FILINFO file_info;
      curr_time->tm_min = curr_time->tm_sec = 0;
      memset(audio_directory, 0, sizeof(audio_directory));
      strftime(audio_directory, sizeof(audio_directory), "%Y", curr_time);
      if (success && (f_stat(audio_directory, &file_info) != FR_OK))
         success = (f_mkdir(audio_directory) == FR_OK);
      strftime(audio_directory, sizeof(audio_directory), "%Y/%m", curr_time);
      if (success && (f_stat(audio_directory, &file_info) != FR_OK))
         success = (f_mkdir(audio_directory) == FR_OK);
      strftime(audio_directory, sizeof(audio_directory), "%Y/%m/%d", curr_time);
      if (success && (f_stat(audio_directory, &file_info) != FR_OK))
         success = (f_mkdir(audio_directory) == FR_OK);
      strftime(audio_directory, sizeof(audio_directory), "%Y/%m/%d/%H", curr_time);
      if (success && (f_stat(audio_directory, &file_info) != FR_OK))
         success = (f_mkdir(audio_directory) == FR_OK);
      if (success)
         audio_directory_timestamp = (uint32_t)mktime(curr_time);
   }

   // Create a file to contain the corresponding audio metadata
   static char file_name[32];
   snprintf(file_name, sizeof(file_name), "%s/%s.meta", audio_directory, time_string);
   audio_file_open = (f_open(&audio_file, file_name, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);
   if (audio_file_open)
   {
      sd_card_write_audio_metadata(audio_data, ai_results);
      for (uint32_t retries = 0; audio_file_open && (retries < 1000); ++retries)
      {
         sd_timed_out = 0;
         audio_file_open = (f_close(&audio_file) != FR_OK);
      }
   }

   // Open the requested audio file
   snprintf(file_name, sizeof(file_name), "%s/%s.flac", audio_directory, time_string);
   audio_file_open = (f_open(&audio_file, file_name, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);

   // Initialize a new FLAC encoder
   if (audio_file_open)
   {
      // Reset the FLAC encoder
      min_clip_samples = (uint32_t)clip_length_seconds * AUDIO_PACKET_SAMPLE_RATE;
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
      arm_copy_q15(audio_data + read_index, &pcm[pcm_write_index], samples_to_copy);
      pcm_write_index = (pcm_write_index + samples_to_copy) % FLAC_ENCODER_BLOCK_SIZE;
      samples_remaining -= samples_to_copy;
      samples_written += samples_to_copy;
      read_index += samples_to_copy;

      // Check if the PCM buffer has been completely filled
      if (!pcm_write_index)
      {
         // Format the raw audio data as FLAC
         tflac_encode_s16p(&flac_encoder, FLAC_ENCODER_BLOCK_SIZE, (int16_t**)&pcm, output_buffer, output_buffer_len, &bytes_written);
         sd_card_write_bytes(output_buffer, bytes_written);

         // Determine whether the full audio clip has been written
         if (samples_written >= min_clip_samples)
            sd_card_close_audio_file();
      }
   }
}

#else

static void sd_card_close_audio_file(void)
{
   // Finalize and close the currently open audio file
   if (audio_file_open)
   {
      UINT data_written;
      f_lseek(&audio_file, 4);
      uint32_t field = 36 + (samples_written * sizeof(int16_t) * AUDIO_PACKET_NUM_CHANNELS);
      f_write(&audio_file, &field, 4, &data_written);
      f_lseek(&audio_file, 40);
      field = samples_written * sizeof(int16_t) * AUDIO_PACKET_NUM_CHANNELS;
      f_write(&audio_file, &field, 4, &data_written);
      for (uint32_t retries = 0; audio_file_open && (retries < 1000); ++retries)
      {
         sd_timed_out = 0;
         audio_file_open = (f_close(&audio_file) != FR_OK);
      }
   }
}

static void sd_card_open_file(uint32_t audio_timestamp, volatile audio_packet_t *audio_data, const ai_data_t *ai_results, uint8_t clip_length_seconds)
{
   // Do not continue if the SD card is not initialized or an audio file is already open
   if (!sd_card_initialized || audio_file_open)
      return;

   // Determine if time to create a new storage directory
   const time_t timestamp = (time_t)audio_timestamp;
   struct tm *curr_time = gmtime(&timestamp);
   static uint32_t audio_directory_timestamp;
   static char time_string[10], audio_directory[14];
   strftime(time_string, sizeof(time_string), "%H%M%S", curr_time);
   if ((audio_timestamp - audio_directory_timestamp) >= 3600)
   {
      // Generate a new directory name from the current date and time
      uint8_t success = 1;
      static FILINFO file_info;
      curr_time->tm_min = curr_time->tm_sec = 0;
      memset(audio_directory, 0, sizeof(audio_directory));
      strftime(audio_directory, sizeof(audio_directory), "%Y", curr_time);
      if (success && (f_stat(audio_directory, &file_info) != FR_OK))
         success = (f_mkdir(audio_directory) == FR_OK);
      strftime(audio_directory, sizeof(audio_directory), "%Y/%m", curr_time);
      if (success && (f_stat(audio_directory, &file_info) != FR_OK))
         success = (f_mkdir(audio_directory) == FR_OK);
      strftime(audio_directory, sizeof(audio_directory), "%Y/%m/%d", curr_time);
      if (success && (f_stat(audio_directory, &file_info) != FR_OK))
         success = (f_mkdir(audio_directory) == FR_OK);
      strftime(audio_directory, sizeof(audio_directory), "%Y/%m/%d/%H", curr_time);
      if (success && (f_stat(audio_directory, &file_info) != FR_OK))
         success = (f_mkdir(audio_directory) == FR_OK);
      if (success)
         audio_directory_timestamp = (uint32_t)mktime(curr_time);
   }

   // Create a file to contain the corresponding audio metadata
   static char file_name[32];
   snprintf(file_name, sizeof(file_name), "%s/%s.meta", audio_directory, time_string);
   audio_file_open = (f_open(&audio_file, file_name, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);
   if (audio_file_open)
   {
      sd_card_write_audio_metadata(audio_data, ai_results);
      for (uint32_t retries = 0; audio_file_open && (retries < 1000); ++retries)
      {
         sd_timed_out = 0;
         audio_file_open = (f_close(&audio_file) != FR_OK);
      }
   }

   // Open the requested audio file
   snprintf(file_name, sizeof(file_name), "%s/%s.wav", audio_directory, time_string);
   audio_file_open = (f_open(&audio_file, file_name, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);

   // Initialize a new WAV writer
   if (audio_file_open)
   {
      // Reset the writer and write the WAV header
      UINT data_written;
      uint16_t num_channels = AUDIO_PACKET_NUM_CHANNELS;
      uint32_t field = 36, bytes_per_sample = 2, sample_rate_hz = AUDIO_PACKET_SAMPLE_RATE;
      f_write(&audio_file, "RIFF", 4, &data_written);
      f_write(&audio_file, &field, 4, &data_written);
      f_write(&audio_file, "WAVE", 4, &data_written);
      f_write(&audio_file, "fmt ", 4, &data_written);
      field = 16;
      f_write(&audio_file, &field, 4, &data_written);
      field = 1;
      f_write(&audio_file, &field, 2, &data_written);
      f_write(&audio_file, &num_channels, 2, &data_written);
      f_write(&audio_file, &sample_rate_hz, 4, &data_written);
      field = sample_rate_hz * num_channels * bytes_per_sample;
      f_write(&audio_file, &field, 4, &data_written);
      field = num_channels * bytes_per_sample;
      f_write(&audio_file, &field, 2, &data_written);
      field = 8 * bytes_per_sample;
      f_write(&audio_file, &field, 2, &data_written);
      f_write(&audio_file, "data", 4, &data_written);
      f_write(&audio_file, &field, 4, &data_written);
      min_clip_samples = (uint32_t)clip_length_seconds * AUDIO_PACKET_SAMPLE_RATE;
      samples_written = 0;
   }
}

static void sd_card_write_audio_file(int16_t *audio_data)
{
   // Only proceed if there is an open audio file
   static int16_t pcm[AUDIO_PACKET_TOTAL_SAMPLES];
   if (sd_card_initialized && audio_file_open)
   {
      // Interleave the audio samples
      UINT data_written = 0;
      for (uint32_t sample_index = 0; sample_index < AUDIO_PACKET_NUM_SAMPLES; sample_index += 8)
      {
         int16x8x4_t channels;
         channels.val[0] = vld1q_s16(&audio_data[(0*AUDIO_PACKET_NUM_SAMPLES)+sample_index]);
         channels.val[1] = vld1q_s16(&audio_data[(1*AUDIO_PACKET_NUM_SAMPLES)+sample_index]);
         channels.val[2] = vld1q_s16(&audio_data[(2*AUDIO_PACKET_NUM_SAMPLES)+sample_index]);
         channels.val[3] = vld1q_s16(&audio_data[(3*AUDIO_PACKET_NUM_SAMPLES)+sample_index]);
         vst4q_s16(&pcm[sample_index*AUDIO_PACKET_NUM_CHANNELS], channels);
      }
      SCB_CleanDCache_by_Addr(pcm, sizeof(pcm));
      if ((f_write(&audio_file, pcm, sizeof(pcm), &data_written) == FR_OK) && (data_written == sizeof(pcm)))
         system_feed_watchdog();

      // Determine whether a full audio clip has been written
      samples_written += AUDIO_PACKET_NUM_SAMPLES;
      if (samples_written >= min_clip_samples)
         sd_card_close_audio_file();
   }
}

#endif  // #ifdef USE_FLAC_ENCODER


// Context-Switching Functions -----------------------------------------------------------------------------------------

static void switch_context(void)
{
   // Ensure that we time out before the next data packet arrives
   if (comms_cycles_since_data_received() >= timeout_num_cycles)
      sd_timed_out = 1;
}


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
   previous_timestamp = 6400.0;
   output_buffer_len = samples_written = audio_file_open = 0;
   timeout_num_cycles = ((SystemCoreClock + AUDIO_PACKET_SAMPLE_RATE) / AUDIO_PACKET_SAMPLE_RATE) * AUDIO_PACKET_NUM_SAMPLES * 2;

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

#ifdef USE_FLAC_ENCODER

   // Power on and enable the FLEXRAM memory bank
   WRITE_REG(RCC->AHB2ENSR, RCC_AHB2ENR_RAMCFGEN);
   (void)READ_BIT(RCC->AHB2ENR, RCC_AHB2ENR_RAMCFGEN);
   WRITE_REG(RCC->MEMENSR, RCC_MEMENR_FLEXRAMEN);
   (void)READ_BIT(RCC->MEMENR, RCC_MEMENR_FLEXRAMEN);
   CLEAR_BIT(RAMCFG_FLEXRAM->CR, RAMCFG_AXISRAM_POWERDOWN);

   // Enable secure access for the FLEXMEM
   system_set_risaf_default(RISAF7);

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

#endif  // #ifdef USE_FLAC_ENCODER

   // Enable interrupts based on the SD_CARD_DETECT pin
   NVIC_SetPriority(EXTI12_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 3, 0));
   NVIC_EnableIRQ(EXTI12_IRQn);
   NVIC_SetPriority(SDMMC1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 2, 0));
   NVIC_EnableIRQ(SDMMC1_IRQn);

   // Attempt to initialize the SD card
   if (enable_sd_card())
   {
      // Mount the SD card file system
      char sd_card_path[4] = { 0 };
      f_mount(&file_system, (TCHAR const*)sd_card_path, 1);
   }
   else
      disable_sd_card();
}

void storage_handle_sd_card_state_change(void)
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
}

void storage_open_audio_file(volatile audio_packet_t *audio_data, const ai_data_t *ai_results, uint8_t clip_length_seconds)
{
   // Keep track of the most recently received audio timestamp
   const double audio_timestamp = audio_data->timestamp;
   if ((audio_timestamp < 1000.0) && (audio_timestamp <= previous_timestamp))
      previous_timestamp += 1.0 / (AUDIO_PACKET_SAMPLE_RATE / AUDIO_PACKET_NUM_SAMPLES);
   else
      previous_timestamp = audio_timestamp;

   // Open a new file on the SD card
   sd_timed_out = 0;
   sd_card_open_file((uint32_t)previous_timestamp, audio_data, ai_results, clip_length_seconds);
}

void storage_write_audio_file(volatile int16_t *audio_data)
{
   // Update the historical PCM data and write the audio to a currently open file
   sd_timed_out = 0;
   //TODO: arm_copy_q15(&pcm_history[AUDIO_PACKET_NUM_SAMPLES], &pcm_history[0], AUDIO_CLIP_HISTORY_NUM_SAMPLES - AUDIO_PACKET_NUM_SAMPLES);
   //arm_copy_q15((int16_t*)audio_data, &pcm_history[AUDIO_CLIP_HISTORY_NUM_SAMPLES - AUDIO_PACKET_NUM_SAMPLES], AUDIO_PACKET_NUM_SAMPLES);
   sd_card_write_audio_file((int16_t*)audio_data);
}

void storage_write_device_metadata_file(const char *fw_revision, volatile audio_packet_t *audio_data)
{
   // Do not continue if the SD card is not initialized
   if (!sd_card_initialized)
      return;

   // Write the desired metadata to a new file
   audio_file_open = (f_open(&audio_file, "metadata.txt", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);
   if (audio_file_open)
   {
      UINT data_written;
      char line_buffer[32];
      int num_chars = snprintf(line_buffer, sizeof(line_buffer), "FW REVISION: %s\n", fw_revision);
      f_write(&audio_file, line_buffer, num_chars, &data_written);
      num_chars = snprintf(line_buffer, sizeof(line_buffer), "IMEI: %s\n", (char*)audio_data->imei);
      f_write(&audio_file, line_buffer, num_chars, &data_written);
      num_chars = snprintf(line_buffer, sizeof(line_buffer), "IMSI: %s\n", (char*)audio_data->imsi);
      f_write(&audio_file, line_buffer, num_chars, &data_written);
      for (uint32_t retries = 0; audio_file_open && (retries < 1000); ++retries)
      {
         sd_timed_out = 0;
         audio_file_open = (f_close(&audio_file) != FR_OK);
      }
   }
}

#ifdef PERIPHERAL_TESTS

uint8_t storage_test_peripheral(void)
{
   // Return false if the SD card is not initialized
   if (!sd_card_initialized)
      return 0;

   // Write some random data to a test file
   audio_file_open = (f_open(&audio_file, "test_file.txt", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);
   if (audio_file_open)
   {
      UINT data_written = 0;
      if ((f_write(&audio_file, "abcdefghijkl", sizeof("abcdefghijkl"), &data_written) != FR_OK) || (data_written != sizeof("abcdefghijkl")))
         return 0;
      data_written = 0;
      if ((f_write(&audio_file, "mnopqrstuvwx", sizeof("mnopqrstuvwx"), &data_written) != FR_OK) || (data_written != sizeof("mnopqrstuvwx")))
         return 0;
      if (f_close(&audio_file) != FR_OK)
         return 0;
      audio_file_open = false;
   }
   else
      return 0;

   // Read back the data from the test file
   audio_file_open = (f_open(&audio_file, "test_file.txt", FA_READ) == FR_OK);
   if (audio_file_open)
   {
      UINT data_read = 0;
      uint8_t data_buffer[36];
      if ((f_read(&audio_file, data_buffer, sizeof(data_buffer), &data_read) != FR_OK) || (data_read != sizeof("abcdefghijklmnopqrstuvwx")))
          return 0;
       if (f_close(&audio_file) != FR_OK)
          return 0;
       if (memcmp(data_buffer, "abcdefghijklmnopqrstuvwx", sizeof("abcdefghijklmnopqrstuvwx")) != 0)
          return 0;
       audio_file_open = false;
   }
   else
      return 0;

   // Delete the test file and return success
   f_unlink("test_file.txt");
   return 1;
}

#endif  // #ifdef PERIPHERAL_TESTS
