/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "sht30.h"
#include "led.h"
#include "key.h"
#include "param_manager.h"
#include "power_monitor.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
static I2C_Bus_Ctx i2c1_ctx;
static OLED_Ctx oled_ctx;
static SHT30_Ctx sht30_ctx;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
    float temp, humi;
    static uint32_t oled_last_tick  = 0;
    uint32_t now_tick = 0;

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
    I2C_Bus_Init(&i2c1_ctx, &hi2c1);
    OLED_Init(&oled_ctx, &i2c1_ctx);
    SHT30_Init(&sht30_ctx, &i2c1_ctx);

    /* 第一步：先恢复阈值。必须在打开PVD中断之前完成——
   如果顺序反了（先调Power_Monitor_Init打开PVD中断，再调Param_Init），
   万一在这两行之间真的发生了掉电（概率虽然极小，但不是不可能），
   HAL_PWR_PVDCallback()里会调用Param_Save()，而这时s_param和
   s_next_write_addr都还是未初始化的垃圾值——存进Flash的会是一条
   完全无意义的数据，把之前保存的有效记录污染掉 */
    Param_Init();

    /* 第二步：阈值恢复完成、RAM状态确定之后，再打开掉电检测。
       这一行执行完，PVD中断随时可能触发——也就是说从这里开始，
       写在它后面的任何代码，都要假设"这一行跑完，下一行可能因为
       掉电根本没机会执行"，这是你们文档里"确定性完成"这个设计
       要求会一直延续下去的地方 */
    Power_Monitor_Init();


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
    {
        now_tick = HAL_GetTick();


        // 按键扫描(内部已消抖,直接调用即可)
        KeyEvent_t evt = Key_Scan();
        if (evt != KEY_EVENT_NONE)
        {
            if (evt == KEY_EVENT_PLUS)
                Param_AdjustTempHigh(3);
            if (evt == KEY_EVENT_MINUS)
                Param_AdjustTempHigh(-3);
        }


        // 非阻塞调度
        if (now_tick - oled_last_tick  >= 1000)
        {
            oled_last_tick  = now_tick;
            SHT30_Read(&sht30_ctx, &temp, &humi);

            /* 每次刷新前都重新取一次句柄，而不是缓存成外层长期持有的变量——
          以后key_handler按键调阈值会实时改动s_param，每次显示前重新取，
          才能保证OLED上显示的永远是当前最新值，不是开机那一刻的旧值 */
            const ThresholdParam_t *param = Param_GetHandle();

            OLED_Clear(&oled_ctx);

            /* 原来这行(y=0)是开机计时，现在换成温度阈值范围 */
            OLED_ShowString(&oled_ctx, 0, 0, "T:");
            OLED_ShowNum(&oled_ctx, 16, 0, param->temp_low, 0);
            OLED_ShowString(&oled_ctx, 40, 0, "-");
            OLED_ShowNum(&oled_ctx, 48, 0, param->temp_high, 0);

            /* 原来这行(y=16)是按键状态，现在换成湿度阈值范围 */
            OLED_ShowString(&oled_ctx, 0, 16, "H:");
            OLED_ShowNum(&oled_ctx, 16, 16, param->humi_low, 0);
            OLED_ShowString(&oled_ctx, 40, 16, "-");
            OLED_ShowNum(&oled_ctx, 48, 16, param->humi_high, 0);

            OLED_ShowString(&oled_ctx, 0, 32, "Temp:");
            OLED_ShowFloat(&oled_ctx, 40, 32, temp, 2);
            OLED_ShowString(&oled_ctx, 0, 48, "Humi:");
            OLED_ShowFloat(&oled_ctx, 40, 48, humi, 2);

            OLED_Refresh(&oled_ctx);


            /* 报警判断改成读真实阈值，不再用写死的temp_max/humi_max。
           这段逻辑长期看应该按你们文档里的分层设计搬进alarm_ctrl.c，
           不该一直留在main.c里，先接上验证显示效果 */
            if (temp > param->temp_high || humi > param->humi_high)
                LED_On();
            else
                LED_Off();
        }





    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
