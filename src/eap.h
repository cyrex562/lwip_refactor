/*
 * eap.h - Extensible Authentication Protocol for PPP (RFC 2284)
 *
 * Copyright (c) 2001 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Non-exclusive rights to redistribute, modify, translate, and use
 * this software in source and binary forms, in whole or in part, is
 * hereby granted, provided that the above copyright notice is
 * duplicated in any source form, and that neither the name of the
 * copyright holder nor the author is used to endorse or promote
 * products derived from this software.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Original version by James Carlson
 *
 * $Id: eap.h,v 1.2 2003/06/11 23:56:26 paulus Exp $
 */

#pragma once

#include "ppp_opts.h"
#include <cstdint>
#ifdef	__cplusplus
struct PppPcb;

extern "C" {
#endif

/*
 * Packet header = Code, id, length.
 */
#define	EAP_HEADERLEN	4


/* EAP message codes. */
#define	EAP_REQUEST	1
#define	EAP_RESPONSE	2
#define	EAP_SUCCESS	3
#define	EAP_FAILURE	4

/* EAP types */
#define	EAPT_IDENTITY		1
#define	EAPT_NOTIFICATION	2
#define	EAPT_NAK		3	/* (response only) */
#define	EAPT_MD5CHAP		4
#define	EAPT_OTP		5	/* One-Time Password; RFC 1938 */
#define	EAPT_TOKEN		6	/* Generic Token Card */
/* 7 and 8 are unassigned. */
#define	EAPT_RSA		9	/* RSA Public Key Authentication */
#define	EAPT_DSS		10	/* DSS Unilateral */
#define	EAPT_KEA		11	/* KEA */
#define	EAPT_KEA_VALIDATE	12	/* KEA-VALIDATE	*/
#define	EAPT_TLS		13	/* EAP-TLS */
#define	EAPT_DEFENDER		14	/* Defender Token (AXENT) */
#define	EAPT_W2K		15	/* Windows 2000 EAP */
#define	EAPT_ARCOT		16	/* Arcot Systems */
#define	EAPT_CISCOWIRELESS	17	/* Cisco Wireless */
#define	EAPT_NOKIACARD		18	/* Nokia IP smart card */
#define	EAPT_SRP		19	/* Secure Remote Password */
/* 20 is deprecated */

/* EAP SRP-SHA1 Subtypes */
#define	EAPSRP_CHALLENGE	1	/* Request 1 - Challenge */
#define	EAPSRP_CKEY		1	/* Response 1 - Client Key */
#define	EAPSRP_SKEY		2	/* Request 2 - Server Key */
#define	EAPSRP_CVALIDATOR	2	/* Response 2 - Client Validator */
#define	EAPSRP_SVALIDATOR	3	/* Request 3 - Server Validator */
#define	EAPSRP_ACK		3	/* Response 3 - final ack */
#define	EAPSRP_LWRECHALLENGE	4	/* Req/resp 4 - Lightweight rechal */

#define	SRPVAL_EBIT	0x00000001	/* Use shared key for ECP */

#define	SRP_PSEUDO_ID	"pseudo_"
#define	SRP_PSEUDO_LEN	7

#define MD5_SIGNATURE_SIZE	16
#define EAP_MIN_CHALLENGE_LENGTH	17
#define EAP_MAX_CHALLENGE_LENGTH	24
#define EAP_MIN_MAX_POWER_OF_TWO_CHALLENGE_LENGTH     3   /* 2^3-1 = 7, 17+7 = 24 */

#define	EAP_STATES	\
	"Initial", "Pending", "Closed", "Listen", "Identify", \
	"SRP1", "SRP2", "SRP3", "MD5Chall", "Open", "SRP4", "BadAuth"


#if PPP_SERVER
#define	eap_server_active(pcb)	\
	((pcb)->eap.es_server.ea_state >= eapIdentify && \
	 (pcb)->eap.es_server.ea_state <= eapMD5Chall)
#endif /* PPP_SERVER */

/*
 * Complete EAP state for one PPP session.
 */
enum eap_state_code {
	eapInitial = 0,	/* No EAP authentication yet requested */
	eapPending,	/* Waiting for LCP (no timer) */
	eapClosed,	/* Authentication not in use */
	kEapListen,	/* Client ready (and timer running) */
	eapIdentify,	/* EAP Identify sent */
	eapSRP1,	/* Sent EAP SRP-SHA1 Subtype 1 */
	eapSRP2,	/* Sent EAP SRP-SHA1 Subtype 2 */
	eapSRP3,	/* Sent EAP SRP-SHA1 Subtype 3 */
	eapMD5Chall,	/* Sent MD5-Challenge */
	eapOpen,	/* Completed authentication */
	eapSRP4,	/* Sent EAP SRP-SHA1 Subtype 4 */
	eapBadAuth	/* Failed authentication */
};

struct eap_auth {
	const char *ea_name;	/* Our name */
	char ea_peer[MAXNAMELEN +1];	/* Peer's name */
	void *ea_session;	/* Authentication library linkage */
	uint8_t *ea_skey;	/* Shared encryption key */
	uint16_t ea_namelen;	/* Length of our name */
	uint16_t ea_peerlen;	/* Length of peer's name */
	enum eap_state_code ea_state;
	uint8_t ea_id;		/* Current id */
	uint8_t ea_requests;	/* Number of Requests sent/received */
	uint8_t ea_responses;	/* Number of Responses */
	uint8_t ea_type;		/* One of EAPT_* */
	uint32_t ea_keyflags;	/* SRP shared key usage flags */
};

#define EAP_MAX_CHALLENGE_LENGTH	24

struct EapState
{
    struct eap_auth es_client; /* Client (authenticatee) data */
    struct eap_auth es_server; /* Server (authenticator) data */
    int es_savedtime; /* Saved timeout */
    int es_rechallenge; /* EAP rechallenge interval */
    int es_lwrechallenge; /* SRP lightweight rechallenge inter */
    uint8_t es_usepseudo; /* Use SRP Pseudonym if offered one */
    int es_usedpseudo; /* Set if we already sent PN */
    int es_challen; /* Length of challenge string */
    uint8_t es_challenge[EAP_MAX_CHALLENGE_LENGTH];
};

inline bool eap_client_active(EapState* eap)
{
    return (eap->es_client.ea_state == kEapListen);
}

void eap_authwithpeer(PppPcb *pcb);
void eap_authpeer(PppPcb *pcb, const char *localname);

extern const struct Protent eap_protent;

#ifdef	__cplusplus
}
#endif
