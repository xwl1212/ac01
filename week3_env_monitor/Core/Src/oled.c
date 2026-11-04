#include "oled.h"
#include "i2c.h"
#include "oledfont.h"
#include <string.h>

/* hi2c1 由 CubeMX 生成在 i2c.c 里，i2c.h 中已有 extern 声明 */

/*
 * SSD1306 把一次 I2C 传输拆成两部分：
 *   第一个字节是"控制字节"，告诉屏后面这串数据是命令还是显存数据
 *     0x00 -> 后面全是命令
 *     0x40 -> 后面全是显存数据（点亮像素用）
 *   剩下的字节才是真正的内容
 */
static void OLED_WriteCmd(uint8_t cmd)
{
  uint8_t buf[2] = { 0x00, cmd };
  HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR << 1, buf, 2, 100);
}

static void OLED_WriteData(uint8_t data)
{
  uint8_t buf[2] = { 0x40, data };
  HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR << 1, buf, 2, 100);
}

/* 一次传一整行（最多 128 字节），比逐字节发快得多 */
static void OLED_WriteDataRow(const uint8_t *data, uint16_t len)
{
  uint8_t buf[129];   /* 1 字节控制字 + 最多 128 字节数据 */
  if (len > 128)
  {
    len = 128;
  }
  buf[0] = 0x40;
  memcpy(&buf[1], data, len);
  HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR << 1, buf, len + 1, 100);
}

/*
 * 把光标移到 (x, page)。之后每写一个字节，列号自动 +1；
 * 写到第 127 列会自动换到下一页，这是 SSD1306 的页寻址模式。
 */
static void OLED_SetPos(uint8_t x, uint8_t page)
{
  OLED_WriteCmd(0xB0 | (page & 0x07));        /* 选页 */
  OLED_WriteCmd(0x00 | (x & 0x0F));           /* 列地址低 4 位 */
  OLED_WriteCmd(0x10 | ((x >> 4) & 0x0F));    /* 列地址高 4 位 */
}

void OLED_Init(void)
{
  HAL_Delay(50);              /* 等屏内部上电复位完成 */

  OLED_WriteCmd(0xAE);        /* 关显示 */
  OLED_WriteCmd(0xD5);        /* 时钟分频 */
  OLED_WriteCmd(0x80);
  OLED_WriteCmd(0xA8);        /* 复用率 = 1/64，屏是 64 行 */
  OLED_WriteCmd(0x3F);
  OLED_WriteCmd(0xD3);        /* 显示偏移 0 */
  OLED_WriteCmd(0x00);
  OLED_WriteCmd(0x40);        /* 起始行 = 0 */
  OLED_WriteCmd(0x8D);        /* 电荷泵 */
  OLED_WriteCmd(0x14);        /* 打开内部升压，否则屏不亮 */
  OLED_WriteCmd(0x20);        /* 寻址模式 */
  OLED_WriteCmd(0x02);        /* 0x02 = 页寻址，和下面的 SetPos 配套 */
  OLED_WriteCmd(0xA1);        /* 左右方向（现方向为正） */
  OLED_WriteCmd(0xC8);        /* 上下方向（现方向为正） */
  OLED_WriteCmd(0xDA);        /* COM 引脚配置 */
  OLED_WriteCmd(0x12);
  OLED_WriteCmd(0x81);        /* 对比度 */
  OLED_WriteCmd(0xCF);
  OLED_WriteCmd(0xD9);        /* 预充电周期 */
  OLED_WriteCmd(0xF1);
  OLED_WriteCmd(0xDB);        /* VCOMH */
  OLED_WriteCmd(0x40);
  OLED_WriteCmd(0xA4);        /* 显示内容跟随显存 */
  OLED_WriteCmd(0xA6);        /* 正常显示（不反色） */
  OLED_WriteCmd(0xAF);        /* 开显示 */

  OLED_Clear();
}

void OLED_Clear(void)
{
  uint8_t blank[128] = {0};
  uint8_t page;

  for (page = 0; page < 8; page++)
  {
    OLED_SetPos(0, page);
    OLED_WriteDataRow(blank, 128);
  }
}

void OLED_DisplayOn(void)
{
  OLED_WriteCmd(0xAF);      /* 0xAF = 打开显示的电平输出 */
}

void OLED_DisplayOff(void)
{
  OLED_WriteCmd(0xAE);      /* 0xAE = 关闭显示，显存内容不变，再 0xAF 就恢复 */
}

/*
 * 显示一个字符。字模是 8 列 x 16 行，所以要把上下两半分别写进 page 和 page+1。
 * page 只能传 0/2/4/6，传 7 会把下半截写到屏外。
 */
void OLED_ShowChar(uint8_t x, uint8_t page, char ch)
{
  uint8_t i;
  const uint8_t *code;

  if (page > 6 || x > 120)
  {
    return;
  }
  if (ch < 0x20 || ch > 0x7E)   /* 字库只覆盖可见 ASCII */
  {
    ch = ' ';
  }

  code = OLED_F8x16[ch - 0x20];

  OLED_SetPos(x, page);
  for (i = 0; i < 8; i++)
  {
    OLED_WriteData(code[i]);          /* 上半页：第 0~7 行 */
  }

  OLED_SetPos(x, page + 1);
  for (i = 0; i < 8; i++)
  {
    OLED_WriteData(code[8 + i]);      /* 下半页：第 8~15 行 */
  }
}

void OLED_ShowString(uint8_t x, uint8_t page, const char *str)
{
  while (*str != '\0')
  {
    if (x > 120)          /* 一行最多 16 个字符，写满就停 */
    {
      return;
    }
    OLED_ShowChar(x, page, *str);
    x += 8;
    str++;
  }
}

/* 显示定长十进制数，len 位，不足补 0。例如 OLED_ShowNum(0, 6, 2026, 4) */
void OLED_ShowNum(uint8_t x, uint8_t page, uint32_t num, uint8_t len)
{
  uint8_t i, j;
  uint32_t div;

  for (i = 0; i < len; i++)
  {
    div = 1;
    for (j = 0; j < len - 1 - i; j++)
    {
      div *= 10;
    }
    OLED_ShowChar(x + i * 8, page, (char)('0' + (num / div) % 10));
  }
}

/*
 * 在某一页整页画一条横向进度条，width 是亮的列数（0~128）。
 * 一页 8 行像素，所以条高 8 像素，正好贴在两行文字之间。
 */
void OLED_ShowBar(uint8_t page, uint8_t width)
{
  uint8_t buf[128];
  uint8_t i;

  if (width > 128)
  {
    width = 128;
  }
  for (i = 0; i < 128; i++)
  {
    buf[i] = (i < width) ? 0xFF : 0x00;
  }

  OLED_SetPos(0, page & 0x07);
  OLED_WriteDataRow(buf, 128);
}
