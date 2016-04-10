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
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 * \author Ilker Oztelcan <i.oztelcan@tue.nl>
 * \author Dimitris Sarakiotis <d.sarakiotis@tue.nl>
 *
 */

#include "plexi-conf.h"

#include "plexi-interface.h"
#include "plexi.h"

#include "plexi-link-statistics.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "net/rime/rime.h"
#include "er-coap-engine.h"
#include "er-coap-block1.h"

MEMB(plexi_stats_mem, plexi_stats, PLEXI_MAX_STATISTICS);
#if PLEXI_DENSE_LINK_STATISTICS == 0
MEMB(plexi_enhanced_stats_mem, plexi_enhanced_stats, PLEXI_MAX_STATISTICS);
void plexi_purge_enhanced_statistics(plexi_enhanced_stats *stats);
#endif

int inbox_post_stats_lock = PLEXI_REQUEST_CONTENT_UNLOCKED;
unsigned char inbox_post_stats[MAX_DATA_LEN];
size_t inbox_post_stats_len = 0;

void plexi_get_stats_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
void plexi_delete_stats_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
void plexi_post_stats_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

void plexi_packet_received(void);
void plexi_packet_sent(int mac_status);

RIME_SNIFFER(plexi_sniffer, plexi_packet_received, plexi_packet_sent);

PARENT_RESOURCE(resource_6top_stats,                /* name */
                "title=\"6top Statistics\"",        /* attributes */
                plexi_get_stats_handler,            /* GET handler */
                plexi_post_stats_handler,           /* POST handler */
                NULL,                     /* PUT handler */
                plexi_delete_stats_handler);          /* DELETE handler */

/**************************************************************************************************/
/** Resource and handler to GET, POST and DELETE statistics										  */
/**************************************************************************************************/

void
plexi_get_stats_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
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
    int base_len = strlen(resource_6top_stats.url);
    uint8_t flags = 0;

    /* Parse the query options and support only the slotframe, the slotoffset and channeloffset queries */
    int query_len = REST.get_query(request, &query);
    char *query_id = NULL, \
         *query_frame = NULL, \
         *query_slot = NULL, \
         *query_channel = NULL, \
         *query_tna = NULL, \
         *query_metric = NULL, \
         *query_enable = NULL;
    int frame = -1, metric = NONE, enable = ENABLE;
    int id = -1, slot = -1, channel = -1;
    linkaddr_t tna;
    int query_id_len = REST.get_query_variable(request, STATS_ID_LABEL, (const char **)(&query_id));
    int query_frame_len = REST.get_query_variable(request, FRAME_ID_LABEL, (const char **)(&query_frame));
    int query_slot_len = REST.get_query_variable(request, LINK_SLOT_LABEL, (const char **)(&query_slot));
    int query_channel_len = REST.get_query_variable(request, LINK_CHANNEL_LABEL, (const char **)(&query_channel));
    int query_tna_len = REST.get_query_variable(request, NEIGHBORS_TNA_LABEL, (const char **)(&query_tna));
    int query_metric_len = REST.get_query_variable(request, STATS_METRIC_LABEL, (const char **)(&query_metric));
    int query_enable_len = REST.get_query_variable(request, STATS_ENABLE_LABEL, (const char **)(&query_enable));
    if(query_frame) {
      *(query_frame + query_frame_len) = '\0';
      frame = (unsigned)strtoul(query_frame, &end, 10);
      flags |= 1;
    }
    if(query_slot) {
      *(query_slot + query_slot_len) = '\0';
      slot = (unsigned)strtoul(query_slot, &end, 10);
      flags |= 2;
    }
    if(query_channel) {
      *(query_channel + query_channel_len) = '\0';
      channel = (unsigned)strtoul(query_channel, &end, 10);
      flags |= 4;
    }
    if(query_tna) {
      *(query_tna + query_tna_len) = '\0';
      plexi_eui64_to_linkaddr(query_tna, query_tna_len, &tna);
      flags |= 8;
    }
    if(query_metric) {
      *(query_metric + query_metric_len) = '\0';
      if(!strcmp(STATS_ETX_LABEL, query_metric)) {
        metric = ETX;
      } else if(!strcmp(STATS_RSSI_LABEL, query_metric)) {
        metric = RSSI;
      } else if(!strcmp(STATS_LQI_LABEL, query_metric)) {
        metric = LQI;
      } else if(!strcmp(STATS_PDR_LABEL, query_metric)) {
        metric = PDR;
      } else if(!strcmp(NEIGHBORS_ASN_LABEL, query_metric)) {
        metric = ASN;
      } else {
        coap_set_status_code(response, NOT_FOUND_4_04);
        coap_set_payload(response, "Unrecognized metric", 19);
        return;
      }
      flags |= 16;
    }
    if(query_enable) {
      *(query_enable + query_enable_len) = '\0';
      if(!strcmp("y", query_enable) || !strcmp("yes", query_enable) || !strcmp("true", query_enable) || !strcmp("1", query_enable)) {
        enable = ENABLE;
      } else if(!strcmp("n", query_enable) || !strcmp("no", query_enable) || !strcmp("false", query_enable) || !strcmp("0", query_enable)) {
        enable = DISABLE;
      }
      flags |= 32;
    }
    if(query_id) {
      *(query_id + query_id_len) = '\0';
      id = (unsigned)strtoul(query_id, &end, 10);
      flags |= 64;
    }
    if(query_len > 0 && (!flags || (query_frame && frame < 0) || (query_slot && slot < 0) || (query_channel && channel < 0))) {
      coap_set_status_code(response, NOT_IMPLEMENTED_5_01);
      coap_set_payload(response, "Supports queries only on slot frame id and/or slotoffset and channeloffset", 74);
      return;
    }

    /* Parse subresources and make sure you can filter the results */
    char *uri_subresource = uri_path + base_len;
    if(*uri_subresource == '/') {
      uri_subresource++;
    }
    if((uri_len > base_len + 1 && strcmp(FRAME_ID_LABEL, uri_subresource) && strcmp(LINK_SLOT_LABEL, uri_subresource) \
        && strcmp(LINK_CHANNEL_LABEL, uri_subresource) && strcmp(STATS_WINDOW_LABEL, uri_subresource) \
        && strcmp(STATS_METRIC_LABEL, uri_subresource) && strcmp(STATS_VALUE_LABEL, uri_subresource) \
        && strcmp(NEIGHBORS_TNA_LABEL, uri_subresource) && strcmp(STATS_ENABLE_LABEL, uri_subresource) \
        && strcmp(STATS_ID_LABEL, uri_subresource))) {
      coap_set_status_code(response, NOT_FOUND_4_04);
      coap_set_payload(response, "Invalid subresource", 19);
      return;
    }
    struct tsch_slotframe *slotframe_ptr = NULL;
    if(flags & 1) {
      slotframe_ptr = (struct tsch_slotframe *)tsch_schedule_get_slotframe_by_handle(frame);
    } else {
      slotframe_ptr = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(NULL);
    }
    if(!slotframe_ptr) {
      coap_set_status_code(response, NOT_FOUND_4_04);
      coap_set_payload(response, "No slotframes found", 19);
      return;
    }
    int first_item = 1;
    do {
      struct tsch_link *link = NULL;
      if(flags & 2) {
        link = (struct tsch_link *)tsch_schedule_get_link_by_timeslot(slotframe_ptr, slot);
      } else {
        link = (struct tsch_link *)tsch_schedule_get_link_next(slotframe_ptr, NULL);
      }
      if(!link) {
        continue;
      }
      do {
        if((!(flags & 4) || link->channel_offset == channel) && (!(flags & 8) || linkaddr_cmp(&link->addr, &tna))) {
          plexi_stats *last_stats = NULL;
          if(memb_inmemb(&plexi_stats_mem, link->data)) {
            last_stats = (plexi_stats *)link->data;
            while(last_stats != NULL) {
              if((!(flags & 16) || metric == plexi_get_statistics_metric(last_stats)) && \
                 (!(flags & 32) || enable == plexi_get_statistics_enable(last_stats)) && \
                 (!(flags & 64) || id == plexi_get_statistics_id(last_stats))) {
                if(first_item) {
                  if(!(flags & 64)) {
                    CONTENT_PRINTF("[");
                  }
                  first_item = 0;
                } else {
                  CONTENT_PRINTF(",");
                }
                if(!strcmp(FRAME_ID_LABEL, uri_subresource)) {
                  CONTENT_PRINTF("%u", link->slotframe_handle);
                } else if(!strcmp(LINK_SLOT_LABEL, uri_subresource)) {
                  CONTENT_PRINTF("%u", link->timeslot);
                } else if(!strcmp(LINK_CHANNEL_LABEL, uri_subresource)) {
                  CONTENT_PRINTF("%u", link->channel_offset);
                } else if(!strcmp(STATS_METRIC_LABEL, uri_subresource)) {
                  if(plexi_get_statistics_metric(last_stats) == ETX) {
                    CONTENT_PRINTF("%s", STATS_ETX_LABEL);
                  } else if(plexi_get_statistics_metric(last_stats) == RSSI) {
                    CONTENT_PRINTF("%s", STATS_RSSI_LABEL);
                  } else if(plexi_get_statistics_metric(last_stats) == LQI) {
                    CONTENT_PRINTF("%s", STATS_LQI_LABEL);
                  } else if(plexi_get_statistics_metric(last_stats) == PDR) {
                    CONTENT_PRINTF("%s", STATS_PDR_LABEL);
                  } else if(plexi_get_statistics_metric(last_stats) == ASN) {
                    CONTENT_PRINTF("%s", NEIGHBORS_ASN_LABEL);
                  }
                } else if(!strcmp(STATS_ENABLE_LABEL, uri_subresource)) {
                  if(plexi_get_statistics_enable(last_stats) == ENABLE) {
                    CONTENT_PRINTF("1");
                  } else if(plexi_get_statistics_enable(last_stats) == DISABLE) {
                    CONTENT_PRINTF("0");
                  }
                } else if(!strcmp(NEIGHBORS_TNA_LABEL, uri_subresource)) {
                  if(!linkaddr_cmp(&link->addr, &linkaddr_null)) {
                    char na[32];
                    plexi_linkaddr_to_eui64(na, &link->addr);
                    CONTENT_PRINTF("\"%s\"", na);
                  }
                } else if(!strcmp(STATS_ID_LABEL, uri_subresource)) {
                  CONTENT_PRINTF("%u", plexi_get_statistics_id(last_stats));
                } else {
                  CONTENT_PRINTF("{\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u", \
                                 STATS_ID_LABEL, plexi_get_statistics_id(last_stats), FRAME_ID_LABEL, link->slotframe_handle, \
                                 LINK_SLOT_LABEL, link->timeslot, LINK_CHANNEL_LABEL, link->channel_offset);
                  if(plexi_get_statistics_metric(last_stats) == ETX) {
                    CONTENT_PRINTF(",\"%s\":\"%s\"", STATS_METRIC_LABEL, STATS_ETX_LABEL);
                  } else if(plexi_get_statistics_metric(last_stats) == RSSI) {
                    CONTENT_PRINTF(",\"%s\":\"%s\"", STATS_METRIC_LABEL, STATS_RSSI_LABEL);
                  } else if(plexi_get_statistics_metric(last_stats) == LQI) {
                    CONTENT_PRINTF(",\"%s\":\"%s\"", STATS_METRIC_LABEL, STATS_LQI_LABEL);
                  } else if(plexi_get_statistics_metric(last_stats) == PDR) {
                    CONTENT_PRINTF(",\"%s\":\"%s\"", STATS_METRIC_LABEL, STATS_PDR_LABEL);
                  } else if(plexi_get_statistics_metric(last_stats) == ASN) {
                    CONTENT_PRINTF(",\"%s\":\"%s\"", STATS_METRIC_LABEL, NEIGHBORS_ASN_LABEL);
                  }
                  if(plexi_get_statistics_enable(last_stats) == ENABLE) {
                    CONTENT_PRINTF(",\"%s\":1", STATS_ENABLE_LABEL);
                  } else if(plexi_get_statistics_enable(last_stats) == DISABLE) {
                    CONTENT_PRINTF(",\"%s\":0", STATS_ENABLE_LABEL);
                  }
                  if(!linkaddr_cmp(&link->addr, &linkaddr_null)) {
                    char na[32];
                    plexi_linkaddr_to_eui64(na, &link->addr);
                    CONTENT_PRINTF(",\"%s\":\"%s\"", NEIGHBORS_TNA_LABEL, na);
                  }
                  CONTENT_PRINTF("}");
                }
              }
              last_stats = last_stats->next;
            }
          }
        }
      } while(!(flags & 2) && (link = (struct tsch_link *)tsch_schedule_get_link_next(slotframe_ptr, link)));
    } while(!(flags & 1) && (slotframe_ptr = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(slotframe_ptr)));

    if(!first_item) {
      if(!(flags & 64)) {
        CONTENT_PRINTF("]");
      }
      REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
      REST.set_response_payload(response, (uint8_t *)plexi_reply_content, plexi_reply_content_len);
    } else {
      coap_set_status_code(response, NOT_FOUND_4_04);
      coap_set_payload(response, "No specified statistics resource found", 38);
    }
  } else {
    coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
  }
}
void
plexi_delete_stats_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
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
    int base_len = strlen(resource_6top_stats.url);
    uint8_t flags = 0;

    /* Parse the query options and support only the slotframe, the slotoffset and channeloffset queries */
    int query_len = REST.get_query(request, &query);
    char *query_id = NULL, \
         *query_frame = NULL, \
         *query_slot = NULL, \
         *query_channel = NULL, \
         *query_tna = NULL, \
         *query_metric = NULL, \
         *query_enable = NULL;
    int frame = -1, metric = NONE, enable = ENABLE;
    int id = -1, slot = -1, channel = -1;
    linkaddr_t tna;
    int query_id_len = REST.get_query_variable(request, STATS_ID_LABEL, (const char **)(&query_id));
    int query_frame_len = REST.get_query_variable(request, FRAME_ID_LABEL, (const char **)(&query_frame));
    int query_slot_len = REST.get_query_variable(request, LINK_SLOT_LABEL, (const char **)(&query_slot));
    int query_channel_len = REST.get_query_variable(request, LINK_CHANNEL_LABEL, (const char **)(&query_channel));
    int query_tna_len = REST.get_query_variable(request, NEIGHBORS_TNA_LABEL, (const char **)(&query_tna));
    int query_metric_len = REST.get_query_variable(request, STATS_METRIC_LABEL, (const char **)(&query_metric));
    int query_enable_len = REST.get_query_variable(request, STATS_ENABLE_LABEL, (const char **)(&query_enable));
    if(query_frame) {
      *(query_frame + query_frame_len) = '\0';
      frame = (unsigned)strtoul(query_frame, &end, 10);
      flags |= 1;
    }
    if(query_slot) {
      *(query_slot + query_slot_len) = '\0';
      slot = (unsigned)strtoul(query_slot, &end, 10);
      flags |= 2;
    }
    if(query_channel) {
      *(query_channel + query_channel_len) = '\0';
      channel = (unsigned)strtoul(query_channel, &end, 10);
      flags |= 4;
    }
    if(query_tna) {
      *(query_tna + query_tna_len) = '\0';
      plexi_eui64_to_linkaddr(query_tna, query_tna_len, &tna);
      flags |= 8;
    }
    if(query_metric) {
      *(query_metric + query_metric_len) = '\0';
      if(!strcmp(STATS_ETX_LABEL, query_metric)) {
        metric = ETX;
      } else if(!strcmp(STATS_RSSI_LABEL, query_metric)) {
        metric = RSSI;
      } else if(!strcmp(STATS_LQI_LABEL, query_metric)) {
        metric = LQI;
      } else if(!strcmp(STATS_PDR_LABEL, query_metric)) {
        metric = PDR;
      } else if(!strcmp(NEIGHBORS_ASN_LABEL, query_metric)) {
        metric = ASN;
      } else {
        coap_set_status_code(response, NOT_FOUND_4_04);
        coap_set_payload(response, "Unrecognized metric", 19);
        return;
      }
      flags |= 16;
    }
    if(query_enable) {
      *(query_enable + query_enable_len) = '\0';
      if(!strcmp("y", query_enable) || !strcmp("yes", query_enable) || !strcmp("true", query_enable) || !strcmp("1", query_enable)) {
        enable = ENABLE;
      } else if(!strcmp("n", query_enable) || !strcmp("no", query_enable) || !strcmp("false", query_enable) || !strcmp("0", query_enable)) {
        enable = DISABLE;
      }
      flags |= 32;
    }
    if(query_id) {
      *(query_id + query_id_len) = '\0';
      id = (unsigned)strtoul(query_id, &end, 10);
      flags |= 64;
    }
    if(query_len > 0 && (!flags || (query_frame && frame < 0) || (query_slot && slot < 0) || (query_channel && channel < 0))) {
      coap_set_status_code(response, NOT_IMPLEMENTED_5_01);
      coap_set_payload(response, "Supports queries only on slot frame id and/or slotoffset and channeloffset", 74);
      return;
    }

    /* Parse subresources and make sure you can filter the results */
    char *uri_subresource = uri_path + base_len;
    if(*uri_subresource == '/') {
      uri_subresource++;
    }
    if(uri_len > base_len + 1) {
      coap_set_status_code(response, NOT_FOUND_4_04);
      coap_set_payload(response, "Subresources are not allowed", 28);
      return;
    }
    struct tsch_slotframe *slotframe_ptr = NULL;
    if(flags & 1) {
      slotframe_ptr = (struct tsch_slotframe *)tsch_schedule_get_slotframe_by_handle(frame);
    } else {
      slotframe_ptr = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(NULL);
    }
    if(!slotframe_ptr) {
      coap_set_status_code(response, NOT_FOUND_4_04);
      coap_set_payload(response, "No slotframes found", 19);
      return;
    }
    int first_item = 1;
    do {
      struct tsch_link *link = NULL;
      if(flags & 2) {
        link = (struct tsch_link *)tsch_schedule_get_link_by_timeslot(slotframe_ptr, slot);
      } else {
        link = (struct tsch_link *)tsch_schedule_get_link_next(slotframe_ptr, NULL);
      }
      if(!link) {
        continue;
      }
      do {
        if((!(flags & 4) || link->channel_offset == channel) && (!(flags & 8) || linkaddr_cmp(&link->addr, &tna))) {
          plexi_stats *last_stats = NULL;
          if(memb_inmemb(&plexi_stats_mem, link->data)) {
            plexi_stats *previous_stats = NULL;
            last_stats = (plexi_stats *)link->data;
            while(last_stats != NULL) {
              uint8_t to_print = 0;
              uint8_t local_metric, local_enable;
              uint16_t local_id;/* , local_window; */
              /* plexi_stats_value_t local_value; */
              if((!(flags & 16) || metric == plexi_get_statistics_metric(last_stats)) && \
                 (!(flags & 32) || enable == plexi_get_statistics_enable(last_stats)) && \
                 (!(flags & 64) || id == plexi_get_statistics_id(last_stats))) {
                local_metric = plexi_get_statistics_metric(last_stats);
                local_enable = plexi_get_statistics_enable(last_stats);
                local_id = plexi_get_statistics_id(last_stats);
                /* local_window = plexi_get_statistics_window(last_stats); */
                if(!(flags & 8) || linkaddr_cmp(&tna, &link->addr)) {
                  to_print = 1;
                  /* local_value = last_stats->value; */
                  plexi_stats *to_be_deleted = last_stats;
                  if(last_stats == link->data) {
                    link->data = last_stats->next;
                    last_stats = (plexi_stats *)link->data;
                  } else {
                    previous_stats->next = last_stats->next;
                    last_stats = last_stats->next;
                  }
                  plexi_purge_statistics(to_be_deleted);
                }
#if PLEXI_DENSE_LINK_STATISTICS == 0
                else {
                  if(flags & 8) {
                    to_print = 1;
                    plexi_enhanced_stats *es = NULL;
                    for(es = (plexi_enhanced_stats *)list_head(last_stats->enhancement); \
                        es != NULL; es = list_item_next(es)) {
                      if(linkaddr_cmp(&tna, &es->target)) {
                        /* local_value = es->value; */
                        list_remove(last_stats->enhancement, es);
                        plexi_purge_enhanced_statistics(es);
                        break;
                      }
                    }
                  }
                  previous_stats = last_stats;
                  last_stats = last_stats->next;
                }
#endif
              } else {
                previous_stats = last_stats;
                last_stats = last_stats->next;
              }
              if(to_print) {
                if(first_item) {
                  CONTENT_PRINTF("[");
                  first_item = 0;
                } else {
                  CONTENT_PRINTF(",");
                }
                CONTENT_PRINTF("{\"%s\":%u,\"%s\":%u,\"%s\":%u,\"%s\":%u", \
                               STATS_ID_LABEL, local_id, FRAME_ID_LABEL, link->slotframe_handle, \
                               LINK_SLOT_LABEL, link->timeslot, LINK_CHANNEL_LABEL, link->channel_offset);
                if(local_metric == ETX) {
                  CONTENT_PRINTF(",\"%s\":\"%s\"", STATS_METRIC_LABEL, STATS_ETX_LABEL);
                } else if(local_metric == RSSI) {
                  CONTENT_PRINTF(",\"%s\":\"%s\"", STATS_METRIC_LABEL, STATS_RSSI_LABEL);
                } else if(local_metric == LQI) {
                  CONTENT_PRINTF(",\"%s\":\"%s\"", STATS_METRIC_LABEL, STATS_LQI_LABEL);
                } else if(local_metric == PDR) {
                  CONTENT_PRINTF(",\"%s\":\"%s\"", STATS_METRIC_LABEL, STATS_PDR_LABEL);
                } else if(local_metric == ASN) {
                  CONTENT_PRINTF(",\"%s\":\"%s\"", STATS_METRIC_LABEL, NEIGHBORS_ASN_LABEL);
                }
                if(local_enable == ENABLE) {
                  CONTENT_PRINTF(",\"%s\":1", STATS_ENABLE_LABEL);
                } else if(local_enable == DISABLE) {
                  CONTENT_PRINTF(",\"%s\":0", STATS_ENABLE_LABEL);
                }
                if(linkaddr_cmp(&tna, &linkaddr_null)) {
                  char na[32];
                  plexi_linkaddr_to_eui64(na, &link->addr);
                  CONTENT_PRINTF(",\"%s\":\"%s\"", NEIGHBORS_TNA_LABEL, na);
                }
                CONTENT_PRINTF("}");
              }
            }
          }
        }
      } while(!(flags & 2) && (link = (struct tsch_link *)tsch_schedule_get_link_next(slotframe_ptr, link)));
    } while(!(flags & 1) && (slotframe_ptr = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(slotframe_ptr)));

    if(!first_item) {
      CONTENT_PRINTF("]");
      REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
      REST.set_response_payload(response, (uint8_t *)plexi_reply_content, plexi_reply_content_len);
    } else {
      coap_set_status_code(response, NOT_FOUND_4_04);
      coap_set_payload(response, "Nothing to delete", 17);
      return;
    }
  } else {
    coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
    return;
  }
}
void
plexi_post_stats_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  if(inbox_post_stats_lock == PLEXI_REQUEST_CONTENT_UNLOCKED) {
    inbox_post_stats_len = 0;
    *inbox_post_stats = '\0';
  }
  inbox_post_stats_lock = PLEXI_REQUEST_CONTENT_UNLOCKED;
  plexi_reply_content_len = 0;
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);
  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {

    int state;
    const uint8_t *request_content;
    int request_content_len;
    char field_buf[32] = "";
    char value_buf[32] = "";

    request_content_len = REST.get_request_payload(request, &request_content);
    if(inbox_post_stats_len + request_content_len > MAX_DATA_LEN) {
      coap_set_status_code(response, NOT_IMPLEMENTED_5_01);
      coap_set_payload(response, "Server reached internal buffer limit. Shorten payload.", 54);
      return;
    }
    int x = coap_block1_handler(request, response, inbox_post_stats, &inbox_post_stats_len, MAX_DATA_LEN);
    if(inbox_post_stats_len < MAX_DATA_LEN) {
      *(inbox_post_stats + inbox_post_stats_len) = '\0';
    }
    if(x == 1) {
      inbox_post_stats_lock = PLEXI_REQUEST_CONTENT_LOCKED;
      return;
    } else if(x == -1) {
      inbox_post_stats_lock = PLEXI_REQUEST_CONTENT_UNLOCKED;
      return;
    }
    /* TODO: It is assumed that the node processes the post request fast enough to return the */
    /*       response within the window assumed by client before retransmitting */
    inbox_post_stats_lock = PLEXI_REQUEST_CONTENT_UNLOCKED;
    struct jsonparse_state js;
    jsonparse_setup(&js, (const char *)inbox_post_stats, inbox_post_stats_len);
#if PLEXI_DENSE_LINK_STATISTICS == 1
    plexi_stats stats = { .next = NULL, .metainfo = 0, .value = 0 };
#else
    plexi_stats stats = { .next = NULL, .id = 0, .enable = 0, .metric = 0, .window = 0, .value = 0 };
#endif
    int channel = -1, slot = -1, slotframe = -1;
    linkaddr_t tna;
    uint8_t flags = 0;
    int to_initialize = 0;
    short unsigned int installed = 0;
    /* Parse json input */
    while((state = plexi_json_find_field(&js, field_buf, sizeof(field_buf)))) {
      switch(state) {
      case '{':   /* New element */
        plexi_set_statistics_window(&stats, 0);
        plexi_set_statistics_enable(&stats, (uint8_t)DISABLE);
        plexi_set_statistics_metric(&stats, (uint8_t)NONE);
        stats.value = (plexi_stats_value_t)(-1);
        plexi_set_statistics_id(&stats, (uint16_t)(-1));
        channel = -1;
        slot = -1;
        slotframe = -1;
        break;
      case '}': {   /* End of current element */
        if(plexi_get_statistics_metric(&stats) == NONE) {
          coap_set_status_code(response, BAD_REQUEST_4_00);
          coap_set_payload(response, "Invalid statistics configuration (metric missing)", 49);
          return;
        }
        struct tsch_slotframe *slotframe_ptr = NULL;
        if(flags & 1) {
          slotframe_ptr = (struct tsch_slotframe *)tsch_schedule_get_slotframe_by_handle(slotframe);
        } else {
          slotframe_ptr = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(NULL);
        }
        if(!slotframe_ptr) {
          coap_set_status_code(response, NOT_FOUND_4_04);
          coap_set_payload(response, "No slotframes found", 19);
          return;
        }
        do {
          struct tsch_link *link = NULL;
          if(flags & 2) {
            link = (struct tsch_link *)tsch_schedule_get_link_by_timeslot(slotframe_ptr, slot);
          } else {
            link = (struct tsch_link *)tsch_schedule_get_link_next(slotframe_ptr, NULL);
          }
          if(!link) {
            continue;
          }
          do {
            if(!(flags & 4) || link->channel_offset == channel) {
              if(!(flags & 8) || linkaddr_cmp(&link->addr, &tna)) {
                plexi_stats *last_stats = NULL, *previous_stats = NULL;
                if(memb_inmemb(&plexi_stats_mem, link->data)) {
                  last_stats = (plexi_stats *)link->data;
                  while(last_stats != NULL) {
                    if(plexi_get_statistics_metric(&stats) == plexi_get_statistics_metric(last_stats) \
                       && (!(flags & 16) || (flags & 16 && plexi_get_statistics_id(last_stats) == plexi_get_statistics_id(&stats)))) {
                      break;
                    } else if((plexi_get_statistics_metric(&stats) == plexi_get_statistics_metric(last_stats) \
                               && flags & 16 && plexi_get_statistics_id(last_stats) != plexi_get_statistics_id(&stats)) || \
                              (plexi_get_statistics_metric(&stats) != plexi_get_statistics_metric(last_stats) \
                               && flags & 16 && plexi_get_statistics_id(last_stats) == plexi_get_statistics_id(&stats))) {
                      coap_set_status_code(response, BAD_REQUEST_4_00);
                      coap_set_payload(response, "Statistics ID represents a different metric", 43);
                      return;
                    }
                    previous_stats = last_stats;
                    last_stats = previous_stats->next;
                  }
                }
                if(last_stats == NULL) {
                  if(link->link_options != 1 && (plexi_get_statistics_metric(&stats) == ETX || plexi_get_statistics_metric(&stats) == PDR)) {
                    coap_set_status_code(response, BAD_REQUEST_4_00);
                    coap_set_payload(response, "Broadcast cells cannot measure ETX and PDR", 42);
                    return;
                  }
                  plexi_stats *link_stats = memb_alloc(&plexi_stats_mem);
                  if(link_stats == NULL) {
                    coap_set_status_code(response, INTERNAL_SERVER_ERROR_5_00);
                    coap_set_payload(response, "Not enough memory (too many statistics)", 39);
                    return;
                  }
                  plexi_set_statistics_id(link_stats, plexi_get_statistics_id(&stats));
                  link_stats->next = NULL;
                  plexi_set_statistics_window(link_stats, plexi_get_statistics_window(&stats));
                  plexi_set_statistics_metric(link_stats, plexi_get_statistics_metric(&stats));
                  if(plexi_get_statistics_metric(link_stats) == RSSI) {
                    link_stats->value = (plexi_stats_value_t)0xFFFFFFFFFFFFFFFF;
                  } else {
                    link_stats->value = (plexi_stats_value_t)(-1);
                  }
                  plexi_set_statistics_enable(link_stats, plexi_get_statistics_enable(&stats));
#if PLEXI_DENSE_LINK_STATISTICS == 0
                  LIST_STRUCT_INIT(link_stats, enhancement);
#endif
                  if(to_initialize) {
                    link_stats->value = stats.value;
                  }
                  if(previous_stats != NULL) {
                    previous_stats->next = link_stats;
                  } else {
                    link->data = (void *)link_stats;
                  }
                } else {
                  plexi_set_statistics_window(last_stats, plexi_get_statistics_window(&stats));
                  plexi_set_statistics_enable(last_stats, plexi_get_statistics_enable(&stats));
                  last_stats->value = stats.value;
                }
                installed = 1;
              }
            }
          } while(!(flags & 2) && (link = (struct tsch_link *)tsch_schedule_get_link_next(slotframe_ptr, link)));
        } while(!(flags & 1) && (slotframe_ptr = (struct tsch_slotframe *)tsch_schedule_get_slotframe_next(slotframe_ptr)));
        if(installed) {
          coap_set_status_code(response, CHANGED_2_04);
        } else {
          coap_set_status_code(response, NOT_FOUND_4_04);
          coap_set_payload(response, "Link not found to install statistics resource", 45);
        }
        return;
      }
      case JSON_TYPE_NUMBER:   /* Try to remove the if statement and change { to [ on line 601. */
        if(!strncmp(field_buf, FRAME_ID_LABEL, sizeof(field_buf))) {
          slotframe = jsonparse_get_value_as_int(&js);
          flags |= 1;
        } else if(!strncmp(field_buf, LINK_SLOT_LABEL, sizeof(field_buf))) {
          slot = jsonparse_get_value_as_int(&js);
          flags |= 2;
        } else if(!strncmp(field_buf, LINK_CHANNEL_LABEL, sizeof(field_buf))) {
          channel = jsonparse_get_value_as_int(&js);
          flags |= 4;
        } else if(!strncmp(field_buf, STATS_VALUE_LABEL, sizeof(field_buf))) {
          stats.value = (plexi_stats_value_t)jsonparse_get_value_as_int(&js);
          to_initialize = 1;
        } else if(!strncmp(field_buf, STATS_ID_LABEL, sizeof(field_buf))) {
          plexi_set_statistics_id(&stats, jsonparse_get_value_as_int(&js));
          if(plexi_get_statistics_id(&stats) < 1) {
            coap_set_status_code(response, BAD_REQUEST_4_00);
            coap_set_payload(response, "Invalid statistics configuration (invalid id)", 45);
            return;
          }
          flags |= 16;
        } else if(!strncmp(field_buf, STATS_ENABLE_LABEL, sizeof(field_buf))) {
          int x = (uint16_t)jsonparse_get_value_as_int(&js);
          if(x == 1) {
            plexi_set_statistics_enable(&stats, (uint8_t)ENABLE);
          } else {
            plexi_set_statistics_enable(&stats, (uint8_t)DISABLE);
          }
        }
        break;
      case JSON_TYPE_STRING:
        if(!strncmp(field_buf, NEIGHBORS_TNA_LABEL, sizeof(field_buf))) {
          jsonparse_copy_value(&js, value_buf, sizeof(value_buf));
          int x = plexi_eui64_to_linkaddr(value_buf, sizeof(value_buf), &tna);
          if(!x) {
            coap_set_status_code(response, BAD_REQUEST_4_00);
            coap_set_payload(response, "Invalid target node address", 27);
            return;
          }
          flags |= 8;
        } else if(!strncmp(field_buf, STATS_ENABLE_LABEL, sizeof(field_buf))) {
          jsonparse_copy_value(&js, value_buf, sizeof(value_buf));
          if(!strcmp("y", value_buf) || !strcmp("yes", value_buf) || !strcmp("true", value_buf)) {
            plexi_set_statistics_enable(&stats, (uint8_t)ENABLE);
          } else if(!strcmp("n", value_buf) || !strcmp("no", value_buf) || !strcmp("false", value_buf)) {
            plexi_set_statistics_enable(&stats, (uint8_t)DISABLE);
          }
        } else if(!strncmp(field_buf, STATS_METRIC_LABEL, sizeof(field_buf))) {
          jsonparse_copy_value(&js, value_buf, sizeof(value_buf));
          if(!strncmp(value_buf, STATS_ETX_LABEL, sizeof(STATS_ETX_LABEL))) {
            plexi_set_statistics_metric(&stats, (uint8_t)ETX);
          } else if(!strncmp(value_buf, STATS_RSSI_LABEL, sizeof(STATS_RSSI_LABEL))) {
            plexi_set_statistics_metric(&stats, (uint8_t)RSSI);
          } else if(!strncmp(value_buf, STATS_LQI_LABEL, sizeof(STATS_LQI_LABEL))) {
            plexi_set_statistics_metric(&stats, (uint8_t)LQI);
          } else if(!strncmp(value_buf, NEIGHBORS_ASN_LABEL, sizeof(NEIGHBORS_ASN_LABEL))) {
            plexi_set_statistics_metric(&stats, (uint8_t)ASN);
          } else if(!strncmp(value_buf, STATS_PDR_LABEL, sizeof(STATS_PDR_LABEL))) {
            plexi_set_statistics_metric(&stats, (uint8_t)PDR);
          } else {
            coap_set_status_code(response, NOT_IMPLEMENTED_5_01);
            coap_set_payload(response, "Unknown metric", 14);
            return;
          }
        }
        break;
      }
    }
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
void
plexi_update_ewma_statistics(uint8_t metric, void *old_value, plexi_stats_value_t new_value)
{
  if(old_value) {
    if(metric == RSSI) {
      plexi_stats_value_st v = new_value;
      if(*((plexi_stats_value_t *)old_value) != (plexi_stats_value_t)0xFFFFFFFFFFFFFFFF) {
        v = (v * 10 + *((plexi_stats_value_st *)old_value) * 90) / 100;
      }
      *((plexi_stats_value_st *)old_value) = v;
    } else {
      plexi_stats_value_t v = new_value;
      if(*((plexi_stats_value_t *)old_value) != (plexi_stats_value_t)(-1)) {
        v = (new_value * 10 + *((plexi_stats_value_t *)old_value) * 90) / 100;
      }
      if(metric == LQI || metric == ETX) {
        *((plexi_stats_value_t *)old_value) = v;
      } else if(metric == PDR) {
        *((plexi_stats_value_t *)old_value) = 100 * 256 / v;
      }
    }
  }
}
void
plexi_purge_neighbor_statistics(linkaddr_t *neighbor)
{
}
void
plexi_purge_link_statistics(struct tsch_link *link)
{
  plexi_purge_statistics((plexi_stats *)link->data);
}
void
plexi_purge_statistics(plexi_stats *stats)
{
  if(memb_inmemb(&plexi_stats_mem, stats)) {
#if PLEXI_DENSE_LINK_STATISTICS == 0
    plexi_enhanced_stats *es = list_head(stats->enhancement);
    while(es != NULL) {
      es = list_pop(stats->enhancement);
      plexi_purge_enhanced_statistics(es);
    }
#endif
    memb_free(&plexi_stats_mem, stats);
    stats = NULL;
  }
}
#if PLEXI_DENSE_LINK_STATISTICS == 0
void
plexi_purge_enhanced_statistics(plexi_enhanced_stats *stats)
{
  memb_free(&plexi_enhanced_stats_mem, stats);
}
#endif

uint16_t
plexi_get_statistics_id(plexi_stats *stats)
{
  if(!stats) {
    return (uint16_t)(-1);
  }
#if PLEXI_DENSE_LINK_STATISTICS == 1
  return (uint16_t)(stats->metainfo >> 5) & 31;
#else
  return stats->id;
#endif
}
int
plexi_set_statistics_id(plexi_stats *stats, uint16_t id)
{
  if(!stats) {
    return 0;
  }
#if PLEXI_DENSE_LINK_STATISTICS == 1
  if(id < 32) {
    stats->metainfo = (stats->metainfo & 64543) | (id << 5);
    return 1;
  } else {
    return 0;
  }
#else
  stats->id = id;
  return 1;
#endif
}
uint8_t
plexi_get_statistics_enable(plexi_stats *stats)
{
  if(!stats) {
    return (uint8_t)(-1);
  }
#if PLEXI_DENSE_LINK_STATISTICS == 1
  return (uint8_t)(stats->metainfo & 1);
#else
  return stats->enable;
#endif
}
int
plexi_set_statistics_enable(plexi_stats *stats, uint8_t enable)
{
  if(!stats) {
    return 0;
  }
#if PLEXI_DENSE_LINK_STATISTICS == 1
  if(enable < 2) {
    stats->metainfo = (stats->metainfo & 65534) | enable;
    return 1;
  } else {
    return 0;
  }
#else
  stats->enable = enable;
  return 1;
#endif
}
uint8_t
plexi_get_statistics_metric(plexi_stats *stats)
{
  if(!stats) {
    return (uint8_t)(-1);
  }
#if PLEXI_DENSE_LINK_STATISTICS == 1
  return (uint8_t)((stats->metainfo >> 1) & 15);
#else
  return stats->metric;
#endif
}
int
plexi_set_statistics_metric(plexi_stats *stats, uint8_t metric)
{
  if(!stats) {
    return 0;
  }
#if PLEXI_DENSE_LINK_STATISTICS == 1
  if(metric < 16) {
    stats->metainfo = (stats->metainfo & 65505) | (metric << 1);
    return 1;
  } else {
    return 0;
  }
#else
  stats->metric = metric;
  return 1;
#endif
}
uint16_t
plexi_get_statistics_window(plexi_stats *stats)
{
  if(!stats) {
    return (uint16_t)(-1);
  }
#if PLEXI_DENSE_LINK_STATISTICS == 1
  return (uint16_t)(stats->metainfo >> 10);
#else
  return stats->window;
#endif
}
int
plexi_set_statistics_window(plexi_stats *stats, uint16_t window)
{
  if(!stats) {
    return 0;
  }
#if PLEXI_DENSE_LINK_STATISTICS == 1
  if(window < 64) {
    stats->metainfo = (stats->metainfo & 1023) | (window << 10);
    return 1;
  } else {
    return 0;
  }
#else
  stats->window = window;
  return 1;
#endif
}
uint8_t
plexi_execute_over_link_stats(void (*callback)(uint16_t, uint8_t, plexi_stats_value_st), struct tsch_link *link, linkaddr_t *target)
{
  plexi_stats_value_st value = -1;
  if(memb_inmemb(&plexi_stats_mem, link->data)) {
    plexi_stats *stats = (plexi_stats *)link->data;
    while(stats) {
      uint8_t metric = plexi_get_statistics_metric(stats);
      value = -1;
      if(target) {
        if(linkaddr_cmp(target, &link->addr)) {
          value = (plexi_stats_value_st)stats->value;
        }
#if !PLEXI_DENSE_LINK_STATISTICS
        else if(memb_inmemb(&plexi_enhanced_stats_mem, list_head(stats->enhancement))) {
          plexi_enhanced_stats *es;
          for(es = list_head(stats->enhancement); es != NULL; es = list_item_next(es)) {
            if(linkaddr_cmp(target, &es->target)) {
              value = (plexi_stats_value_st)es->value;
              break;
            }
          }
        }
#endif
      }
      callback(plexi_get_statistics_id(stats), metric, value);
      stats = stats->next;
    }
    return 1;
  } else {
    return 0;
  }
}
/*
   plexi_stats_value_st plexi_get_stats_value_from_target(plexi_stats *stats_list, uint8_t metric, linkaddr_t target) {

    plexi_stats_value_st value = -1;
    while(stats_list) {
   if(linkaddr_cmp(lla, &link->addr))
                    value = (plexi_stats_value_st)stats->value;
   #if !PLEXI_DENSE_LINK_STATISTICS
                  else if(memb_inmemb(&plexi_enhanced_stats_mem, list_head(stats->enhancement))) {
                    plexi_enhanced_stats * es;
                    for(es = list_head(stats->enhancement); es!=NULL; es = list_item_next(es)) {
                      if(linkaddr_cmp(lla, &es->target)) {
                        value = (plexi_stats_value_st)es->value;
                        break;
                      }
                    }
                  }
   }
 */
void
plexi_printubin(plexi_stats_value_t a)
{
  char buf[128];
  int i = 0;
  for(i = 8 * sizeof(plexi_stats_value_t) - 1; i >= 0; i--) {
    buf[i] = (a & 1) + '0';
    a >>= 1;
  }
  for(i = 0; i < 8 * sizeof(plexi_stats_value_t); i++) {
    printf("%c", buf[i]);
  }
}
void
plexi_printsbin(plexi_stats_value_st a)
{
  char buf[128];
  int i = 0;
  for(i = 8 * sizeof(plexi_stats_value_t) - 1; i >= 0; i--) {
    buf[i] = (a & 1) + '0';
    a >>= 1;
  }
  for(i = 0; i < 8 * sizeof(plexi_stats_value_t); i++) {
    printf("%c", buf[i]);
  }
}

void
plexi_packet_received(void)
{
#if TSCH_WITH_LINK_STATISTICS

#if PLEXI_DENSE_LINK_STATISTICS == 0
  linkaddr_t *sender = (linkaddr_t *)packetbuf_addr(PACKETBUF_ADDR_SENDER);
#endif
  struct tsch_slotframe *slotframe = tsch_schedule_get_slotframe_by_handle((uint16_t)packetbuf_attr(PACKETBUF_ATTR_TSCH_SLOTFRAME));
  uint16_t slotoffset = (uint16_t)packetbuf_attr(PACKETBUF_ATTR_TSCH_TIMESLOT);
  struct tsch_link *link = tsch_schedule_get_link_by_timeslot(slotframe, slotoffset);
  if(memb_inmemb(&plexi_stats_mem, link->data)) {
    plexi_stats *stats = (plexi_stats *)link->data;
    while(stats != NULL) {
      if(plexi_get_statistics_metric(stats) == RSSI) {
        plexi_stats_value_st s_value = (int16_t)packetbuf_attr(PACKETBUF_ATTR_RSSI);
        plexi_stats_value_t u_value = s_value;
        plexi_update_ewma_statistics(plexi_get_statistics_metric(stats), &stats->value, u_value);
      } else if(plexi_get_statistics_metric(stats) == LQI) {
        plexi_update_ewma_statistics(plexi_get_statistics_metric(stats), &stats->value, (plexi_stats_value_t)packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY));
      } else if(plexi_get_statistics_metric(stats) == ASN) {
        stats->value = packetbuf_attr(PACKETBUF_ATTR_TSCH_ASN_2_1);
      }
#if PLEXI_DENSE_LINK_STATISTICS == 0
      if(link->link_options & LINK_OPTION_SHARED) {
        uint8_t found = 0;
        plexi_enhanced_stats *es = NULL;
        for(es = list_head(stats->enhancement); es != NULL; es = list_item_next(es)) {
          if(linkaddr_cmp(&(es->target), sender)) {
            found = 1;
            break;
          }
        }
        if(!found) {
          es = memb_alloc(&plexi_enhanced_stats_mem);
          linkaddr_copy(&(es->target), sender);
          if(plexi_get_statistics_metric(stats) == RSSI) {
            es->value = (plexi_stats_value_t)0xFFFFFFFFFFFFFFFF;
          } else {
            es->value = -1;
          }
          list_add(stats->enhancement, es);
        }
        if(plexi_get_statistics_metric(stats) == RSSI) {
          plexi_stats_value_st s_value = (int16_t)packetbuf_attr(PACKETBUF_ATTR_RSSI);
          plexi_stats_value_t u_value = s_value;
          plexi_update_ewma_statistics(plexi_get_statistics_metric(stats), &es->value, u_value);
        } else if(plexi_get_statistics_metric(stats) == LQI) {
          plexi_update_ewma_statistics(plexi_get_statistics_metric(stats), &es->value, (plexi_stats_value_t)packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY));
        } else if(plexi_get_statistics_metric(stats) == ASN) {
          es->value = packetbuf_attr(PACKETBUF_ATTR_TSCH_ASN_2_1);
        }
      }
#endif
      stats = stats->next;
    }
  }
#endif
}
void
plexi_packet_sent(int mac_status)
{
#if TSCH_WITH_LINK_STATISTICS
  if(mac_status == MAC_TX_OK && packetbuf_attr(PACKETBUF_ATTR_MAC_ACK)) {
    struct tsch_slotframe *slotframe = (struct tsch_slotframe *)tsch_schedule_get_slotframe_by_handle((uint16_t)packetbuf_attr(PACKETBUF_ATTR_TSCH_SLOTFRAME));
    uint16_t slotoffset = (uint16_t)packetbuf_attr(PACKETBUF_ATTR_TSCH_TIMESLOT);
    struct tsch_link *link = (struct tsch_link *)tsch_schedule_get_link_by_timeslot(slotframe, slotoffset);
    if(memb_inmemb(&plexi_stats_mem, link->data)) {
      plexi_stats *stats = (plexi_stats *)link->data;
      while(stats != NULL) {
        if(plexi_get_statistics_metric(stats) == ETX || plexi_get_statistics_metric(stats) == PDR) {
          plexi_update_ewma_statistics(plexi_get_statistics_metric(stats), &stats->value, 256 * packetbuf_attr(PACKETBUF_ATTR_TSCH_TRANSMISSIONS));
        }
        stats = stats->next;
      }
    }
  }
#endif
}
void
plexi_link_statistics_init()
{
  rime_sniffer_add(&plexi_sniffer);
  memb_init(&plexi_stats_mem);
#if PLEXI_DENSE_LINK_STATISTICS == 0
  memb_init(&plexi_enhanced_stats_mem);
#endif
  rest_activate_resource(&resource_6top_stats, STATS_RESOURCE);
}
