# TypeRush UART v1

USART2、PA2 TX / PA3 RX、115200 baud、8 data bits、no parity、1 stop bit、no flow control。每個訊息以 LF (`\n`) 結尾，接受 CRLF。指令為 ASCII，區分大小寫。瀏覽器使用 `TextEncoder` / 串流 `TextDecoder` 與跨 USB 封包的換行緩衝。

## 電腦 → STM32

| 指令 | 說明 |
| --- | --- |
| `SYNC` | 取得完整狀態，不重設遊戲 |
| `START` | 以目前速率開始新局（重設分數、進度） |
| `PAUSE` | running→paused，其餘狀態不變 |
| `RESUME` | paused→running，其餘狀態不變 |
| `RESET` | 回到預設待機、8 秒、0 分 |
| `KEY 97` | 十進位 ASCII：97 為小寫 a，32–126 才接受 |
| `COLOR` | 手動輪播綠／黃／紅，下題恢復自動燈色 |
| `SPEED` | 8→5→3→8 秒；保留當題剩餘比例 |

一次送一行，例如 `KEY 114\n`。字元本身不直接送成命令；所以空白、引號、換行不會破壞命令格式。Backspace、Enter、非 ASCII 與貼上不作為作答。

實體 BTN 1 與 `START` 略有差異：進行中忽略、暫停中繼續，以免誤觸重開；網頁進行中會停用開始鍵。沒有網頁也可用 UART 終端輸入上述指令，按鈕可在沒有網頁時開始／暫停遊戲。

指令上限 63 個字元；超長／非 ASCII 行丟棄至 LF。RX ring 為 512 bytes，滿溢或 UART FE/NE/ORE/PE 時捨棄不完整資料，並通知主迴圈捨棄正在組合的命令，避免執行殘缺指令。TX 2048-byte ring 只整行排入，空間不足則捨棄這次狀態並累計 txDrops；下一次 SYNC 或定期回報仍可取得真實狀態。

## STM32 → 電腦

每約 100 ms 及處理有效指令／按鈕／換題時送出完整 JSON 狀態。可能有重複狀態，接收端以最近一份有效狀態為準。不會傳送密鑰或帳號資料。

```json
{"t":"state","v":1,"state":"running","word":"rhythm","pos":2,"score":0,"correct":0,"wrong":0,"misses":0,"attempts":2,"hits":2,"question":1,"total":10,"remaining":7000,"duration":8000,"elapsed":1000,"rate":0,"color":"green","manual":0,"lastAward":0,"result":"hit","rxErrors":0,"txDrops":0,"protocolErrors":0}
```

| 欄位 | 定義 |
| --- | --- |
| t / v | state / 協定版本 1 |
| state | ready、running、paused、finished |
| word / pos | 當前題目與已正確輸入的字元數 |
| score / correct | 累計分數／完成題數 |
| wrong / misses | 錯字數／逾時題數，兩者不是相同指標 |
| attempts / hits | 所有作答字元／正確作答字元（已完成與當前題均計入） |
| question / total | 當前題號／局數 10；待機 question=0 |
| remaining / duration / elapsed | 當題剩餘 ms／當題時限 ms／整局有效遊戲 ms（排除暫停） |
| rate | 0=8 秒、1=5 秒、2=3 秒 |
| color / manual | LED 邏輯色 green/yellow/red/blue；是否手動覆蓋燈色 |
| lastAward / result | 最近一次完成題的分數／最近事件；不能藉 lastAward 重複加總 |
| rxErrors | RX ring 滿溢或 UART 接收錯誤數 |
| txDrops | TX ring 無足夠空間，捨棄完整狀態訊息的次數 |
| protocolErrors | 主迴圈拒絕的指令／資料行數 |

錯誤回應範例：`{"t":"error","code":"INVALID_KEY"}`。其他代碼：`UNKNOWN_COMMAND`、`RX_FRAME_DROPPED`。JSON 的最後還有 LF，範例不顯示。

接收端檢查協定版本、狀態、數值範圍與單字格式，拒絕無法辨識的訊息。開啟序列埠不等於連線完成；收到有效 state 才可操作。超過 1.5 秒無有效 state，網頁停止送出遊戲按鍵並顯示待回應，可按 SYNC 或重新連接。實際 USB 斷線會清理讀寫鎖並關閉 port。

這是可信任本機 USB 裝置的課堂協定，沒有實作網路多人、身分驗證或對抗惡意韌體的競賽防作弊機制。
