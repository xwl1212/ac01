# Week 1 · GPIO + 串口中断 + 外部中断

STM32F103C8T6 第 1 周练习：按键和电脑串口都能点亮 LED，1 秒后自动熄灭，串口回显收到的字符。

## 硬件

- 开发板：野火小智 STM32F103C8T6（LQFP48，72MHz）
- 下载器：CMSIS-DAP（SWD：PA13 / PA14）
- LED：共阳极接法，**低电平点亮**
- 按键：PA0，外部下拉，按下读到高电平

## 引脚分配

| 引脚 | 功能 | 模式 |
|---|---|---|
| PA0 | 按键 | EXTI 上升沿中断 + 下拉 |
| PA1 | LED1（按键控制） | 推挽输出 |
| PA2 | LED2（串口控制） | 推挽输出 |
| PA3 | LED3（预留） | 推挽输出 |
| PA9 | USART1 TX | 复用推挽 |
| PA10 | USART1 RX | 浮空输入 |
| PA13 / PA14 | SWD 调试 | 调试口 |

串口参数：115200 / 8N1

## 功能

| 触发源 | 现象 |
|---|---|
| 上电 | 三灯全灭，串口无输出 |
| 按下 PA0 | PA1 点亮，约 1 秒后自动熄灭；串口输出 `button pressed` |
| 电脑发一个字符 | PA2 点亮，约 1 秒后自动熄灭；串口回显该字符 + 换行 |

## 架构：中断只置标志，业务在主循环

两个中断都不做业务，只做两件事：置标志、重新挂起接收，然后立刻返回。

```c
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        rx_flag = 1;                               // 只置标志
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1); // 立刻续挂，防止 overrun
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
    if (gpio_pin == GPIO_PIN_0)
    {
        btn_flag = 1;                              // 只置标志
    }
}
```

主循环按 10ms 一圈轮询标志，统一处理业务和灭灯：

```c
while (1)
{
    if (rx_flag == 1)   { /* 电脑分支：点 PA2 + 回显 */ }
    if (btn_flag == 1)  { /* 按键分支：点 PA1 + 发固定串 */ }

    if (timer_expired(&led1_off)) { led_off(GPIOA, GPIO_PIN_1); }
    if (timer_expired(&led2_off)) { led_off(GPIOA, GPIO_PIN_2); }

    HAL_Delay(10);
}
```

这样中断不会因为串口发送、延时而被长时间占用，也不用在中断里放业务逻辑。

## 非阻塞定时

没有用 `HAL_Delay(1000)` 死等，而是记录"目标时间戳"，由主循环轮询比较：

```c
void timer_start(volatile uint32_t *deadline, uint32_t ms)
{
    *deadline = HAL_GetTick() + ms;      // 登记"什么时候该灭"
}

uint8_t timer_expired(volatile uint32_t *deadline)
{
    if (*deadline != 0 && HAL_GetTick() >= *deadline)
    {
        *deadline = 0;                    // 0 表示"当前没有任务"
        return 1;
    }
    return 0;
}
```

两个细节：

- 用 `>=` 而不是 `==`：主循环 10ms 才转一圈，时间戳正好相等几乎不可能
- 用 `0` 当空标记：清零后 `*deadline != 0` 不成立，不会重复触发

## 遇到的问题

**1. 编译 0 错误、下载成功，但烧录后完全没有反应**

回调函数名写成了小写 `hal_uart_rxcpltcallback` / `hal_gpio_exit_callback`。

HAL 只认 `HAL_UART_RxCpltCallback` / `HAL_GPIO_EXTI_Callback`，C 语言区分大小写。名字写错不会有编译错误，只是定义了两个永远不会被调用的普通函数，HAL 走的是它自带的空弱函数，导致 `rx_flag` / `btn_flag` 永远是 0。

**2. printf 输出乱码**

`fputc` 里写成 `(uint8_t *)ch`，少了取地址符，改成 `&ch`。

**3. 连续发多个字符只回显第一个**

回调里先阻塞发送、再续挂接收，中间发生了 overrun。改成"取数据 → 立刻续挂 → 再发送"。

**4. 回显缺少换行**

`HAL_UART_Transmit(&huart1, crlf, 1, ...)` 长度写成 1，只发出了 `\r`，改成 2。

## 已知限制

单字节 `rx_byte` + 标志位的结构，一次只能处理一个字节。主循环 10ms 一圈，而 115200 下 1 个字节只需要约 87µs，连续发送会丢字符。

后续用环形缓冲区（`rx_head` / `rx_tail`）解决，放到第 2 周。

## 编译说明

仓库只提交 `Core/Inc`、`Core/Src` 和 `.ioc`，不包含 `Drivers/`（CubeMX 生成的 HAL 库，文件数量太多）。

复现方式：

1. 用 STM32CubeMX 打开 `week_1_1.ioc`
2. 点击 Generate Code
3. 用 Keil MDK 打开生成的工程编译下载

所有用户代码都写在 `main.c` 的 `USER CODE BEGIN/END` 区域内，重新生成不会丢失。

## 演示视频

[点此观看演示视频（41 秒，1080p）](https://github.com/xwl1212/ac01/releases/download/v1.0-week1/week1_demo.mp4)

视频内容：

1. 上电，三灯全灭，串口无输出
2. 按下 PA0，PA1 点亮约 1 秒后熄灭，串口输出 `button pressed`
3. 电脑发送字符，PA2 点亮约 1 秒后熄灭，串口回显该字符

也可以从 [Release 页面](https://github.com/xwl1212/ac01/releases/tag/v1.0-week1) 下载。

## 关键文件

| 文件 | 内容 |
|---|---|
| `Core/Src/main.c` | 全部用户逻辑：状态机、定时、回调、printf 重定向 |
| `Core/Src/gpio.c` | PA0 中断配置、PA1–PA3 输出配置 |
| `Core/Src/usart.c` | USART1 115200 配置、NVIC 使能 |
| `week_1_1.ioc` | CubeMX 工程配置 |
