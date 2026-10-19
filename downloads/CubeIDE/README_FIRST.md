# 把 TypeRush 放進你的 STM32CubeIDE 專案

這包是針對你貼上的 `main.c` 做好的「整合檔」，不是另一個全新的 CubeIDE 工程。完整遊戲集中在 `Core/Src/main.c`；你原本的 `HAL_Init()`、`SystemClock_Config()`、Cube USER CODE 區段都保留。**原本 170 MHz 時脈函式逐字保留。**

## 你要放入的檔案

1. 備份原本工程，再將本包 `Core/Src/main.c` 放到你工程的 **Core/Src/main.c**。
2. 將 `Core/Inc/typerush_irq.h` 放到你工程的 **Core/Inc/**。
3. 在你原本的 **Core/Src/stm32g4xx_it.c** 加入 `#include "typerush_irq.h"`，並接上四個 IRQ。完整內容見 **IRQ_HANDLERS_TO_MERGE.c.txt** 及下面的說明。
4. Build Project，再經 ST-LINK 燒錄。網頁 UART 參數保留 115200 / 8 / 1 / None / None。

**不能只更換 main.c 就省略第 3 步。** STM32 向量表呼叫的是 `TIM2_IRQHandler()` 等標準名稱，而 main.c 中實際工作函式刻意命名為 `TypeRush_TIM2_IRQHandler()` 等，避免和 Cube 產生的標準入口重複定義。

不要再加入先前獨立韌體版本的 board.c、game.c、protocol.c 或另一份 main.c；這份 main.c 已包含相同功能。

## 四個中斷入口

在 `stm32g4xx_it.c` 的 `USER CODE BEGIN Includes` 裡加：

```c
#include "typerush_irq.h"
```

標準 IRQ 函式的主體應如下。**若已存在函式，修改該函式，不要再新增同名函式。**

```c
void TIM2_IRQHandler(void)
{
  /* USER CODE BEGIN TIM2_IRQn 0 */
  TypeRush_TIM2_IRQHandler();
  /* USER CODE END TIM2_IRQn 0 */
}

void USART2_IRQHandler(void)
{
  /* USER CODE BEGIN USART2_IRQn 0 */
  TypeRush_USART2_IRQHandler();
  /* USER CODE END USART2_IRQn 0 */
}

void EXTI4_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI4_IRQn 0 */
  TypeRush_EXTI4_IRQHandler();
  /* USER CODE END EXTI4_IRQn 0 */
}

void EXTI9_5_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI9_5_IRQn 0 */
  TypeRush_EXTI9_5_IRQHandler();
  /* USER CODE END EXTI9_5_IRQn 0 */
}
```

這四個入口內不再呼叫 `HAL_TIM_IRQHandler()`、`HAL_UART_IRQHandler()`、或用於 PB4–PB7 的 `HAL_GPIO_EXTI_IRQHandler()`，因為遊戲已直接讀寫暫存器及清中斷旗標。CubeMX 中請把這四個 IRQ 的 **Call HAL handler** 取消，保留 **Generate IRQ handler**，詳細操作看 **CubeMX_SETUP.md**。

`Reference/stm32g4xx_it.c` 是最小工程的完整參考檔，用於本次建置驗證。它不是你原本 it.c 的逐字修改版本，因為你沒有提供該檔。請優先合併四段入口，保留你原本的其他中斷與例外處理。

## 為什麼還要看 CubeMX 設定？

這份 main.c 已在 `TypeRush_Init()` 裡自行開啟 RCC、GPIO、USART2、TIM2、TIM3、EXTI、NVIC。你目前的工程不必依賴 HAL_UART 或 HAL_TIM 模組才能使用本版的暫存器實作。

但 `.ioc` 必須和程式一致，否則之後按 Generate Code 可能把 PA2/PA3 改回原來的 LPUART1 AF12、或把 HAL IRQ 呼叫加回來。**CubeMX_SETUP.md** 列出了你應該設定的每一項，以及不必開的功能。

若要先調整 CubeMX：先按文件設定並 Generate Code，再放入上述 main.c / header，最後合併四個 IRQ。若後續又重新產生程式，請檢查 USER CODE 內容和 IRQ 入口仍在。

## 接線

| 功能 | 腳位 | 接線 |
| --- | --- | --- |
| USART2 TX / RX | PA2 / PA3 | NUCLEO-G474RE 的 ST-LINK VCP；使用 ST-LINK 資料 USB |
| 紅／綠／藍 LED | PA6 / PA7 / PB0 | 每色各串一顆 1 kΩ，接共陰極 RGB；共陰極接 GND |
| BTN 1 開始／繼續 | PB4 | 常開按鈕，按下接 GND，內部上拉 |
| BTN 2 暫停／繼續 | PB5 | 同上 |
| BTN 3 燈色 | PB6 | 同上 |
| BTN 4 速率 | PB7 | 同上 |
| 板載 LD2 | PA5 | 不必外接 |

PC13 板載 B1 未使用。若實際不是 NUCLEO-G474RE，ST-LINK VCP 路由和按鈕接線需以實際板子確認。

## 驗證狀態

- 已把實際整合 main.c 和四個 IRQ 參考入口，使用 **ST 官方 HAL v1.2.5、CMSIS-G4 v1.2.5、Arm GNU 14.2.1** 交叉編譯及連結。
- 從整合檔抽取遊戲與協定核心執行原生 C 斷言，並通過 8 項 Node 測試，包含和網頁練習引擎的逐步結果一致性。
- 已驗證四個 IRQ 的向量指向正確標準入口，且 SysTick 有保留。
- 尚未取得你的完整 `.ioc`、`main.h`、`stm32g4xx_it.c`、HAL 設定及 linker 檔，所以 **尚未在你的完整 CubeIDE 工程內建置**。
- 目前沒有接實體板，尚未燒錄或實測 LED、UART、按鈕、PWM。

其他檔案：**CHANGES.md** 列出補了哪裡；**main.c.patch** 提供你原始 main.c 與整合版本的差異；**VALIDATION.md** 說明編譯依據與限制。
