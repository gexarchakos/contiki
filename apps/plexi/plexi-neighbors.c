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
 *         PLEXI RPL DoDAG resource interface (implementation file)
 *
 * \brief  Neighbor list resource provides access to the list of nodes in range.
 *
 * \details This resource refers to the MAC-layer neighbors. This list might not be
 *         equivalent to TSCH-layer neighbors. That is, it might contain nodes that
 *         is not scheduled to communicate with. Vice versa, the schedule might contain
 *         links to neighbors that are not momentarily or ever reachable.
 *
 *         However, even if the node is not scheduled to communicate with certain neighbor,
 *         the negihbor has to be able to send EBs and the node to receive them. That means,
 *         that the neighbor list contains reachable neighbors which have joined the TSCH network
 *         and can send EBs on the proper cells.
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 * \author Ilker Oztelcan <i.oztelcan@tue.nl>
 * \author Dimitris Sarakiotis <d.sarakiotis@tue.nl>
 *
 */

#include "plexi-interface.h"

#include "plexi.h"

#include "er-coap-engine.h"
#include "net/ip/uip-debug.h"

#ifndef PLEXI_NEIGHBOR_UPDATE_INTERVAL
/**
 * \brief plexi notifies observers of neighbor list every \ref PLEXI_NEIGHBOR_UPDATE_INTERVAL secs
 */
#define PLEXI_NEIGHBOR_UPDATE_INTERVAL      (10 * CLOCK_SECOND)
#endif

#if PLEXI_WITH_LINK_STATISTICS

#include "plexi-link-statistics.h"

struct aggregate_stats_struct {
  int rssi, lqi, etx, pdr;
  unsigned int asn;
  int rssi_counter, lqi_counter, asn_counter, etx_counter, pdr_counter;
};

/** \brief Α module-scope temporary structure to keep track of help the aggregation of the link-oriented statistics into neighbor-wide statistics
 *
 * Structure populated by \ref aggregate_statistics. Its lifetime is a single execution of \ref plexi_get_neighbors_handler. Once \ref plexi_get_neighbors_handler is called again, the structure is re-initialized.
 */
static struct aggregate_stats_struct temp_aggregate_stats = { .rssi = (int)0xFFFFFFFFFFFFFFFF, .lqi = -1, .etx = -1, .pdr = -1, .asn = -1, .rssi_counter = 0, .lqi_counter = 0, .asn_counter = 0, .etx_counter = 0, .pdr_counter = 0 };

/**
 * \brief Aggregates statistics metric values per link for each neighbor.
 * \details Statistics maintained for every link with a neighbor are aggregated by this function. Values for the same metric update an exponential window moving average that will represent the value of that metric for a specific neighbor.
 * However, NEIGHBOR_ASN_LABEL represents the latest communication received by the neighbor. Hence, EWMA is not applicable. Instead,
 * the function replaces the old value with a newer one.
 * The neighbor is detected via the statistics metrics identifier.
 * \param id statistics metric identifier (see YANG model <a href="https://tools.ietf.org/html/draft-ietf-6tisch-6top-interface-04"> 6TiSCH Operation Sublayer (6top) Interface</a> and \ref plexi-link-statistics.h)
 * \param metric the metric the values of which will be aggregated (see \ref STATS_ETX_LABEL, \ref STATS_PDR_LABEL, \ref STATS_RSSI_LABEL, \ref STATS_LQI_LABEL, \ref NEIGHBORS_ASN_LABEL)
 * \param value the value to be aggregated with previous values on the same metric on the same neighbor
 *
 * \bug could metric be also retrieved by the statistics metric identifier?
 */
void aggregate_statistics(uint16_t id, uint8_t metric, plexi_stats_value_st value);

#endif

/**
 * \brief Notifies subscribers to neighbor list of any changes in the neighbor list.
 *
 * \bug The implementation does not guarantee detection of node removal or rewiring. Avoid relying on uip_ds6_nbr
 */
static void plexi_neighbors_event_handler(void);

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
static void plexi_get_neighbors_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

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
#ifdef PLEXI_NEIGHBOR_UPDATE_INTERVAL
   PARENT_PERIODIC_RESOURCE(resource_6top_nbrs,            /* name */
#else
   PARENT_EVENT_RESOURCE(resource_6top_nbrs,            /* name */
#endif
                         "obs;title=\"6top neighbours\"", /* attributes */
                         plexi_get_neighbors_handler, /* GET handler */
                         NULL,      /* POST handler */
                         NULL,      /* PUT handler */
                         NULL,      /* DELETE handler */
#ifdef PLEXI_NEIGHBOR_UPDATE_INTERVAL
                         PLEXI_NEIGHBOR_UPDATE_INTERVAL,
#endif
                         plexi_neighbors_event_handler); /* EVENT handler */

static void
plexi_get_neighbors_handler(void *request, void *response, uint8_t *buffer, uint16_t bufsize, int32_t *offset)
{
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);
  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {
    size_t strpos = 0;            /* position in overall string (which is larger than the buffer) */
    size_t bufpos = 0;            /* position within buffer (bytes written) */
    int32_t local_offset = 0;
    if(offset == NULL) {
      offset = &local_offset;
      *offset = 0;
    }

    char *uri_path = NULL;
    const char *query = NULL;
    int uri_len = REST.get_url(request, (const char **)(&uri_path));
    int base_len = 0, query_value_len = 0;
    char *uri_subresource = NULL, *query_value = NULL;
    linkaddr_t tna;
    linkaddr_copy(&tna,&linkaddr_null);

    if(uri_len > 0) {
      *(uri_path + uri_len) = '\0';
      base_len = strlen(resource_6top_nbrs.url);
      uri_subresource = uri_path + base_len;
      if(*uri_subresource == '/') {
        uri_subresource++;
      }
      query_value_len = REST.get_query_variable(request, NEIGHBORS_TNA_LABEL, (const char **)(&query_value));
      if(query_value) {
        *(query_value + query_value_len) = '\0';
        int success = plexi_string_to_linkaddr(query_value, query_value_len, &tna);
        if(!success) {
          coap_set_status_code(response, BAD_REQUEST_4_00);
          coap_set_payload(response, "Bad node address format", 23);
          return;
        }
      }
    }
#if PLEXI_WITH_LINK_STATISTICS
    if((uri_len > base_len + 1 && strcmp(NEIGHBORS_TNA_LABEL, uri_subresource) \
        && strcmp(STATS_RSSI_LABEL, uri_subresource) \
        && strcmp(STATS_LQI_LABEL, uri_subresource) \
        && strcmp(STATS_ETX_LABEL, uri_subresource) \
        && strcmp(STATS_PDR_LABEL, uri_subresource) \
        && strcmp(NEIGHBORS_ASN_LABEL, uri_subresource) \
        ) || (query && !query_value)) {
      coap_set_status_code(response, BAD_REQUEST_4_00);
      coap_set_payload(response, "Supports only queries on neighbor address", 41);
      return;
    }
#else
    if((uri_len > base_len + 1 && strcmp(NEIGHBORS_TNA_LABEL, uri_subresource)) \
       || (query && !query_value)) {
      coap_set_status_code(response, BAD_REQUEST_4_00);
      coap_set_payload(response, "Supports only queries on neighbor address", 41);
      return;
    }
#endif
    if(linkaddr_cmp(&tna, &linkaddr_null)) {
      plexi_reply_char_if_possible('[', buffer, &bufpos, bufsize, &strpos, offset);
    }
    uip_ds6_nbr_t *nbr;
    int first_item = 1;
    uip_ipaddr_t *last_next_hop = NULL;
    uip_ipaddr_t *curr_next_hop = NULL;
    for(nbr = nbr_table_head(ds6_neighbors); nbr != NULL; nbr = nbr_table_next(ds6_neighbors, nbr)) {
      curr_next_hop = (uip_ipaddr_t *)uip_ds6_nbr_get_ipaddr(nbr);
      linkaddr_t *lla = (linkaddr_t *)uip_ds6_nbr_get_ll(nbr);
      if(curr_next_hop != last_next_hop) {
        if(first_item) {
          first_item = 0;
        } else {
          plexi_reply_char_if_possible(',', buffer, &bufpos, bufsize, &strpos, offset);
        }

        if(!strcmp(NEIGHBORS_TNA_LABEL, uri_subresource)) {
          plexi_reply_char_if_possible('"', buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_lladdr_if_possible(lla, buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_char_if_possible('"', buffer, &bufpos, bufsize, &strpos, offset);
        } else {
#if PLEXI_WITH_LINK_STATISTICS
          temp_aggregate_stats = (struct aggregate_stats_struct){.rssi = (int)0xFFFFFFFFFFFFFFFF, .lqi = -1, .etx = -1, .pdr = -1, .asn = -1, .rssi_counter = 0, .lqi_counter = 0, .asn_counter = 0, .etx_counter = 0, .pdr_counter = 0 };
          struct tsch_slotframe *slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(NULL);
          while(slotframe) {
            struct tsch_link *link = (struct tsch_link *)tsch_schedule_get_link_next(slotframe, NULL);
            while(link) {
              plexi_execute_over_link_stats(aggregate_statistics, link, lla);
              link = (struct tsch_link *)tsch_schedule_get_link_next(slotframe, link);
            }
            slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(slotframe);
          }
          if(temp_aggregate_stats.rssi < (int)0xFFFFFFFFFFFFFFFF && !strcmp(STATS_RSSI_LABEL, uri_subresource)) {
            plexi_reply_uint16_if_possible((uint16_t)(temp_aggregate_stats.rssi / temp_aggregate_stats.rssi_counter),\
               buffer, &bufpos, bufsize, &strpos, offset);
          } else if(temp_aggregate_stats.lqi > -1 && !strcmp(STATS_LQI_LABEL, uri_subresource)) {
            plexi_reply_uint16_if_possible((uint16_t)(temp_aggregate_stats.lqi / temp_aggregate_stats.lqi_counter),\
               buffer, &bufpos, bufsize, &strpos, offset);
          } else if(temp_aggregate_stats.etx > -1 && !strcmp(STATS_ETX_LABEL, uri_subresource)) {
            plexi_reply_uint16_if_possible((uint16_t)(temp_aggregate_stats.etx / 256 / temp_aggregate_stats.etx_counter),\
               buffer, &bufpos, bufsize, &strpos, offset);
          } else if(temp_aggregate_stats.pdr > -1 && !strcmp(STATS_PDR_LABEL, uri_subresource)) {
            plexi_reply_uint16_if_possible((uint16_t)(temp_aggregate_stats.pdr / temp_aggregate_stats.pdr_counter),\
               buffer, &bufpos, bufsize, &strpos, offset);
          } else if(temp_aggregate_stats.asn > -1 && !strcmp(NEIGHBORS_ASN_LABEL, uri_subresource)) {
            plexi_reply_char_if_possible('\"', buffer, &bufpos, bufsize, &strpos, offset);
            plexi_reply_hex_if_possible(temp_aggregate_stats.asn, buffer, &bufpos, bufsize, &strpos, offset,1);
            plexi_reply_char_if_possible('\"', buffer, &bufpos, bufsize, &strpos, offset);
          } else if(base_len == uri_len) {
            plexi_reply_string_if_possible("{\"", buffer, &bufpos, bufsize, &strpos, offset);
            plexi_reply_string_if_possible(NEIGHBORS_TNA_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
            plexi_reply_string_if_possible("\":\"", buffer, &bufpos, bufsize, &strpos, offset);
            plexi_reply_lladdr_if_possible(lla, buffer, &bufpos, bufsize, &strpos, offset);
            plexi_reply_char_if_possible('\"', buffer, &bufpos, bufsize, &strpos, offset);
            if(temp_aggregate_stats.rssi < (int)0xFFFFFFFFFFFFFFFF) {
              plexi_reply_string_if_possible(",\"", buffer, &bufpos, bufsize, &strpos, offset);
              plexi_reply_string_if_possible(STATS_RSSI_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
              plexi_reply_string_if_possible("\":", buffer, &bufpos, bufsize, &strpos, offset);
              plexi_reply_uint16_if_possible((uint16_t)(temp_aggregate_stats.rssi / temp_aggregate_stats.rssi_counter),\
               buffer, &bufpos, bufsize, &strpos, offset);
            }
            if(temp_aggregate_stats.lqi > -1) {
              plexi_reply_string_if_possible(",\"", buffer, &bufpos, bufsize, &strpos, offset);
              plexi_reply_string_if_possible(STATS_LQI_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
              plexi_reply_string_if_possible("\":", buffer, &bufpos, bufsize, &strpos, offset);
              plexi_reply_uint16_if_possible((uint16_t)(temp_aggregate_stats.lqi / temp_aggregate_stats.lqi_counter),\
               buffer, &bufpos, bufsize, &strpos, offset);
            }
            if(temp_aggregate_stats.etx > -1) {
              plexi_reply_string_if_possible(",\"", buffer, &bufpos, bufsize, &strpos, offset);
              plexi_reply_string_if_possible(STATS_ETX_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
              plexi_reply_string_if_possible("\":", buffer, &bufpos, bufsize, &strpos, offset);
              plexi_reply_uint16_if_possible((uint16_t)(temp_aggregate_stats.etx / 256 / temp_aggregate_stats.etx_counter),\
               buffer, &bufpos, bufsize, &strpos, offset);
            }
            if(temp_aggregate_stats.pdr > -1) {
              plexi_reply_string_if_possible(",\"", buffer, &bufpos, bufsize, &strpos, offset);
              plexi_reply_string_if_possible(STATS_PDR_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
              plexi_reply_string_if_possible("\":", buffer, &bufpos, bufsize, &strpos, offset);
              plexi_reply_uint16_if_possible((uint16_t)(temp_aggregate_stats.pdr / temp_aggregate_stats.pdr_counter),\
               buffer, &bufpos, bufsize, &strpos, offset);
            }
            if(temp_aggregate_stats.asn > -1) {
              plexi_reply_string_if_possible(",\"", buffer, &bufpos, bufsize, &strpos, offset);
              plexi_reply_string_if_possible(NEIGHBORS_ASN_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
              plexi_reply_string_if_possible("\":\"", buffer, &bufpos, bufsize, &strpos, offset);
              plexi_reply_hex_if_possible(temp_aggregate_stats.asn, buffer, &bufpos, bufsize, &strpos, offset,1);
              plexi_reply_char_if_possible('\"', buffer, &bufpos, bufsize, &strpos, offset);
            }
            plexi_reply_char_if_possible('}', buffer, &bufpos, bufsize, &strpos, offset);
          }
#else
          if(base_len == uri_len) {
            plexi_reply_string_if_possible("{\"", buffer, &bufpos, bufsize, &strpos, offset);
            plexi_reply_string_if_possible(NEIGHBORS_TNA_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
            plexi_reply_string_if_possible("\":\"", buffer, &bufpos, bufsize, &strpos, offset);
            plexi_reply_lladdr_if_possible(lla, buffer, &bufpos, bufsize, &strpos, offset);
            plexi_reply_string_if_possible("\"}", buffer, &bufpos, bufsize, &strpos, offset);
          }
#endif
        }
      }
    }
    if(linkaddr_cmp(&tna, &linkaddr_null)) {
      plexi_reply_char_if_possible(']', buffer, &bufpos, bufsize, &strpos, offset);
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
    if(strpos <= *offset + bufsize) {
      *offset = -1;
    } else {
      *offset += bufsize;
    }
  } else {
    coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
    return;
  }
}

/**
 * \brief Notifies periodically all clients who observe the neighbor list resource
 */
static void
plexi_neighbors_event_handler(void)
{
  REST.notify_subscribers(&resource_6top_nbrs);
}
/** \brief Wait for 30s without activity before notifying subscribers
 */
static struct ctimer route_changed_timer;

/**
 * \brief Notifies all clients who observe changes to the neighbor list resource
 */
static void
plexi_neighbors_changed_handler(void *ptr)
{
  REST.notify_subscribers(&resource_6top_nbrs);
}
/**
 * \brief Callback function to be called when a change to the DAG_RESOURCE resource has occurred.
 * Any change is delayed 30seconds before it is propagated to the observers.
 *
 * \bug UIP+DS6_NOTIFICATION_* do not provide a reliable way to listen to events
 */
void
route_changed_callback(int event, uip_ipaddr_t *route, uip_ipaddr_t *ipaddr, int num_routes)
{
  /* We have added or removed a routing entry, notify subscribers */
  if(event == UIP_DS6_NOTIFICATION_ROUTE_ADD || event == UIP_DS6_NOTIFICATION_ROUTE_RM) {
    PRINTF("PLEXI: notifying observers of rpl/dag resource \n");/* setting route_changed callback with 30s delay\n"); */
    ctimer_set(&route_changed_timer, 30 * CLOCK_SECOND, plexi_neighbors_changed_handler, NULL);
  }
}


#if PLEXI_WITH_LINK_STATISTICS
void
aggregate_statistics(uint16_t id, uint8_t metric, plexi_stats_value_st value)
{
  if(value != -1) {
    if(metric == RSSI) {
      if(temp_aggregate_stats.rssi == (int)0xFFFFFFFFFFFFFFFF) {
        temp_aggregate_stats.rssi = value;
      } else {
        temp_aggregate_stats.rssi += value;
      }
      temp_aggregate_stats.rssi_counter++;
    } else if(metric == LQI) {
      if(temp_aggregate_stats.lqi == -1) {
        temp_aggregate_stats.lqi = value;
      } else {
        temp_aggregate_stats.lqi += value;
      }
      temp_aggregate_stats.lqi_counter++;
    } else if(metric == ETX) {
      if(temp_aggregate_stats.etx == -1) {
        temp_aggregate_stats.etx = (int)value;
      } else {
        temp_aggregate_stats.etx += (int)value;
      }
      temp_aggregate_stats.etx_counter++;
    } else if(metric == PDR) {
      if(temp_aggregate_stats.pdr == -1) {
        temp_aggregate_stats.pdr = value;
      } else {
        temp_aggregate_stats.pdr += value;
      }
      temp_aggregate_stats.pdr_counter++;
    } else if(metric == ASN) {
      if(temp_aggregate_stats.asn == -1 || value > temp_aggregate_stats.asn) {
        temp_aggregate_stats.asn = value;
      }
      temp_aggregate_stats.asn_counter++;
    }
  }
}
#endif
