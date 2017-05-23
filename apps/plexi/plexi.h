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
 *
 * \file
 *         plexi is a CoAP interface of IEEE802.15.4 PHY, MAC (incl. TSCH) and RPL resources.
 *         Link quality metrics (ETX, RSSI, LQI), schedule properties (ASN, slotFrames and cells), DoDAG structure (parents, children) are monitored, observed or modified.
 *         (refer to "plexi: Adaptive re-scheduling web service of time synchronized low-power wireless networks¨, JNCA, Elsevier)
 *
 *         plexi resource interface (header file). Landing header file.
 *         Defines new type of resources that enable both subresources and event handling.
 *         Defines commonly used functions.
 *
 *         plexi tries to follow the YANG model as defined in <a href="https://tools.ietf.org/html/draft-ietf-6tisch-6top-interface-04"> 6TiSCH Operation Sublayer (6top) Interface</a>.
 *         However, as that standardization effort seems stalled, plexi has deviations from that. As soon as that activity restarts, we are keen to reconsider decisions made.
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 * \author Ilker Oztelcan <i.oztelcan@tue.nl>
 * \author Dimitris Sarakiotis <d.sarakiotis@tue.nl>
 */

#ifndef __PLEXI_H__
#define __PLEXI_H__

#include <stdio.h>
#include "contiki.h"
#include "net/linkaddr.h"
#include "net/ip/uip.h"
#include "lib/list.h"
#include "jsonparse.h"

//#include "plexi-conf.h" /* Defines the size of CoAP reply buffer */

/** \brief Maximum size of buffer for CoAP replies
 */
#define MAX_DATA_LEN                        REST_MAX_CHUNK_SIZE


/** \def PLEXI_REQUEST_CONTENT_UNLOCKED
 * \brief Mutex flag to release the lock on plexi request buffer
 *
 * POST commands on TSCH cellList and statistics may have long payloads that have to be transferred in multiple blocks.
 * plexi does not allow two simultaneous POST requests on the same resource using a different mutex per resource.
 * Though each mutex is defined in the relevant module, its possible values are the PLEXI_REQUEST_CONTENT_UNLOCKED and PLEXI_REQUEST_CONTENT_LOCKED.
 * However, two mutexes are independent and requests to the resources they protect can be processed in parallel.
 *
 * \sa PLEXI_REQUEST_CONTENT_LOCKED
 */
#define PLEXI_REQUEST_CONTENT_UNLOCKED   0
/** \def PLEXI_REQUEST_CONTENT_LOCKED
 * \brief Mutex flag to acquire the lock on plexi request buffer
 *
 * \sa PLEXI_REQUEST_CONTENT_UNLOCKED
 */
#define PLEXI_REQUEST_CONTENT_LOCKED     1

/** \def PARENT_EVENT_RESOURCE(name, attributes, get_handler, post_handler, put_handler, delete_handler, event_handler)
 * \brief New type of resource enabling subresources and events (observable on change). See apps/rest-engine/rest-engine.h for more
 */
#define PARENT_EVENT_RESOURCE(name, attributes, get_handler, post_handler, put_handler, delete_handler, event_handler) \
  resource_t name = { NULL, NULL, HAS_SUB_RESOURCES | IS_OBSERVABLE, attributes, get_handler, post_handler, put_handler, delete_handler, { .trigger = event_handler } }

/** \def PARENT_PERIODIC_RESOURCE(name, attributes, get_handler, post_handler, put_handler, delete_handler, event_handler)
 * \brief New type of resource enabling subresources and time-based events (periodically observable). See apps/rest-engine/rest-engine.h for more
 */
#define PARENT_PERIODIC_RESOURCE(name, attributes, get_handler, post_handler, put_handler, delete_handler, period, periodic_handler) \
  periodic_resource_t periodic_##name; \
  resource_t name = { NULL, NULL, HAS_SUB_RESOURCES | IS_OBSERVABLE | IS_PERIODIC, attributes, get_handler, post_handler, put_handler, delete_handler, { .periodic = &periodic_##name } }; \
  periodic_resource_t periodic_##name = { NULL, &name, period, { { 0 } }, periodic_handler };

/** \var char plexi_reply_content[MAX_DATA_LEN]
 * \brief Buffer of characters containing the payload of CoAP replies
 *
 * The buffer is used by er-coap to populate the payload of a reply. As soon as the function coap_set_payload is called the contents of this buffer are copied into the payload.
 * Though a local variable could be used, this global buffer allows the construction of a payload progressively in multiple functions.
 * \sa MAX_DATA_LEN
 */
//char plexi_reply_content[MAX_DATA_LEN];
/** \var int plexi_reply_content_len
 * \brief Length of \link plexi_reply_content \endlink buffer.
 *
 * \link plexi_reply_content \endlink is not a null terminated string and this keeps track of the last character. Note that \link plexi_reply_content_len \endlink &le; \link MAX_DATA_LEN \endlink.
 * \sa plexi_reply_content
 */
//extern int plexi_reply_content_len;

/** \def CONTENT_PRINTF(...)
 * \brief printf-like macro to save strings to the reply buffer \link plexi_reply_content \endlink.
 *
 * Like snprintf, CONTENT_PRINTF writes the formatted input to the \link plexi_reply_content \endlink buffer and increases the \link plexi_reply_content_len \endlink to reflect the actual size
 * of the buffer. The input parameters are the same as those of printf. If the formatted input exceeds the maximum size of \link plexi_reply_content \endlink, the buffer remains unchanged.
 */
uint8_t plexi_reply_char_if_possible(char c, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset);
uint8_t plexi_reply_string_if_possible(char *s, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset);
uint8_t plexi_reply_02hex_if_possible(unsigned int hex, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset);
uint8_t plexi_reply_uint16_if_possible(uint16_t d, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset);
void plexi_reply_lladdr_if_possible(const linkaddr_t *lladdr, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset);
uint8_t plexi_reply_ip_if_possible(const uip_ipaddr_t *addr, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset);


///**
// * \brief Utility function. Converts na field (string containing the lower 64bit of the IPv6) to
// * 64-bit MAC.
// * \param na_inbuf An array of characters containing the EUI64 address. It should be null-terminated i.e. '\0'
// * \param bufsize The size of na_inbuf in number of characters
// * \param linkaddress Pointer to the linkaddr_t variable in which the result will be stored
// * \retval 0 Conversion failed
// * \retval 1 Coversion was successful
// */
//int plexi_eui64_to_linkaddr(const char *na_inbuf, int bufsize, linkaddr_t *linkaddress);
///**
// * \brief Utility function. Converts a 64-bit MAC address to a EUI-64 IP address stored in a char array.
// * \param buf The array of characters containing the EUI64 address in hexadecimal form. It should be of no less than 20 characters long.
// * \param addr Pointer to the linkaddr_t variable which will be converted
// * \retval 0 Conversion failed
// * \retval 1 Coversion was successful
// */
//int plexi_linkaddr_to_eui64(char *buf, linkaddr_t *addr);

/**
 * \brief Utility function. Searches for a field in a json object.
 * \param js Pointer to the input json object
 * \param field_buf Char array containing the name of the field to be found
 * \param field_buf_len The length of the field in number of characters
 * \return Returns the json type of the field field_buf or 0 if field not found
 *
 * For more information on json types see apps/json/jsonparse.h
 */
int plexi_json_find_field(struct jsonparse_state *js, char *field_buf, int field_buf_len);

/* activate RPL-related module of plexi only when needed */
#if PLEXI_WITH_RPL_DAG_RESOURCE
/**
 * \brief Callback registered to events on RPL DoDAG.
 *
 * Upon an event this callback is triggered which then calls the \link plexi_dag_event_handler \endlink.
 * It introduces a delay of \ref PLEXI_RPL_UPDATE_INTERVAL before the \link plexi_dag_event_handler \endlink is called.
 * This delay is counted with the help of \link rpl_changed_timer \endlink.
 */
void rpl_changed_callback(int event, uip_ipaddr_t *route, uip_ipaddr_t *ipaddr, int num_routes);
#endif

#if PLEXI_WITH_NEIGHBOR_RESOURCE
/**
 * \brief Callback function to be called when a change to the DAG_RESOURCE resource has occurred.
 * Any change is delayed 30seconds before it is propagated to the observers.
 *
 * \bug UIP+DS6_NOTIFICATION_* do not provide a reliable way to listen to events
 */
void route_changed_callback(int event, uip_ipaddr_t *route, uip_ipaddr_t *ipaddr, int num_routes);
#endif

/**
 * \brief Landing initialization function of plexi. Call from application to start plexi.
 *
 * plexi enables/disables the various modules based on plexi-conf.h
 * plexi may be used for RPL even if TSCH is not running. Symmetrically, plexi can interact with TSCH even if RPL is not present.
 * However, to monitor neighbors or link and queue statistics TSCH should be running.
 */
void plexi_init();

#endif
