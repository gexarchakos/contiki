/*
 * Copyright (c) 2015 NXP B.V.
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
 * 3. Neither the name of NXP B.V. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY NXP B.V. AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NXP B.V. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/**
 * \file
 * This file is part of the Contiki operating system.
 *
 * \author Theo van Daele <theo.van.daele@nxp.com>
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

#include "net/ip/uip-debug.h"
#define DEBUG DEBUG_FULL


static int total_time = 0;
static unsigned int interval = 0;

static struct uip_udp_conn *udp_conn;

static unsigned int
udp_interval(const int* cdf, int size)
{
  unsigned int tmp = random_rand();
  int i = 0;
  for(i=0; i<size; i++)
  {
    if(tmp<=cdf[i])
      return cdf[i];
  }
  return (unsigned int)65536;
}

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


void 
traffic_receive_notification(void)
{
  printf("TRAFFIC: new udp traffic received\n");
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
  PROCESS_BEGIN();
#ifndef TRAFFIC_CDF
  PROCESS_END();
#endif
  static struct etimer et;
  
  /* Listen to any host on 8185 */
  udp_conn = udp_new(NULL, 0, NULL);
  udp_bind(udp_conn, UIP_HTONS(TRAFFIC_PORT));
  /* Wait for timer event 
     On timer event, handle next sample */
  interval = udp_interval(TRAFFIC_CDF, TRAFFIC_CDF_SIZE);
  etimer_set(&et, interval*CLOCK_SECOND);
  while(1) {
    PROCESS_WAIT_EVENT();
#ifdef TRAFFIC_RECEIVE_CALLBACK
    if (ev == tcpip_event) {
      traffic_receive_notification();
    }
#endif
#ifdef TRAFFIC_TRANSMIT_PAYLOAD
    if (etimer_expired(&et) ) {
      total_time += interval;
      uip_ipaddr_t destination;
      traffic_str_to_ipaddr(&destination,(char*)destinations[random_rand() % DESTINATIONS_COUNT],128);
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
        printf("TRAFFIC: after %u sec a new UDP is ready to be send\n",interval);
		char buffer[UIP_APPDATA_SIZE];
		int siz = TRAFFIC_TRANSMIT_PAYLOAD(buffer, UIP_APPDATA_SIZE);
        uip_udp_packet_sendto(udp_conn, buffer, siz, &destination, UIP_HTONS(TRAFFIC_PORT));
      }
      interval = udp_interval(TRAFFIC_CDF, TRAFFIC_CDF_SIZE);
      etimer_reset_with_new_interval(&et, interval*CLOCK_SECOND);
    }
#endif
  }
  PROCESS_END();
}

void
traffic_init()
{
  process_start(&traffic_process, NULL);
}