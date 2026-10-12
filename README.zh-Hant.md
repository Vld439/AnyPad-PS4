<p align="center"><img src="assets/icon0.png" width="180" alt="AnyPad PS5"></p>

# AnyPad PS5：讓你的 PS5 擺脫 DualSense

**適用於已越獄 PS5 的藍牙控制器橋接工具。**

[English · Español](README.md) · [Français](README.fr.md) · [Italiano](README.it.md) · [Português](README.pt.md) · [Deutsch](README.de.md) · [Русский](README.ru.md) · [简体中文](README.zh.md) · **繁體中文** · [日本語](README.ja.md) · [한국어](README.ko.md) · [العربية](README.ar.md)

---

AnyPad PS5 讓你在 PS5 上使用其他平台的控制器。它透過主機內建的藍牙晶片連接這些控制器，並以虛擬 DualSense 控制器的形式呈現給系統。

不需要 USB 轉接器。

**版本 0.9.1-beta。** 已在 PS5 fat、韌體 10.01 上，使用 DualShock 4 和 8BitDo SF30 Pro（Switch 模式）測試，涵蓋主機選單、遊戲和模擬器。

**提供十二種語言：** 英語、西班牙語、法語、義大利語、葡萄牙語、德語、俄語、簡體中文、繁體中文、日語、韓語和阿拉伯語。AnyPad 頁面會跟隨主機的語言。

---

## 關於這個專案

AnyPad PS5 開始於 2026 年 10 月 3 日 14:20 左右（西班牙時間），也就是我發布第一個版本的那一天。靈感來自 MissionControl，它讓 Nintendo Switch 可以使用其他主機的控制器。AnyPad 或許不是第一個讓 PS5 使用其他控制器的專案：先前已有透過 USB 連接的專案。但沒有任何一個透過藍牙連接，AnyPad 是第一個做到的。我有了一個想法，並且把它做了出來。

現在的目標是讓 AnyPad 支援更多控制器。如果你試用了它，請把你的記錄檔寄給我（見下文「協助我們確認相容性」）：每一份記錄檔都有幫助。

如果你開發模擬器並想整合 AnyPad，我願意與你分享程式碼。請來信至 [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com)。

仍有一個已知問題：如果在 AnyPad 執行時關閉 DualSense，它可能會一直藍燈閃爍。我希望今後的專案不會有這個問題。以我發布的 AnyPad 程式碼為基礎的分支（fork）很可能會繼承這個問題。這是 AnyPad 留下的印記，也說明至少在藍牙部分，它們是 AnyPad 的分支。

---

## 0.9 的新內容

- **相容性更好：** 支援更多控制器，並相容 PS5 中使用的兩種藍牙晶片。
- **修正了若干錯誤。**
- AnyPad 頁面採用**全新的圖形介面**。

## 0.8 的新內容

### 更快的配對

配對現在更快、更可靠。在我們的測試中，DualShock 4 和 8BitDo 幾秒鐘內就能被找到，再過幾秒就能使用，而以前往往需要多次嘗試。

### 更穩定

- **回應流暢：** 遊戲時控制器反應順暢，沒有延遲。
- **正常開機的控制器**（而不是進入配對模式）現在第一次就能配對。
- **更聰明的處理：** 針對回應緩慢或拒絕某一步的控制器。

### 設定 PS

對於沒有可用 PS 鍵的控制器（Switch Pro、8BitDo 等），**設定 PS** 按鈕可讓你選擇**一個按鍵或三個按鍵的組合**來充當 PS 鍵。選擇數量，在控制器上按下這些按鍵並保持一秒：AnyPad 會自動偵測。該設定會針對此控制器型號儲存。

DualShock 4 和 DualSense 本身就有 PS 鍵，因此不會顯示此選項。

### 十二種語言

AnyPad 頁面跟隨主機語言：英語、西班牙語、法語、義大利語、葡萄牙語、德語、俄語、簡體中文、繁體中文、日語、韓語和阿拉伯語（由右至左）。其他語言使用英語。

---

## ⚠️ 重要：AnyPad 執行時請勿關閉 DualSense

如果在 AnyPad 執行時關閉 DualSense，它可能進入藍燈閃爍模式，在主機重新啟動前無法使用。

為避免主機在一段時間未使用後自動將其關閉，請設定：

**設定 → 系統 → 節能 → 設定控制器關閉前的時間 → 不關閉**

如果你不關閉它，一切正常。

---

## ⚠️ 重要：外接控制器請選擇「快速遊戲 / 訪客」

當你在外接控制器上按下 PS 鍵時，主機會詢問由誰使用。請選擇**快速遊戲 / 訪客**。你的主要使用者保留 DualSense，隨時拿起 DualSense 即可回到它。

---

# 控制器相容性

| 控制器 | 狀態 | 備註 |
| --- | --- | --- |
| **DualShock 4** | ✅ **已驗證** | 每次配對都要讓它進入配對模式（Share + PS）。 |
| **8BitDo SF30 Pro（Switch 模式）** | ✅ **已驗證** | 顯示為 Switch Pro Controller。PS 鍵請使用「設定 PS」。 |
| **GameSir G7 SE** | ✅ **已驗證** | |
| **Nintendo Switch Pro Controller** | 🧪 **已支援，尚未測試** | 與 Switch 模式下的 8BitDo 協定相同。 |
| **Xbox 控制器** | ✅ **可用（使用者回報）** | 多位使用者確認從 0.8 起可以使用。如果你的控制器無法連線，請把記錄檔和型號寄給我。 |
| **任何其他藍牙 HID 控制器** | 🧪 **通用設定檔** | 沒有專用設定檔也可能可用。 |

**我們認為可能相容的控制器**（程式碼中已支援，尚未在主機上測試）：DualShock 4 v1、Hori Onyx、Razer Raiju Ultimate、Razer Raiju Tournament Edition、SCUF Vantage 2、Nintendo Switch Pro Controller、Nintendo Switch Online（SNES、N64、Mega Drive / Genesis）、GameSir G3s、G4s、G7 Pro、T1s 和 T2a、Betop 2585N2、Bionik Vulkan、LanShen X1Pro、Xiaomi Mi Controller、Amazon Fire / iPega、iPega 9-series、8BitDo SN30 Pro（Xbox Cloud Gaming 版）、8BitDo Ultimate 2.4G、SteelSeries、NVIDIA Shield 控制器（2017）、Razer Serval、PowerA MOGA Hero、MOGA Pro 和 MOGA Pro 2、Atari VCS Modern 控制器、Hyperkin Scout、Mocute 050。如果你試用了其中任何一款，請把記錄檔寄給我。

完整的逐款清單請見 [COMPATIBILITY.md](COMPATIBILITY.md)。

---

# 運作方式

AnyPad PS5 以常駐 payload 的形式執行在已越獄的 PS5 上。它使用主機的藍牙晶片，**與系統共用**，來探索並配對控制器、讀取輸入，並把它們作為虛擬 DualSense 呈現給 PS5。

它**不會**修改遊戲、PS5 韌體或越獄本身。你的 PS5 必須已經能夠執行 payload。

---

# 使用方法

1. **載入 AnyPad。** 從 [Releases](https://codeberg.org/elmonomalva0/Any-Pad-ps5/releases) 下載 `AnyPad-PS5-0.9.1-beta.elf`，用你平常使用的 payload 載入器載入。
2. **開啟頁面。** 在主機的「媒體」列開啟 **AnyPad**，或在同一網路的手機或電腦上開啟 `http://<主機IP>:8095`。
3. **配對。** 按下**配對**，然後在靠近主機的地方讓控制器進入配對模式。

| 控制器 | 配對模式 |
| --- | --- |
| **DualShock 4** | 按住 **Share + PS**，直到燈條閃爍兩次 |
| **8BitDo（Switch 模式）** | 按住 **Pair** 約 3 秒，直到指示燈快速閃爍 |
| **Nintendo Switch Pro** | 按下 USB-C 連接埠旁的小型同步鍵 |
| **其他控制器** | 依照製造商的說明操作 |

4. **選擇使用者。** 主機詢問時，選擇**快速遊戲 / 訪客**。

可以同時使用多個控制器，並與 DualSense 一起使用。

---

# 每次執行都從零開始

AnyPad 不會在兩次執行之間儲存已配對的控制器。每次啟動 AnyPad 都要重新配對控制器。這樣可以避免失效的舊配對，並讓藍牙晶片處於已知狀態。

如果一個被關閉的控制器自行重新連線卻沒有反應，請將其關閉，然後在頁面上重新配對。

---

# 停止 AnyPad

請務必使用頁面上的**關閉 AnyPad PS5**來停止它，包括再次執行之前，以及關閉或重新啟動主機之前。**離開**按鈕只會關閉頁面。

---

# 疑難排解

### 找不到任何控制器

1. 先按**配對**，之後再讓控制器進入配對模式。
2. 確認控制器確實處於配對模式（例如 DualShock 4 的燈條必須閃爍兩次）。
3. 如果控制器以前與其他裝置配對過，仍然沒有出現，請將其重設（DualShock 4：用迴紋針按 L2 旁的小孔約 5 秒；8BitDo Pro 2：按住 Pair 約 5 秒），然後重試。

### 另一個控制器 payload 正在執行

另一個使用藍牙晶片和 8095 連接埠的 payload 會造成干擾。執行 AnyPad 之前，請先在它自己的選單中停止它；AnyPad 偵測到這種情況時會顯示提示。

### 頁面打不開

請確認 AnyPad 正在執行，手機或電腦與 PS5 在同一網路，並且使用的是主機的 IP 位址加連接埠 `8095`。

---

# 協助我們確認相容性

如果你能確認 AnyPad PS5 可以（或不可以）與某款控制器搭配使用，請把記錄檔（`/data/anypad/anypad.log`）寄到 [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com)，並註明控制器型號、主機韌體、越獄方式和 AnyPad 版本。這非常有幫助。

### 支持這個專案

很多人請我測試其他控制器和其他主機韌體。非常歡迎任何協助，把本專案**移植到 PS4** 和 **PS5 韌體 13.60**，並支援**更多控制器**。除了我這台 10.01 的主機，我也很想有一台 13.60 韌體的主機。這不是強制的，但想幫助這個專案的人可以透過我的 Throne 頁面：**[throne.com/elmonomalvad0](https://throne.com/elmonomalvad0)**

---

# 致謝

AnyPad PS5 是原創程式碼。我們研究了這些專案以理解其運作方式，並為 AnyPad 重新編寫了其中的想法。感謝它們的作者：

- **[FGG-PlayPods](https://github.com/FGGstore/FGG-PlayPods)**（FGGstore）：藍牙部分的想法，即與系統共用 PS5 內建的藍牙晶片。
- **[MissionControl](https://github.com/ndeadly/MissionControl)**（ndeadly）：依廠商和型號劃分的控制器設定檔，以及許多控制器的報告格式。
- **Ghostcontrol**（StonedModder）：如何在 PS5 上建立虛擬 DualSense 並指派給使用者。
- **[SplashDown](https://github.com/seregonwar/SplashDown)**（seregonwar）和 **[shadPS4](https://github.com/shadps4-emu/shadPS4)**：虛擬控制器的流程和控制器資料格式。
- **[HearBridge-PS5](https://github.com/X-F1REBALL-X/HearBridge-PS5)**（X-F1REBALL-X）：關於藍牙協商的想法。
- **[ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk)**（John Törnblom）：用於建置一切的工具鏈。

授權：GPL-3.0-or-later（[LICENSE](LICENSE)）。

作者：**elmonomalvad0**，與 *sinfiltros*（GitHub 上最初的 AnyPad PS5）是同一位作者。
X：[@elmonomalvad0](https://x.com/elmonomalvad0) · 聯絡：exposed.ct.ws@gmail.com
