#include "bsp_uart.h"
#include "usart.h"

/* huart1 由 CubeMX 生成在 usart.c 里，usart.h 里有它的 extern 声明，这里直接用 */

/*
 * 环形缓冲：head 是"写指针"（中断里往前推），tail 是"读指针"（主循环里的 GetByte 往前推）。
 * head == tail 表示空；head 追到 tail 前面一格表示满（留一个空位区分空和满）。
 */
static uint8_t  s_rx_buf[RX_BUF_SIZE];
static volatile uint16_t s_head;
static volatile uint16_t s_tail;
static uint8_t  s_rx_byte;          /* HAL 每次只收一个字节，收完放进缓冲再重新挂起 */

/*
 * printf 重定向：C 库把每个字符交给 fputc，我们在这里转给串口 1。
 * 前提：Keil 的 Target 选项里要勾上 Use MicroLIB。
 */
int fputc(int ch, FILE *f)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
  return ch;
}

void BspUart_Init(void)
{
  s_head = 0;
  s_tail = 0;
  HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);
}

/* 中断回调：CubeMX 已经把 USART1 的中断打开了，这里只做搬运，不做解析 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    uint16_t next = (s_head + 1) % RX_BUF_SIZE;

    if (next != s_tail)             /* 缓冲没满才写，满了就丢这一字节 */
    {
      s_rx_buf[s_head] = s_rx_byte;
      s_head = next;
    }

    HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);   /* 重新挂起，等下一个字节 */
  }
}

uint8_t BspUart_GetByte(uint8_t *byte)
{
  if (s_head == s_tail)
  {
    return 0;
  }

  *byte = s_rx_buf[s_tail];
  s_tail = (s_tail + 1) % RX_BUF_SIZE;
  return 1;
}
