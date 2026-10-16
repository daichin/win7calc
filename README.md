# win7calc — Windows 7 小算盤 Linux 複刻版

用 Python 3 + GTK3 重寫的 Windows 7 經典小算盤（win32calc）。
[rcx/classic-win32calc](https://github.com/rcx/classic-win32calc) 只有執行檔，沒有原始碼。所以按鈕座標、選單、快捷鍵、錯誤訊息和字串，都是從它的 `win32calc.exe.mui` 資源表解析出來，再照原樣重建。按鈕和顯示區的 Aero 漸層，則是取樣原版點陣圖的色彩後，用 cairo 重新繪製（不包含任何微軟的圖檔）。

## 執行

```sh
./win7calc.sh                 # 或 python3 -m win7calc
./win7calc.sh --scale 1.5     # 放大（預設 1.0 = 原版 Win7 尺寸）
./win7calc.sh --mode sci      # 指定啟動模式：std / sci / prog / stat
```

### 透過 SSH 使用（遠端伺服器沒有顯示器）

- **X 轉發**：從有 X 伺服器的電腦用 `ssh -X user@host` 連線，再執行 `./win7calc.sh`。Windows 端可用 MobaXterm、VcXsrv，或 WSLg。
- **瀏覽器模式**：在伺服器上執行 `./win7calc.sh --web`。它只綁定 127.0.0.1:8085（可用 `WIN7CALC_PORT` 修改），所以要先在自己的電腦開通道 `ssh -L 8085:localhost:8085 user@host`，再用瀏覽器開 <http://localhost:8085/>。

需要：Python 3.8+、PyGObject（GTK 3）。大多數桌面發行版都已預裝（Ubuntu：`sudo apt install python3-gi gir1.2-gtk-3.0`）。

加入應用程式選單：

```sh
sed "s|@DIR@|$PWD|" win7calc.desktop > ~/.local/share/applications/win7calc.desktop
```

## 功能

| 檢視 | 內容 |
|---|---|
| 標準型 (Alt+1) | 即時運算（不考慮先乘除）、`%`、`√`、`1/x`、記憶體 MC/MR/MS/M+/M-、重複按 `=` |
| 工程型 (Alt+2) | 運算優先順序、括號 `(=n`、Inv、度／弧度／梯度、sin/cos/tan 與雙曲函數、ln/log、xʸ、ʸ√x、n!（含非整數 Γ）、dms、F-E、Exp、Mod、π，32 位有效數字 |
| 程式設計師 (Alt+3) | Hex/Dec/Oct/Bin、Qword/Dword/Word/Byte、And/Or/Xor/Not/Lsh/Rsh/RoL/RoR、可點擊切換的 64 位元面板 |
| 統計資料 (Alt+4) | 資料集清單、Count、x̄、x̄²、∑x、∑x²、σn、σn-1、CAD |
| 歷程記錄 (Ctrl+H) | 標準型／工程型的計算紀錄，點一下即可取回結果 |
| 擴充面板 | 單位換算 (Ctrl+U)、日期計算 (Ctrl+E)、工作表（房貸、汽車租賃、油耗 mpg 和 L/100 km） |
| 編輯 | 複製／貼上（貼上的文字會當成按鍵輸入，例如 `12*(3+4)=`）、複製歷程記錄／資料集 |

鍵盤快捷鍵和原版的加速鍵表一致，完整列表見「說明 → 檢視說明」（F1）。
檢視模式、數字分位、歷程記錄和擴充面板的設定，會存在 `~/.config/win7calc/settings.json`。

## 原版行為細節

- 顯示規則照 ratpak：標準型顯示 16 位、工程型顯示 32 位有效數字。小數點後超過 2 個零就改用科學記號（`1/30000 = 3.333333333333333e-5`），整數太大時顯示成 `1.e+16`。
- 運算式列顯示 `sqrt(9)`、`sind(30)`、`negate(5)`、`reciproc(4)` 等原版函數名稱，過長時左邊用 `«` 截斷。
- 錯誤訊息：`Cannot divide by zero`、`Invalid input`、`Result is undefined`、`Overflow`。
- `2 + 3 =` 之後輸入 `4 =` 會得到 7（重複上一個運算）；`5 + =` 得到 10。

## 專案結構

```
win7calc/
  engine.py     計算狀態機（命令 ID 沿用 win32calc 原始控制項 ID）
  mathfn.py     Decimal 高精度數學函數
  numfmt.py     顯示格式化
  layout.py     各模式版面（由對話框資源換算）
  render.py     Aero 風格 cairo 繪圖
  view.py       整個計算機畫面的繪製（可在沒有顯示器的環境下輸出）
  app.py        GTK 視窗、選單、鍵盤、剪貼簿
  panels.py     單位換算／日期計算／工作表面板
  units.py, datecalc.py, worksheets.py
tools/snapshot.py   不需顯示器就能輸出 PNG：python3 -m tools.snapshot out.png sci 2 ADD 3
tests/              python3 -m unittest discover -s tests
```
