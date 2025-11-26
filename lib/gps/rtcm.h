#ifndef RTCM_H
#define RTCM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef TAG
    #define TAG "RTCM"
#endif

#include "log.h"

// RTCM3 프로토콜 정의
#define RTCM3_PREAMBLE 0xD3
#define RTCM3_MIN_PACKET_SIZE 6  // 헤더(3) + CRC(3)
#define RTCM3_MAX_PACKET_SIZE 1023 + 6  // 최대 메시지 길이 + 헤더 + CRC

// RTCM 패킷 파싱 상태
typedef enum {
    RTCM_PARSE_STATE_IDLE = 0,
    RTCM_PARSE_STATE_PREAMBLE,
    RTCM_PARSE_STATE_LENGTH,
    RTCM_PARSE_STATE_DATA,
    RTCM_PARSE_STATE_CRC,
    RTCM_PARSE_STATE_COMPLETE
} rtcm_parse_state_t;

// RTCM 패킷 구조체
typedef struct {
    uint8_t buffer[RTCM3_MAX_PACKET_SIZE];
    uint16_t length;          // 페이로드 길이
    uint16_t bytes_received;  // 현재까지 받은 바이트 수
    uint16_t message_type;    // RTCM 메시지 타입
    rtcm_parse_state_t state;
} rtcm_packet_t;

// RTCM 파서 구조체
typedef struct {
    rtcm_packet_t current_packet;
    uint32_t packet_count;     // 총 파싱된 패킷 수
    uint32_t error_count;      // 에러 카운트
} rtcm_parser_t;

/**
 * @brief RTCM 파서 초기화
 * @param parser 파서 구조체 포인터
 */
void rtcm_parser_init(rtcm_parser_t *parser);

/**
 * @brief RTCM 패킷 여부 확인 (프리앰블 체크)
 * @param data 데이터 버퍼
 * @param len 데이터 길이
 * @return true: RTCM 패킷, false: RTCM 패킷 아님
 */
bool rtcm_is_packet(const uint8_t *data, size_t len);

/**
 * @brief RTCM 패킷에서 페이로드 길이 추출
 * @param data 데이터 버퍼 (최소 3바이트 필요)
 * @return 페이로드 길이 (0-1023)
 */
uint16_t rtcm_get_payload_length(const uint8_t *data);

/**
 * @brief RTCM 패킷에서 메시지 타입 추출
 * @param data 데이터 버퍼 (최소 6바이트 필요)
 * @return 메시지 타입 (12비트)
 */
uint16_t rtcm_get_message_type(const uint8_t *data);

/**
 * @brief RTCM 패킷 전체 길이 계산 (헤더 + 페이로드 + CRC)
 * @param payload_length 페이로드 길이
 * @return 전체 패킷 길이
 */
uint16_t rtcm_get_total_length(uint16_t payload_length);

/**
 * @brief CRC24 계산
 * @param data 데이터 버퍼
 * @param len 데이터 길이
 * @return CRC24 값
 */
uint32_t rtcm_crc24(const uint8_t *data, size_t len);

/**
 * @brief RTCM 패킷 CRC 검증
 * @param data 완전한 RTCM 패킷
 * @param len 패킷 길이
 * @return true: CRC 유효, false: CRC 오류
 */
bool rtcm_verify_crc(const uint8_t *data, size_t len);

/**
 * @brief 바이트 단위 RTCM 파싱 (스트림 처리용)
 * @param parser 파서 구조체
 * @param byte 입력 바이트
 * @return true: 패킷 완성, false: 계속 수신 필요
 */
bool rtcm_parse_byte(rtcm_parser_t *parser, uint8_t byte);

/**
 * @brief 완성된 RTCM 패킷 가져오기
 * @param parser 파서 구조체
 * @param out_data 출력 버퍼 포인터
 * @param out_len 출력 길이 포인터
 */
void rtcm_get_packet(rtcm_parser_t *parser, const uint8_t **out_data, uint16_t *out_len);

/**
 * @brief 파서 리셋
 * @param parser 파서 구조체
 */
void rtcm_parser_reset(rtcm_parser_t *parser);

#endif
