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

#include "net/ip/ip64-addr.h"
#include "er-coap-engine.h" /* needed for rest-init-engine */

#include "plexi.h"
#include "plexi-interface.h"
#include "../rest-engine/rest-engine.h"

#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <inttypes.h>
#include "net/rime/rime.h"
#include "net/mac/tsch/tsch-schedule.h"
#include "net/packetbuf.h"

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

void plexi_packet_received(void);
void plexi_packet_sent(int mac_status);

RIME_SNIFFER(plexi_sniffer, plexi_packet_received, plexi_packet_sent);


/* activate ID-related module of plexi only when needed */
#if PLEXI_WITH_ID_RESOURCE
extern resource_t resource_ids;
#endif

/* activate RPL-related module of plexi only when needed */
#if PLEXI_WITH_RPL_DAG_RESOURCE
extern resource_t resource_rpl_dag;
extern void plexi_rpl_packet_received(void);
extern void plexi_rpl_init();
#endif

/* activate neighbor list monitoring module of plexi only when needed */
#if PLEXI_WITH_NEIGHBOR_RESOURCE
extern resource_t resource_6top_nbrs;
#endif


/* activate slotframe resource module of plexi only when needed */
#if PLEXI_WITH_SLOTFRAME_RESOURCE
extern resource_t resource_6top_slotframe;
#endif

/* activate TSCH-related module of plexi only when needed */
#if PLEXI_WITH_LINK_RESOURCE
extern resource_t resource_6top_links;
#endif

/* activate link quality monitoring module of plexi only when needed */
#if PLEXI_WITH_LINK_STATISTICS
#include "plexi-link-statistics.h"
#endif

/* activate queue monitoring module of plexi only when needed */
#if PLEXI_WITH_QUEUE_STATISTICS
#include "plexi-queue-statistics.h"
#endif


void
plexi_packet_received(void)
{
#if TSCH_LOG_CONF_LEVEL
  linkaddr_t *sender = (linkaddr_t *)packetbuf_addr(PACKETBUF_ADDR_SENDER);
  struct tsch_slotframe *slotframe = tsch_schedule_get_slotframe_by_handle((uint16_t)packetbuf_attr(PACKETBUF_ATTR_TSCH_SLOTFRAME));
  uint16_t slotoffset = (uint16_t)packetbuf_attr(PACKETBUF_ATTR_TSCH_TIMESLOT);
  uint64_t asn21 = packetbuf_attr(PACKETBUF_ATTR_TSCH_ASN_2_1);
  uint64_t asn43 = packetbuf_attr(PACKETBUF_ATTR_TSCH_ASN_4_3);
  uint64_t asn5 = packetbuf_attr(PACKETBUF_ATTR_TSCH_ASN_5);
  uint64_t asn = (asn5 << 32) | (asn43 << 16) | asn21;

  PRINTF("PLEXI: [%ld] ",clock_time());
#if LINKADDR_SIZE == 2
  PRINTF("%02x",(unsigned int)(linkaddr_node_addr.u16 & 0xFF));
  PRINTF(':');
  PRINTF("%02x",(unsigned int)((linkaddr_node_addr.u16>>8) & 0xFF));
#else
  unsigned int i;
  for(i = 0; i < LINKADDR_SIZE; i++) {
    if(i > 0) {
      PRINTF(":");
    }
    PRINTF("%02x",(unsigned int)linkaddr_node_addr.u8[i]);
  }
#endif
  PRINTF(" <- ");
#if LINKADDR_SIZE == 2
  PRINTF("%02x",(unsigned int)(sender->u16 & 0xFF));
  PRINTF(":");
  PRINTF("%02x",(unsigned int)((sender->u16>>8) & 0xFF));
#else
  for(i = 0; i < LINKADDR_SIZE; i++) {
    if(i > 0) {
      PRINTF(":");
    }
    PRINTF("%02x",(unsigned int)sender->u8[i]);
  }
#endif
  PRINTF(", asn=%ld, slotframe=%d, slotoffset=%d\n", asn, slotframe->handle, slotoffset);
#endif

#if PLEXI_WITH_RPL_DAG_RESOURCE
  plexi_rpl_packet_received();
#endif
}

void
plexi_packet_sent(int mac_status)
{
#if TSCH_LOG_CONF_LEVEL
  if(mac_status == MAC_TX_OK && packetbuf_attr(PACKETBUF_ATTR_MAC_ACK)) {
    linkaddr_t *receiver = (linkaddr_t *)packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
    struct tsch_slotframe *slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_by_handle((uint16_t)packetbuf_attr(PACKETBUF_ATTR_TSCH_SLOTFRAME));
    uint16_t slotoffset = (uint16_t)packetbuf_attr(PACKETBUF_ATTR_TSCH_TIMESLOT);
    struct tsch_link *link = (struct tsch_link *)tsch_schedule_get_link_by_timeslot(slotframe, slotoffset);

    PRINTF("PLEXI: [%ld] ",clock_time());

#if LINKADDR_SIZE == 2
    PRINTF("%02x",(unsigned int)(linkaddr_node_addr.u16 & 0xFF));
    PRINTF(":");
    PRINTF("%02x",(unsigned int)((linkaddr_node_addr.u16>>8) & 0xFF));
#else
    unsigned int i;
    for(i = 0; i < LINKADDR_SIZE; i++) {
      if(i > 0) {
        PRINTF(":");
      }
      PRINTF("%02x",(unsigned int)linkaddr_node_addr.u8[i]);
    }
#endif
    PRINTF(" -> ");
#if LINKADDR_SIZE == 2
    PRINTF("%02x",(unsigned int)(receiver->u16 & 0xFF));
    PRINTF(":");
    PRINTF("%02x",(unsigned int)((receiver->u16>>8) & 0xFF));
#else
    for(i = 0; i < LINKADDR_SIZE; i++) {
      if(i > 0) {
        PRINTF(":");
      }
      PRINTF("%02x",(unsigned int)receiver->u8[i]);
    }
#endif
    PRINTF(", timeslot=%d, slotframe=%d, channeloffset=%d\n", link->timeslot, link->slotframe_handle, link->channel_offset);
  }
#endif
}

void
plexi_init()
{
  
  rime_sniffer_add(&plexi_sniffer);
  PRINTF("PLEXI: initializing scheduler interface modules:\n");

  /* Initialize CoAP service */
  rest_init_engine();

#if PLEXI_WITH_ID_RESOURCE
  rest_activate_resource(&resource_ids, IDS_RESOURCE);
  PRINTF("  * L2 & L3 addresses resource\n");
#endif

#if PLEXI_WITH_RPL_DAG_RESOURCE
  rest_activate_resource(&resource_rpl_dag, DAG_RESOURCE);
  static struct uip_ds6_notification n;
  uip_ds6_notification_add(&n, rpl_changed_callback);
  PRINTF("  * RPL resource\n");
  plexi_rpl_init();
#endif

#if PLEXI_WITH_NEIGHBOR_RESOURCE
  rest_activate_resource(&resource_6top_nbrs, NEIGHBORS_RESOURCE);
  static struct uip_ds6_notification m;
  uip_ds6_notification_add(&m, route_changed_callback);
  PRINTF("  * Neighbor list resource\n");
#endif

#if PLEXI_WITH_SLOTFRAME_RESOURCE
  rest_activate_resource(&resource_6top_slotframe, FRAME_RESOURCE);
  PRINTF("  * TSCH slotframe resource\n");
#endif

#if PLEXI_WITH_LINK_RESOURCE
  rest_activate_resource(&resource_6top_links, LINK_RESOURCE);
  PRINTF("  * TSCH links resource\n");
#endif

#if PLEXI_WITH_LINK_STATISTICS
  plexi_link_statistics_init(); /* initialize plexi-link-statistics module */
  PRINTF("  * TSCH link statistics resource\n");
#endif

#if PLEXI_WITH_QUEUE_STATISTICS
  //plexi_queue_statistics_init(); /* initialize plexi-queue-statistics module */
#endif
}

int
plexi_string_to_linkaddr(char* address, unsigned int size, linkaddr_t* lladdr) {
  char *end;
  char *start = address;
  unsigned int count = 0;
  unsigned char byte;
  while((byte = (unsigned char)strtol(start,&end,16)) || end-start==2 || end-start==1) {
    count++;
    lladdr->u8[count-1] = byte;
    if (count < LINKADDR_SIZE && *end == ':') {
      end++;
    } else if (count == LINKADDR_SIZE && *end != ':') {
      return 1;
    }
    start = end;
  }
  return 0;
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

uint8_t
plexi_reply_char_if_possible(char c, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset)
{
  if(*strpos >= *offset && *bufpos < bufsize) {
    buffer[(*bufpos)++] = c;
  }
  ++(*strpos); 
  return 1;
}

uint8_t
plexi_reply_string_if_possible(char *s, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset)
{
  if(*strpos + strlen(s) > *offset) {
    uint8_t n = snprintf((char*)buffer + (unsigned int)(*bufpos),
                       (unsigned int)bufsize - (unsigned int)(*bufpos) + 1,
                       "%s",
                       s
                       + (*offset - (int32_t)(*strpos) > 0 ?
                          *offset - (int32_t)(*strpos) : 0));
    *bufpos += n;
    if(*bufpos >= bufsize) {
      *strpos += n;
      return 0; 
    } 
  }
  *strpos += strlen(s); 
  return 1; 
}

uint8_t
plexi_reply_hex_if_possible(unsigned int hex, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset, uint8_t min_size_format)
{
  int hexlen = 0;
  unsigned int temp_hex = hex;
  while(temp_hex > 0) {
    hexlen++;
    temp_hex = temp_hex>>4;
  }
  if(hex == 0)
    hexlen = 1;
  unsigned int zeros = min_size_format > hexlen ? min_size_format-hexlen : 0;
  int j;
  for(j=0; j<zeros; j++) {
    plexi_reply_char_if_possible('0', buffer, bufpos, bufsize, strpos, offset);
  }
  int mask = 0;
  int mask_size = hexlen - (int)*offset + (int)(*strpos);
  int i = mask_size;
  while(i>0) {
    mask = mask<<4;
    mask = mask | 0xF;
    i--;
  }
  if(*strpos + hexlen > *offset) {
    if(mask_size < hexlen) {
      unsigned int h = mask & hex;
      int hlen = 0;
      unsigned int temp_h = h;
      while(temp_h > 0) {
        hlen++;
        temp_h = temp_h>>4;
      }
      if(h == 0)
        hlen = 1;
      if(hlen < mask_size) {
        int j=0;
        for(j=0; j<mask_size-hlen; j++) {
          buffer[(*bufpos)++] = '0';
        }
      }
    }
    *bufpos += snprintf((char *)buffer + (*bufpos),
                     bufsize - (*bufpos) + 1,
                     "%x",
                     (*offset - (int32_t)(*strpos) > 0 ?
                        hex & mask : hex));  
    if(*bufpos >= bufsize) {
      return 0; 
    } 
  }
  *strpos += hexlen; 
  return 1; 
}

uint32_t
embedded_pow10(x)
{
  if(x==1)
    return 10;
  else if(x==2)
    return 100;
  else if(x==3)
    return 1000;
  else if(x==4)
    return 10000;
  else if(x==5)
    return 100000;
  else if(x==6)
    return 1000000;
  else
    return 0;
}


uint8_t
plexi_reply_uint16_if_possible(uint16_t d, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset)
{
  int len = 1;
  uint16_t temp_d = d;
  while(temp_d > 9) {
    len++;
    temp_d /= 10;
  }
  if(*strpos + len > *offset) {
    uint8_t n = snprintf((char *)buffer + (*bufpos),
                       bufsize - (*bufpos) + 1,
                       "%"PRIu16,
                       (*offset - (int32_t)(*strpos) > 0 ?
                          (uint16_t)d % (uint16_t)embedded_pow10(len - *offset + (int32_t)(*strpos)) : (uint16_t)d));
    *bufpos += n;
    if(*bufpos >= bufsize) {
      *strpos += n;
      return 0; 
    } 
  }
  *strpos += len;
  return 1; 
}

void
plexi_reply_lladdr_if_possible(const linkaddr_t *lladdr, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset)
{
#if LINKADDR_SIZE == 2
  plexi_reply_hex_if_possible((unsigned int)(lladdr->u16 & 0xFF), buffer, bufpos, bufsize, strpos, offset,2);
  plexi_reply_char_if_possible(':', buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_hex_if_possible((unsigned int)((lladdr->u16>>8) & 0xFF), buffer, bufpos, bufsize, strpos, offset,2);
#else
  unsigned int i;
  for(i = 0; i < LINKADDR_SIZE; i++) {
    if(i > 0) {
      plexi_reply_char_if_possible(':', buffer, bufpos, bufsize, strpos, offset);
    }
    plexi_reply_hex_if_possible((unsigned int)lladdr->u8[i], buffer, bufpos, bufsize, strpos, offset,2);
  }
#endif
}

uint8_t
plexi_reply_ip_if_possible(const uip_ipaddr_t *addr, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset)
{
  if(*bufpos >= bufsize)
    return 0;
#if NETSTACK_CONF_WITH_IPV6
  uint16_t a;
  unsigned int i;
  int f;
#endif /* NETSTACK_CONF_WITH_IPV6 */
  if(addr == NULL) {
    return 0;
  }
#if NETSTACK_CONF_WITH_IPV6
  if(ip64_addr_is_ipv4_mapped_addr(addr)) {
    /*
     * Printing IPv4-mapped addresses is done according to RFC 3513 [1]
     *
     *     "An alternative form that is sometimes more
     *     convenient when dealing with a mixed environment
     *     of IPv4 and IPv6 nodes is x:x:x:x:x:x:d.d.d.d,
     *     where the 'x's are the hexadecimal values of the
     *     six high-order 16-bit pieces of the address, and
     *     the 'd's are the decimal values of the four
     *     low-order 8-bit pieces of the address (standard
     *     IPv4 representation)."
     *
     * [1] https://tools.ietf.org/html/rfc3513#page-5
     */
    plexi_reply_string_if_possible("::FFFF:", buffer, bufpos, bufsize, strpos, offset);
    plexi_reply_uint16_if_possible((uint16_t)addr->u8[12], buffer, bufpos, bufsize, strpos, offset);
    plexi_reply_char_if_possible('.', buffer, bufpos, bufsize, strpos, offset);
    plexi_reply_uint16_if_possible((uint16_t)addr->u8[13], buffer, bufpos, bufsize, strpos, offset);
    plexi_reply_char_if_possible('.', buffer, bufpos, bufsize, strpos, offset);
    plexi_reply_uint16_if_possible((uint16_t)addr->u8[14], buffer, bufpos, bufsize, strpos, offset);
    plexi_reply_char_if_possible('.', buffer, bufpos, bufsize, strpos, offset);
    plexi_reply_uint16_if_possible((uint16_t)addr->u8[15], buffer, bufpos, bufsize, strpos, offset);

//    PRINTA("::FFFF:%u.%u.%u.%u", addr->u8[12], addr->u8[13], addr->u8[14], addr->u8[15]);
  } else {
    for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
      a = (addr->u8[i] << 8) + addr->u8[i + 1];
      if(a == 0 && f >= 0) {
        if(f++ == 0) {
          plexi_reply_string_if_possible("::", buffer, bufpos, bufsize, strpos, offset);
//          PRINTA("::");
        }
      } else {
        if(f > 0) {
          f = -1;
        } else if(i > 0) {
          plexi_reply_char_if_possible(':', buffer, bufpos, bufsize, strpos, offset);
//          PRINTA(":");
        }
        plexi_reply_hex_if_possible((uint16_t)a, buffer, bufpos, bufsize, strpos, offset,1);
//        PRINTA("%x", a);
      }
    }
  }
#else /* NETSTACK_CONF_WITH_IPV6 */
  plexi_reply_uint16_if_possible((uint16_t)addr->u8[0], buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_char_if_possible('.', buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_uint16_if_possible((uint16_t)addr->u8[1], buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_char_if_possible('.', buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_uint16_if_possible((uint16_t)addr->u8[2], buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_char_if_possible('.', buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_uint16_if_possible((uint16_t)addr->u8[3], buffer, bufpos, bufsize, strpos, offset);
//  PRINTA("%u.%u.%u.%u", addr->u8[0], addr->u8[1], addr->u8[2], addr->u8[3]);
#endif /* NETSTACK_CONF_WITH_IPV6 */
  return 1;
}
