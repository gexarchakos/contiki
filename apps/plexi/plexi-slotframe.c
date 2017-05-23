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

#include "plexi-interface.h"
#include "plexi.h"

#include "er-coap-engine.h"
#include "er-coap-block1.h"
#include "net/mac/tsch/tsch-schedule.h"
#include "net/ip/uip-debug.h"
#include "../rest-engine/rest-engine.h"

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
static void plexi_get_slotframe_handler(void *request, void *response, uint8_t *buffer, uint16_t bufsize, int32_t *offset);

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
static void plexi_post_slotframe_handler(void *request, void *response, uint8_t *buffer, uint16_t bufsize, int32_t *offset);

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
static void plexi_delete_slotframe_handler(void *request, void *response, uint8_t *buffer, uint16_t bufsize, int32_t *offset);

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
PARENT_RESOURCE(resource_6top_slotframe,      /* name */
                "title=\"6top Slotframe\";",  /* attributes */
                plexi_get_slotframe_handler,  /*GET handler*/
                plexi_post_slotframe_handler, /*POST handler*/
                NULL,     /*PUT handler*/
                plexi_delete_slotframe_handler); /*DELETE handler*/

uint8_t
plexi_reply_slotframe_if_possible(struct tsch_slotframe *slotframe, uint8_t *buffer, size_t *bufpos, uint16_t bufsize, size_t *strpos, int32_t *offset)
{
  if(plexi_reply_string_if_possible("{\"", buffer, bufpos, bufsize, strpos, offset) &&
      plexi_reply_string_if_possible(FRAME_ID_LABEL, buffer, bufpos, bufsize, strpos, offset) &&
      plexi_reply_string_if_possible("\":", buffer, bufpos, bufsize, strpos, offset) &&
      plexi_reply_uint16_if_possible(slotframe->handle, buffer, bufpos, bufsize, strpos, offset) &&
      plexi_reply_string_if_possible(",\"", buffer, bufpos, bufsize, strpos, offset) &&
      plexi_reply_string_if_possible(FRAME_SLOTS_LABEL, buffer, bufpos, bufsize, strpos, offset) &&
      plexi_reply_string_if_possible("\":", buffer, bufpos, bufsize, strpos, offset) &&
      plexi_reply_uint16_if_possible(slotframe->size.val, buffer, bufpos, bufsize, strpos, offset) &&
      plexi_reply_char_if_possible('}', buffer, bufpos, bufsize, strpos, offset))
    return (uint8_t)1;
  else
    return (uint8_t)0;
}

static void
plexi_get_slotframe_handler(void *request, void *response, uint8_t *buffer, uint16_t bufsize, int32_t *offset)
{
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);

  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {
    size_t strpos = 0;            /* position in overall string (which is larger than the buffer) */
    size_t bufpos = 0;            /* position within buffer (bytes written) */

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
    query = FRAME_ID_LABEL;
    if(!query_value) {
      query_value_len = REST.get_query_variable(request, FRAME_SLOTS_LABEL, (const char **)(&query_value));
        query = FRAME_SLOTS_LABEL;
    }
    if(query_value) {
      *(query_value + query_value_len) = '\0';
      value = (unsigned)strtoul((const char *)query_value, &end, 10);
    } else {
      query = NULL;
    }
    /* make sure no other url structures are accepted */
    if((uri_len > base_len + 1 && strcmp(FRAME_ID_LABEL, uri_subresource) && strcmp(FRAME_SLOTS_LABEL, uri_subresource)) || (query && !query_value)) {
      coap_set_status_code(response, NOT_IMPLEMENTED_5_01);
      coap_set_payload(response, "Supports only slot frame id XOR size as subresource or query", 60);
      return;
    }
    /* iterate over all slotframes to pick the ones specified by the query */
    int item_counter = 0;
    plexi_reply_char_if_possible('[', buffer, &bufpos, bufsize, &strpos, offset);
    struct tsch_slotframe *slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(NULL);
    while(slotframe) {
      if(!query_value || (!strncmp(FRAME_ID_LABEL, query, sizeof(FRAME_ID_LABEL) - 1) && slotframe->handle == value) || \
         (!strncmp(FRAME_SLOTS_LABEL, query, sizeof(FRAME_SLOTS_LABEL) - 1) && slotframe->size.val == value)) {
        if(item_counter > 0) {
          plexi_reply_char_if_possible(',', buffer, &bufpos, bufsize, &strpos, offset);
        } else if(query_value && uri_len == base_len && !strncmp(FRAME_ID_LABEL, query, sizeof(FRAME_ID_LABEL) - 1) && slotframe->handle == value) {
          bufpos = 0;
          strpos = 0;
        }
        item_counter++;
        if(!strcmp(FRAME_ID_LABEL, uri_subresource)) {
          plexi_reply_uint16_if_possible(slotframe->handle, buffer, &bufpos, bufsize, &strpos, offset);
        } else if(!strcmp(FRAME_SLOTS_LABEL, uri_subresource)) {
          plexi_reply_uint16_if_possible(slotframe->size.val, buffer, &bufpos, bufsize, &strpos, offset);
        } else {
          plexi_reply_string_if_possible("{\"", buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_string_if_possible(FRAME_ID_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_string_if_possible("\":", buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_uint16_if_possible(slotframe->handle, buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_string_if_possible(",\"", buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_string_if_possible(FRAME_SLOTS_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_string_if_possible("\":", buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_uint16_if_possible(slotframe->size.val, buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_char_if_possible('}', buffer, &bufpos, bufsize, &strpos, offset);
        }
      }
      slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(slotframe);
    }
    if(!query || uri_len != base_len || strncmp(FRAME_ID_LABEL, query, sizeof(FRAME_ID_LABEL) - 1)) {
      plexi_reply_char_if_possible(']', buffer, &bufpos, bufsize, &strpos, offset);
    }
    if(item_counter > 0) {
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
plexi_post_slotframe_handler(void *request, void *response, uint8_t *buffer, uint16_t bufsize, int32_t *offset)
{
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
    size_t strpos = 0;            /* position in overall string (which is larger than the buffer) */
    size_t bufpos = 0;            /* position within buffer (bytes written) */

    request_content_len = REST.get_request_payload(request, &request_content);

    struct jsonparse_state js;
    jsonparse_setup(&js, (const char *)request_content, request_content_len);

    /* Start creating response */
    plexi_reply_char_if_possible('[', buffer, &bufpos, bufsize, &strpos, offset);

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
            plexi_reply_char_if_possible(',', buffer, &bufpos, bufsize, &strpos, offset);
          }
          first_item = 0;
          if(slotframe || fd < 0) {
            plexi_reply_char_if_possible('0', buffer, &bufpos, bufsize, &strpos, offset);
          } else {
            if(tsch_schedule_add_slotframe(fd, ns)) {
              new_sf_count++;
              plexi_reply_char_if_possible('1', buffer, &bufpos, bufsize, &strpos, offset);
            } else {
              plexi_reply_char_if_possible('0', buffer, &bufpos, bufsize, &strpos, offset);
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
    plexi_reply_char_if_possible(']', buffer, &bufpos, bufsize, &strpos, offset);
    /* Check if json parsing succeeded */
    if(js.error == JSON_ERROR_OK) {
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
      coap_set_status_code(response, BAD_REQUEST_4_00);
      coap_set_payload(response, "Can only support JSON payload format", 36);
    }
  } else {
    coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
    return;
  }
}

static void
plexi_delete_slotframe_handler(void *request, void *response, uint8_t *buffer, uint16_t bufsize, int32_t *offset)
{
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);

  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {
    size_t strpos = 0;            /* position in overall string (which is larger than the buffer) */
    size_t bufpos = 0;            /* position within buffer (bytes written) */

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
    char *query_value = NULL;
    int query_value_len = REST.get_query_variable(request, FRAME_ID_LABEL, (const char **)(&query_value));
    if((uri_len == base_len || uri_len == base_len + 1) && query_value) {
      *(query_value + query_value_len) = '\0';
      int id = (unsigned)strtoul((const char *)query_value, &end, 10);

      struct tsch_slotframe *sf = (struct tsch_slotframe *)tsch_schedule_get_slotframe_by_handle(id);
      if(sf && tsch_schedule_remove_slotframe(sf)) {
        int slots = sf->size.val;
        plexi_reply_string_if_possible("{\"", buffer, &bufpos, bufsize, &strpos, offset);
        plexi_reply_string_if_possible(FRAME_ID_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
        plexi_reply_string_if_possible("\":", buffer, &bufpos, bufsize, &strpos, offset);
        plexi_reply_uint16_if_possible(id, buffer, &bufpos, bufsize, &strpos, offset);
        plexi_reply_string_if_possible(",\"", buffer, &bufpos, bufsize, &strpos, offset);
        plexi_reply_string_if_possible(FRAME_SLOTS_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
        plexi_reply_string_if_possible("\":", buffer, &bufpos, bufsize, &strpos, offset);
        plexi_reply_uint16_if_possible(slots, buffer, &bufpos, bufsize, &strpos, offset);
        plexi_reply_char_if_possible('}', buffer, &bufpos, bufsize, &strpos, offset);
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
      coap_set_status_code(response, DELETED_2_02);
    } else if(!query_value) {
      /* TODO: make sure it is idempotent */
      struct tsch_slotframe *sf = NULL;
      short int first_item = 1;
      while((sf = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(NULL))) {
        if(first_item) {
          plexi_reply_char_if_possible('[', buffer, &bufpos, bufsize, &strpos, offset);
          first_item = 0;
        } else {
          plexi_reply_char_if_possible(',', buffer, &bufpos, bufsize, &strpos, offset);
        }
        int slots = sf->size.val;
        int id = sf->handle;
        if(sf && tsch_schedule_remove_slotframe(sf)) {
          plexi_reply_string_if_possible("{\"", buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_string_if_possible(FRAME_ID_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_string_if_possible("\":", buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_uint16_if_possible(id, buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_string_if_possible(",\"", buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_string_if_possible(FRAME_SLOTS_LABEL, buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_string_if_possible("\":", buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_uint16_if_possible(slots, buffer, &bufpos, bufsize, &strpos, offset);
          plexi_reply_char_if_possible('}', buffer, &bufpos, bufsize, &strpos, offset);
        }
      }
      if(!first_item) {
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
      coap_set_status_code(response, DELETED_2_02);
    } else if(query_value) {
      coap_set_status_code(response, NOT_IMPLEMENTED_5_01);
      coap_set_payload(response, "Supports only slot frame id as query", 29);
      return;
    }
  } else {
    coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
    return;
  }
}

