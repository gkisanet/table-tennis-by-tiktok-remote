# 🏓 탁구 전자 점수판 - BLE 버전 (Table Tennis Digital Scoreboard)

ESP32-S3와 P5 LED 모듈을 사용한 BLE 탁구 전자 점수판입니다.  
TikTok BLE 리모컨(JX-05)으로 점수를 조작할 수 있습니다.

> 📌 **Bluetooth Classic 버전**은 [`classic-bt`](https://github.com/gkisanet/table-tennis-by-tiktok-remote/tree/classic-bt) 브랜치를 참고하세요.

---

## 📌 프로젝트 개요

| 항목 | 설명 |
|------|------|
| **MCU** | ESP32-S3 |
| **디스플레이** | P5 LED 모듈 (64x32, HUB75 인터페이스) |
| **리모컨** | TikTok BLE 리모컨 (JX-05) |
| **통신** | Bluetooth Low Energy (BLE) HID |
| **개발 환경** | ESP-IDF v5.5.2 |

### ✨ 주요 기능
- 🎮 BLE 리모컨으로 점수 조작
- 📊 세트 점수 및 게임 점수 표시
- 🔄 서브권 표시 (녹색 탈리 바)
- ⚡ 자동 서브권 교체 (2점마다, 듀스 시 1점마다)
- 🔌 연결 끊김 감지 및 자동 재연결
- ↩️ 실행취소 (Undo) 기능

---

## ⚠️ 사용 전 필수 설정: 리모컨 MAC 주소

> [!CAUTION]
> 이 프로젝트를 사용하려면 **본인의 TikTok BLE 리모컨 MAC 주소**를 코드에 직접 입력해야 합니다.

### 1단계: 리모컨 MAC 주소 확인

리모컨의 MAC 주소를 모르는 경우, 먼저 코드를 빌드하여 ESP32-S3에 플래시하세요.  
시리얼 모니터에서 스캔된 BLE 장치 목록이 표시됩니다:

```
BLE: 6c:86:80:f2:7e:7f, RSSI: -45, APPEARANCE: 0x03c1, NAME: JX-05
```

여기서 `6c:86:80:f2:7e:7f`가 리모컨의 MAC 주소입니다.

### 2단계: 코드에 MAC 주소 입력

`main/esp_hid_host_main.c` 파일에서 아래 부분을 **본인의 리모컨 MAC 주소**로 변경하세요:

```c
// main/esp_hid_host_main.c
static const uint8_t TARGET_BDA[6] = {0x6c, 0x86, 0x80, 0xf2, 0x7e, 0x7f};
//                                     ↑ 이 값들을 본인의 MAC 주소로 변경!
```

예를 들어, MAC 주소가 `AA:BB:CC:DD:EE:FF`라면:
```c
static const uint8_t TARGET_BDA[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
```

---

## 🔧 하드웨어 연결 (GPIO 핀맵 - ESP32-S3)

### HUB75 데이터 핀
| ESP32-S3 GPIO | P5 LED | 설명 |
|---------------|--------|------|
| GPIO 4 | R1 | 상단 빨강 |
| GPIO 5 | G1 | 상단 녹색 |
| GPIO 6 | B1 | 상단 파랑 |
| GPIO 7 | R2 | 하단 빨강 |
| GPIO 15 | G2 | 하단 녹색 |
| GPIO 16 | B2 | 하단 파랑 |

### HUB75 제어 핀
| ESP32-S3 GPIO | P5 LED | 설명 |
|---------------|--------|------|
| GPIO 18 | A | 행 주소 A |
| GPIO 8 | B | 행 주소 B |
| GPIO 3 | C | 행 주소 C |
| GPIO 46 | D | 행 주소 D |
| GPIO 9 | CLK | 클럭 |
| GPIO 10 | LAT | 래치 |
| GPIO 11 | OE | 출력 활성화 |
| GND | GND | 접지 |

---

## 🎮 리모컨 버튼 조작

TikTok BLE 리모컨의 5개 버튼으로 점수판을 조작합니다.

### 서브권 선택 화면 (시작 시)
| 버튼 | 동작 |
|------|------|
| ◀️ LEFT | 왼쪽 플레이어 서브 시작 |
| ▶️ RIGHT | 오른쪽 플레이어 서브 시작 |

### 게임 진행 중
| 버튼 | 동작 |
|------|------|
| ◀️ LEFT | 왼쪽 플레이어 +1점 |
| ▶️ RIGHT | 오른쪽 플레이어 +1점 |
| 🔘 CENTER | 실행취소 (Undo) |
| ⬆️ UP | 메뉴 열기 |
| ⬇️ DOWN | 리셋 확인 화면 |

### 메뉴 화면
| 버튼 | 동작 |
|------|------|
| ⬆️ UP | "Switch" 선택 (서브권 강제 교체) |
| ⬇️ DOWN | "Stop" 선택 (세트 강제 종료) |
| 🔘 CENTER | 선택 적용 |
| ◀️/▶️ | 메뉴 취소 |

### 세트 승리 화면
| 버튼 | 동작 |
|------|------|
| 🔘 CENTER | 다음 세트 시작 (선수 위치 교체) |

---

## 🎮 JX-05 BLE 버튼 감지 알고리즘

### HID Report 구조 (Report ID 2)

```
[0]    [1]    [2-3]    [4-5]    [6]    [7]
action axis   X값      Y값      ?      state
```

| 버튼 | axis | 감지 조건 (RELEASE 이벤트) |
|------|------|--------------------------|
| **왼쪽** | `0x04` | X > `0x0700` |
| **오른쪽** | `0x04` | X < `0x0200` |
| **위** | `0x06` | Y > `0x0a00` |
| **아래** | `0x06` | Y < `0x0300` |
| **중간** | `0x06` | 그 외 |
| **중간 길게** | Report ID 3 | Consumer Control |

---

## 📁 파일 구조

```
table-tennis-by-tiktok-remote/
├── CMakeLists.txt           # 프로젝트 빌드 설정
├── sdkconfig.defaults       # ESP-IDF 설정 (BLE, GATTC 등)
└── main/
    ├── CMakeLists.txt       # 메인 컴포넌트 빌드 설정
    ├── esp_hid_host_main.c  # 메인 (BLE 연결, 버튼 처리, UI)
    ├── esp_hid_gap.c        # BLE GAP 스캔 구현
    ├── esp_hid_gap.h        # GAP 헤더
    ├── hub75_display.c      # P5 LED 디스플레이 제어
    ├── hub75_display.h      # 디스플레이 헤더 (GPIO 핀 정의)
    ├── scoreboard.c         # 점수판 로직 (점수, 서브권 계산)
    ├── scoreboard.h         # 점수판 헤더
    ├── font5x7.h            # 작은 텍스트 폰트 (5x7)
    ├── font_large.h         # 큰 숫자 폰트 (10x14)
    └── Kconfig.projbuild    # menuconfig 옵션
```

---

## ⚙️ 게임 규칙 (탁구)

### 서브권 교체
- **일반**: 2점마다 서브권 교체
- **듀스** (10:10 이상): 1점마다 서브권 교체

### 세트 승리 조건
- 11점 이상 득점 **AND** 2점 차이 이상

### 경기 승리 조건
- 5세트 중 3세트 선승 (Best of 5)

### 세트 교체 시
- 선수들이 코트를 교체합니다
- 세트 점수가 좌/우로 바뀝니다

---

## 🛠️ 빌드 및 플래시 방법

### 1. ESP-IDF 환경 설정
```bash
# ESP-IDF 환경 변수 설정 (PowerShell)
$env:IDF_PATH = 'C:/Espressif/frameworks/esp-idf-v5.5.2/'
```

### 2. 타겟 설정 (ESP32-S3)
```bash
idf.py set-target esp32s3
```

### 3. 빌드
```bash
idf.py build
```

### 4. 플래시 및 모니터
```bash
idf.py -p COM8 flash monitor
```

---

## 🔀 브랜치 안내

| 브랜치 | MCU | 통신 | 리모컨 |
|--------|-----|------|--------|
| [`classic-bt`](https://github.com/gkisanet/table-tennis-by-tiktok-remote/tree/classic-bt) | ESP32 | Bluetooth Classic | Beauty-R1 |
| **`ble`** (현재) | ESP32-S3 | BLE | JX-05 |

---

## 📄 라이센스

ESP-IDF 예제 기반 프로젝트입니다.
