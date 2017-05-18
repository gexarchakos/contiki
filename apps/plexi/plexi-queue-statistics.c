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
 *         PLEXI resource interface (header file)
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 * \author Ilker Oztelcan <i.oztelcan@tue.nl>
 * \author Dimitris Sarakiotis <d.sarakiotis@tue.nl>
 *
 */

//#include "plexi-conf.h"

#include <stdlib.h>

#include "plexi-queue-statistics.h"
#include "plexi-interface.h"
#include "plexi.h"

#include "er-coap-engine.h"
#include "net/mac/tsch/tsch-queue.h"

static void plexi_get_queue_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void plexi_queue_event_handler(void);
//static void plexi_queue_changed(uint8_t event, struct tsch_neighbor *n);

/**************************************************************************************************/
/** Observable queuelist resource and event handler to obtain txqlength               */
/**************************************************************************************************/

PARENT_PERIODIC_RESOURCE(resource_6top_queue,         /* * name */
                         "obs;title=\"6TOP Queue statistics\"", /* * attributes */
                         plexi_get_queue_handler,   /* * GET handler */
                         NULL,            /* * POST handler */
                         NULL,            /* * PUT handler */
                         NULL,            /* * DELETE handler */
                         PLEXI_QUEUE_UPDATE_INTERVAL,
                         plexi_queue_event_handler);

/* Responds to GET with a JSON object with the following format:
 * {
 *    "215:8d00:57:6466":5,
 *    "215:8d00:57:6499":1
 * }
 * Each item in the object has the format: "EUI 64 address":<# of packets in tx queue>
 * */
static void
plexi_get_queue_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  plexi_reply_content_len = 0;
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);
  if(accept == -1 || accept == REST.type.APPLICATION_JSON) {
    /* Run through all the neighbors. Each neighbor has one queue. There are two extra for the EBs and the broadcast messages. */
    /* The function tsch_queue_get_nbr_next is defined by George in tsch_queue.h/.c */
    int first_item = 1;
    char buf[32];
    struct tsch_neighbor *neighbor = NULL;
    for(neighbor = (struct tsch_neighbor *)tsch_queue_get_nbr_next(NULL);
        neighbor != NULL;
        neighbor = (struct tsch_neighbor *)tsch_queue_get_nbr_next(neighbor)) {
      linkaddr_t tna = neighbor->addr; /* get the link layer address of neighbor */
      int txlength = tsch_queue_packet_count(&tna); /* get the size of his queue */
      int success = plexi_linkaddr_to_eui64(buf, &tna); /* convert his address to string */
      if(success) { /* if the address was valid */
        if(first_item) {
          CONTENT_PRINTF("{");
        } else {
          CONTENT_PRINTF(",");
        }
        first_item = 0;
        CONTENT_PRINTF("\"%s\":%d", buf, txlength); /* put to the output string the new JSON item */
      }
    }
    if(!first_item) { /* if you found at least one queue */
      CONTENT_PRINTF("}");
      REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
      REST.set_response_payload(response, (uint8_t *)plexi_reply_content, plexi_reply_content_len);
    } else { /* if no queues */
      coap_set_status_code(response, NOT_FOUND_4_04);
      coap_set_payload(response, "No neighbor was found", 21);
      return;
    }
  } else {
    coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
    return;
  }
}
static void
plexi_queue_event_handler(void)
{
  REST.notify_subscribers(&resource_6top_queue);
}
void
plexi_queue_changed(uint8_t event, struct tsch_neighbor *n)
{
  /* There are two events coming from queues TSCH_QUEUE_EVENT_SHRINK and TSCH_QUEUE_EVENT_GROW */
  /* For now we do not treat them separately and we let plexi return the complete list of queues when there is a change even on one. */
  plexi_queue_event_handler();
  /* For handling better the events work on the following */
  /*
     if(event == TSCH_QUEUE_EVENT_SHRINK) {

     } else if(event == TSCH_QUEUE_EVENT_GROW) {

     }
   */
}
void
plexi_queue_statistics_init()
{
  rest_activate_resource(&resource_6top_queue, QUEUE_RESOURCE);
}
