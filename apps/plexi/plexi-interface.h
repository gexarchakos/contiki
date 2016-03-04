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
 * PLEXI CoAP interface.
 * Definition of resource URIs and subresources. URIs are activated
 * according to the configuration set in plexi-config.h. Changes to
 * these URIs in this file are automatically adhered by all plexi modules.
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 * \author Ilker Oztelcan <i.oztelcan@tue.nl>
 * \author Dimitris Sarakiotis <d.sarakiotis@tue.nl>
 *
 */

#ifndef __PLEXI_INTERFACE_H__
#define __PLEXI_INTERFACE_H__

#include "plexi-conf.h"

/* when RPL module is enabled, two only subresources are defined */
#if PLEXI_WITH_RPL_DAG_RESOURCE
/** \def DAG_RESOURCE
 * \brief URL of local RPL DoDAG resource
 */
#define DAG_RESOURCE "rpl/dag"
/** \def DAG_PARENT_LABEL
 * \brief subresource URL of preferred RPL parent
 */
#define DAG_PARENT_LABEL "parent"
/** \def DAG_CHILD_LABEL
 * \brief subresource URL of local RPL children
 */
#define DAG_CHILD_LABEL "child"
#endif

/* when neighbor list is enabled, two only subresources are defined */
#if PLEXI_WITH_NEIGHBOR_RESOURCE
/** \def NEIGHBORS_RESOURCE
 * \brief URL of local neighbor list resource
 */
#define NEIGHBORS_RESOURCE "6top/nbrList"
/** \def NEIGHBORS_ASN_LABEL
 * \brief subresource URL of the last moment the neighbor sent a packet
 */
#define NEIGHBORS_ASN_LABEL "asn"
/** \def NEIGHBORS_TNA_LABEL
 * \brief subresource URL of the neighbors EUI-64 address
 */
#define NEIGHBORS_TNA_LABEL "tna"
#endif

/* when TSCH is enabled, slotframes and their two only subresources are defined */
#if PLEXI_WITH_TSCH_RESOURCE
/** \def FRAME_RESOURCE
 * \brief URL of slotframe resource
 */
#define FRAME_RESOURCE "6top/slotFrame"
/** \def FRAME_ID_LABEL
 * \brief subresource URL of slotframe identifier
 */
#define FRAME_ID_LABEL "frame"
/** \def FRAME_SLOTS_LABEL
 * \brief subresource URL of slotframe size
 */
#define FRAME_SLOTS_LABEL "slots"
/** \def LINK_RESOURCE
 * \brief URL of list of links resource
 */
#define LINK_RESOURCE "6top/cellList"
/** \def LINK_ID_LABEL
 * \brief subresource URL of link identifier
 */
#define LINK_ID_LABEL "link"
/** \def LINK_SLOT_LABEL
 * \brief subresource URL of slotoffset of a link
 */
#define LINK_SLOT_LABEL "slot"
/** \def LINK_CHANNEL_LABEL
 * \brief subresource URL of channeloffset of a link
 */
#define LINK_CHANNEL_LABEL "channel"
/** \def LINK_OPTION_LABEL
 * \brief subresource URL of link option
 */
#define LINK_OPTION_LABEL "option"
/** \def LINK_TYPE_LABEL
 * \brief subresource URL of link type
 */
#define LINK_TYPE_LABEL "type"
/** \def LINK_STATS_LABEL
 * \brief subresource URL of statistics on a link
 */
#define LINK_STATS_LABEL "stats"
#endif

/* when TSCH link statistics are enabled, statistics URI with 5 subresources and 4 metrics are defined */
#if PLEXI_WITH_LINK_STATISTICS
#define STATS_RESOURCE "6top/stats"
#define STATS_METRIC_LABEL "metric"
#define STATS_ID_LABEL "id"
#define STATS_VALUE_LABEL "value"
#define STATS_WINDOW_LABEL "window"
#define STATS_ENABLE_LABEL "enable"
/** \def STATS_ETX_LABEL
 * \brief subresource URL of ETX metric
 */
#define STATS_ETX_LABEL "etx"
/** \def STATS_RSSI_LABEL
 * \brief subresource URL of RSSI metric
 */
#define STATS_RSSI_LABEL "rssi"
/** \def STATS_LQI_LABEL
 * \brief subresource URL of LQI metric
 */
#define STATS_LQI_LABEL "lqi"
/** \def STATS_PDR_LABEL
 * \brief subresource URL of PDR metric
 */
#define STATS_PDR_LABEL "pdr"
#endif

/* when TSCH queus are enabled, queu URI and their two only subresources are defined */
#if PLEXI_WITH_QUEUE_STATISTICS
#define QUEUE_RESOURCE "6top/qList"
#define QUEUE_ID_LABEL "id"
#define QUEUE_TXLEN_LABEL "txlen"
#endif

#endif
