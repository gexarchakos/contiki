/*
 * Copyright (c) 2014, Swedish Institute of Computer Science.
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/**
 * \author Simon Duquennoy <simonduq@sics.se>
 */

#ifndef __COMMON_CONF_H__
#define __COMMON_CONF_H__

/* Global config flags */

#undef ENABLE_COOJA_DEBUG
#define ENABLE_COOJA_DEBUG 1

#undef IEEE802154_CONF_PANID
#define IEEE802154_CONF_PANID 0x81b2

/* RPL */

#undef UIP_CONF_ROUTER
#define UIP_CONF_ROUTER                 1

/* RPL storing mode */
#undef RPL_CONF_MOP
#define RPL_CONF_MOP RPL_MOP_STORING_NO_MULTICAST

/* Default link metric */
#undef RPL_CONF_INIT_LINK_METRIC
#define RPL_CONF_INIT_LINK_METRIC 2 /* default 5 */

#define RPL_CONF_MAX_INSTANCES    1 /* default 1 */
#define RPL_CONF_MAX_DAG_PER_INSTANCE 1 /* default 2 */

/* No RA, No NA */
#undef UIP_CONF_ND6_SEND_NA
#define UIP_CONF_ND6_SEND_NA 0

#undef UIP_CONF_ND6_SEND_RA
#define UIP_CONF_ND6_SEND_RA 0

#undef UIP_CONF_TCP
#define UIP_CONF_TCP             0
#undef UIP_CONF_DS6_ADDR_NBU
#define UIP_CONF_DS6_ADDR_NBU    1
#undef UIP_CONF_FWCACHE_SIZE
#define UIP_CONF_FWCACHE_SIZE    1
#undef UIP_CONF_UDP_CHECKSUMS
#define UIP_CONF_UDP_CHECKSUMS   1

/* Contiki netstack: MAC */
#undef NETSTACK_CONF_MAC
#define NETSTACK_CONF_MAC     csma_driver

/* Contiki netstack: RDC */
#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC     nullrdc_driver

#undef RF_CHANNEL
#define RF_CHANNEL 26

#undef CC2420_CONF_AUTOACK
#define CC2420_CONF_AUTOACK 1

#undef MICROMAC_CONF_AUTOACK
#define MICROMAC_CONF_AUTOACK 1

/* increase internal radio buffering */
#undef MIRCOMAC_CONF_BUF_NUM
#define MIRCOMAC_CONF_BUF_NUM 4

#undef CONTIKI_VERSION_STRING
#define CONTIKI_VERSION_STRING "Contiki 3.0-TRAFFIC"

#if CONTIKI_TARGET_SKY || CONTIKI_TARGET_Z1

#include "common-conf-sky-z1.h"

#endif /* CONTIKI_TARGET_SKY || CONTIKI_TARGET_Z1 */

#if CONTIKI_TARGET_JN516X

#include "common-conf-jn516x.h"

#endif /* CONTIKI_TARGET_JN516X */

#endif /* __COMMON_CONF_H__ */
