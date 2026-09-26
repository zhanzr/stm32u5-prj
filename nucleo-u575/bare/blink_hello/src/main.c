#include <stdio.h>
#include "board.h"
#include "adc_internal.h"

/* Factory-calibrated temperature-sensor ADC values (12-bit, Vref+ = 3.0 V),
 * stored in system memory by ST (addresses from stm32u5xx_ll_adc.h):
 *   TS_CAL1   = raw ADC code at  30 C (0x0BFA0710)
 *   TS_CAL2   = raw ADC code at 130 C (0x0BFA0742)
 * VREFINT_CAL (0x0BFA07A5) is the VREFINT code at 3.0 V and is used to back
 * out the actual supply voltage. */

/* Temperature from the raw temp-sensor code, using the factory calibration.
 * The measured code is first re-scaled to the 3.0 V reference the calibration
 * was done at, then linearly interpolated between the two calibration points:
 *   T = 30 + (adc_scaled - TS_CAL1) * (TS_CAL2_TEMP - TS_CAL1_TEMP)
 *                                   / (TS_CAL2 - TS_CAL1)  */
static int TempC_FromCode(uint32_t raw_temp, uint32_t vdda_mv)
{
    uint32_t cal1 = *TEMPSENSOR_CAL1_ADDR;
    uint32_t cal2 = *TEMPSENSOR_CAL2_ADDR;

    if (cal1 == 0U || cal2 <= cal1 || vdda_mv == 0U)
    {
        return 0;   /* no valid calibration data */
    }
    /* Re-scale the measured code to the 3.0 V calibration reference. */
    uint32_t adc_scaled = (raw_temp * (uint32_t)VREFINT_CAL_VREF) / vdda_mv;
    if (adc_scaled <= cal1)
    {
        return (int)TEMPSENSOR_CAL1_TEMP;
    }
    if (adc_scaled >= cal2)
    {
        return (int)TEMPSENSOR_CAL2_TEMP;
    }
    return (int)TEMPSENSOR_CAL1_TEMP
         + (int)((adc_scaled - cal1) * (uint32_t)(TEMPSENSOR_CAL2_TEMP - TEMPSENSOR_CAL1_TEMP)
                 / (cal2 - cal1));
}

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

                /* Actual supply voltage from VREFINT:
                 *   Vdda = VREFINT_CAL_VREF * VREFINT_CAL / raw_vrefint  */
                uint32_t vdda_mv = 0;
                if (adc.raw_vrefint != 0U)
                {
                    vdda_mv = ((uint32_t)VREFINT_CAL_VREF * (*VREFINT_CAL_ADDR))
                            / adc.raw_vrefint;
                }

                /* Junction temperature from the factory-calibrated temp sensor. */
                int temp_c = TempC_FromCode(adc.raw_temp, vdda_mv);

                /* VBAT: the U5 measures VBAT/4 (internal 1/4 divider), so x4. */
                uint32_t vbat_mv = ((vdda_mv * adc.raw_vbat) / 4095UL) * 4UL;

                printf("ADC1: VREFINT=%hu code, temp=%hu code, VBAT=%hu code\r\n",
                       adc.raw_vrefint, adc.raw_temp, adc.raw_vbat);
                printf("     Vdda ~= %lu mV, chip temp ~= %d C, VBAT ~= %lu mV\r\n",
                       (unsigned long)vdda_mv, temp_c, (unsigned long)vbat_mv);
            }
        }
    }

    return 0;
}
