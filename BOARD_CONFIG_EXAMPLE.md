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
├── board_config.h      # 보드 설정 API
├── board_config.c      # 보드별 초기화 구현
├── board_pinmap.h      # 보드별 핀맵 정의
└── FreeRTOSConfig.h
```

---

## 💻 주요 기능

### 1. 보드별 초기화 시퀀스

각 보드의 특성에 맞게 자동으로 초기화됩니다:

**UM982 GPS (PCB2, PCB4)**
- Reset 후 RDY 신호 대기 (최대 5초)
- RDY 확인 후 UART 통신 시작
- 설정 명령 전송

**F9P GPS (PCB1, PCB3)**
- Reset 후 즉시 사용 가능
- UBX 프로토콜 설정

**듀얼 GPS (PCB3)**
- GPS1, GPS2 순차 초기화
- 각각 독립적인 UART 사용

### 2. 보드별 핀맵 자동 적용

`config/board_pinmap.h`에 정의된 핀맵이 자동으로 적용됩니다:

```c
// PCB1: USART2로 F9P GPS 연결
// PCB2: USART2로 UM982 GPS 연결 + RDY 핀
// PCB3: USART2로 GPS1, USART3으로 GPS2 연결
// PCB4: USART2로 UM982 GPS 연결 + RDY 핀
```

### 3. 조건부 컴파일

사용하지 않는 모듈은 자동으로 제외됩니다:

```c
// PCB1, PCB2만 BLE 코드 포함
#if HAS_BLE
    ble_init();
#endif

// PCB3, PCB4만 RS485 코드 포함
#if HAS_RS485
    rs485_init();
#endif
```

---

## 📝 코드 사용 예시

### 메인 함수

```c
#include "board_config.h"

int main(void) {
    HAL_Init();
    SystemClock_Config();

    // 보드 자동 초기화
    board_init();  // GPS, BLE/RS485, LoRa 등 모두 초기화

    // 보드 정보 확인
    const board_config_t* config = board_get_config();
    printf("Board: %s\n", config->board_name);
    printf("GPS Count: %d\n", config->gps_count);

    // FreeRTOS 시작
    vTaskStartScheduler();
}
```

### 보드별 동작 확인

```c
void setup_modules(void) {
    // GPS 개수 확인
    uint8_t gps_count = board_get_gps_count();
    if (gps_count == 2) {
        printf("Dual GPS mode\n");
    }

    // 특정 인터페이스 사용 여부 확인
    if (board_has_interface(COMM_TYPE_BLE)) {
        printf("BLE available\n");
    }

    if (board_has_interface(COMM_TYPE_RS485)) {
        printf("RS485 available\n");
    }
}
```

### 조건부 컴파일 활용

```c
void init_communication(void) {
    // BLE 초기화 (PCB1, PCB2만 컴파일됨)
    #if HAS_BLE
        ble_module_init();
        printf("BLE initialized\n");
    #endif

    // RS485 초기화 (PCB3, PCB4만 컴파일됨)
    #if HAS_RS485
        rs485_driver_init();
        printf("RS485 initialized\n");
    #endif

    // LoRa 초기화 (모든 보드)
    #if HAS_LORA
        lora_module_init();
        printf("LoRa initialized\n");
    #endif
}
```

### GPS 타입별 처리

```c
void process_gps_data(void) {
    const board_config_t* config = board_get_config();

    // GPS 타입별 처리
    #if GPS_PRIMARY == GPS_TYPE_F9P
        // F9P는 UBX 프로토콜 사용
        process_ubx_data();
    #elif GPS_PRIMARY == GPS_TYPE_UM982
        // UM982는 NMEA + 바이너리
        process_um982_data();
    #endif

    // 듀얼 GPS 처리 (PCB3만)
    #if GPS_COUNT == 2
        process_secondary_gps();
    #endif
}
```

---

## 🔧 초기화 시퀀스 상세

### board_init() 동작 순서

```
1. 공통 모듈 초기화
   - LED 초기화 (optional)

2. GPS 초기화
   [PCB1, PCB3] F9P GPS:
     └─ Reset 핀 HIGH
     └─ 즉시 사용 가능
     └─ UBX 설정 명령 전송

   [PCB2, PCB4] UM982 GPS:
     └─ Reset 핀 토글
     └─ RDY 신호 대기 (최대 5초)
     └─ RDY 확인 후 설정 명령 전송

   [PCB3] 듀얼 GPS:
     └─ GPS1 초기화
     └─ GPS2 초기화

3. 통신 인터페이스 초기화
   [PCB1, PCB2] BLE:
     └─ Reset 핀 토글
     └─ 부팅 대기 (500ms)
     └─ AT 명령으로 설정

   [PCB3, PCB4] RS485:
     └─ DE/RE 핀 초기화 (수신 모드)
     └─ UART 설정

   [모든 보드] LoRa:
     └─ Reset 핀 토글
     └─ 부팅 대기 (500ms)
     └─ 설정 명령 전송

4. LTE(EC25) 초기화
   └─ 모든 보드 공통
```

---

## 🎯 핀맵 커스터마이징

보드별로 핀이 다르다면 `config/board_pinmap.h`를 수정하세요:

```c
#if defined(BOARD_TYPE_PCB1)
    #define GPS1_UART                   USART2
    #define GPS1_UART_TX_PIN            GPIO_PIN_2
    #define GPS1_UART_RX_PIN            GPIO_PIN_3
    // ... 실제 하드웨어에 맞게 수정
#endif
```

---

## ⚙️ 고급 기능

### 런타임 보드 정보 활용

```c
void print_board_info(void) {
    const board_config_t* config = board_get_config();

    printf("=== Board Info ===\n");
    printf("Name: %s\n", config->board_name);
    printf("GPS Primary: %s\n",
        config->gps_primary == GPS_TYPE_F9P ? "F9P" : "UM982");

    if (config->gps_count == 2) {
        printf("GPS Secondary: F9P\n");
    }

    printf("Interfaces:\n");
    if (config->comm_interfaces & COMM_TYPE_BLE)
        printf("  - BLE\n");
    if (config->comm_interfaces & COMM_TYPE_RS485)
        printf("  - RS485\n");
    if (config->comm_interfaces & COMM_TYPE_LORA)
        printf("  - LoRa\n");
}
```

### 에러 처리

```c
void init_with_error_check(void) {
    const board_config_t* config = board_get_config();

    // UM982는 RDY 대기 실패 가능
    #if GPS_PRIMARY == GPS_TYPE_UM982
        if (!um982_wait_for_ready(5000)) {
            LOG_ERR("UM982 initialization failed!");
            // 폴백 처리
        }
    #endif
}
```

---

## 🎉 장점

✅ **하나의 프로젝트** - 소스 코드 통일 관리
✅ **쉬운 보드 변경** - 파일 한 줄 수정
✅ **자동 최적화** - 불필요한 코드 제거
✅ **안전한 빌드** - 잘못된 설정 시 컴파일 에러
✅ **보드별 초기화** - GPS RDY 대기, 핀맵 자동 적용
✅ **유지보수 편리** - 공통 코드 한 곳에서 관리

---

## 📌 다음 단계

이제 각 모듈의 실제 드라이버를 구현하면 됩니다:

1. **GPS 드라이버**
   - `gps_f9p_configure()` - F9P UBX 설정
   - `gps_um982_configure()` - UM982 설정

2. **통신 드라이버**
   - `ble_init()` - BLE 모듈 초기화
   - `rs485_init()` - RS485 드라이버
   - `lora_init()` - LoRa 모듈 초기화

3. **기존 코드 통합**
   - `main.c`에서 `board_init()` 호출
   - 기존 GPS 초기화 코드를 보드별로 분리

---

질문이나 문제가 있으면 `config/board_type.h`를 확인하세요!
