/**
  * @file    adc_internal.h
  * @brief   ADC1 internal-channel sampling for the STM32U575 "nucleo-u575" board.
  *
  * Internal channels of the master ADC1 peripheral on the STM32U575:
  *   - VREFINT     internal reference voltage (~1.21 V)
  *   - TEMPSENSOR  temperature sensor
  *   - VBAT        VBAT/4 (internal 1/4 divider; the U5 divides by 4, not 3)
  *
  * All three sit on the ADC1 regular group, so a single scan pass reads them.
  */

#ifndef __ADC_INTERNAL_H__
#define __ADC_INTERNAL_H__

#include <stdint.h>

typedef struct {
    uint16_t raw_vrefint; /* VREFINT code */
    uint16_t raw_temp;    /* temperature sensor code */
    uint16_t raw_vbat;    /* VBAT/4 code */
} ADC_InternalResult;

void ADC_Internal_Init(void);
void ADC_Internal_Sample(ADC_InternalResult *res);

#endif /* __ADC_INTERNAL_H__ */
