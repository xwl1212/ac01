#include "app_cmd.h"
#include "bsp_uart.h"
#include "app_adc.h"
#include "app_power.h"
#include "stdio.h"
#include "string.h"

#define CMD_BUF_SIZE    16

uint8_t g_page = 1;

static char    s_cmd[CMD_BUF_SIZE];
static uint8_t s_len;

/* main.c 里采集到的数据，这里只读，不改 */
extern uint16_t g_adc_raw;
extern uint16_t g_adc_avg;

void Cmd_Init(void)
{
  g_page = 1;
  s_len  = 0;
}

/* 按键翻页：1 → 2 → 1 循环，最多两页所以用一个判断就够 */
void Cmd_NextPage(void)
{
  g_page = (g_page == 1) ? 2 : 1;
  printf(">> page %u\r\n", g_page);
}

static void Cmd_ShowHelp(void)
{
  printf(">> commands:\r\n");
  printf("   GET       query light data\r\n");
  printf("   PAGE 1    main screen\r\n");
  printf("   PAGE 2    data screen\r\n");
  printf("   HELP      this message\r\n");
}

static void Cmd_Execute(const char *cmd)
{
  uint16_t mv = (uint16_t)((uint32_t)g_adc_avg * APP_ADC_VREF / APP_ADC_FULL);
  uint8_t  pct = App_AdcPercent(g_adc_avg);

  if (strcmp(cmd, "GET") == 0)
  {
    printf(">> light=%u%% raw=%u avg=%u v=%u.%02u page=%u\r\n",
           pct, g_adc_raw, g_adc_avg, mv / 1000, (mv % 1000) / 10, g_page);
  }
  else if (strcmp(cmd, "PAGE 1") == 0)
  {
    g_page = 1;
    printf(">> page 1: light monitor\r\n");
  }
  else if (strcmp(cmd, "PAGE 2") == 0)
  {
    g_page = 2;
    printf(">> page 2: raw data\r\n");
  }
  else if (strcmp(cmd, "HELP") == 0)
  {
    Cmd_ShowHelp();
  }
  else
  {
    printf(">> unknown: %s (type HELP)\r\n", cmd);
  }
}

/*
 * 主循环里反复调用。每次把缓冲里的字节全部取出来，
 * 遇到回车/换行就算一条命令结束，其余情况拼进字符串。
 */
void Cmd_Process(void)
{
  uint8_t b;

  while (BspUart_GetByte(&b))
  {
    if (b == '\r' || b == '\n')
    {
      if (s_len > 0)
      {
        s_cmd[s_len] = '\0';
        Cmd_Execute(s_cmd);
        s_len = 0;
        Power_TouchActivity();      /* 串口有操作，推迟休眠 */
      }
    }
    else if (s_len < CMD_BUF_SIZE - 1)
    {
      /* 统一转成大写，串口助手输小写也能识别 */
      if (b >= 'a' && b <= 'z')
      {
        b = (uint8_t)(b - 'a' + 'A');
      }
      s_cmd[s_len++] = (char)b;
    }
  }
}
