// Header Inclusions ---------------------------------------------------------------------------------------------------

#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/times.h>
#include "stm32n6xx.h"
#include "system.h"


// C Standard Library Replacement Functions ----------------------------------------------------------------------------

static uint8_t *__sbrk_heap_end;
static volatile uint32_t sys_tick;

extern int __io_putchar(int ch) __attribute__((weak));
extern int __io_getchar(void) __attribute__((weak));

void initialise_monitor_handles() {}
int _getpid(void) { return 1; }
int _kill(int, int) { errno = EINVAL; return -1; }
void _exit (int status) { _kill(status, -1); while(1); }
__attribute__((weak)) int _read(int, char *ptr, int len) { for (int idx = 0; idx < len; ++idx) *ptr++ = __io_getchar(); return len; }
__attribute__((weak)) int _write(int, char *ptr, int len) { for (int idx = 0; idx < len; ++idx) __io_putchar(*ptr++); return len; }
int _close(int) { return -1; }
int _fstat(int, struct stat *st) { st->st_mode = S_IFCHR; return 0; }
int _isatty(int) { return 1; }
int _lseek(int, int, int) { return 0; }
int _open(char*, int, ...) { return -1; }
int _wait(int*) { errno = ECHILD; return -1; }
int _unlink(char*) { errno = ENOENT; return -1; }
int _times(struct tms*) { return -1; }
int _stat(char*, struct stat *st) { st->st_mode = S_IFCHR; return 0; }
int _link(char*, char*) { errno = EMLINK; return -1; }
int _fork(void) { errno = EAGAIN; return -1; }
int _execve(char*, char**, char**) { errno = ENOMEM; return -1; }

void *_sbrk(ptrdiff_t incr)
{
   // Symbols defined in the linker script
   extern uint8_t _end, _estack;
   extern uint32_t _Min_Stack_Size;

   // Stack and heap limits
   const uint32_t stack_limit = (uint32_t)&_estack - (uint32_t)&_Min_Stack_Size;
   const uint8_t *max_heap = (uint8_t *)stack_limit;
   if (!__sbrk_heap_end)
      __sbrk_heap_end = &_end;

   // Protect heap from growing into the reserved MSP stack
   if ((__sbrk_heap_end + incr) > max_heap)
   {
      errno = ENOMEM;
      return (void*)-1;
   }

   // Return newly allocated heap memory
   uint8_t *prev_heap_end = __sbrk_heap_end;
   __sbrk_heap_end += incr;
   return (void*)prev_heap_end;
}


// ARM Cortex Processor Interrupt and Exception Handlers ---------------------------------------------------------------

void NMI_Handler(void) { while(1); }
void HardFault_Handler(void) { while (1); }
void MemManage_Handler(void) { while (1); }
void BusFault_Handler(void) { while (1); }
void UsageFault_Handler(void) { while (1); }
void SecureFault_Handler(void) { while (1); }
void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}
void SysTick_Handler(void) { sys_tick++; }
void Error_Handler(void) { __disable_irq(); while (1); }


// System Initialization Functions and Definitions ---------------------------------------------------------------------

#define HSLV_OTP 124
#define VDDIO2_HSLV_MASK (1U<<16)
#define VDDIO3_HSLV_MASK (1U<<15)
#define VDDIO4_HSLV_MASK (1U<<14)

extern void *g_pfnVectors;
uint32_t SystemCoreClock;

void SystemInit(void)
{
   // Configure the initial System Core Clock and Vector Table location
   SystemCoreClock = HSI_VALUE;
   SCB->VTOR = ((uint32_t)&g_pfnVectors);

   // Reset the RNG and deactivate its clock
   WRITE_REG(RCC->AHB3RSTSR, RCC_AHB3RSTSR_RNGRSTS);
   WRITE_REG(RCC->AHB3RSTCR, RCC_AHB3RSTCR_RNGRSTC);
   WRITE_REG(RCC->AHB3ENCR, RCC_AHB3ENCR_RNGENC);

   // Clear all SAU regions
   for (uint32_t i = 0; i < 8; ++i)
   {
      WRITE_REG(SAU->RNR, i);
      WRITE_REG(SAU->RBAR, 0);
      WRITE_REG(SAU->RLAR, 0);
   }

   // Enable the system configuration clock
   WRITE_REG(RCC->APB4ENSR2, RCC_APB4ENSR2_SYSCFGENS);
   (void)READ_REG(RCC->APB4ENR2);

   // Set the default Vector Table location after system reset
   WRITE_REG(SYSCFG->INITSVTORCR, SCB->VTOR);

   // Put the VREFBUF peripheral into Hi-Z disabled mode
   WRITE_REG(RCC->APB4ENSR1, RCC_APB4ENR1_VREFBUFEN);
   (void)READ_REG(RCC->APB4ENR1);
   MODIFY_REG(VREFBUF->CSR, VREFBUF_CSR_ENVR, VREFBUF_CSR_HIZ);

   // RCC fix to lower power consumption
   SET_BIT(RCC->APB4ENR2, 0x00000010UL);
   (void)READ_REG(RCC->APB4ENR2);
   CLEAR_BIT(RCC->APB4ENR2, 0x00000010UL);

   // Reset XSPI2 and XSPIM
   WRITE_REG(RCC->AHB5RSTSR, (RCC_AHB5RSTSR_XSPIMRSTS | RCC_AHB5RSTSR_XSPI2RSTS));
   WRITE_REG(RCC->AHB5RSTCR, (RCC_AHB5RSTCR_XSPIMRSTC | RCC_AHB5RSTCR_XSPI2RSTC));

   // Reset TIM2 and deactivate the TIM2 clock
   WRITE_REG(RCC->APB1RSTSR1, RCC_APB1RSTSR1_TIM2RSTS);
   WRITE_REG(RCC->APB1RSTCR1, RCC_APB1RSTCR1_TIM2RSTC);
   WRITE_REG(RCC->APB1ENCR1, RCC_APB1ENCR1_TIM2ENC);

   // Deactivate the GPIOG clock
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENCR_GPIOGENC);

   // Deactivate the SYSCFG clock
   (void)READ_REG(SYSCFG->INITSVTORCR);
   WRITE_REG(RCC->APB4ENCR2, RCC_APB4ENCR2_SYSCFGENC);

   // Enable the FPU
   SET_BIT(SCB->CPACR, ((3UL << 20U) | (3UL << 22U)));
   SET_BIT(SCB_NS->CPACR, ((3UL << 20U) | (3UL << 22U)));
}

void SystemCoreClockUpdate(void)
{
   // Initialize the system clock configuration variables
   uint32_t sysclk = 0, pllm = 0, plln = 0, pllfracn = 0, pllp1 = 0, pllp2 = 0;
   uint32_t pllcfgr, pllsource, pllbypass, ic_divider;
   float pllvco;

   // Retrieve the CPUCLK source
   switch (RCC->CFGR1 & RCC_CFGR1_CPUSWS)
   {
      case 0:                                            // HSI used as system clock source (default after reset)
         sysclk = HSI_VALUE >> ((RCC->HSICFGR & RCC_HSICFGR_HSIDIV) >> RCC_HSICFGR_HSIDIV_Pos);
         break;
      case RCC_CFGR1_CPUSWS_0:                           // MSI used as system clock source
         sysclk = (READ_BIT(RCC->MSICFGR, RCC_MSICFGR_MSIFREQSEL) == 0UL) ? MSI_VALUE : 16000000UL;
         break;
      case RCC_CFGR1_CPUSWS_1:                           // HSE used as system clock source
         sysclk = HSE_VALUE;
         break;
      case (RCC_CFGR1_CPUSWS_1 | RCC_CFGR1_CPUSWS_0):    // IC1 used as system clock  source
         switch (READ_BIT(RCC->IC1CFGR, RCC_IC1CFGR_IC1SEL))
         {
            case 0:                                      // PLL1 selected at IC1 clock source
               pllcfgr = READ_REG(RCC->PLL1CFGR1);
               pllsource = pllcfgr & RCC_PLL1CFGR1_PLL1SEL;
               pllbypass = pllcfgr & RCC_PLL1CFGR1_PLL1BYP;
               if (pllbypass == 0U)
               {
                  pllm = (pllcfgr & RCC_PLL1CFGR1_PLL1DIVM) >> RCC_PLL1CFGR1_PLL1DIVM_Pos;
                  plln = (pllcfgr & RCC_PLL1CFGR1_PLL1DIVN) >> RCC_PLL1CFGR1_PLL1DIVN_Pos;
                  pllfracn = READ_BIT(RCC->PLL1CFGR2, RCC_PLL1CFGR2_PLL1DIVNFRAC) >> RCC_PLL1CFGR2_PLL1DIVNFRAC_Pos;
                  pllcfgr = READ_REG(RCC->PLL1CFGR3);
                  pllp1 = (pllcfgr & RCC_PLL1CFGR3_PLL1PDIV1) >> RCC_PLL1CFGR3_PLL1PDIV1_Pos;
                  pllp2 = (pllcfgr & RCC_PLL1CFGR3_PLL1PDIV2) >> RCC_PLL1CFGR3_PLL1PDIV2_Pos;
               }
               break;
            case RCC_IC1CFGR_IC1SEL_0:                   // PLL2 selected at IC1 clock source
               pllcfgr = READ_REG(RCC->PLL2CFGR1);
               pllsource = pllcfgr & RCC_PLL2CFGR1_PLL2SEL;
               pllbypass = pllcfgr & RCC_PLL2CFGR1_PLL2BYP;
               if (pllbypass == 0U)
               {
                  pllm = (pllcfgr & RCC_PLL2CFGR1_PLL2DIVM) >> RCC_PLL2CFGR1_PLL2DIVM_Pos;
                  plln = (pllcfgr & RCC_PLL2CFGR1_PLL2DIVN) >> RCC_PLL2CFGR1_PLL2DIVN_Pos;
                  pllfracn = READ_BIT(RCC->PLL2CFGR2, RCC_PLL2CFGR2_PLL2DIVNFRAC) >> RCC_PLL2CFGR2_PLL2DIVNFRAC_Pos;
                  pllcfgr = READ_REG(RCC->PLL2CFGR3);
                  pllp1 = (pllcfgr & RCC_PLL2CFGR3_PLL2PDIV1) >> RCC_PLL2CFGR3_PLL2PDIV1_Pos;
                  pllp2 = (pllcfgr & RCC_PLL2CFGR3_PLL2PDIV2) >> RCC_PLL2CFGR3_PLL2PDIV2_Pos;
               }
               break;
            case RCC_IC1CFGR_IC1SEL_1:                   // PLL3 selected at IC1 clock source
               pllcfgr = READ_REG(RCC->PLL3CFGR1);
               pllsource = pllcfgr & RCC_PLL3CFGR1_PLL3SEL;
               pllbypass = pllcfgr & RCC_PLL3CFGR1_PLL3BYP;
               if (pllbypass == 0U)
               {
                  pllm = (pllcfgr & RCC_PLL3CFGR1_PLL3DIVM) >>  RCC_PLL3CFGR1_PLL3DIVM_Pos;
                  plln = (pllcfgr & RCC_PLL3CFGR1_PLL3DIVN) >>  RCC_PLL3CFGR1_PLL3DIVN_Pos;
                  pllfracn = READ_BIT(RCC->PLL3CFGR2, RCC_PLL3CFGR2_PLL3DIVNFRAC) >>  RCC_PLL3CFGR2_PLL3DIVNFRAC_Pos;
                  pllcfgr = READ_REG(RCC->PLL3CFGR3);
                  pllp1 = (pllcfgr & RCC_PLL3CFGR3_PLL3PDIV1) >>  RCC_PLL3CFGR3_PLL3PDIV1_Pos;
                  pllp2 = (pllcfgr & RCC_PLL3CFGR3_PLL3PDIV2) >>  RCC_PLL3CFGR3_PLL3PDIV2_Pos;
               }
               break;
            default:                                     // PLL4 selected at IC1 clock source
               pllcfgr = READ_REG(RCC->PLL4CFGR1);
               pllsource = pllcfgr & RCC_PLL4CFGR1_PLL4SEL;
               pllbypass = pllcfgr & RCC_PLL4CFGR1_PLL4BYP;
               if (pllbypass == 0U)
               {
                  pllm = (pllcfgr & RCC_PLL4CFGR1_PLL4DIVM) >>  RCC_PLL4CFGR1_PLL4DIVM_Pos;
                  plln = (pllcfgr & RCC_PLL4CFGR1_PLL4DIVN) >>  RCC_PLL4CFGR1_PLL4DIVN_Pos;
                  pllfracn = READ_BIT(RCC->PLL4CFGR2, RCC_PLL4CFGR2_PLL4DIVNFRAC) >>  RCC_PLL4CFGR2_PLL4DIVNFRAC_Pos;
                  pllcfgr = READ_REG(RCC->PLL4CFGR3);
                  pllp1 = (pllcfgr & RCC_PLL4CFGR3_PLL4PDIV1) >>  RCC_PLL4CFGR3_PLL4PDIV1_Pos;
                  pllp2 = (pllcfgr & RCC_PLL4CFGR3_PLL4PDIV2) >>  RCC_PLL4CFGR3_PLL4PDIV2_Pos;
               }
               break;
         }
         switch (pllsource)
         {
            case 0:                                      // HSI selected as PLL clock source
               sysclk = HSI_VALUE >> ((RCC->HSICFGR & RCC_HSICFGR_HSIDIV) >> RCC_HSICFGR_HSIDIV_Pos);
               break;
            case RCC_PLL1CFGR1_PLL1SEL_0:                // MSI selected as PLL clock source
               sysclk = (READ_BIT(RCC->MSICFGR, RCC_MSICFGR_MSIFREQSEL) == 0UL) ? MSI_VALUE : 16000000UL;
               break;
            case RCC_PLL1CFGR1_PLL1SEL_1:                // HSE selected as PLL clock source
               sysclk = HSE_VALUE;
               break;
            case (RCC_PLL1CFGR1_PLL1SEL_1 | RCC_PLL1CFGR1_PLL1SEL_0):  // I2S_CKIN selected as PLL clock source
               sysclk = 12288000UL;
               break;
            default:
               break;
         }
         if (pllbypass == 0U)
         {
            // Compte PLL output frequency (Integer and fractional modes)
            // PLLVCO = (Freq * (DIVN + (FRACN / 0x1000000) / DIVM) / (DIVP1 * DIVP2))
            pllvco = ((float)sysclk * ((float)plln + ((float)pllfracn / (float)0x1000000UL))) / (float)pllm;
            sysclk = (uint32_t)((float)(pllvco / (((float)pllp1) * ((float)pllp2))));
         }
         ic_divider = (READ_BIT(RCC->IC1CFGR, RCC_IC1CFGR_IC1INT) >> RCC_IC1CFGR_IC1INT_Pos) + 1UL;
         sysclk = sysclk / ic_divider;
         break;
      default:
         break;
   }
   SystemCoreClock = sysclk;
}

__attribute((cmse_nonsecure_entry))
uint32_t SECURE_SystemCoreClockUpdate(void)
{
   SystemCoreClockUpdate();
   return SystemCoreClock;
}


// Global Interrupt Service Routines -----------------------------------------------------------------------------------

void EXTI12_IRQHandler(void)
{
   // Check if a rising edge was detected
   if (READ_BIT(EXTI->RPR1, SD_CARD_DETECT_Pin))
   {
      WRITE_REG(EXTI->RPR1, SD_CARD_DETECT_Pin);
      sd_card_detection_isr(1);
   }

   // Check if a falling edge was detected
   if (READ_BIT(EXTI->FPR1, SD_CARD_DETECT_Pin))
   {
      WRITE_REG(EXTI->FPR1, SD_CARD_DETECT_Pin);
      sd_card_detection_isr(0);
   }
}

void EXTI15_IRQHandler(void)
{
   // Check if a rising edge was detected
   if (READ_BIT(EXTI->RPR1, DATA_IN_CS_Pin))
   {
      WRITE_REG(EXTI->RPR1, DATA_IN_CS_Pin);
      comms_spi_cs_isr();
   }
}


// Private Helper Functions --------------------------------------------------------------------------------------------

static uint32_t get_risaf_max_addr(RISAF_TypeDef *risaf)
{
  uint32_t max_addr = 0U;
  if      ((risaf == RISAF1_S)  || (risaf == RISAF1_NS))  { max_addr = RISAF1_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF2_S)  || (risaf == RISAF2_NS))  { max_addr = RISAF2_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF3_S)  || (risaf == RISAF3_NS))  { max_addr = RISAF3_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF4_S)  || (risaf == RISAF4_NS))  { max_addr = RISAF4_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF5_S)  || (risaf == RISAF5_NS))  { max_addr = RISAF5_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF6_S)  || (risaf == RISAF6_NS))  { max_addr = RISAF6_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF7_S)  || (risaf == RISAF7_NS))  { max_addr = RISAF7_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF8_S)  || (risaf == RISAF8_NS))  { max_addr = RISAF8_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF9_S)  || (risaf == RISAF9_NS))  { max_addr = RISAF9_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF11_S) || (risaf == RISAF11_NS)) { max_addr = RISAF11_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF12_S) || (risaf == RISAF12_NS)) { max_addr = RISAF12_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF13_S) || (risaf == RISAF13_NS)) { max_addr = RISAF13_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF14_S) || (risaf == RISAF14_NS)) { max_addr = RISAF14_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF15_S) || (risaf == RISAF15_NS)) { max_addr = RISAF15_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF21_S) || (risaf == RISAF21_NS)) { max_addr = RISAF21_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF22_S) || (risaf == RISAF22_NS)) { max_addr = RISAF22_LIMIT_ADDRESS_SPACE_SIZE; }
  else if ((risaf == RISAF23_S) || (risaf == RISAF23_NS)) { max_addr = RISAF23_LIMIT_ADDRESS_SPACE_SIZE; }
  return max_addr;
}


// Public API Functions ------------------------------------------------------------------------------------------------

void system_init(void)
{
   // Disable global interrupts
   uint32_t primask_bit = __get_PRIMASK();
   __disable_irq();

   // Disable fault exceptions and the MPU
   __DMB();
   CLEAR_BIT(SCB->SHCSR, SCB_SHCSR_MEMFAULTENA_Msk);
   CLEAR_BIT(MPU->CTRL, MPU_CTRL_ENABLE_Msk);
   __DSB();
   __ISB();

   // Create a non-cacheable attribute configuration for the MPU
   MODIFY_REG(MPU->MAIR0, (0xFFU << (MPU_ATTRIBUTES_NUMBER0 * 8U)), (INNER_OUTER(MPU_NOT_CACHEABLE) << (MPU_ATTRIBUTES_NUMBER0 * 8U)));

   // Set up a first non-cacheable memory region
   WRITE_REG(MPU->RNR, MPU_REGION_NUMBER0);
   CLEAR_BIT(MPU->RLAR, MPU_RLAR_EN_Msk);
   WRITE_REG(MPU->RBAR, ((__NON_CACHEABLE_SECTION_BEGIN & 0xFFFFFFE0UL) | (MPU_ACCESS_OUTER_SHAREABLE << MPU_RBAR_SH_Pos) | (MPU_REGION_ALL_RW << MPU_RBAR_AP_Pos) | (MPU_INSTRUCTION_ACCESS_DISABLE << MPU_RBAR_XN_Pos)));
   WRITE_REG(MPU->RLAR, ((__NON_CACHEABLE_SECTION_END & 0xFFFFFFE0UL) | (MPU_PRIV_INSTRUCTION_ACCESS_DISABLE << MPU_RLAR_PXN_Pos) | (MPU_ATTRIBUTES_NUMBER0 << MPU_RLAR_AttrIndx_Pos) | (MPU_REGION_ENABLE << MPU_RLAR_EN_Pos)));

   // Set up a second non-cacheable memory region
   WRITE_REG(MPU->RNR, MPU_REGION_NUMBER1);
   CLEAR_BIT(MPU->RLAR, MPU_RLAR_EN_Msk);
   WRITE_REG(MPU->RBAR, ((__NON_ESSENTIAL_SECTION_BEGIN & 0xFFFFFFE0UL) | (MPU_ACCESS_OUTER_SHAREABLE << MPU_RBAR_SH_Pos) | (MPU_REGION_ALL_RW << MPU_RBAR_AP_Pos) | (MPU_INSTRUCTION_ACCESS_DISABLE << MPU_RBAR_XN_Pos)));
   WRITE_REG(MPU->RLAR, ((__NON_ESSENTIAL_SECTION_END & 0xFFFFFFE0UL) | (MPU_PRIV_INSTRUCTION_ACCESS_DISABLE << MPU_RLAR_PXN_Pos) | (MPU_ATTRIBUTES_NUMBER0 << MPU_RLAR_AttrIndx_Pos) | (MPU_REGION_ENABLE << MPU_RLAR_EN_Pos)));

   // Enable the MPU and re-enable fault exceptions
   __DMB();
   WRITE_REG(MPU->CTRL, (MPU_PRIVILEGED_DEFAULT | MPU_CTRL_ENABLE_Msk));
   SET_BIT(SCB->SHCSR, SCB_SHCSR_MEMFAULTENA_Msk);
   __DSB();
   __ISB();

   // Enable global interrupts
   __set_PRIMASK(primask_bit);

   // Ensure that the CPU and SYS clocks are sourced from HSI in a known good configuration
   MODIFY_REG(RCC->CFGR1, RCC_CFGR1_CPUSW, RCC_CPUCLKSOURCE_HSI);
   MODIFY_REG(RCC->CFGR1, RCC_CFGR1_SYSSW, RCC_SYSCLKSOURCE_HSI);
   MODIFY_REG(RCC->HSICFGR, RCC_HSICFGR_HSIDIV, RCC_HSI_DIV1);
   MODIFY_REG(RCC->HSICFGR, RCC_HSICFGR_HSITRIM, RCC_HSICALIBRATION_DEFAULT << RCC_HSICFGR_HSITRIM_Pos);

   // Set the interrupt group priority
   NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

   // Enable the BSEC and SYSCFG peripherals
   WRITE_REG(RCC->APB4ENSR2, RCC_APB4ENR2_BSECEN);
   (void)READ_BIT(RCC->APB4ENR2, RCC_APB4ENR2_BSECEN);
   WRITE_REG(RCC->APB4ENSR2, RCC_APB4ENR2_SYSCFGEN);
   (void)READ_BIT(RCC->APB4ENR2, RCC_APB4ENR2_SYSCFGEN);

   // Read the current voltage fuse data
   MODIFY_REG(BSEC->OTPCR, (BSEC_OTPCR_PPLOCK | BSEC_OTPCR_PROG | BSEC_OTPCR_ADDR), HSLV_OTP);
   while (READ_BIT(BSEC->OTPSR, BSEC_OTPSR_BUSY));
   uint32_t fuse_data = BSEC->FVRw[HSLV_OTP];

   // Blow all necessary fuses
   const uint32_t expected_data = VDDIO2_HSLV_MASK | VDDIO3_HSLV_MASK | VDDIO4_HSLV_MASK;
   if ((fuse_data & expected_data) != expected_data)
   {
      uint8_t success = 0;
      fuse_data |= expected_data;
      for (uint8_t i = 0; !success && (i < 10); ++i)
      {
         BSEC->WDR = fuse_data;
         MODIFY_REG(BSEC->OTPCR, (BSEC_OTPCR_PPLOCK | BSEC_OTPCR_PROG | BSEC_OTPCR_ADDR), (HSLV_OTP | BSEC_OTPCR_PROG));
         while (READ_BIT(BSEC->OTPSR, BSEC_OTPSR_BUSY));
         if (!READ_BIT(BSEC->OTPSR, BSEC_OTPSR_PROGFAIL))
         {
            MODIFY_REG(BSEC->OTPCR, (BSEC_OTPCR_PPLOCK | BSEC_OTPCR_PROG | BSEC_OTPCR_ADDR), HSLV_OTP);
            while (READ_BIT(BSEC->OTPSR, BSEC_OTPSR_BUSY));
            success = (BSEC->FVRw[HSLV_OTP] == fuse_data);
         }
      }
   }

   // Enable the PWR peripheral
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_PWREN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_PWREN);

   // Enable non-privileged access for the PWR peripheral
   SET_BIT(PWR_S->SECCFGR, PWR_ITEM_0);
   CLEAR_BIT(PWR->PRIVCFGR, PWR_ITEM_0);

   // Configure the voltage range for each VDDIO domain
   SET_BIT(PWR->SVMCR3, PWR_SVMCR3_VDDIO2SV);
   MODIFY_REG(PWR->SVMCR3, PWR_SVMCR3_VDDIO2VRSEL, PWR_VDDIO_RANGE_1V8 << PWR_SVMCR3_VDDIO2VRSEL_Pos);
   SET_BIT(PWR->SVMCR3, PWR_SVMCR3_VDDIO3SV);
   MODIFY_REG(PWR->SVMCR3, PWR_SVMCR3_VDDIO3VRSEL, PWR_VDDIO_RANGE_1V8 << PWR_SVMCR3_VDDIO3VRSEL_Pos);
   SET_BIT(PWR->SVMCR1, PWR_SVMCR1_VDDIO4SV);
   MODIFY_REG(PWR->SVMCR1, PWR_SVMCR1_VDDIO4VRSEL, PWR_VDDIO_RANGE_3V3 << PWR_SVMCR1_VDDIO4VRSEL_Pos);
   SET_BIT(PWR->SVMCR2, PWR_SVMCR2_VDDIO5SV);

   // Configure the voltage compensation cells
   MODIFY_REG(SYSCFG->VDDIO2CCCR, 0x2FFU, (0x7 | (0x8 << 4U) | SYSCFG_VDDIO2CCCR_CS));
   MODIFY_REG(SYSCFG->VDDIO3CCCR, 0x2FFU, (0x7 | (0x8 << 4U) | SYSCFG_VDDIO3CCCR_CS));
   MODIFY_REG(SYSCFG->VDDIO4CCCR, 0x2FFU, (0x7 | (0x8 << 4U) | SYSCFG_VDDIO4CCCR_CS));
   MODIFY_REG(SYSCFG->VDDIO5CCCR, 0x2FFU, (0x7 | (0x8 << 4U) | SYSCFG_VDDIO5CCCR_CS));
   MODIFY_REG(SYSCFG->VDDCCCR, 0xFFU, (0x7 | (0x8 << 4U) | SYSCFG_VDDCCCR_CS));

   // Disable unused voltage domain VDDIO2
   CLEAR_BIT(PWR->SVMCR3, PWR_SVMCR3_VDDIO2SV);

   // Configure the system to use an external SMPS power supply
   MODIFY_REG(PWR->CR1, PWR_SUPPLY_CONFIG_MASK, PWR_EXTERNAL_SOURCE_SUPPLY);
   while (!READ_REG(PWR->VOSCR & PWR_VOSCR_ACTVOSRDY));

   // Enable PLL1 to output at 800MHz
   WRITE_REG(RCC->CCR, RCC_CCR_PLL1ONC);
   while (READ_BIT(RCC->SR, RCC_SR_PLL1RDY));
   SET_BIT(RCC->PLL1CFGR3, RCC_PLL1CFGR3_PLL1MODSSDIS);
   CLEAR_BIT(RCC->PLL1CFGR1, RCC_PLL1CFGR1_PLL1BYP);
   MODIFY_REG(RCC->PLL1CFGR1, (RCC_PLL1CFGR1_PLL1SEL | RCC_PLL1CFGR1_PLL1DIVM | RCC_PLL1CFGR1_PLL1DIVN), (RCC_PLLSOURCE_HSI | (2U << RCC_PLL1CFGR1_PLL1DIVM_Pos) | (25U << RCC_PLL1CFGR1_PLL1DIVN_Pos)));
   MODIFY_REG(RCC->PLL1CFGR3, (RCC_PLL1CFGR3_PLL1PDIV1 | RCC_PLL1CFGR3_PLL1PDIV2), ((1U << RCC_PLL1CFGR3_PLL1PDIV1_Pos) | (1U << RCC_PLL1CFGR3_PLL1PDIV2_Pos)));
   MODIFY_REG(RCC->PLL1CFGR2, RCC_PLL1CFGR2_PLL1DIVNFRAC, (0U << RCC_PLL1CFGR2_PLL1DIVNFRAC_Pos));
   CLEAR_BIT(RCC->PLL1CFGR3, RCC_PLL1CFGR3_PLL1MODDSEN);
   SET_BIT(RCC->PLL1CFGR3, (RCC_PLL1CFGR3_PLL1MODSSRST | RCC_PLL1CFGR3_PLL1PDIVEN));
   WRITE_REG(RCC->CSR, RCC_CSR_PLL1ONS);
   while (!READ_BIT(RCC->SR, RCC_SR_PLL1RDY));

   // Configure the various peripheral clock sources
   if (RCC_HCLK_DIV2 > (RCC->CFGR2 & RCC_CFGR2_HPRE))
      MODIFY_REG(RCC->CFGR2, RCC_CFGR2_HPRE, RCC_HCLK_DIV2);
   WRITE_REG(RCC->IC1CFGR, (RCC_ICCLKSOURCE_PLL1 | ((2U - 1U) << RCC_IC1CFGR_IC1INT_Pos)));   // IC1 used for CPU clock @ 400MHz
   WRITE_REG(RCC->DIVENSR, RCC_DIVENSR_IC1ENS);
   MODIFY_REG(RCC->CFGR1, RCC_CFGR1_CPUSW, RCC_CPUCLKSOURCE_IC1);
   while (READ_BIT(RCC->CFGR1, RCC_CFGR1_CPUSWS) != (RCC_CPUCLKSOURCE_IC1 << 4U));
   WRITE_REG(RCC->IC2CFGR, RCC_ICCLKSOURCE_PLL1 | ((2U - 1U) << RCC_IC2CFGR_IC2INT_Pos));     // IC2 used for SYS clock @ 400MHz
   WRITE_REG(RCC->IC6CFGR, RCC_ICCLKSOURCE_PLL1 | ((1U - 1U) << RCC_IC6CFGR_IC6INT_Pos));     // IC6 used for NPU @ 800MHz
   WRITE_REG(RCC->IC11CFGR, RCC_ICCLKSOURCE_PLL1 | ((1U - 1U) << RCC_IC11CFGR_IC11INT_Pos));  // IC11 used for AXISRAM 3-6 @ 800MHz
   WRITE_REG(RCC->DIVENSR, (RCC_DIVENSR_IC2ENS | RCC_DIVENSR_IC6ENS | RCC_DIVENSR_IC11ENS));
   MODIFY_REG(RCC->CFGR1, RCC_CFGR1_SYSSW, RCC_SYSCLKSOURCE_IC2_IC6_IC11);
   while (READ_BIT(RCC->CFGR1, RCC_CFGR1_SYSSWS) != (RCC_SYSCLKSOURCE_IC2_IC6_IC11 << 4U));
   if (RCC_HCLK_DIV2 < (RCC->CFGR2 & RCC_CFGR2_HPRE))
      MODIFY_REG(RCC->CFGR2, RCC_CFGR2_HPRE, RCC_HCLK_DIV2);
   if (RCC_APB1_DIV1 < (RCC->CFGR2 & RCC_CFGR2_PPRE1))
      MODIFY_REG(RCC->CFGR2, RCC_CFGR2_PPRE1, RCC_APB1_DIV1);
   if (RCC_APB2_DIV1 < (RCC->CFGR2 & RCC_CFGR2_PPRE2))
      MODIFY_REG(RCC->CFGR2, RCC_CFGR2_PPRE2, RCC_APB2_DIV1);
   if (RCC_APB4_DIV1 < (RCC->CFGR2 & RCC_CFGR2_PPRE4))
      MODIFY_REG(RCC->CFGR2, RCC_CFGR2_PPRE4, RCC_APB4_DIV1);
   if (RCC_APB5_DIV1 < (RCC->CFGR2 & RCC_CFGR2_PPRE5))
      MODIFY_REG(RCC->CFGR2, RCC_CFGR2_PPRE5, RCC_APB5_DIV1);

   // Update the SystemCoreClock global variable
   SystemCoreClockUpdate();

   // Initialize a 1ms HSI-based SysTick (to be disabled later)
   WRITE_REG(SysTick->LOAD, (SystemCoreClock / 1000UL) - 1UL);
   WRITE_REG(SysTick->VAL, 0UL);
   WRITE_REG(SysTick->CTRL, (SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk));
   NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), TICK_INT_PRIORITY, 0));

   // Enable secure access for RIF-aware peripherals
   WRITE_REG(RCC->AHB3ENSR, RCC_AHB3ENR_RIFSCEN);
   (void)READ_BIT(RCC->AHB3ENR, RCC_AHB3ENR_RIFSCEN);
   system_set_risaf_default(RISAF2);  // AXISRAM1
   system_set_risaf_default(RISAF3);  // AXISRAM2
   const uint32_t master_cid = POSITION_VAL(RIF_CID_1);
   MODIFY_REG(RIFSC->RIMC_ATTRx[RIF_MASTER_INDEX_NPU], (RIFSC_RIMC_ATTRx_MCID | RIFSC_RIMC_ATTRx_MPRIV | RIFSC_RIMC_ATTRx_MSEC), ((master_cid << RIFSC_RIMC_ATTRx_MCID_Pos) | ((RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV) << RIFSC_RIMC_ATTRx_MSEC_Pos)));
   MODIFY_REG(RIFSC->RISC_SECCFGRx[RIF_RISC_PERIPH_INDEX_NPU >> RIF_PERIPH_REG_SHIFT], (1UL << (RIF_RISC_PERIPH_INDEX_NPU & RIF_PERIPH_BIT_POSITION)), (RIF_ATTRIBUTE_SEC << (RIF_RISC_PERIPH_INDEX_NPU & RIF_PERIPH_BIT_POSITION)));
   MODIFY_REG(RIFSC->RISC_PRIVCFGRx[RIF_RISC_PERIPH_INDEX_NPU >> RIF_PERIPH_REG_SHIFT], (1UL << (RIF_RISC_PERIPH_INDEX_NPU & RIF_PERIPH_BIT_POSITION)), ((RIF_ATTRIBUTE_PRIV >> 1U) << (RIF_RISC_PERIPH_INDEX_NPU & RIF_PERIPH_BIT_POSITION)));
   MODIFY_REG(RIFSC->RIMC_ATTRx[RIF_MASTER_INDEX_SDMMC1], (RIFSC_RIMC_ATTRx_MCID | RIFSC_RIMC_ATTRx_MPRIV | RIFSC_RIMC_ATTRx_MSEC), ((master_cid << RIFSC_RIMC_ATTRx_MCID_Pos) | ((RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV) << RIFSC_RIMC_ATTRx_MSEC_Pos)));
   MODIFY_REG(RIFSC->RISC_SECCFGRx[RIF_RISC_PERIPH_INDEX_SDMMC1 >> RIF_PERIPH_REG_SHIFT], (1UL << (RIF_RISC_PERIPH_INDEX_SDMMC1 & RIF_PERIPH_BIT_POSITION)), (RIF_ATTRIBUTE_SEC << (RIF_RISC_PERIPH_INDEX_SDMMC1 & RIF_PERIPH_BIT_POSITION)));
   MODIFY_REG(RIFSC->RISC_PRIVCFGRx[RIF_RISC_PERIPH_INDEX_SDMMC1 >> RIF_PERIPH_REG_SHIFT], (1UL << (RIF_RISC_PERIPH_INDEX_SDMMC1 & RIF_PERIPH_BIT_POSITION)), ((RIF_ATTRIBUTE_PRIV >> 1U) << (RIF_RISC_PERIPH_INDEX_SDMMC1 & RIF_PERIPH_BIT_POSITION)));
   MODIFY_REG(RIFSC->RISC_SECCFGRx[RIF_RISC_PERIPH_INDEX_SPI1 >> RIF_PERIPH_REG_SHIFT], (1UL << (RIF_RISC_PERIPH_INDEX_SPI1 & RIF_PERIPH_BIT_POSITION)), (RIF_ATTRIBUTE_SEC << (RIF_RISC_PERIPH_INDEX_SPI1 & RIF_PERIPH_BIT_POSITION)));
   MODIFY_REG(RIFSC->RISC_PRIVCFGRx[RIF_RISC_PERIPH_INDEX_SPI1 >> RIF_PERIPH_REG_SHIFT], (1UL << (RIF_RISC_PERIPH_INDEX_SPI1 & RIF_PERIPH_BIT_POSITION)), ((RIF_ATTRIBUTE_PRIV >> 1U) << (RIF_RISC_PERIPH_INDEX_SPI1 & RIF_PERIPH_BIT_POSITION)));
   MODIFY_REG(RIFSC->RISC_SECCFGRx[RIF_RISC_PERIPH_INDEX_I2C3 >> RIF_PERIPH_REG_SHIFT], (1UL << (RIF_RISC_PERIPH_INDEX_I2C3 & RIF_PERIPH_BIT_POSITION)), (RIF_ATTRIBUTE_SEC << (RIF_RISC_PERIPH_INDEX_I2C3 & RIF_PERIPH_BIT_POSITION)));
   MODIFY_REG(RIFSC->RISC_PRIVCFGRx[RIF_RISC_PERIPH_INDEX_I2C3 >> RIF_PERIPH_REG_SHIFT], (1UL << (RIF_RISC_PERIPH_INDEX_I2C3 & RIF_PERIPH_BIT_POSITION)), ((RIF_ATTRIBUTE_PRIV >> 1U) << (RIF_RISC_PERIPH_INDEX_I2C3 & RIF_PERIPH_BIT_POSITION)));
   MODIFY_REG(RIFSC->RISC_SECCFGRx[RIF_RISC_PERIPH_INDEX_IWDG >> RIF_PERIPH_REG_SHIFT], (1UL << (RIF_RISC_PERIPH_INDEX_IWDG & RIF_PERIPH_BIT_POSITION)), (RIF_ATTRIBUTE_SEC << (RIF_RISC_PERIPH_INDEX_IWDG & RIF_PERIPH_BIT_POSITION)));
   MODIFY_REG(RIFSC->RISC_PRIVCFGRx[RIF_RISC_PERIPH_INDEX_IWDG >> RIF_PERIPH_REG_SHIFT], (1UL << (RIF_RISC_PERIPH_INDEX_IWDG & RIF_PERIPH_BIT_POSITION)), ((RIF_ATTRIBUTE_PRIV >> 1U) << (RIF_RISC_PERIPH_INDEX_IWDG & RIF_PERIPH_BIT_POSITION)));

   // Enable all GPIO clocks
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOAEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOAEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOBEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOBEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOCEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOCEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIODEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIODEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOEEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOEEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOFEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOFEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOGEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOGEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOHEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOHEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIONEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIONEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOOEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOOEN);
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOPEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOPEN);

   // Configure non-privileged access for all GPIO pins
   SET_BIT(HOST_WAKEUP_GPIO_Port->SECCFGR, HOST_WAKEUP_Pin); CLEAR_BIT(HOST_WAKEUP_GPIO_Port->PRIVCFGR, HOST_WAKEUP_Pin);
   SET_BIT(DATA_IN_SCK_GPIO_Port->SECCFGR, DATA_IN_SCK_Pin); CLEAR_BIT(DATA_IN_SCK_GPIO_Port->PRIVCFGR, DATA_IN_SCK_Pin);
   SET_BIT(DATA_IN_MISO_GPIO_Port->SECCFGR, DATA_IN_MISO_Pin); CLEAR_BIT(DATA_IN_MISO_GPIO_Port->PRIVCFGR, DATA_IN_MISO_Pin);
   SET_BIT(DATA_IN_MOSI_GPIO_Port->SECCFGR,  DATA_IN_MOSI_Pin); CLEAR_BIT(DATA_IN_MOSI_GPIO_Port->PRIVCFGR,  DATA_IN_MOSI_Pin);
   SET_BIT(DATA_IN_CS_GPIO_Port->SECCFGR, DATA_IN_CS_Pin); CLEAR_BIT(DATA_IN_CS_GPIO_Port->PRIVCFGR, DATA_IN_CS_Pin);
   SET_BIT(DATA_OUT_SCL_GPIO_Port->SECCFGR, DATA_OUT_SCL_Pin); CLEAR_BIT(DATA_OUT_SCL_GPIO_Port->PRIVCFGR, DATA_OUT_SCL_Pin);
   SET_BIT(DATA_OUT_SDA_GPIO_Port->SECCFGR, DATA_OUT_SDA_Pin); CLEAR_BIT(DATA_OUT_SDA_GPIO_Port->PRIVCFGR, DATA_OUT_SDA_Pin);
   SET_BIT(SD_PWR_SELECT_GPIO_Port->SECCFGR, SD_PWR_SELECT_Pin); CLEAR_BIT(SD_PWR_SELECT_GPIO_Port->PRIVCFGR, SD_PWR_SELECT_Pin);
   SET_BIT(SD_CARD_EN_GPIO_Port->SECCFGR, SD_CARD_EN_Pin); CLEAR_BIT(SD_CARD_EN_GPIO_Port->PRIVCFGR, SD_CARD_EN_Pin);
   SET_BIT(SD_CARD_DETECT_GPIO_Port->SECCFGR, SD_CARD_DETECT_Pin); CLEAR_BIT(SD_CARD_DETECT_GPIO_Port->PRIVCFGR, SD_CARD_DETECT_Pin);
#if REV_ID < REV_C
   SET_BIT(LED_MCU_STATUS_GPIO_Port->SECCFGR, LED_MCU_STATUS_Pin); CLEAR_BIT(LED_MCU_STATUS_GPIO_Port->PRIVCFGR, LED_MCU_STATUS_Pin);
   SET_BIT(AI_OVERDRIVE_GPIO_Port->SECCFGR, AI_OVERDRIVE_Pin); CLEAR_BIT(AI_OVERDRIVE_GPIO_Port->PRIVCFGR, AI_OVERDRIVE_Pin);
#endif

   // Set all unused pins an analog inputs for lowest power consumption
   const gpio_pin_t unused_pins[] = UNUSED_PINS;
   for (uint32_t i = 0; i < (sizeof(unused_pins) / sizeof(unused_pins[0])); ++i)
   {
     const uint32_t position = 32 - __builtin_clz(unused_pins[i].pin) - 1;
     MODIFY_REG(unused_pins[i].port->OSPEEDR, (GPIO_OSPEEDR_OSPEED0 << (position * 2U)), (GPIO_SPEED_FREQ_LOW << (position * 2U)));
     MODIFY_REG(unused_pins[i].port->OTYPER, (GPIO_OTYPER_OT0 << position), (((GPIO_MODE_ANALOG & OUTPUT_TYPE) >> OUTPUT_TYPE_Pos) << position));
     MODIFY_REG(unused_pins[i].port->PUPDR, (GPIO_PUPDR_PUPD0 << (position * 2U)), (GPIO_NOPULL << (position * 2U)));
     MODIFY_REG(unused_pins[i].port->MODER, (GPIO_MODER_MODE0 << (position * 2U)), ((GPIO_MODE_ANALOG & GPIO_MODE) << (position * 2U)));
   }

#if REV_ID < REV_C

   // Clear the initial state of required GPIO pins
   WRITE_REG(LED_MCU_STATUS_GPIO_Port->BRR, LED_MCU_STATUS_Pin);
   WRITE_REG(AI_OVERDRIVE_GPIO_Port->BRR, AI_OVERDRIVE_Pin);

   // Initialize system-wide GPIO pins
   uint32_t position = 32 - __builtin_clz(LED_MCU_STATUS_Pin) - 1;
   MODIFY_REG(LED_MCU_STATUS_GPIO_Port->OSPEEDR, (GPIO_OSPEEDR_OSPEED0 << (position * 2U)), (GPIO_SPEED_FREQ_LOW << (position * 2U)));
   MODIFY_REG(LED_MCU_STATUS_GPIO_Port->OTYPER, (GPIO_OTYPER_OT0 << position), (((GPIO_MODE_OUTPUT_PP & OUTPUT_TYPE) >> OUTPUT_TYPE_Pos) << position));
   MODIFY_REG(LED_MCU_STATUS_GPIO_Port->PUPDR, (GPIO_PUPDR_PUPD0 << (position * 2U)), (GPIO_NOPULL << (position * 2U)));
   MODIFY_REG(LED_MCU_STATUS_GPIO_Port->MODER, (GPIO_MODER_MODE0 << (position * 2U)), ((GPIO_MODE_OUTPUT_PP & GPIO_MODE) << (position * 2U)));
   position = 32 - __builtin_clz(AI_OVERDRIVE_Pin) - 1;
   MODIFY_REG(AI_OVERDRIVE_GPIO_Port->OSPEEDR, (GPIO_OSPEEDR_OSPEED0 << (position * 2U)), (GPIO_SPEED_FREQ_LOW << (position * 2U)));
   MODIFY_REG(AI_OVERDRIVE_GPIO_Port->OTYPER, (GPIO_OTYPER_OT0 << position), (((GPIO_MODE_OUTPUT_PP & OUTPUT_TYPE) >> OUTPUT_TYPE_Pos) << position));
   MODIFY_REG(AI_OVERDRIVE_GPIO_Port->PUPDR, (GPIO_PUPDR_PUPD0 << (position * 2U)), (GPIO_NOPULL << (position * 2U)));
   MODIFY_REG(AI_OVERDRIVE_GPIO_Port->MODER, (GPIO_MODER_MODE0 << (position * 2U)), ((GPIO_MODE_OUTPUT_PP & GPIO_MODE) << (position * 2U)));

#endif  // #if REV_ID < REV_C

   // Disable all unused GPIO clocks
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIOAEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIOBEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIOCEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIODEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIOEEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIOFEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIOGEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIOHEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIONEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIOOEN);
   WRITE_REG(RCC->AHB4ENCR, RCC_AHB4ENR_GPIOPEN);

   // Disable unused AHBSRAM and BKPSRAM
   WRITE_REG(RCC->MEMENCR, (LL_MEM_AHBSRAM1 | LL_MEM_AHBSRAM2 | LL_MEM_BKPSRAM));

   // Enable the instruction and data caches
   SET_BIT(MEMSYSCTL->MSCR, (MEMSYSCTL_MSCR_DCACTIVE_Msk | MEMSYSCTL_MSCR_ICACTIVE_Msk));
   SCB_EnableICache();
   SCB_EnableDCache();

   // Disable CPU deep-sleep mode
   CLEAR_BIT(SCB->SCR, SCB_SCR_SLEEPDEEP_Msk);

   // Enable the DWT cycle counter to use for timeouts
   SET_BIT(CoreDebug->DEMCR, CoreDebug_DEMCR_TRCENA_Msk);
   WRITE_REG(DWT->CYCCNT, 0);
   SET_BIT(DWT->CTRL, DWT_CTRL_CYCCNTENA_Msk);

   // Deactivate the SYSCFG clock
   (void)READ_REG(SYSCFG->INITSVTORCR);
   WRITE_REG(RCC->APB4ENCR2, RCC_APB4ENCR2_SYSCFGENC);
}

void system_reset(void)
{
   // Fully reset chip
   NVIC_SystemReset();
}

void system_finalize(void)
{
   // Disable SysTick interrupts
   CLEAR_BIT(SysTick->CTRL, SysTick_CTRL_TICKINT_Msk);

   // Enable an independent watchdog that resets if not fed within 16 seconds
   SET_BIT(DBGMCU->APB4FZ1, DBGMCU_APB4FZ1_DBG_IWDG_STOP);
   WRITE_REG(IWDG->KR, IWDG_KEY_ENABLE);
   WRITE_REG(IWDG->KR, IWDG_KEY_WRITE_ACCESS_ENABLE);
   WRITE_REG(IWDG->PR, IWDG_PRESCALER_512);
   WRITE_REG(IWDG->RLR, 1000);
   while (READ_BIT(IWDG->SR, IWDG_SR_RVU));
   WRITE_REG(IWDG->ICR, IWDG_ICR_EWIC);
   WRITE_REG(IWDG->EWCR, 0U);
   while (READ_BIT(IWDG->SR, (IWDG_SR_EWU | IWDG_SR_WVU | IWDG_SR_RVU | IWDG_SR_PVU)));
   WRITE_REG(IWDG->KR, IWDG_KEY_RELOAD);

#if REV_ID < REV_C

   // Illuminate the MCU status LED
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOBEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOBEN);
   WRITE_REG(LED_MCU_STATUS_GPIO_Port->BSRR, LED_MCU_STATUS_Pin);

#endif  // #if REV_ID < REV_C
}

void system_sleep(void)
{
   // Put the CPU to sleep until awoken by an interrupt
   __DSB();
   __WFI();
   __ISB();
}

void system_feed_watchdog(void)
{
   // Reset the independent watchdog timer
   WRITE_REG(IWDG->KR, IWDG_KEY_RELOAD);
}

void system_delay(uint32_t ms)
{
   // Resume the SysTick timer, delay, then stop the timer
   SET_BIT(SysTick->CTRL, SysTick_CTRL_TICKINT_Msk);
   const uint32_t tick_start = sys_tick, wait = ms + 1;
   while ((sys_tick - tick_start) < wait);
   CLEAR_BIT(SysTick->CTRL, SysTick_CTRL_TICKINT_Msk);
}

uint32_t system_get_tick(void)
{
   return sys_tick;
}

void system_set_risaf_default(RISAF_TypeDef *risaf)
{
  // Configure 2 fully overlapping regions, one for secure and one for non-secure accesses
  WRITE_REG(risaf->REG[0].STARTR, 0x0);
  WRITE_REG(risaf->REG[0].ENDR, get_risaf_max_addr(risaf));
  WRITE_REG(risaf->REG[0].CIDCFGR, RIF_CID_MASK | (RIF_CID_MASK << RISAF_REGx_CIDCFGR_WRENC0_Pos));
  WRITE_REG(risaf->REG[0].CFGR, RISAF_FILTER_ENABLE | (RIF_ATTRIBUTE_SEC << RISAF_REGx_CFGR_SEC_Pos));
  WRITE_REG(risaf->REG[1].STARTR, 0x0);
  WRITE_REG(risaf->REG[1].ENDR, get_risaf_max_addr(risaf));
  WRITE_REG(risaf->REG[1].CIDCFGR, RIF_CID_MASK | (RIF_CID_MASK << RISAF_REGx_CIDCFGR_WRENC0_Pos));
  WRITE_REG(risaf->REG[1].CFGR, RISAF_FILTER_ENABLE | (RIF_ATTRIBUTE_NSEC << RISAF_REGx_CFGR_SEC_Pos));
}
