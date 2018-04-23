/*
 * Copyright (c) 2017, Technische Universiteit Eindhoven.
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
 *         PLEXI ID resource interface (implementation file)
 *
 * \brief  ID resource provides access to the list of L2, L3 addresses.
 *
 * \details This resource refers to the MAC and IP addresses of the device. The former is a single
 *         address but the latter is a list.
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 *
 */

#include "plexi-interface.h"

#include "plexi.h"

#include "er-coap-engine.h"
#include "net/ip/uip-debug.h"

/**
 * \brief Retrieves the list of neighbors upon a CoAP GET request to neighbor list resource.
 *
 * The handler reacts to requests on the following URLs:
 * - base returning a json array of the complete list of neighbor json objects
 *   - if \ref PLEXI_WITH_LINK_STATISTICS is not set: \code{http} GET /NEIGHBORS_RESOURCE -> e.g. [ {NEIGHBORS_TNA_LABEL: "215:8d00:57:6466"}, {NEIGHBORS_TNA_LABEL: "215:8d00:57:64a2"} ] \endcode
 *   - else if \ref PLEXI_WITH_LINK_STATISTICS is set: \code{http} GET /NEIGHBORS_RESOURCE -> e.g. [ {NEIGHBORS_TNA_LABEL: "215:8d00:57:6466",STATS_PDR_LABEL:89}, {NEIGHBORS_TNA_LABEL: "215:8d00:57:64a2",STATS_ETX_LABEL:2,STATS_RSSI_LABEL:-50} ] \endcode
 *   Note that each neighbor may have different metrics according to the statistics configuration installed in each link with that neighbor.
 *   All of the following are possible: \ref STATS_ETX_LABEL, \ref STATS_PDR_LABEL, \ref STATS_RSSI_LABEL, \ref STATS_LQI_LABEL, \ref NEIGHBORS_ASN_LABEL.
 * - subresources returning json arrays with the values of the specified subresource for all neighbors:
 *   - \code{http} GET /NEIGHBORS_RESOURCE/NEIGHBORS_TNA_LABEL -> a json array of EUI-64 IP addresses (one per neighbor) e.g. ["215:8d00:57:6466","215:8d00:57:64a2"] \endcode
 *   - if \ref PLEXI_WITH_LINK_STATISTICS is set: \code{http} GET /NEIGHBORS_RESOURCE/STATS_RSSI_LABEL -> a json array of slotframe RSSI values per neighbor e.g. [-50] \endcode
 *     Note, that any of the available metrics might by a subresource in the URL.
 * - queries on neighbor attributes returning the complete neighbor objects specified by the queries. Neihbors can be queried only by \ref NEGIHBORS_TNA_LABEL. It returns one neighbor object that fulfills the query:
 *   - \code{http} GET /NEIGHBORS_RESOURCE?NEIGHBORS_TNA_LABEL=215:8d00:57:6466 -> a json neighbor object with specific address e.g.
 *     if PLEXI_WITH_LINK_STATISTICS not set, then return
 *         {NEIGHBORS_TNA_LABEL: "215:8d00:57:6466"}
 *     else if PLEXI_WITH_LINK_STATISTICS is set, then return
 *         {NEIGHBORS_TNA_LABEL: "215:8d00:57:6466",STATS_PDR_LABEL:89}
 *     \endcode
 * - subresource and query. As expected, querying neighbors based on their \ref NEIGHBORS_TNA_LABEL and rendering one only subresource of the selected neighbors, returns an array of subresource values. E.g.
 *   - \code{http} GET /NEIGHBORS_RESOURCE/PDR?NEIGHBORS_TNA_LABEL=215:8d00:57:6466 -> a json array of PDR values from the selected neighbor
 *     if PLEXI_WITH_LINK_STATISTICS not set, then return []
 *     else if PLEXI_WITH_LINK_STATISTICS is set, then return [89]
 *     \endcode
 *
 *
 * \sa apps/rest-engine/rest-engine.h for more information on the handler signatures
 */
static void plexi_get_ids_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/**
 * \brief Neighbor List resource to GET and OBSERVE list of neighbors. Subscribers to this resource are notified of changes and periodically.
 *
 * Neighbor list is a read only resource and consists of neihbor objects. A neighbor, at its basic structure, is a json object containing one key-value pair: the key \ref NEIGHBORS_TNA_LABEL and its paired EUI-64 address in string format.
 * Multiple neighbors are represented by multiple json objects in that array. For example:
 * \code{.json}[ {NEIGHBORS_TNA_LABEL: "215:8d00:57:6466"}, {NEIGHBORS_TNA_LABEL: "215:8d00:57:64a2"} ] \endcode
 * The neighbor list object is addressed via the URL set in \ref NEIGHBORS_RESOURCE within plexi-interface.h.
 * Besides the \ref NEIGHBORS_TNA_LABEL subresource, each neighbor may contain extra subresources iff statistics are calculated for that neighbor.
 * The statistics-related subresources are: \ref STATS_ETX_LABEL, \ref STATS_RSSI_LABEL, \ref STATS_LQI_LABEL, \ref STATS_PDR_LABEL.
 * The switch that (de)activates statistics module is the \ref PLEXI_WITH_LINK_STATISTICS. Eanbling statistics module is not sufficient
 * to also enable those subresources in a neighbor object. Recall that statistics are maintained per link, not per neighbor.
 * Hence, to retrieve aggregated statistics per neighbor, links with the specific neighbor have to be configured to maintain statistics.
 * \note Due to preprocessor commands the documentation of the PARENT_EVENT_RESOURCE does not show up. Two different types of resources
 * are implemented based on whether the neighbor list is periodically (PARENT_PERIODIC_RESOURCE) or event-based (PARENT_EVENT_RESOURCE) observable. Check the code.
 * \sa plexi-link-statistics.h, plexi-link-statistics.c, PARENT_PERIODIC_RESOURCE
 */
PARENT_RESOURCE(resource_ids,                   /* name */
                         "title=\"Local IDs\"", /* attributes */
                         plexi_get_ids_handler, /* GET handler */
                         NULL,                  /* POST handler */
                         NULL,                  /* PUT handler */
                         NULL);                 /* DELETE handler */

static void
plexi_get_ids_handler(void *request, void *response, uint8_t *buffer, uint16_t bufsize, int32_t *offset)
{
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);
  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {
    size_t strpos = 0;            /* position in overall string (which is larger than the buffer) */
    size_t bufpos = 0;            /* position within buffer (bytes written) */

    char *uri_path = NULL;
    int uri_len = REST.get_url(request, (const char **)(&uri_path));
    int base_len = 0;
    char *uri_subresource = NULL;
    if(uri_len > 0) {
      *(uri_path + uri_len) = '\0';
      base_len = strlen(resource_ids.url);
      uri_subresource = uri_path + base_len;
      if(*uri_subresource == '/') {
        uri_subresource++;
      }
    }
    if(uri_len > base_len + 1 && strcmp(LL_ADDR_LABEL, uri_subresource) && strcmp(IP_ADDR_LABEL, uri_subresource)) {
      coap_set_status_code(response, BAD_REQUEST_4_00);
      coap_set_payload(response, "Supports only requests for L2 and L3 addresses", 46);
      return;
    }
    if(uri_subresource==NULL || *uri_subresource=='\0') {
      plexi_reply_string_if_possible("{\"", buffer, &bufpos, bufsize, &strpos, offset);
      plexi_reply_string_if_possible(IP_ADDR_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
      plexi_reply_string_if_possible("\":", buffer, &bufpos, bufsize, &strpos, offset);
    }
    if(uri_subresource==NULL || *uri_subresource=='\0' || !strcmp(IP_ADDR_LABEL, uri_subresource)) {
      int i;
      uint8_t state;

      plexi_reply_char_if_possible('[', buffer, &bufpos, bufsize, &strpos, offset);

      for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
        state = uip_ds6_if.addr_list[i].state;
        if(uip_ds6_if.addr_list[i].isused &&
           (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
          if(i>0)
            plexi_reply_char_if_possible(',', buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_char_if_possible('"', buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_ip_if_possible(&uip_ds6_if.addr_list[i].ipaddr, buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_char_if_possible('"', buffer, &bufpos, bufsize, &strpos, offset);
        }
      }
      plexi_reply_char_if_possible(']', buffer, &bufpos, bufsize, &strpos, offset);
    }

    if(uri_subresource==NULL || *uri_subresource=='\0') {
      plexi_reply_string_if_possible(",\"", buffer, &bufpos, bufsize, &strpos, offset);
      plexi_reply_string_if_possible(LL_ADDR_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
      plexi_reply_string_if_possible("\":", buffer, &bufpos, bufsize, &strpos, offset);
    }
    if(uri_subresource==NULL || *uri_subresource=='\0' || !strcmp(LL_ADDR_LABEL, uri_subresource)) {
      plexi_reply_string_if_possible("[\"", buffer, &bufpos, bufsize, &strpos, offset);
      plexi_reply_lladdr_if_possible(&linkaddr_node_addr, buffer, &bufpos, bufsize, &strpos, offset);
      plexi_reply_string_if_possible("\"]", buffer, &bufpos, bufsize, &strpos, offset);
    }
    if(uri_subresource==NULL || *uri_subresource=='\0') {
      plexi_reply_char_if_possible('}', buffer, &bufpos, bufsize, &strpos, offset);
    }
    if(bufpos > 0) {
      /* Build the header of the reply */
      REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
      /* Build the payload of the reply */
      REST.set_response_payload(response, buffer, bufpos);
    } else if(strpos > 0) {
      coap_set_status_code(response, BAD_OPTION_4_02);
      coap_set_payload(response, "BlockOutOfScope", 15);
    }
    if(strpos < *offset + bufsize) {
      *offset = -1;
    } else {
      *offset += bufsize;
    }
  } else {
    coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
    return;
  }
}
