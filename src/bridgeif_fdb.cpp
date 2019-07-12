/**
 * @file
 * lwIP netif implementing an FDB for IEEE 802.1D MAC Bridge
 */

/*
 * Copyright (c) 2017 Simon Goldschmidt.
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
 * Author: Simon Goldschmidt <goldsimon@gmx.de>
 *
 */

/**
 * @defgroup bridgeif_fdb FDB example code
 * @ingroup bridgeif
 * This file implements an example for an FDB (Forwarding DataBase)
 */

#include "bridgeif.h"
#include "lwip_debug.h"
#include "sys.h"
#include "timeouts.h"
#include <cstring>

constexpr auto kBridgeifAgeTimerMs = 1000; 
constexpr auto kBrFdbTimeoutSec = (60*5) /* 5 minutes FDB timeout */;

struct BridgeIfDfDbEntry {
  uint8_t used;
  uint8_t port;
  uint32_t ts;
  struct EthAddr addr;
} ;

struct BridgeIfDfDb {
  uint16_t max_fdb_entries;
  BridgeIfDfDbEntry *fdb;
} ;

/**
 * @ingroup bridgeif_fdb
 * A real simple and slow implementation of an auto-learning forwarding database that
 * remembers known src mac addresses to know which port to send frames destined for that
 * mac address.
 *
 * ATTENTION: This is meant as an example only, in real-world use, you should 
 * provide a better implementation :-)
 */
void bridgeif_fdb_update_src(void* fdb_ptr, struct EthAddr* src_addr, uint8_t port_idx)
{
    int i;
    auto fdb = static_cast<BridgeIfDfDb *>(fdb_ptr);
    for (i = 0; i < fdb->max_fdb_entries; i++)
    {
        auto e = &fdb->fdb[i];
        if (e->used && e->ts)
        {
            if (!memcmp(&e->addr, src_addr, sizeof(struct EthAddr)))
            {
                // LWIP_DEBUGF(BRIDGEIF_FDB_DEBUG,
                //             ("br: update src %02x:%02x:%02x:%02x:%02x:%02x (from %d) @ idx %d\n"
                //                 , src_addr->addr[0], src_addr->addr[1], src_addr->addr[2],
                //                 src_addr->addr[3], src_addr->addr[4], src_addr->addr[5],
                //                 port_idx, i));
                e->ts = kBrFdbTimeoutSec;
                e->port = port_idx;
                return;
            }
        }
    } /* not found, allocate new entry from free */
    for (i = 0; i < fdb->max_fdb_entries; i++)
    {
        auto e = &fdb->fdb[i];
        if (!e->used || !e->ts)
        {
            if (!e->used || !e->ts)
            {
                // LWIP_DEBUGF(BRIDGEIF_FDB_DEBUG,
                //             ("br: create src %02x:%02x:%02x:%02x:%02x:%02x (from %d) @ idx %d\n"
                //                 , src_addr->addr[0], src_addr->addr[1], src_addr->addr[2],
                //                 src_addr->addr[3], src_addr->addr[4], src_addr->addr[5],
                //                 port_idx, i));
                memcpy(&e->addr, src_addr, sizeof(struct EthAddr));
                e->ts = kBrFdbTimeoutSec;
                e->port = port_idx;
                e->used = 1;
                return;
            }
        }
    }
}

/** 
 * @ingroup bridgeif_fdb
 * Walk our list of auto-learnt fdb entries and return a port to forward or BR_FLOOD if unknown 
 */
bridgeif_portmask_t
bridgeif_fdb_get_dst_ports(void *fdb_ptr, struct EthAddr *dst_addr)
{
    auto fdb = static_cast<BridgeIfDfDb *>(fdb_ptr);
  for (auto i = 0; i < fdb->max_fdb_entries; i++) {
      auto e = &fdb->fdb[i];
    if (e->used && e->ts) {
      if (!memcmp(&e->addr, dst_addr, sizeof(struct EthAddr))) {
          auto ret = (bridgeif_portmask_t)(1 << e->port);
        return ret;
      }
    }
  }

  return kBrFlood;
}

/**
 * @ingroup bridgeif_fdb
 * Aging implementation of our simple fdb
 */
static void
bridgeif_fdb_age_one_second(void *fdb_ptr)
{
  int i;
  BridgeIfDfDb *fdb;
  BRIDGEIF_DECL_PROTECT(lev);

  fdb = (BridgeIfDfDb *)fdb_ptr;
  BRIDGEIF_READ_PROTECT(lev);

  for (i = 0; i < fdb->max_fdb_entries; i++) {
    BridgeIfDfDbEntry *e = &fdb->fdb[i];
    if (e->used && e->ts) {
      BRIDGEIF_WRITE_PROTECT(lev);
      /* check again when protected */
      if (e->used && e->ts) {
        if (--e->ts == 0) {
          e->used = 0;
        }
      }
      BRIDGEIF_WRITE_UNPROTECT(lev);
    }
  }
  BRIDGEIF_READ_UNPROTECT(lev);
}

/** Timer callback for fdb aging, called once per second */
static void
bridgeif_age_tmr(void *arg)
{
    const auto fdb = static_cast<BridgeIfDfDb *>(arg);

  LWIP_ASSERT("invalid arg", arg != nullptr);

  bridgeif_fdb_age_one_second(fdb);
  sys_timeout(kBridgeifAgeTimerMs, bridgeif_age_tmr, arg);
}

/**
 * @ingroup bridgeif_fdb
 * Init our simple fdb list
 */
void *
bridgeif_fdb_init(const uint16_t max_fdb_entries)
{
    auto alloc_len_sizet = sizeof(BridgeIfDfDb) + (max_fdb_entries * sizeof(BridgeIfDfDbEntry));
    auto alloc_len = size_t(alloc_len_sizet);
  LWIP_ASSERT("alloc_len == alloc_len_sizet", alloc_len == alloc_len_sizet);
  Logf(BRIDGEIF_DEBUG, "bridgeif_fdb_init: allocating %d bytes for private FDB data\n", int(alloc_len));
    const auto fdb = new BridgeIfDfDb;
  if (fdb == nullptr) {
    return nullptr;
  }
  fdb->max_fdb_entries = max_fdb_entries;
  fdb->fdb = reinterpret_cast<BridgeIfDfDbEntry *>(fdb + 1);

  sys_timeout(kBridgeifAgeTimerMs, bridgeif_age_tmr, fdb);

  return fdb;
}
