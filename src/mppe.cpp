/*
 * mppe.c - interface MPPE to the PPP code.
 *
 * By Frank Cusack <fcusack@fcusack.com>.
 * Copyright (c) 2002,2003,2004 Google, Inc.
 * All rights reserved.
 *
 * License:
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, provided that the above copyright
 * notice appears in all copies.  This software is provided without any
 * warranty, express or implied.
 *
 * Changelog:
 *      08/12/05 - Matt Domsch <Matt_Domsch@dell.com>
 *                 Only need extra skb padding on transmit, not receive.
 *      06/18/04 - Matt Domsch <Matt_Domsch@dell.com>, Oleg Makarenko <mole@quadra.ru>
 *                 Use Linux kernel 2.6 arc4 and sha1 routines rather than
 *                 providing our own.
 *      2/15/04 - TS: added #include <version.h> and testing for Kernel
 *                    version before using
 *                    MOD_DEC_USAGE_COUNT/MOD_INC_USAGE_COUNT which are
 *                    deprecated in 2.6
 */

#include <ppp_opts.h>
#include <cstring>
#include <lwip_status.h>
#include <ccp.h>
#include <mppe.h>
#include <pppdebug.h>
#include <pppcrypt.h>
#include <lcp.h>
#include "netbuf.h"
constexpr auto SHA1_SIGNATURE_SIZE = 20;

/* ppp_mppe_state.bits definitions */
enum MppeStateBits : uint8_t
{
    MPPE_BIT_A =0x80,
    /* Encryption table were (re)inititalized */
    MPPE_BIT_B =0x40,
    /* MPPC only (not implemented) */
    MPPE_BIT_C =0x20,
    /* MPPC only (not implemented) */
    MPPE_BIT_D =0x10,
    /* This is an encrypted frame */
    MPPE_BIT_FLUSHED =MPPE_BIT_A,
    MPPE_BIT_ENCRYPTED =MPPE_BIT_D,
};

inline uint8_t MPPE_BITS(uint8_t* p)
{
    return ((p)[0] & 0xf0);
}

inline uint8_t MPPE_CCOUNT(uint8_t* p)
{
    return ((((p)[0] & 0x0f) << 8) + (p)[1]);
}

constexpr auto MPPE_CCOUNT_SPACE =0x1000;	/* The size of the ccount space */

constexpr auto MPPE_OVHD=2;	/* MPPE overhead/packet */
#define SANITY_MAX	1600	/* Max bogon factor we will tolerate */

/*
 * Perform the MPPE rekey algorithm, from RFC 3078, sec. 7.3.
 * Well, not what's written there, but rather what they meant.
 */
static void mppe_rekey(PppMppeState * state, int initial_key)
{
	lwip_sha1_context sha1_ctx;
	uint8_t sha1_digest[SHA1_SIGNATURE_SIZE];

	/*
	 * Key Derivation, from RFC 3078, RFC 3079.
	 * Equivalent to Get_Key() for MS-CHAP as described in RFC 3079.
	 */
	lwip_sha1_init(&sha1_ctx);
	lwip_sha1_starts(&sha1_ctx);
	lwip_sha1_update(&sha1_ctx, state->master_key, state->keylen);
	lwip_sha1_update(&sha1_ctx, MPPE_SHA1_PAD1, SHA1_PAD_SIZE);
	lwip_sha1_update(&sha1_ctx, state->session_key, state->keylen);
	lwip_sha1_update(&sha1_ctx, MPPE_SHA1_PAD2, SHA1_PAD_SIZE);
	lwip_sha1_finish(&sha1_ctx, sha1_digest);
	lwip_sha1_free(&sha1_ctx);
	memcpy(state->session_key, sha1_digest, state->keylen);

	if (!initial_key) {
		lwip_arc4_init(&state->arc4);
		lwip_arc4_setup(&state->arc4, sha1_digest, state->keylen);
		lwip_arc4_crypt(&state->arc4, state->session_key, state->keylen);
		lwip_arc4_free(&state->arc4);
	}
	if (state->keylen == 8) {
		/* See RFC 3078 */
		state->session_key[0] = 0xd1;
		state->session_key[1] = 0x26;
		state->session_key[2] = 0x9e;
	}
	lwip_arc4_init(&state->arc4);
	lwip_arc4_setup(&state->arc4, state->session_key, state->keylen);
}

/*
 * Set key, used by MSCHAP before mppe_init() is actually called by CCP so we
 * don't have to keep multiple copies of keys.
 */
void mppe_set_key(PppPcb *pcb, PppMppeState *state, uint8_t *key) {
    memcpy(state->master_key, key, MPPE_MAX_KEY_LEN);
}

/*
 * Initialize (de)compressor state.
 */
LwipStatus
mppe_init(PppPcb& pcb, PppMppeState& state, uint8_t options)
{

	const uint8_t *debugstr = (const uint8_t*)"mppe_comp_init";
	if (pcb.mppe_decomp == state) {
	    debugstr = (const uint8_t*)"mppe_decomp_init";
	}


	/* Save keys. */
	memcpy(state.session_key, state.master_key, sizeof(state.master_key));

	if (options & MPPE_OPT_128)
    {
        state.keylen = 16;
    }
    else if (options & MPPE_OPT_40)
    {
        state.keylen = 8;
    }
    else {
		// PPPDEBUG(LOG_DEBUG, ("%s[%d]: unknown key length\n", debugstr,
		// 	pcb->netif->num));
		lcp_close(pcb, "MPPE required but peer negotiation failed");
		return;
	}
	if (options & MPPE_OPT_STATEFUL)
    {
        state.stateful = 1;
    } /* Generate the initial session key. */
	mppe_rekey(state, 1);


	{
		int i;
		char mkey[sizeof(state.master_key) * 2 + 1];
		char skey[sizeof(state.session_key) * 2 + 1];

		// PPPDEBUG(LOG_DEBUG, ("%s[%d]: initialized with %d-bit %s mode\n",
		//        debugstr, pcb->netif->num, (state->keylen == 16) ? 128 : 40,
		//        (state->stateful) ? "stateful" : "stateless"));

		for (i = 0; i < (int)sizeof(state.master_key); i++)
			sprintf(mkey + i * 2, "%02x", state.master_key[i]);
		for (i = 0; i < (int)sizeof(state.session_key); i++)
        {
            sprintf(skey + i * 2, "%02x", state.session_key[i]);
        } // PPPDEBUG(LOG_DEBUG,
		//        ("%s[%d]: keys: master: %s initial session: %s\n",
		//        debugstr, pcb->netif->num, mkey, skey));
	}


	/*
	 * Initialize the coherency count.  The initial value is not specified
	 * in RFC 3078, but we can make a reasonable assumption that it will
	 * start at 0.  Setting it to the max here makes the comp/decomp code
	 * do the right thing (determined through experiment).
	 */
	state.ccount = MPPE_CCOUNT_SPACE - 1;

	/*
	 * Note that even though we have initialized the key table, we don't
	 * set the FLUSHED bit.  This is contrary to RFC 3078, sec. 3.1.
	 */
	state.bits = MPPE_BIT_ENCRYPTED;
}

/*
 * We received a CCP Reset-Request (actually, we are sending a Reset-Ack),
 * tell the compressor to rekey.  Note that we MUST NOT rekey for
 * every CCP Reset-Request; we only rekey on the next xmit packet.
 * We might get multiple CCP Reset-Requests if our CCP Reset-Ack is lost.
 * So, rekeying for every CCP Reset-Request is broken as the peer will not
 * know how many times we've rekeyed.  (If we rekey and THEN get another
 * CCP Reset-Request, we must rekey again.)
 */
void mppe_comp_reset(PppPcb *pcb, PppMppeState *state)
{
    state->bits |= MPPE_BIT_FLUSHED;
}

/*
 * Compress (encrypt) a packet.
 * It's strange to call this a compressor, since the output is always
 * MPPE_OVHD + 2 bytes larger than the input.
 */
LwipStatus
mppe_compress(PppPcb& pcb, PppMppeState& state, PacketBuffer& pb, uint16_t protocol)
{
    LwipStatus err; /* TCP stack requires that we don't change the packet payload, therefore we copy
	 * the whole packet before encryption.
	 */
	// struct PacketBuffer* np = pbuf_alloc();
    PacketBuffer np{};
	if (!np) {
		return ERR_MEM;
	}

	/* Hide MPPE header + protocol */
	// pbuf_remove_header(np, MPPE_OVHD + sizeof(protocol));

	if ((err = copy_pkt_buf(np, *pb)) != STATUS_SUCCESS) {
		free_pkt_buf(np);
		return err;
	}

	/* Reveal MPPE header + protocol */
	// pbuf_add_header(np, MPPE_OVHD + sizeof(protocol));

	*pb = np;
	uint8_t* pl = (uint8_t*)np->payload;

	state->ccount = (state->ccount + 1) % MPPE_CCOUNT_SPACE;
	// PPPDEBUG(LOG_DEBUG, ("mppe_compress[%d]: ccount %d\n", pcb->netif->num, state->ccount));
	/* FIXME: use PUT* macros */
	pl[0] = state->ccount>>8;
	pl[1] = state->ccount;

	if (!state->stateful ||	/* stateless mode     */
	    ((state->ccount & 0xff) == 0xff) ||	/* "flag" packet      */
	    (state->bits & MPPE_BIT_FLUSHED)) {	/* CCP Reset-Request  */
		/* We must rekey */
		if (state->stateful) {
			// PPPDEBUG(LOG_DEBUG, ("mppe_compress[%d]: rekeying\n", pcb->netif->num));
		}
		mppe_rekey(state, 0);
		state->bits |= MPPE_BIT_FLUSHED;
	}
	pl[0] |= state->bits;
	state->bits &= ~MPPE_BIT_FLUSHED;	/* reset for next xmit */
	pl += MPPE_OVHD;

	/* Add protocol */
	/* FIXME: add PFC support */
	pl[0] = protocol >> 8;
	pl[1] = protocol;

	/* Hide MPPE header */
	// pbuf_remove_header(np, MPPE_OVHD);

	/* Encrypt packet */
	for (struct PacketBuffer* n = np; n != nullptr; n = n->next) {
		lwip_arc4_crypt(&state->arc4, (uint8_t*)n->payload, n->len);
		if (n->tot_len == n->len) {
			break;
		}
	}

	/* Reveal MPPE header */
	// pbuf_add_header(np, MPPE_OVHD);

	return STATUS_SUCCESS;
}

/*
 * We received a CCP Reset-Ack.  Just ignore it.
 */
void mppe_decomp_reset(PppPcb *pcb, PppMppeState *state)
{
}

/*
 * Decompress (decrypt) an MPPE packet.
 */
LwipStatus
mppe_decompress(PppPcb& ppp_pcb, PppMppeState& ppp_mppe_state, void* pkt_buf)
{
	struct PacketBuffer *n0 = *pkt_buf; /* MPPE Header */
	if (n0->len < MPPE_OVHD) {
		// PPPDEBUG(LOG_DEBUG,
		//        ("mppe_decompress[%d]: short pkt (%d)\n",
		//        pcb->netif->num, n0->len));
		ppp_mppe_state->sanity_errors += 100;
		goto sanity_error;
	}

	uint8_t* pl = (uint8_t*)n0->payload;
	uint8_t flushed = MPPE_BITS(pl) & MPPE_BIT_FLUSHED;
	uint16_t ccount = MPPE_CCOUNT(pl);
	// PPPDEBUG(LOG_DEBUG, ("mppe_decompress[%d]: ccount %d\n",
	//        pcb->netif->num, ccount));

	/* sanity checks -- terminate with extreme prejudice */
	if (!(MPPE_BITS(pl) & MPPE_BIT_ENCRYPTED)) {
		// PPPDEBUG(LOG_DEBUG,
		//        ("mppe_decompress[%d]: ENCRYPTED bit not set!\n",
		//        pcb->netif->num));
		ppp_mppe_state->sanity_errors += 100;
		goto sanity_error;
	}
	if (!ppp_mppe_state->stateful && !flushed) {
		// PPPDEBUG(LOG_DEBUG, ("mppe_decompress[%d]: FLUSHED bit not set in "
		//        "stateless mode!\n", pcb->netif->num));
		ppp_mppe_state->sanity_errors += 100;
		goto sanity_error;
	}
	if (ppp_mppe_state->stateful && ((ccount & 0xff) == 0xff) && !flushed) {
		// PPPDEBUG(LOG_DEBUG, ("mppe_decompress[%d]: FLUSHED bit not set on "
		//        "flag packet!\n", pcb->netif->num));
		ppp_mppe_state->sanity_errors += 100;
		goto sanity_error;
	}

	/*
	 * Check the coherency count.
	 */

	if (!ppp_mppe_state->stateful) {
		/* Discard late packet */
		if ((ccount - ppp_mppe_state->ccount) % MPPE_CCOUNT_SPACE > MPPE_CCOUNT_SPACE / 2) {
			ppp_mppe_state->sanity_errors++;
			goto sanity_error;
		}

		/* RFC 3078, sec 8.1.  Rekey for every packet. */
		while (ppp_mppe_state->ccount != ccount) {
			mppe_rekey(ppp_mppe_state, 0);
			ppp_mppe_state->ccount = (ppp_mppe_state->ccount + 1) % MPPE_CCOUNT_SPACE;
		}
	} else {
		/* RFC 3078, sec 8.2. */
		if (!ppp_mppe_state->discard) {
			/* normal state */
			ppp_mppe_state->ccount = (ppp_mppe_state->ccount + 1) % MPPE_CCOUNT_SPACE;
			if (ccount != ppp_mppe_state->ccount) {
				/*
				 * (ccount > state->ccount)
				 * Packet loss detected, enter the discard state.
				 * Signal the peer to rekey (by sending a CCP Reset-Request).
				 */
				ppp_mppe_state->discard = 1;
				ccp_resetrequest(&ppp_pcb->ccp_localstate);
				return ERR_BUF;
			}
		} else {
			/* discard state */
			if (!flushed) {
				/* ccp.c will be silent (no additional CCP Reset-Requests). */
				return ERR_BUF;
			} else {
				/* Rekey for every missed "flag" packet. */
				while ((ccount & ~0xff) !=
				       (ppp_mppe_state->ccount & ~0xff)) {
					mppe_rekey(ppp_mppe_state, 0);
					ppp_mppe_state->ccount =
					    (ppp_mppe_state->ccount +
					     256) % MPPE_CCOUNT_SPACE;
				}

				/* reset */
				ppp_mppe_state->discard = 0;
				ppp_mppe_state->ccount = ccount;
				/*
				 * Another problem with RFC 3078 here.  It implies that the
				 * peer need not send a Reset-Ack packet.  But RFC 1962
				 * requires it.  Hopefully, M$ does send a Reset-Ack; even
				 * though it isn't required for MPPE synchronization, it is
				 * required to reset CCP state.
				 */
			}
		}
		if (flushed)
        {
            mppe_rekey(ppp_mppe_state, 0);
        }
    }

	/* Hide MPPE header */
	// pbuf_remove_header(n0, MPPE_OVHD);

	/* Decrypt the packet. */
	for (struct PacketBuffer* n = n0; n != nullptr; n = n->next) {
		lwip_arc4_crypt(&ppp_mppe_state->arc4, (uint8_t*)n->payload, n->len);
		if (n->tot_len == n->len) {
			break;
		}
	}

	/* good packet credit */
	ppp_mppe_state->sanity_errors >>= 1;

	return STATUS_SUCCESS;

sanity_error:
	if (ppp_mppe_state->sanity_errors >= SANITY_MAX) {
		/*
		 * Take LCP down if the peer is sending too many bogons.
		 * We don't want to do this for a single or just a few
		 * instances since it could just be due to packet corruption.
		 */
		lcp_close(ppp_pcb, "Too many MPPE errors");
	}
	return ERR_BUF;
}

