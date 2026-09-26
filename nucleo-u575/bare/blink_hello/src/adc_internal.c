/**
  * @file    adc_internal.c
  * @brief   ADC1 internal-channel sampling (VREFINT / temperature / VBAT).
  *
  * ADC1's kernel clock is SYSCLK (160 MHz) divided by 4 = 40 MHz
  * (ADC_CLOCK_ASYNC_DIV4; the U5 ADC clock must stay within its datasheet
  * limit, so 40 MHz is comfortably in range). 814-cycle sample time settles
  * the internal bandgap/sensor.
  *
  * The three internal channels are on the ADC1 regular group:
  *   rank 1: VREFINT      - used to back out Vdda
  *   rank 2: temperature sensor
  *   rank 3: VBAT/4
  *
  * HAL_ADC_ConfigChannel() also enables the corresponding internal analog path
  * (VREFEN / VSENSEEN / VBATEN in ADCx_CCR), so no manual CCR handling is
  * needed.
  *
  * Note: the STM32U5 divides VBAT by 4 (the L4 divided by 3).
  */

#include "adc_internal.h"
#include "board.h"

static ADC_HandleTypeDef hadc1;

/* ------------------------------------------------------------------------ */
void ADC_Internal_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    __HAL_RCC_ADC12_CLK_ENABLE();

    /* ADC kernel clock = SYSCLK (160 MHz); the ADC divides it by 4 in the
     * ClockPrescaler below (160/4 = 40 MHz). */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADCDAC;
    PeriphClkInit.AdcDacClockSelection = RCC_ADCDACCLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }

    hadc1.Instance                    = ADC1;
    hadc1.Init.ClockPrescaler         = ADC_CLOCK_ASYNC_DIV4;   /* 160/4 = 40 MHz */
    hadc1.Init.Resolution             = ADC_RESOLUTION_12B;
    hadc1.Init.GainCompensation       = 0;
    hadc1.Init.ScanConvMode           = ADC_SCAN_ENABLE;   /* scan the 3 internal channels */
    hadc1.Init.EOCSelection           = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait       = DISABLE;
    hadc1.Init.ContinuousConvMode     = DISABLE;           /* one scan per HAL_ADC_Start */
    hadc1.Init.NbrOfConversion        = 3;
    hadc1.Init.DiscontinuousConvMode  = DISABLE;
    hadc1.Init.ExternalTrigConv       = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge   = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests  = DISABLE;
    hadc1.Init.TriggerFrequencyMode   = ADC_TRIGGER_FREQ_HIGH;
    hadc1.Init.Overrun                = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.LeftBitShift           = ADC_LEFTBITSHIFT_NONE;
    hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    hadc1.Init.OversamplingMode       = DISABLE;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    /* Calibrate the ADC before conversions. */
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
    {
        Error_Handler();
    }

    sConfig.SingleDiff   = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset       = 0;

    /* Rank 1: VREFINT. */
    sConfig.Channel      = ADC_CHANNEL_VREFINT;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_814CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    /* Rank 2: temperature sensor. */
    sConfig.Channel      = ADC_CHANNEL_TEMPSENSOR;
    sConfig.Rank         = ADC_REGULAR_RANK_2;
    sConfig.SamplingTime = ADC_SAMPLETIME_814CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    /* Rank 3: VBAT/4. */
    sConfig.Channel      = ADC_CHANNEL_VBAT;
    sConfig.Rank         = ADC_REGULAR_RANK_3;
    sConfig.SamplingTime = ADC_SAMPLETIME_814CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------------ */
void ADC_Internal_Sample(ADC_InternalResult *res)
{
    res->raw_vrefint = 0;
    res->raw_temp    = 0;
    res->raw_vbat    = 0;

    HAL_ADC_Start(&hadc1);   /* single scan of 3 conversions */

    if (HAL_ADC_PollForConversion(&hadc1, 10U) == HAL_OK)
    {
        res->raw_vrefint = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    if (HAL_ADC_PollForConversion(&hadc1, 10U) == HAL_OK)
    {
        res->raw_temp = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    if (HAL_ADC_PollForConversion(&hadc1, 10U) == HAL_OK)
    {
        res->raw_vbat = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }

    HAL_ADC_Stop(&hadc1);
}
