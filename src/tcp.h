/**
 * @file
 * TCP API (to be used from TCPIP thread)\n
 * See also @ref tcp_raw
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#ifndef LWIP_HDR_TCP_H
#define LWIP_HDR_TCP_H

#include "err.h"

#include "icmp.h"

#include "ip.h"

#include "ip6.h"

#include "ip6_addr.h"

#include "mem.h"

#include "opt.h"

#include "PacketBuffer.h"

#include "tcpbase.h"


/* Length of the TCP header, excluding options. */
#define TCP_HLEN 20

/* Fields are (of course) in network byte order.
 * Some fields are converted to host byte order in tcp_input().
 */
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct tcp_hdr {
    PACK_STRUCT_FIELD(uint16_t src);
    PACK_STRUCT_FIELD(uint16_t dest);
    PACK_STRUCT_FIELD(uint32_t seqno);
    PACK_STRUCT_FIELD(uint32_t ackno);
    PACK_STRUCT_FIELD(uint16_t _hdrlen_rsvd_flags);
    PACK_STRUCT_FIELD(uint16_t wnd);
    PACK_STRUCT_FIELD(uint16_t chksum);
    PACK_STRUCT_FIELD(uint16_t urgp);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "epstruct.h"
#endif

/* TCP header flags bits */
#define TCP_FIN 0x01U
#define TCP_SYN 0x02U
#define TCP_RST 0x04U
#define TCP_PSH 0x08U
#define TCP_ACK 0x10U
#define TCP_URG 0x20U
#define TCP_ECE 0x40U
#define TCP_CWR 0x80U
/* Valid TCP header flags */
#define TCP_FLAGS 0x3fU

#define TCP_MAX_OPTION_BYTES 40

#define TCPH_HDRLEN(phdr) ((uint16_t)(lwip_ntohs((phdr)->_hdrlen_rsvd_flags) >> 12))
#define TCPH_HDRLEN_BYTES(phdr) ((uint8_t)(TCPH_HDRLEN(phdr) << 2))
#define TCPH_FLAGS(phdr)  ((uint8_t)((lwip_ntohs((phdr)->_hdrlen_rsvd_flags) & TCP_FLAGS)))

#define TCPH_HDRLEN_SET(phdr, len) (phdr)->_hdrlen_rsvd_flags = lwip_htons(((len) << 12) | TCPH_FLAGS(phdr))
#define TCPH_FLAGS_SET(phdr, flags) (phdr)->_hdrlen_rsvd_flags = (((phdr)->_hdrlen_rsvd_flags & PP_HTONS(~TCP_FLAGS)) | lwip_htons(flags))
#define TCPH_HDRLEN_FLAGS_SET(phdr, len, flags) (phdr)->_hdrlen_rsvd_flags = (uint16_t)(lwip_htons((uint16_t)((len) << 12) | (flags)))

#define TCPH_SET_FLAG(phdr, flags ) (phdr)->_hdrlen_rsvd_flags = ((phdr)->_hdrlen_rsvd_flags | lwip_htons(flags))
#define TCPH_UNSET_FLAG(phdr, flags) (phdr)->_hdrlen_rsvd_flags = ((phdr)->_hdrlen_rsvd_flags & ~lwip_htons(flags))


#if LWIP_TCP /* don't build if not configured for use in lwipopts.h */



#ifdef __cplusplus
extern "C" {
#endif

struct tcp_pcb;
struct tcp_pcb_listen;

/** Function prototype for tcp accept callback functions. Called when a new
 * connection can be accepted on a listening pcb.
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param newpcb The new connection pcb
 * @param err An error code if there has been an error accepting.
 *            Only return ERR_ABRT if you have called tcp_abort from within the
 *            callback function!
 */
typedef err_t (*tcp_accept_fn)(void *arg, struct tcp_pcb *newpcb, err_t err);

/** Function prototype for tcp receive callback functions. Called when data has
 * been received.
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param tpcb The connection pcb which received data
 * @param p The received data (or NULL when the connection has been closed!)
 * @param err An error code if there has been an error receiving
 *            Only return ERR_ABRT if you have called tcp_abort from within the
 *            callback function!
 */
typedef err_t (*tcp_recv_fn)(void *arg, struct tcp_pcb *tpcb,
                             struct pbuf *p, err_t err);

/** Function prototype for tcp sent callback functions. Called when sent data has
 * been acknowledged by the remote side. Use it to free corresponding resources.
 * This also means that the pcb has now space available to send new data.
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param tpcb The connection pcb for which data has been acknowledged
 * @param len The amount of bytes acknowledged
 * @return ERR_OK: try to send some data by calling tcp_output
 *            Only return ERR_ABRT if you have called tcp_abort from within the
 *            callback function!
 */
typedef err_t (*tcp_sent_fn)(void *arg, struct tcp_pcb *tpcb,
                              uint16_t len);

/** Function prototype for tcp poll callback functions. Called periodically as
 * specified by @see tcp_poll.
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param tpcb tcp pcb
 * @return ERR_OK: try to send some data by calling tcp_output
 *            Only return ERR_ABRT if you have called tcp_abort from within the
 *            callback function!
 */
typedef err_t (*tcp_poll_fn)(void *arg, struct tcp_pcb *tpcb);

/** Function prototype for tcp error callback functions. Called when the pcb
 * receives a RST or is unexpectedly closed for any other reason.
 *
 * @note The corresponding pcb is already freed when this callback is called!
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param err Error code to indicate why the pcb has been closed
 *            ERR_ABRT: aborted through tcp_abort or by a TCP timer
 *            ERR_RST: the connection was reset by the remote host
 */
typedef void  (*tcp_err_fn)(void *arg, err_t err);

/** Function prototype for tcp connected callback functions. Called when a pcb
 * is connected to the remote side after initiating a connection attempt by
 * calling tcp_connect().
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param tpcb The connection pcb which is connected
 * @param err An unused error code, always ERR_OK currently ;-) @todo!
 *            Only return ERR_ABRT if you have called tcp_abort from within the
 *            callback function!
 *
 * @note When a connection attempt fails, the error callback is currently called!
 */
typedef err_t (*tcp_connected_fn)(void *arg, struct tcp_pcb *tpcb, err_t err);

#if LWIP_WND_SCALE
#define RCV_WND_SCALE(pcb, wnd) (((wnd) >> (pcb)->rcv_scale))
#define SND_WND_SCALE(pcb, wnd) (((wnd) << (pcb)->snd_scale))
#define TCPWND16(x)             ((uint16_t)LWIP_MIN((x), 0xFFFF))
#define TCP_WND_MAX(pcb)        ((tcpwnd_size_t)(((pcb)->flags & TF_WND_SCALE) ? TCP_WND : TCPWND16(TCP_WND)))
#else
#define RCV_WND_SCALE(pcb, wnd) (wnd)
#define SND_WND_SCALE(pcb, wnd) (wnd)
#define TCPWND16(x)             (x)
#define TCP_WND_MAX(pcb)        TCP_WND
#endif
/* Increments a tcpwnd_size_t and holds at max value rather than rollover */
#define TCP_WND_INC(wnd, inc)   do { \
                                  if ((tcpwnd_size_t)(wnd + inc) >= wnd) { \
                                    wnd = (tcpwnd_size_t)(wnd + inc); \
                                  } else { \
                                    wnd = (tcpwnd_size_t)-1; \
                                  } \
                                } while(0)

#if LWIP_TCP_SACK_OUT
/** SACK ranges to include in ACK packets.
 * SACK entry is invalid if left==right. */
struct tcp_sack_range {
  /** Left edge of the SACK: the first acknowledged sequence number. */
  uint32_t left;
  /** Right edge of the SACK: the last acknowledged sequence number +1 (so first NOT acknowledged). */
  uint32_t right;
};
#endif /* LWIP_TCP_SACK_OUT */

/** Function prototype for deallocation of arguments. Called *just before* the
 * pcb is freed, so don't expect to be able to do anything with this pcb!
 *
 * @param id ext arg id (allocated via @ref tcp_ext_arg_alloc_id)
 * @param data pointer to the data (set via @ref tcp_ext_arg_set before)
 */
typedef void (*tcp_extarg_callback_pcb_destroyed_fn)(uint8_t id, void *data);

/** Function prototype to transition arguments from a listening pcb to an accepted pcb
 *
 * @param id ext arg id (allocated via @ref tcp_ext_arg_alloc_id)
 * @param lpcb the listening pcb accepting a connection
 * @param cpcb the newly allocated connection pcb
 * @return ERR_OK if OK, any error if connection should be dropped
 */
typedef err_t (*tcp_extarg_callback_passive_open_fn)(uint8_t id, struct tcp_pcb_listen *lpcb, struct tcp_pcb *cpcb);

/** A table of callback functions that is invoked for ext arguments */
struct tcp_ext_arg_callbacks {
  /** @ref tcp_extarg_callback_pcb_destroyed_fn */
  tcp_extarg_callback_pcb_destroyed_fn destroy;
  /** @ref tcp_extarg_callback_passive_open_fn */
  tcp_extarg_callback_passive_open_fn passive_open;
};

#define LWIP_TCP_PCB_NUM_EXT_ARG_ID_INVALID 0xFF

#if LWIP_TCP_PCB_NUM_EXT_ARGS
/* This is the structure for ext args in tcp pcbs (used as array) */
struct tcp_pcb_ext_args {
  const struct tcp_ext_arg_callbacks *callbacks;
  void *data;
};
/* This is a helper define to prevent zero size arrays if disabled */
#define TCP_PCB_EXTARGS struct tcp_pcb_ext_args ext_args[LWIP_TCP_PCB_NUM_EXT_ARGS];
#else
#define TCP_PCB_EXTARGS
#endif

typedef uint16_t tcpflags_t;
#define TCP_ALLFLAGS 0xffffU

/**
 * members common to struct tcp_pcb and struct tcp_listen_pcb
 */
#define TCP_PCB_COMMON(type) \
  type *next; /* for the linked list */ \
  void *callback_arg; \
  TCP_PCB_EXTARGS \
  enum tcp_state state; /* TCP state */ \
  uint8_t prio; \
  /* ports are in host byte order */ \
  uint16_t local_port


/** the TCP protocol control block for listening pcbs */
struct tcp_pcb_listen {
/** Common members of all PCB types */
  IP_PCB;
/** Protocol specific PCB members */
  TCP_PCB_COMMON(struct tcp_pcb_listen);

#if LWIP_CALLBACK_API
  /* Function to call when a listener has been connected. */
  tcp_accept_fn accept_fn;
#endif /* LWIP_CALLBACK_API */

#if TCP_LISTEN_BACKLOG
  uint8_t backlog;
  uint8_t accepts_pending;
#endif /* TCP_LISTEN_BACKLOG */
};


/** the TCP protocol control block */
struct tcp_pcb {
/** common PCB members */
  IP_PCB;
/** protocol specific PCB members */
  TCP_PCB_COMMON(struct tcp_pcb);

  /* ports are in host byte order */
  uint16_t remote_port;

  tcpflags_t flags;
#define TF_ACK_DELAY   0x01U   /* Delayed ACK. */
#define TF_ACK_NOW     0x02U   /* Immediate ACK. */
#define TF_INFR        0x04U   /* In fast recovery. */
#define TF_CLOSEPEND   0x08U   /* If this is set, tcp_close failed to enqueue the FIN (retried in tcp_tmr) */
#define TF_RXCLOSED    0x10U   /* rx closed by tcp_shutdown */
#define TF_FIN         0x20U   /* Connection was closed locally (FIN segment enqueued). */
#define TF_NODELAY     0x40U   /* Disable Nagle algorithm */
#define TF_NAGLEMEMERR 0x80U   /* nagle enabled, memerr, try to output to prevent delayed ACK to happen */
#if LWIP_WND_SCALE
#define TF_WND_SCALE   0x0100U /* Window Scale option enabled */
#endif
#if TCP_LISTEN_BACKLOG
#define TF_BACKLOGPEND 0x0200U /* If this is set, a connection pcb has increased the backlog on its listener */
#endif
#if LWIP_TCP_TIMESTAMPS
#define TF_TIMESTAMP   0x0400U   /* Timestamp option enabled */
#endif
#define TF_RTO         0x0800U /* RTO timer has fired, in-flight data moved to unsent and being retransmitted */
#if LWIP_TCP_SACK_OUT
#define TF_SACK        0x1000U /* Selective ACKs enabled */
#endif

  /* the rest of the fields are in host byte order
     as we have to do some math with them */

  /* Timers */
  uint8_t polltmr, pollinterval;
  uint8_t last_timer;
  uint32_t tmr;

  /* receiver variables */
  uint32_t rcv_nxt;   /* next seqno expected */
  tcpwnd_size_t rcv_wnd;   /* receiver window available */
  tcpwnd_size_t rcv_ann_wnd; /* receiver window to announce */
  uint32_t rcv_ann_right_edge; /* announced right edge of window */

#if LWIP_TCP_SACK_OUT
  /* SACK ranges to include in ACK packets (entry is invalid if left==right) */
  struct tcp_sack_range rcv_sacks[LWIP_TCP_MAX_SACK_NUM];
#define LWIP_TCP_SACK_VALID(pcb, idx) ((pcb)->rcv_sacks[idx].left != (pcb)->rcv_sacks[idx].right)
#endif /* LWIP_TCP_SACK_OUT */

  /* Retransmission timer. */
  int16_t rtime;

  uint16_t mss;   /* maximum segment size */

  /* RTT (round trip time) estimation variables */
  uint32_t rttest; /* RTT estimate in 500ms ticks */
  uint32_t rtseq;  /* sequence number being timed */
  int16_t sa, sv; /* @see "Congestion Avoidance and Control" by Van Jacobson and Karels */

  int16_t rto;    /* retransmission time-out (in ticks of TCP_SLOW_INTERVAL) */
  uint8_t nrtx;    /* number of retransmissions */

  /* fast retransmit/recovery */
  uint8_t dupacks;
  uint32_t lastack; /* Highest acknowledged seqno. */

  /* congestion avoidance/control variables */
  tcpwnd_size_t cwnd;
  tcpwnd_size_t ssthresh;

  /* first byte following last rto byte */
  uint32_t rto_end;

  /* sender variables */
  uint32_t snd_nxt;   /* next new seqno to be sent */
  uint32_t snd_wl1, snd_wl2; /* Sequence and acknowledgement numbers of last
                             window update. */
  uint32_t snd_lbb;       /* Sequence number of next byte to be buffered. */
  tcpwnd_size_t snd_wnd;   /* sender window */
  tcpwnd_size_t snd_wnd_max; /* the maximum sender window announced by the remote host */

  tcpwnd_size_t snd_buf;   /* Available buffer space for sending (in bytes). */
#define TCP_SNDQUEUELEN_OVERFLOW (0xffffU-3)
  uint16_t snd_queuelen; /* Number of pbufs currently in the send buffer. */

#if TCP_OVERSIZE
  /* Extra bytes available at the end of the last PacketBuffer in unsent. */
  uint16_t unsent_oversize;
#endif /* TCP_OVERSIZE */

  tcpwnd_size_t bytes_acked;

  /* These are ordered by sequence number: */
  struct tcp_seg *unsent;   /* Unsent (queued) segments. */
  struct tcp_seg *unacked;  /* Sent but unacknowledged segments. */
#if TCP_QUEUE_OOSEQ
  struct tcp_seg *ooseq;    /* Received out of sequence segments. */
#endif /* TCP_QUEUE_OOSEQ */

  struct pbuf *refused_data; /* Data previously received but not yet taken by upper layer */

#if LWIP_CALLBACK_API || TCP_LISTEN_BACKLOG
  struct tcp_pcb_listen* listener;
#endif /* LWIP_CALLBACK_API || TCP_LISTEN_BACKLOG */

#if LWIP_CALLBACK_API
  /* Function to be called when more send buffer space is available. */
  tcp_sent_fn sent;
  /* Function to be called when (in-sequence) data has arrived. */
  tcp_recv_fn recv;
  /* Function to be called when a connection has been set up. */
  tcp_connected_fn connected;
  /* Function which is called periodically. */
  tcp_poll_fn poll;
  /* Function to be called whenever a fatal error occurs. */
  tcp_err_fn errf;
#endif /* LWIP_CALLBACK_API */

#if LWIP_TCP_TIMESTAMPS
  uint32_t ts_lastacksent;
  uint32_t ts_recent;
#endif /* LWIP_TCP_TIMESTAMPS */

  /* idle time before KEEPALIVE is sent */
  uint32_t keep_idle;
#if LWIP_TCP_KEEPALIVE
  uint32_t keep_intvl;
  uint32_t keep_cnt;
#endif /* LWIP_TCP_KEEPALIVE */

  /* Persist timer counter */
  uint8_t persist_cnt;
  /* Persist timer back-off */
  uint8_t persist_backoff;
  /* Number of persist probes */
  uint8_t persist_probe;

  /* KEEPALIVE counter */
  uint8_t keep_cnt_sent;

#if LWIP_WND_SCALE
  uint8_t snd_scale;
  uint8_t rcv_scale;
#endif
};

#if LWIP_EVENT_API

enum lwip_event {
  LWIP_EVENT_ACCEPT,
  LWIP_EVENT_SENT,
  LWIP_EVENT_RECV,
  LWIP_EVENT_CONNECTED,
  LWIP_EVENT_POLL,
  LWIP_EVENT_ERR
};

LwipError lwip_tcp_event(void *arg, struct tcp_pcb *pcb,
         enum lwip_event,
         struct PacketBuffer *p,
         uint16_t size,
         LwipError err);

#endif /* LWIP_EVENT_API */

/* Application program's interface: */
struct tcp_pcb * tcp_new     (void);
struct tcp_pcb * tcp_new_ip_type (uint8_t type);

void             tcp_arg     (struct tcp_pcb *pcb, void *arg);
#if LWIP_CALLBACK_API
void             tcp_recv    (struct tcp_pcb *pcb, tcp_recv_fn recv);
void             tcp_sent    (struct tcp_pcb *pcb, tcp_sent_fn sent);
void             tcp_err     (struct tcp_pcb *pcb, tcp_err_fn err);
void             tcp_accept  (struct tcp_pcb *pcb, tcp_accept_fn accept);
#endif /* LWIP_CALLBACK_API */
void             tcp_poll    (struct tcp_pcb *pcb, tcp_poll_fn poll, uint8_t interval);

#define          tcp_set_flags(pcb, set_flags)     do { (pcb)->flags = (tcpflags_t)((pcb)->flags |  (set_flags)); } while(0)
#define          tcp_clear_flags(pcb, clr_flags)   do { (pcb)->flags = (tcpflags_t)((pcb)->flags & (tcpflags_t)(~(clr_flags) & TCP_ALLFLAGS)); } while(0)
#define          tcp_is_flag_set(pcb, flag)        (((pcb)->flags & (flag)) != 0)

#if LWIP_TCP_TIMESTAMPS
#define          tcp_mss(pcb)             (((pcb)->flags & TF_TIMESTAMP) ? ((pcb)->mss - 12)  : (pcb)->mss)
#else /* LWIP_TCP_TIMESTAMPS */
/** @ingroup tcp_raw */
#define          tcp_mss(pcb)             ((pcb)->mss)
#endif /* LWIP_TCP_TIMESTAMPS */
/** @ingroup tcp_raw */
#define          tcp_sndbuf(pcb)          (TCPWND16((pcb)->snd_buf))
/** @ingroup tcp_raw */
#define          tcp_sndqueuelen(pcb)     ((pcb)->snd_queuelen)
/** @ingroup tcp_raw */
#define          tcp_nagle_disable(pcb)   tcp_set_flags(pcb, TF_NODELAY)
/** @ingroup tcp_raw */
#define          tcp_nagle_enable(pcb)    tcp_clear_flags(pcb, TF_NODELAY)
/** @ingroup tcp_raw */
#define          tcp_nagle_disabled(pcb)  tcp_is_flag_set(pcb, TF_NODELAY)

#if TCP_LISTEN_BACKLOG
#define          tcp_backlog_set(pcb, new_backlog) do { \
  LWIP_ASSERT("pcb->state == LISTEN (called for wrong pcb?)", (pcb)->state == LISTEN); \
  ((struct tcp_pcb_listen *)(pcb))->backlog = ((new_backlog) ? (new_backlog) : 1); } while(0)
void             tcp_backlog_delayed(struct tcp_pcb* pcb);
void             tcp_backlog_accepted(struct tcp_pcb* pcb);
#else  /* TCP_LISTEN_BACKLOG */
#define          tcp_backlog_set(pcb, new_backlog)
#define          tcp_backlog_delayed(pcb)
#define          tcp_backlog_accepted(pcb)
#endif /* TCP_LISTEN_BACKLOG */
#define          tcp_accepted(pcb) do { LWIP_UNUSED_ARG(pcb); } while(0) /* compatibility define, not needed any more */

void             tcp_recved  (struct tcp_pcb *pcb, uint16_t len);
err_t            tcp_bind    (struct tcp_pcb *pcb, const LwipIpAddr *ipaddr,
                              uint16_t port);
void             tcp_bind_netif(struct tcp_pcb *pcb, const struct netif *netif);
err_t            tcp_connect (struct tcp_pcb *pcb, const LwipIpAddr *ipaddr,
                              uint16_t port, tcp_connected_fn connected);

struct tcp_pcb * tcp_listen_with_backlog_and_err(struct tcp_pcb *pcb, uint8_t backlog, err_t *err);
struct tcp_pcb * tcp_listen_with_backlog(struct tcp_pcb *pcb, uint8_t backlog);
/** @ingroup tcp_raw */
#define          tcp_listen(pcb) tcp_listen_with_backlog(pcb, TCP_DEFAULT_LISTEN_BACKLOG)

void             tcp_abort (struct tcp_pcb *pcb);
err_t            tcp_close   (struct tcp_pcb *pcb);
err_t            tcp_shutdown(struct tcp_pcb *pcb, int shut_rx, int shut_tx);

err_t            tcp_write   (struct tcp_pcb *pcb, const void *dataptr, uint16_t len,
                              uint8_t apiflags);

void             tcp_setprio (struct tcp_pcb *pcb, uint8_t prio);

err_t            tcp_output  (struct tcp_pcb *pcb);

err_t            tcp_tcp_get_tcp_addrinfo(struct tcp_pcb *pcb, int local, LwipIpAddr *addr, uint16_t *port);

#define tcp_dbg_get_tcp_state(pcb) ((pcb)->state)

/* for compatibility with older implementation */
#define tcp_new_ip6() tcp_new_ip_type(IPADDR_TYPE_V6)

#if LWIP_TCP_PCB_NUM_EXT_ARGS
uint8_t tcp_ext_arg_alloc_id(void);
void tcp_ext_arg_set_callbacks(struct tcp_pcb *pcb, uint8_t id, const struct tcp_ext_arg_callbacks * const callbacks);
void tcp_ext_arg_set(struct tcp_pcb *pcb, uint8_t id, void *arg);
void *tcp_ext_arg_get(const struct tcp_pcb *pcb, uint8_t id);
#endif

#ifdef __cplusplus
}
#endif

#endif /* LWIP_TCP */

#endif /* LWIP_HDR_TCP_H */
