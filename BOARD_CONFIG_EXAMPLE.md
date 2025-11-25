# 멀티 PCB 펌웨어 설정 가이드

하나의 소스 코드로 4가지 PCB 버전을 지원합니다!

## 지원 보드

| 보드 | GPS | 통신 |
|------|-----|------|
| PCB1 | F9P x1 | BLE + LoRa |
| PCB2 | UM982 x1 | BLE + LoRa |
| PCB3 | F9P x2 | RS485 + LoRa |
| PCB4 | UM982 x1 | RS485 + LoRa |

---

## 🚀 빠른 시작

### 1. 보드 선택

`config/board_type.h` 파일을 열어서 사용할 보드만 활성화:

```c
/* ====== 아래 중 하나만 선택 (나머지는 주석 처리) ====== */

#define BOARD_TYPE_PCB1     // F9P + BLE + LoRa
// #define BOARD_TYPE_PCB2  // UM982 + BLE + LoRa
// #define BOARD_TYPE_PCB3  // F9P x2 + RS485 + LoRa
// #define BOARD_TYPE_PCB4  // UM982 + RS485 + LoRa
```

### 2. 빌드

STM32CubeIDE에서 **Project → Build Project**

끝! ✨

---

## 📁 파일 구조

```
config/
├── board_type.h        # 보드 선택 (여기서 수정!)
├── board_config.h      # 보드 정보 API (초기화 코드 없음)
└── board_config.c      # 보드 정보만 제공

modules/gps/
├── gps_port.h/c        # 보드별 핀맵 + GPS 타입별 초기화
├── gps_app.h/c         # GPS 태스크 (태스크 내부에서 초기화)

modules/comm/
├── ble_port.h/c        # BLE 핀맵 + 초기화
├── rs485_port.h/c      # RS485 핀맵 + 초기화
└── lora_port.h/c       # LoRa 핀맵 + 초기화
```

---

## 💡 핵심 설계 원칙

### 1. 핀맵은 각 모듈 내부에서 관리

각 모듈(GPS, BLE, RS485 등)이 자신의 핀맵을 내부에서 관리합니다.

**장점:**
- ✅ 모듈 독립성 유지
- ✅ 다른 프로젝트에 재사용 쉬움
- ✅ 각 모듈이 필요한 핀만 알면 됨

**예시: modules/gps/gps_port.c**
```c
#if defined(BOARD_TYPE_PCB1)
    #define GPS1_UART    USART2
    #define GPS1_TX_PIN  LL_GPIO_PIN_2
    // ...
#elif defined(BOARD_TYPE_PCB2)
    // UM982용 핀맵
#endif
```

### 2. 초기화는 각 태스크에서 수행

FreeRTOS 태스크 내부에서 모듈을 초기화합니다.

**장점:**
- ✅ vTaskDelay 사용 가능 (UM982 RDY 대기 등)
- ✅ 모듈 독립성 유지
- ✅ 동적 초기화 가능

**예시: modules/gps/gps_app.c**
```c
static void gps_process_task(void *pvParameter) {
  // 태스크 내부에서 초기화
  gps_init(&gps_handle);        // 라이브러리 초기화
  gps_port_init();              // 하드웨어 초기화
  gps_start();                  // GPS 시작 (타입별 초기화)

  while (1) {
    // 메인 루프
  }
}
```

### 3. board_config는 정보만 제공

`board_config`는 초기화 코드가 없고 보드 정보만 제공합니다.

**제공하는 정보:**
- 현재 보드 타입
- GPS 타입 (F9P / UM982)
- GPS 개수 (1 or 2)
- 통신 인터페이스 (BLE / RS485 / LoRa)

**사용 예시:**
```c
const board_config_t* config = board_get_config();

if (config->gps_primary == GPS_TYPE_F9P) {
    // F9P 처리
} else if (config->gps_primary == GPS_TYPE_UM982) {
    // UM982 처리
}
```

---

## 📝 GPS 초기화 과정

### F9P GPS (PCB1, PCB3)

```c
void gps_init_f9p(void) {
  // 1. Reset 핀 HIGH
  HAL_GPIO_WritePin(GPS1_RESET_PORT, GPS1_RESET_PIN, GPIO_PIN_SET);

  // 2. 즉시 사용 가능 - 대기 불필요

  // 3. UBX 설정 명령 전송 (optional)
}
```

### UM982 GPS (PCB2, PCB4)

```c
void gps_init_um982(void) {
  // 1. Reset 핀 토글
  HAL_GPIO_WritePin(GPS1_RESET_PORT, GPS1_RESET_PIN, GPIO_PIN_RESET);
  vTaskDelay(pdMS_TO_TICKS(100));
  HAL_GPIO_WritePin(GPS1_RESET_PORT, GPS1_RESET_PIN, GPIO_PIN_SET);

  // 2. UART로 RDY 응답 대기 (최대 5초)
  if (!gps_wait_for_um982_ready(5000)) {
    // 타임아웃 에러 처리
    return;
  }

  // 3. 설정 명령 전송
}
```

### UM982 RDY 확인 (UART 파싱)

```c
static bool gps_wait_for_um982_ready(uint32_t timeout_ms) {
  // TODO: gps_recv 버퍼에서 RDY 메시지 파싱
  // 예: "$GNGGA" 같은 NMEA 메시지 수신 확인
  // 또는 UM982 전용 RDY 응답 메시지 확인

  while (타임아웃_전) {
    if (strstr(gps_recv, "RDY") != NULL) {
      return true;  // RDY 수신 성공
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  return false;  // 타임아웃
}
```

---

## 🔧 코드 사용 예시

### 보드 정보 확인

```c
#include "board_config.h"

void print_board_info(void) {
    const board_config_t* config = board_get_config();

    printf("Board: %s\n", config->board_name);
    printf("GPS Count: %d\n", config->gps_count);

    if (config->gps_primary == GPS_TYPE_F9P) {
        printf("GPS: F9P\n");
    } else if (config->gps_primary == GPS_TYPE_UM982) {
        printf("GPS: UM982\n");
    }

    if (board_has_interface(COMM_TYPE_BLE)) {
        printf("BLE available\n");
    }
}
```

### 조건부 컴파일

```c
void init_communication(void) {
    // BLE 초기화 (PCB1, PCB2만 컴파일됨)
    #if HAS_BLE
        ble_init();
    #endif

    // RS485 초기화 (PCB3, PCB4만 컴파일됨)
    #if HAS_RS485
        rs485_init();
    #endif

    // LoRa 초기화 (모든 보드)
    #if HAS_LORA
        lora_init();
    #endif
}
```

### GPS 타입별 처리

```c
void process_gps_data(void) {
    // 컴파일 타임에 결정
    #if GPS_PRIMARY == GPS_TYPE_F9P
        // F9P UBX 처리
    #elif GPS_PRIMARY == GPS_TYPE_UM982
        // UM982 처리
    #endif

    // 런타임에 결정 (필요한 경우)
    const board_config_t* config = board_get_config();
    if (config->gps_primary == GPS_TYPE_UM982) {
        // UM982 전용 처리
    }
}
```

---

## 🎯 새 모듈 추가하기

예: BLE 모듈 추가

### 1. 파일 생성

```
modules/comm/
├── ble_port.h
└── ble_port.c
```

### 2. ble_port.c 작성

```c
#include "ble_port.h"
#include "board_config.h"

/* 보드별 핀맵 */
#if defined(BOARD_TYPE_PCB1) || defined(BOARD_TYPE_PCB2)
    #define BLE_UART          UART4
    #define BLE_TX_PIN        GPIO_PIN_0
    #define BLE_RX_PIN        GPIO_PIN_1
    #define BLE_RESET_PIN     GPIO_PIN_8
    // ...
#endif

void ble_init(void) {
    // BLE 초기화 코드
}
```

### 3. 태스크에서 사용

```c
void ble_task(void *pvParameter) {
    #if HAS_BLE
        ble_init();  // 태스크 내부에서 초기화

        while (1) {
            // BLE 처리
        }
    #endif
}
```

---

## ✅ 장점

**모듈 독립성**
- 각 모듈이 자신의 핀맵과 초기화 코드 관리
- 다른 프로젝트에 재사용 쉬움

**유연한 초기화**
- 태스크 내부에서 초기화 → FreeRTOS API 사용 가능
- UM982 RDY 대기 같은 블로킹 동작 가능

**깔끔한 구조**
- board_config는 정보만 제공
- 초기화 로직은 각 모듈에 분산

**컴파일러 최적화**
- 사용하지 않는 코드 자동 제거
- 각 보드에 최적화된 바이너리

---

## 📌 다음 단계

1. **UM982 RDY 파싱 구현**
   - `gps_wait_for_um982_ready()` 함수에서 실제 RDY 메시지 파싱

2. **듀얼 GPS 지원 (PCB3)**
   - GPS2 포트 초기화
   - 두 GPS 데이터 동시 처리

3. **통신 모듈 구현**
   - `modules/comm/ble_port.c`
   - `modules/comm/rs485_port.c`
   - `modules/comm/lora_port.c`

---

## 🔍 FAQ

**Q: 핀맵을 중앙에서 관리하는 게 낫지 않나요?**

A: 모듈 독립성을 위해 각 모듈 내부에서 관리합니다. 다른 프로젝트에 재사용할 때 의존성이 줄어듭니다.

**Q: 초기화를 main()에서 하는 게 낫지 않나요?**

A: FreeRTOS 시작 전에는 vTaskDelay를 사용할 수 없어서, UM982 RDY 대기 같은 블로킹 동작이 불가능합니다. 태스크 내부에서 초기화하면 FreeRTOS API를 자유롭게 사용할 수 있습니다.

**Q: board_config에서 초기화를 하지 않나요?**

A: 이제 하지 않습니다! board_config는 정보만 제공하고, 실제 초기화는 각 모듈의 port 파일과 태스크에서 수행합니다.

**Q: UM982 RDY가 GPIO인가요, UART 메시지인가요?**

A: 이 구현에서는 UART 파싱으로 처리합니다. GPIO RDY 핀이 있다면 코드를 수정하면 됩니다.

---

질문이나 문제가 있으면 `config/board_type.h`를 확인하세요!
