/**
  * @file    swv_printf.h
  * @brief   SWV/ITM (SWO) printf support for the STM32U575ZIT6 "nucleo-u575" board.
  *
  * SWO is available on TRACESWO = PB3 (AF0). The console on this board is the
  * UART; SWV/ITM is enabled mainly to keep the DWT cycle counter running (it is
  * also usable for profiling). When no debugger is attached the writes are
  * dropped (no blocking).
  */

#ifndef __SWV_PRINTF_H__
#define __SWV_PRINTF_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SWV_Init(void);        /* enable DWT + ITM, stimulus port 0 */
int  SWV_PutChar(int ch);   /* non-blocking ITM port-0 write (0x00 drop) */

#ifdef __cplusplus
}
#endif

#endif /* __SWV_PRINTF_H__ */
