// Header Inclusions ---------------------------------------------------------------------------------------------------

#include "flash.h"


// Flash Memory Characteristics ----------------------------------------------------------------------------------------

#define DUMMY_CYCLES_READ                                       8U
#define DUMMY_CYCLES_READ_OCTAL                                 6U
#define DUMMY_CYCLES_READ_OCTAL_DTR                             6U
#define DUMMY_CYCLES_REG_OCTAL                                  4U
#define DUMMY_CYCLES_REG_OCTAL_DTR                              5U

#define BLOCK_size_BYTES                                        (uint32_t)(64 * 1024)
#define SECTOR_size_BYTES                                       (uint32_t)(4 * 1024)
#define FLASH_size_BYTES                                        (uint32_t)(256*1024*1024/8)
#define PAGE_size_BYTES                                         (uint32_t)256

#define MX25UM25645G_BULK_ERASE_MAX_TIME                        150000U
#define MX25UM25645G_BLOCK_ERASE_MAX_TIME                       2000U
#define MX25UM25645G_BLOCK_4K_ERASE_MAX_TIME                    400U
#define MX25UM25645G_WRITE_REG_MAX_TIME                         40U
#define MX25UM25645G_RESET_MAX_TIME                             100U
#define MX25UM25645G_AUTOPOLLING_INTERVAL_TIME                  0x10U

#define MX25UM25645G_OK                                         (0)
#define MX25UM25645G_ERROR                                      (-1)


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

static int32_t MX25UM25645G_AutoPollingMemReady(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate)
{
   // SPI mode and DTR transfer not supported by memory
   if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
      return MX25UM25645G_ERROR;

   // Configure automatic polling mode to wait for memory ready
   XSPI_RegularCmdTypeDef s_command = {
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

   XSPI_AutoPollingTypeDef s_config = {
      .MatchValue = 0U,
      .MatchMask = MX25UM25645G_SR_WIP,
      .MatchMode = HAL_XSPI_MATCH_MODE_AND,
      .IntervalTime = MX25UM25645G_AUTOPOLLING_INTERVAL_TIME,
      .AutomaticStop = HAL_XSPI_AUTOMATIC_STOP_ENABLE
   };

   if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
      return MX25UM25645G_ERROR;
   if (HAL_XSPI_AutoPolling(ctx, &s_config, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
      return MX25UM25645G_ERROR;
   return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_EnableMemoryMappedModeDTR(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode)
{
  // Initialize the read command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_READ_CFG;
  s_command.InstructionMode = HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_ENABLE;
  s_command.InstructionWidth = HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = MX25UM25645G_OCTA_READ_DTR_CMD;
  s_command.AddressMode = HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = HAL_XSPI_ADDRESS_DTR_ENABLE;
  s_command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = HAL_XSPI_DATA_8_LINES;
  s_command.DataDTRMode = HAL_XSPI_DATA_DTR_ENABLE;
  s_command.DummyCycles = DUMMY_CYCLES_READ_OCTAL_DTR;
  s_command.DQSMode = HAL_XSPI_DQS_ENABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;

  // Initialize the program command
  s_command.OperationType = HAL_XSPI_OPTYPE_WRITE_CFG;
  s_command.Instruction = MX25UM25645G_OCTA_PAGE_PROG_CMD;
  s_command.DummyCycles = 0U;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;

  // Configure the memory mapped mode
  XSPI_MemoryMappedTypeDef s_mem_mapped_cfg = { 0 };
  s_mem_mapped_cfg.TimeOutActivation = HAL_XSPI_TIMEOUT_COUNTER_DISABLE;
  if (HAL_XSPI_MemoryMapped(ctx, &s_mem_mapped_cfg) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_WriteEnable(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the write enable command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_WRITE_ENABLE_CMD : MX25UM25645G_OCTA_WRITE_ENABLE_CMD;
  s_command.AddressMode = HAL_XSPI_ADDRESS_NONE;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = HAL_XSPI_DATA_NONE;
  s_command.DummyCycles = 0U;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;

  // Configure automatic polling mode to wait for write enabling
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_READ_STATUS_REG_CMD : MX25UM25645G_OCTA_READ_STATUS_REG_CMD;
  s_command.AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_NONE : HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_ADDRESS_DTR_ENABLE : HAL_XSPI_ADDRESS_DTR_DISABLE;
  s_command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
  s_command.Address = 0U;
  s_command.DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES;
  s_command.DataDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DATA_DTR_ENABLE : HAL_XSPI_DATA_DTR_DISABLE;
  s_command.DummyCycles = (mode == MX25UM25645G_SPI_MODE) ? 0U : ((rate == MX25UM25645G_DTR_TRANSFER) ? DUMMY_CYCLES_REG_OCTAL_DTR : DUMMY_CYCLES_REG_OCTAL);
  s_command.DataLength = (rate == MX25UM25645G_DTR_TRANSFER) ? 2U : 1U;
  s_command.DQSMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DQS_ENABLE : HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;

  XSPI_AutoPollingTypeDef s_config = { 0 };
  s_config.MatchValue = 2U;
  s_config.MatchMask = 2U;
  s_config.MatchMode = HAL_XSPI_MATCH_MODE_AND;
  s_config.IntervalTime = MX25UM25645G_AUTOPOLLING_INTERVAL_TIME;
  s_config.AutomaticStop = HAL_XSPI_AUTOMATIC_STOP_ENABLE;
  if (HAL_XSPI_AutoPolling(ctx, &s_config, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_ReadCfg2Register(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate, uint32_t read_addr, uint8_t *value)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the reading of status register
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_READ_CFG_REG2_CMD : MX25UM25645G_OCTA_READ_CFG_REG2_CMD;
  s_command.AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_1_LINE : HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_ADDRESS_DTR_ENABLE : HAL_XSPI_ADDRESS_DTR_DISABLE;
  s_command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
  s_command.Address = read_addr;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES;
  s_command.DataDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DATA_DTR_ENABLE : HAL_XSPI_DATA_DTR_DISABLE;
  s_command.DummyCycles = (mode == MX25UM25645G_SPI_MODE) ? 0U : ((rate == MX25UM25645G_DTR_TRANSFER) ? DUMMY_CYCLES_REG_OCTAL_DTR : DUMMY_CYCLES_REG_OCTAL);
  s_command.DataLength = (rate == MX25UM25645G_DTR_TRANSFER) ? 2U : 1U;
  s_command.DQSMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DQS_ENABLE : HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;

  // Reception of the data
  if (HAL_XSPI_Receive(ctx, value, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_WriteCfg2Register(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate, uint32_t write_addr, uint8_t value)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the writing of configuration register 2
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_WRITE_CFG_REG2_CMD : MX25UM25645G_OCTA_WRITE_CFG_REG2_CMD;
  s_command.AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_1_LINE : HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_ADDRESS_DTR_ENABLE : HAL_XSPI_ADDRESS_DTR_DISABLE;
  s_command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
  s_command.Address = write_addr;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES;
  s_command.DataDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DATA_DTR_ENABLE : HAL_XSPI_DATA_DTR_DISABLE;
  s_command.DummyCycles = 0U;
  s_command.DataLength = (mode == MX25UM25645G_SPI_MODE) ? 1U : ((rate == MX25UM25645G_DTR_TRANSFER) ? 2U : 1U);
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  if (HAL_XSPI_Transmit(ctx, &value, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_ResetEnable(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the reset enable command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.IOSelect =  HAL_XSPI_SELECT_IO_3_0;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_RESET_ENABLE_CMD : MX25UM25645G_OCTA_RESET_ENABLE_CMD;
  s_command.AddressMode = HAL_XSPI_ADDRESS_NONE;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = HAL_XSPI_DATA_NONE;
  s_command.DummyCycles = 0U;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_ResetMemory(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the reset enable command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_RESET_MEMORY_CMD : MX25UM25645G_OCTA_RESET_MEMORY_CMD;
  s_command.AddressMode = HAL_XSPI_ADDRESS_NONE;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = HAL_XSPI_DATA_NONE;
  s_command.DummyCycles = 0U;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

#ifdef FULL_FLASH_DRIVER

static int32_t MX25UM25645G_ReadStatusRegister(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate, uint8_t *value)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the reading of status register
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_READ_STATUS_REG_CMD : MX25UM25645G_OCTA_READ_STATUS_REG_CMD;
  s_command.AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_NONE : HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_ADDRESS_DTR_ENABLE : HAL_XSPI_ADDRESS_DTR_DISABLE;
  s_command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
  s_command.Address = 0U;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES;
  s_command.DataDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DATA_DTR_ENABLE : HAL_XSPI_DATA_DTR_DISABLE;
  s_command.DummyCycles = (mode == MX25UM25645G_SPI_MODE) ? 0U : ((rate == MX25UM25645G_DTR_TRANSFER) ? DUMMY_CYCLES_REG_OCTAL_DTR : DUMMY_CYCLES_REG_OCTAL);
  s_command.DataLength = (rate == MX25UM25645G_DTR_TRANSFER) ? 2U : 1U;
  s_command.DQSMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DQS_ENABLE : HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;

  // Reception of the data
  if (HAL_XSPI_Receive(ctx, value, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_ReadCfgRegister(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate, uint8_t *value)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the reading of configuration register
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_READ_CFG_REG_CMD : MX25UM25645G_OCTA_READ_CFG_REG_CMD;
  s_command.AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_NONE : HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_ADDRESS_DTR_ENABLE : HAL_XSPI_ADDRESS_DTR_DISABLE;
  s_command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
  s_command.Address = 1U;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES;
  s_command.DataDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DATA_DTR_ENABLE : HAL_XSPI_DATA_DTR_DISABLE;
  s_command.DummyCycles = (mode == MX25UM25645G_SPI_MODE) ? 0U : ((rate == MX25UM25645G_DTR_TRANSFER) ? DUMMY_CYCLES_REG_OCTAL_DTR : DUMMY_CYCLES_REG_OCTAL);
  s_command.DataLength = (rate == MX25UM25645G_DTR_TRANSFER) ? 2U : 1U;
  s_command.DQSMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DQS_ENABLE : HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;

  // Reception of the data
  if (HAL_XSPI_Receive(ctx, value, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_ReadSTR(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_address_width_t address_width, uint8_t *data, uint32_t read_addr, uint32_t size)
{
  // OPI mode and 3-bytes address size not supported by memory
  if ((mode == MX25UM25645G_OPI_MODE) && (address_width == MX25UM25645G_3BYTES_SIZE))
    return MX25UM25645G_ERROR;

  // Initialize the read command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? ((address_width == MX25UM25645G_3BYTES_SIZE) ? MX25UM25645G_FAST_READ_CMD : MX25UM25645G_4_BYTE_ADDR_FAST_READ_CMD) : MX25UM25645G_OCTA_READ_CMD;
  s_command.AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_1_LINE : HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = HAL_XSPI_ADDRESS_DTR_DISABLE;
  s_command.AddressWidth = (address_width == MX25UM25645G_3BYTES_SIZE) ? HAL_XSPI_ADDRESS_24_BITS : HAL_XSPI_ADDRESS_32_BITS;
  s_command.Address = read_addr;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES;
  s_command.DataDTRMode = HAL_XSPI_DATA_DTR_DISABLE;
  s_command.DummyCycles = (mode == MX25UM25645G_SPI_MODE) ? DUMMY_CYCLES_READ : DUMMY_CYCLES_READ_OCTAL;
  s_command.DataLength = size;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;

  // Reception of the data
  if (HAL_XSPI_Receive(ctx, data, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_ReadDTR(XSPI_HandleTypeDef *ctx, uint8_t *data, uint32_t read_addr, uint32_t size)
{
  // Initialize the read command
   XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_ENABLE;
  s_command.InstructionWidth = HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = MX25UM25645G_OCTA_READ_DTR_CMD;
  s_command.AddressMode = HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = HAL_XSPI_ADDRESS_DTR_ENABLE;
  s_command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
  s_command.Address = read_addr;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = HAL_XSPI_DATA_8_LINES;
  s_command.DataDTRMode = HAL_XSPI_DATA_DTR_ENABLE;
  s_command.DummyCycles = DUMMY_CYCLES_READ_OCTAL_DTR;
  s_command.DataLength = size;
  s_command.DQSMode = HAL_XSPI_DQS_ENABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;

  // Reception of the data
  if (HAL_XSPI_Receive(ctx, data, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_PageProgram(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_address_width_t address_width, uint8_t *data, uint32_t write_addr, uint32_t size)
{
  // OPI mode and 3-bytes address size not supported by memory
  if ((mode == MX25UM25645G_OPI_MODE) && (address_width == MX25UM25645G_3BYTES_SIZE))
    return MX25UM25645G_ERROR;

  // Initialize the program command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? ((address_width == MX25UM25645G_3BYTES_SIZE) ? MX25UM25645G_PAGE_PROG_CMD : MX25UM25645G_4_BYTE_PAGE_PROG_CMD) : MX25UM25645G_OCTA_PAGE_PROG_CMD;
  s_command.AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_1_LINE : HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = HAL_XSPI_ADDRESS_DTR_DISABLE;
  s_command.AddressWidth = (address_width == MX25UM25645G_3BYTES_SIZE) ? HAL_XSPI_ADDRESS_24_BITS : HAL_XSPI_ADDRESS_32_BITS;
  s_command.Address = write_addr;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES;
  s_command.DataDTRMode = HAL_XSPI_DATA_DTR_DISABLE;
  s_command.DummyCycles = 0U;
  s_command.DataLength = size;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Configure the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;

  // Transmission of the data
  if (HAL_XSPI_Transmit(ctx, data, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_PageProgramDTR(XSPI_HandleTypeDef *ctx, uint8_t *data, uint32_t write_addr, uint32_t size)
{
  // Initialize the program command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_ENABLE;
  s_command.InstructionWidth = HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = MX25UM25645G_OCTA_PAGE_PROG_CMD;
  s_command.AddressMode = HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = HAL_XSPI_ADDRESS_DTR_ENABLE;
  s_command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
  s_command.Address = write_addr;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = HAL_XSPI_DATA_8_LINES;
  s_command.DataDTRMode = HAL_XSPI_DATA_DTR_ENABLE;
  s_command.DummyCycles = 0U;
  s_command.DataLength = size;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Configure the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;

  // Transmission of the data
  if (HAL_XSPI_Transmit(ctx, data, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_BlockErase(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate, MX25UM25645G_address_width_t address_width, uint32_t block_address, MX25UM25645G_Erase_t block_size)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the erase command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.IOSelect = HAL_XSPI_SELECT_IO_7_0;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_1_LINE : HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_ADDRESS_DTR_ENABLE : HAL_XSPI_ADDRESS_DTR_DISABLE;
  s_command.AddressWidth = (address_width == MX25UM25645G_3BYTES_SIZE) ? HAL_XSPI_ADDRESS_24_BITS : HAL_XSPI_ADDRESS_32_BITS;
  s_command.Address = block_address;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = HAL_XSPI_DATA_NONE;
  s_command.DummyCycles = 0U;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  switch (mode)
  {
    case MX25UM25645G_OPI_MODE:
      s_command.Instruction = (block_size == MX25UM25645G_ERASE_64K) ? MX25UM25645G_OCTA_BLOCK_ERASE_64K_CMD : MX25UM25645G_OCTA_SECTOR_ERASE_4K_CMD;
      break;
    case MX25UM25645G_SPI_MODE:
    default:
      if (block_size == MX25UM25645G_ERASE_64K)
        s_command.Instruction = (address_width == MX25UM25645G_3BYTES_SIZE) ? MX25UM25645G_BLOCK_ERASE_64K_CMD : MX25UM25645G_4_BYTE_BLOCK_ERASE_64K_CMD;
      else
        s_command.Instruction = (address_width == MX25UM25645G_3BYTES_SIZE) ? MX25UM25645G_SECTOR_ERASE_4K_CMD : MX25UM25645G_4_BYTE_SECTOR_ERASE_4K_CMD;
      break;
  }

  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_ChipErase(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the erase command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_BULK_ERASE_CMD : MX25UM25645G_OCTA_BULK_ERASE_CMD;
  s_command.AddressMode = HAL_XSPI_ADDRESS_NONE;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = HAL_XSPI_DATA_NONE;
  s_command.DummyCycles = 0U;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_EnableMemoryMappedModeSTR(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_address_width_t address_width)
{
  // OPI mode and 3-bytes address size not supported by memory
  if ((mode == MX25UM25645G_OPI_MODE) && (address_width == MX25UM25645G_3BYTES_SIZE))
    return MX25UM25645G_ERROR;

  // Initialize the read command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_READ_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? ((address_width == MX25UM25645G_3BYTES_SIZE) ? MX25UM25645G_FAST_READ_CMD : MX25UM25645G_4_BYTE_ADDR_FAST_READ_CMD) : MX25UM25645G_OCTA_READ_CMD;
  s_command.AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_1_LINE : HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = HAL_XSPI_ADDRESS_DTR_DISABLE;
  s_command.AddressWidth = (address_width == MX25UM25645G_3BYTES_SIZE) ? HAL_XSPI_ADDRESS_24_BITS : HAL_XSPI_ADDRESS_32_BITS;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES;
  s_command.DataDTRMode = HAL_XSPI_DATA_DTR_DISABLE;
  s_command.DummyCycles = (mode == MX25UM25645G_SPI_MODE) ? DUMMY_CYCLES_READ : DUMMY_CYCLES_READ_OCTAL;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the read command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;

  // Initialize the program command
  s_command.OperationType = HAL_XSPI_OPTYPE_WRITE_CFG;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? ((address_width == MX25UM25645G_3BYTES_SIZE) ? MX25UM25645G_PAGE_PROG_CMD : MX25UM25645G_4_BYTE_PAGE_PROG_CMD) : MX25UM25645G_OCTA_PAGE_PROG_CMD;
  s_command.DummyCycles = 0U;

  // Send the write command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;

  // Configure the memory mapped mode
  XSPI_MemoryMappedTypeDef s_mem_mapped_cfg = { 0 };
  s_mem_mapped_cfg.TimeOutActivation = HAL_XSPI_TIMEOUT_COUNTER_DISABLE;
  if (HAL_XSPI_MemoryMapped(ctx, &s_mem_mapped_cfg) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_Suspend(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the suspend command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_PROG_ERASE_SUSPEND_CMD : MX25UM25645G_OCTA_PROG_ERASE_SUSPEND_CMD;
  s_command.AddressMode = HAL_XSPI_ADDRESS_NONE;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = HAL_XSPI_DATA_NONE;
  s_command.DummyCycles = 0U;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_Resume(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the resume command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_PROG_ERASE_RESUME_CMD : MX25UM25645G_OCTA_PROG_ERASE_RESUME_CMD;
  s_command.AddressMode = HAL_XSPI_ADDRESS_NONE;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = HAL_XSPI_DATA_NONE;
  s_command.DummyCycles = 0U;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_WriteDisable(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the write disable command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_WRITE_DISABLE_CMD : MX25UM25645G_OCTA_WRITE_DISABLE_CMD;
  s_command.AddressMode = HAL_XSPI_ADDRESS_NONE;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = HAL_XSPI_DATA_NONE;
  s_command.DummyCycles = 0U;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command */
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_ReadSecurityRegister(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate, uint8_t *value)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the reading of security register
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_READ_SECURITY_REG_CMD : MX25UM25645G_OCTA_READ_SECURITY_REG_CMD;
  s_command.AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_NONE : HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_ADDRESS_DTR_ENABLE : HAL_XSPI_ADDRESS_DTR_DISABLE;
  s_command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
  s_command.Address = 0U;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES;
  s_command.DataDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DATA_DTR_ENABLE : HAL_XSPI_DATA_DTR_DISABLE;
  s_command.DummyCycles = (mode == MX25UM25645G_SPI_MODE) ? 0U : ((rate == MX25UM25645G_DTR_TRANSFER) ? DUMMY_CYCLES_REG_OCTAL_DTR : DUMMY_CYCLES_REG_OCTAL);
  s_command.DataLength = (rate == MX25UM25645G_DTR_TRANSFER) ? 2U : 1U;
  s_command.DQSMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DQS_ENABLE : HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;

  // Reception of the data
  if (HAL_XSPI_Receive(ctx, value, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_Readid(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate, uint8_t *id)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the read id command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_READ_ID_CMD : MX25UM25645G_OCTA_READ_ID_CMD;
  s_command.AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_NONE : HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_ADDRESS_DTR_ENABLE : HAL_XSPI_ADDRESS_DTR_DISABLE;
  s_command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
  s_command.Address = 0U;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES;
  s_command.DataDTRMode = HAL_XSPI_DATA_DTR_DISABLE;
  s_command.DummyCycles = (mode == MX25UM25645G_SPI_MODE) ? 0U : DUMMY_CYCLES_REG_OCTAL;
  s_command.DataLength = 3U;
  s_command.DQSMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DQS_ENABLE : HAL_XSPI_DQS_DISABLE;

  // Configure the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;

  // Reception of the data
  if (HAL_XSPI_Receive(ctx, id, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_WriteStatusRegister(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate, uint8_t value)
{
  uint8_t reg[2];

  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // In SPI mode, the status register is configured with configuration register
  if (mode == MX25UM25645G_SPI_MODE)
  {
    if (MX25UM25645G_ReadCfgRegister(ctx, mode, rate, &reg[1]) != MX25UM25645G_OK)
      return MX25UM25645G_ERROR;
  }
  reg[0] = value;

  // Initialize the writing of status register
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_WRITE_STATUS_REG_CMD : MX25UM25645G_OCTA_WRITE_STATUS_REG_CMD;
  s_command.AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_NONE : HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_ADDRESS_DTR_ENABLE : HAL_XSPI_ADDRESS_DTR_DISABLE;
  s_command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
  s_command.Address = 0U;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES;
  s_command.DataDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DATA_DTR_ENABLE : HAL_XSPI_DATA_DTR_DISABLE;
  s_command.DummyCycles = 0U;
  s_command.DataLength = (mode == MX25UM25645G_SPI_MODE) ? 2U : ((rate == MX25UM25645G_DTR_TRANSFER) ? 2U : 1U);
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  if (HAL_XSPI_Transmit(ctx, reg, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_WriteCfgRegister(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate, uint8_t value)
{
  uint8_t reg[2];

  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // In SPI mode, the configuration register is configured with status register
  if (mode == MX25UM25645G_SPI_MODE)
  {
    if (MX25UM25645G_ReadStatusRegister(ctx, mode, rate, &reg[0]) != MX25UM25645G_OK)
      return MX25UM25645G_ERROR;
    reg[1] = value;
  }
  else
    reg[0] = value;

  // Initialize the writing of configuration register
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_WRITE_STATUS_REG_CMD : MX25UM25645G_OCTA_WRITE_STATUS_REG_CMD;
  s_command.AddressMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_ADDRESS_NONE : HAL_XSPI_ADDRESS_8_LINES;
  s_command.AddressDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_ADDRESS_DTR_ENABLE : HAL_XSPI_ADDRESS_DTR_DISABLE;
  s_command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
  s_command.Address = 1U;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_DATA_1_LINE : HAL_XSPI_DATA_8_LINES;
  s_command.DataDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_DATA_DTR_ENABLE : HAL_XSPI_DATA_DTR_DISABLE;
  s_command.DummyCycles = 0U;
  s_command.DataLength = (mode == MX25UM25645G_SPI_MODE) ? 2U : ((rate == MX25UM25645G_DTR_TRANSFER) ? 2U : 1U);
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  if (HAL_XSPI_Transmit(ctx, reg, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_WriteSecurityRegister(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate, uint8_t value)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the write of security register
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_WRITE_SECURITY_REG_CMD : MX25UM25645G_OCTA_WRITE_SECURITY_REG_CMD;
  s_command.AddressMode = HAL_XSPI_ADDRESS_NONE;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = HAL_XSPI_DATA_NONE;
  s_command.DummyCycles = 0U;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_NoOperation(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the no operation command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_NOP_CMD : MX25UM25645G_OCTA_NOP_CMD;
  s_command.AddressMode = HAL_XSPI_ADDRESS_NONE;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = HAL_XSPI_DATA_NONE;
  s_command.DummyCycles = 0U;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_EnterPowerDown(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the enter power down command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_ENTER_DEEP_POWER_DOWN_CMD : MX25UM25645G_OCTA_ENTER_DEEP_POWER_DOWN_CMD;
  s_command.AddressMode = HAL_XSPI_ADDRESS_NONE;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = HAL_XSPI_DATA_NONE;
  s_command.DummyCycles = 0U;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

static int32_t MX25UM25645G_ReleasePowerDown(XSPI_HandleTypeDef *ctx, MX25UM25645G_Interface_t mode, MX25UM25645G_Transfer_t rate)
{
  // SPI mode and DTR transfer not supported by memory
  if ((mode == MX25UM25645G_SPI_MODE) && (rate == MX25UM25645G_DTR_TRANSFER))
    return MX25UM25645G_ERROR;

  // Initialize the enter power down command
  XSPI_RegularCmdTypeDef s_command = { 0 };
  s_command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_1_LINE : HAL_XSPI_INSTRUCTION_8_LINES;
  s_command.InstructionDTRMode = (rate == MX25UM25645G_DTR_TRANSFER) ? HAL_XSPI_INSTRUCTION_DTR_ENABLE : HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth = (mode == MX25UM25645G_SPI_MODE) ? HAL_XSPI_INSTRUCTION_8_BITS : HAL_XSPI_INSTRUCTION_16_BITS;
  s_command.Instruction = (mode == MX25UM25645G_SPI_MODE) ? MX25UM25645G_RELEASE_DEEP_POWER_DOWN_CMD : MX25UM25645G_OCTA_RELEASE_DEEP_POWER_DOWN_CMD;
  s_command.AddressMode = HAL_XSPI_ADDRESS_NONE;
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  s_command.DataMode = HAL_XSPI_DATA_NONE;
  s_command.DummyCycles = 0U;
  s_command.DQSMode = HAL_XSPI_DQS_DISABLE;

  // Send the command
  if (HAL_XSPI_Command(ctx, &s_command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return MX25UM25645G_ERROR;
  return MX25UM25645G_OK;
}

#endif  // #ifdef FULL_FLASH_DRIVER


// Public API Functions ------------------------------------------------------------------------------------------------

void HAL_XSPI_MspInit(XSPI_HandleTypeDef* hxspi)
{
  GPIO_InitTypeDef GPIO_InitStruct = { 0 };
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = { 0 };
  if (hxspi->Instance == XSPI2)
  {
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_XSPI2;
    PeriphClkInitStruct.Xspi2ClockSelection = RCC_XSPI2CLKSOURCE_HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
      Error_Handler();

    __HAL_RCC_XSPIM_CLK_ENABLE();
    __HAL_RCC_XSPI2_CLK_ENABLE();
    __HAL_RCC_GPION_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_XSPIM_P2;
    HAL_GPIO_Init(GPION, &GPIO_InitStruct);
  }
}

void flash_init(void)
{
   // Initialize the flash memory peripheral
   XSPI_HandleTypeDef hxspi2 = {
     .Instance = XSPI2,
     .Init = {
       .FifoThresholdByte = 4, // TODO: IS THIS OKAY OR DOES IT HAVE TO BE 1
       .MemoryMode = HAL_XSPI_SINGLE_MEM,
       .MemoryType = HAL_XSPI_MEMTYPE_MACRONIX,
       .MemorySize = HAL_XSPI_SIZE_256MB,
       .ChipSelectHighTimeCycle = 2,
       .FreeRunningClock = HAL_XSPI_FREERUNCLK_DISABLE,
       .ClockMode = HAL_XSPI_CLOCK_MODE_0,
       .WrapSize = HAL_XSPI_WRAP_NOT_SUPPORTED,
       .ClockPrescaler = 0x03,  // TODO: WHY 0x03 AND NOT 0
       .SampleShifting = HAL_XSPI_SAMPLE_SHIFT_NONE,
       .DelayHoldQuarterCycle = HAL_XSPI_DHQC_ENABLE,
       .ChipSelectBoundary = HAL_XSPI_BONDARYOF_NONE,
       .MaxTran = 0,
       .Refresh = 0,
       .MemorySelect = HAL_XSPI_CSSEL_NCS1
     }
   };
   if (HAL_XSPI_Init(&hxspi2) != HAL_OK)
     Error_Handler();

   // Configure the OctoSPI peripheral to use direct mapping for the flash
   XSPIM_CfgTypeDef sXspiManagerCfg = {
     .nCSOverride = HAL_XSPI_CSSEL_OVR_NCS1,
     .IOPort = HAL_XSPIM_IOPORT_2,
     .Req2AckTime = 1
   };
   if (HAL_XSPIM_Config(&hxspi2, &sXspiManagerCfg, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
     Error_Handler();

   // Reset the flash memory to its default configuration (STR SPI mode)
   if ((MX25UM25645G_ResetEnable(&hxspi2, MX25UM25645G_SPI_MODE, MX25UM25645G_STR_TRANSFER) != MX25UM25645G_OK) ||
       (MX25UM25645G_ResetMemory(&hxspi2, MX25UM25645G_SPI_MODE, MX25UM25645G_STR_TRANSFER) != MX25UM25645G_OK) ||
       (MX25UM25645G_ResetEnable(&hxspi2, MX25UM25645G_OPI_MODE, MX25UM25645G_STR_TRANSFER) != MX25UM25645G_OK) ||
       (MX25UM25645G_ResetMemory(&hxspi2, MX25UM25645G_OPI_MODE, MX25UM25645G_STR_TRANSFER) != MX25UM25645G_OK) ||
       (MX25UM25645G_ResetEnable(&hxspi2, MX25UM25645G_OPI_MODE, MX25UM25645G_DTR_TRANSFER) != MX25UM25645G_OK) ||
       (MX25UM25645G_ResetMemory(&hxspi2, MX25UM25645G_OPI_MODE, MX25UM25645G_DTR_TRANSFER) != MX25UM25645G_OK))
     Error_Handler();

   // Wait until the memory becomes available again
   HAL_Delay(100U);
   if (MX25UM25645G_AutoPollingMemReady(&hxspi2, MX25UM25645G_SPI_MODE, MX25UM25645G_STR_TRANSFER) != MX25UM25645G_OK)
     Error_Handler();

   // Configure the flash memory to operate in DTR OPI mode
   if ((MX25UM25645G_WriteEnable(&hxspi2, MX25UM25645G_SPI_MODE, MX25UM25645G_STR_TRANSFER) != MX25UM25645G_OK) ||
       (MX25UM25645G_WriteCfg2Register(&hxspi2, MX25UM25645G_SPI_MODE, MX25UM25645G_STR_TRANSFER, MX25UM25645G_CR2_REG3_ADDR, MX25UM25645G_CR2_DC_20_CYCLES) != MX25UM25645G_OK) ||
       (MX25UM25645G_WriteEnable(&hxspi2, MX25UM25645G_SPI_MODE, MX25UM25645G_STR_TRANSFER) != MX25UM25645G_OK) ||
       (MX25UM25645G_WriteCfg2Register(&hxspi2, MX25UM25645G_SPI_MODE, MX25UM25645G_STR_TRANSFER, MX25UM25645G_CR2_REG1_ADDR, MX25UM25645G_CR2_DOPI) != MX25UM25645G_OK))
     Error_Handler();

   // Wait for the configuration to take effect and re-initialize the peripheral
   HAL_Delay(40U);
   if (HAL_XSPI_Init(&hxspi2) != HAL_OK)
     Error_Handler();

   // Wait until the memory becomes available again
   uint8_t reg[2];
   if ((MX25UM25645G_AutoPollingMemReady(&hxspi2, MX25UM25645G_OPI_MODE, MX25UM25645G_DTR_TRANSFER) != MX25UM25645G_OK) ||
       (MX25UM25645G_ReadCfg2Register(&hxspi2, MX25UM25645G_OPI_MODE, MX25UM25645G_DTR_TRANSFER, MX25UM25645G_CR2_REG1_ADDR, reg) != MX25UM25645G_OK) ||
       (reg[0] != MX25UM25645G_CR2_DOPI))
     Error_Handler();

   // Reconfigure the clock to operate at full speed (200MHz)
   HAL_XSPI_SetClockPrescaler(&hxspi2, 0);

   // Map the flash memory to internal address 0x70000000 and disable prefetch (TODO: disabled due to errata - test and see if needed here - also test and see the effect on inferrence time)
   if (MX25UM25645G_EnableMemoryMappedModeDTR(&hxspi2, MX25UM25645G_OPI_MODE) != MX25UM25645G_OK)
     Error_Handler();
   MODIFY_REG(XSPI2->CR, XSPI_CR_NOPREF, HAL_XSPI_AUTOMATIC_PREFETCH_DISABLE);
}
