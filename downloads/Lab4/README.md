# Lab4：照 PPT 方法控制紅、黃、綠燈

這是依你提供的 `Lab4_BASIC_v11_Buffer_200ms_FIXED.pptx` 改寫的版本，不使用先前遊戲的 JSON、KEY 指令或 USART2 暫存器輪詢。

## 跟 PPT 相同的流程

1. BASIC 01：LPUART1，115200 / 8N1；開機 `HAL_UART_Transmit()` 傳一次 `HELLO WORLD`。
2. BASIC 02：`ProcessReceivedByte()` 收到 LF 才呼叫 `ExecuteCommand()`，用 `strcmp()` 判斷文字，`HAL_GPIO_WritePin()` 控制燈號。
3. BASIC 03：`HAL_UART_Receive_IT()` 每次收 1 byte；接收回呼 `RingPush()` 後重新啟動接收；主迴圈 `HAL_Delay(200U)` 再用 `RingPop()` 把累積資料取完。

保留 PPT 的 rxHead、rxTail、rxDropped、commandCount 變數，方便你在 Debug 觀察。

## 依你指定的接線，與 PPT 不同的地方

| 指令 | 本版腳位 | 說明 |
| --- | --- | --- |
| RED ON / RED OFF | PC3 | 紅燈獨立開／關 |
| YELLOW ON / YELLOW OFF | PC4 | 比 PPT 多一顆黃燈 |
| GREEN ON / GREEN OFF | PC6 | 綠燈獨立開／關 |

PPT 範例是 PC8 綠、PC6 紅，本版已明確改為你指定的 PC3 紅、PC4 黃、PC6 綠。三燈可同時亮，不會開紅燈就自動關掉其他顏色。

每個 GPIO 各串一顆 1kΩ 電阻接 LED 正極，LED 負極接 GND。預設為三顆獨立、有效高的 LED。

另外只補兩個小功能：合法指令執行後回覆 `OK 指令`，讓網頁確認；過長資料行不執行，避免截斷後誤判。

## CubeMX 要改的地方

| 點哪裡 | 設定 |
| --- | --- |
| System Core → SYS | Debug = Serial Wire；HAL Timebase = SysTick |
| Connectivity → USART2 | 此版不用，Disable／釋放 PA2、PA3 |
| Connectivity → LPUART1 | Asynchronous；TX/RX；115200、8 Bits、None、1 Stop Bit、無流量控制 |
| PA2 / PA3 | LPUART1_TX / LPUART1_RX，AF12 |
| LPUART1 → NVIC Settings | 勾選 LPUART1 global interrupt，Priority 5、Subpriority 0 |
| NVIC → Code generation | LPUART1 的 Generate IRQ handler 和 Call HAL handler 都保留勾選 |
| PC3 / PC4 / PC6 | GPIO_Output，Push-Pull、No Pull、初始 Low |
| Clock Configuration | 保留 HSI16 /4 ×85 /2 = 170MHz，AHB/APB1/APB2 都 /1 |

這次不用開 TIM2、TIM3、PWM、EXTI 或 DMA 傳輸。

**注意：這次使用的是 PPT 的標準 HAL 中斷方式，LPUART1 必須呼叫 `HAL_UART_IRQHandler()`。先前版本「取消 HAL handler」的指示不適用這一版。**

## 檔案放哪裡

1. 先依上表設定 `.ioc`、Generate Code，讓工程啟用 HAL UART 模組。維持初始化集中 main.c 的方式，不要另外產生並同時編譯另一份含 hlpuart1 的 usart.c。
2. 將本包 `Core/Src/main.c` 取代工程的 `Core/Src/main.c`。
3. 依 `IRQ_TO_MERGE.txt` 檢查 `stm32g4xx_it.c`：

```c
extern UART_HandleTypeDef hlpuart1;

void LPUART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&hlpuart1);
}
```

若已經由 CubeMX 產生同名函式，不要再加第二份。保留 SysTick_Handler 的 HAL_IncTick。
移除先前手動加入的 TypeRush_TIM2 / USART2 / EXTI IRQ 呼叫；本版不定義那些函式。
只需要本版 main.c 與標準 HAL LPUART1 入口，不要混入舊版 board.c / game.c / protocol.c 或另一個 main.c。

此包不是完整的 `.project/.ioc` CubeIDE 工程；你已經有工程，因此只需替換上述來源檔並合併 IRQ。
若重新 Generate Code，請確認 USER CODE 2、3、4 裡的內容保留，.ioc 腳位與設定也須和本版一致。

## 新網頁

發布路徑沿用 `https://jamie96123.github.io/Midterm-Project/web/`，這次頁面標題為 **Lab4｜STM32 三色燈控制**。
本包 `web/index.html` 是一個獨立檔案，沒有 npm、外部字型、CDN 或框架依賴。

1. 燒錄本版韌體，用 ST-LINK 的資料 USB 線連接 NUCLEO-G474RE。
2. 用電腦 Chrome/Edge 打開網頁，按「選擇序列埠並連接」，選 STLink Virtual COM Port。
3. 可先按板上 Reset，看 UART 對話是否收到 `HELLO WORLD`。開機字串只送一次，若開機時尚未連線，看不到屬正常。
4. 按紅／黃／綠燈的 ON/OFF。網頁送文字加 LF，約 200ms 後板子執行並回覆 OK，畫面才更新。
5. 也能手動輸入 `RED ON` 送出，或展開批次區送 PPT 四條指令。

網頁不會因為序列埠開啟就宣稱板端已有回應，也不會把未收到 OK 的指令畫成成功。這個確認只代表板端執行 GPIO 寫入，實體 LED 接線仍需目視驗收。

如需在自己電腦開啟本包網頁，可在本包根目錄執行：

```sh
python3 -m http.server 8000 --bind 127.0.0.1
```

再以本機瀏覽器開啟 `/web/`。Web Serial 要由 HTTPS 或 localhost 提供，請不要直接雙擊 HTML 用 file:// 測連線。

## PPT 的 200ms 驗證

Reset 後（計數歸零），一次送出：

```text
GREEN ON
RED ON
GREEN OFF
RED OFF
```

每行有 LF，總共 34 bytes。等待至少 300ms，再看 Debug：

- commandCount = 4
- rxDropped = 0
- rxHead = rxTail = 34
- 紅、綠燈都熄滅，黃燈維持原狀

十批、每批至少間隔 300ms，累計 340 bytes，預期 commandCount=40、head=tail=84。
這些數字以剛 Reset、沒有其他送入命令為前提。網頁不會自動送 SYNC 或初始化指令，避免干擾課堂計數。

環形陣列有 256 格、保留一格，可存 255 bytes。網頁單批限制 240 bytes；連續批次仍需留時間讓主迴圈處理，不能宣稱永不丟資料。rxDropped 可在 Debug 查看。

## 已驗證與待測

- 本版實際 main.c 已用官方 STM32G4 HAL v1.2.5、CMSIS-G4 v1.2.5、Arm GNU 14.2.1 編譯與連結。
- C 主機測試驗證 PPT 的 34/84 索引、容量 255、滿載丟棄、六條指令、獨立燈態、CRLF、過長行及接收回呼續約。
- 網頁測試使用模擬 Web Serial，不是實體 USB/STM32 驗證。
- 沒有接實體板，也沒有取得你的完整 `.ioc` / it.c / HAL config；尚未驗證你工程或實體燈號。
- VerifiedBuild 的 HEX/BIN 是用匹配的最小 HAL 環境與標準 LPUART1 IRQ 編譯，提供快速燒錄參考；正式仍建議在你的 CubeIDE 工程 Build。
