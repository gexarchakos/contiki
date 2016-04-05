#ifndef __TRAFFIC_H__
#define __TRAFFIC_H__

#ifdef TRAFFIC_RECEIVE_CALLBACK
void TRAFFIC_RECEIVE_CALLBACK(void);
#endif

#ifdef TRAFFIC_TRANSMIT_PAYLOAD
int TRAFFIC_TRANSMIT_PAYLOAD(char* buffer, int max);
#endif

int traffic_transmit_hello(char* buffer, int max);
void traffic_receive_notification(void);

void traffic_init();

#endif