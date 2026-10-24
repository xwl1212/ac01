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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "string.h"
#include "stdio.h"
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

uint8_t rx_byte;
volatile uint8_t rx_flag=0;//电脑记号
volatile uint8_t btn_flag=0;//按键记号
volatile uint32_t led1_off=0;//pa1 该在几毫秒灭（按键控制）
volatile uint32_t led2_off=0;//pa2 该在几毫秒灭（电脑串口控制）


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

//申明函数
void led_on(GPIO_TypeDef *port,uint16_t pin);
void led_off(GPIO_TypeDef *port,uint16_t pin);
void timer_start(volatile uint32_t *deadline,uint32_t ms);
uint8_t timer_expired(volatile uint32_t *deadline);
void uart_send(const char *text);
void uart_echo(uint8_t ch);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin);


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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

//第一次唤醒中断
		HAL_UART_Receive_IT(&huart1,&rx_byte,1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		//电脑发来数据的后续操作
		if(rx_flag==1)
		{
			uint8_t ch = rx_byte;
			rx_flag = 0;
			
			led_on(GPIOA,GPIO_PIN_2);
			timer_start(&led2_off,1000);
			uart_echo(ch);
		}
		//按键后续操作
		if(btn_flag == 1)
		{
			btn_flag = 0;
			led_on(GPIOA,GPIO_PIN_1);
			timer_start(&led1_off,1000);
			uart_send("button pressed\r\n");
		}
		//灭灯，两个一起查一次，查到哪个灭哪个
		if(timer_expired(&led1_off))
		{
			led_off(GPIOA,GPIO_PIN_1);
		}
		if(timer_expired(&led2_off))
		{
			led_off(GPIOA,GPIO_PIN_2);
		}
		HAL_Delay(10);
		
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

//函数本体书写区域
//printf重写
int fputc(int ch,FILE *f)
{
	HAL_UART_Transmit(&huart1,(uint8_t *)&ch,1,HAL_MAX_DELAY);
	return ch;
}

//开灯函数（共阳极，低电平亮）
void led_on(GPIO_TypeDef *port,uint16_t pin)
{
	HAL_GPIO_WritePin(port,pin,GPIO_PIN_RESET);
}
//关灯，高电平灭
void led_off(GPIO_TypeDef *port,uint16_t pin)
{
	HAL_GPIO_WritePin(port,pin,GPIO_PIN_SET);
}
//计时：登记“现在”+“要计数到的时间”，从现在的时间开始计时不是从0也不是倒计时，没有返回的值，是吧赋的值直接给变量地址了无需传回，还在原地址
void timer_start(volatile uint32_t *deadline,uint32_t ms)
{
	*deadline = HAL_GetTick()+ms;
}
//计时，时间计时到1清零，防止0的重复多一毫秒,利用while（1）循环来循环，所以只if，用for的话会阻塞
uint8_t timer_expired(volatile uint32_t *deadline)
{
    if (*deadline != 0 && HAL_GetTick() >= *deadline)
    {
        *deadline = 0;
        return 1;
    }
    return 0;
}
//发送固定文字用于案件的显示
void uart_send(const char *text)
{
	printf("%s",text);
}
//回显一个字节，+换行，用于电脑发送来的数据
void uart_echo(uint8_t ch)
{
	uint8_t crlf[2]={'\r','\n'};
	HAL_UART_Transmit(&huart1,&ch,1,HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart1,crlf,2,HAL_MAX_DELAY);
}
//中断，串口的接受加回调，然后续上唤醒
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance == USART1)
	{
		rx_flag=1;
		HAL_UART_Receive_IT(&huart1,&rx_byte,1);
	}
}
//中断，按键的回调，加发送一个数据，然后续上唤醒
void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
	if(gpio_pin == GPIO_PIN_0)
	{
		btn_flag=1;
	}
}

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
