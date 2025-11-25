# 멀티 PCB 펌웨어 설정 가이드

이 프로젝트는 하나의 펌웨어로 4가지 PCB 버전을 지원합니다.

## 지원 보드 타입

| 보드 | GPS | 통신 |
|------|-----|------|
| PCB1 | F9P x1 | BLE + LoRa |
| PCB2 | UM982 x1 | BLE + LoRa |
| PCB3 | F9P x2 | RS485 + LoRa |
| PCB4 | UM982 x1 | RS485 + LoRa |

---

## 방법 1: 런타임 감지 (추천)

### 장점
- ✅ 하나의 바이너리로 모든 보드 지원
- ✅ 현장에서 보드 교체 시 펌웨어 재빌드 불필요
- ✅ 재고 관리 편리

### 단점
- ❌ GPIO 핀 2개 필요 (보드 ID용)
- ❌ 런타임 오버헤드 약간 존재

### 하드웨어 설정

보드마다 GPIO 핀을 다르게 풀업/풀다운:

```
PCB1: PIN0=LOW,  PIN1=LOW   (00)
PCB2: PIN0=HIGH, PIN1=LOW   (01)
PCB3: PIN0=LOW,  PIN1=HIGH  (10)
PCB4: PIN0=HIGH, PIN1=HIGH  (11)
```

예시 회로:
```
PCB1: PIN0 --[10kΩ]-- GND,  PIN1 --[10kΩ]-- GND
PCB2: PIN0 --[10kΩ]-- VCC,  PIN1 --[10kΩ]-- GND
PCB3: PIN0 --[10kΩ]-- GND,  PIN1 --[10kΩ]-- VCC
PCB4: PIN0 --[10kΩ]-- VCC,  PIN1 --[10kΩ]-- VCC
```

### 빌드 방법

`board_config.h`에서 `BOARD_RUNTIME_DETECT` 정의:

```c
#define BOARD_RUNTIME_DETECT
```

빌드:
```bash
make clean
make
```

### 코드 사용 예시

```c
#include "board_config.h"

int main(void) {
    HAL_Init();
    SystemClock_Config();

    // 보드 자동 감지 및 초기화
    board_init();

    // 현재 보드 정보 확인
    const board_config_t* config = board_get_config();
    printf("Board: %s\n", config->board_name);
    printf("GPS Count: %d\n", config->gps_count);

    // GPS 타입별 동작
    if (config->gps_primary == GPS_TYPE_F9P) {
        // F9P 전용 설정
    } else if (config->gps_primary == GPS_TYPE_UM982) {
        // UM982 전용 설정
    }

    // 통신 인터페이스별 동작
    if (config->comm_interfaces & COMM_TYPE_BLE) {
        // BLE 사용
    }
    if (config->comm_interfaces & COMM_TYPE_RS485) {
        // RS485 사용
    }

    while(1) {
        // 메인 루프
    }
}
```

---

## 방법 2: 컴파일 타임 설정

### 장점
- ✅ GPIO 핀 불필요
- ✅ 컴파일러 최적화로 코드 크기 작음
- ✅ 런타임 오버헤드 없음

### 단점
- ❌ 보드마다 별도 빌드 필요
- ❌ 재고 관리 복잡 (4가지 바이너리)

### 빌드 방법

각 PCB마다 다른 매크로로 빌드:

**PCB1 빌드:**
```bash
make clean
make CFLAGS="-DBOARD_TYPE_PCB1"
# 출력: firmware_pcb1.bin
```

**PCB2 빌드:**
```bash
make clean
make CFLAGS="-DBOARD_TYPE_PCB2"
# 출력: firmware_pcb2.bin
```

**PCB3 빌드:**
```bash
make clean
make CFLAGS="-DBOARD_TYPE_PCB3"
# 출력: firmware_pcb3.bin
```

**PCB4 빌드:**
```bash
make clean
make CFLAGS="-DBOARD_TYPE_PCB4"
# 출력: firmware_pcb4.bin
```

### 코드 사용 예시

컴파일 타임에 코드가 자동으로 최적화됩니다:

```c
#include "board_config.h"

int main(void) {
    HAL_Init();
    SystemClock_Config();

    // 보드 초기화 (컴파일 타임에 결정됨)
    board_init();

    // 조건부 컴파일 가능
#if HAS_BLE
    // BLE 코드 (PCB1, PCB2만 포함)
    ble_start();
#endif

#if HAS_RS485
    // RS485 코드 (PCB3, PCB4만 포함)
    rs485_start();
#endif

#if GPS_COUNT == 2
    // 듀얼 GPS 코드 (PCB3만 포함)
    dual_gps_start();
#endif

    while(1) {
        // 메인 루프
    }
}
```

---

## 다음 단계

1. **GPS 모듈별 드라이버 구현**
   - `lib/gps/gps_f9p.c` - F9P 전용 설정
   - `lib/gps/gps_um982.c` - UM982 전용 설정

2. **통신 인터페이스 구현**
   - `modules/comm/comm_ble.c`
   - `modules/comm/comm_rs485.c`
   - `modules/comm/comm_lora.c`

3. **듀얼 GPS 지원**
   - PCB3용 멀티 GPS 관리 로직

---

## 추천 방식

**프로덕션 환경**: 방법 1 (런타임 감지)
- 하나의 펌웨어로 모든 보드 지원
- 유지보수 편리

**개발/테스트 환경**: 방법 2 (컴파일 타임)
- 코드 크기 최적화
- 디버깅 간편
