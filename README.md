# 🏓 탁구 전자 점수판 (Table Tennis Digital Scoreboard)

ESP32와 P5 LED 모듈을 사용한 블루투스 탁구 전자 점수판입니다.  
TikTok 블루투스 리모컨(Beauty-R1)으로 점수를 조작할 수 있습니다.

---

## 📌 프로젝트 개요

| 항목 | 설명 |
|------|------|
| **MCU** | ESP32 |
| **디스플레이** | P5 LED 모듈 (64x32, HUB75 인터페이스) |
| **리모컨** | TikTok 블루투스 리모컨 (Beauty-R1) |
| **개발 환경** | ESP-IDF v5.5.2 |

### ✨ 주요 기능
- 🎮 블루투스 리모컨으로 점수 조작
- 📊 세트 점수 및 게임 점수 표시
- 🔄 서브권 표시 (녹색 탈리 바)
- ⚡ 자동 서브권 교체 (2점마다, 듀스 시 1점마다)
- 🔌 연결 끊김 감지 및 자동 재연결

---

## 🔧 하드웨어 연결 (GPIO 핀맵)

ESP32와 P5 LED 모듈의 연결 방법입니다.

### HUB75 데이터 핀
| ESP32 GPIO | P5 LED | 설명 |
|------------|--------|------|
| GPIO 25 | R1 | 상단 빨강 |
| GPIO 26 | G1 | 상단 녹색 |
| GPIO 27 | B1 | 상단 파랑 |
| GPIO 14 | R2 | 하단 빨강 |
| GPIO 12 | G2 | 하단 녹색 |
| GPIO 13 | B2 | 하단 파랑 |

### HUB75 제어 핀
| ESP32 GPIO | P5 LED | 설명 |
|------------|--------|------|
| GPIO 23 | A | 행 주소 A |
| GPIO 22 | B | 행 주소 B |
| GPIO 5 | C | 행 주소 C |
| GPIO 17 | D | 행 주소 D |
| GPIO 16 | CLK | 클럭 |
| GPIO 4 | LAT | 래치 |
| GPIO 15 | OE | 출력 활성화 |
| GND | GND | 접지 |

---

## 🎮 리모컨 버튼 조작

TikTok 리모컨의 5개 버튼으로 점수판을 조작합니다.

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

## 📁 파일 구조

```
bt-discovery/
├── main/
│   ├── main.c           # 메인 프로그램 (버튼 처리, 상태 관리)
│   ├── scoreboard.c     # 점수판 로직 (점수, 서브권 계산)
│   ├── scoreboard.h     # 점수판 헤더 파일
│   ├── hub75_display.c  # LED 디스플레이 제어
│   ├── hub75_display.h  # 디스플레이 헤더 파일
│   ├── font_large.h     # 큰 숫자 폰트 (10x14 픽셀)
│   ├── font5x7.c        # 작은 텍스트 폰트 (5x7 픽셀)
│   ├── esp_hid_gap.c    # 블루투스 HID 연결
│   └── esp_hid_gap.h    # 블루투스 헤더 파일
├── CMakeLists.txt       # 빌드 설정
└── sdkconfig.defaults   # ESP-IDF 설정
```

---

## 🎨 UI 화면 설명

### 1️⃣ 연결 대기 화면
```
Connecting..
```
블루투스 리모컨을 찾고 있습니다.

### 2️⃣ 서브권 선택 화면
```
  0 : 0        ← 세트 점수
[████████]     ← 깜빡이는 탈리 바 (서브권)
  00 : 00      ← 게임 점수
```
LEFT/RIGHT 버튼으로 먼저 서브할 플레이어를 선택합니다.

### 3️⃣ 게임 진행 화면
```
  1 : 0        ← 세트 점수
 ████          ← 녹색 탈리 바 (현재 서브권)
  05 : 03      ← 게임 점수 (주황색)
```

### 4️⃣ 세트 승리 화면
```
  1 : 0
 ████          ← 깜빡이는 탈리 (승자 측)
  11 : 09
   OK          ← OK 버튼 안내
```

### 5️⃣ 연결 끊김 화면
```
  1 : 0
  05 : 03      ← 빨간색, 깜빡임
 Disconn       ← 연결 끊김 표시
```
10% 밝기로 깜빡이며 재연결을 시도합니다.

### 6️⃣ 경기 종료 화면 (3세트 승리)
```
  2 : 1
   WIN!        ← 승자 측에 표시
```

---

## ⚙️ 게임 규칙 (탁구)

### 서브권 교체
- **일반**: 2점마다 서브권 교체
- **듀스** (10:10 이상): 1점마다 서브권 교체

### 세트 승리 조건
- 11점 이상 득점 **AND** 2점 차이 이상

### 경기 승리 조건
- 3세트 중 2세트 선승 (Best of 3)

### 세트 교체 시
- 선수들이 코트를 교체합니다
- 세트 점수가 좌/우로 바뀝니다
- 서브권은 최초 선택한 위치에서 시작

---

## ⚠️ 사용 전 필수 설정: 리모컨 MAC 주소

> [!CAUTION]
> 이 프로젝트를 사용하려면 **본인의 TikTok 리모컨 MAC 주소**를 코드에 직접 입력해야 합니다.

### 1단계: 리모컨 MAC 주소 확인

리모컨의 MAC 주소를 모르는 경우, 먼저 코드를 빌드하여 ESP32에 플래시하세요.  
시리얼 모니터에서 스캔된 블루투스 장치 목록이 표시됩니다:

```
BT : 12:22:34:01:00:c0, RSSI: -45, USAGE: GENERIC, NAME: Beauty-R1
```

여기서 `12:22:34:01:00:c0`이 리모컨의 MAC 주소입니다.

### 2단계: 코드에 MAC 주소 입력

`main/main.c` 파일에서 아래 부분을 **본인의 리모컨 MAC 주소**로 변경하세요:

```c
// main/main.c (36번째 줄)
static const uint8_t TARGET_MAC[6] = {0x12, 0x22, 0x34, 0x01, 0x00, 0xc0};
//                                     ↑ 이 값들을 본인의 MAC 주소로 변경!
```

예를 들어, MAC 주소가 `AA:BB:CC:DD:EE:FF`라면:
```c
static const uint8_t TARGET_MAC[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
```

---

## 🛠️ 빌드 및 플래시 방법

### 1. ESP-IDF 환경 설정
```bash
# ESP-IDF 환경 변수 설정 (PowerShell)
$env:IDF_PATH = 'C:/Espressif/frameworks/esp-idf-v5.5.2/'
```

### 2. 빌드
```bash
idf.py build
```

### 3. 플래시 및 모니터
```bash
idf.py -p COM6 flash monitor
```

### 4. 클린 빌드 (문제 발생 시)
```bash
idf.py fullclean
idf.py build flash monitor
```

---

## 🔍 코드 이해하기 (초보자용)

### 게임 상태 (Game State)
프로그램은 여러 "상태"를 가지고 있습니다:

```c
typedef enum {
  GAME_STATE_INIT,           // 초기화
  GAME_STATE_CONNECTING,     // 연결 중
  GAME_STATE_DISCONNECTED,   // 연결 끊김
  GAME_STATE_SELECT_FIRST,   // 서브권 선택
  GAME_STATE_PLAYING,        // 게임 진행
  GAME_STATE_WINNER,         // 세트 승자
  GAME_STATE_MATCH_END,      // 경기 종료
  GAME_STATE_MENU,           // 메뉴
  GAME_STATE_CONFIRM_RESET   // 리셋 확인
} game_state_t;
```

### 버튼 처리 흐름
```
버튼 입력 → process_button() → 상태에 따라 동작
                ↓
            scoreboard 함수 호출
                ↓
            UI 업데이트
```

### 서브권 계산 로직
```c
// scoreboard.c의 update_serve()
int total_points = left_score + right_score;

if (듀스 상태) {
  // 1점마다 교체
  serve_offset = total_points % 2;
} else {
  // 2점마다 교체
  serve_offset = (total_points / 2) % 2;
}

// 첫 서브권 기준으로 계산
serve_side = (first_serve_side + serve_offset) % 2;
```

---

## � 프로그래밍 개념 상세 설명

이 섹션은 C언어 초보자를 위해 이 프로젝트에서 사용된 프로그래밍 개념들을 상세히 설명합니다.

---

### 🔤 1. C언어 기본 개념

#### 헤더 파일 (.h) vs 소스 파일 (.c)
```
┌─────────────────┐    ┌─────────────────┐
│  scoreboard.h   │    │  scoreboard.c   │
│  (선언부)        │ ←→ │  (구현부)        │
│                 │    │                 │
│  함수 이름만     │    │  함수 내용 전체  │
│  정의           │    │  코드 작성       │
└─────────────────┘    └─────────────────┘
```

**scoreboard.h (선언)**
```c
// 함수가 존재한다는 것만 알려줌 (껍데기)
void scoreboard_add_point(scoreboard_t *sb, int side);
```

**scoreboard.c (구현)**
```c
// 함수가 실제로 무엇을 하는지 작성 (내용)
void scoreboard_add_point(scoreboard_t *sb, int side) {
  // 점수를 올리고, 서브권을 업데이트하고...
  // 100줄의 코드...
}
```

#### typedef와 struct (사용자 정의 데이터 타입)
```c
// struct: 여러 변수를 묶어서 하나의 그룹으로 만듦
// typedef: 새로운 이름을 붙여줌

typedef struct {
  int left_score;      // 왼쪽 점수
  int right_score;     // 오른쪽 점수
  int left_sets;       // 왼쪽 세트 수
  int right_sets;      // 오른쪽 세트 수
  int serve_side;      // 서브권 (0=왼쪽, 1=오른쪽)
  game_state_t state;  // 현재 게임 상태
} scoreboard_t;

// 사용 예시
scoreboard_t scoreboard;          // 점수판 변수 선언
scoreboard.left_score = 5;        // 왼쪽 점수를 5로 설정
scoreboard.serve_side = 0;        // 왼쪽이 서브
```

#### enum (열거형)
```c
// enum: 숫자 대신 의미있는 이름 사용
typedef enum {
  BTN_NONE = 0,    // 0
  BTN_LEFT = 1,    // 1
  BTN_RIGHT = 2,   // 2
  BTN_UP = 3,      // 3
  BTN_DOWN = 4,    // 4
  BTN_CENTER = 5   // 5
} button_t;

// if (button == 1) 보다 읽기 쉬움
if (btn == BTN_LEFT) {
  // 왼쪽 버튼 처리
}
```

#### 포인터 (*)
```c
// 포인터: 변수의 "주소"를 저장
// 왜 사용? 함수에서 원본 변수를 직접 수정하기 위해

void scoreboard_add_point(scoreboard_t *sb, int side) {
  //                                    ↑
  //                       sb는 scoreboard의 "주소"
  
  sb->left_score++;  // 원본 scoreboard의 left_score 증가
  //  ↑
  // 포인터일 때는 . 대신 -> 사용
}

// 호출할 때
scoreboard_add_point(&scoreboard, 0);  
//                   ↑
//           &는 "주소를 알려줘"라는 의미
```

---

### 🔄 2. 프로그램 실행 흐름

#### 전체 프로그램 흐름도
```
┌──────────────────────────────────────────────────────────────┐
│                        app_main()                             │
│                    (프로그램 시작점)                           │
└─────────────────────────┬────────────────────────────────────┘
                          ↓
            ┌─────────────────────────────┐
            │    1. NVS 초기화            │
            │    (비휘발성 저장소)          │
            └───────────────┬─────────────┘
                            ↓
            ┌─────────────────────────────┐
            │    2. 블루투스 초기화        │
            │    (컨트롤러 + Bluedroid)    │
            └───────────────┬─────────────┘
                            ↓
            ┌─────────────────────────────┐
            │    3. LED 디스플레이 초기화  │
            │    (HUB75 + Refresh Task)   │
            └───────────────┬─────────────┘
                            ↓
            ┌─────────────────────────────┐
            │    4. 블루투스 장치 검색     │
            │    (Beauty-R1 찾기)         │
            └───────────────┬─────────────┘
                            ↓
            ┌─────────────────────────────┐
            │    5. 메인 루프              │
            │    (UI 업데이트 무한 반복)   │
            └─────────────────────────────┘
```

#### 메인 루프 상세
```c
// main.c의 메인 루프
while (1) {
    // 500ms 마다 blink_state 토글 (깜빡임 효과)
    if (현재시간 - 마지막블링크시간 > 500ms) {
        blink_state = !blink_state;  // true ↔ false 토글
    }
    
    // 현재 상태에 따라 다른 화면 표시
    switch (scoreboard.state) {
        case GAME_STATE_CONNECTING:
            hub75_show_connecting();
            break;
            
        case GAME_STATE_PLAYING:
            hub75_show_scoreboard(...);
            break;
            
        // ... 다른 상태들 ...
    }
    
    vTaskDelay(16);  // 약 60fps로 화면 갱신
}
```

---

### 📞 3. 함수 호출 흐름 상세

#### 점수 추가 시 함수 호출 순서
```
사용자가 LEFT 버튼 누름
        │
        ↓
┌───────────────────────────────────────────────┐
│  hidh_callback()                              │
│  - 블루투스로부터 버튼 데이터 수신            │
│  - 버튼 종류 파악 (LEFT)                      │
│  - process_button(BTN_LEFT) 호출              │
└──────────────────────────┬────────────────────┘
                           ↓
┌───────────────────────────────────────────────┐
│  process_button(BTN_LEFT)                     │
│  - 현재 상태 확인 (GAME_STATE_PLAYING)        │
│  - switch 문으로 상태별 분기                  │
│  - scoreboard_add_point(&scoreboard, 0) 호출  │
└──────────────────────────┬────────────────────┘
                           ↓
┌───────────────────────────────────────────────┐
│  scoreboard_add_point(&scoreboard, 0)          │
│  - 히스토리 저장 (실행취소용)                 │
│  - left_score++ (점수 증가)                   │
│  - update_serve() 호출 (서브권 계산)          │
│  - scoreboard_check_winner() 호출             │
└──────────────────────────┬────────────────────┘
                           ↓
┌───────────────────────────────────────────────┐
│  update_serve()                               │
│  - 총 점수 계산                               │
│  - 듀스 여부 확인                             │
│  - 서브권 결정                                │
└──────────────────────────┬────────────────────┘
                           ↓
┌───────────────────────────────────────────────┐
│  scoreboard_check_winner()                    │
│  - 11점 이상인지 확인                         │
│  - 2점 차이 나는지 확인                       │
│  - 승자 있으면 state = GAME_STATE_WINNER      │
└───────────────────────────────────────────────┘
```

#### LED 디스플레이 함수 호출
```
┌───────────────────────────────────────────────┐
│  hub75_show_scoreboard()                      │
│  - hub75_clear() 호출 (화면 지우기)           │
│  - hub75_draw_text() 호출 (세트 점수)         │
│  - hub75_draw_tally() 호출 (서브권 표시)      │
│  - draw_score() 호출 (게임 점수)              │
└──────────────────────────┬────────────────────┘
                           ↓
┌───────────────────────────────────────────────┐
│  hub75_draw_text("0 : 0", ...)                │
│  - 각 문자에 대해 font5x7_get_char() 호출     │
│  - 픽셀 데이터를 frame_buffer에 쓰기          │
└──────────────────────────┬────────────────────┘
                           ↓
┌───────────────────────────────────────────────┐
│  refresh_frame() [백그라운드 태스크]          │
│  - frame_buffer의 데이터를 GPIO로 출력        │
│  - 16개의 행을 순차적으로 표시                │
│  - 초당 수백 번 반복하여 잔상 효과            │
└───────────────────────────────────────────────┘
```

---

### 💾 4. 데이터 구조 상세

#### scoreboard_t 구조체
```c
typedef struct {
  // 현재 게임 점수
  int left_score;           // 왼쪽 플레이어 점수 (0~99)
  int right_score;          // 오른쪽 플레이어 점수 (0~99)
  
  // 세트 점수
  int left_sets;            // 왼쪽 세트 승리 수 (0, 1, 2)
  int right_sets;           // 오른쪽 세트 승리 수 (0, 1, 2)
  
  // 서브권
  int serve_side;           // 현재 서브권 (0=왼쪽, 1=오른쪽)
  int first_serve_side;     // 첫 서브권 (세트별 계산용)
  
  // 게임 상태
  game_state_t state;       // 현재 화면 상태
  int winner_side;          // 세트 승자 (-1=없음, 0=왼쪽, 1=오른쪽)
  
  // 실행취소 기능
  score_history_t history[MAX_UNDO];  // 최근 5개 상태 저장
  int history_index;        // 히스토리 인덱스
  int undo_count;           // 남은 실행취소 횟수
  
  // 메뉴
  int menu_selection;       // 메뉴 선택 (0=Switch, 1=Stop)
} scoreboard_t;
```

#### frame_buffer (화면 버퍼)
```c
// 64x32 픽셀, RGB 3채널
static uint8_t frame_buffer[32][64][3];
//                          ↑   ↑   ↑
//                          Y   X  RGB

// 픽셀 설정 예시
frame_buffer[10][20][0] = 255;  // (20, 10) 위치에 빨강
frame_buffer[10][20][1] = 255;  // (20, 10) 위치에 녹색 
frame_buffer[10][20][2] = 0;    // (20, 10) 위치에 파랑 없음
// → (20, 10) 위치에 주황색 표시
```

---

### ⚡ 5. 중요한 C언어 문법

#### switch 문 (상태 분기)
```c
// if-else 대신 깔끔하게 분기
switch (scoreboard.state) {
  case GAME_STATE_PLAYING:
    // 게임 중 화면 표시
    hub75_show_scoreboard(...);
    break;  // break 없으면 다음 case도 실행됨!
    
  case GAME_STATE_WINNER:
    // 승자 화면 표시
    hub75_show_winner(...);
    break;
    
  default:
    // 어떤 case에도 해당 안 되면
    hub75_show_connecting();
    break;
}
```

#### 비트 연산 (&, |, >>)
```c
// 색상 추출 예시 (RGB565 형식)
uint16_t color = 0xFFE0;  // 주황색

// & 연산: 해당 비트만 추출
int red   = (color & 0xF800);  // 0xF800 (빨강 있음)
int green = (color & 0x07E0);  // 0x07E0 (녹색 있음)
int blue  = (color & 0x001F);  // 0x0000 (파랑 없음)

// 값이 0보다 크면 해당 LED 켜기
gpio_set_level(PIN_R1, red > 0 ? 1 : 0);
//                        ↑
//                  삼항 연산자: 조건 ? 참값 : 거짓값
```

#### 삼항 연산자
```c
// 긴 if-else를 한 줄로
int brightness;
if (disconnected) {
    brightness = 10;
} else {
    brightness = 20;
}

// 위와 동일한 코드
int brightness = disconnected ? 10 : 20;
//              조건           참   거짓
```

---

### 🔧 6. FreeRTOS 개념 (멀티태스킹)

```c
// ESP32는 여러 작업을 동시에 실행 (태스크)

// 태스크 생성
xTaskCreate(
    refresh_task,        // 실행할 함수
    "hub75_refresh",     // 태스크 이름
    2048,                // 스택 크기 (바이트)
    NULL,                // 함수에 전달할 인자
    5,                   // 우선순위 (높을수록 우선)
    &refresh_task_handle // 태스크 핸들 (제어용)
);

// vTaskDelay: 다른 태스크에게 CPU 양보
vTaskDelay(1);  // 1틱(보통 10ms) 동안 대기

// 왜 필요한가?
// - Watchdog 타이머가 IDLE 태스크 실행을 감시
// - delay 없이 무한루프 돌면 Watchdog 에러 발생
```

---

## �🐛 문제 해결

### Task Watchdog 에러
```
E (25497) task_wdt: Task watchdog got triggered
```
→ `vTaskDelay(1)`이 없으면 발생. 반드시 refresh 루프에 delay 필요.

### 연결이 안 될 때
1. 리모컨 배터리 확인
2. ESP32 재부팅
3. 리모컨 전원 껐다 켜기

### 화면 깜빡임
→ `portDISABLE_INTERRUPTS()` / `portENABLE_INTERRUPTS()` 사용으로 개선됨

---

## 📝 색상 정의

| 색상 | 코드 | 용도 |
|------|------|------|
| 주황색 (R+G) | 0xFFE0 | 메인 점수 (정상 연결) |
| 순수 빨강 | 0xF800 | 메인 점수 (연결 끊김) |
| 녹색 | 0x07E0 | 서브권 탈리 바 |
| 어두운 주황 | 0x3320 | 비활성 요소 |

---

## 🙏 크레딧

이 프로젝트는 AI 코딩 어시스턴트의 도움으로 개발되었습니다.  
C언어 경험 없이도 바이브 코딩(Vibe Coding)을 통해 완성할 수 있었습니다!

---

## 📄 라이센스

ESP-IDF 예제 기반 프로젝트입니다.

