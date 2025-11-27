#include "gps_unicore.h"
#include "gps.h"
#include "gps_parse.h"
#include <string.h>

static void parse_unicore_command(gps_t *gps);

/**
 * @brief Unicore $command Q� �
 *
 * : $command,mode base time 60,response: OK*7B
 *
 * @param[inout] gps
 */
static void parse_unicore_command(gps_t *gps) {
  switch (gps->unicore.term_num) {
  case 0: // "command"
    // t� Ux(
    break;

  default:
    // "response:" >0
    if (strstr(gps->unicore.term_str, "response:") != NULL) {
      // "response: OK" � "response: ERROR" �
      const char *resp_start = strstr(gps->unicore.term_str, "response:");
      if (resp_start) {
        resp_start += 9; // "response:" 8t

        // �1 ��
        while (*resp_start == ' ' || *resp_start == ':') {
          resp_start++;
        }

        // Q� ��
        strncpy(gps->unicore_data.response_str, resp_start,
                GPS_UNICORE_TERM_SIZE - 1);
        gps->unicore_data.response_str[GPS_UNICORE_TERM_SIZE - 1] = '\0';

        // OK/ERROR �
        if (strncmp(resp_start, "OK", 2) == 0) {
          gps->unicore_data.last_response = GPS_UNICORE_RESP_OK;
        } else if (strncmp(resp_start, "ERROR", 5) == 0) {
          gps->unicore_data.last_response = GPS_UNICORE_RESP_ERROR;
        } else {
          gps->unicore_data.last_response = GPS_UNICORE_RESP_UNKNOWN;
        }
      }
    }
    break;
  }
}

/**
 * @brief Unicore \�\ term �
 *
 * @param[inout] gps
 * @return uint8_t 1: success, 0: fail
 */
uint8_t gps_parse_unicore_term(gps_t *gps) {
  if (gps->unicore.msg_type == GPS_UNICORE_MSG_NONE) {
    if (gps->state == GPS_PARSE_STATE_UNICORE_START) {
      const char *msg = gps->unicore.term_str;

      if (!strncmp(msg, "command", 7)) {
        gps->unicore.msg_type = GPS_UNICORE_MSG_COMMAND;
      } else if (!strncmp(msg, "config", 6)) {
        gps->unicore.msg_type = GPS_UNICORE_MSG_CONFIG;
      } else {
        gps->unicore.msg_type = GPS_UNICORE_MSG_NONE;
        gps->state = GPS_PARSE_STATE_NONE;
        return 0;
      }

      gps->state = GPS_PARSE_STATE_UNICORE_DATA;
    }

    return 1;
  }

  if (gps->unicore.msg_type == GPS_UNICORE_MSG_COMMAND ||
      gps->unicore.msg_type == GPS_UNICORE_MSG_CONFIG) {
    parse_unicore_command(gps);
  }

  return 1;
}

/**
 * @brief Unicore 바이너리 CRC32 계산
 */
static uint32_t calculate_crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0;

  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xEDB88320;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

/**
 * @brief Unicore 바이너리 프로토콜 파싱
 *
 * UBX와 유사한 상태 머신 방식
 * Sync: 0xAA 0x44 0x12
 * Header: 28 bytes
 * Payload: variable
 * CRC: 4 bytes (CRC32)
 *
 * @param[inout] gps
 */
void gps_parse_unicore_bin(gps_t *gps) {
  char ch = gps->payload[gps->pos - 1];  // 방금 추가된 바이트

  switch (gps->state) {
    case GPS_PARSE_STATE_UNICORE_BIN_SYNC_3:
      // Sync 완료, 헤더 읽기 시작
      gps->state = GPS_PARSE_STATE_UNICORE_BIN_HEADER;
      gps->unicore_bin.header_pos = 0;
      break;

    case GPS_PARSE_STATE_UNICORE_BIN_HEADER:
      {
        // 헤더 28바이트 읽기
        uint8_t* header_ptr = (uint8_t*)&gps->unicore_bin.header;
        header_ptr[gps->unicore_bin.header_pos++] = (uint8_t)ch;

        if (gps->unicore_bin.header_pos >= GPS_UNICORE_BIN_HEADER_SIZE) {
          // 헤더 완료, 페이로드 읽기 시작
          gps->unicore_bin.payload_pos = 0;

          if (gps->unicore_bin.header.msg_len > 0) {
            gps->state = GPS_PARSE_STATE_UNICORE_BIN_PAYLOAD;
          } else {
            // 페이로드 없음, CRC로
            gps->state = GPS_PARSE_STATE_UNICORE_BIN_CRC;
            gps->unicore_bin.crc_pos = 0;
          }
        }
      }
      break;

    case GPS_PARSE_STATE_UNICORE_BIN_PAYLOAD:
      gps->unicore_bin.payload_pos++;

      if (gps->unicore_bin.payload_pos >= gps->unicore_bin.header.msg_len) {
        // 페이로드 완료, CRC 읽기
        gps->state = GPS_PARSE_STATE_UNICORE_BIN_CRC;
        gps->unicore_bin.crc_pos = 0;
      }
      break;

    case GPS_PARSE_STATE_UNICORE_BIN_CRC:
      gps->unicore_bin.crc_bytes[gps->unicore_bin.crc_pos++] = (uint8_t)ch;

      if (gps->unicore_bin.crc_pos >= 4) {
        // CRC 완료, 검증
        uint32_t received_crc =
          (uint32_t)gps->unicore_bin.crc_bytes[0] |
          ((uint32_t)gps->unicore_bin.crc_bytes[1] << 8) |
          ((uint32_t)gps->unicore_bin.crc_bytes[2] << 16) |
          ((uint32_t)gps->unicore_bin.crc_bytes[3] << 24);

        // CRC 계산 (sync 3바이트 + 헤더 + 페이로드)
        size_t total_len = 3 + GPS_UNICORE_BIN_HEADER_SIZE + gps->unicore_bin.header.msg_len;
        uint32_t calculated_crc = calculate_crc32((uint8_t*)gps->payload, total_len);

        if (received_crc == calculated_crc) {
          // 파싱 성공
          LOG_DEBUG("Unicore BIN: ID=%d, Len=%d",
                    gps->unicore_bin.header.msg_id,
                    gps->unicore_bin.header.msg_len);

          // 이벤트 발생 (나중에 메시지별 처리 추가)
          if (gps->handler) {
            gps_msg_t msg = {0};
            gps->handler(gps, GPS_EVENT_DATA_PARSED, GPS_PROTOCOL_UNICORE, msg);
          }
        }

        // 파싱 완료
        gps->protocol = GPS_PROTOCOL_NONE;
        gps->state = GPS_PARSE_STATE_NONE;
        gps->pos = 0;
      }
      break;

    default:
      break;
  }
}
