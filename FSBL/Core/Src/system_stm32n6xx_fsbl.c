#include "stm32n6xx.h"
#include <math.h>

#define CMSE_NS_ENTRY __attribute((cmse_nonsecure_entry))

#ifndef EXTERNAL_I2S_CLOCK_VALUE
#define EXTERNAL_I2S_CLOCK_VALUE 12288000UL
#endif

uint32_t SystemCoreClock = HSI_VALUE;

extern void *g_pfnVectors;
#define INTVECT_START ((uint32_t)&g_pfnVectors)

void SystemInit(void)
{
  /* Configure the Vector Table location -------------------------------------*/
  SCB->VTOR = INTVECT_START;

  /* RNG reset */
  RCC->AHB3RSTSR = RCC_AHB3RSTSR_RNGRSTS;
  RCC->AHB3RSTCR = RCC_AHB3RSTCR_RNGRSTC;
  /* Deactivate RNG clock */
  RCC->AHB3ENCR = RCC_AHB3ENCR_RNGENC;

  /* Clear SAU regions */
  SAU->RNR = 0;
  SAU->RBAR = 0;
  SAU->RLAR = 0;
  SAU->RNR = 1;
  SAU->RBAR = 0;
  SAU->RLAR = 0;
  SAU->RNR = 2;
  SAU->RBAR = 0;
  SAU->RLAR = 0;
  SAU->RNR = 3;
  SAU->RBAR = 0;
  SAU->RLAR = 0;
  SAU->RNR = 4;
  SAU->RBAR = 0;
  SAU->RLAR = 0;
  SAU->RNR = 5;
  SAU->RBAR = 0;
  SAU->RLAR = 0;
  SAU->RNR = 6;
  SAU->RBAR = 0;
  SAU->RLAR = 0;
  SAU->RNR = 7;
  SAU->RBAR = 0;
  SAU->RLAR = 0;

  /* System configuration setup */
  RCC->APB4ENSR2 = RCC_APB4ENSR2_SYSCFGENS;
  /* Delay after an RCC peripheral clock enabling */
  (void)RCC->APB4ENR2;

  /* Set default Vector Table location after system reset or return from Standby */
  SYSCFG->INITSVTORCR = SCB->VTOR;

  /* Enable VDDADC CLAMP */
  PWR->SVMCR3 |= PWR_SVMCR3_ASV;
  PWR->SVMCR3 |= PWR_SVMCR3_AVMEN;
  /* read back the register to make sure that the transaction has taken place */
  (void) PWR->SVMCR3;
  /* enable VREF */
  RCC->APB4ENR1 |= RCC_APB4ENR1_VREFBUFEN;

  /* RCC Fix to lower power consumption */
  RCC->APB4ENR2 |= 0x00000010UL;
  (void) RCC->APB4ENR2;
  RCC->APB4ENR2 &= ~(0x00000010UL);

  /* XSPI2 & XSPIM reset                                  */
  RCC->AHB5RSTSR = RCC_AHB5RSTSR_XSPIMRSTS | RCC_AHB5RSTSR_XSPI2RSTS;
  RCC->AHB5RSTCR = RCC_AHB5RSTCR_XSPIMRSTC | RCC_AHB5RSTCR_XSPI2RSTC;

  /* TIM2 reset */
  RCC->APB1RSTSR1 = RCC_APB1RSTSR1_TIM2RSTS;
  RCC->APB1RSTCR1 = RCC_APB1RSTCR1_TIM2RSTC;
  /* Deactivate TIM2 clock */
  RCC->APB1ENCR1 = RCC_APB1ENCR1_TIM2ENC;

  /* Deactivate GPIOG clock */
  RCC->AHB4ENCR = RCC_AHB4ENCR_GPIOGENC;

  /* Read back the value to make sure it is written before deactivating SYSCFG */
  (void) SYSCFG->INITSVTORCR;
  /* Deactivate SYSCFG clock */
  RCC->APB4ENCR2 = RCC_APB4ENCR2_SYSCFGENC;

  /* FPU settings ------------------------------------------------------------*/
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
  SCB->CPACR |= ((3UL << 20U)|(3UL << 22U));  /* set CP10 and CP11 Full Access */

  SCB_NS->CPACR |= ((3UL << 20U)|(3UL << 22U));  /* set CP10 and CP11 Full Access */
#endif
}

void SystemCoreClockUpdate(void)
{
  uint32_t sysclk = 0;
  uint32_t pllm = 0;
  uint32_t plln = 0;
  uint32_t pllfracn = 0;
  uint32_t pllp1 = 0;
  uint32_t pllp2 = 0;
  uint32_t pllcfgr, pllsource, pllbypass, ic_divider;
  float_t pllvco;

  /* Get CPUCLK source -------------------------------------------------------*/
  switch (RCC->CFGR1 & RCC_CFGR1_CPUSWS)
  {
  case 0:  /* HSI used as system clock source (default after reset) */
    sysclk = HSI_VALUE >> ((RCC->HSICFGR & RCC_HSICFGR_HSIDIV) >> RCC_HSICFGR_HSIDIV_Pos);
    break;

  case RCC_CFGR1_CPUSWS_0:  /* MSI used as system clock source */
    if (READ_BIT(RCC->MSICFGR, RCC_MSICFGR_MSIFREQSEL) == 0UL)
    {
      sysclk = MSI_VALUE;
    }
    else
    {
      sysclk = 16000000UL;
    }
    break;

  case RCC_CFGR1_CPUSWS_1:  /* HSE used as system clock source */
    sysclk = HSE_VALUE;
    break;

  case (RCC_CFGR1_CPUSWS_1 | RCC_CFGR1_CPUSWS_0):  /* IC1 used as system clock  source */
    /* Get IC1 clock source parameters */
    switch (READ_BIT(RCC->IC1CFGR, RCC_IC1CFGR_IC1SEL))
    {
    case 0:  /* PLL1 selected at IC1 clock source */
      pllcfgr = READ_REG(RCC->PLL1CFGR1);
      pllsource = pllcfgr & RCC_PLL1CFGR1_PLL1SEL;
      pllbypass = pllcfgr & RCC_PLL1CFGR1_PLL1BYP;
      if (pllbypass == 0U)
      {
        pllm = (pllcfgr & RCC_PLL1CFGR1_PLL1DIVM) >>  RCC_PLL1CFGR1_PLL1DIVM_Pos;
        plln = (pllcfgr & RCC_PLL1CFGR1_PLL1DIVN) >>  RCC_PLL1CFGR1_PLL1DIVN_Pos;
        pllfracn = READ_BIT(RCC->PLL1CFGR2, RCC_PLL1CFGR2_PLL1DIVNFRAC) >>  RCC_PLL1CFGR2_PLL1DIVNFRAC_Pos;
        pllcfgr = READ_REG(RCC->PLL1CFGR3);
        pllp1 = (pllcfgr & RCC_PLL1CFGR3_PLL1PDIV1) >>  RCC_PLL1CFGR3_PLL1PDIV1_Pos;
        pllp2 = (pllcfgr & RCC_PLL1CFGR3_PLL1PDIV2) >>  RCC_PLL1CFGR3_PLL1PDIV2_Pos;
      }
      break;
    case RCC_IC1CFGR_IC1SEL_0:  /* PLL2 selected at IC1 clock source */
      pllcfgr = READ_REG(RCC->PLL2CFGR1);
      pllsource = pllcfgr & RCC_PLL2CFGR1_PLL2SEL;
      pllbypass = pllcfgr & RCC_PLL2CFGR1_PLL2BYP;
      if (pllbypass == 0U)
      {
        pllm = (pllcfgr & RCC_PLL2CFGR1_PLL2DIVM) >>  RCC_PLL2CFGR1_PLL2DIVM_Pos;
        plln = (pllcfgr & RCC_PLL2CFGR1_PLL2DIVN) >>  RCC_PLL2CFGR1_PLL2DIVN_Pos;
        pllfracn = READ_BIT(RCC->PLL2CFGR2, RCC_PLL2CFGR2_PLL2DIVNFRAC) >>  RCC_PLL2CFGR2_PLL2DIVNFRAC_Pos;
        pllcfgr = READ_REG(RCC->PLL2CFGR3);
        pllp1 = (pllcfgr & RCC_PLL2CFGR3_PLL2PDIV1) >>  RCC_PLL2CFGR3_PLL2PDIV1_Pos;
        pllp2 = (pllcfgr & RCC_PLL2CFGR3_PLL2PDIV2) >>  RCC_PLL2CFGR3_PLL2PDIV2_Pos;
      }
      break;

    case RCC_IC1CFGR_IC1SEL_1:  /* PLL3 selected at IC1 clock source */
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

    default: /* RCC_IC1CFGR_IC1SEL_1 | RCC_IC1CFGR_IC1SEL_0 */  /* PLL4 selected at IC1 clock source */
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

    /* Get oscillator frequency used as PLL clock source */
    switch (pllsource)
    {
    case 0:  /* HSI selected as PLL clock source */
      sysclk = HSI_VALUE >> ((RCC->HSICFGR & RCC_HSICFGR_HSIDIV) >> RCC_HSICFGR_HSIDIV_Pos);
      break;
    case RCC_PLL1CFGR1_PLL1SEL_0: /* MSI selected as PLL clock source */
      if (READ_BIT(RCC->MSICFGR, RCC_MSICFGR_MSIFREQSEL) == 0UL)
      {
        sysclk = MSI_VALUE;
      }
      else
      {
        sysclk = 16000000UL;
      }
      break;
    case RCC_PLL1CFGR1_PLL1SEL_1: /* HSE selected as PLL clock source */
      sysclk = HSE_VALUE;
      break;
    case (RCC_PLL1CFGR1_PLL1SEL_1 | RCC_PLL1CFGR1_PLL1SEL_0):  /* I2S_CKIN selected as PLL clock source */
      sysclk = EXTERNAL_I2S_CLOCK_VALUE;
      break;
    default:
      /* Nothing to do, should not occur */
      break;
    }

    /* Check whether PLL is in bypass mode or not */
    if (pllbypass == 0U)
    {
      /* Compte PLL output frequency (Integer and fractional modes) */
      /* PLLVCO = (Freq * (DIVN + (FRACN / 0x1000000) / DIVM) / (DIVP1 * DIVP2)) */
      pllvco = ((float_t)sysclk * ((float_t)plln + ((float_t)pllfracn/(float_t)0x1000000UL))) / (float_t)pllm;
      sysclk = (uint32_t)((float_t)(pllvco/(((float_t)pllp1) * ((float_t)pllp2))));
    }
    /* Apply IC1 divider */
    ic_divider = (READ_BIT(RCC->IC1CFGR, RCC_IC1CFGR_IC1INT) >> RCC_IC1CFGR_IC1INT_Pos) + 1UL;
    sysclk = sysclk / ic_divider;
    break;
  default:
    /* Nothing to do, should not occur */
    break;
  }

  /* Return system clock frequency (CPU frequency) */
  SystemCoreClock = sysclk;
}

CMSE_NS_ENTRY uint32_t SECURE_SystemCoreClockUpdate(void)
{
  SystemCoreClockUpdate();
  return SystemCoreClock;
}
