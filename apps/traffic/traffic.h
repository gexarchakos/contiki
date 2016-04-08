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
 *
 * \file
 *         UDP Traffic Generator. 
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 */

#ifndef __TRAFFIC_H__
#define __TRAFFIC_H__

#undef NETSTACK_CONF_WITH_IPV6
#define NETSTACK_CONF_WITH_IPV6 1

#define UIP_IP_BUF ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_UDP_BUF  ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])

#ifdef TRAFFIC_RECEIVE_CALLBACK
void TRAFFIC_RECEIVE_CALLBACK(uip_ipaddr_t *srcaddr, uint16_t srcport, char* payload);
#endif

#ifdef TRAFFIC_TRANSMIT_PAYLOAD
int TRAFFIC_TRANSMIT_PAYLOAD(char* buffer, int max);
#endif

#if defined TRAFFIC_DESTINATIONS && TRAFFIC_DESTINATIONS_COUNT
extern const char *TRAFFIC_DESTINATIONS[TRAFFIC_DESTINATIONS_COUNT];
#endif

void traffic_init();
void traffic_end();

#endif