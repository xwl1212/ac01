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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "stdio.h"
#include <string.h>   /* strcmp / strncmp：用来比较命令字符串 */
#include <stdlib.h>   /* atoi：把 "80" 这种文本转成整数 80 */
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
#define RX_SIZE 128	//环形缓存的定长，防止后续更改会出现不一致报错
uint8_t rx_byte;                  // HAL 接收的临时存放（过路站）电脑传来的单个存储，转环形缓存
uint8_t rx_buf[RX_SIZE];          // 环形缓冲区
volatile uint16_t rx_head = 0;    // 写指针（只有中断改）
volatile uint16_t rx_tail = 0;    // 读指针（只有主循环改）
volatile uint32_t rx_drop = 0;   // 丢了多少字节，环形缓存丢的时候用来计数的
volatile uint32_t tick_count = 0;   // TIM3 中断里加一

//volatile uint8_t rx_flag=0;//电脑记号
volatile uint8_t btn_flag=0;//按键记号

volatile uint32_t led1_off=0;//pa1 该在几毫秒灭（按键控制）
volatile uint32_t led2_off=0;//pa2 该在几毫秒灭（电脑串口控制）

//pwm的电脑输入然后来控制
#define CMD_MAX 24                  /* 一条命令最多 24 字符：够 "SET 100"，也不会白占 RAM */
char    cmd_buf[CMD_MAX];           /* 字符仓库：中断每来 1 字节，就往里塞一个 */
uint8_t cmd_len = 0;                /* 仓库里现在攒了几个字符，0 表示空的 */
volatile uint8_t pwm_mode = 0;      /* 0=呼吸 1=固定 2=灭；主循环每轮都看它，决定该干啥 */
int32_t pwm_fixed = 200;            /* SET 算出来的 CCR 值，默认先给个 80% 亮的档 */

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

//
static void handle_cmd(const char *s);


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
	int32_t ccr = 1000;   /* 当前比较值，初始 1000 = 全灭 */
	int8_t  dir = -1;    /* 变化方向：-1 往 0 走（变亮），+1 往 1000 走（变暗） */
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
  MX_TIM3_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);              /* 放行计数器，PA1 开始输出 PWM */
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 1000);     /* 上电先给全灭，避免一亮到底 */

//第一次唤醒中断
		HAL_UART_Receive_IT(&huart1,&rx_byte,1);
		HAL_TIM_Base_Start_IT(&htim3);//启动定时器

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		
		// 串口数据处理：缓冲里有多少取多少
		while (rx_tail != rx_head)
		{
				uint8_t ch = rx_buf[rx_tail];
            /* ==== 命令解析：把碎片字节拼成一条命令 ==== */
            if (ch == '\r' || ch == '\n')
            {
                /* 收到回车/换行 = 一条命令说完了，可以开工 */
                if (cmd_len > 0)              /* 连按两次回车会来空行，空的直接丢掉 */
                {
                    cmd_buf[cmd_len] = '\0';  /* C 字符串必须以 '\0' 结尾，否则 strcmp 读不到尽头 */
                    handle_cmd(cmd_buf);      /* 交给解析器 */
                    cmd_len = 0;              /* 清空仓库，准备收下一条 */
                }
            }
            else if (cmd_len < CMD_MAX - 1)   /* 留 1 格给结尾的 '\0'，防写溢出踩坏别的变量 */
            {
                cmd_buf[cmd_len++] = ch;      /* 先存字符，长度再 +1 */
            }
									rx_tail = (rx_tail + 1) % RX_SIZE;   /* 读完必须推进读指针，否则同一字节被反复读，死循环 */
		}
		
		//电脑发来数据的后续操作
/*		if(rx_flag==1)
		{
			uint8_t ch = rx_byte;
			rx_flag = 0;
			
			led_on(GPIOA,GPIO_PIN_2);
			timer_start(&led2_off,1000);
			uart_echo(ch);
		}
*/
		//按键后续操作
		if(btn_flag == 1)
		{
			btn_flag = 0;
			led_on(GPIOA,GPIO_PIN_3);
			timer_start(&led1_off,1000);
			uart_send("button pressed\r\n");
		}
		//灭灯，两个一起查一次，查到哪个灭哪个
		if(timer_expired(&led1_off))
		{
			led_off(GPIOA,GPIO_PIN_3);
		}
		if(timer_expired(&led2_off))
		{
			led_off(GPIOA,GPIO_PIN_2);
		}
		
		// 只在计数变化时打印，否则每秒会刷 100 行
		static uint32_t last_tick = 0;
		if (tick_count != last_tick)
		{
			last_tick = tick_count;
			printf("tick=%u\r\n", tick_count);
		}
		
		//pwm呼吸灯
            /* ==== 根据 pwm_mode 决定这一轮往 CCR 写什么 ==== */
            if (pwm_mode == 0)                          /* 呼吸：每轮微调一点，慢慢变亮变暗 */
            {
                ccr += dir * 5;                        /* 每轮走 5 档，配合下面的 10ms 约 2 秒半程 */
                if (ccr <= 0)   { ccr = 0;   dir = 1;  }  /* 到最亮就掉头往回（变暗） */
                if (ccr >= 1000) { ccr = 1000; dir = -1; }  /* 到最暗就掉头往回（变亮） */
                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, (uint32_t)ccr);
            }
            else if (pwm_mode == 1)                    /* 固定：直接把算好的值写进去 */
            {
                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, (uint32_t)pwm_fixed);
            }
            else                                       /* 灭：写 1000 = 全程高电平 = 共阳极全灭 */
            {
                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 1000);
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
//发送固定文字用于文字的显示，现已改不会换行
void uart_send(const char *text)
{
	printf("%s",text);
}
//回显一个字节，用于电脑发送来的数据
void uart_echo(uint8_t ch)
{
    HAL_UART_Transmit(&huart1, &ch, 1, HAL_MAX_DELAY);
}
//中断，串口的接受加回调，然后续上唤醒
/*
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance == USART1)
	{
		rx_flag=1;
		HAL_UART_Receive_IT(&huart1,&rx_byte,1);
	}
}
*/
//中断，按键的回调，加发送一个数据，然后续上唤醒
void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
	if(gpio_pin == GPIO_PIN_0)
	{
		btn_flag=1;
	}
}

//中断，串口的接受加回调，然后续上唤醒,环形缓存保证数据的可以多字节接收到电脑的发送
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uint16_t next = (rx_head + 1) % RX_SIZE;

        if (next != rx_tail)        // 没满，才存
        {
            rx_buf[rx_head] = rx_byte;// ① 存进缓冲区
            rx_head = next;// ② 指针前进并绕回
        }
        else
        {
            rx_drop++;              // 满了，丢弃这个新字节
        }

        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);// ③ 立刻重挂
    }
}

// TIM3 到点（每 500ms）进这里
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM3)
	{
		tick_count++;
	}
}

/* 解析一条完整命令：
 *   SET 80    固定亮度 80%
 *   BREATH    恢复呼吸
 *   STOP      全灭
 */
static void handle_cmd(const char *s)
{
	/* 只比较前 4 个字符 "SET "：后面还带数字，整串 strcmp 永远不相等。
	   空格不能省，少了 "SETTING" 也会被误判成 SET */
	if (strncmp(s, "SET ", 4) == 0)
	{
		int v = atoi(s + 4);        /* s+4 跳过 "SET " 四字节，正好指到数字开头；atoi 转整数 */

		if (v < 0)   v = 0;         /* atoi 遇到非法输入返回 0 而不报错，所以要自己夹住范围 */
		if (v > 100) v = 100;

		/* 1) 用户说的是"亮度"，CCR 记的是"暗度"，所以要 100-v
		   2) 乘 10 把 0~100 铺满 0~1000，才用尽 ARR 的分辨率；
		      不乘的话 SET 80 会算成 CCR 20，只有 2% 亮 */
		pwm_fixed = (100 - v) * 10;
		if (pwm_fixed > 999) pwm_fixed = 999;   /* SET 0 会算成 1000，超出 ARR 就夹回来 */
		pwm_mode  = 1;              /* 切到固定模式，主循环下一轮就拿 pwm_fixed 去写 CCR */
		printf("OK SET %d\r\n", v); /* 回显：给你一个"确实解析成功"的证据 */
	}
	else if (strcmp(s, "BREATH") == 0)   /* 无参数，可以整串精确比较 */
	{
		pwm_mode = 0;
		printf("OK BREATH\r\n");
	}
	else if (strcmp(s, "STOP") == 0)
	{
		pwm_mode = 2;
		printf("OK STOP\r\n");
	}
	else                                 /* 拼错、未知命令 */
	{
		printf("ERR unknown\r\n");
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
