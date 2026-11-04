#include "app_power.h"
#include "bsp_key.h"
#include "bsp_uart.h"
#include "app_adc.h"
#include "oled.h"
#include "i2c.h"
#include "adc.h"
#include "usart.h"

/* 这些只在 main.c / CubeMX 生成的文件里定义，这里直接引用 */
extern void SystemClock_Config(void);
extern I2C_HandleTypeDef  hi2c1;
extern ADC_HandleTypeDef  hadc1;
extern UART_HandleTypeDef huart1;

static uint32_t s_last_active;      /* 最近一次"有操作"的时刻 */

void Power_Init(void)
{
  __HAL_RCC_PWR_CLK_ENABLE();       /* 进 STOP 要写 PWR 的寄存器，必须先把它的时钟打开 */
  s_last_active = HAL_GetTick();
}

void Power_TouchActivity(void)
{
  s_last_active = HAL_GetTick();
}

/*
 * EXTI0 的中断回调。中断里只更新计时，不做任何耗时动作。
 * 这个回调同时承担两件事：睡眠时产生唤醒事件；清醒时按键盘也刷新空闲计时。
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == KEY_PIN)
  {
    s_last_active = HAL_GetTick();
  }
}

uint8_t Power_Task(void)
{
  /* 还没到 1 分钟，什么都不做 */
  if ((uint32_t)(HAL_GetTick() - s_last_active) < POWER_IDLE_TIMEOUT_MS)
  {
    return 0;
  }

  /*
   * 安全阀：EXTI0 中断没使能的话，进了 STOP 就没人能把它叫醒，
   * 只能按复位键。这种情况干脆不睡。
   */
  if ((EXTI->IMR & (1U << 0)) == 0)
  {
    Power_TouchActivity();
    return 0;
  }

  /* 按键正按着的时候别睡：要等松手再按下才会产生下降沿，否则会一直睡下去 */
  if (HAL_GPIO_ReadPin(KEY_PORT, KEY_PIN) == GPIO_PIN_RESET)
  {
    Power_TouchActivity();
    return 0;
  }

  /* 清掉可能还挂着的 EXTI 标志，不然刚进 STOP 就会被这次旧事件立刻唤醒 */
  __HAL_GPIO_EXTI_CLEAR_IT(KEY_PIN);

  OLED_DisplayOff();                /* 屏先关，省掉 OLED 那部分电流 */

  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
  /* ↑ 程序在这里停住，直到 PB0 的下降沿唤醒 */

  /*
   * 唤醒后 STOP 把 HSE+PLL 关掉了，系统时钟自动回到 HSI 8MHz。
   * 所有按 72MHz 配好的外设都得按新时钟重来一遍。
   */
  SystemClock_Config();

  HAL_I2C_DeInit(&hi2c1);
  MX_I2C1_Init();
  HAL_ADC_DeInit(&hadc1);
  MX_ADC1_Init();
  App_AdcInit();                    /* 重新校准，ADC 时钟也变了 */
  HAL_UART_DeInit(&huart1);
  MX_USART1_UART_Init();
  BspUart_Init();                   /* 重新挂上串口接收中断 */

  OLED_Init();                      /* 屏重新上电初始化，包含清屏 */
  OLED_Clear();

  BspKey_Sync();                    /* 唤醒用的那一次按下不算翻页，避免多翻一页 */
  Power_TouchActivity();
  return 1;                         /* 告诉 main：屏是空的，从头重画 */
}
