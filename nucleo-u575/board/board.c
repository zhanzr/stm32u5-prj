/**
  * @file    board.c
  * @brief   Board init for the STM32U575ZIT6 "nucleo-u575" board (NUCLEO-U575ZI-Q).
  *
  * Clock tree (MSI = 4 MHz, no external crystal needed):
  *   PLLM=1   -> PLL input    4 MHz  (PLLRGE_0 = 4..8 MHz)
  *   PLLN=80  -> VCO        320 MHz
  *   PLLR=2   -> SYSCLK     160 MHz
  *   PLLP=2   -> 160 MHz, PLLQ=2 -> 160 MHz (not used)
  *   AHB=160 MHz, APB1=160 MHz (/1), APB2=160 MHz (/1), APB3=160 MHz (/1)
  *   Flash latency 4 wait states, regulator voltage scale 1 (VOS1) on the
  *   SMPS regulator (the NUCLEO-U575ZI-Q default supply path).
  *   ICACHE is enabled in 1-way (direct-mapped) mode: at 160 MHz the core
  *   outruns flash, so instruction caching is required for usable performance.
  *
  * SystemClock_Config() is declared weak so an individual project can bring
  * in its own clock setup without affecting the default 160 MHz used by
  * every other project.
  */

#include "board.h"
#include "uart_printf.h"
#include "swv_printf.h"

/* ------------------------------------------------------------------------ */
static void SystemPower_Config(void)
{
    /* Disable the internal pull-ups in the UCPD dead-battery pins. */
    HAL_PWREx_DisableUCPDDeadBattery();

    /* NUCLEO-U575ZI-Q power supply: SMPS regulator instead of the LDO. */
    if (HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------------ */
__attribute__((weak)) void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Regulator voltage scale 1 (VOS1) is required for 160 MHz. */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
    {
        Error_Handler();
    }

    /* MSI 4 MHz -> PLL -> 160 MHz. */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState            = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.MSIClockRange       = RCC_MSIRANGE_4;   /* 4 MHz */
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_MSI;
    RCC_OscInitStruct.PLL.PLLMBOOST       = RCC_PLLMBOOST_DIV1;
    RCC_OscInitStruct.PLL.PLLM            = 1U;
    RCC_OscInitStruct.PLL.PLLN            = 80U;    /* VCO = 320 MHz */
    RCC_OscInitStruct.PLL.PLLP            = 2U;
    RCC_OscInitStruct.PLL.PLLQ            = 2U;
    RCC_OscInitStruct.PLL.PLLR            = 2U;     /* SYSCLK = 160 MHz */
    RCC_OscInitStruct.PLL.PLLRGE          = RCC_PLLVCIRANGE_0;  /* 4..8 MHz */
    RCC_OscInitStruct.PLL.PLLFRACN        = 0U;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                     | RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------------ */
static void ICACHE_Init(void)
{
    /* Direct-mapped (1-way) instruction cache; at 160 MHz the core runs
     * faster than flash, so this is required for usable performance. */
    if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_ICACHE_Enable() != HAL_OK)
    {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------------ */
static void GPIO_LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* LD1 PC7, LD2 PB7, LD3 PG2: push-pull output, HIGH = ON. Start OFF. */
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    GPIO_InitStruct.Pin = LED1_Pin;
    HAL_GPIO_Init(LED1_GPIO_Port, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = LED2_Pin;
    HAL_GPIO_Init(LED2_GPIO_Port, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = LED3_Pin;
    HAL_GPIO_Init(LED3_GPIO_Port, &GPIO_InitStruct);

    LED1_OFF();
    LED2_OFF();
    LED3_OFF();
}

/* ------------------------------------------------------------------------ */
static void GPIO_Button_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* B1 PC13: pressed shorts the pin to GND (external pull-up on the board). */
    GPIO_InitStruct.Pin  = BTN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BTN_GPIO_Port, &GPIO_InitStruct);
}

/* ------------------------------------------------------------------------ */
void Board_Init(void)
{
    SystemPower_Config();
    SystemClock_Config();
    ICACHE_Init();
    GPIO_LED_Init();
    GPIO_Button_Init();
    UART_Init();
    SWV_Init();
}

/* ------------------------------------------------------------------------ */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
        /* Blink LD3 red as a fatal-error indicator. */
        LED3_TOGGLE();
        for (volatile uint32_t i = 0; i < 1000000UL; i++) { }
    }
}
