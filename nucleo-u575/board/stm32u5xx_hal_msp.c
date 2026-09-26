/**
  * @file    stm32u5xx_hal_msp.c
  * @brief   HAL MSP (MCU Support Package) callbacks.
  *
  * The board layer configures each peripheral's GPIO/clock itself (see
  * board.c and uart_printf.c), so only the global MSP is needed here.
  */

#include "stm32u5xx_hal.h"

/**
  * @brief  Initializes the global MSP.
  */
void HAL_MspInit(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();

    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_3);
}
