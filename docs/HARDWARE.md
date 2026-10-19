# NUCLEO-G474RE 接線、燒錄與驗收

本版本預設 ST **NUCLEO-G474RE**（MCU STM32G474RET6）。若是其他 G474RE 自製板，需自行提供 ST-LINK/SWD、電源與對應的 UART 路由；不能只憑 MCU 名稱假設具有 Nucleo 的 USB VCP。

## 準備零件

- NUCLEO-G474RE 一片、可傳資料的 ST-LINK USB 線。
- 共陰極 RGB LED 一顆（或三顆獨立紅／綠／藍 LED）。請用 LED 資料表確認腳位，勿假設最長腳必然正確。
- **每個顏色各一顆 1 kΩ 限流電阻**，共三顆。
- 四顆常開瞬時按鈕、麵包板與杜邦線。

MCU GPIO 為 3.3 V。RGB 使用共陰極、有效高輸出；此程式不直接適用共陽極 LED。請勿將 5 V 或無限流的 LED 接到 GPIO。四腳按鈕的同側腳通常互通，須量測或確認接到按下才接通的兩端。

## 腳位表

下表名稱是 **MCU port/pin**，請對照板上 Morpho / Arduino 接頭的官方 pinout 找腳位，不是接頭的數字編號。

| 功能 | STM32 腳位 | 接法與模式 |
| --- | --- | --- |
| 紅 LED | PA6 | PA6 → 1 kΩ → RGB R 陽極，TIM3_CH1 AF2 |
| 綠 LED | PA7 | PA7 → 1 kΩ → RGB G 陽極，TIM3_CH2 AF2 |
| 藍 LED | PB0 | PB0 → 1 kΩ → RGB B 陽極，TIM3_CH3 AF2 |
| RGB 共陰極 | GND | 接開發板 GND，三色共用 |
| BTN 1 開始／繼續 | PB4 | PB4 → 常開按鈕 → GND；內部上拉，EXTI4 下降緣 |
| BTN 2 暫停／繼續 | PB5 | PB5 → 常開按鈕 → GND；EXTI5 下降緣 |
| BTN 3 燈色展示 | PB6 | PB6 → 常開按鈕 → GND；EXTI6 下降緣 |
| BTN 4 速率 | PB7 | PB7 → 常開按鈕 → GND；EXTI7 下降緣 |
| 板上 LD2 | PA5 | 不用外接；遊戲中每 500 ms 切換一次狀態 |
| USART2 TX | PA2 | AF7，Nucleo 預設路由至 ST-LINK VCP |
| USART2 RX | PA3 | AF7，Nucleo 預設路由至 ST-LINK VCP |
| SWD | PA13 / PA14 | 保留給 ST-LINK，不接遊戲零件 |

PC13 板載 B1 未使用；四顆按鈕全部按上表外接，避免依賴不同板次 B1 的電平／焊橋配置。PB4 不使用 JTAG 功能，改作一般 GPIO；SWD 的 PA13/PA14 仍可正常除錯。

```text
PA6 ── 1kΩ ── R 陽極 ┐
PA7 ── 1kΩ ── G 陽極 ├─ 共陰極 ── GND
PB0 ── 1kΩ ── B 陽極 ┘

PB4 ── BTN1（常開）── GND
PB5 ── BTN2（常開）── GND
PB6 ── BTN3（常開）── GND
PB7 ── BTN4（常開）── GND
```

使用 Nucleo 內建 ST-LINK 時，不需要另外接 PA2/PA3 到電腦。若使用外接 **3.3 V USB-UART**，接轉接器 TX→PA3、RX←PA2、GND↔GND；先依官方板手冊隔離 ST-LINK 與 PA3 的連線，避免兩個 TX 同時驅動。不要用傳統 ±RS-232 電平。

## 燒錄（推薦 STM32CubeProgrammer GUI）

1. 取得 [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html)，依作業系統安裝 ST-LINK 驅動／權限。
2. USB 插在板上的 **ST-LINK USB 接頭**，使用資料線，保持預設正常啟動／電源設定。
3. 開啟 CubeProgrammer，右側介面選 **ST-LINK**、Port 選 **SWD**，按 Connect。確認偵測到 STM32G474 系列及 512 KiB Flash。
4. 開啟已編譯的 `build/typerush.hex`（壓縮檔內也提供此檔），執行 Download，勾選 Verify。HEX 已包含地址。
5. 若使用 `typerush.bin`，指定起始位址 **0x08000000**，執行 Download / Verify。
6. 按 Reset / Run 或板上 Reset。預期 RGB 為低亮度藍燈；尚未開始倒數。
7. 關閉占用 VCP 的序列監控工具。啟動網頁，選 115200 / 8 / 1 / None / None，選 **STLink Virtual COM Port**。
8. 狀態出現「STM32 已連線」後，開始打字。若網頁只顯示序列埠已開啟而無有效狀態，請先查看 UART 對話。

CLI（已安裝且在 PATH、板子已接好時）：

```sh
STM32_Programmer_CLI -c port=SWD -w build/typerush.hex -v -rst
```

先用預編譯 HEX 也可操作，不必安裝編譯工具。要修改再依 README 執行 `./scripts/setup-firmware.sh`。雲端本身沒有你的 USB 裝置，**無法直接替你完成實體燒錄**。

## 待接板的驗收表

本次沒有實體板，下列全為 **待測**，不是已通過紀錄。

| 步驟 | 期望行為 |
| --- | --- |
| 上電／Reset | 藍色待機；UART 每約 100 ms 回報一行 TypeRush v1 JSON |
| 網頁連線／SYNC | 顯示 ready 狀態，不只開埠成功 |
| BTN 1 | 開始第 1 題，倒數約 8 秒，LED 由綠→黃→紅 |
| 打完一個單字 | 剩餘 >60% +7；30–60% +5；<30% +3，板端與網頁一致 |
| 故意打錯 | wrong / attempts 增加、pos 不前進，正確字元可接續 |
| BTN 2 | 倒數凍結；三色輸出恆亮；再次按下恢復呼吸與倒數 |
| BTN 3 | 手動輪播綠／黃／紅，不改時間判分；下題恢復自動燈色 |
| BTN 4 | 8／5／3 秒循環，剩餘比例保留，呼吸同步加速 |
| 按鈕去彈跳 | 一次短按只觸發一次；長按不重複輪播 |
| 不輸入直到結束 | 每題逾時換題，10 題後 finished、分數 0 |
| 連續輸入 | 無字元錯序；觀察 rxErrors、txDrops、protocolErrors |
| 拔 USB／重插 | 網頁停止傳送並顯示中斷；重連以板端狀態為準 |
| 示波器（可選） | PWM 載波約 1 kHz；TIM2 時間基準 10 ms，HSI 有標稱誤差 |

## 選用 PDF 的 HSE + PLL 時脈

預編譯 `build/typerush.hex` 是 HSI16 版，不需要晶振或 ST-LINK MCO。
若課程要求展示 **HSE + PLL**，程式也提供編譯選項，支援 8/16/24 MHz 輸入。
必須先依實際板次電路圖確認 HSE 焊橋／MCO 或晶振已配置，且頻率相符；不要用猜測的頻率。

例如：**已確認**板子有 24 MHz 外部時脈驅動 HSE 時：

```sh
make firmware BUILD_DIR=build/hse EXTRA_CFLAGS='-DTYPERUSH_HSE_HZ=24000000 -DTYPERUSH_HSE_BYPASS=1'
```

輸出在 `build/hse/`，分別與 HSI 版隔離；此分支已交叉編譯，但沒有實板時脈測試。
外部方波／ST-LINK MCO 使用 bypass=1；外接晶振使用 bypass=0。
時脈公式為 `HSE ÷ (HSE_HZ / 4MHz) ×85 ÷2 = 170MHz`。
選 HSE 而實際沒提供時脈時，韌體會停在 HSERDY 等待，沒有 UART 回應；改燒 HSI 版即可恢復。
修改旗標後請換不同 BUILD_DIR，或先清除該目錄的產物，因為 Make 不會自動追蹤旗標變動。

## 常見問題

- **找不到序列埠**：換資料 USB 線、確認 ST-LINK 驅動、作業系統的 COM / ttyACM 權限。Linux 可依系統管理方式授權使用者存取 ttyACM；不要任意放寬所有裝置權限。
- **Port busy / Access denied**：關閉 CubeIDE Serial Monitor、PlatformIO Monitor、PuTTY 等占用同一 VCP 的程式。
- **亂碼／JSON 解析失敗**：回到 115200 / 8N1 / None；確認已燒錄本專案韌體、不是原廠範例。
- **按鍵沒反應**：確認是 PB4–PB7、另一端 GND、按鈕不是接到原本已短路的兩腳；未使用板載 PC13 B1。
- **紅綠對調／燈不亮**：檢查 RGB 腳位與共陰極方向，各通道必須各有限流電阻。
- **黃燈偏紅或偏綠**：LED 的各色效率不同，可調整各通道電阻／亮度比例；邏輯黃燈由 R+G 合成。
- **燒錄連不上**：確認 SWD PA13/PA14 無外接干擾，可在 CubeProgrammer 嘗試 Connect under reset；不要修改遊戲串列參數來處理 SWD 問題。

官方板資料：[NUCLEO-G474RE](https://www.st.com/en/evaluation-tools/nucleo-g474re.html)。請以實際板次的使用手冊與電路圖確認焊橋及接頭位置。
