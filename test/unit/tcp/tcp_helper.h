#ifndef LWIP_HDR_TCP_HELPER_H
#define LWIP_HDR_TCP_HELPER_H

#include "../lwip_check.h"
#include "arch.h"
#include "tcp.h"
#include "netif.h"

/* counters used for test_tcp_counters_* callback functions */
struct test_tcp_counters {
  uint32_t recv_calls;
  uint32_t recved_bytes;
  uint32_t recv_calls_after_close;
  uint32_t recved_bytes_after_close;
  uint32_t close_calls;
  uint32_t err_calls;
  err_t last_err;
  char* expected_data;
  uint32_t expected_data_len;
};

struct test_tcp_txcounters {
  uint32_t num_tx_calls;
  uint32_t num_tx_bytes;
  uint8_t  copy_tx_packets;
  struct pbuf *tx_packets;
};

extern const ip_addr_t test_local_ip;
extern const ip_addr_t test_remote_ip;
extern const ip_addr_t test_netmask;
#define TEST_REMOTE_PORT    0x100
#define TEST_LOCAL_PORT     0x101

/* Helper functions */
void tcp_remove_all(void);

struct pbuf* tcp_create_segment(ip_addr_t* src_ip, ip_addr_t* dst_ip,
                   uint16_t src_port, uint16_t dst_port, void* data, size_t data_len,
                   uint32_t seqno, uint32_t ackno, uint8_t headerflags);
struct pbuf* tcp_create_rx_segment(struct tcp_pcb* pcb, void* data, size_t data_len,
                   uint32_t seqno_offset, uint32_t ackno_offset, uint8_t headerflags);
struct pbuf* tcp_create_rx_segment_wnd(struct tcp_pcb* pcb, void* data, size_t data_len,
                   uint32_t seqno_offset, uint32_t ackno_offset, uint8_t headerflags, uint16_t wnd);
void tcp_set_state(struct tcp_pcb* pcb, enum tcp_state state, const ip_addr_t* local_ip,
                   const ip_addr_t* remote_ip, uint16_t local_port, uint16_t remote_port);
void test_tcp_counters_err(void* arg, err_t err);
err_t test_tcp_counters_recv(void* arg, struct tcp_pcb* pcb, struct pbuf* p, err_t err);

struct tcp_pcb* test_tcp_new_counters_pcb(struct test_tcp_counters* counters);

void test_tcp_input(struct pbuf *p, struct netif *inp);

void test_tcp_init_netif(struct netif *netif, struct test_tcp_txcounters *txcounters,
                         const ip_addr_t *ip_addr, const ip_addr_t *netmask);


#endif
