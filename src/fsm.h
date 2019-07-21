/*
 * fsm.h - {Link, IP} Control Protocol Finite State Machine definitions.
 *
 * Copyright (c) 1984-2000 Carnegie Mellon University. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any legal
 *    details, please contact
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: fsm.h,v 1.10 2004/11/13 02:28:15 paulus Exp $
 */
#pragma once
#include <protent.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PppPcb;

/*
 * Packet header = Code, id, length.
 */
constexpr auto kHeaderlen = 4;


/*
 *  CP (LCP, IPCP, etc.) codes.
 */
enum CpCodes
{
    CONFREQ =1,
    /* Configuration Request */
    CONFACK = 2,
    /* Configuration Ack */
    CONFNAK = 3,
    /* Configuration Nak */
    CONFREJ = 4,
    /* Configuration Reject */
    TERMREQ = 5,
    /* Termination Request */
    TERMACK = 6,
    /* Termination Ack */
    CODEREJ = 7,
    /* Code Reject */
};



/*
 * Each FSM is described by an fsm structure and fsm callbacks.
 */
struct Fsm
{
    PppPcb* pcb; /* PPP Interface */
    const struct FsmCallbacks* callbacks; /* Callback routines */
    const char* term_reason; /* Reason for closing protocol */
    uint8_t seen_ack; /* Have received valid Ack/Nak/Rej to Req */
    /* -- This is our only flag, we might use u_int :1 if we have more flags */
    uint16_t protocol; /* Data Link Layer Protocol field value */
    uint8_t state; /* State */
    uint8_t flags; /* Contains option bits */
    uint8_t id; /* Current id */
    uint8_t reqid; /* Current request id */
    uint8_t retransmits; /* Number of retransmissions left */
    uint8_t nakloops; /* Number of nak loops since last ack */
    uint8_t rnakloops; /* Number of naks received */
    uint8_t maxnakloops; /* Maximum number of nak loops tolerated
				   (necessary because IPCP require a custom large max nak loops value) */
    uint8_t term_reason_len; /* Length of term_reason */
    int unit;
};


struct FsmCallbacks {
    /* Reset our Configuration Information */
    void (*resetci)(Fsm *, PppPcb*);
    /* Length of our Configuration Information */
    size_t  (*cilen)	 (PppPcb*);
    /* Add our Configuration Information */
    void (*addci) (Fsm *, uint8_t *, int *, PppPcb*);
    /* ACK our Configuration Information */
    int  (*ackci) (Fsm *, uint8_t *, int, PppPcb*);
    /* NAK our Configuration Information */
    int (*nakci)(Fsm*, const uint8_t*, int, int, PppPcb*);
    /* Reject our Configuration Information */
    int (*rejci)(Fsm*, const uint8_t*, int, PppPcb*);
    /* Request peer's Configuration Information */
    int  (*reqci)	 (Fsm *, uint8_t *, size_t *, int, PppPcb*);
    /* Called when fsm reaches PPP_FSM_OPENED state */
    void (*up) (Fsm *, PppPcb*);
    /* Called when fsm leaves PPP_FSM_OPENED state */
    void (*down) (Fsm *, Fsm*, PppPcb*);
    /* Called when we want the lower layer */
    void (*starting)	 (Fsm *);
    /* Called when we don't want the lower layer */
    void (*finished) (Fsm *);
    /* Called when Protocol-Reject received */
    void (*protreject)	 (int);
    /* Retransmission is necessary */
    void (*retransmit)	 (Fsm *);
    /* Called when unknown code received */
    int  (*extcode)	 (Fsm *, int, int, uint8_t *, int, PppPcb*);
    const char *proto_name;	/* String name for protocol (for messages) */
} ;


/*
 * Link states.
 */
#define PPP_FSM_INITIAL		0	/* Down, hasn't been opened */
#define PPP_FSM_STARTING	1	/* Down, been opened */
#define PPP_FSM_CLOSED		2	/* Up, hasn't been opened */
#define PPP_FSM_STOPPED		3	/* Open, waiting for down event */
#define PPP_FSM_CLOSING		4	/* Terminating the connection, not open */
#define PPP_FSM_STOPPING	5	/* Terminating, but open */
#define PPP_FSM_REQSENT		6	/* We've sent a Config Request */
#define PPP_FSM_ACKRCVD		7	/* We've received a Config Ack */
#define PPP_FSM_ACKSENT		8	/* We've sent a Config Ack */
#define PPP_FSM_OPENED		9	/* Connection available */


/*
 * Flags - indicate options controlling FSM operation
 */
#define OPT_PASSIVE	1	/* Don't die if we don't get a response */
#define OPT_RESTART	2	/* Treat 2nd OPEN as DOWN, UP */
#define OPT_SILENT	4	/* Wait for peer to speak first */

/*
 * Prototypes
 */
void fsm_init(Fsm* f);
void fsm_lowerup(Fsm* f);
void fsm_lowerdown(Fsm* f);
bool fsm_open(Fsm* f);
void fsm_close(Fsm* f, const char* reason);
void fsm_input(Fsm* f, uint8_t* inpacket, int l);
void fsm_protreject(Fsm* f);
void fsm_sdata(Fsm* f, uint8_t code, uint8_t id, const uint8_t* data, int datalen);

#ifdef __cplusplus
}
#endif
