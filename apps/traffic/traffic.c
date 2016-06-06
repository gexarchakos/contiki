/*
 * Copyright (c) 2016, Georgios Exarchakos
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
/**
 * \file
 *       Implementation of a peer (client and server) of UDP random traffic
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 *
 */

#include <errno.h>

#include "contiki-conf.h"
#include "net/netstack.h"
#include "net/rpl/rpl-private.h"
#include "net/ip/uip-debug.h"
#include "lib/random.h"
#include "net/ip/uiplib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net/ip/uip-udp-packet.h"

#include "traffic-conf.h"
#include "traffic.h"
#include "traffic-cdfs.h"

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

#if defined TRAFFIC_TRANSMIT_PAYLOAD && defined TRAFFIC_DESTINATIONS && TRAFFIC_DESTINATIONS_COUNT
static int total_time = 0;
static unsigned int interval = 0;

static unsigned int
udp_interval(const int* cdf, int size)
{
  unsigned int tmp = random_rand()*65535/RANDOM_RAND_MAX;
  int i = 0;
  for(i=0; i<size; i++)
  {
    if(tmp<=cdf[i]) {
      return i*65535/size;
    }
  }
  return (unsigned int)65535;
}

#endif

static struct uip_udp_conn *udp_conn;

int
traffic_str_to_ipaddr(uip_ipaddr_t* address, char *na_inbuf, int bufsize)
{
	char *prefix = NULL, *suffix = NULL;
	int preblocks = 0, postblocks = 0;
	int prefixsize = 0, suffixsize = 0;
	int i;
	for(i=0; i<bufsize; i++) {
		if(na_inbuf[i]==':' && na_inbuf[i+1]==':')
		{
			prefix = na_inbuf;
			prefixsize = i;
			if(i>0) preblocks++;
			suffix = &na_inbuf[i+2];
			suffixsize = i+2;
		}
		else if(na_inbuf[i]==':')
		{
			if(suffix)
			{
				postblocks++;
			}
			else if(i>0)
			{
				preblocks++;
			}
		}
		else if(na_inbuf[i]=='\0')
		{
			if(suffix)
				suffixsize = i - suffixsize;
			else
			{
				prefix = na_inbuf;
				prefixsize = i;
				if(i>0) preblocks++;
			}
			break;
		}
	}
	if(prefix && preblocks < 8 && !suffix) {
		suffix = prefix;
		postblocks = preblocks;
		suffixsize = prefixsize;
		prefix = NULL;
		preblocks = 0;
		prefixsize = 0;
	}

	if(preblocks + postblocks < 8)
	{
		int template_found = 0;
		uint8_t state;
		for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
			state = uip_ds6_if.addr_list[i].state;
			if(uip_ds6_if.addr_list[i].isused && (state == ADDR_TENTATIVE || state == ADDR_PREFERRED))
			{
				uip_ipaddr_copy(address, &uip_ds6_if.addr_list[i].ipaddr);
				template_found = 1;
				break;
			}
		}
		if(!template_found)
		{
			return 0;
		}
	}
	char next_end_char = ':';
	char *end;
	unsigned val;
	char *buf_end = prefix + prefixsize + 1;
	for(i=0; i<preblocks; i++)
	{
		if(prefix >= buf_end) {
			return 0;
		}
		if(i == 7) {
			next_end_char = '\0';
		}
		val = (unsigned)strtoul(prefix, &end, 16);
		if(end != prefix && *end == next_end_char && errno != ERANGE) {
			address->u8[2 * i] = val >> 8;
			address->u8[2 * i + 1] = val;
			prefix = end + 1;
		} else {
			return 0;
		}
	}
	buf_end = suffix + suffixsize + 1;
	for(i=8-postblocks; i<8; i++)
	{
		if(suffix >= buf_end) {
			return 0;
		}
		if(i == 7) {
			next_end_char = '\0';
		}
		val = (unsigned)strtoul(suffix, &end, 16);
		if(end != suffix && *end == next_end_char && errno != ERANGE) {
			address->u8[2 * i] = val >> 8;
			address->u8[2 * i + 1] = val;
			suffix = end + 1;
		} else {
			return 0;
		}
	}
	return 1;
}

int
traffic_transmit_hello(char* buffer, int max)
{
	buffer[0] = 'h';
	buffer[1] = 'e';
	buffer[2] = 'l';
	buffer[3] = 'l';
	buffer[4] = 'o';
	return 5;
}

PROCESS(traffic_process, "Traffic Generator process");

PROCESS_THREAD(traffic_process, ev, data)
{

#ifndef TRAFFIC_CDF
}
#else

  PROCESS_BEGIN();

  printf("TRAFFIC: process started\n");

  /* Listen to any host */
  udp_conn = udp_new(NULL, 0, NULL);
  udp_bind(udp_conn, UIP_HTONS(TRAFFIC_PORT));
  /* Wait for timer event 
     On timer event, handle next sample */
#if defined TRAFFIC_TRANSMIT_PAYLOAD && defined TRAFFIC_DESTINATIONS && TRAFFIC_DESTINATIONS_COUNT
  static struct etimer et;
  interval = udp_interval(TRAFFIC_CDF, TRAFFIC_CDF_SIZE);

#ifdef TRAFFIC_CDF_SHIFT_FACTOR
  interval = interval + TRAFFIC_CDF_SHIFT_FACTOR;
#endif
#ifdef TRAFFIC_CDF_SHRINK_FACTOR
  interval = interval >> TRAFFIC_CDF_SHRINK_FACTOR;
#endif

  etimer_set(&et, interval*CLOCK_SECOND);
#endif
  while(1) {
    PROCESS_WAIT_EVENT();
    if (ev == tcpip_event) {
      if(uip_newdata()) {
        ((char *)uip_appdata)[uip_datalen()] = '\0';
        printf("TRAFFIC: <- [");
		uip_debug_ipaddr_print(&UIP_IP_BUF->srcipaddr);
		printf("]:%u, \"%s\"\n",uip_ntohs(UIP_UDP_BUF->srcport), (char *)uip_appdata);
#ifdef TRAFFIC_RECEIVE_CALLBACK
        TRAFFIC_RECEIVE_CALLBACK(&UIP_IP_BUF->srcipaddr, uip_ntohs(UIP_UDP_BUF->srcport), (char *)uip_appdata);
#endif
      }
    }
#if defined TRAFFIC_TRANSMIT_PAYLOAD && defined TRAFFIC_DESTINATIONS && TRAFFIC_DESTINATIONS_COUNT
    if (etimer_expired(&et) ) {
      total_time += interval;
      uip_ipaddr_t destination;
	  char *addr_str = (char*)TRAFFIC_DESTINATIONS[random_rand() % TRAFFIC_DESTINATIONS_COUNT];
      traffic_str_to_ipaddr(&destination,addr_str,strlen(addr_str)+1);
	  int i;
	  int skip = 0;
      uint8_t state;
      for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
        state = uip_ds6_if.addr_list[i].state;
        if((uip_ds6_if.addr_list[i].isused && (state == ADDR_TENTATIVE || state == ADDR_PREFERRED))
			&& (uip_ipaddr_cmp(&uip_ds6_if.addr_list[i].ipaddr,&destination)))
        {
			skip = 1;
			break;
		}
	  }  
	  if(!skip)
      {
        char buffer[UIP_APPDATA_SIZE];
		int siz = TRAFFIC_TRANSMIT_PAYLOAD(buffer, UIP_APPDATA_SIZE);
        printf("TRAFFIC: -> [");
		uip_debug_ipaddr_print(&destination);
		printf("]:%u, \"%s\" //after delay of %usec\n",TRAFFIC_PORT, buffer, interval);
		uip_udp_packet_sendto(udp_conn, buffer, siz, &destination, UIP_HTONS(TRAFFIC_PORT));
      }
      interval = udp_interval(TRAFFIC_CDF, TRAFFIC_CDF_SIZE);

#ifdef TRAFFIC_CDF_SHIFT_FACTOR
  interval = interval + TRAFFIC_CDF_SHIFT_FACTOR;
#endif
#ifdef TRAFFIC_CDF_SHRINK_FACTOR
      interval = interval >> TRAFFIC_CDF_SHRINK_FACTOR;
#endif
      etimer_reset_with_new_interval(&et, interval*CLOCK_SECOND);
    }
#endif
  }
  PROCESS_END();
  printf("TRAFFIC: process ended\n");
}
#endif

void
traffic_init()
{
  process_start(&traffic_process, NULL);
}

void
traffic_end()
{
  process_exit(&traffic_process);
}