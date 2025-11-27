#include "gps_unicore.h"
#include "gps.h"
#include "gps_parse.h"
#include <string.h>

static void parse_unicore_command(gps_t *gps);

/**
 * @brief Unicore $command Qõ ñ
 *
 * : $command,mode base time 60,response: OK*7B
 *
 * @param[inout] gps
 */
static void parse_unicore_command(gps_t *gps) {
  switch (gps->unicore.term_num) {
  case 0: // "command"
    // tø Ux(
    break;

  default:
    // "response:" >0
    if (strstr(gps->unicore.term_str, "response:") != NULL) {
      // "response: OK" ” "response: ERROR" ñ
      const char *resp_start = strstr(gps->unicore.term_str, "response:");
      if (resp_start) {
        resp_start += 9; // "response:" 8t

        // õ1 ¤µ
        while (*resp_start == ' ' || *resp_start == ':') {
          resp_start++;
        }

        // Qõ õ¬
        strncpy(gps->unicore_data.response_str, resp_start,
                GPS_UNICORE_TERM_SIZE - 1);
        gps->unicore_data.response_str[GPS_UNICORE_TERM_SIZE - 1] = '\0';

        // OK/ERROR è
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
 * @brief Unicore \ \ term ñ
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
