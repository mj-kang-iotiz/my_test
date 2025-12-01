#ifndef GPS_UBX_H
#define GPS_UBX_H

#include "gps_types.h"
#include <stdint.h>

/**
 * @brief ubx 프로토콜 클래스 타입
 *
 */
typedef enum {
  GPS_UBX_CLASS_NONE = 0,
  GPS_UBX_CLASS_NAV = 0x01,
  GPS_UBX_CLASS_ACK = 0x05,
  GPS_UBX_CLASS_CFG = 0x06,
} gps_ubx_class_t;

/**
 * @brief ubx 프로토콜 NAV 클래스 메시지 id
 *
 */
typedef enum {
  GPS_UBX_NAV_ID_NONE = 0,
  GPS_UBX_NAV_ID_HPPOSLLH = 0x14,
} gps_ubx_nav_id_t;

/**
 * @brief ubx 프로토콜 ACK 클래스 메시지 id
 *
 */
typedef enum {
  GPS_UBX_ACK_ID_NAK = 0x00,
  GPS_UBX_ACK_ID_ACK = 0x01,
} gps_ubx_ack_id_t;

/**
 * @brief ubx 프로토콜 CFG 클래스 메시지 id
 *
 */
typedef enum {
  GPS_UBX_CFG_ID_VALSET = 0x8A,
  GPS_UBX_CFG_ID_VALGET = 0x8B,
  GPS_UBX_CFG_ID_VALDEL = 0x8C,
} gps_ubx_cfg_id_t;

/**
 * @brief ubx 프로토콜 NAV 클래스 HPPOSLLH 메시지
 *
 */
typedef struct {
  uint8_t version; // 0x00
  uint8_t reserved[2];
  uint8_t flag; // 0: valid 1: invalid
  uint32_t tow; // time of week [ms]
  int32_t lon;
  int32_t lat;
  int32_t height;   // [mm]
  int32_t msl;      // [mm]
  int8_t lon_hp;    // 1e-9 단위 / deg * 1e-7 = lon + (lon_hp * 1e-2)
  int8_t lat_hp;    // 1e-9 단위 / deg * 1e-7 = lat + (lat_hp * 1e-2)
  int8_t height_hp; // 0.1mm 단위 / height + height_hp * 0.1 [mm]
  int8_t msl_hp;    // 0.1mm 단위 / msl + msl_hp * 0.1 [mm]
  uint32_t hacc;    // 0.1mm 단위
  uint32_t vacc;    // 0.1mm 단위
} gps_ubx_nav_hpposllh_t;

/**
 * @brief UBX 파싱에 필요한 변수
 *
 */
typedef struct {
  uint8_t class;
  uint8_t id;
  uint16_t len;     // 2byte
  uint8_t chksum_a; // 1byte
  uint8_t chksum_b; // 1byte

  uint8_t cal_chksum_a;
  uint8_t cal_chksum_b;
} gps_ubx_parser_t;

/**
 * @brief UBX 프로토콜 데이터
 *
 */
typedef struct {
  gps_ubx_nav_hpposllh_t hpposllh;
} gps_ubx_data_t;

/**
 * @brief UBX Layer 정의 (VAL-SET/GET/DEL)
 *
 */
typedef enum {
  UBX_LAYER_RAM = 0x01,     // RAM layer
  UBX_LAYER_BBR = 0x02,     // Battery-backed RAM
  UBX_LAYER_FLASH = 0x04,   // Flash (persistent)
} ubx_layer_t;

/**
 * @brief UBX 명령 상태
 *
 */
typedef enum {
  UBX_CMD_STATE_IDLE = 0,
  UBX_CMD_STATE_WAITING,    // 응답 대기 중
  UBX_CMD_STATE_ACK,        // ACK 받음
  UBX_CMD_STATE_NAK,        // NAK 받음
  UBX_CMD_STATE_TIMEOUT,    // 타임아웃
} ubx_cmd_state_t;

/**
 * @brief UBX 명령 콜백 타입
 *
 */
typedef void (*ubx_ack_callback_t)(bool ack, void *user_data);

/**
 * @brief UBX 명령 핸들러 (pending command 관리)
 *
 */
typedef struct {
  uint8_t pending_cls;              // 대기 중인 명령 Class
  uint8_t pending_id;               // 대기 중인 명령 ID
  ubx_cmd_state_t state;            // 명령 상태
  uint32_t timestamp;               // 명령 전송 시각 (ms)

  // 콜백 (옵션)
  ubx_ack_callback_t callback;
  void *callback_data;
} ubx_cmd_handler_t;

/**
 * @brief UBX CFG item (VAL-SET/GET용)
 *
 */
typedef struct {
  uint32_t key_id;          // Configuration key ID
  uint8_t value[8];         // Value (최대 8 bytes)
  uint8_t value_len;        // Value length
} ubx_cfg_item_t;

typedef struct gps_s gps_t;

/* Parsing functions */
uint8_t gps_parse_ubx(gps_t *gps);

/* Command functions */
void ubx_cmd_handler_init(ubx_cmd_handler_t *handler);
ubx_cmd_state_t ubx_get_cmd_state(ubx_cmd_handler_t *handler, uint32_t timeout_ms);

bool ubx_send_valset(gps_t *gps, ubx_layer_t layer,
                     const ubx_cfg_item_t *items, size_t item_count);

bool ubx_send_valset_cb(gps_t *gps, ubx_layer_t layer,
                        const ubx_cfg_item_t *items, size_t item_count,
                        ubx_ack_callback_t callback, void *user_data);

bool ubx_send_valset_sync(gps_t *gps, ubx_layer_t layer,
                          const ubx_cfg_item_t *items, size_t item_count,
                          uint32_t timeout_ms);

bool ubx_send_valget(gps_t *gps, ubx_layer_t layer,
                     const uint32_t *key_ids, size_t key_count);

/* Helper functions */
void ubx_calc_checksum(const uint8_t *data, size_t len,
                       uint8_t *ck_a, uint8_t *ck_b);

#endif
