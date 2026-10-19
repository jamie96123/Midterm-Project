# STM32CubeMX / CubeIDE 需要開啟什麼

你只提供 main.c，沒有 `.ioc` 和 main.h。因此「從片段看得到缺少初始化」不等於能判定你 GUI 每一個勾選狀態。下表是整合版的確切目標設定。

此版 **直接用暫存器初始化硬體**，並保留 HAL 系統時脈。CubeMX 設定的用途是讓 pinout、初始化與未來 Generate Code 保持一致；不能只開 GUI 中斷勾選，就省略 it.c 的四個入口接線。

## 1. System Core → SYS

- Debug：**Serial Wire**，保留 PA13 / PA14 給 SWD，讓 PB4 能作一般 GPIO。
- Timebase Source：**SysTick**。不要把 HAL timebase 改成 TIM2，TIM2 在本遊戲專門產生 10 ms ticks。
- 保留你原本 `SysTick_Handler()` 裡的 `HAL_IncTick()`。

## 2. RCC / Clock Configuration：原本已經正確

你的函式已經包含下列內容，不需要重新改成 HSE：

| 項目 | 值 |
| --- | --- |
| HSI | ON，16 MHz |
| PLL Source | HSI |
| PLL M / N / R | 4 / 85 / 2 |
| SYSCLK | 170 MHz |
| AHB Prescaler | /1，HCLK 170 MHz |
| APB1 / APB2 Prescaler | /1，PCLK1/PCLK2 均 170 MHz |
| Voltage Scaling | Range 1 Boost |
| Flash Latency | 4 wait states |

`SystemClock_Config()` 在整合檔中逐字保留。`TypeRush_Init()` 也檢查系統與兩個 APB 是否為 170 MHz；若修改時脈而不調整 UART/Timer，會進 Error_Handler，而不讓錯誤的時間參數繼續執行。

## 3. Connectivity → USART2

先把 **LPUART1 關閉／將占用同一腳位的 AF12 釋放**，改用：

| 項目 | 值 |
| --- | --- |
| Mode | Asynchronous |
| TX / RX Pin | PA2 = USART2_TX、PA3 = USART2_RX |
| AF | AF7，不是 AF12 |
| Baud Rate | 115200 |
| Word Length | 8 Bits |
| Parity | None |
| Stop Bits | 1 |
| Hardware Flow Control | None |
| Direction | Transmit and Receive |
| Oversampling | 16 |
| Clock Source（若可選） | PCLK1，170 MHz |
| NVIC | USART2 global interrupt：Enable，Preemption Priority 1，Subpriority 0 |

你的舊程式只有 `GPIO_AF12_LPUART1`，沒有串列外設的 BRR/CR1 或 `MX_LPUART1_UART_Init()`。單純把 GPIO 設 AF 不能啟動 UART。

整合版的 `uart_init()` 已設 RCC mux 為 PCLK1、BRR=1476，啟用 UE/TE/RE、RXNEIE/EIE 與 NVIC，並用 TXEIE 發送環形佇列。

這版不需要 DMA、不使用 `HAL_UART_Receive_IT()`，不要額外用 HAL 接收／中斷回呼去讀同一個 USART2 RDR。

## 4. Timers → TIM2：10 ms 定時中斷

| 項目 | 值 |
| --- | --- |
| Clock Source | Internal Clock |
| Prescaler | 169 |
| Counter Mode | Up |
| Counter Period / ARR | 9999 |
| Clock Division | DIV1 |
| NVIC | TIM2 global interrupt：Enable，Priority 0、Subpriority 0 |

計算：`170000000 / (169+1) / (9999+1) = 100 Hz`，每次中斷 **10 ms**。

TIM2 不需要輸出腳位。整合版會設 UIE / CEN、清 UIF，並讓 `ticks += 10`；不必另外呼叫 HAL_TIM_Base_Start_IT。

## 5. Timers → TIM3：RGB PWM

| 項目 | 值 |
| --- | --- |
| Clock Source | Internal Clock |
| Channel 1 / 2 / 3 | PWM Generation CH1 / CH2 / CH3 |
| CH1 Pin | PA6，AF2，紅 |
| CH2 Pin | PA7，AF2，綠 |
| CH3 Pin | PB0，AF2，藍 |
| Prescaler | 169 |
| Counter Period / ARR | 999 |
| Counter Mode / Clock Division | Up / DIV1 |
| PWM Mode / Polarity | PWM Mode 1 / High |
| Initial Pulse | 各通道 0 |
| NVIC TIM3 IRQ | 不需要開啟 |

載波：`170000000 / 170 / 1000 = 1000 Hz`。
呼吸亮度包絡由每次主迴圈更新 CCR1/2/3，週期依速率為 1.8／1.2／0.7 秒；不是把 PWM 載波改到這麼慢。

暫停時會把 PA6/PA7/PB0 從 AF 改成 GPIO 輸出，用 BSRR 恆亮，繼續後回到 AF2。整合版已啟動 TIM3 和三個通道，不需再呼叫 HAL_TIM_PWM_Start。

## 6. System Core → GPIO / Pinout

| Pin | Mode | Pull | Speed / 初始值 |
| --- | --- | --- | --- |
| PA5 | GPIO_Output，Push-Pull | No Pull | Low / 初始 Low |
| PB4 | GPIO_EXTI4，Falling edge trigger | Pull-up | BTN1，一端 GND |
| PB5 | GPIO_EXTI5，Falling edge trigger | Pull-up | BTN2，一端 GND |
| PB6 | GPIO_EXTI6，Falling edge trigger | Pull-up | BTN3，一端 GND |
| PB7 | GPIO_EXTI7，Falling edge trigger | Pull-up | BTN4，一端 GND |
| PA2 / PA3 | USART2 AF7，Push-Pull | No Pull | Low 可用 |
| PA6 / PA7 / PB0 | TIM3 AF2，Push-Pull | No Pull | Low 可用 |

EXTI4、EXTI9_5 在 NVIC 都 Enable，Priority **2**、Subpriority 0。
PB5/PB6/PB7 分別是獨立 EXTI5/6/7，但共用 `EXTI9_5_IRQHandler` 硬體入口；該入口一次檢查三條線。

按鈕每鍵獨立做 50 ms 去彈跳。這個版本使用四顆外接按鈕，沒有使用 PC13 板載 B1。

## 7. NVIC → Code generation：最容易漏掉的地方

針對以下 **四個** IRQ：

- TIM2 global interrupt
- USART2 global interrupt
- EXTI line4 interrupt
- EXTI line[9:5] interrupts

設定：

1. **Enabled：勾選。**
2. **Generate IRQ handler：勾選**（有此選項的 CubeMX 版本）。
3. **Call HAL handler：取消勾選**（有此選項的版本）。
4. Generate Code 後，在各 `USER CODE BEGIN ..._IRQn 0` 加上對應的 `TypeRush_..._IRQHandler();`。
5. 若版本沒有 Call HAL handler 選項，確認四個標準 IRQ 的主體只接遊戲處理函式，不再保留對應 HAL IRQ 呼叫；重新 Generate Code 後再次檢查。

完整合併片段在 `IRQ_HANDLERS_TO_MERGE.c.txt`，不要讓同一個標準 IRQ 同時在 main.c 和 it.c 各定義一次。

**不要刪除 SysTick 的 HAL_IncTick，也不要對 SysTick 套用上面取消 HAL tick 的做法。**

## 8. Project Manager → Code Generator

- 開啟 **Keep User Code when re-generating**（名稱依版本略有差異）。
- 維持你原本 main.c 內有 `MX_GPIO_Init()` 的程式組織方式即可。
- 若 Cube 產生 UART / TIM 的 HAL 初始化，這些初始化在 USER CODE 2 前；`TypeRush_Init()` 最後會再次以指定暫存器設定周邊，因此遊戲仍以本檔參數為準。
- 不需要開 USB Device / CDC：NUCLEO 的 ST-LINK 虛擬 COM 是由 ST-LINK 轉接 USART2。
- 不需要開 ADC、DMA、RTOS、TIM input capture。

## 編譯後快速檢查

1. `main.c` 中只有一個 `main()`，沒有額外的標準 TIM2/USART2/EXTI IRQ 定義。
2. it.c 中四個入口各呼叫一次 TypeRush 函式。
3. 顯示 duplicate symbol：通常是把完整參考 it.c 與原 it.c 同時編譯，或又加入舊版 board.c。
4. 找不到 typerush_irq.h：檔案要放 Core/Inc，Include Paths 應包含該目錄。
5. 找不到 htim2 / huart2，且 it.c 還有 HAL_*_IRQHandler：四個入口尚未改成遊戲派送方式。
6. 能編譯但完全沒倒數／沒回應：檢查向量是否仍落在 startup 的弱 `Default_Handler`，以及是否真的接上四個標準入口。
7. UART 亂碼：核對 AF7、PCLK1=170MHz、115200 8N1、USB 接 ST-LINK。
