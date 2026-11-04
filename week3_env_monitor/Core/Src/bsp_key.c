#include "bsp_key.h"

/*
 * 机械按键按下的瞬间会弹跳几十次，直接读会一次按键识别成好几次。
 * 所以用状态机：电平变化后要稳定 20ms，才承认它真的变了；
 * 只有"松开 → 按下"这个方向才返回 1（叫下降沿触发）。
 */
#define KEY_DEBOUNCE_MS     20

uint8_t BspKey_Scan(void)
{
  static uint8_t  s_stable = 1;     /* 稳定电平，上电默认松开 */
  static uint32_t s_tick   = 0;     /* 上次电平变化（或稳定）的时刻 */
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
