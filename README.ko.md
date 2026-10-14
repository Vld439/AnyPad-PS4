<p align="center"><img src="assets/icon0.png" width="180" alt="AnyPad PS5"></p>

# AnyPad PS5: PS5를 DualSense에서 해방하세요

**탈옥된 PS5를 위한 블루투스 컨트롤러 브리지.**

[English · Español](README.md) · [Français](README.fr.md) · [Italiano](README.it.md) · [Português](README.pt.md) · [Deutsch](README.de.md) · [Русский](README.ru.md) · [简体中文](README.zh.md) · [繁體中文](README.zh-Hant.md) · [日本語](README.ja.md) · **한국어** · [العربية](README.ar.md)

---

AnyPad PS5를 사용하면 다른 플랫폼의 컨트롤러를 PS5에서 쓸 수 있습니다. 본체의 블루투스 칩을 통해 컨트롤러를 연결하고, 시스템에는 가상 DualSense 컨트롤러로 보여 줍니다.

USB 어댑터가 필요 없습니다.

**버전 0.9.5-beta.** PS5 fat, 펌웨어 10.01에서 DualShock 4와 8BitDo SF30 Pro(Switch 모드)로 본체 메뉴, 게임, 에뮬레이터에서 테스트했습니다.

**12개 언어 지원:** 영어, 스페인어, 프랑스어, 이탈리아어, 포르투갈어, 독일어, 러시아어, 중국어 간체, 중국어 번체, 일본어, 한국어, 아랍어. AnyPad 페이지는 본체의 언어를 따릅니다.

---

## 이 프로젝트에 대하여

AnyPad PS5는 첫 버전을 공개한 2026년 10월 3일 14시 20분경(스페인 시간)에 시작되었습니다. 아이디어는 닌텐도 스위치에서 다른 콘솔의 컨트롤러를 쓸 수 있게 해 주는 MissionControl에서 왔습니다. AnyPad가 PS5에서 다른 컨트롤러를 쓰게 한 첫 프로젝트는 아닐 수 있습니다. USB로 연결하는 프로젝트는 이미 있었습니다. 하지만 블루투스로 연결하는 것은 없었고, AnyPad가 처음이었습니다. 아이디어가 떠올랐고, 그것을 실행에 옮겼습니다.

이제 목표는 AnyPad가 더 많은 컨트롤러에서 동작하게 하는 것입니다. 써 보셨다면 로그를 보내 주세요(아래 '호환성 확인에 도움을 주세요' 참고). 로그 하나하나가 도움이 됩니다.

소스 코드 전체가 이 저장소에 GPL-3.0-or-later 라이선스로 공개되어 있습니다(아래 「오픈 소스, 그리고 AI와 함께 작업하기」 참조). 에뮬레이터를 개발하면서 AnyPad를 통합하고 싶으시다면 그 라이선스에 따라 하실 수 있습니다. 도움이 필요하면 [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com)으로 연락해 주세요.

알려진 문제가 하나 남아 있습니다. AnyPad가 실행 중일 때 DualSense를 끄면 파란색으로 깜빡이는 상태로 남을 수 있습니다. 앞으로의 프로젝트에는 이 문제가 없기를 바랍니다. 제가 공개한 코드를 바탕으로 만든 AnyPad의 포크는 이 문제를 그대로 물려받을 가능성이 매우 큽니다. 이는 AnyPad가 남긴 유산이며, 적어도 블루투스 부분에서는 AnyPad의 포크라는 표시입니다.

---

## 오픈 소스, 그리고 AI와 함께 작업하기

0.9.5 버전부터 AnyPad PS5의 소스 코드 전체가 이 저장소에 GPL-3.0-or-later 라이선스로 공개됩니다. 블루투스 호스트, 컨트롤러 프로파일, AnyPad 페이지, 테스트, 빌드 스크립트가 들어 있습니다. 시뮬레이터도 포함되어 있어 본체 없이 컴퓨터에서 컨트롤러를 확인할 수 있습니다.

저는 AnyPad가 브리지 자체보다 진짜로 더하는 것이 **AI의 도움으로 새 컨트롤러를 통합하기 위한 통신 시스템**이라고 생각합니다. AnyPad가 실행되는 동안 그 페이지는 로컬 네트워크에서 다음을 제공합니다.

- 실시간 로그.
- 다시 시작하거나 다시 빌드하지 않고도 실행 중에 바꿀 수 있는 블루투스 연결 설정 몇 가지.
- 페어링 버튼.

같은 네트워크의 컴퓨터에서 터미널을 쓸 수 있는 AI 어시스턴트(예: Claude Code, Codex CLI, Gemini CLI)는 컨트롤러를 켜는 동안 AnyPad가 하는 일을 따라가고, 변경을 제안하고, ELF를 다시 빌드해 본체에 올리고, 다음 실행에서 결과를 확인할 수 있습니다. AnyPad 자체도 이렇게 개선되어 왔습니다.

**[AI-WORKFLOW.md](AI-WORKFLOW.md)** 파일(영어)은 그런 어시스턴트를 위한 안내서입니다. 연결하는 방법, 건드려도 되는 것과 안 되는 것, 변경을 테스트하는 방법, 새 컨트롤러를 추가하는 방법이 들어 있습니다. 첫 번째 지시로 어시스턴트에게 주면 됩니다.

유의하세요.

- 브라우저의 채팅으로는 본체에 닿을 수 없습니다. 어시스턴트는 PS5와 같은 로컬 네트워크에 연결된 컴퓨터의 터미널이 필요합니다. AnyPad 자체는 인터넷에 연결하지 않습니다.
- 모든 버전과 마찬가지로 페이지에는 PIN이 없습니다. 로컬 네트워크의 누구나 로그를 읽고 AnyPad에 명령을 보낼 수 있습니다. 신뢰할 수 있는 네트워크에서만 사용하고, 끝나면 AnyPad를 중지하세요.
- 로그에는 컨트롤러의 블루투스 주소와 본체 사용자의 ID가 들어 있습니다. 공유하기 전에 확인하세요.

### 여러분(과 AI)이 알게 된 것을 공유해 주세요

AI를 쓰든 쓰지 않든 AnyPad를 어떤 컨트롤러에 맞게 조정했다면, 컨트롤러 모델, 본체 펌웨어, 탈옥 방법, AnyPad 버전, 무엇을 왜 바꿨는지, 그리고 로그와 함께 [이슈](https://codeberg.org/elmonomalva0/Any-Pad-ps5/issues)를 열어 주세요. AI가 도왔다면 그렇게 밝히고, 설명은 AI가 직접 쓰게 하세요. 컨트롤러가 무엇이 다른지, 무엇을 바꿨는지, 어떻게 테스트했는지입니다. 모든 제안을 검토하고 호환되는 것은 이후 버전에 통합하겠습니다. 변경은 `make test`를 계속 통과해야 하며, DualShock 4와 8BitDo가 연결되는 방식을 바꾸어서는 안 됩니다.

### 소스에서 빌드하기

[ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk)가 필요합니다.

```
make test                                               # 컴퓨터에서 하는 테스트 (블루투스 시뮬레이터 사용)
make ps5 PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk       # dist/ 에 ELF 생성
tools/build-elf-docker.sh /path/to/ps5-payload-sdk      # 또는 같은 일을 컨테이너 안에서
```

### 저를 언급해 주세요

AnyPad의 자체 버전을 만들거나 AnyPad(또는 그 일부)를 에뮬레이터, HEN, payload 또는 다른 프로그램에 포함시킨다면, **elmonomalvad0**를 언급하고 이 프로젝트에 링크해 주시면 정말 고맙겠습니다. GPL-3.0-or-later 라이선스는 이미 저작권 고지를 유지하고 변경 사항을 같은 라이선스로 배포하도록 요구합니다. 언급은 조건이 아니라 부탁이며, 이 작업을 계속 공유할 보람을 주는 것입니다.

### 코드 구성

| 위치 | 하는 일 |
| --- | --- |
| `src/host.c` | 블루투스 Classic 호스트: 검색, 호출, 페어링, L2CAP 채널, SDP, HID |
| `src/le.c` | 블루투스 LE: 스캔, Secure Connections 페어링과 신원 교환, GATT 기반 HID |
| `src/profiles.c` | 컨트롤러 계열마다 프로파일 하나: ID, 리포트를 읽는 방법, 깨우기 순서, 진동과 조명 |
| `src/generic.c`, `maps/` | 그 밖의 모든 HID 컨트롤러. 자체 디스크립터와 선택 사항인 매핑 파일로 읽습니다 |
| `src/hci_usb.c`, `src/usb_desc.c`, `src/evstream.c` | 본체의 블루투스 칩(시스템과 공유)과 USB로 통신하는 HCI |
| `src/ps5_vpad.c` | 가상 DualSense를 만들고 데이터를 공급합니다 |
| `src/web.c`, `web/index.html` | 페이지와 그 API |
| `src/ps5_main.c` | payload의 메인 루프 |
| `tests/` | 블루투스 시뮬레이터(`sim.c`, `sim_le.c`)와 모든 테스트 |

컨트롤러를 추가하는 일은 대부분 `src/profiles.c`에 프로파일 하나를 추가하고 시뮬레이터로 테스트를 만드는 것입니다. 자세한 내용은 [AI-WORKFLOW.md](AI-WORKFLOW.md)에 있습니다.

---

## 0.9.5의 새로운 점

- **오픈 소스:** 코드 전체를 공개하고, AI 어시스턴트와 함께 작업하는 안내서를 추가했습니다.

## 0.9의 새로운 점

- **호환성 향상:** 더 많은 컨트롤러를 지원하며, PS5에 쓰이는 두 가지 블루투스 칩 모두와 호환됩니다.
- **버그 수정.**
- AnyPad 페이지의 **새로운 그래픽 인터페이스**.

## 0.8의 새로운 점

### 더 빠른 페어링

페어링이 훨씬 빠르고 안정적으로 바뀌었습니다. 저희 테스트에서 DualShock 4와 8BitDo는 몇 초 안에 발견되고 몇 초 뒤에 사용할 수 있게 됩니다. 이전 버전에서는 여러 번 시도해야 하는 경우가 많았습니다.

### 더 안정적

- **부드러운 반응:** 플레이하는 동안 컨트롤러가 지연 없이 부드럽게 반응합니다.
- **페어링 모드가 아니라 평소처럼 켠 컨트롤러**도 이제 첫 시도에 페어링됩니다.
- **응답이 느리거나 어떤 단계를 거부하는 컨트롤러**를 더 똑똑하게 처리합니다.

### PS 설정

쓸 수 있는 PS 버튼이 없는 컨트롤러(Switch Pro, 8BitDo 등)는 **PS 설정** 버튼으로 PS 버튼 역할을 할 **버튼 1개 또는 3개의 조합**을 고를 수 있습니다. 개수를 고르고, 컨트롤러에서 그 버튼들을 눌러 1초간 유지하면 AnyPad가 스스로 인식합니다. 설정은 해당 컨트롤러 모델별로 저장됩니다.

DualShock 4와 DualSense는 자체 PS 버튼이 있으므로 이 옵션이 표시되지 않습니다.

### 12개 언어

AnyPad 페이지는 본체 언어를 따릅니다. 영어, 스페인어, 프랑스어, 이탈리아어, 포르투갈어, 독일어, 러시아어, 중국어 간체, 중국어 번체, 일본어, 한국어, 아랍어(오른쪽에서 왼쪽). 그 밖의 언어는 영어를 사용합니다.

---

## ⚠️ 중요: AnyPad가 실행 중일 때는 DualSense를 끄지 마세요

AnyPad가 실행 중일 때 DualSense를 끄면 파란색 깜빡임 모드로 들어가 본체를 다시 시작할 때까지 작동하지 않을 수 있습니다.

한동안 사용하지 않았을 때 본체가 알아서 끄지 않도록 다음과 같이 설정하세요.

**설정 → 시스템 → 절전 → 컨트롤러가 꺼질 때까지의 시간 설정 → 끄지 않음**

끄지 않으면 모든 것이 정상적으로 작동합니다.

---

## ⚠️ 중요: 외부 컨트롤러는 '빠른 플레이 / 게스트'를 선택하세요

외부 컨트롤러에서 PS 버튼을 누르면 본체가 누가 사용할지 묻습니다. **빠른 플레이 / 게스트**를 선택하세요. 메인 사용자는 DualSense를 그대로 쓰고, DualSense를 다시 집어 들면 언제든 돌아갈 수 있습니다.

---

# 컨트롤러 호환성

| 컨트롤러 | 상태 | 비고 |
| --- | --- | --- |
| **DualShock 4** | ✅ **검증됨** | 페어링할 때마다 페어링 모드(Share + PS)로 만드세요. |
| **8BitDo SF30 Pro(Switch 모드)** | ✅ **검증됨** | Switch Pro Controller로 인식됩니다. PS 버튼은 'PS 설정'을 사용하세요. |
| **GameSir G7 SE** | ✅ **검증됨** | |
| **Nintendo Switch Pro Controller** | 🧪 **지원, 아직 테스트 전** | Switch 모드의 8BitDo와 같은 프로토콜입니다. |
| **Xbox 컨트롤러** | ✅ **작동 (사용자 보고)** | 0.8부터 작동한다는 사용자 보고가 있습니다. 연결되지 않으면 로그와 모델명을 보내 주세요. |
| **그 밖의 블루투스 HID 게임패드** | 🧪 **범용 프로필** | 전용 프로필 없이도 작동할 수 있습니다. |

**호환될 것으로 보는 컨트롤러**(코드에서는 지원, 실제 본체에서는 아직 테스트 전): DualShock 4 v1, Hori Onyx, Razer Raiju Ultimate, Razer Raiju Tournament Edition, SCUF Vantage 2, Nintendo Switch Pro Controller, Nintendo Switch Online(SNES, N64, Mega Drive / Genesis), GameSir G3s·G4s·G7 Pro·T1s·T2a, Betop 2585N2, Bionik Vulkan, LanShen X1Pro, Xiaomi Mi Controller, Amazon Fire / iPega, iPega 9-series, 8BitDo SN30 Pro(Xbox Cloud Gaming용), 8BitDo Ultimate 2.4G, SteelSeries, NVIDIA Shield 컨트롤러(2017), Razer Serval, PowerA MOGA Hero·MOGA Pro·MOGA Pro 2, Atari VCS Modern 컨트롤러, Hyperkin Scout, Mocute 050. 이 중 하나를 써 보셨다면 로그를 보내 주세요.

모델별 전체 목록은 [COMPATIBILITY.md](COMPATIBILITY.md)에 있습니다.

---

# 작동 방식

AnyPad PS5는 탈옥된 PS5에서 상주 payload로 실행됩니다. 본체의 블루투스 칩을 **시스템과 공유**하면서 컨트롤러를 찾아 페어링하고, 입력을 읽어, 가상 DualSense로 PS5에 보여 줍니다.

게임, PS5 펌웨어, 탈옥 자체를 **수정하지 않습니다**. PS5가 이미 payload를 실행할 수 있어야 합니다.

---

# 사용 방법

1. **AnyPad 불러오기.** [Releases](https://codeberg.org/elmonomalva0/Any-Pad-ps5/releases)에서 `AnyPad-PS5-0.9.5-beta.elf`를 내려받아 평소 쓰는 payload 로더로 불러옵니다.
2. **페이지 열기.** 본체의 '미디어' 줄에서 **AnyPad**를 열거나, 같은 네트워크의 휴대폰이나 컴퓨터에서 `http://<본체-IP>:8095`를 엽니다.
3. **페어링.** **페어링**을 누른 뒤, 본체 가까이에서 컨트롤러를 페어링 모드로 만듭니다.

| 컨트롤러 | 페어링 모드 |
| --- | --- |
| **DualShock 4** | 라이트 바가 두 번 깜빡일 때까지 **Share + PS**를 누릅니다 |
| **8BitDo(Switch 모드)** | 표시등이 빠르게 깜빡일 때까지 **Pair**를 약 3초 누릅니다 |
| **Nintendo Switch Pro** | USB-C 포트 옆의 작은 동기화 버튼을 누릅니다 |
| **그 밖의 컨트롤러** | 제조사의 안내를 따릅니다 |

4. **사용자 선택.** 본체가 물으면 **빠른 플레이 / 게스트**를 선택합니다.

DualSense와 함께 여러 컨트롤러를 동시에 사용할 수 있습니다.

---

# 실행할 때마다 깨끗하게 시작합니다

AnyPad는 실행 사이에 페어링된 컨트롤러를 저장하지 않습니다. AnyPad를 시작할 때마다 컨트롤러를 페어링하세요. 이렇게 하면 더 이상 작동하지 않는 오래된 페어링을 피하고, 블루투스 칩을 알려진 상태로 유지할 수 있습니다.

꺼 두었던 컨트롤러가 스스로 다시 연결되었는데 반응이 없다면, 끈 다음 페이지에서 다시 페어링하세요.

---

# AnyPad 종료

항상 페이지의 **AnyPad PS5 끄기**로 종료하세요. 다시 실행하기 전과 본체를 끄거나 재시작하기 전에도 마찬가지입니다. **나가기** 버튼은 페이지만 닫습니다.

---

# 문제 해결

### 컨트롤러가 발견되지 않아요

1. 먼저 **페어링**을 누르고, 그다음에 컨트롤러를 페어링 모드로 만드세요.
2. 컨트롤러가 정말 페어링 모드인지 확인하세요(예: DualShock 4의 라이트 바가 두 번 깜빡여야 합니다).
3. 컨트롤러가 이전에 다른 기기와 페어링된 적이 있고 여전히 나타나지 않으면 초기화한 뒤 다시 시도하세요(DualShock 4: L2 옆의 작은 구멍에 클립을 넣고 약 5초, 8BitDo Pro 2: Pair를 약 5초 누르기).

### 다른 컨트롤러 payload가 실행 중이에요

블루투스 칩과 8095 포트를 쓰는 다른 payload가 있으면 방해가 됩니다. AnyPad를 실행하기 전에 그 payload의 메뉴에서 종료하세요. AnyPad는 이를 감지하면 화면에 알려 줍니다.

### 페이지가 열리지 않아요

AnyPad가 실행 중인지, 휴대폰이나 컴퓨터가 PS5와 같은 네트워크에 있는지, 본체의 IP 주소에 포트 `8095`를 붙여 쓰고 있는지 확인하세요.

---

# 호환성 확인에 도움을 주세요

AnyPad PS5가 어떤 컨트롤러에서 작동하는지(또는 작동하지 않는지) 확인하셨다면, 로그(`/data/anypad/anypad.log`)를 [exposed.ct.ws@gmail.com](mailto:exposed.ct.ws@gmail.com)으로 보내 주세요. 컨트롤러 모델, 본체 펌웨어, 탈옥, AnyPad 버전을 함께 알려 주시면 큰 도움이 됩니다.

### 프로젝트 후원

다른 컨트롤러와 다른 본체 버전도 테스트해 달라는 요청을 많이 받습니다. **이 프로젝트를 PS4와 PS5 펌웨어 13.60으로 이식**하고 **더 많은 컨트롤러를 지원**하는 데 도움을 주시면 큰 힘이 됩니다. 10.01 본체 외에 13.60 본체도 갖고 싶습니다. 의무는 아니지만, 프로젝트를 돕고 싶은 분은 제 Throne 페이지에서 할 수 있습니다: **[throne.com/elmonomalvad0](https://throne.com/elmonomalvad0)**

---

# 크레딧

AnyPad PS5는 독자적인 코드입니다. 다음 프로젝트들을 작동 원리를 이해하려고 살펴보았고, 그 아이디어를 AnyPad를 위해 새로 작성했습니다. 저자분들께 감사드립니다.

- **[FGG-PlayPods](https://github.com/FGGstore/FGG-PlayPods)**(FGGstore): 블루투스 부분의 아이디어, 즉 PS5의 블루투스 칩을 시스템과 공유하는 것.
- **[MissionControl](https://github.com/ndeadly/MissionControl)**(ndeadly): 제조사·모델별 컨트롤러 프로필과 여러 컨트롤러의 리포트 형식.
- **Ghostcontrol**(StonedModder): PS5에서 가상 DualSense를 만들어 사용자에게 할당하는 방법.
- **[SplashDown](https://github.com/seregonwar/SplashDown)**(seregonwar)과 **[shadPS4](https://github.com/shadps4-emu/shadPS4)**: 가상 컨트롤러의 절차와 컨트롤러 데이터 형식.
- **[HearBridge-PS5](https://github.com/X-F1REBALL-X/HearBridge-PS5)**(X-F1REBALL-X): 블루투스 협상에 관한 아이디어.
- **[ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk)**(John Törnblom): 모든 것을 빌드하는 데 쓰는 툴체인.

라이선스: GPL-3.0-or-later([LICENSE](LICENSE)).

저자: **elmonomalvad0**, GitHub의 오리지널 AnyPad PS5를 만든 *sinfiltros*와 같은 저자입니다.
X: [@elmonomalvad0](https://x.com/elmonomalvad0) · 연락처: exposed.ct.ws@gmail.com
