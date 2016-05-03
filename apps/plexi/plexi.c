/*
 * Copyright (c) 2015, Technische Universiteit Eindhoven.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * \file
 *         plexi is a CoAP interface of IEEE802.15.4 PHY, MAC (incl. TSCH) and RPL resources.
 *         Link quality metrics (ETX, RSSI, LQI), schedule properties (ASN, slotFrames and cells), DoDAG structure (parents, children) are monitored, observed or modified.
 *         (refer to "plexi: Adaptive re-scheduling web service of time synchronized low-power wireless networks¨, JNCA, Elsevier)
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 * \author Ilker Oztelcan <i.oztelcan@tue.nl>
 * \author Dimitris Sarakiotis <d.sarakiotis@tue.nl>
 *
 */

#include "er-coap-engine.h" /* needed for rest-init-engine */

#include "plexi.h"
#include "plexi-conf.h"
#include "plexi-interface.h"

#include <stdlib.h>
#include <errno.h>

/* activate RPL-related module of plexi only when needed */
#if PLEXI_WITH_RPL_DAG_RESOURCE
#include "plexi-rpl.h"
#endif

/* activate neighbor list monitoring module of plexi only when needed */
#if PLEXI_WITH_NEIGHBOR_RESOURCE
#include "plexi-neighbors.h"
#endif

/* activate TSCH-related module of plexi only when needed */
#if PLEXI_WITH_TSCH_RESOURCE
#include "plexi-tsch.h"
#endif

/* activate link quality monitoring module of plexi only when needed */
#if PLEXI_WITH_LINK_STATISTICS
#include "plexi-link-statistics.h"
#endif

/* activate queue monitoring module of plexi only when needed */
#if PLEXI_WITH_QUEUE_STATISTICS
#include "plexi-queue-statistics.h"
#endif

int plexi_reply_content_len = 0;

void
plexi_init()
{
  printf("PLEXI: initializing scheduler interface\n");

  /* Initialize CoAP service */
  rest_init_engine();

#if PLEXI_WITH_RPL_DAG_RESOURCE
  plexi_rpl_init(); /* initialize plexi-rpl module */
#endif

#if PLEXI_WITH_NEIGHBOR_RESOURCE
  plexi_neighbors_init(); /* initialize plexi-neighbors module */
#endif

#if PLEXI_WITH_TSCH_RESOURCE
  plexi_tsch_init(); /* initialize plexi-tsch module */
#endif

#if PLEXI_WITH_LINK_STATISTICS
  //plexi_link_statistics_init(); /* initialize plexi-link-statistics module */
#endif

#if PLEXI_WITH_QUEUE_STATISTICS
  //plexi_queue_statistics_init(); /* initialize plexi-queue-statistics module */
#endif
}
/* Utility function for json parsing */
int
plexi_json_find_field(struct jsonparse_state *js, char *field_buf, int field_buf_len)
{
  int state = jsonparse_next(js);
  while(state) {
    switch(state) {
    case JSON_TYPE_PAIR_NAME:
      jsonparse_copy_value(js, field_buf, field_buf_len);
      /* Move to ":" */
      jsonparse_next(js);
      /* Move to value and return its type */
      return jsonparse_next(js);
    default:
      return state;
    }
    state = jsonparse_next(js);
  }
  return 0;
}
/* Utility function. Converts na field (string containing the lower 64bit of the IPv6) to
 * 64-bit MAC. */
int
plexi_eui64_to_linkaddr(const char *na_inbuf, int bufsize, linkaddr_t *linkaddress)
{
  int i;
  char next_end_char = ':';
  const char *na_inbuf_end = na_inbuf + bufsize - 1;
  char *end;
  unsigned val;
  for(i = 0; i < 4; i++) {
    if(na_inbuf >= na_inbuf_end) {
      return 0;
    }
    if(i == 3) {
      next_end_char = '\0';
    }
    val = (unsigned)strtoul(na_inbuf, &end, 16);
    /* Check conversion */
    if(end != na_inbuf && *end == next_end_char && errno != ERANGE) {
      linkaddress->u8[2 * i] = val >> 8;
      linkaddress->u8[2 * i + 1] = val;
      na_inbuf = end + 1;
    } else {
      return 0;
    }
  }
  /* We consider only links with IEEE EUI-64 identifier */
  linkaddress->u8[0] ^= 0x02;
  return 1;
}
int
plexi_linkaddr_to_eui64(char *buf, linkaddr_t *addr)
{
  char *pointer = buf;
  unsigned int i;
  for(i = 0; i < sizeof(linkaddr_t); i++) {
    if(i > 1 && i != 3 && i != 4 && i != 7) {
      *pointer = ':';
      pointer++;
    }
    if(i == 4) {
      continue;
    }
    if(i == 0) {
      sprintf(pointer, "%x", addr->u8[i] ^ 0x02);
      pointer++;
    } else {
      sprintf(pointer, "%02x", addr->u8[i]);
      pointer += 2;
    }
  }
  return strlen(buf);
}