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
 *         plexi-tsch module: plexi interface for TSCH configuration (slotframes and links)
 *         - implementation file.
 *         All declarations and definitions in this file are only active iff \link PLEXI_WITH_TSCH_RESOURCE \endlink
 *         is defined and set to non-zero in plexi-conf.h
 *
 * \brief
 *         Defines the TSCH slotframe and link resources and its GET, DEL and POST handlers.
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 * \author Ilker Oztelcan <i.oztelcan@tue.nl>
 * \author Dimitris Sarakiotis <d.sarakiotis@tue.nl>
 *
 */

#include "plexi-conf.h"
#if PLEXI_WITH_TSCH_RESOURCE

#include "plexi-interface.h"
#include "plexi.h"
#include "plexi-tsch.h"

#if PLEXI_WITH_LINK_STATISTICS
#include "plexi-link-statistics.h"
#endif

#include "er-coap-engine.h"
#include "er-coap-block1.h"

#include <stdlib.h>

/**
 * \brief Retrieves existing slotframe(s) upon a CoAP GET request to TSCH slotframe resource.
 *
 * The handler reacts to requests on the following URLs:
 * - basis returning an array of the complete slotframe json objects \code{http} GET /FRAME_RESOURCE -> e.g. [{FRAME_ID_LABEL:1,FRAME_SLOTS_LABEL:13},{FRAME_ID_LABEL:3,FRAME_SLOTS_LABEL:101}]\endcode
 * - subresources returning json arrays with the values of the specified subresource for all slotframes:
 *   - \code{http} GET /FRAME_RESOURCE/FRAME_ID_LABEL -> a json array of identifiers (one per slotframe) e.g. [1,3]\endcode
 *   - \code{http} GET /FRAME_RESOURCE/FRAME_SLOTS_LABEL -> a json array of slotframe sizes (one per slotframe) e.g. [13,101]\endcode
 * - queries:
 *   - \code{http} GET /FRAME_RESOURCE?FRAME_ID_LABEL=3 -> one slotframe object of specific identifier e.g. {FRAME_ID_LABEL:3,FRAME_SLOTS_LABEL:101}\endcode
 *   - \code{http} GET /FRAME_RESOURCE?FRAME_SLOTS_LABEL=101 -> a json array of slotframe objects of specific size [{FRAME_ID_LABEL:3,FRAME_SLOTS_LABEL:101}]\endcode
 * - subresources and queries:
 *   - \code{http} GET /FRAME_RESOURCE/FRAME_SLOTS_LABEL?FRAME_ID_LABEL=3 -> a json array with a single slotframe size e.g. [101]\endcode
 *   - \code{http} GET /FRAME_RESOURCE/FRAME_ID_LABEL?FRAME_SLOTS_LABEL=101 -> a json array of identifiers of all slotframes of specific size e.g. [3]\endcode
 *
 * \note This handler does not support two query key-value pairs at the same request
 *
 * \sa apps/rest-engine/rest-engine.h for more information on the handler signatures
 */
static void plexi_get_slotframe_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/**
 * \brief Installs a new TSCH slotframe upon a CoAP POST request and returns a success/failure flag.
 *
 * The handler reacts to requests on the base URL only: i.e. \code{http} POST /FRAME_RESOURCE - Payload: {FRAME_ID_LABEL:3,FRAME_SLOTS_LABEL:101}\endcode
 * Each request carries in payload the complete json object of one only slotframe.
 * Installs one slotframe with the provided id and amount of slots detailed in the payload.
 *
 * The response is an array of 0 and 1 indicating unsuccessful and successful creation of the slotframe i.e. \code [1] \endcode
 *
 * \note For now, posting multiple slotframes is not supported.
 *
 * \sa apps/rest-engine/rest-engine.h for more information on the handler signatures
 */
static void plexi_post_slotframe_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/** \brief Deletes an existing slotframe upon a CoAP DEL request and returns the deleted objects.
 *
 * Handler to request the deletion of all slotframes or a specific one via a query:
 * - \code DEL /FRAME_RESOURCE -> json array with all slotframe objects e.g. [{FRAME_ID_LABEL:1,FRAME_SLOTS_LABEL:13},{FRAME_ID_LABEL:3,FRAME_SLOTS_LABEL:101}] \endcode
 * - \code DEL /FRAME_RESOURCE?FRAME_ID_LABEL=3 -> json object of the specific deleted slotframe e.g. {FRAME_ID_LABEL:3,FRAME_SLOTS_LABEL:101}\endcode
 *
 * \note: subresources or more generic queries are not supported. e.g. To delete all slotFrames of size 101 slots
 * (i.e. DEL /ref FRAME_RESOURCE?\ref FRAME_SLOTS_LABEL=101) is not yet supported. To achieve the same combine:
 * -# \code GET /FRAME_RESOURCE/FRAME_ID_LABEL?FRAME_SLOTS_LABEL=101 -> an array of ids e.g. [x,y,z]\endcode
 * -# \code for i in [x,y,z]:
 *     DEL /FRAME_RESOURCE?FRAME_ID_LABEL=i
 *  \endcode
 *
 * \warning Deleting all slotframes will cause the node disconnect from the network. A disconnected node with no slotframes installed cannot
 * be recovered unless an internal algorithm resets at least a 6tisch minimal configuration or a slotframe with at least one cell to be used for EBs
 *
 * \sa apps/rest-engine/rest-engine.h for more information on the handler signatures
 */
static void plexi_delete_slotframe_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/*---------------------------------------------------------------------------*/

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
static void plexi_get_links_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
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
static void plexi_delete_links_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
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
static void plexi_post_links_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/*---------------------------------------------------------------------------*/

/**
 * \brief Slotframe Resource to GET, POST or DELETE slotframes. POST is substituting PUT, too. Not observable resource to retrieve, create and delete slotframes.
 *
 * Slotframes are objects consisting of two properties: an identifier and the size in number of slots.
 * A slotframe object is addressed via the URL set in \ref FRAME_RESOURCE within plexi-interface.h.
 * The object has two attributes: the identifier of the frame, and the size of the frame in number of slots.
 * According to the YANG model <a href="https://tools.ietf.org/html/draft-ietf-6tisch-6top-interface-04"> 6TiSCH Operation Sublayer (6top) Interface</a>,
 * the slotframe identifiers are 8-bit long unsigned integers. Though TSCH does not impose a maximum slotframe size,
 * the YANG model assumes a 16-bit unsigned integer to represent the size of the slotframes.
 * Each slotframe is a json object like:
 * \code{.json}
 *  {
 *    FRAME_ID_LABEL: a uint8 to identify each slotframe
 *    FRAME_SLOTS_LABEL: a uint16 representing the number of slots in slotframe
 *  }
 * \endcode
 */
PARENT_RESOURCE(resource_6top_slotframe,    /* name */
                "title=\"6top Slotframe\";", /* attributes */
                plexi_get_slotframe_handler, /*GET handler*/
                plexi_post_slotframe_handler, /*POST handler*/
                NULL,     /*PUT handler*/
                plexi_delete_slotframe_handler); /*DELETE handler*/

static int inbox_post_link_lock = PLEXI_REQUEST_CONTENT_UNLOCKED;
static unsigned char inbox_post_link[MAX_DATA_LEN];
static size_t inbox_post_link_len = 0;

static void print_stats(uint16_t id, uint8_t metric, plexi_stats_value_st value);

static uint16_t new_tx_slotframe = 0;
static uint16_t new_tx_timeslot = 0;

static uint8_t first_stat = 1;

static void
plexi_get_slotframe_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  plexi_reply_content_len = 0;
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);

  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {
    char *end;
    char *uri_path = NULL;
    const char *query = NULL;

    /* split request url to base, subresources and queries */
    int uri_len = REST.get_url(request, (const char **)(&uri_path));
    *(uri_path + uri_len) = '\0';
    int base_len = strlen(resource_6top_slotframe.url);
    /* pick the start of the subresource in the url buffer */
    char *uri_subresource = uri_path + base_len;
    if(*uri_subresource == '/') {
      uri_subresource++;
    }
    char *query_value = NULL;
    unsigned long value = -1;
    int query_value_len = REST.get_query_variable(request, FRAME_ID_LABEL, (const char **)(&query_value));
    if(!query_value) {
      query_value_len = REST.get_query_variable(request, FRAME_SLOTS_LABEL, (const char **)(&query_value));
    }
    if(query_value) {
      *(query_value + query_value_len) = '\0';
      value = (unsigned)strtoul((const char *)query_value, &end, 10);
    }
    /* make sure no other url structures are accepted */
    if((uri_len > base_len + 1 && strcmp(FRAME_ID_LABEL, uri_subresource) && strcmp(FRAME_SLOTS_LABEL, uri_subresource)) || (query && !query_value)) {
      coap_set_status_code(response, NOT_IMPLEMENTED_5_01);
      coap_set_payload(response, "Supports only slot frame id XOR size as subresource or query", 60);
      return;
    }
    /* iterate over all slotframes to pick the ones specified by the query */
    int item_counter = 0;
    CONTENT_PRINTF("[");
    struct tsch_slotframe *slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(NULL);
    while(slotframe) {
      if(!query_value || (!strncmp(FRAME_ID_LABEL, query, sizeof(FRAME_ID_LABEL) - 1) && slotframe->handle == value) || \
         (!strncmp(FRAME_SLOTS_LABEL, query, sizeof(FRAME_SLOTS_LABEL) - 1) && slotframe->size.val == value)) {
        if(item_counter > 0) {
          CONTENT_PRINTF(",");
        } else if(query_value && uri_len == base_len && !strncmp(FRAME_ID_LABEL, query, sizeof(FRAME_ID_LABEL) - 1) && slotframe->handle == value) {
          plexi_reply_content_len = 0;
        }
        item_counter++;
        if(!strcmp(FRAME_ID_LABEL, uri_subresource)) {
          CONTENT_PRINTF("%u", slotframe->handle);
        } else if(!strcmp(FRAME_SLOTS_LABEL, uri_subresource)) {
          CONTENT_PRINTF("%u", slotframe->size.val);
        } else {
          CONTENT_PRINTF("{\"%s\":%u,\"%s\":%u}", FRAME_ID_LABEL, slotframe->handle, FRAME_SLOTS_LABEL, slotframe->size.val);
        }
      }
      slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(slotframe);
    }
    if(!query || uri_len != base_len || strncmp(FRAME_ID_LABEL, query, sizeof(FRAME_ID_LABEL) - 1)) {
      CONTENT_PRINTF("]");
    }
    if(item_counter > 0) {
      REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
      REST.set_response_payload(response, (uint8_t *)plexi_reply_content, plexi_reply_content_len);
    } else { /* if no slotframes were found return a CoAP 404 error */
      coap_set_status_code(response, NOT_FOUND_4_04);
      coap_set_payload(response, "No slotframe was found", 22);
      return;
    }
  } else { /* if the client accepts a response payload format other than json, return 406 */
    coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
    return;
  }
}
static void
plexi_post_slotframe_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  plexi_reply_content_len = 0;
  int state;
  int request_content_len;
  int first_item = 1;
  const uint8_t *request_content;

  char field_buf[32] = "";
  int new_sf_count = 0; /* The number of newly added slotframes */
  int ns = 0; /* number of slots */
  int fd = 0;
  /* Add new slotframe */

  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);
  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {

    request_content_len = REST.get_request_payload(request, &request_content);

    struct jsonparse_state js;
    jsonparse_setup(&js, (const char *)request_content, request_content_len);

    /* Start creating response */
    CONTENT_PRINTF("[");

    /* Parse json input */
    while((state = plexi_json_find_field(&js, field_buf, sizeof(field_buf)))) {
      switch(state) {
      case '{':   /* New element */
        ns = 0;
        fd = 0;
        break;
      case '}': {   /* End of current element */
        struct tsch_slotframe *slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_by_handle(fd);
        if(!first_item) {
          CONTENT_PRINTF(",");
        }
        first_item = 0;
        if(slotframe || fd < 0) {
          printf("PLEXI:! could not add slotframe %u with length %u\n", fd, ns);
          CONTENT_PRINTF("0");
        } else {
          if(tsch_schedule_add_slotframe(fd, ns)) {
            new_sf_count++;
            printf("PLEXI: added slotframe %u with length %u\n", fd, ns);
            CONTENT_PRINTF("1");
          } else {
            CONTENT_PRINTF("0");
          }
        }
        break;
      }
      case JSON_TYPE_NUMBER:   /* Try to remove the if statement and change { to [ on line 601. */
        if(!strncmp(field_buf, FRAME_ID_LABEL, sizeof(field_buf))) {
          fd = jsonparse_get_value_as_int(&js);
        } else if(!strncmp(field_buf, FRAME_SLOTS_LABEL, sizeof(field_buf))) {
          ns = jsonparse_get_value_as_int(&js);
        }
        break;
      }
    }
    CONTENT_PRINTF("]");
    /* Check if json parsing succeeded */
    if(js.error == JSON_ERROR_OK) {
      REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
      REST.set_response_payload(response, (uint8_t *)plexi_reply_content, plexi_reply_content_len);
    } else {
      coap_set_status_code(response, BAD_REQUEST_4_00);
      coap_set_payload(response, "Can only support JSON payload format", 36);
    }
  } else {
    coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
    return;
  }
}
static void
plexi_delete_slotframe_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  plexi_reply_content_len = 0;
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);

  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {
    char *end;
    char *uri_path = NULL;
    int uri_len = REST.get_url(request, (const char **)(&uri_path));
    *(uri_path + uri_len) = '\0';
    int base_len = strlen(resource_6top_slotframe.url);
    if(uri_len > base_len + 1) {
      coap_set_status_code(response, NOT_IMPLEMENTED_5_01);
      coap_set_payload(response, "Subresources are not supported for DELETE method", 48);
      return;
    }
    const char *query = NULL;
    char *query_value = NULL;
    int query_value_len = REST.get_query_variable(request, FRAME_ID_LABEL, (const char **)(&query_value));

    if((uri_len == base_len || uri_len == base_len + 1) && query && query_value) {
      *(query_value + query_value_len) = '\0';
      int id = (unsigned)strtoul((const char *)query_value, &end, 10);

      struct tsch_slotframe *sf = (struct tsch_slotframe *)tsch_schedule_get_slotframe_by_handle(id);
      if(sf && tsch_schedule_remove_slotframe(sf)) {
        int slots = sf->size.val;
        printf("PLEXI: deleted slotframe {%s:%u, %s:%u}\n", FRAME_ID_LABEL, id, FRAME_SLOTS_LABEL, slots);
        CONTENT_PRINTF("{\"%s\":%u, \"%s\":%u}", FRAME_ID_LABEL, id, FRAME_SLOTS_LABEL, slots);
        REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
        REST.set_response_payload(response, (uint8_t *)plexi_reply_content, plexi_reply_content_len);
      }
      coap_set_status_code(response, DELETED_2_02);
    } else if(!query) {
      /* TODO: make sure it is idempotent */
      struct tsch_slotframe *sf = NULL;
      short int first_item = 1;
      while((sf = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(NULL))) {
        if(first_item) {
          CONTENT_PRINTF("[");
          first_item = 0;
        } else {
          CONTENT_PRINTF(",");
        }
        int slots = sf->size.val;
        int id = sf->handle;
        if(sf && tsch_schedule_remove_slotframe(sf)) {
          printf("PLEXI: deleted slotframe {%s:%u, %s:%u}\n", FRAME_ID_LABEL, id, FRAME_SLOTS_LABEL, slots);
          CONTENT_PRINTF("{\"%s\":%u, \"%s\":%u}", FRAME_ID_LABEL, id, FRAME_SLOTS_LABEL, slots);
        }
      }
      if(!first_item) {
        CONTENT_PRINTF("]");
      }
      REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
      REST.set_response_payload(response, (uint8_t *)plexi_reply_content, plexi_reply_content_len);
      coap_set_status_code(response, DELETED_2_02);
    } else if(query) {
      coap_set_status_code(response, NOT_IMPLEMENTED_5_01);
      coap_set_payload(response, "Supports only slot frame id as query", 29);
      return;
    }
  } else {
    coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
    return;
  }
}
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

static void
plexi_get_links_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  plexi_reply_content_len = 0;
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);

  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {
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
                  CONTENT_PRINTF("[");
                }
                first_item = 0;
              } else {
                CONTENT_PRINTF(",");
              }
              if(!strcmp(LINK_ID_LABEL, uri_subresource)) {
                CONTENT_PRINTF("%u", link->handle);
              } else if(!strcmp(FRAME_ID_LABEL, uri_subresource)) {
                CONTENT_PRINTF("%u", link->slotframe_handle);
              } else if(!strcmp(LINK_SLOT_LABEL, uri_subresource)) {
                CONTENT_PRINTF("%u", link->timeslot);
              } else if(!strcmp(LINK_CHANNEL_LABEL, uri_subresource)) {
                CONTENT_PRINTF("%u", link->channel_offset);
              } else if(!strcmp(LINK_OPTION_LABEL, uri_subresource)) {
                CONTENT_PRINTF("%u", link->link_options);
              } else if(!strcmp(LINK_TYPE_LABEL, uri_subresource)) {
                CONTENT_PRINTF("%u", link->link_type);
              } else if(!strcmp(NEIGHBORS_TNA_LABEL, uri_subresource)) {
                if(!linkaddr_cmp(&link->addr, &linkaddr_null)) {
                  char na[32];
                  plexi_linkaddr_to_eui64(na, &link->addr);
                  CONTENT_PRINTF("\"%s\"", na);
                } else {
                  coap_set_status_code(response, NOT_FOUND_4_04);
                  coap_set_payload(response, "Link has no target node address.", 32);
                  return;
                }
              } else if(!strcmp(LINK_STATS_LABEL, uri_subresource)) {
#if PLEXI_WITH_LINK_STATISTICS
                first_stat = 1;
                if(!plexi_execute_over_link_stats(print_stats, link, NULL)) {
#endif
                coap_set_status_code(response, NOT_FOUND_4_04);
                coap_set_payload(response, "No specified statistics was found", 33);
                return;
#if PLEXI_WITH_LINK_STATISTICS
              }
#endif
              } else {
                CONTENT_PRINTF("{\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u", \
                               LINK_ID_LABEL, link->handle, FRAME_ID_LABEL, link->slotframe_handle, \
                               LINK_SLOT_LABEL, link->timeslot, LINK_CHANNEL_LABEL, link->channel_offset, \
                               LINK_OPTION_LABEL, link->link_options, LINK_TYPE_LABEL, link->link_type);
                if(!linkaddr_cmp(&link->addr, &linkaddr_null)) {
                  char na[32];
                  plexi_linkaddr_to_eui64(na, &link->addr);
                  CONTENT_PRINTF(",\"%s\":\"%s\"", NEIGHBORS_TNA_LABEL, na);
                }
#if PLEXI_WITH_LINK_STATISTICS
                CONTENT_PRINTF(",\"%s\":[", LINK_STATS_LABEL);
                first_stat = 1;
                if(plexi_execute_over_link_stats(print_stats, link, NULL)) {
                  CONTENT_PRINTF("]");
                } else {
                  plexi_reply_content_len -= sizeof(LINK_STATS_LABEL) - 5;
                }
#endif
                CONTENT_PRINTF("}");
              }
            }
          }
          link = (struct tsch_link *)tsch_schedule_get_link_next(slotframe, link);
        }
      }
      slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(slotframe);
    }
    if(flag < 7 || uri_len > base_len + 1) {
      CONTENT_PRINTF("]");
    }
    if(!first_item) {
      REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
      REST.set_response_payload(response, (uint8_t *)plexi_reply_content, plexi_reply_content_len);
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
plexi_delete_links_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  plexi_reply_content_len = 0;
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);

  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {
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
    while(slotframe) {
      if(!(flags & 4) || frame == slotframe->handle) {
        struct tsch_link *link = (struct tsch_link *)tsch_schedule_get_link_next(slotframe, NULL);
        while(link) {
          struct tsch_link *next_link = (struct tsch_link *)tsch_schedule_get_link_next(slotframe, link);
          int link_handle = link->handle;
          int link_slotframe_handle = link->slotframe_handle;
          int link_timeslot = link->timeslot;
          int link_channel_offset = link->channel_offset;
          int link_link_options = link->link_options;
          int link_link_type = link->link_type;
          char na[32];
          *na = '\0';
          if(!linkaddr_cmp(&link->addr, &linkaddr_null)) {
            plexi_linkaddr_to_eui64(na, &link->addr);
          }
          if((!(flags & 2) || link_timeslot == slot) && (!(flags & 1) || link_channel_offset == channel)) {
            int deleted = tsch_schedule_remove_link(slotframe, link);
            if(((!(flags & 2) && !(flags & 1)) || ((flags & 2) && !(flags & 1) && slot == link_timeslot) || (!(flags & 2) && (flags & 1) && channel == link_channel_offset) || ((flags & 2) && (flags & 1) && slot == link_timeslot && channel == link_channel_offset)) && deleted) {
              if(first_item) {
                if(flags < 7) {
                  CONTENT_PRINTF("[");
                }
                first_item = 0;
              } else {
                CONTENT_PRINTF(",");
              }
              printf("PLEXI: deleted link {\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u}", \
                     LINK_ID_LABEL, link_handle, FRAME_ID_LABEL, link_slotframe_handle, LINK_SLOT_LABEL, link_timeslot, \
                     LINK_CHANNEL_LABEL, link_channel_offset, LINK_OPTION_LABEL, link_link_options, LINK_TYPE_LABEL, link_link_type);
              CONTENT_PRINTF("{\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u", \
                             LINK_ID_LABEL, link_handle, FRAME_ID_LABEL, link_slotframe_handle, \
                             LINK_SLOT_LABEL, link_timeslot, LINK_CHANNEL_LABEL, link_channel_offset, \
                             LINK_OPTION_LABEL, link_link_options, LINK_TYPE_LABEL, link_link_type);
              if(strlen(na) > 0) {
                CONTENT_PRINTF(",\"%s\":\"%s\"", NEIGHBORS_TNA_LABEL, na);
              }
              CONTENT_PRINTF("}");
            }
          }
          link = next_link;
        }
      }
      slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(slotframe);
    }
    if(flags < 7) {
      CONTENT_PRINTF("]");
    }
    REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
    if(flags != 7 || !first_item) {
      REST.set_response_payload(response, (uint8_t *)plexi_reply_content, plexi_reply_content_len);
    }
    coap_set_status_code(response, DELETED_2_02);
  } else {
    coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
    return;
  }
}
static void
plexi_post_links_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  if(inbox_post_link_lock == PLEXI_REQUEST_CONTENT_UNLOCKED) {
    inbox_post_link_len = 0;
    *inbox_post_link = '\0';
  }
  plexi_reply_content_len = 0;
  int first_item = 1;
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);

  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {
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

    char field_buf[32] = "";
    char value_buf[32] = "";
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
            char buf[32];
            plexi_linkaddr_to_eui64(buf, &na);
            printf("PLEXI: added {\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u", \
                   LINK_ID_LABEL, link->handle, FRAME_ID_LABEL, fd, LINK_SLOT_LABEL, so, \
                   LINK_CHANNEL_LABEL, co, LINK_OPTION_LABEL, lo, LINK_TYPE_LABEL, lt);
            if(!linkaddr_cmp(&na, &linkaddr_null)) {
              char buf[32];
              plexi_linkaddr_to_eui64(buf, &na);
              printf(",\"%s\":\"%s\"", NEIGHBORS_TNA_LABEL, buf);
            }
            printf("}\n");
            /* * Update response * */
            if(!first_item) {
              CONTENT_PRINTF(",");
            } else {
              CONTENT_PRINTF("[");
            }
            first_item = 0;
            CONTENT_PRINTF("%u", link->handle);
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
        if(!strncmp(field_buf, NEIGHBORS_TNA_LABEL, sizeof(field_buf))) {
          jsonparse_copy_value(&js, value_buf, sizeof(value_buf));
          int x = plexi_eui64_to_linkaddr(value_buf, sizeof(value_buf), &na);
          if(!x) {
            coap_set_status_code(response, BAD_REQUEST_4_00);
            coap_set_payload(response, "Invalid target node address", 27);
            return;
          }
        }
        break;
      }
    }
    CONTENT_PRINTF("]");

    REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
    REST.set_response_payload(response, (uint8_t *)plexi_reply_content, plexi_reply_content_len);
  }
}
void
plexi_tsch_init()
{
  rest_activate_resource(&resource_6top_slotframe, FRAME_RESOURCE);
  rest_activate_resource(&resource_6top_links, LINK_RESOURCE);
}
static void
print_stats(uint16_t id, uint8_t metric, plexi_stats_value_st value)
{
  if(!first_stat) {
    CONTENT_PRINTF(",");
  } else {
    first_stat = 0;
  }
  CONTENT_PRINTF("{\"%s\":%u,\"%s\":", STATS_ID_LABEL, id, STATS_VALUE_LABEL);
  if(metric == ASN) {
    CONTENT_PRINTF("\"%x\"}", (int)value);
  } else if(metric == RSSI) {
    int x = value;
    CONTENT_PRINTF("%d}", x);
  } else {
    CONTENT_PRINTF("%u}", (unsigned)value);
  }
}
#endif
