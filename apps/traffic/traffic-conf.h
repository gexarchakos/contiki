/*
 * Copyright (c) 2016, Georgios Exarchakos
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
 /**
 *
 * \file
 *         Configuration of UDP traffic generator parameters
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 */

#ifndef __TRAFFIC_CONF_H__
#define __TRAFFIC_CONF_H__

#ifndef TRAFFIC_PORT
#define TRAFFIC_PORT 9011
#endif

#ifndef TRAFFIC_TRANSMIT_PAYLOAD
#define TRAFFIC_TRANSMIT_PAYLOAD traffic_transmit_hello
#endif

//#ifndef TRAFFIC_CDF
//#define TRAFFIC_CDF STDNORMAL
//#endif

#endif