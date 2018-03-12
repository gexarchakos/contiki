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
 *         plexi-link module: plexi interface for TSCH configuration (links) - implementation file.
 *         All declarations and definitions in this file are only active iff \link PLEXI_WITH_LINK_RESOURCE \endlink
 *         is defined and set to non-zero
 *
 * \brief
 *         Defines the TSCH link resource and its GET, DEL and POST handlers.
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 * \author Ilker Oztelcan <i.oztelcan@tue.nl>
 * \author Dimitris Sarakiotis <d.sarakiotis@tue.nl>
 *
 */

#include "plexi-interface.h"
#include "plexi.h"

#if PLEXI_WITH_LINK_STATISTICS
#include "plexi-link-statistics.h"
#endif

#include "er-coap-engine.h"
#include "er-coap-block1.h"
#include "net/mac/tsch/tsch-schedule.h"
#include "net/ip/uip-debug.h"
#include "../rest-engine/rest-engine.h"

#include <stdlib.h>

void plexi_reply_link_if_possible(const struct tsch_link *link, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset);
uint8_t plexi_reply_tna_if_possible(const linkaddr_t *tna, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset);

/**
 * \brief Retrieves existing link(s) upon a CoAP GET request to TSCH link resource.
 *
 * The handler reacts to requests on the following URLs:
 * - base returning an array of the complete list of links of all slotframes in json array format \code{http} GET /LINK_RESOURCE -> e.g. [{LINK_ID_LABEL:8,FRAME_ID_LABEL:1,LINK_SLOT_LABEL:3,LINK_CHANNEL_LABEL:5,LINK_OPTION_LABEL:0,LINK_TYPE_LABEL:0},{LINK_ID_LABEL:9,FRAME_ID_LABEL:3,LINK_SLOT_LABEL:4,LINK_CHANNEL_LABEL:5,"option"1:,LINK_TYPE_LABEL:0}]\endcode
 * - subresources returning json arrays with the values of the specified subresource for all slotframes:
 *   - \code{http} GET /LINK_RESOURCE/LINK_ID_LABEL -> a json array of identifiers (one per link) e.g. [8,9]\endcode
 *   - \code{http} GET /LINK_RESOURCE/FRAME_ID_LABEL -> a json array of slotframe identifiers the links belong to (one per link) e.g. [1,3]\endcode Note, that the array is of size equal to the number of links. It is not a list of unique sloframe identifiers.
 *   - \code{http} GET /LINK_RESOURCE/LINK_SLOT_LABEL -> a json array of slotoffsets the links have been allocated to (one per link) e.g. [3,4]\endcode. Note, again, this is not a list of unique slotoffsets.
 *   - \code{http} GET /LINK_RESOURCE/LINK_CHANNEL_LABEL -> a json array of channeloffsets the links have been allocated to (one per link) e.g. [5,5]\endcode
 *   - \code{http} GET /LINK_RESOURCE/LINK_OPTION_LABEL -> a json array of link options the links belong to (one per link) e.g. [0,1]\endcode
 *   - \code{http} GET /LINK_RESOURCE/LINK_TYPE_LABEL -> a json array of link types (one per link) e.g. [0,0]\endcode
 *   - \code{http} GET /LINK_RESOURCE/LINK_STATS_LABEL -> a json array of statistics json objects kept per link e.g. [{"id":1,"value":5},{"id":2,"value":111}]\endcode Note the statistics identifier is unique in the whole system i.e. two different statistics in same link or different links in same or different slotframe are also different..
 * - queries returning the complete link objects of a subset of links specified by the queries. Links can be queried by eitehr their id xor any combination of the following subresources: slotframe, slotoffset and/or channeloffset. It returns either a complete link object or an array with the complete link json objects that fulfill the queries:
 *   - \code{http} GET /LINK_RESOURCE?LINK_SLOT_LABEL=3 -> a json array of link objects allocated to specific slotoffset e.g. [{LINK_ID_LABEL:8,FRAME_ID_LABEL:1,LINK_SLOT_LABEL:3,LINK_CHANNEL_LABEL:5,LINK_OPTION_LABEL:0,LINK_TYPE_LABEL:0}]\endcode
 *   - \code{http} GET /LINK_RESOURCE?LINK_CHANNEL_LABEL=5 -> a json array of link objects allocated to specific channeloffset e.g. [{LINK_ID_LABEL:8,FRAME_ID_LABEL:1,LINK_SLOT_LABEL:3,LINK_CHANNEL_LABEL:5,LINK_OPTION_LABEL:0,LINK_TYPE_LABEL:0},{LINK_ID_LABEL:9,FRAME_ID_LABEL:3,LINK_SLOT_LABEL:4,LINK_CHANNEL_LABEL:5,"option"1:,LINK_TYPE_LABEL:0}]\endcode
 *   - \code{http} GET /LINK_RESOURCE?FRAME_ID_LABEL=1 -> a json array of link objects allocated in specific slotframe e.g. [{LINK_ID_LABEL:8,FRAME_ID_LABEL:1,LINK_SLOT_LABEL:3,LINK_CHANNEL_LABEL:5,LINK_OPTION_LABEL:0,LINK_TYPE_LABEL:0}]\endcode
 *   - Combination of any of the queries above, simply, makes the choice more specific. Any two or all three of the queries above are possible. For example:
 *     \code{http} GET /LINK_RESOURCE?FRAME_ID_LABEL=1&LINK_CHANNEL_LABEL=5 -> a json array of link objects allocated in specific slotframe and channeloffset e.g. [{LINK_ID_LABEL:8,FRAME_ID_LABEL:1,LINK_SLOT_LABEL:3,LINK_CHANNEL_LABEL:5,LINK_OPTION_LABEL:0,LINK_TYPE_LABEL:0}]\endcode
 *     If combination of all three is queried, the link is uniquely identified; hence, the response (if any) is a single json link object. For example: \code{http} GET /LINK_RESOURCE?FRAME_ID_LABEL=1&LINK_CHANNEL_LABEL=5&LINK_SLOT_LABEL=3 -> a json array of link objects allocated in specific slotframe, slotoffset and channeloffset e.g. {LINK_ID_LABEL:8,FRAME_ID_LABEL:1,LINK_SLOT_LABEL:3,LINK_CHANNEL_LABEL:5,LINK_OPTION_LABEL:0,LINK_TYPE_LABEL:0}\endcode
 *   - \code{http} GET /LINK_RESOURCE?LINK_ID_LABEL=8 -> a json link object with specific identifier e.g. {LINK_ID_LABEL:8,FRAME_ID_LABEL:1,LINK_SLOT_LABEL:3,LINK_CHANNEL_LABEL:5,LINK_OPTION_LABEL:0,LINK_TYPE_LABEL:0}\endcode
 * - subresources and queries. If subresources is included in the resource path together with the queries, the response includes the values of the given subresource of those link objects selected by the queries. Any of the subresources presented above may be used. The returned values will be included in a json array, even if a link is uniquely identified by a query triplet.
 *
 *
 * \sa apps/rest-engine/rest-engine.h for more information on the handler signatures
 */
static void plexi_get_links_handler(void *request, void *response, uint8_t *buffer, uint16_t bufsize, int32_t *offset);
/** \brief Deletes an existing link upon a CoAP DEL request and returns the deleted objects.
 *
 * Handler to request the deletion of all links or specific ones via a query:
 * - base URL to wipe out all links
 *   \code DEL /LINK_RESOURCE -> json array with all link objects e.g. [{LINK_ID_LABEL:8,FRAME_ID_LABEL:1,LINK_SLOT_LABEL:3,LINK_CHANNEL_LABEL:5,LINK_OPTION_LABEL:0,LINK_TYPE_LABEL:0},{LINK_ID_LABEL:9,FRAME_ID_LABEL:3,LINK_SLOT_LABEL:4,LINK_CHANNEL_LABEL:5,"option"1:,LINK_TYPE_LABEL:0}] \endcode
 *   \warning Deleting all links will cause the node disconnect from the network. A disconnected node with no links installed cannot be recovered unless an internal algorithm resets at least a 6tisch minimal configuration or a cell to be used for EBs
 * - queries returning the complete link objects of a subset of links specified by the queries. Links can be queried by any combination of the following subresources: slotframe, slotoffset and/or channeloffset. It returns either a complete link object or an array with the complete link json objects that fulfill the queries:
 *   - \code{http} DEL /LINK_RESOURCE?LINK_SLOT_LABEL=3 -> a json array of link objects allocated to specific slotoffset e.g. [{LINK_ID_LABEL:8,FRAME_ID_LABEL:1,LINK_SLOT_LABEL:3,LINK_CHANNEL_LABEL:5,LINK_OPTION_LABEL:0,LINK_TYPE_LABEL:0}]\endcode
 *   - \code{http} DEL /LINK_RESOURCE?LINK_CHANNEL_LABEL=5 -> a json array of link objects allocated to specific channeloffset e.g. [{LINK_ID_LABEL:8,FRAME_ID_LABEL:1,LINK_SLOT_LABEL:3,LINK_CHANNEL_LABEL:5,LINK_OPTION_LABEL:0,LINK_TYPE_LABEL:0},{LINK_ID_LABEL:9,FRAME_ID_LABEL:3,LINK_SLOT_LABEL:4,LINK_CHANNEL_LABEL:5,"option"1:,LINK_TYPE_LABEL:0}]\endcode
 *   - \code{http} DEL /LINK_RESOURCE?FRAME_ID_LABEL=1 -> a json array of link objects allocated in specific slotframe e.g. [{LINK_ID_LABEL:8,FRAME_ID_LABEL:1,LINK_SLOT_LABEL:3,LINK_CHANNEL_LABEL:5,LINK_OPTION_LABEL:0,LINK_TYPE_LABEL:0}]\endcode
 *   - Combination of any of the queries above, simply, makes the choice more specific. Any two or all three of the queries above are possible. For example:
 *     \code{http} DEL /LINK_RESOURCE?FRAME_ID_LABEL=1&LINK_CHANNEL_LABEL=5 -> a json array of link objects allocated in specific slotframe and channeloffset e.g. [{LINK_ID_LABEL:8,FRAME_ID_LABEL:1,LINK_SLOT_LABEL:3,LINK_CHANNEL_LABEL:5,LINK_OPTION_LABEL:0,LINK_TYPE_LABEL:0}]\endcode
 *     If combination of all three is queried, the link is uniquely identified; hence, the response (if any) is a single json link object. For example: \code{http} GET /LINK_RESOURCE?FRAME_ID_LABEL=1&LINK_CHANNEL_LABEL=5&LINK_SLOT_LABEL=3 -> a json array of link objects allocated in specific slotframe, slotoffset and channeloffset e.g. {LINK_ID_LABEL:8,FRAME_ID_LABEL:1,LINK_SLOT_LABEL:3,LINK_CHANNEL_LABEL:5,LINK_OPTION_LABEL:0,LINK_TYPE_LABEL:0}\endcode
 *
 * \note: subresources or more generic queries are not supported. e.g. To delete all links with specific link option or type
 * (i.e. DEL /ref LINK_RESOURCE?\ref FRAME_ID_LABEL=1\&\ref LINK_OPTION_LABEL=1) is not yet supported. To achieve the same combine:
 * -# \code GET /LINK_RESOURCE?FRAME_ID_LABEL=1&LINK_OPTION_LABEL=1 -> an array of complete link objects e.g. [x,y,z]\endcode
 * -# \code for i in [x,y,z]:
 *     DEL /LINK_RESOURCE?FRAME_ID_LABEL=i.frame&LINK_SLOT_LABEL=i.slot&LINK_CHANNEL_LABEL=i.channel
 *  \endcode
 *
 * \sa apps/rest-engine/rest-engine.h for more information on the handler signatures
 */
static void plexi_delete_links_handler(void *request, void *response, uint8_t *buffer, uint16_t bufsize, int32_t *offset);
/**
 * \brief Installs a new TSCH link upon a CoAP POST request and returns the link identifier is successful.
 *
 * The handler reacts to requests on the base URL only. Each request carries in payload the complete json object of one only link.
 * Installs one link with the attributes. The link identifier is set by lower layers and returned as a scalar array.
 * i.e. \code{http} POST /LINK_RESOURCE - Payload: {FRAME_ID_LABEL:1,LINK_SLOT_LABEL:6,LINK_CHANNEL_LABEL:5,"option"1:,LINK_TYPE_LABEL:0} -> [12]\endcode
 *
 * \note For now, posting multiple links is not supported.
 *
 * \sa apps/rest-engine/rest-engine.h for more information on the handler signatures
 */
static void plexi_post_links_handler(void *request, void *response, uint8_t *buffer, uint16_t bufsize, int32_t *offset);

/*---------------------------------------------------------------------------*/

static int inbox_post_link_lock = PLEXI_REQUEST_CONTENT_UNLOCKED;
static unsigned char inbox_post_link[MAX_DATA_LEN];
static size_t inbox_post_link_len = 0;

#if PLEXI_WITH_LINK_STATISTICS

typedef struct plexi_print_struct plexi_print_state;
struct plexi_print_struct {
  uint8_t *buffer;
  size_t *bufpos;
  uint16_t bufsize;
  size_t *strpos;
  int32_t *offset;
};

static uint8_t first_stat = 1;
static void plexi_reply_link_stats_if_possible(uint16_t id, uint8_t metric, plexi_stats_value_st value, void* print_state);
#endif

static uint16_t new_tx_slotframe = 0;
static uint16_t new_tx_timeslot = 0;

/*---------------------------------------------------------------------------*/

/**
 * \brief Link Resource to GET, POST or DELETE links. POST is substituting PUT, too. Not observable resource to retrieve, create and delete links.
 *
 * Links are objects consisting of six attributes: an identifier, the slotframe, the slotoffset, the channel offset, the tranception option and the type.
 * A link object is addressed via the URL set in \ref LINK_RESOURCE within plexi-interface.h.
 * According to the YANG model <a href="https://tools.ietf.org/html/draft-ietf-6tisch-6top-interface-04"> 6TiSCH Operation Sublayer (6top) Interface</a>,
 * Each link is a json object like:
 * \code{.json}
 *  {
 *    LINK_ID_LABEL: a uint16 to identify each link
 *    FRAME_ID_LABEL: a uint8 to identify the slotframe the link belongs to
 *    LINK_SLOT_LABEL: a uint16 representing the number of slots from the beginning of the slotframe the link is located at
 *    LINK_CHANNEL_LABEL: a uint16 representing the number of logical channels from the beginning of the slotframe the link is located at
 *    LINK_OPTION_LABEL: 4 bit flags to specify transmitting, receiving, shared or timekeeping link
 *    LINK_TYPE_LABEL: a flag to specify a normal or advertising link
 *  }
 * \endcode
 */
PARENT_RESOURCE(resource_6top_links,    /* name */
                "title=\"6top links\"", /* attributes */
                plexi_get_links_handler, /* GET handler */
                plexi_post_links_handler, /* POST handler */
                NULL,   /* PUT handler */
                plexi_delete_links_handler); /* DELETE handler */

void
plexi_reply_link_if_possible(const struct tsch_link *link, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset)
{
  plexi_reply_char_if_possible('"', buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible(LINK_ID_LABEL, buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible("\":", buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_uint16_if_possible(link->handle, buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible(",\"", buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible(FRAME_ID_LABEL, buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible("\":", buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_uint16_if_possible(link->slotframe_handle, buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible(",\"", buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible(LINK_SLOT_LABEL, buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible("\":", buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_uint16_if_possible(link->timeslot, buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible(",\"", buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible(LINK_CHANNEL_LABEL, buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible("\":", buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_uint16_if_possible(link->channel_offset, buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible(",\"", buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible(LINK_OPTION_LABEL, buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible("\":", buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_uint16_if_possible(link->link_options, buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible(",\"", buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible(LINK_TYPE_LABEL, buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible("\":", buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_uint16_if_possible(link->link_type, buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_tna_if_possible(&link->addr, buffer, bufpos, bufsize, strpos, offset);
}

uint8_t
plexi_reply_tna_if_possible(const linkaddr_t* tna, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset)
{
  if(!linkaddr_cmp(tna, &linkaddr_null))
  {
    plexi_reply_string_if_possible(",\"", buffer, bufpos, bufsize, strpos, offset);
    plexi_reply_string_if_possible(NEIGHBORS_TNA_LABEL, buffer, bufpos, bufsize, strpos, offset);
    plexi_reply_string_if_possible("\":\"", buffer, bufpos, bufsize, strpos, offset);
    plexi_reply_lladdr_if_possible(tna, buffer, bufpos, bufsize, strpos, offset);
    plexi_reply_char_if_possible('"', buffer, bufpos, bufsize, strpos, offset);
    return (uint8_t)1;
  }
  return (uint8_t)0;
}

static void
plexi_get_links_handler(void *request, void *response, uint8_t *buffer, uint16_t bufsize, int32_t *offset)
{
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);

  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {
    size_t strpos = 0;            /* position in overall string (which is larger than the buffer) */
    size_t bufpos = 0;            /* position within buffer (bytes written) */

    char *end;

    char *uri_path = NULL;
    const char *query = NULL;
    int uri_len = REST.get_url(request, (const char **)(&uri_path));
    int base_len = 0, query_len = 0, query_id_len = 0, query_frame_len = 0, query_slot_len = 0, query_channel_len = 0;
    char *query_id = NULL, *query_frame = NULL, *query_slot = NULL, *query_channel = NULL, *uri_subresource = NULL;
    unsigned long id = -1, frame = -1, slot = -1, channel = -1;
    short int flag = 0;

    if(uri_len > 0) {
      *(uri_path + uri_len) = '\0';
      base_len = strlen(resource_6top_links.url);

      /* Parse the query options and support only the id, the slotframe, the slotoffset and channeloffset queries */
      query_len = REST.get_query(request, &query);
      query_id_len = REST.get_query_variable(request, LINK_ID_LABEL, (const char **)(&query_id));
      query_frame_len = REST.get_query_variable(request, FRAME_ID_LABEL, (const char **)(&query_frame));
      query_slot_len = REST.get_query_variable(request, LINK_SLOT_LABEL, (const char **)(&query_slot));
      query_channel_len = REST.get_query_variable(request, LINK_CHANNEL_LABEL, (const char **)(&query_channel));

      if(query_id) {
        *(query_id + query_id_len) = '\0';
        id = (unsigned)strtoul(query_id, &end, 10);
        flag |= 8;
      }
      if(query_frame) {
        *(query_frame + query_frame_len) = '\0';
        frame = (unsigned)strtoul(query_frame, &end, 10);
        flag |= 4;
      }
      if(query_slot) {
        *(query_slot + query_slot_len) = '\0';
        slot = (unsigned)strtoul(query_slot, &end, 10);
        flag |= 2;
      }
      if(query_channel) {
        *(query_channel + query_channel_len) = '\0';
        channel = (unsigned)strtoul(query_channel, &end, 10);
        flag |= 1;
      }
      if(query_len > 0 && (!flag || (query_id && id < 0) || (query_frame && frame < 0) || (query_slot && slot < 0) || (query_channel && channel < 0))) {
        coap_set_status_code(response, NOT_IMPLEMENTED_5_01);
        coap_set_payload(response, "Supports queries only on slot frame id and/or slotoffset and channeloffset", 74);
        return;
      }

      /* Parse subresources and make sure you can filter the results */
      uri_subresource = uri_path + base_len;
      if(*uri_subresource == '/') {
        uri_subresource++;
      }
      if((uri_len > base_len + 1 && strcmp(LINK_ID_LABEL, uri_subresource) && strcmp(FRAME_ID_LABEL, uri_subresource) \
          && strcmp(LINK_SLOT_LABEL, uri_subresource) && strcmp(LINK_CHANNEL_LABEL, uri_subresource) \
          && strcmp(LINK_OPTION_LABEL, uri_subresource) && strcmp(LINK_TYPE_LABEL, uri_subresource) \
          && strcmp(NEIGHBORS_TNA_LABEL, uri_subresource) && strcmp(LINK_STATS_LABEL, uri_subresource))) {
        coap_set_status_code(response, NOT_FOUND_4_04);
        coap_set_payload(response, "Invalid subresource", 19);
        return;
      }
    } else {
      base_len = (int)strlen(LINK_RESOURCE);
      uri_len = (int)(base_len + 1 + strlen(LINK_STATS_LABEL));
      uri_subresource = LINK_STATS_LABEL;
    }
    struct tsch_slotframe *slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(NULL);
    int first_item = 1;
    while(slotframe) {
      if(!(flag & 4) || frame == slotframe->handle) {
        struct tsch_link *link = (struct tsch_link *)tsch_schedule_get_link_next(slotframe, NULL);
        while(link) {
          if((!(flag & 2) || slot == link->timeslot) && (!(flag & 1) || channel == link->channel_offset)) {
            if(!(flag & 8) || id == link->handle) {
              if(first_item) {
                if(flag < 7 || uri_len > base_len + 1) {
                  plexi_reply_char_if_possible('[', buffer, &bufpos, bufsize, &strpos, offset);
                }
                first_item = 0;
              } else {
                plexi_reply_char_if_possible(',', buffer, &bufpos, bufsize, &strpos, offset);
              }
              plexi_print_state ps = { .buffer=buffer,\
                .bufpos=&bufpos,\
                .bufsize=bufsize,\
                .strpos=&strpos,\
                .offset=offset};
              if(!strcmp(LINK_ID_LABEL, uri_subresource)) {
                plexi_reply_uint16_if_possible(link->handle, buffer, &bufpos, bufsize, &strpos, offset);
              } else if(!strcmp(FRAME_ID_LABEL, uri_subresource)) {
                plexi_reply_uint16_if_possible(link->slotframe_handle, buffer, &bufpos, bufsize, &strpos, offset);
              } else if(!strcmp(LINK_SLOT_LABEL, uri_subresource)) {
                plexi_reply_uint16_if_possible(link->timeslot, buffer, &bufpos, bufsize, &strpos, offset);
              } else if(!strcmp(LINK_CHANNEL_LABEL, uri_subresource)) {
                plexi_reply_uint16_if_possible(link->channel_offset, buffer, &bufpos, bufsize, &strpos, offset);
              } else if(!strcmp(LINK_OPTION_LABEL, uri_subresource)) {
                plexi_reply_uint16_if_possible(link->link_options, buffer, &bufpos, bufsize, &strpos, offset);
              } else if(!strcmp(LINK_TYPE_LABEL, uri_subresource)) {
                plexi_reply_uint16_if_possible(link->link_type, buffer, &bufpos, bufsize, &strpos, offset);
              } else if(!strcmp(NEIGHBORS_TNA_LABEL, uri_subresource)) {
                if(!plexi_reply_tna_if_possible(&link->addr, buffer, &bufpos, bufsize, &strpos, offset)) {
                  coap_set_status_code(response, NOT_FOUND_4_04);
                  coap_set_payload(response, "Link has no target node address.", 32);
                  return;
                }
              } else if(!strcmp(LINK_STATS_LABEL, uri_subresource)) {
#if PLEXI_WITH_LINK_STATISTICS
                first_stat = 1;
                if(!plexi_execute_over_link_stats(plexi_reply_link_stats_if_possible, link, NULL, (void*)(&ps))) {
#endif
                coap_set_status_code(response, NOT_FOUND_4_04);
                coap_set_payload(response, "No specified statistics was found", 33);
                return;
#if PLEXI_WITH_LINK_STATISTICS
              }
#endif
              } else {
                plexi_reply_char_if_possible('{', buffer, &bufpos, bufsize, &strpos, offset);
                plexi_reply_link_if_possible(link, buffer, &bufpos, bufsize, &strpos, offset);
#if PLEXI_WITH_LINK_STATISTICS
                int undo_bufpos = bufpos;
                int undo_strpos = strpos;
                plexi_reply_string_if_possible(",\"", buffer, &bufpos, bufsize, &strpos, offset);
                plexi_reply_string_if_possible(LINK_STATS_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
                plexi_reply_string_if_possible("\":[", buffer, &bufpos, bufsize, &strpos, offset);
                first_stat = 1;
                if(plexi_execute_over_link_stats(plexi_reply_link_stats_if_possible, link, NULL, (void*)(&ps))) {
                  plexi_reply_char_if_possible(']', buffer, &bufpos, bufsize, &strpos, offset);
                } else {
                  bufpos = undo_bufpos;
                  strpos = undo_strpos;
                }
#endif
                plexi_reply_char_if_possible('}', buffer, &bufpos, bufsize, &strpos, offset);
              }
            }
          }
          link = (struct tsch_link *)tsch_schedule_get_link_next(slotframe, link);
        }
      }
      slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(slotframe);
    }
    if(flag < 7 || uri_len > base_len + 1) {
      plexi_reply_char_if_possible(']', buffer, &bufpos, bufsize, &strpos, offset);
    }
    if(!first_item) {
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
      coap_set_status_code(response, NOT_FOUND_4_04);
      coap_set_payload(response, "No specified statistics resource not found", 42);
      return;
    }
  } else {
    coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
    return;
  }
}

static void
plexi_delete_links_handler(void *request, void *response, uint8_t *buffer, uint16_t bufsize, int32_t *offset)
{
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);

  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {
    size_t strpos = 0;            /* position in overall string (which is larger than the buffer) */
    size_t bufpos = 0;            /* position within buffer (bytes written) */

    char *end;

    char *uri_path = NULL;
    const char *query = NULL;
    int uri_len = REST.get_url(request, (const char **)(&uri_path));
    *(uri_path + uri_len) = '\0';
    int base_len = strlen(resource_6top_links.url);

    /* Parse the query options and support only the slotframe, the slotoffset and channeloffset queries */
    int query_len = REST.get_query(request, &query);
    char *query_frame = NULL, *query_slot = NULL, *query_channel = NULL;
    unsigned long frame = -1, slot = -1, channel = -1;
    uint8_t flags = 0;
    int query_frame_len = REST.get_query_variable(request, FRAME_ID_LABEL, (const char **)(&query_frame));
    int query_slot_len = REST.get_query_variable(request, LINK_SLOT_LABEL, (const char **)(&query_slot));
    int query_channel_len = REST.get_query_variable(request, LINK_CHANNEL_LABEL, (const char **)(&query_channel));
    if(query_frame) {
      *(query_frame + query_frame_len) = '\0';
      frame = (unsigned)strtoul(query_frame, &end, 10);
      flags |= 4;
    }
    if(query_slot) {
      *(query_slot + query_slot_len) = '\0';
      slot = (unsigned)strtoul(query_slot, &end, 10);
      flags |= 2;
    }
    if(query_channel) {
      *(query_channel + query_channel_len) = '\0';
      channel = (unsigned)strtoul(query_channel, &end, 10);
      flags |= 1;
    }
    if(query_len > 0 && (!flags || (query_frame && frame < 0) || (query_slot && slot < 0) || (query_channel && channel < 0))) {
      coap_set_status_code(response, NOT_IMPLEMENTED_5_01);
      coap_set_payload(response, "Supports queries only on slot frame id and/or slotoffset and channeloffset", 74);
      return;
    }

    /* Parse subresources and make sure you can filter the results */
    if(uri_len > base_len + 1) {
      coap_set_status_code(response, NOT_IMPLEMENTED_5_01);
      coap_set_payload(response, "Subresources are not supported for DELETE method", 48);
      return;
    }

    struct tsch_slotframe *slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(NULL);
    int first_item = 1;
    int tmp_bufpos = 0;
    int tmp_strpos = 0;
    int tmp_offset = 0;
    while(slotframe) {
      if(!(flags & 4) || frame == slotframe->handle) {
        struct tsch_link *link = (struct tsch_link *)tsch_schedule_get_link_next(slotframe, NULL);
        while(link) {
          struct tsch_link *next_link = (struct tsch_link *)tsch_schedule_get_link_next(slotframe, link);
//          int link_handle = link->handle;
//          int link_slotframe_handle = link->slotframe_handle;
//          int link_timeslot = link->timeslot;
//          int link_channel_offset = link->channel_offset;
//          int link_link_options = link->link_options;
//          int link_link_type = link->link_type;
          if((!(flags & 2) || link->timeslot == slot) && (!(flags & 1) || link->channel_offset == channel)) {
//            if(((!(flags & 2) && !(flags & 1)) || ((flags & 2) && !(flags & 1) && slot == link_timeslot) || (!(flags & 2) && (flags & 1) && channel == link_channel_offset) || ((flags & 2) && (flags & 1) && slot == link_timeslot && channel == link_channel_offset)) && deleted) {
            tmp_bufpos = bufpos;
            tmp_strpos = strpos;
            tmp_offset = *offset;
            if(first_item) {
              if(flags < 7) {
                plexi_reply_char_if_possible('[', buffer, &bufpos, bufsize, &strpos, offset);    
              }
              first_item = 0;
            } else {
              plexi_reply_char_if_possible(',', buffer, &bufpos, bufsize, &strpos, offset);    
            }
            plexi_reply_char_if_possible('{', buffer, &bufpos, bufsize, &strpos, offset);
            plexi_reply_link_if_possible(link, buffer, &bufpos, bufsize, &strpos, offset);
            plexi_reply_char_if_possible('}', buffer, &bufpos, bufsize, &strpos, offset);
            int deleted = tsch_schedule_remove_link(slotframe, link);
            if(!deleted)
            {
              bufpos = tmp_bufpos;
              strpos = tmp_strpos;
              *offset = tmp_offset;
            }
          }
          link = next_link;
        }
      }
      slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(slotframe);
    }
    if(flags < 7) {
      plexi_reply_char_if_possible(']', buffer, &bufpos, bufsize, &strpos, offset);
    }
    REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
    if(flags != 7 || !first_item) {
      if(bufpos > 0) {
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
    }
    coap_set_status_code(response, DELETED_2_02);
  } else {
    coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
    return;
  }
}

static void
plexi_post_links_handler(void *request, void *response, uint8_t *buffer, uint16_t bufsize, int32_t *offset)
{
  if(inbox_post_link_lock == PLEXI_REQUEST_CONTENT_UNLOCKED) {
    inbox_post_link_len = 0;
    *inbox_post_link = '\0';
  }

  int first_item = 1;
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);

  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {
    size_t strpos = 0;            /* position in overall string (which is larger than the buffer) */
    size_t bufpos = 0;            /* position within buffer (bytes written) */

    const uint8_t *request_content;
    int request_content_len;
    request_content_len = coap_get_payload(request, &request_content);
    if(inbox_post_link_len + request_content_len > MAX_DATA_LEN) {
      coap_set_status_code(response, NOT_IMPLEMENTED_5_01);
      coap_set_payload(response, "Server reached internal buffer limit. Shorten payload.", 54);
      return;
    }
    int x = coap_block1_handler(request, response, inbox_post_link, &inbox_post_link_len, MAX_DATA_LEN);
    if(inbox_post_link_len < MAX_DATA_LEN) {
      *(inbox_post_link + inbox_post_link_len) = '\0';
    }
    if(x == 1) {
      inbox_post_link_lock = PLEXI_REQUEST_CONTENT_LOCKED;
      return;
    } else if(x == -1) {
      inbox_post_link_lock = PLEXI_REQUEST_CONTENT_UNLOCKED;
      return;
    }
    /* TODO: It is assumed that the node processes the post request fast enough to return the */
    /*       response within the window assumed by client before retransmitting */
    inbox_post_link_lock = PLEXI_REQUEST_CONTENT_UNLOCKED;
    int i = 0;
    for(i = 0; i < inbox_post_link_len; i++) {
      if(inbox_post_link[i] == '[') {
        coap_set_status_code(response, BAD_REQUEST_4_00);
        coap_set_payload(response, "Array of links is not supported yet. POST each link separately.", 63);
        return;
      }
    }
    int state;

    int so = 0;   /* * slot offset * */
    int co = 0;   /* * channel offset * */
    int fd = 0;   /* * slotframeID (handle) * */
    int lo = 0;   /* * link options * */
    int lt = 0;   /* * link type * */
    linkaddr_t na;  /* * node address * */

    char field_buf[24] = "";
    char value_buf[24] = "";
    struct jsonparse_state js;
    jsonparse_setup(&js, (const char *)inbox_post_link, inbox_post_link_len);
    while((state = plexi_json_find_field(&js, field_buf, sizeof(field_buf)))) {
      switch(state) {
      case '{':   /* * New element * */
        so = co = fd = lo = lt = 0;
        linkaddr_copy(&na, &linkaddr_null);
        break;
      case '}': {   /* * End of current element * */
        struct tsch_slotframe *slotframe;
        struct tsch_link *link;
        slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_by_handle((uint16_t)fd);
        if(slotframe) {
          new_tx_timeslot = so;
          new_tx_slotframe = fd;
          if((link = (struct tsch_link *)tsch_schedule_add_link(slotframe, (uint8_t)lo, lt, &na, (uint16_t)so, (uint16_t)co))) {
            PRINTF("PLEXI: added {\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u", \
                   LINK_ID_LABEL, link->handle, FRAME_ID_LABEL, fd, LINK_SLOT_LABEL, so, \
                   LINK_CHANNEL_LABEL, co, LINK_OPTION_LABEL, lo, LINK_TYPE_LABEL, lt);
            if(!linkaddr_cmp(&na, &linkaddr_null)) {
              PRINTF(",\"%s\":\"", NEIGHBORS_TNA_LABEL);
              PRINTLLADDR((const uip_lladdr_t *)&na);
              PRINTF("\"");
            }
            PRINTF("}\n");
            /* * Update response * */
            if(!first_item) {
              plexi_reply_char_if_possible(',', buffer, &bufpos, bufsize, &strpos, offset);
            } else {
              plexi_reply_char_if_possible('[', buffer, &bufpos, bufsize, &strpos, offset);
            }
            first_item = 0;
            plexi_reply_uint16_if_possible(link->handle, buffer, &bufpos, bufsize, &strpos, offset);
          } else {
            coap_set_status_code(response, INTERNAL_SERVER_ERROR_5_00);
            coap_set_payload(response, "Link could not be added", 23);
            return;
          }
        } else {
          coap_set_status_code(response, NOT_FOUND_4_04);
          coap_set_payload(response, "Slotframe handle not found", 26);
          return;
        }
        break;
      }
      case JSON_TYPE_NUMBER:
        if(!strncmp(field_buf, LINK_SLOT_LABEL, sizeof(field_buf))) {
          so = jsonparse_get_value_as_int(&js);
        } else if(!strncmp(field_buf, LINK_CHANNEL_LABEL, sizeof(field_buf))) {
          co = jsonparse_get_value_as_int(&js);
        } else if(!strncmp(field_buf, FRAME_ID_LABEL, sizeof(field_buf))) {
          fd = jsonparse_get_value_as_int(&js);
        } else if(!strncmp(field_buf, LINK_OPTION_LABEL, sizeof(field_buf))) {
          lo = jsonparse_get_value_as_int(&js);
        } else if(!strncmp(field_buf, LINK_TYPE_LABEL, sizeof(field_buf))) {
          lt = jsonparse_get_value_as_int(&js);
        }
        break;
      case JSON_TYPE_STRING:
        printf("\njson=%s",js.json);
        if(!strncmp(field_buf, NEIGHBORS_TNA_LABEL, sizeof(field_buf))) {
          jsonparse_copy_value(&js, value_buf, sizeof(value_buf));
          if(!plexi_string_to_linkaddr(value_buf, sizeof(value_buf), &na)) {
            coap_set_status_code(response, BAD_REQUEST_4_00);
            coap_set_payload(response, "Invalid target node address", 27);
            return;
          }
        }
        break;
      }
    }
    plexi_reply_char_if_possible(']', buffer, &bufpos, bufsize, &strpos, offset);
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
  }
}

#if PLEXI_WITH_LINK_STATISTICS
static void
plexi_reply_link_stats_if_possible(uint16_t id, uint8_t metric, plexi_stats_value_st value, void* print_state)
{
  plexi_print_state *ps = (plexi_print_state*)print_state;
  uint8_t *buffer = ps->buffer;
  size_t *bufpos = ps->bufpos;
  uint16_t bufsize = ps->bufsize;
  size_t *strpos = ps->strpos;
  int32_t *offset = ps->offset;
  
  if(!first_stat) {
    plexi_reply_char_if_possible(',', buffer, bufpos, bufsize, strpos, offset);
  } else {
    first_stat = 0;
  }
  plexi_reply_string_if_possible("{\"", buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible(STATS_ID_LABEL, buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible("\":", buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_uint16_if_possible(id, buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible(",\"", buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible(STATS_VALUE_LABEL, buffer, bufpos, bufsize, strpos, offset);
  plexi_reply_string_if_possible("\":", buffer, bufpos, bufsize, strpos, offset);
  if(metric == ASN) {
    plexi_reply_char_if_possible('"', buffer, bufpos, bufsize, strpos, offset);
    plexi_reply_hex_if_possible((unsigned int)value, buffer, bufpos, bufsize, strpos, offset,1);
    plexi_reply_string_if_possible("\"}", buffer, bufpos, bufsize, strpos, offset);
  } else if(metric == RSSI) {
    int x = value;
    plexi_reply_uint16_if_possible((unsigned int)x, buffer, bufpos, bufsize, strpos, offset);
    plexi_reply_char_if_possible('}', buffer, bufpos, bufsize, strpos, offset);
  } else {
    plexi_reply_uint16_if_possible((unsigned int)value, buffer, bufpos, bufsize, strpos, offset);
    plexi_reply_char_if_possible('}', buffer, bufpos, bufsize, strpos, offset);
  }
}
#endif
