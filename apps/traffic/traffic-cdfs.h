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
 *         Cumulative Density Functions for random intervals between two transmitted packets
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 */

#ifndef __TRAFFIC_CDF_H__
#define __TRAFFIC_CDF_H__

#if TRAFFIC_CDF == STDNORMAL
#define TRAFFIC_CDF_SIZE 101
#ifndef TRAFFIC_CDF_SHRINK_FACTOR
#define TRAFFIC_CDF_SHRINK_FACTOR 10
#endif
#elif TRAFFIC_CDF == GPARETO
#define TRAFFIC_CDF_SIZE 21
#ifndef TRAFFIC_CDF_SHRINK_FACTOR
#define TRAFFIC_CDF_SHRINK_FACTOR 0
#endif
#endif

static const int STDNORMAL [TRAFFIC_CDF_SIZE] = {
  2,
  3,
  4,
  6,
  8,
  10,
  14,
  19,
  26,
  34,
  45,
  59,
  78,
  101,
  130,
  167,
  214,
  272,
  343,
  431,
  537,
  667,
  822,
  1008,
  1230,
  1491,
  1798,
  2155,
  2569,
  3046,
  3591,
  4211,
  4911,
  5696,
  6571,
  7541,
  8609,
  9776,
  11045,
  12414,
  13884,
  15451,
  17111,
  18857,
  20684,
  22582,
  24542,
  26553,
  28603,
  30679,
  32768,
  34857,
  36933,
  38983,
  40994,
  42954,
  44852,
  46679,
  48425,
  50085,
  51652,
  53122,
  54491,
  55760,
  56927,
  57995,
  58965,
  59840,
  60625,
  61325,
  61945,
  62490,
  62967,
  63381,
  63738,
  64045,
  64306,
  64528,
  64714,
  64869,
  64999,
  65105,
  65193,
  65264,
  65322,
  65369,
  65406,
  65435,
  65458,
  65477,
  65491,
  65502,
  65510,
  65517,
  65522,
  65526,
  65528,
  65530,
  65532,
  65533,
  65534
};

static const int GPARETO [TRAFFIC_CDF_SIZE] = {
  0,
  41427,
  56667,
  62273,
  64336,
  65094,
  65374,
  65476,
  65514,
  65528,
  65533,
  65535,
  65535,
  65535,
  65535,
  65535,
  65535,
  65535,
  65535,
  65535,
  65535
};

#endif
