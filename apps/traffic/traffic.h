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