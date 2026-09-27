#include <stdio.h>
#include "board.h"
#include "adc_internal.h"

/* The factory calibration constants ST stores in system memory (addresses in
 * stm32u5xx_ll_adc.h) are 14-bit codes, while this demo runs the ADC at 12
 * bits. The LL helper macros below rescale the measured data to 14 bits
 * before using the constants, so use them rather than open-coding the maths:
 *   VREFINT_CAL       (0x0BFA07A5) VREFINT code at Vref+ = 3.0 V
 *   TEMPSENSOR_CAL1/2 (0x0BFA0710 / 0x0BFA0742) temp codes at 30 C / 130 C
 *
 * Scaled like that the U5 constants work out ~4x the 12-bit equivalents; a
 * naive 12-bit formula would report Vdda ~13 V instead of ~3.3 V. */

/* Chase the three board LEDs (LD1 green PC7, LD2 blue PB7, LD3 red PG2). */
static void LED_Chase(uint32_t step)
{
    LED1_OFF();
    LED2_OFF();
    LED3_OFF();

    switch (step % 3U)
    {
        case 0U: LED1_ON(); break;
        case 1U: LED2_ON(); break;
        default: LED3_ON(); break;
    }
}

int main(void)
{
    HAL_Init();
    Board_Init();
    ADC_Internal_Init();

    printf("\r\n==== nucleo-u575 (STM32U575ZIT6) blink_hello @ 160 MHz ====\r\n");
    printf("SYSCLK = %lu Hz (%lu MHz)\r\n",
           (unsigned long)SystemCoreClock,
           (unsigned long)(SystemCoreClock / 1000000UL));
    printf("FLASH_ACR latency = %lu (4 WS + ICACHE)\r\n",
           (unsigned long)(FLASH->ACR & 0xFU));

    uint32_t phase = 0;
    uint32_t last_report = 0;

    while (1)
    {
        /* Basic operation: chase the LEDs (HIGH = ON). */
        LED_Chase(phase);
        phase++;
        HAL_Delay(250);

        /* Every 4 phases (~1 s): sample the ADC1 internal channels. */
        if (phase % 4U == 0U)
        {
            uint32_t now = HAL_GetTick();
            if (now - last_report >= 1000U)
            {
                last_report = now;

                ADC_InternalResult adc;
                ADC_Internal_Sample(&adc);

                /* Actual analog supply from VREFINT (14-bit-aware). */
                uint32_t vdda_mv = __LL_ADC_CALC_VREFANALOG_VOLTAGE(
                                       ADC1, adc.raw_vrefint,
                                       LL_ADC_RESOLUTION_12B);

                /* Junction temperature from the factory-calibrated sensor. */
                int32_t temp_c = __LL_ADC_CALC_TEMPERATURE(
                                     ADC1, vdda_mv, adc.raw_temp,
                                     LL_ADC_RESOLUTION_12B);

                /* VBAT: the U5 measures VBAT/4 (internal 1/4 divider), so x4. */
                uint32_t vbat_mv = __LL_ADC_CALC_DATA_TO_VOLTAGE(
                                       ADC1, vdda_mv, adc.raw_vbat,
                                       LL_ADC_RESOLUTION_12B) * 4UL;

                printf("ADC1: VREFINT=%hu code, temp=%hu code, VBAT=%hu code\r\n",
                       adc.raw_vrefint, adc.raw_temp, adc.raw_vbat);
                printf("     Vdda ~= %lu mV, chip temp ~= %ld C, VBAT ~= %lu mV\r\n",
                       (unsigned long)vdda_mv, (long)temp_c, (unsigned long)vbat_mv);
            }
        }
    }

    return 0;
}
