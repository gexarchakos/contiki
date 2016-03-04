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
 *         plexi configuration file. Flags to (de)activate modules and
 *         set initial values to resource notification timers
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 * \author Ilker Oztelcan <i.oztelcan@tue.nl>
 * \author Dimitris Sarakiotis <d.sarakiotis@tue.nl>
 *
 */

#ifndef __PLEXI_CONF_H__
#define __PLEXI_CONF_H__

#ifndef PLEXI_WITH_RPL_DAG_RESOURCE
/**
 * \brief Switch plexi-rpl module on (1) or off (0)
 */
#define PLEXI_WITH_RPL_DAG_RESOURCE         1
#endif

#ifndef PLEXI_WITH_TSCH_RESOURCE
/**
 * \brief Switch plexi-tsch module on (1) or off (0)
 */
#define PLEXI_WITH_TSCH_RESOURCE            1
#endif

#ifndef PLEXI_WITH_NEIGHBOR_RESOURCE
/**
 * \brief Switch plexi-neighbor module on (1) or off (0)
 */
#define PLEXI_WITH_NEIGHBOR_RESOURCE        1
#endif

#ifndef PLEXI_WITH_LINK_STATISTICS
/**
 * \brief Switch plexi-link-statistics module on (1) or off (0)
 */
#define PLEXI_WITH_LINK_STATISTICS          1
#endif

#ifndef PLEXI_WITH_QUEUE_STATISTICS
/**
 * \brief Switch plexi-queue-statistics module on (1) or off (0)
 */
#define PLEXI_WITH_QUEUE_STATISTICS         1
#endif

#undef TSCH_WITH_LINK_STATISTICS
#define TSCH_WITH_LINK_STATISTICS PLEXI_WITH_LINK_STATISTICS



/** \brief Maximum size of buffer for CoAP replies
 */
#define MAX_DATA_LEN                        REST_MAX_CHUNK_SIZE

#define DEBUG                               DEBUG_PRINT

#endif
