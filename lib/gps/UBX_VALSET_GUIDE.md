# UBX VAL-SET/GET 사용 가이드

u-blox GPS 모듈에서 VAL-SET/GET 명령을 사용하여 설정을 변경하는 방법을 설명합니다.

## 목차
- [개요](#개요)
- [주요 특징](#주요-특징)
- [API 설명](#api-설명)
- [사용 방법](#사용-방법)
- [Configuration Key IDs](#configuration-key-ids)
- [문제 해결](#문제-해결)

---

## 개요

UBX VAL-SET/GET은 u-blox GPS 모듈의 설정을 읽고 쓰는 프로토콜입니다.

- **VAL-SET**: 설정값 변경
- **VAL-GET**: 설정값 읽기
- **ACK/NAK**: 명령 성공/실패 응답

이 구현은 **UM982보다 간단한 구조**로 만들어졌습니다:
- Queue 없음 (직접 전송)
- Class/ID 자동 매칭
- 동기/비동기/콜백 모두 지원

---

## 주요 특징

### 1. 세 가지 사용 방식

| 방식 | 설명 | 장점 | 사용 예시 |
|------|------|------|-----------|
| **동기** | ACK/NAK을 기다림 | 간단함 | 초기화 시퀀스 |
| **비동기** | 바로 리턴 후 폴링 | 유연함 | 다른 작업과 병행 |
| **콜백** | ACK/NAK 시 콜백 호출 | 반응성 높음 | 이벤트 기반 처리 |

### 2. Layer 선택

- **RAM**: 임시 저장 (재부팅 시 초기화)
- **BBR**: Battery-backed RAM (배터리 유지)
- **Flash**: 영구 저장 (재부팅 후에도 유지)

### 3. 타임아웃 자동 처리

- 지정된 시간 내에 응답이 없으면 자동으로 타임아웃
- 타임아웃 후 다음 명령 전송 가능

---

## API 설명

### 초기화

```c
void ubx_cmd_handler_init(ubx_cmd_handler_t *handler);
```

GPS 초기화 시 자동으로 호출됨 (`gps_init()`에서).

### 동기 전송 (추천)

```c
bool ubx_send_valset_sync(gps_t *gps,
                          ubx_layer_t layer,
                          const ubx_cfg_item_t *items,
                          size_t item_count,
                          uint32_t timeout_ms);
```

**파라미터:**
- `gps`: GPS 구조체 포인터
- `layer`: `UBX_LAYER_RAM`, `UBX_LAYER_BBR`, `UBX_LAYER_FLASH`
- `items`: 설정 아이템 배열
- `item_count`: 아이템 개수
- `timeout_ms`: 타임아웃 (밀리초)

**리턴:**
- `true`: ACK 받음 (성공)
- `false`: NAK 또는 타임아웃 (실패)

**예시:**
```c
ubx_cfg_item_t items[1];
uint16_t rate = 100;  // 100ms
items[0].key_id = CFG_RATE_MEAS;
memcpy(items[0].value, &rate, sizeof(rate));
items[0].value_len = sizeof(rate);

if (ubx_send_valset_sync(gps, UBX_LAYER_RAM, items, 1, 3000)) {
    printf("설정 성공\n");
} else {
    printf("설정 실패\n");
}
```

### 비동기 전송

```c
bool ubx_send_valset(gps_t *gps,
                     ubx_layer_t layer,
                     const ubx_cfg_item_t *items,
                     size_t item_count);
```

**리턴:**
- `true`: 전송 성공 (ACK는 나중에 확인)
- `false`: 전송 실패 (이미 대기 중인 명령 있음)

**상태 확인:**
```c
ubx_cmd_state_t ubx_get_cmd_state(ubx_cmd_handler_t *handler,
                                  uint32_t timeout_ms);
```

**상태 값:**
- `UBX_CMD_STATE_IDLE`: 명령 없음
- `UBX_CMD_STATE_WAITING`: 응답 대기 중
- `UBX_CMD_STATE_ACK`: ACK 받음
- `UBX_CMD_STATE_NAK`: NAK 받음
- `UBX_CMD_STATE_TIMEOUT`: 타임아웃

**예시:**
```c
// 전송
ubx_send_valset(gps, UBX_LAYER_RAM, items, 1);

// 다른 작업...
vTaskDelay(100);

// 상태 확인
ubx_cmd_state_t state = ubx_get_cmd_state(&gps->ubx_cmd_handler, 3000);
if (state == UBX_CMD_STATE_ACK) {
    printf("성공\n");
}
```

### 콜백 전송

```c
bool ubx_send_valset_cb(gps_t *gps,
                        ubx_layer_t layer,
                        const ubx_cfg_item_t *items,
                        size_t item_count,
                        ubx_ack_callback_t callback,
                        void *user_data);
```

**콜백 타입:**
```c
typedef void (*ubx_ack_callback_t)(bool ack, void *user_data);
```

**예시:**
```c
void on_ack(bool ack, void *user_data) {
    if (ack) {
        printf("설정 성공: %s\n", (char*)user_data);
    } else {
        printf("설정 실패: %s\n", (char*)user_data);
    }
}

ubx_send_valset_cb(gps, UBX_LAYER_RAM, items, 1,
                   on_ack, (void*)"Measurement Rate");
```

### VAL-GET (설정 읽기)

```c
bool ubx_send_valget(gps_t *gps,
                     ubx_layer_t layer,
                     const uint32_t *key_ids,
                     size_t key_count);
```

**예시:**
```c
uint32_t keys[] = {CFG_RATE_MEAS, CFG_RATE_NAV};
ubx_send_valget(gps, UBX_LAYER_RAM, keys, 2);
// 응답은 UBX-CFG-VALGET 메시지로 수신됨
```

---

## 비동기 초기화 (추천!)

**외부 태스크에서 블로킹 없이 초기화하는 방법**

동기 방식(`ubx_send_valset_sync`)은 블로킹이라 다른 태스크를 방해할 수 있습니다. 비동기 초기화를 사용하면 백그라운드에서 초기화가 진행됩니다.

### 비동기 초기화 API

```c
// 초기화 시작
bool ubx_init_async_start(gps_t *gps, ubx_layer_t layer,
                          const ubx_cfg_item_t *configs, size_t config_count,
                          ubx_init_complete_callback_t on_complete,
                          void *user_data);

// 진행 (주기적으로 호출 필요)
void ubx_init_async_process(gps_t *gps);

// 상태 확인
ubx_init_state_t ubx_init_async_get_state(gps_t *gps);

// 취소
void ubx_init_async_cancel(gps_t *gps);
```

### 비동기 초기화 예시

```c
// 설정 배열 (전역 또는 static으로 유지!)
static const ubx_cfg_item_t g_configs[] = {
    {CFG_RATE_MEAS, {100, 0}, 2},
    {CFG_RATE_NAV, {1, 0}, 2},
    {CFG_MSGOUT_UBX_NAV_HPPOSLLH_UART1, {1}, 1},
};

// 완료 콜백
void on_init_done(bool success, size_t failed_step, void *user_data) {
    if (success) {
        printf("✓ Init completed!\n");
    } else {
        printf("✗ Init failed at step %zu\n", failed_step);
    }
}

// 메인 태스크
void main_task(void *pvParam) {
    gps_t *gps = (gps_t *)pvParam;

    // 비동기 초기화 시작
    ubx_init_async_start(gps, UBX_LAYER_RAM,
                         g_configs, 3,
                         on_init_done, NULL);

    // 메인 루프 (블로킹 없음!)
    while (1) {
        // 초기화 진행
        ubx_init_async_process(gps);

        // 다른 작업들 수행 가능
        do_other_work();

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### 비동기 초기화 장점

- ✅ **블로킹 없음** - 다른 태스크를 방해하지 않음
- ✅ **자동 재시도** - NAK/타임아웃 시 자동으로 최대 3번 재시도
- ✅ **진행 상황 추적** - `current_step`으로 진행률 확인 가능
- ✅ **완료 콜백** - 완료 시 자동으로 콜백 호출
- ✅ **취소 가능** - `ubx_init_async_cancel()` 호출로 중단 가능

### 주의사항

⚠️ **설정 배열을 반드시 전역/static으로 선언!**
```c
// ❌ 잘못된 예시 (스택에 생성)
void bad_example() {
    ubx_cfg_item_t configs[3] = {...};  // ← 함수 종료 시 사라짐!
    ubx_init_async_start(gps, UBX_LAYER_RAM, configs, 3, ...);  // 위험!
}

// ✅ 올바른 예시 (static/전역)
static const ubx_cfg_item_t g_configs[3] = {...};
void good_example() {
    ubx_init_async_start(gps, UBX_LAYER_RAM, g_configs, 3, ...);  // 안전!
}
```

---

## 사용 방법

### 1. 기본 사용 (1개 설정 변경)

```c
ubx_cfg_item_t item;
uint16_t rate = 100;  // 100ms

item.key_id = CFG_RATE_MEAS;  // Measurement rate
memcpy(item.value, &rate, sizeof(rate));
item.value_len = sizeof(rate);

bool ok = ubx_send_valset_sync(gps, UBX_LAYER_RAM, &item, 1, 3000);
```

### 2. 여러 설정 동시 변경

```c
ubx_cfg_item_t items[3];

// 1. Measurement rate
uint16_t meas_rate = 200;
items[0].key_id = CFG_RATE_MEAS;
memcpy(items[0].value, &meas_rate, sizeof(meas_rate));
items[0].value_len = sizeof(meas_rate);

// 2. Navigation rate
uint16_t nav_rate = 1;
items[1].key_id = CFG_RATE_NAV;
memcpy(items[1].value, &nav_rate, sizeof(nav_rate));
items[1].value_len = sizeof(nav_rate);

// 3. Output rate
items[2].key_id = CFG_MSGOUT_UBX_NAV_HPPOSLLH_UART1;
items[2].value[0] = 1;
items[2].value_len = 1;

// 한 번에 전송
bool ok = ubx_send_valset_sync(gps, UBX_LAYER_RAM, items, 3, 5000);
```

### 3. 초기화 시퀀스 (권장 패턴)

```c
typedef struct {
    const char *name;
    uint32_t key_id;
    uint8_t value[8];
    uint8_t value_len;
} init_cfg_t;

static const init_cfg_t configs[] = {
    {"Meas rate 100ms", CFG_RATE_MEAS, {100, 0}, 2},
    {"Nav rate 1 cycle", CFG_RATE_NAV, {1, 0}, 2},
    {"Enable HPPOSLLH", CFG_MSGOUT_UBX_NAV_HPPOSLLH_UART1, {1}, 1},
};

bool init_ublox(gps_t *gps) {
    for (size_t i = 0; i < ARRAY_SIZE(configs); i++) {
        ubx_cfg_item_t item;
        item.key_id = configs[i].key_id;
        memcpy(item.value, configs[i].value, configs[i].value_len);
        item.value_len = configs[i].value_len;

        printf("Config: %s\n", configs[i].name);
        if (!ubx_send_valset_sync(gps, UBX_LAYER_RAM, &item, 1, 3000)) {
            printf("Failed at: %s\n", configs[i].name);
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return true;
}
```

### 4. Flash에 저장 (재부팅 후에도 유지)

```c
// 1. RAM에 먼저 설정
ubx_send_valset_sync(gps, UBX_LAYER_RAM, items, count, 3000);

// 2. Flash에 저장
ubx_send_valset_sync(gps, UBX_LAYER_FLASH, items, count, 5000);
```

---

## Configuration Key IDs

### 주요 설정값 예시

```c
// Rate Configuration
#define CFG_RATE_MEAS       0x30210001  // Measurement rate (U2, ms)
#define CFG_RATE_NAV        0x30210002  // Navigation rate (U2, cycles)

// Message Output Rates (UART1)
#define CFG_MSGOUT_UBX_NAV_HPPOSLLH_UART1  0x209100a5  // NAV-HPPOSLLH (U1)
#define CFG_MSGOUT_NMEA_GGA_UART1          0x209100ba  // GGA (U1)
#define CFG_MSGOUT_NMEA_RMC_UART1          0x209100ab  // RMC (U1)

// UART Configuration
#define CFG_UART1_BAUDRATE  0x40520001  // UART1 baud rate (U4)
#define CFG_UART1_ENABLED   0x10520005  // UART1 enable (L)

// GNSS Configuration
#define CFG_SIGNAL_GPS_ENA  0x1031001f  // GPS enable (L)
#define CFG_SIGNAL_GLO_ENA  0x10310025  // GLONASS enable (L)
```

**타입 설명:**
- `U1`: 1 byte unsigned
- `U2`: 2 bytes unsigned (little-endian)
- `U4`: 4 bytes unsigned (little-endian)
- `L`: 1 byte boolean

**중요:**
- `ubx_cfg_item_t`의 `value` 배열은 최대 8 bytes
- `value_len`은 1~8 사이여야 함
- 잘못된 크기는 자동으로 거부됨

전체 목록은 u-blox Interface Description 문서 참고.

---

## 문제 해결

### 1. "이미 대기 중인 명령 있음" 오류

```c
if (!ubx_send_valset(...)) {
    // 이전 명령이 아직 완료되지 않음
    // 상태 확인 후 재시도
    ubx_get_cmd_state(&gps->ubx_cmd_handler, 1000);
}
```

### 2. 타임아웃 발생

- GPS 모듈이 응답하지 않음
- UART 연결 확인
- Baud rate 확인
- 타임아웃 시간 증가 (5000ms 이상)

### 3. NAK 수신

- 잘못된 Configuration Key ID
- 잘못된 Value 타입/크기
- 지원하지 않는 설정
- u-blox 문서에서 Key ID 재확인

### 4. 설정이 적용되지 않음

- Layer 확인 (RAM에만 설정했는지)
- Flash에 저장 필요 시:
  ```c
  ubx_send_valset_sync(gps, UBX_LAYER_FLASH, items, count, 5000);
  ```

### 5. 콜백이 호출되지 않음

- GPS 파서가 실행 중인지 확인
- `gps_parse_process()`가 호출되고 있는지 확인
- ACK 메시지가 수신되고 있는지 확인

---

## UM982와의 차이점

| 항목 | UM982 (기존) | u-blox (개선) |
|------|--------------|---------------|
| Queue | 사용 (복잡함) | 없음 (직접 전송) |
| 응답 매칭 | pending 구조체 | Class/ID 자동 매칭 |
| 동기/비동기 | 동기만 지원 | 둘 다 지원 |
| 콜백 | 없음 | 지원 |
| 타임아웃 | 세마포어 기반 | 폴링 기반 (더 유연) |
| Critical Section | 많음 | 최소화 |
| 코드 복잡도 | 높음 | 낮음 |

---

## 추가 자료

- `ubx_valset_example.c`: 실제 사용 예시 코드
- u-blox Interface Description: 공식 프로토콜 문서
- `lib/gps/gps_ubx.h`: API 헤더 파일
- `lib/gps/gps_ubx.c`: 구현 소스 파일

---

## 요약

**가장 많이 사용하는 패턴:**

```c
// 1개 설정
ubx_cfg_item_t item = {
    .key_id = CFG_RATE_MEAS,
    .value = {100, 0},
    .value_len = 2
};

if (ubx_send_valset_sync(gps, UBX_LAYER_RAM, &item, 1, 3000)) {
    printf("OK\n");
}
```

**여러 설정 + 에러 처리:**

```c
ubx_cfg_item_t items[3] = { /* ... */ };

for (int i = 0; i < 3; i++) {
    if (!ubx_send_valset_sync(gps, UBX_LAYER_RAM, &items[i], 1, 3000)) {
        printf("Failed at item %d\n", i);
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
}
```

간단하고 직관적입니다! 🎉
