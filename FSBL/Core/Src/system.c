// Header Inclusions ---------------------------------------------------------------------------------------------------

#include <errno.h>
#include <sys/stat.h>
#include <sys/times.h>
#include "system.h"


// C Standard Library Replacement Functions ----------------------------------------------------------------------------

static uint8_t *__sbrk_heap_end = NULL;

extern int __io_putchar(int ch) __attribute__((weak));
extern int __io_getchar(void) __attribute__((weak));

char *__env[1] = { 0 };
char **environ = __env;

void initialise_monitor_handles() {}
int _getpid(void) { return 1; }
int _kill(int pid, int sig) { errno = EINVAL; return -1; }
void _exit (int status) { _kill(status, -1); while(1); }
__attribute__((weak)) int _read(int file, char *ptr, int len) { for (int idx = 0; idx < len; ++idx) *ptr++ = __io_getchar(); return len; }
__attribute__((weak)) int _write(int file, char *ptr, int len) { for (int idx = 0; idx < len; ++idx) __io_putchar(*ptr++); return len; }
int _close(int file) { return -1; }
int _fstat(int file, struct stat *st) { st->st_mode = S_IFCHR; return 0; }
int _isatty(int file) { return 1; }
int _lseek(int file, int ptr, int dir) { return 0; }
int _open(char *path, int flags, ...) { return -1; }
int _wait(int *status) { errno = ECHILD; return -1; }
int _unlink(char *name) { errno = ENOENT; return -1; }
int _times(struct tms *buf) { return -1; }
int _stat(char *file, struct stat *st) { st->st_mode = S_IFCHR; return 0; }
int _link(char *old, char *new) { errno = EMLINK; return -1; }
int _fork(void) { errno = EAGAIN; return -1; }
int _execve(char *name, char **argv, char **env) { errno = ENOMEM; return -1; }

void *_sbrk(ptrdiff_t incr)
{
   // Symbols defined in the linker script
   extern uint8_t _end;
   extern uint8_t _estack;
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
// TODO: Should probably reboot if any of these occur (or alternately use a watchdog to auto-restart)

void NMI_Handler(void) { while(1); }
void HardFault_Handler(void) { while (1); }
void MemManage_Handler(void) { while (1); }
void BusFault_Handler(void) { while (1); }
void UsageFault_Handler(void) { while (1); }
void SecureFault_Handler(void) { while (1); }
void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}
void SysTick_Handler(void) { HAL_IncTick(); }
void Error_Handler(void) { __disable_irq(); while (1); }


// Global Interrupt Service Routines -----------------------------------------------------------------------------------

void EXTI12_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_12); }
void EXTI15_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_15); }

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
  switch (GPIO_Pin)
  {
    case SD_CARD_DETECT_Pin:
      sd_card_detection_isr(1);
      break;
    case DATA_IN_CS_Pin:
      comms_spi_cs_isr();
      break;
    default:
      break;
  }
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  switch (GPIO_Pin)
  {
    case SD_CARD_DETECT_Pin:
      sd_card_detection_isr(0);
      break;
    default:
      break;
  }
}


// Public API Functions ------------------------------------------------------------------------------------------------

void chip_reset(void)
{
   // Fully reset chip
   NVIC_SystemReset();
}

void chip_initialize_unused_pins(void)
{
   // Enable all GPIO clocks
   __HAL_RCC_GPIOA_CLK_ENABLE();
   __HAL_RCC_GPIOB_CLK_ENABLE();
   __HAL_RCC_GPIOC_CLK_ENABLE();
   __HAL_RCC_GPIOD_CLK_ENABLE();
   __HAL_RCC_GPIOE_CLK_ENABLE();
   __HAL_RCC_GPIOF_CLK_ENABLE();
   __HAL_RCC_GPIOG_CLK_ENABLE();
   __HAL_RCC_GPIOH_CLK_ENABLE();
   __HAL_RCC_GPION_CLK_ENABLE();
   __HAL_RCC_GPIOO_CLK_ENABLE();
   __HAL_RCC_GPIOP_CLK_ENABLE();

   // Set all unused pins an analog input with pull-down enabled
   const gpio_pin_t unused_pins[] = UNUSED_PINS;
   GPIO_InitTypeDef GPIO_InitStruct = { .Mode = GPIO_MODE_ANALOG, .Pull = GPIO_PULLDOWN, .Speed = GPIO_SPEED_FREQ_LOW };
   for (uint32_t i = 0; i < (sizeof(unused_pins) / sizeof(unused_pins[0])); ++i)
   {
     GPIO_InitStruct.Pin = unused_pins[i].pin;
     HAL_GPIO_Init(unused_pins[i].port, &GPIO_InitStruct);
   }

   // Disable all GPIO clocks
   __HAL_RCC_GPIOA_CLK_DISABLE();
   __HAL_RCC_GPIOB_CLK_DISABLE();
   __HAL_RCC_GPIOC_CLK_DISABLE();
   __HAL_RCC_GPIOD_CLK_DISABLE();
   __HAL_RCC_GPIOE_CLK_DISABLE();
   __HAL_RCC_GPIOF_CLK_DISABLE();
   __HAL_RCC_GPIOG_CLK_DISABLE();
   __HAL_RCC_GPIOH_CLK_DISABLE();
   __HAL_RCC_GPION_CLK_DISABLE();
   __HAL_RCC_GPIOO_CLK_DISABLE();
   __HAL_RCC_GPIOP_CLK_DISABLE();
}

void cpu_init(void)
{
}

void cpu_sleep(void)
{
   // Put the CPU to sleep until awoken by an interrupt
   __DSB();
   __WFI();
   __ISB();
}

uint32_t system_start_execution_timer(void)
{
   SET_BIT(CoreDebug->DEMCR, CoreDebug_DEMCR_TRCENA_Msk);
   WRITE_REG(DWT->CYCCNT, 0);
   SET_BIT(DWT->CTRL, DWT_CTRL_CYCCNTENA_Msk);
   return DWT->CYCCNT;
}

uint32_t system_get_execution_time_ms(uint32_t start_count)
{
   const uint32_t execution_time = (uint32_t)((uint64_t)(DWT->CYCCNT - start_count) * 1000 / SystemCoreClock);
   CLEAR_BIT(DWT->CTRL, DWT_CTRL_CYCCNTENA_Msk);
   CLEAR_BIT(CoreDebug->DEMCR, CoreDebug_DEMCR_TRCENA_Msk);
   return execution_time;
}
