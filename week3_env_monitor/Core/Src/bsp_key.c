#include "bsp_key.h"

/*
 * 机械按键按下的瞬间会弹跳几十次，直接读会一次按键识别成好几次。
 * 所以用状态机：电平变化后要稳定 20ms，才承认它真的变了；
 * 只有"松开 → 按下"这个方向才返回 1（叫下降沿触发）。
 */
#define KEY_DEBOUNCE_MS     20

/* 放到文件作用域，BspKey_Sync 也要改这两个量 */
static uint8_t  s_stable = 1;     /* 稳定电平，上电默认松开 */
static uint32_t s_tick   = 0;     /* 上次电平变化（或稳定）的时刻 */

uint8_t BspKey_Scan(void)
{
  uint8_t now = (uint8_t)HAL_GPIO_ReadPin(KEY_PORT, KEY_PIN);

  if (now != s_stable)
  {
    /* 电平变了，先等 20ms 看是不是抖动 */
    if ((uint32_t)(HAL_GetTick() - s_tick) >= KEY_DEBOUNCE_MS)
    {
      s_stable = now;
      s_tick   = HAL_GetTick();

      if (s_stable == 0)            /* 0 = 按下，返回一次事件 */
      {
        return 1;
      }
    }
  }
  else
  {
    s_tick = HAL_GetTick();         /* 一直稳定，计时器跟着走 */
  }

  return 0;
}

/*
 * 休眠唤醒时按键可能还按着。如果直接 Scan，会把这个"还按着"的状态
 * 当成一次新按下，多翻一页。所以唤醒后先同步一次，把当前电平当成基准，
 * 只有之后再从松开变按下才算事件。
 */
void BspKey_Sync(void)
{
  s_stable = (uint8_t)HAL_GPIO_ReadPin(KEY_PORT, KEY_PIN);
  s_tick   = HAL_GetTick();
}
