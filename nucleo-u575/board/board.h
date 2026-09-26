/**
  * @file    board.h
  * @brief   Board support for the STM32U575ZIT6 "nucleo-u575" board
  *          (NUCLEO-U575ZI-Q, STM32U575ZIT6 in LQFP144).
  *
  * LEDs:    LD1 green  PC7,  LD2 blue PB7,  LD3 red PG2 - all HIGH-active
  *          (the NUCLEO drives them directly; HIGH = ON).
  * Button:  B1 user, PC13, active-low (pressed = GND).
  * Console: USART1 PA9 (TX) / PA10 (RX), AF7, 115200 8-N-1, wired to the
  *          ST-Link virtual COM port (VCP).
  */

#ifndef __BOARD_H__
#define __BOARD_H__

#include "stm32u5xx_hal.h"

/* --- LEDs (high-active) ---------------------------------------------------- */
#define LED1_Pin        GPIO_PIN_7
#define LED1_GPIO_Port  GPIOC
#define LED2_Pin        GPIO_PIN_7
#define LED2_GPIO_Port  GPIOB
#define LED3_Pin        GPIO_PIN_2
#define LED3_GPIO_Port  GPIOG

#define LED1_ON()       HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET)
#define LED1_OFF()      HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET)
#define LED1_TOGGLE()   HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin)

#define LED2_ON()       HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET)
#define LED2_OFF()      HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET)
#define LED2_TOGGLE()   HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin)

#define LED3_ON()       HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET)
#define LED3_OFF()      HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET)
#define LED3_TOGGLE()   HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin)

/* --- User button (B1 = PC13, pressed -> GND) ------------------------------ */
#define BTN_Pin         GPIO_PIN_13
#define BTN_GPIO_Port   GPIOC

#define BTN_PRESSED()   (HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin) == GPIO_PIN_RESET)

/* --- Init ------------------------------------------------------------------ */
void Board_Init(void);          /* SMPS/VOS1, 160 MHz, ICACHE, LEDs, button, console (USART1 PA9/PA10) */
void SystemClock_Config(void);  /* MSI 4 MHz -> PLL -> 160 MHz (weak: a project may override) */
void Error_Handler(void);

#endif /* __BOARD_H__ */
