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

#ifndef __PLEXI_LINK_STATISTICS_H__
#define __PLEXI_LINK_STATISTICS_H__

//#include "plexi-conf.h"

#include "lib/memb.h"
#include "lib/list.h"

#include "net/linkaddr.h"
#include "net/mac/tsch/tsch.h"
#include "net/mac/tsch/tsch-schedule.h"

#include "net/packetbuf.h"

#ifndef PLEXI_DENSE_LINK_STATISTICS
/** \brief Flag to condense the statistics kept. Memory efficient but potential loss of information
 */
#define PLEXI_DENSE_LINK_STATISTICS         1
#endif

#ifndef TSCH_CALLBACK_REMOVE_LINK
/** \brief Callback when a link is removed. All statistics kept about it should also be removed
 */
#define TSCH_CALLBACK_REMOVE_LINK           plexi_purge_link_statistics
#endif

#ifndef PLEXI_MAX_STATISTICS
/** \brief Maximum number of links plexi can keep statistics about
 */
#define PLEXI_MAX_STATISTICS                2
#endif

#ifndef PLEXI_LINK_STATS_UPDATE_INTERVAL
/** \brief plexi notifies observers of TSCH links statistics every PLEXI_LINK_STATS_UPDATE_INTERVAL secs
 */
#define PLEXI_LINK_STATS_UPDATE_INTERVAL    (10 * CLOCK_SECOND)
#endif

typedef enum {
  NONE = 0,
  /* macCounterOctets = 1, */
  /* macRetryCount = 2, */
  /* macMultipleRetryCount = 3, */
  /* macTXFailCount = 4, */
  /* macTXSuccessCount = 5, */
  /* macFCSErrorCount = 6, */
  /* macSecurityFailure = 7, */
  /* macDuplicateFrameCount = 8, */
  /* macRXSuccessCount = 9, */
  /* macNACKcount = 10, */
  PDR = 11,
  ETX = 12,
  RSSI = 13,
  LQI = 14,
  ASN = 15
} STATISTIC_METRIC;

typedef enum { ENABLE, DISABLE } STATISTIC_METRIC_ACTIVITY;

#if PLEXI_DENSE_LINK_STATISTICS
typedef uint16_t plexi_stats_value_t;
typedef int16_t plexi_stats_value_st;
#else
typedef uint64_t plexi_stats_value_t;
typedef int64_t plexi_stats_value_st;
typedef struct plexi_enhanced_stats_struct plexi_enhanced_stats;
struct plexi_enhanced_stats_struct {
  plexi_enhanced_stats *next;
  linkaddr_t target;
  plexi_stats_value_t value;
};
#endif

typedef struct plexi_stats_struct plexi_stats;
struct plexi_stats_struct {
  plexi_stats *next;
#if PLEXI_DENSE_LINK_STATISTICS
  uint16_t metainfo;       /* enable:1lsb, metric:2-5lsb, id:6-10lsb, window:11-16lsb */
#else
  uint16_t id;
  uint8_t enable;
  uint8_t metric;
  uint16_t window;
  LIST_STRUCT(enhancement);
#endif
  plexi_stats_value_t value;
};

uint16_t plexi_get_statistics_id(plexi_stats *stats);
int plexi_set_statistics_id(plexi_stats *stats, uint16_t id);
uint8_t plexi_get_statistics_enable(plexi_stats *stats);
int plexi_set_statistics_enable(plexi_stats *stats, uint8_t enable);
uint8_t plexi_get_statistics_metric(plexi_stats *stats);
int plexi_set_statistics_metric(plexi_stats *stats, uint8_t metric);
uint16_t plexi_get_statistics_window(plexi_stats *stats);
int plexi_set_statistics_window(plexi_stats *stats, uint16_t window);

void plexi_purge_statistics(plexi_stats *stats);
void plexi_update_ewma_statistics(uint8_t metric, void *old_value, plexi_stats_value_t new_value);

void plexi_printubin(plexi_stats_value_t a);
void plexi_printsbin(plexi_stats_value_st a);

uint8_t plexi_execute_over_link_stats(void (*callback)(uint16_t, uint8_t, plexi_stats_value_st, uint8_t*, size_t*, uint16_t, size_t*, int32_t*), struct tsch_link *link, linkaddr_t *target);

void plexi_link_statistics_init();

#endif
