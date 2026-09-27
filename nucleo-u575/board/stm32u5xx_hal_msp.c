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

    /* Enable the VDDA (analog) supply. Without this the ADC analog block
     * stays unpowered: its internal regulator never becomes ready (LDORDY
     * never sets), so ADC_Enable() times out and calibration hangs. VDDA also
     * feeds the DAC/comparators/OPAMP. */
    HAL_PWREx_EnableVddA();

    /* Enable the VDDIO2 supply, which powers the PG[15:2] I/Os. LD3 (red) on
     * this board is PG2, so without this its output driver is unpowered and
     * the LED never lights even though the pin is configured as an output. */
    HAL_PWREx_EnableVddIO2();

    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_3);
}
