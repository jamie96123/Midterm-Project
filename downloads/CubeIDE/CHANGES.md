# 我在哪裡補了什麼

下列行號指向本次交付的 `Core/Src/main.c`；之後重新 Generate Code 可能改變行號，可搜尋 `[TR-...]` 註記或函式名。

| 項目 | 位置 | 補上的內容 | main.c 行號 |
| --- | --- | --- | --- |
| 標頭與型別 | `USER CODE Includes / PTD / PD` | 補標準 C 標頭、IRQ 宣告、遊戲資料結構、10 題及 ring 容量。 | 24 |
| 遊戲狀態 | `USER CODE PV` | 補分數、題號、答題進度、UART 命令暫存與回報時間戳。 | 67 |
| 遊戲核心 | `USER CODE 0 → game_init/game_update/game_key` | 補 40 個單字、8/5/3 秒、7/5/3 分、暫停／繼續、逾時換題及十題結束。 | 108 |
| UART 命令 | `game_command` | 補 SYNC/START/PAUSE/RESUME/RESET/COLOR/SPEED/KEY 的嚴格解析。 | 229 |
| GPIO/EXTI/NVIC | `gpio_init / button_irq` | 補 PA5 狀態燈、PB4–PB7 上拉下降緣中斷、50 ms 去彈跳及按鈕事件佇列。 | 265 |
| USART2 | `uart_init / TypeRush_USART2_IRQHandler` | 補 USART2 APB 時脈、PCLK1 mux、BRR=1476、RXNE/TXE 中斷、RX/TX 環形緩衝與錯誤重同步。 | 286 |
| Timer/PWM | `timers_init / board_lights` | 補 TIM2 10ms tick；TIM3 1kHz 三通道 PWM；暫停切 GPIO 恆亮。 | 305 |
| 與網頁同步 | `report / command` | 補 TypeRush v1 完整 JSON 狀態與錯誤回報，相容先前遊戲網頁。 | 445 |
| 初始化 | `USER CODE 2 → TypeRush_Init` | 在你原本 MX_GPIO_Init 之後初始化遊戲周邊並回報待機狀態。 | 555 |
| 空的 while | `USER CODE 3 → TypeRush_Process` | 補定時更新、按鈕事件、串列行解析、LED 刷新及 WFI 等待中斷。 | 566 |
| 原 AF12 配置 | `MX_GPIO_Init` | 明確指定 PA2/PA3，從 GPIO_AF12_LPUART1 改為 GPIO_AF7_USART2。這個自動產生區域也要在 .ioc 同步修正。 | 637 |

## 保留的正確部分

- `HAL_Init()` 與你原本的 `SystemClock_Config()`。
- HSI16、PLLM=4、PLLN=85、PLLR=2、170MHz、Boost、Flash 4WS、APB /1。
- 原本 main.c 的 USER CODE 架構與 Error_Handler。
- 本版沒有再呼叫舊獨立韌體的 clock_init，因此不會在 HAL 已配置時再直接改一次 PLL。

## 不是在 main.c 裡面就能完成的部分

1. **四個標準中斷入口**位於 `stm32g4xx_it.c`。必須把 TIM2、USART2、EXTI4、EXTI9_5 接到對應的 TypeRush helper。你的該檔沒有提供，所以我附合併片段與最小參考，沒有假裝已修改你電腦上的 it.c。
2. **.ioc 設定**未提供，所以不能宣稱已替你勾選 GUI。目標設定在 CubeMX_SETUP.md，重點是釋放 LPUART1、改 USART2 AF7、設定 TIM2/TIM3、GPIO 和 NVIC。
3. 本整合版在 USER CODE 區裡會直接開啟周邊及 NVIC，因此目前程式的執行不依賴 CubeMX 額外產生 HAL UART/TIM 初始化；但 .ioc 仍應同步，避免再生程式時不一致。

## 從你原始片段能確定的缺失

- `MX_GPIO_Init()` 只有埠時脈和 GPIOA 的 LPUART1 AF12 設定；片段沒有串列外設初始化。
- `while (1)` 沒有遊戲處理內容。
- 沒有 TIM2 / TIM3 初始化或啟動。
- 沒有四顆按鈕的 GPIO/EXTI/NVIC 設定。
- main.h 的 `LPUART1_TX_Pin` / `LPUART1_RX_Pin` 定義未附，因此不推定你原本巨集一定對應 PA2/PA3；本版已明確使用 PA2/PA3。
- 不能僅根據此 main.c 斷言其他檔案完全沒有中斷函式，因此修改 it.c 前須先搜尋同名函式，合併而非重複新增。

本版新增 IRQ helper 和應用程式都位於 USER CODE 內；唯一對既有自動產生 GPIO 配置的必要修改是 AF12→AF7 與明確腳位，必須同步進 .ioc。
