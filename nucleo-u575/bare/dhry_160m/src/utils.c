#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "custom_def.h"
#include "utils.h"

//volatile uint32_t g_ticks;

//uint32_t HAL_GetTick(void) { return g_ticks; }

////#pragma clang section text="RAM_CODE"
//void HAL_Delay(uint32_t t) {
//  uint32_t d = t + g_ticks;
//  while (d > g_ticks) {
//    __NOP();
//  }
//}
