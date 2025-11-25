# 멀티 PCB 펌웨어 설정 가이드

이 프로젝트는 **하나의 소스 코드**로 4가지 PCB 버전을 지원합니다.
빌드 전에 보드 타입만 선택하면 됩니다!

## 지원 보드 타입

| 보드 | GPS | 통신 |
|------|-----|------|
| PCB1 | F9P x1 | BLE + LoRa |
| PCB2 | UM982 x1 | BLE + LoRa |
| PCB3 | F9P x2 | RS485 + LoRa |
| PCB4 | UM982 x1 | RS485 + LoRa |

---

## 🚀 빠른 시작

### 방법 1: 스크립트 사용 (추천)

```bash
# 원하는 보드 선택
./select_board.sh PCB1

# 빌드 (STM32CubeIDE에서 Build)
# 빌드하면 선택한 보드용 펌웨어가 생성됩니다!
```

**사용 예시:**
```bash
$ ./select_board.sh PCB2
✅ 보드가 PCB2 (UM982 + BLE + LoRa)로 설정되었습니다.

이제 프로젝트를 빌드하세요!
```

### 방법 2: 수동 설정

`config/board_type.h` 파일을 열어서 원하는 보드의 주석을 해제:

```c
// 예: PCB2를 사용하려면
// #define BOARD_TYPE_PCB1     // F9P + BLE + LoRa
#define BOARD_TYPE_PCB2        // UM982 + BLE + LoRa  ← 이거만 활성화
// #define BOARD_TYPE_PCB3     // F9P x2 + RS485 + LoRa
// #define BOARD_TYPE_PCB4     // UM982 + RS485 + LoRa
```

저장 후 빌드!

---

## 📝 상세 사용법

### 보드 선택 스크립트

`select_board.sh` 스크립트는 `config/board_type.h` 파일을 자동으로 생성합니다.

```bash
# 사용법
./select_board.sh [PCB1|PCB2|PCB3|PCB4]

# 예시
./select_board.sh PCB1  # F9P + BLE + LoRa
./select_board.sh PCB2  # UM982 + BLE + LoRa
./select_board.sh PCB3  # F9P x2 + RS485 + LoRa
./select_board.sh PCB4  # UM982 + RS485 + LoRa
```

### 빌드 방법

#### STM32CubeIDE 사용 시

1. 보드 선택: `./select_board.sh PCB1`
2. STM32CubeIDE에서 **Project → Build Project**
3. 완료!

#### 커맨드라인 빌드 (Makefile 있는 경우)

```bash
# 1. 보드 선택
./select_board.sh PCB1

# 2. 빌드
make clean
make

# 또는 한 줄로
./select_board.sh PCB1 && make clean && make
```

---

## 💻 코드 사용 예시

빌드 시 선택한 보드에 맞춰 자동으로 코드가 최적화됩니다.

### 기본 사용

```c
#include "board_config.h"

int main(void) {
    HAL_Init();
    SystemClock_Config();

    // 보드 초기화 (선택한 보드에 맞춰 자동 초기화)
    board_init();

    // 현재 보드 정보 확인
    const board_config_t* config = board_get_config();
    printf("Board: %s\n", config->board_name);
    printf("GPS Count: %d\n", config->gps_count);

    while(1) {
        // 메인 루프
    }
}
```

### 조건부 컴파일

선택한 보드에 따라 필요한 코드만 컴파일됩니다:

```c
#include "board_config.h"

void setup_communication(void) {
    // BLE 초기화 (PCB1, PCB2만 컴파일됨)
    #if HAS_BLE
        ble_init();
        printf("BLE initialized\n");
    #endif

    // RS485 초기화 (PCB3, PCB4만 컴파일됨)
    #if HAS_RS485
        rs485_init();
        printf("RS485 initialized\n");
    #endif

    // LoRa 초기화 (모든 보드)
    #if HAS_LORA
        lora_init();
        printf("LoRa initialized\n");
    #endif
}

void setup_gps(void) {
    // 듀얼 GPS (PCB3만 컴파일됨)
    #if GPS_COUNT == 2
        printf("Dual GPS mode\n");
        gps_init_primary();
        gps_init_secondary();
    #else
        printf("Single GPS mode\n");
        gps_init_primary();
    #endif

    // GPS 타입별 설정
    #if GPS_PRIMARY == GPS_TYPE_F9P
        printf("Using F9P GPS\n");
        // F9P 전용 설정
    #elif GPS_PRIMARY == GPS_TYPE_UM982
        printf("Using UM982 GPS\n");
        // UM982 전용 설정
    #endif
}
```

### 런타임 보드 정보 확인

```c
void print_board_info(void) {
    const board_config_t* config = board_get_config();

    printf("=== Board Configuration ===\n");
    printf("Board: %s\n", config->board_name);
    printf("GPS Count: %d\n", config->gps_count);
    printf("Primary GPS: %s\n",
        config->gps_primary == GPS_TYPE_F9P ? "F9P" : "UM982");

    if (config->gps_count == 2) {
        printf("Secondary GPS: %s\n",
            config->gps_secondary == GPS_TYPE_F9P ? "F9P" : "UM982");
    }

    printf("Communication:\n");
    if (config->comm_interfaces & COMM_TYPE_BLE)
        printf("  - BLE\n");
    if (config->comm_interfaces & COMM_TYPE_RS485)
        printf("  - RS485\n");
    if (config->comm_interfaces & COMM_TYPE_LORA)
        printf("  - LoRa\n");
}
```

---

## 🎯 장점

### ✅ 하나의 프로젝트
- 모든 보드를 하나의 소스 코드로 관리
- 코드 중복 없음
- 유지보수 간편

### ✅ 쉬운 보드 변경
- 스크립트 한 줄: `./select_board.sh PCB2`
- 또는 파일 한 줄 수정
- 빌드만 다시 하면 끝!

### ✅ 컴파일러 최적화
- 사용하지 않는 코드는 자동 제거
- 각 보드에 최적화된 바이너리 생성
- 메모리 효율적

### ✅ 안전성
- 컴파일 타임에 검증
- 잘못된 설정 시 빌드 에러 발생
- 런타임 오류 없음

---

## 🔧 다음 단계

이제 각 모듈별로 구현하면 됩니다:

### 1. GPS 모듈별 드라이버

`lib/gps/` 또는 `modules/gps/`에 추가:

- `gps_f9p.c/h` - F9P 전용 설정
- `gps_um982.c/h` - UM982 전용 설정
- `gps_multi.c/h` - 듀얼 GPS 관리 (PCB3용)

### 2. 통신 인터페이스

`modules/comm/`에 추가:

- `comm_ble.c/h` - BLE 모듈 제어
- `comm_rs485.c/h` - RS485 드라이버
- `comm_lora.c/h` - LoRa 모듈 제어

### 3. board_init_gps() 구현

`Core/Src/board_config.c`의 `board_init_gps()` 함수에서:

```c
void board_init_gps(gps_type_t gps_type, uint8_t instance) {
    switch(gps_type) {
        case GPS_TYPE_F9P:
            gps_f9p_init(instance);
            break;
        case GPS_TYPE_UM982:
            gps_um982_init(instance);
            break;
        default:
            break;
    }
}
```

### 4. board_init_comm_interfaces() 구현

```c
void board_init_comm_interfaces(void) {
    const board_config_t* config = board_get_config();

    if (config->comm_interfaces & COMM_TYPE_BLE) {
        comm_ble_init();
    }
    if (config->comm_interfaces & COMM_TYPE_RS485) {
        comm_rs485_init();
    }
    if (config->comm_interfaces & COMM_TYPE_LORA) {
        comm_lora_init();
    }
}
```

---

## 📦 파일 구조

```
my_test/
├── config/
│   ├── board_type.h           # 현재 선택된 보드 (자동 생성 또는 수동 편집)
│   └── FreeRTOSConfig.h
├── Core/
│   ├── Inc/
│   │   └── board_config.h     # 보드 설정 API
│   └── Src/
│       └── board_config.c     # 보드 설정 구현
├── select_board.sh            # 보드 선택 스크립트
└── BOARD_CONFIG_EXAMPLE.md    # 이 문서
```

---

## ❓ FAQ

**Q: 보드를 바꾸려면?**
- `./select_board.sh PCB2` 실행 후 빌드

**Q: 현재 선택된 보드 확인?**
- `config/board_type.h` 파일 확인

**Q: 여러 보드용 펌웨어를 한번에 빌드?**
```bash
./select_board.sh PCB1 && make && cp firmware.bin firmware_pcb1.bin
./select_board.sh PCB2 && make && cp firmware.bin firmware_pcb2.bin
./select_board.sh PCB3 && make && cp firmware.bin firmware_pcb3.bin
./select_board.sh PCB4 && make && cp firmware.bin firmware_pcb4.bin
```

**Q: 실수로 여러 보드를 동시에 정의하면?**
- 컴파일 에러 발생: "정확히 하나의 보드 타입만 선택해야 합니다!"

---

## 🎉 완료!

이제 하나의 프로젝트로 4가지 PCB를 관리할 수 있습니다!

궁금한 점이 있으면 `config/board_type.h`를 확인하거나
이 문서를 참고하세요.
