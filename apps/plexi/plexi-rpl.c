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
 * \brief
 *         Defines the observable RPL DoDAG resource and its GET and event handlers.
 *
 * \file
 *         plexi-rpl module: plexi interface for RPL DoDAG resource (implementation file)
 *
 *         RPL DoDAG is an event-based observable resource. That is, all subscribers to this resource
 *         receive notifications upon any changes, not periodically.
 *
 *         Due to instability of RPL at times, esp. at the bootstrapping phase of a network/node,
 *         the notifications are delayed by \ref PLEXI_RPL_UPDATE_INTERVAL seconds to avoid reflecting
 *         the instability to subscribers.
 *
 * \bug    Events of RPL DoDAG are not properly captured. Child addition works fine but
 *         child removal or parent switching most probably does not.
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 * \author Ilker Oztelcan <i.oztelcan@tue.nl>
 * \author Dimitris Sarakiotis <d.sarakiotis@tue.nl>
 *
 */

#include "plexi-interface.h"

#include "er-coap-engine.h"
#include "net/rpl/rpl.h"

#include "plexi.h"
#include "plexi-conf.h"
#include "plexi-rpl.h"

/** \brief
 *         Retrieves the preferred parent and direct children of a node in a RPL DoDAG
 *
 * Returns the complete local DoDAG object upon a request with \ref DAG_RESOURCE URL.
 * No subresources or queries are currenty supported.
 *
 * The requestor should set the "accept" field of the request empty or to "json".
 * Otherwise, the handler will reply with a 406-Not Acceptable error.
 *
 */
static void plexi_get_dag_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/** \brief
 *         Notifies subscribers of any change in the local DoDAG.
 *
 * Called when a change in local DoDAG occurs and, subsequently, calls \link plexi_get_dag_handler \endlink.
 * No subresources or queries are currenty supported.
 *
 * The requestor should set the "accept" field of the request empty or to "json".
 * Otherwise, the handler will reply with a 406-Not Acceptable error.
 *
 */
static void plexi_dag_event_handler(void);

/**
 * \brief RPL DoDAG Resource to GET the preferred parent and immediate children of the node.
 * It is observable based on local DoDAG changes.
 *
 * RPL DoDAG is an object consisting of two attributes: the parent and the children.
 * The local DoDAG is addressed via the URL set in \ref DAG_RESOURCE within plexi-interface.h.
 * Both the preferred parent and the children are packed in arrays. This is to provide future extensibility
 * by allowing more than one parent in a response e.g. preferred and backup parents.
 * The values stored in \ref DAG_RESOURCE are the EUI-64 addresses of the preferred parent and children.
 * Each RPL DoDAG resource is a json object like:
 * \code{.json}
 *  {
 *    DAG_PARENT_LABEL: array of EUI-64 address in string format
 *    DAG_CHILD_LABEL: array of EUI-64 addresses in string format
 *  }
 * \endcode
 */
EVENT_RESOURCE(resource_rpl_dag, "obs;title=\"RPL DAG Parent and Children\"",
               plexi_get_dag_handler,       /* GET handler */
               NULL,                /* POST handler */
               NULL,                /* PUT handler */
               NULL,                /* DELETE handler */
               plexi_dag_event_handler);      /* event handler */

/**
 * \brief Counter of the delay of each notification.
 * \sa rpl_changed_callback
 */
static struct ctimer rpl_changed_timer;

/**
 * \brief Callback registered to \ref rpl_changed_timer event. Once the timer expires this callback is triggered and subscribers notified.
 */
static void plexi_rpl_changed_handler(void *ptr);

/**
 * \brief Callback registered to events on RPL DoDAG.
 *
 * Upon an event this callback is triggered which then calls the \link plexi_dag_event_handler \endlink.
 * It introduces a delay of \ref PLEXI_RPL_UPDATE_INTERVAL before the \link plexi_dag_event_handler \endlink is called.
 * This delay is counted with the help of \link rpl_changed_timer \endlink.
 */
static void rpl_changed_callback(int event, uip_ipaddr_t *route, uip_ipaddr_t *ipaddr, int num_routes);

static void
plexi_get_dag_handler(void *request,
                      void *response,
                      uint8_t *buffer,
                      uint16_t preferred_size,
                      int32_t *offset)
{
//  size_t strpos = 0;
//  size_t bufpos = 0;
  
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);
  /* make sure the request accepts JSON reply or does not specify the reply type */
  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {
    plexi_reply_content_len = 0;
    CONTENT_PRINTF("{");
//    plexi_reply_char_if_possible('{', buffer, &bufpos, preferred_size, &strpos, offset);
    /* TODO: get details per dag id other than the default */
    /* Ask RPL to provide the preferred parent or if not known (e.g. LBR) leave it empty */
    rpl_parent_t *parent = rpl_get_any_dag()->preferred_parent;
    if(parent != NULL) {
      /* retrieve the IP address of the parent */
      uip_ipaddr_t *parent_addr = rpl_get_parent_ipaddr(parent);
      CONTENT_PRINTF("\"%s\":[\"%x:%x:%x:%x\"]", DAG_PARENT_LABEL,
                     UIP_HTONS(parent_addr->u16[4]), UIP_HTONS(parent_addr->u16[5]),
                     UIP_HTONS(parent_addr->u16[6]), UIP_HTONS(parent_addr->u16[7])
                     );
//      plexi_reply_char_if_possible('"', buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_string_if_possible(DAG_PARENT_LABEL, buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_string_if_possible("\":[\"", buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_hex_if_possible(UIP_HTONS(parent_addr->u16[4]), buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_char_if_possible(':', buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_hex_if_possible(UIP_HTONS(parent_addr->u16[5]), buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_char_if_possible(':', buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_hex_if_possible(UIP_HTONS(parent_addr->u16[6]), buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_char_if_possible(':', buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_hex_if_possible(UIP_HTONS(parent_addr->u16[7]), buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_string_if_possible("\"]", buffer, &bufpos, preferred_size, &strpos, offset);
    } else {
      CONTENT_PRINTF("\"%s\":[]", DAG_PARENT_LABEL);
//      plexi_reply_char_if_possible('"', buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_string_if_possible(DAG_PARENT_LABEL, buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_string_if_possible("\":[]", buffer, &bufpos, preferred_size, &strpos, offset);
    }

    CONTENT_PRINTF(",\"%s\":[", DAG_CHILD_LABEL);
//    plexi_reply_string_if_possible(",\"", buffer, &bufpos, preferred_size, &strpos, offset);
//    plexi_reply_string_if_possible(DAG_CHILD_LABEL, buffer, &bufpos, preferred_size, &strpos, offset);
//    plexi_reply_string_if_possible("\":[", buffer, &bufpos, preferred_size, &strpos, offset);

    nbr_table_item_t *r;
    for(r = nbr_table_head(nbr_routes); r != NULL; r = nbr_table_next(nbr_routes, r)) {
      if(r != nbr_table_head(nbr_routes)) {
        CONTENT_PRINTF(",");
//        plexi_reply_char_if_possible(',', buffer, &bufpos, preferred_size, &strpos, offset);
      }
      linkaddr_t *addr = (linkaddr_t *)nbr_table_get_lladdr(nbr_routes, r);
      CONTENT_PRINTF("\"2%02x:%02x%02x:%02x:%02x%02x\"",
                     UIP_HTONS(addr->u8[1]), UIP_HTONS(addr->u8[2]), UIP_HTONS(addr->u8[3]),
                     UIP_HTONS(addr->u8[5]), UIP_HTONS(addr->u8[6]), UIP_HTONS(addr->u8[7])
                     );
//      plexi_reply_string_if_possible("\"2", buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_hex_if_possible(UIP_HTONS(addr->u8[1]), buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_char_if_possible(':', buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_hex_if_possible(UIP_HTONS(addr->u8[2]), buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_hex_if_possible(UIP_HTONS(addr->u8[3]), buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_char_if_possible(':', buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_hex_if_possible(UIP_HTONS(addr->u8[5]), buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_char_if_possible(':', buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_hex_if_possible(UIP_HTONS(addr->u8[6]), buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_hex_if_possible(UIP_HTONS(addr->u8[7]), buffer, &bufpos, preferred_size, &strpos, offset);
//      plexi_reply_char_if_possible('"', buffer, &bufpos, preferred_size, &strpos, offset);
      
//      if(bufpos > preferred_size && strpos - bufpos > *offset) {
//        break;
//      }
    }
    CONTENT_PRINTF("]}");
//    plexi_reply_string_if_possible("]}", buffer, &bufpos, preferred_size, &strpos, offset);

//    if(bufpos > 0) {
      /* Build the header of the reply */
      REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
      /* Build the payload of the reply */
//      REST.set_response_payload(response, buffer, bufpos);
      REST.set_response_payload(response, plexi_reply_content, plexi_reply_content_len);
//    } else if(strpos > 0) {
//      coap_set_status_code(response, BAD_OPTION_4_02);
//      coap_set_payload(response, "BlockOutOfScope", 15);
//    }
//    if(strpos < *offset + preferred_size) {
//      *offset = -1;
//    } else {
//      *offset += preferred_size;
//    }
  } else { /* if the client accepts a response payload format other than json, return 406 */
    coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
    return;
  }
}
/* Notify all clients who observe changes to rpl/dag resource i.e. to the RPL dodag connections */
static void
plexi_dag_event_handler()
{
  /* Registered observers are notified and will trigger the GET handler to create the response. */
  REST.notify_subscribers(&resource_rpl_dag);
}
static void
plexi_rpl_changed_handler(void *ptr)
{
  REST.notify_subscribers(&resource_rpl_dag);
}
static void
rpl_changed_callback(int event, uip_ipaddr_t *route, uip_ipaddr_t *ipaddr, int num_routes)
{
  /* We have added or removed a routing entry, notify subscribers */
  if(event == UIP_DS6_NOTIFICATION_ROUTE_ADD || event == UIP_DS6_NOTIFICATION_ROUTE_RM) {
    ctimer_set(&rpl_changed_timer, PLEXI_RPL_UPDATE_INTERVAL, plexi_rpl_changed_handler, NULL);
  }
}
void
plexi_rpl_init()
{
  rest_activate_resource(&resource_rpl_dag, DAG_RESOURCE);
  static struct uip_ds6_notification n;
  uip_ds6_notification_add(&n, rpl_changed_callback);
}
