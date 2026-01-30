// Header Inclusions ---------------------------------------------------------------------------------------------------

#include "flash.h"
#include "system.h"


// Flash Memory Characteristics ----------------------------------------------------------------------------------------

#define DUMMY_CYCLES_READ                                       8U
#define DUMMY_CYCLES_READ_OCTAL                                 6U
#define DUMMY_CYCLES_READ_OCTAL_DTR                             6U
#define DUMMY_CYCLES_REG_OCTAL                                  4U
#define DUMMY_CYCLES_REG_OCTAL_DTR                              5U

#define BLOCK_SIZE_BYTES                                        (uint32_t)(64 * 1024)
#define SECTOR_SIZE_BYTES                                       (uint32_t)(4 * 1024)
#define FLASH_SIZE_BYTES                                        (uint32_t)(256 * 1024 * 1024 / 8)
#define PAGE_SIZE_BYTES                                         (uint32_t)256

#define MX25UM25645G_BULK_ERASE_MAX_TIME                        150000U
#define MX25UM25645G_BLOCK_ERASE_MAX_TIME                       2000U
#define MX25UM25645G_BLOCK_4K_ERASE_MAX_TIME                    400U
#define MX25UM25645G_WRITE_REG_MAX_TIME                         40U
#define MX25UM25645G_RESET_MAX_TIME                             100U
#define MX25UM25645G_AUTOPOLLING_INTERVAL_TIME                  0x10U


// SPI Command Set -----------------------------------------------------------------------------------------------------

// READ/WRITE MEMORY Operations with 3-Byte Address
#define MX25UM25645G_READ_CMD                                   0x03U
#define MX25UM25645G_FAST_READ_CMD                              0x0BU
#define MX25UM25645G_PAGE_PROG_CMD                              0x02U
#define MX25UM25645G_SECTOR_ERASE_4K_CMD                        0x20U
#define MX25UM25645G_BLOCK_ERASE_64K_CMD                        0xD8U
#define MX25UM25645G_BULK_ERASE_CMD                             0x60U

// READ/WRITE MEMORY Operations with 4-Byte Address
#define MX25UM25645G_4_BYTE_ADDR_READ_CMD                       0x13U
#define MX25UM25645G_4_BYTE_ADDR_FAST_READ_CMD                  0x0CU
#define MX25UM25645G_4_BYTE_PAGE_PROG_CMD                       0x12U
#define MX25UM25645G_4_BYTE_SECTOR_ERASE_4K_CMD                 0x21U
#define MX25UM25645G_4_BYTE_BLOCK_ERASE_64K_CMD                 0xDCU

// Setting Commands
#define MX25UM25645G_WRITE_ENABLE_CMD                           0x06U
#define MX25UM25645G_WRITE_DISABLE_CMD                          0x04U
#define MX25UM25645G_PROG_ERASE_SUSPEND_CMD                     0xB0U
#define MX25UM25645G_PROG_ERASE_RESUME_CMD                      0x30U
#define MX25UM25645G_ENTER_DEEP_POWER_DOWN_CMD                  0xB9U
#define MX25UM25645G_RELEASE_DEEP_POWER_DOWN_CMD                0xABU
#define MX25UM25645G_SET_BURST_LENGTH_CMD                       0xC0U
#define MX25UM25645G_ENTER_SECURED_OTP_CMD                      0xB1U
#define MX25UM25645G_EXIT_SECURED_OTP_CMD                       0xC1U

// RESET Commands
#define MX25UM25645G_NOP_CMD                                    0x00U
#define MX25UM25645G_RESET_ENABLE_CMD                           0x66U
#define MX25UM25645G_RESET_MEMORY_CMD                           0x99U

// Register Commands (SPI)
#define MX25UM25645G_READ_ID_CMD                                0x9FU
#define MX25UM25645G_READ_SERIAL_FLASH_DISCO_PARAM_CMD          0x5AU
#define MX25UM25645G_READ_STATUS_REG_CMD                        0x05U
#define MX25UM25645G_READ_CFG_REG_CMD                           0x15U
#define MX25UM25645G_WRITE_STATUS_REG_CMD                       0x01U
#define MX25UM25645G_READ_CFG_REG2_CMD                          0x71U
#define MX25UM25645G_WRITE_CFG_REG2_CMD                         0x72U
#define MX25UM25645G_READ_FAST_BOOT_REG_CMD                     0x16U
#define MX25UM25645G_WRITE_FAST_BOOT_REG_CMD                    0x17U
#define MX25UM25645G_ERASE_FAST_BOOT_REG_CMD                    0x18U
#define MX25UM25645G_READ_SECURITY_REG_CMD                      0x2BU
#define MX25UM25645G_WRITE_SECURITY_REG_CMD                     0x2FU
#define MX25UM25645G_READ_LOCK_REG_CMD                          0x2DU
#define MX25UM25645G_WRITE_LOCK_REG_CMD                         0x2CU

#define MX25UM25645G_READ_DPB_REG_CMD                           0xE0U
#define MX25UM25645G_WRITE_DPB_REG_CMD                          0xE1U
#define MX25UM25645G_READ_SPB_STATUS_CMD                        0xE2U
#define MX25UM25645G_WRITE_SPB_BIT_CMD                          0xE3U
#define MX25UM25645G_ERASE_ALL_SPB_CMD                          0xE4U
#define MX25UM25645G_WRITE_PROTECT_SEL_CMD                      0x68U
#define MX25UM25645G_GANG_BLOCK_LOCK_CMD                        0x7EU
#define MX25UM25645G_GANG_BLOCK_UNLOCK_CMD                      0x98U
#define MX25UM25645G_READ_PASSWORD_REGISTER_CMD                 0x27U
#define MX25UM25645G_WRITE_PASSWORD_REGISTER_CMD                0x28U
#define MX25UM25645G_PASSWORD_UNLOCK_CMD                        0x29U


// OPI Command Set -----------------------------------------------------------------------------------------------------

// READ/WRITE MEMORY Operations
#define MX25UM25645G_OCTA_READ_CMD                              0xEC13U
#define MX25UM25645G_OCTA_READ_DTR_CMD                          0xEE11U
#define MX25UM25645G_OCTA_PAGE_PROG_CMD                         0x12EDU
#define MX25UM25645G_OCTA_SECTOR_ERASE_4K_CMD                   0x21DEU
#define MX25UM25645G_OCTA_BLOCK_ERASE_64K_CMD                   0xDC23U
#define MX25UM25645G_OCTA_BULK_ERASE_CMD                        0x609FU

// Setting Commands
#define MX25UM25645G_OCTA_WRITE_ENABLE_CMD                      0x06F9U
#define MX25UM25645G_OCTA_WRITE_DISABLE_CMD                     0x04FBU
#define MX25UM25645G_OCTA_PROG_ERASE_SUSPEND_CMD                0xB04FU
#define MX25UM25645G_OCTA_PROG_ERASE_RESUME_CMD                 0x30CFU
#define MX25UM25645G_OCTA_ENTER_DEEP_POWER_DOWN_CMD             0xB946U
#define MX25UM25645G_OCTA_RELEASE_DEEP_POWER_DOWN_CMD           0xAB54U
#define MX25UM25645G_OCTA_SET_BURST_LENGTH_CMD                  0xC03FU
#define MX25UM25645G_OCTA_ENTER_SECURED_OTP_CMD                 0xB14EU
#define MX25UM25645G_OCTA_EXIT_SECURED_OTP_CMD                  0xC13EU

// RESET Commands
#define MX25UM25645G_OCTA_NOP_CMD                               0x00FFU
#define MX25UM25645G_OCTA_RESET_ENABLE_CMD                      0x6699U
#define MX25UM25645G_OCTA_RESET_MEMORY_CMD                      0x9966U

// Register Commands (OPI)
#define MX25UM25645G_OCTA_READ_ID_CMD                           0x9F60U
#define MX25UM25645G_OCTA_READ_SERIAL_FLASH_DISCO_PARAM_CMD     0x5AA5U
#define MX25UM25645G_OCTA_READ_STATUS_REG_CMD                   0x05FAU
#define MX25UM25645G_OCTA_READ_CFG_REG_CMD                      0x15EAU
#define MX25UM25645G_OCTA_WRITE_STATUS_REG_CMD                  0x01FEU
#define MX25UM25645G_OCTA_READ_CFG_REG2_CMD                     0x718EU
#define MX25UM25645G_OCTA_WRITE_CFG_REG2_CMD                    0x728DU
#define MX25UM25645G_OCTA_READ_FAST_BOOT_REG_CMD                0x16E9U
#define MX25UM25645G_OCTA_WRITE_FAST_BOOT_REG_CMD               0x17E8U
#define MX25UM25645G_OCTA_ERASE_FAST_BOOT_REG_CMD               0x18E7U
#define MX25UM25645G_OCTA_READ_SECURITY_REG_CMD                 0x2BD4U
#define MX25UM25645G_OCTA_WRITE_SECURITY_REG_CMD                0x2FD0U
#define MX25UM25645G_OCTA_READ_LOCK_REG_CMD                     0x2DD2U
#define MX25UM25645G_OCTA_WRITE_LOCK_REG_CMD                    0x2CD3U
#define MX25UM25645G_OCTA_READ_DPB_REG_CMD                      0xE01FU
#define MX25UM25645G_OCTA_WRITE_DPB_REG_CMD                     0xE11EU
#define MX25UM25645G_OCTA_READ_SPB_STATUS_CMD                   0xE21DU
#define MX25UM25645G_OCTA_WRITE_SPB_BIT_CMD                     0xE31CU
#define MX25UM25645G_OCTA_ERASE_ALL_SPB_CMD                     0xE41BU
#define MX25UM25645G_OCTA_WRITE_PROTECT_SEL_CMD                 0x6897U
#define MX25UM25645G_OCTA_GANG_BLOCK_LOCK_CMD                   0x7E81U
#define MX25UM25645G_OCTA_GANG_BLOCK_UNLOCK_CMD                 0x9867U
#define MX25UM25645G_OCTA_READ_PASSWORD_REGISTER_CMD            0x27D8U
#define MX25UM25645G_OCTA_WRITE_PASSWORD_REGISTER_CMD           0x28D7U
#define MX25UM25645G_OCTA_PASSWORD_UNLOCK_CMD                   0x29D6U


// Flash Register Set --------------------------------------------------------------------------------------------------

// Status Register
#define MX25UM25645G_SR_WIP                                     0x01U
#define MX25UM25645G_SR_WEL                                     0x02U
#define MX25UM25645G_SR_PB                                      0x3CU

// Configuration Register 1
#define MX25UM25645G_CR1_ODS                                    0x07U
#define MX25UM25645G_CR1_TB                                     0x08U
#define MX25UM25645G_CR1_PBE                                    0x10U

// Configuration Register 2
// Address : 0x00000000
#define MX25UM25645G_CR2_REG1_ADDR                              0x00000000U
#define MX25UM25645G_CR2_SOPI                                   0x01U
#define MX25UM25645G_CR2_DOPI                                   0x02U
// Address : 0x00000200
#define MX25UM25645G_CR2_REG2_ADDR                              0x00000200U
#define MX25UM25645G_CR2_DQSPRC                                 0x01U
#define MX25UM25645G_CR2_DOS                                    0x02U
// Address : 0x00000300
#define MX25UM25645G_CR2_REG3_ADDR                              0x00000300U
#define MX25UM25645G_CR2_DC                                     0x07U
#define MX25UM25645G_CR2_DC_20_CYCLES                           0x00U
#define MX25UM25645G_CR2_DC_18_CYCLES                           0x01U
#define MX25UM25645G_CR2_DC_16_CYCLES                           0x02U
#define MX25UM25645G_CR2_DC_14_CYCLES                           0x03U
#define MX25UM25645G_CR2_DC_12_CYCLES                           0x04U
#define MX25UM25645G_CR2_DC_10_CYCLES                           0x05U
#define MX25UM25645G_CR2_DC_8_CYCLES                            0x06U
#define MX25UM25645G_CR2_DC_6_CYCLES                            0x07U
// Address : 0x00000500
#define MX25UM25645G_CR2_REG4_ADDR                              0x00000500U
#define MX25UM25645G_CR2_PPTSEL                                 0x01U
// Address : 0x40000000
#define MX25UM25645G_CR2_REG5_ADDR                              0x40000000U
#define MX25UM25645G_CR2_DEFSOPI                                0x01U
#define MX25UM25645G_CR2_DEFDOPI                                0x02U

// Security Register
#define MX25UM25645G_SECR_SOI                                   0x01U
#define MX25UM25645G_SECR_LDSO                                  0x02U
#define MX25UM25645G_SECR_PSB                                   0x04U
#define MX25UM25645G_SECR_ESB                                   0x08U
#define MX25UM25645G_SECR_P_FAIL                                0x20U
#define MX25UM25645G_SECR_E_FAIL                                0x40U
#define MX25UM25645G_SECR_WPSEL                                 0x80U

#define XSPI_ALTERNATE_BYTE_PATTERN                             0x00U


// Internal Flash Types ------------------------------------------------------------------------------------------------

typedef enum
{
   MX25UM25645G_SPI_MODE = 0,
   MX25UM25645G_OPI_MODE
} MX25UM25645G_Interface_t;

typedef enum
{
   MX25UM25645G_STR_TRANSFER = 0,
   MX25UM25645G_DTR_TRANSFER
} MX25UM25645G_Transfer_t;

typedef enum
{
   MX25UM25645G_ERASE_4K = 0,
   MX25UM25645G_ERASE_64K,
   MX25UM25645G_ERASE_BULK
} MX25UM25645G_Erase_t;

typedef enum
{
   MX25UM25645G_3BYTES_SIZE = 0,
   MX25UM25645G_4BYTES_SIZE
} MX25UM25645G_address_width_t;


// Private Helper Functions --------------------------------------------------------------------------------------------

extern void UsageFault_Handler(void);

static uint8_t auto_polling_mem_ready(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate)
{
   // Configure automatic polling mode to wait for memory ready
   const XSPI_RegularCmdTypeDef command = {
      .OperationType = HAL_XSPI_OPTYPE_COMMON_CFG,
      .InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES,
      .InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE,
      .InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS,
      .Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_READ_STATUS_REG_CMD : MX25UM25645G_OCTA_READ_STATUS_REG_CMD,
      .AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_NONE : HAL_XSPI_ADDRESS_8_LINES,
      .AddressDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_ADDRESS_DTR_ENABLE : HAL_XSPI_ADDRESS_DTR_DISABLE,
      .AddressWidth = HAL_XSPI_ADDRESS_32_BITS,
      .Address = 0U,
      .AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE,
      .DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES,
      .DataDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DATA_DTR_ENABLE : HAL_XSPI_DATA_DTR_DISABLE,
      .DummyCycles = (mode == MX25UM25645G_SPI_MODE) ? 0U : ((rate == MX25UM25645G_DTR_TRANSFER) ? DUMMY_CYCLES_REG_OCTAL_DTR : DUMMY_CYCLES_REG_OCTAL),
      .DataLength = (rate == MX25UM25645G_DTR_TRANSFER) ? 2U : 1U,
      .DQSMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DQS_ENABLE : HAL_XSPI_DQS_DISABLE
   };
   const XSPI_AutoPollingTypeDef config = {
      .MatchValue = 0U,
      .MatchMask = MX25UM25645G_SR_WIP,
      .MatchMode = HAL_XSPI_MATCH_MODE_AND,
      .IntervalTime = MX25UM25645G_AUTOPOLLING_INTERVAL_TIME,
      .AutomaticStop = HAL_XSPI_AUTOMATIC_STOP_ENABLE
   };
   return (HAL_XSPI_Command(ctx, &command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK) &&
          (HAL_XSPI_AutoPolling(ctx, &config, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK);
}

static uint8_t enable_memory_mapped_mode_dtr(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t)
{
   // Send a read command
   XSPI_RegularCmdTypeDef command = {
      .OperationType = HAL_XSPI_OPTYPE_READ_CFG,
      .InstructionMode = HAL_XSPI_INSTRUCTION_8_LINES,
      .InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_ENABLE,
      .InstructionWidth = HAL_XSPI_INSTRUCTION_16_BITS,
      .Instruction = MX25UM25645G_OCTA_READ_DTR_CMD,
      .AddressMode = HAL_XSPI_ADDRESS_8_LINES,
      .AddressDTRMode = HAL_XSPI_ADDRESS_DTR_ENABLE,
      .AddressWidth = HAL_XSPI_ADDRESS_32_BITS,
      .AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE,
      .DataMode = HAL_XSPI_DATA_8_LINES,
      .DataDTRMode = HAL_XSPI_DATA_DTR_ENABLE,
      .DummyCycles = DUMMY_CYCLES_READ_OCTAL_DTR,
      .DQSMode = HAL_XSPI_DQS_ENABLE
   };
   if (HAL_XSPI_Command(ctx, &command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
      return 0;

   // Send a program command
   command.OperationType = HAL_XSPI_OPTYPE_WRITE_CFG;
   command.Instruction = MX25UM25645G_OCTA_PAGE_PROG_CMD;
   command.DummyCycles = 0U;
   command.DQSMode = HAL_XSPI_DQS_DISABLE;
   if (HAL_XSPI_Command(ctx, &command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
      return 0;

   // Configure the flash for memory-mapped mode
   const XSPI_MemoryMappedTypeDef mem_mapped_cfg = { .TimeOutActivation = HAL_XSPI_TIMEOUT_COUNTER_DISABLE };
   return (HAL_XSPI_MemoryMapped(ctx, &mem_mapped_cfg) == HAL_OK);
}

static uint8_t write_enable(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate)
{
   // Send a write enable command
   XSPI_RegularCmdTypeDef command = {
      .OperationType = HAL_XSPI_OPTYPE_COMMON_CFG,
      .InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES,
      .InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE,
      .InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS,
      .Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_WRITE_ENABLE_CMD : MX25UM25645G_OCTA_WRITE_ENABLE_CMD,
      .AddressMode = HAL_XSPI_ADDRESS_NONE,
      .AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE,
      .DataMode = HAL_XSPI_DATA_NONE,
      .DummyCycles = 0U,
      .DQSMode = HAL_XSPI_DQS_DISABLE
   };
   if (HAL_XSPI_Command(ctx, &command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
      return 0;

   // Configure automatic polling mode to wait for write enabling
   command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_READ_STATUS_REG_CMD : MX25UM25645G_OCTA_READ_STATUS_REG_CMD;
   command.AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_NONE : HAL_XSPI_ADDRESS_8_LINES;
   command.AddressDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_ADDRESS_DTR_ENABLE : HAL_XSPI_ADDRESS_DTR_DISABLE;
   command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
   command.Address = 0U;
   command.DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES;
   command.DataDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DATA_DTR_ENABLE : HAL_XSPI_DATA_DTR_DISABLE;
   command.DummyCycles = (mode == MX25UM25645G_SPI_MODE) ? 0U : ((rate == MX25UM25645G_DTR_TRANSFER) ? DUMMY_CYCLES_REG_OCTAL_DTR : DUMMY_CYCLES_REG_OCTAL);
   command.DataLength = (rate == MX25UM25645G_DTR_TRANSFER) ? 2U : 1U;
   command.DQSMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DQS_ENABLE : HAL_XSPI_DQS_DISABLE;
   if (HAL_XSPI_Command(ctx, &command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
      return 0;
   const XSPI_AutoPollingTypeDef config = {
      .MatchValue = 2U,
      .MatchMask = 2U,
      .MatchMode = HAL_XSPI_MATCH_MODE_AND,
      .IntervalTime = MX25UM25645G_AUTOPOLLING_INTERVAL_TIME,
      .AutomaticStop = HAL_XSPI_AUTOMATIC_STOP_ENABLE
   };
   return (HAL_XSPI_AutoPolling(ctx, &config, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK);
}

static uint8_t read_cfg2_register(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate, uint32_t read_addr, uint8_t *value)
{
   // Read from configuration register 2
   const XSPI_RegularCmdTypeDef command = {
      .OperationType = HAL_XSPI_OPTYPE_COMMON_CFG,
      .InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES,
      .InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE,
      .InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS,
      .Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_READ_CFG_REG2_CMD : MX25UM25645G_OCTA_READ_CFG_REG2_CMD,
      .AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_1_LINE : HAL_XSPI_ADDRESS_8_LINES,
      .AddressDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_ADDRESS_DTR_ENABLE : HAL_XSPI_ADDRESS_DTR_DISABLE,
      .AddressWidth = HAL_XSPI_ADDRESS_32_BITS,
      .Address = read_addr,
      .AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE,
      .DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES,
      .DataDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DATA_DTR_ENABLE : HAL_XSPI_DATA_DTR_DISABLE,
      .DummyCycles = (mode == MX25UM25645G_SPI_MODE) ? 0U : ((rate == MX25UM25645G_DTR_TRANSFER) ? DUMMY_CYCLES_REG_OCTAL_DTR : DUMMY_CYCLES_REG_OCTAL),
      .DataLength = (rate == MX25UM25645G_DTR_TRANSFER) ? 2U : 1U,
      .DQSMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DQS_ENABLE : HAL_XSPI_DQS_DISABLE
   };
   return (HAL_XSPI_Command(ctx, &command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK) &&
          (HAL_XSPI_Receive(ctx, value, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK);
}

static uint8_t write_cfg2_register(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate, uint32_t write_addr, uint8_t value)
{
   // Write to configuration register 2
   const XSPI_RegularCmdTypeDef command = {
      .OperationType = HAL_XSPI_OPTYPE_COMMON_CFG,
      .InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES,
      .InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE,
      .InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS,
      .Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_WRITE_CFG_REG2_CMD : MX25UM25645G_OCTA_WRITE_CFG_REG2_CMD,
      .AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_1_LINE : HAL_XSPI_ADDRESS_8_LINES,
      .AddressDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_ADDRESS_DTR_ENABLE : HAL_XSPI_ADDRESS_DTR_DISABLE,
      .AddressWidth = HAL_XSPI_ADDRESS_32_BITS,
      .Address = write_addr,
      .AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE,
      .DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES,
      .DataDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DATA_DTR_ENABLE : HAL_XSPI_DATA_DTR_DISABLE,
      .DummyCycles = 0U,
      .DataLength = (mode == MX25UM25645G_SPI_MODE) ? 1U : ((rate == MX25UM25645G_DTR_TRANSFER) ? 2U : 1U),
      .DQSMode = HAL_XSPI_DQS_DISABLE
   };
   return (HAL_XSPI_Command(ctx, &command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK) &&
          (HAL_XSPI_Transmit(ctx, &value, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK);
}

static uint8_t reset_enable(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate)
{
   // Send a reset enable command
   const XSPI_RegularCmdTypeDef command = {
      .OperationType = HAL_XSPI_OPTYPE_COMMON_CFG,
      .IOSelect =  HAL_XSPI_SELECT_IO_3_0,
      .InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES,
      .InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE,
      .InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS,
      .Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_RESET_ENABLE_CMD : MX25UM25645G_OCTA_RESET_ENABLE_CMD,
      .AddressMode = HAL_XSPI_ADDRESS_NONE,
      .AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE,
      .DataMode = HAL_XSPI_DATA_NONE,
      .DummyCycles = 0U,
      .DQSMode = HAL_XSPI_DQS_DISABLE
   };
   return (HAL_XSPI_Command(ctx, &command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK);
}

static uint8_t reset_memory(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate)
{
   // Send a reset memory command
   const XSPI_RegularCmdTypeDef command = {
      .OperationType = HAL_XSPI_OPTYPE_COMMON_CFG,
      .InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES,
      .InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE,
      .InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS,
      .Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_RESET_MEMORY_CMD : MX25UM25645G_OCTA_RESET_MEMORY_CMD,
      .AddressMode = HAL_XSPI_ADDRESS_NONE,
      .AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE,
      .DataMode = HAL_XSPI_DATA_NONE,
      .DummyCycles = 0U,
      .DQSMode = HAL_XSPI_DQS_DISABLE
   };
   return (HAL_XSPI_Command(ctx, &command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK);
}


// Public API Functions ------------------------------------------------------------------------------------------------

void flash_init(void)
{
   // Enable the XSPIM, XSPI2, and GPIO clocks
   LL_RCC_SetXSPIClockSource(RCC_XSPI2CLKSOURCE_HCLK);
   WRITE_REG(RCC->AHB5ENSR, RCC_AHB5ENR_XSPIMEN);
   (void)READ_BIT(RCC->AHB5ENR, RCC_AHB5ENR_XSPIMEN);
   WRITE_REG(RCC->AHB5ENSR, RCC_AHB5ENR_XSPI2EN);
   (void)READ_BIT(RCC->AHB5ENR, RCC_AHB5ENR_XSPI2EN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIONEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIONEN);

   // Ensure all other XSPI clocks are disabled
   WRITE_REG(RCC->AHB5ENCR, RCC_AHB5ENR_XSPI1EN);
   WRITE_REG(RCC->AHB5ENCR, RCC_AHB5ENR_XSPI3EN);

   // Initialize the XSPI GPIO pins
   const uint16_t xspi_pins[] = XSPI_PINS;
   for (uint32_t i = 0; i < (sizeof(xspi_pins) / sizeof(xspi_pins[0])); ++i)
   {
      const uint32_t position = 32 - __builtin_clz(xspi_pins[i]) - 1;
      MODIFY_REG(XSPI_PORT->OSPEEDR, (GPIO_OSPEEDR_OSPEED0 << (position * 2U)), (GPIO_SPEED_FREQ_HIGH << (position * 2U)));
      MODIFY_REG(XSPI_PORT->OTYPER, (GPIO_OTYPER_OT0 << position), (((GPIO_MODE_AF_PP & OUTPUT_TYPE) >> OUTPUT_TYPE_Pos) << position));
      MODIFY_REG(XSPI_PORT->PUPDR, (GPIO_PUPDR_PUPD0 << (position * 2U)), (GPIO_NOPULL << (position * 2U)));
      MODIFY_REG(XSPI_PORT->AFR[position >> 3U], (0xFU << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)), (GPIO_AF9_XSPIM_P2 << ((position & 0x07U) * GPIO_AFRL_AFSEL1_Pos)));
      MODIFY_REG(XSPI_PORT->MODER, (GPIO_MODER_MODE0 << (position * 2U)), ((GPIO_MODE_AF_PP & GPIO_MODE) << (position * 2U)));
   }

   // Configure the memory type, device size, chip-select high time, clock mode, and FIFO threshold
   MODIFY_REG(XSPI2->DCR1, (XSPI_DCR1_MTYP | XSPI_DCR1_DEVSIZE | XSPI_DCR1_CSHT | XSPI_DCR1_FRCK | XSPI_DCR1_CKMODE), (HAL_XSPI_MEMTYPE_MACRONIX | (HAL_XSPI_SIZE_256MB << XSPI_DCR1_DEVSIZE_Pos) | ((2U - 1U) << XSPI_DCR1_CSHT_Pos) | HAL_XSPI_CLOCK_MODE_0));
   CLEAR_BIT(XSPI2->DCR2, XSPI_DCR2_WRAPSIZE);
   CLEAR_BIT(XSPI2->DCR3, (XSPI_DCR3_CSBOUND | XSPI_DCR3_MAXTRAN));
   CLEAR_BIT(XSPI2->DCR4, XSPI_DCR4_REFRESH);
   MODIFY_REG(XSPI2->CR, XSPI_CR_FTHRES, ((4U - 1U) << XSPI_CR_FTHRES_Pos));

   // Wait until the XSPI2 peripheral is ready
   while (READ_BIT(XSPI2->SR, HAL_XSPI_FLAG_BUSY));

   // Configure the clock prescaler to generate 50MHz and wait until calibration is complete
   MODIFY_REG(XSPI2->DCR2, XSPI_DCR2_PRESCALER, (0x03 << XSPI_DCR2_PRESCALER_Pos));
   while (READ_BIT(XSPI2->SR, HAL_XSPI_FLAG_BUSY));

   // Configure dual-memory mode, CS selection, and sample shifting
   CLEAR_BIT(XSPI2->CR, (XSPI_CR_DMM | XSPI_CR_CSSEL));
   CLEAR_BIT(XSPI2->TCR, (XSPI_TCR_SSHIFT));

   // Deactivate all XSPIM configurations
   WRITE_REG(XSPIM->CR, 0x0);

   // Enable the XSPI2 peripheral
   SET_BIT(XSPI2->CR, XSPI_CR_EN);

   // Disable the XSPIM and GPIO configuration clocks
   WRITE_REG(RCC->AHB5ENCR, RCC_AHB5ENR_XSPIMEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIONEN);

   // Use HAL just for configuring the flash
   XSPI_HandleTypeDef hxspi2 = {
     .Instance = XSPI2,
     .Init = {
       .FifoThresholdByte = 4,
       .MemoryMode = HAL_XSPI_SINGLE_MEM,
       .MemoryType = HAL_XSPI_MEMTYPE_MACRONIX,
       .MemorySize = HAL_XSPI_SIZE_256MB,
       .ChipSelectHighTimeCycle = 2,
       .FreeRunningClock = HAL_XSPI_FREERUNCLK_DISABLE,
       .ClockMode = HAL_XSPI_CLOCK_MODE_0,
       .WrapSize = HAL_XSPI_WRAP_NOT_SUPPORTED,
       .ClockPrescaler = 0x03,
       .SampleShifting = HAL_XSPI_SAMPLE_SHIFT_NONE,
       .DelayHoldQuarterCycle = HAL_XSPI_DHQC_ENABLE,
       .ChipSelectBoundary = HAL_XSPI_BONDARYOF_NONE,
       .MaxTran = 0,
       .Refresh = 0,
       .MemorySelect = HAL_XSPI_CSSEL_NCS1
     },
     .State = HAL_XSPI_STATE_READY
   };

   // Reset the flash memory to its default configuration (STR SPI mode)
   if (!reset_enable(&hxspi2, MX25UM25645G_SPI_MODE, MX25UM25645G_STR_TRANSFER) || !reset_memory(&hxspi2, MX25UM25645G_SPI_MODE, MX25UM25645G_STR_TRANSFER) ||
       !reset_enable(&hxspi2, MX25UM25645G_OPI_MODE, MX25UM25645G_STR_TRANSFER) || !reset_memory(&hxspi2, MX25UM25645G_OPI_MODE, MX25UM25645G_STR_TRANSFER) ||
       !reset_enable(&hxspi2, MX25UM25645G_OPI_MODE, MX25UM25645G_DTR_TRANSFER) || !reset_memory(&hxspi2, MX25UM25645G_OPI_MODE, MX25UM25645G_DTR_TRANSFER))
      UsageFault_Handler();

   // Wait until the memory becomes available again
   uint32_t tick_start = system_get_tick();
   while ((system_get_tick() - tick_start) < 101U);
   if (!auto_polling_mem_ready(&hxspi2, MX25UM25645G_SPI_MODE, MX25UM25645G_STR_TRANSFER))
      UsageFault_Handler();

   // Configure the flash memory to operate in DTR OPI mode
   if (!write_enable(&hxspi2, MX25UM25645G_SPI_MODE, MX25UM25645G_STR_TRANSFER) ||
       !write_cfg2_register(&hxspi2, MX25UM25645G_SPI_MODE, MX25UM25645G_STR_TRANSFER, MX25UM25645G_CR2_REG3_ADDR, MX25UM25645G_CR2_DC_20_CYCLES) ||
       !write_enable(&hxspi2, MX25UM25645G_SPI_MODE, MX25UM25645G_STR_TRANSFER) ||
       !write_cfg2_register(&hxspi2, MX25UM25645G_SPI_MODE, MX25UM25645G_STR_TRANSFER, MX25UM25645G_CR2_REG1_ADDR, MX25UM25645G_CR2_DOPI))
      UsageFault_Handler();

   // Wait for the configuration to take effect then re-initialize the peripheral to operate at 200MHz
   tick_start = system_get_tick();
   while ((system_get_tick() - tick_start) < 41U);
   MODIFY_REG(XSPI2->DCR1, (XSPI_DCR1_MTYP | XSPI_DCR1_DEVSIZE | XSPI_DCR1_CSHT | XSPI_DCR1_FRCK | XSPI_DCR1_CKMODE), (HAL_XSPI_MEMTYPE_MACRONIX | (HAL_XSPI_SIZE_256MB << XSPI_DCR1_DEVSIZE_Pos) | ((2U - 1U) << XSPI_DCR1_CSHT_Pos) | HAL_XSPI_CLOCK_MODE_0));
   CLEAR_BIT(XSPI2->DCR2, XSPI_DCR2_WRAPSIZE);
   CLEAR_BIT(XSPI2->DCR3, (XSPI_DCR3_CSBOUND | XSPI_DCR3_MAXTRAN));
   CLEAR_BIT(XSPI2->DCR4, XSPI_DCR4_REFRESH);
   while (READ_BIT(XSPI2->SR, HAL_XSPI_FLAG_BUSY));
   CLEAR_BIT(XSPI2->DCR2, XSPI_DCR2_PRESCALER);
   while (READ_BIT(XSPI2->SR, HAL_XSPI_FLAG_BUSY));
   CLEAR_BIT(XSPI2->CR, (XSPI_CR_DMM | XSPI_CR_CSSEL));
   CLEAR_BIT(XSPI2->TCR, (XSPI_TCR_SSHIFT));
   SET_BIT(XSPI2->CR, XSPI_CR_EN);

   // Wait until the memory becomes available again
   uint8_t reg[2];
   if (!auto_polling_mem_ready(&hxspi2, MX25UM25645G_OPI_MODE, MX25UM25645G_DTR_TRANSFER) ||
       !read_cfg2_register(&hxspi2, MX25UM25645G_OPI_MODE, MX25UM25645G_DTR_TRANSFER, MX25UM25645G_CR2_REG1_ADDR, reg) ||
       (reg[0] != MX25UM25645G_CR2_DOPI))
      UsageFault_Handler();

   // Map the flash memory to internal address 0x70000000
   if (!enable_memory_mapped_mode_dtr(&hxspi2, MX25UM25645G_OPI_MODE))
      UsageFault_Handler();

   // Disable data prefetching (due to errata)
   MODIFY_REG(XSPI2->CR, XSPI_CR_NOPREF, HAL_XSPI_AUTOMATIC_PREFETCH_DISABLE);
}
