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
#include "adc.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "bsp_uart.h"
#include "oled.h"
#include "app_adc.h"
#include "app_cmd.h"
#include "bsp_key.h"
#include "app_power.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

uint16_t g_adc_raw = 0;     /* 最近一次原始采样值 0~4095 */
uint16_t g_adc_avg = 0;     /* 8 点滑动平均后的值 */
uint32_t g_last_ms = 0;     /* 上次刷新的时刻（HAL_GetTick 的毫秒计数） */
uint8_t  g_last_pct = 255;  /* 上次画进度条时的百分比，用来避免无意义的重复传输 */
uint8_t  g_last_page = 0;   /* 上次绘制的页号，页号变了要清屏重画 */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*
 * 显示 1~3 位的百分数，右对齐。
 * 为什么要先写三个空格：百分比会从 100 掉到 45，不擦掉高位的话屏上会留个 "1"。
 */
static void ShowPercent(uint8_t x, uint8_t page, uint8_t pct)
{
  OLED_ShowString(x, page, "   ");
  if (pct >= 100)
  {
    OLED_ShowNum(x, page, pct, 3);
  }
  else if (pct >= 10)
  {
    OLED_ShowNum(x + 8, page, pct, 2);
  }
  else
  {
    OLED_ShowNum(x + 16, page, pct, 1);
  }
}

/*
 * 画当前页的固定部分（标题和文字标签）。
 * 页 1 是给人看的主界面，页 2 是给调试看的数据界面。
 */
static void DrawFrame(void)
{
  if (g_page == 1)
  {
    OLED_ShowString(0, 0, "ENV MONITOR");
    OLED_ShowString(0, 2, "LIGHT");
    OLED_ShowString(72, 2, "%");
    OLED_ShowString(32, 4, "V");
    OLED_ShowBar(6, 0);
  }
  else
  {
    OLED_ShowString(0, 0, "ENV DATA");
    OLED_ShowString(0, 2, "RAW");
    OLED_ShowString(0, 4, "AVG");
    OLED_ShowString(0, 6, "V");
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

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
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

  printf("\r\nboot\r\n");

  OLED_Init();                /* 初始化屏幕 */
  App_AdcInit();              /* ADC 校准一次 */
  BspUart_Init();             /* 打开串口中断接收 */
  Cmd_Init();                 /* 页号回到第 1 页 */
  Power_Init();               /* 低功耗计时从这里开始 */

  DrawFrame();                /* 画第 1 页的固定框架 */
  g_last_page = g_page;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* 每轮都来取一次串口字节，不阻塞，有命令就执行 */
    Cmd_Process();

    /*
     * 空闲满 1 分钟就关屏进 STOP，靠 PB0 的下降沿唤醒。
     * 唤醒回来屏是空的，把 g_last_page 置 0 让下面的重画分支生效。
     */
    if (Power_Task())
    {
      g_last_page = 0;
      g_last_pct  = 255;
      g_last_ms   = HAL_GetTick();
      continue;
    }

    /* 按键按下就翻页，和串口 PAGE 命令走同一个入口 */
    if (BspKey_Scan())
    {
      Cmd_NextPage();
      Power_TouchActivity();
    }

    /* 页号被命令改了，清屏重画固定框架 */
    if (g_page != g_last_page)
    {
      g_last_page = g_page;
      g_last_pct  = 255;          /* 强制重画进度条 */
      OLED_Clear();
      DrawFrame();
    }

    /* 每 200ms 采一次、刷一次。用 HAL_GetTick 时间片而不是 HAL_Delay，
       这样主循环一直是"活的"，串口命令随时能响应。 */
    if ((uint32_t)(HAL_GetTick() - g_last_ms) >= 200)
    {
      uint16_t mv;
      uint8_t pct;

      g_last_ms += 200;

      g_adc_raw = App_AdcReadRaw();                 /* 原始值 */
      g_adc_avg = App_AdcSmooth(g_adc_raw);         /* 滤波后的值 */
      mv  = (uint16_t)((uint32_t)g_adc_avg * APP_ADC_VREF / APP_ADC_FULL);
      pct = App_AdcPercent(g_adc_avg);               /* 实测区间换算，越亮百分比越大 */

      if (g_page == 1)
      {
        const char *level;

        if (pct < 15)
        {
          level = "DARK  ";
        }
        else if (pct < 60)
        {
          level = "NORMAL";
        }
        else
        {
          level = "BRIGHT";
        }

        ShowPercent(48, 2, pct);                     /* LIGHT  xx% */

        OLED_ShowNum(0, 4, mv / 1000, 1);            /* 电压整数部分 */
        OLED_ShowChar(8, 4, '.');
        OLED_ShowNum(16, 4, (mv % 1000) / 10, 2);    /* 两位小数 */
        OLED_ShowString(64, 4, level);               /* 等级文字 */

        /* 百分比没变就不重画进度条，省掉一次 128 字节的 I2C 传输 */
        if (pct != g_last_pct)
        {
          g_last_pct = pct;
          OLED_ShowBar(6, (uint8_t)((uint32_t)pct * 128 / 100));
        }
      }
      else
      {
        OLED_ShowNum(32, 2, g_adc_raw, 4);           /* RAW 0~4095 */
        OLED_ShowNum(32, 4, g_adc_avg, 4);           /* AVG 0~4095 */
        OLED_ShowNum(16, 6, mv / 1000, 1);
        OLED_ShowChar(24, 6, '.');
        OLED_ShowNum(32, 6, (mv % 1000) / 10, 2);
      }
    }

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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
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

#ifdef  USE_FULL_ASSERT
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
