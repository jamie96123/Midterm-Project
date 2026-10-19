# 第 3–7 週技術勾選表

下列勾選代表 **原始碼有實作且已交叉編譯**，不是實體板測試已通過。按圖片格式，每項都能說出「用在哪裡、哪個周邊／暫存器、程式在哪」。

| 使用 | 技術 | 遊戲用途 | 周邊／暫存器 | 程式位置 |
| --- | --- | --- | --- | --- |
| ☑ | GPIO 輸出 | PA5 LD2 遊戲心跳；暫停時 RGB 恆亮 | GPIOA/B MODER=01、BSRR 設／清，PA6/PA7/PB0 | `board.c:69 gpio_init()`、`board.c:214 board_lights()` |
| ☑ | GPIO 輸入 | 四顆常開按鈕 | PB4–PB7、MODER=00、PUPDR=01、IDR | `board.c:69 gpio_init()`、`board.c:133 button_irq()` |
| ☑ | 複用功能 AF | USART2 收發與 TIM3 三色 PWM | PA2/PA3 AF7；PA6/PA7/PB0 AF2；MODER=10、AFR | `board.c:64 alternate()`、`uart_init()`、`timers_init()` |
| ☑ | PLL 170 MHz | 遊戲計時、UART、PWM 共用時脈 | HSI16 /4 ×85 /2；RCC CR/PLLCFGR/CFGR、FLASH ACR 4WS、PWR CR1/CR5 boost | `board.c:25 clock_init()` |
| ☑ | 外部中斷 EXTI | 按鈕觸發開始／暫停／燈色／速率 | SYSCFG EXTICR 選 PB4–PB7；FTSR1、IMR1、PR1 寫 1 清除 | `board.c:69 gpio_init()`、`EXTI4_IRQHandler()`、`EXTI9_5_IRQHandler()` |
| ☑ | NVIC 致能 | 計時、UART、按鍵中斷進入 CPU | NVIC_EnableIRQ：TIM2、USART2、EXTI4、EXTI9_5 | `gpio_init()`、`uart_init()`、`timers_init()` |
| ☑ | 優先權與可搶占 | 10ms 計時高於串列、串列高於按鍵 | NVIC_SetPriority：TIM2=0、USART2=1、EXTI=2，數字小優先 | 同上；實際巢狀時序待實機量測 |
| ☑ | 軟體去彈跳 | 按一下按鈕只產生一個事件 | 每鍵獨立 button_times[]，`ticks - last >= 50`；主迴圈取事件 bitmask | `board.c:133 button_irq()`、`board_buttons()` |
| ☑ | UART 傳送 | 發題、進度、分數、計時與錯誤回報 | USART2 BRR=1476（170MHz）、115200 8N1、TDR/TXE/TXEIE；2048-byte TX ring | `board.c:90 uart_init()`、`board_send()`、`USART2_IRQHandler()`；`main.c report()` |
| ☑ | UART 接收＋環形緩衝 | 接收網頁鍵盤與控制指令 | RXNE IRQ、RDR、512-byte RX ring；主迴圈遇 LF 分析，不在 ISR 執行遊戲 | `board.c:153 USART2_IRQHandler()`、`board_getchar()`；`main.c` 行解析；`protocol.c game_command()` |
| ☑ | Timer 定時中斷 | 每題倒數、50ms 去彈跳時間戳 | TIM2 PSC=169、ARR=9999、UIE/UIF；170MHz/170/10000=100Hz | `board.c:104 timers_init()`、`board.c:130 TIM2_IRQHandler()` |
| ☑ | PWM | 1kHz 三通道呼吸燈、切速率調呼吸頻率 | TIM3 PSC=169、ARR=999、CCMR1/2 PWM mode1、CCER、CCR1/2/3 | `board.c:104 timers_init()`、`board.c:214 board_lights()` |
| ☐ | 輸入捕捉 | 本遊戲沒有外部脈衝測速需求 | 未配置 input capture，不列為已用技術 | 無 |

韌體位置皆相對 `firmware/src/`，行號依此交付版本；之後編輯請以函式名搜尋。網頁的「技術勾選表」也提供相同說明。

## 設計與簡報的對應

- **三階動態計分**：`game.c game_tier()` 與 `game_key()`。>60% 綠燈 7 分，30–60% 黃燈 5 分，<30% 紅燈 3 分。計分讀實際剩餘比例，不使用手動展示燈色。
- **雙態光影**：running 時 TIM3 三通道 PWM；paused 時將同一腳位切成一般 GPIO，由 BSRR 設定恆亮。不是僅把 PWM duty 固定而宣稱 GPIO。
- **四組獨立按鈕**：PB4、PB5、PB6、PB7 分別映射 EXTI4–7；EXTI5–7 共用硬體 IRQ 入口但逐一清除與處理。
- **非阻塞接收**：ISR 入 ring，主迴圈出 ring、組命令、更新遊戲；TX 同樣用 ring 與 TXE IRQ，避免等待每個位元組的線路傳送。
- **硬體時基**：TIM2 10ms 中斷更新 `ticks`，`game_update()` 使用時間差，暫停也更新 last_tick 因而不把暫停時間帶入恢復後的倒數。
- **時脈選擇**：PDF 說明 HSE + PLL；此版選 HSI16 + PLL，與技術範例圖的 HSI16 /4 ×85 /2 一致。Range 1 boost、Flash 4WS，以及暫時 AHB /2 後升至全速均在 `clock_init()`。另有 8/16/24MHz 的可選 HSE 分支，24MHz bypass 已交叉編譯；頻率及焊橋必須依板次確認。
- **IRQ 負擔**：TIM2 只加 ticks，EXTI 只去彈跳與記錄事件，USART2 只處理收發佇列；出題、字串比對、JSON 格式化均留在 main。主迴圈每次最多處理 128 bytes，避免忙碌的接收端餓死遊戲更新。

## 答辯可實際展示

1. 在 `clock_init()` 指出 PLLM=4（暫存器編碼為 3）、PLLN=85、PLLR=2 的來源及 170MHz 算式。
2. 打開 `timers_init()` 比較 TIM2 的 10ms 更新中斷與 TIM3 的 1kHz 硬體 PWM，說明二者不是同一個頻率。
3. 暫停遊戲，在 `board_lights()` 指出 MODER=01 和 BSRR 變更；繼續時恢復 AF2。
4. 按 BTN 4，說明只是改每題時限與呼吸包絡週期，PWM 載波仍然是 1kHz。
5. 在 UART 對話查看 `KEY <ASCII>` 與板端 state，錯字時 wrong 增加、pos 不變。
6. 在 `button_irq()` 指出每鍵獨立 50ms 時間戳，在 `board_buttons()` 指出短暫臨界區，避免 ISR 與主迴圈清除事件競爭。
7. 在 `USART2_IRQHandler()`、`board_getchar()` 與 main 的 discard 分支說明 RX overflow 及資料行重同步。

實際燈光、IRQ 時序與 UART 電氣層需要接板驗證，不能以網頁動畫或主機測試替代。
