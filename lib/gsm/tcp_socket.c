#include "tcp_socket.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief TCP 소켓 구조체 (내부)
 */
struct tcp_socket_s {
  gsm_t *gsm;
  uint8_t connect_id;
  bool is_connected;
  bool is_closed_by_peer; // 서버가 종료한 경우

  // 수신 데이터 큐 (pbuf 포인터 저장)
  QueueHandle_t rx_queue;
  SemaphoreHandle_t mutex;

  // 기본 수신 타임아웃 (ms)
  uint32_t default_recv_timeout;
};

// 내부 콜백 (forward declaration)
static void _internal_recv_callback(uint8_t connect_id);
static void _internal_close_callback(uint8_t connect_id);

// 소켓 인스턴스 배열 (콜백에서 접근용)
static tcp_socket_t *g_sockets[GSM_TCP_MAX_SOCKETS] = {NULL};

//=============================================================================
// 공개 API
//=============================================================================

tcp_socket_t *tcp_socket_create(gsm_t *gsm, uint8_t connect_id) {
  if (!gsm || connect_id >= GSM_TCP_MAX_SOCKETS) {
    return NULL;
  }

  tcp_socket_t *sock = (tcp_socket_t *)pvPortMalloc(sizeof(tcp_socket_t));
  if (!sock) {
    return NULL;
  }

  memset(sock, 0, sizeof(tcp_socket_t));
  sock->gsm = gsm;
  sock->connect_id = connect_id;
  sock->is_connected = false;
  sock->is_closed_by_peer = false;
  sock->default_recv_timeout = 5000; // 기본 5초

  // 수신 큐 생성 (최대 10개 pbuf 저장)
  sock->rx_queue = xQueueCreate(10, sizeof(tcp_pbuf_t *));
  if (!sock->rx_queue) {
    vPortFree(sock);
    return NULL;
  }

  sock->mutex = xSemaphoreCreateMutex();
  if (!sock->mutex) {
    vQueueDelete(sock->rx_queue);
    vPortFree(sock);
    return NULL;
  }

  // 전역 배열에 저장 (콜백에서 접근용)
  g_sockets[connect_id] = sock;

  return sock;
}

int tcp_connect(tcp_socket_t *sock, uint8_t context_id, const char *remote_ip,
                uint16_t remote_port, uint32_t timeout_ms) {
  if (!sock || !remote_ip) {
    return -1;
  }

  // ★ 내부 콜백 등록하여 gsm_tcp_open() 호출
  int ret = gsm_tcp_open(sock->gsm, sock->connect_id, context_id, remote_ip,
                         remote_port,
                         0, // 로컬 포트 자동
                         _internal_recv_callback, _internal_close_callback,
                         NULL); // 동기식

  if (ret == 0) {
    if (xSemaphoreTake(sock->mutex, portMAX_DELAY) == pdTRUE) {
      sock->is_connected = true;
      sock->is_closed_by_peer = false;
      xSemaphoreGive(sock->mutex);
    }
  }

  return ret;
}

int tcp_send(tcp_socket_t *sock, const uint8_t *data, size_t len) {
  if (!sock || !data || len == 0) {
    return -1;
  }

  if (!sock->is_connected) {
    return -1;
  }

  // ★ 동기식 전송
  int ret = gsm_tcp_send(sock->gsm, sock->connect_id, data, len, NULL);

  if (ret == 0) {
    return (int)len; // 전송 성공
  } else {
    return -1; // 전송 실패
  }
}

void tcp_set_recv_timeout(tcp_socket_t *sock, uint32_t timeout_ms) {
  if (!sock) {
    return;
  }

  if (xSemaphoreTake(sock->mutex, portMAX_DELAY) == pdTRUE) {
    sock->default_recv_timeout = timeout_ms;
    xSemaphoreGive(sock->mutex);
  }
}

int tcp_recv(tcp_socket_t *sock, uint8_t *buf, size_t len,
             uint32_t timeout_ms) {
  if (!sock || !buf || len == 0) {
    return -1;
  }

  // ★ timeout_ms=0이면 기본값 사용
  if (timeout_ms == 0) {
    if (xSemaphoreTake(sock->mutex, portMAX_DELAY) == pdTRUE) {
      timeout_ms = sock->default_recv_timeout;
      xSemaphoreGive(sock->mutex);
    } else {
      timeout_ms = 5000; // fallback
    }
  }

  TickType_t timeout_ticks =
      (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

  // ★ 큐에서 pbuf 꺼내기 (블로킹)
  tcp_pbuf_t *pbuf = NULL;
  if (xQueueReceive(sock->rx_queue, &pbuf, timeout_ticks) == pdTRUE) {
    if (!pbuf) {
      return -1; // NULL pbuf (에러)
    }

    // 데이터 복사
    size_t copy_len = (pbuf->len < len) ? pbuf->len : len;
    memcpy(buf, pbuf->payload, copy_len);

    // if(pbuf->len > copy_len)
    // {
    //     // 남은 데이터로 새 pbuf 생성
    //     size_t remaining = pbuf->len - copy_len;
    //     tcp_pbuf_t *new_pbuf = tcp_pbuf_alloc(remaining);
    //     if(new_pbuf)
    //     {
    //         memcpy(new_pbuf->payload, pbuf->payload + copy_len, remaining);
    //         // 큐 앞에 넣기 (다음 recv에서 먼저 읽힘)
    //         xQueueSendToFront(sock->rx_queue, &new_pbuf, 0);
    //     }
    //     // 메모리 부족 시 남은 데이터 손실 (어쩔 수 없음)
    // }

    // pbuf 해제
    tcp_pbuf_free(pbuf);

    return (int)copy_len;
  } else {
    // 타임아웃
    return 0;
  }
}

int tcp_close(tcp_socket_t *sock) {
  if (!sock) {
    return -1;
  }

  if (!sock->is_connected) {
    return 0; // 이미 닫힘
  }

  // ★ 동기식 닫기
  int ret = gsm_tcp_close(sock->gsm, sock->connect_id, NULL);

  if (xSemaphoreTake(sock->mutex, portMAX_DELAY) == pdTRUE) {
    sock->is_connected = false;
    xSemaphoreGive(sock->mutex);
  }

  return ret;
}


int tcp_close_force(tcp_socket_t *sock) {

  if (!sock) {

    return -1;

  }

 

  // ★ 상태 무관 강제 닫기 (에러 566 복구용)

  int ret = gsm_tcp_close_force(sock->gsm, sock->connect_id);

 

  if (xSemaphoreTake(sock->mutex, portMAX_DELAY) == pdTRUE) {

    sock->is_connected = false;

    sock->is_closed_by_peer = false;

    xSemaphoreGive(sock->mutex);

  }

 

  return ret;

}
void tcp_socket_destroy(tcp_socket_t *sock) {
  if (!sock) {
    return;
  }

  // 연결 종료
  if (sock->is_connected) {
    tcp_close(sock);
  }

  // 큐에 남은 pbuf 모두 해제
  tcp_pbuf_t *pbuf;
  while (xQueueReceive(sock->rx_queue, &pbuf, 0) == pdTRUE) {
    if (pbuf) {
      tcp_pbuf_free(pbuf);
    }
  }

  // 전역 배열에서 제거
  g_sockets[sock->connect_id] = NULL;

  // 리소스 해제
  vQueueDelete(sock->rx_queue);
  vSemaphoreDelete(sock->mutex);
  vPortFree(sock);
}

size_t tcp_available(tcp_socket_t *sock) {
  if (!sock) {
    return 0;
  }

  return uxQueueMessagesWaiting(sock->rx_queue);
}

//=============================================================================
// 내부 콜백 (gsm_tcp_open에서 호출됨)
//=============================================================================

/**
 * @brief 내부 수신 콜백
 *
 * gsm_tcp_read() 완료 시 호출됨
 * → pbuf를 큐에 추가하여 tcp_recv()에서 읽을 수 있게 함
 */
static void _internal_recv_callback(uint8_t connect_id) {
  if (connect_id >= GSM_TCP_MAX_SOCKETS) {
    return;
  }

  tcp_socket_t *sock = g_sockets[connect_id];
  if (!sock || !sock->gsm) {
    return;
  }

  tcp_pbuf_t *pbuf = NULL;
  if (xSemaphoreTake(sock->gsm->tcp.tcp_mutex, portMAX_DELAY) == pdTRUE) {
    pbuf = tcp_pbuf_dequeue(&sock->gsm->tcp.sockets[connect_id]);
    xSemaphoreGive(sock->gsm->tcp.tcp_mutex);
  }

  if (!pbuf) {
    return; // 데이터 없음
  }

  // ★ rx_queue에 pbuf 포인터 추가
  if (xQueueSend(sock->rx_queue, &pbuf, 0) != pdTRUE) {
    // 큐 가득 참 - pbuf 버림
    tcp_pbuf_free(pbuf);

    // 메모리 오버플로우 방지: 애플리케이션이 수신 속도를 따라가지 못함
    // 해결: tcp_recv() 호출 주기를 줄이거나 큐 크기 증가 필요
  }
}

/**
 * @brief 내부 연결 종료 콜백
 *
 * 서버가 연결을 종료했을 때 호출됨
 */
static void _internal_close_callback(uint8_t connect_id) {
  if (connect_id >= GSM_TCP_MAX_SOCKETS) {
    return;
  }

  tcp_socket_t *sock = g_sockets[connect_id];
  if (!sock) {
    return;
  }

  if (xSemaphoreTake(sock->mutex, portMAX_DELAY) == pdTRUE) {
    sock->is_connected = false;
    sock->is_closed_by_peer = true;
    xSemaphoreGive(sock->mutex);
  }

  // ★ NULL pbuf를 큐에 넣어 tcp_recv() 깨우기 (종료 알림)
  tcp_pbuf_t *null_pbuf = NULL;
  xQueueSend(sock->rx_queue, &null_pbuf, 0);
}

gsm_tcp_state_t tcp_get_socket_state(tcp_socket_t *sock, uint8_t id) {
  return sock->gsm->tcp.sockets[id].state;
}
