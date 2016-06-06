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

#define DELTA delta_cdf
#define UNIFORM uniform_cdf
#define STDNORMAL normal_cdf
#define GPARETO gpareto_cdf

#if TRAFFIC_CDF==STDNORMAL
  #undef TRAFFIC_CDF_SIZE
  #define TRAFFIC_CDF_SIZE 101
#endif

#if TRAFFIC_CDF==GPARETO
  #undef TRAFFIC_CDF_SIZE
  #define TRAFFIC_CDF_SIZE 21
#endif

#if TRAFFIC_CDF==UNIFORM
  #undef TRAFFIC_CDF_SIZE
  #define TRAFFIC_CDF_SIZE 100
#endif

#if TRAFFIC_CDF==DELTA
  #undef TRAFFIC_CDF_SIZE
  #define TRAFFIC_CDF_SIZE 3
  #ifndef TRAFFIC_CDF_DELTA_PULSE
    #define TRAFFIC_CDF_DELTA_PULSE 100
  #endif
  #ifdef TRAFFIC_CDF_SHIFT_FACTOR
    #undef TRAFFIC_CDF_SHIFT_FACTOR
  #endif
  #define TRAFFIC_CDF_SHIFT_FACTOR (TRAFFIC_CDF_DELTA_PULSE - 21845) // TRAFFIC_CDF_SHIFT_FACTOR = TRAFFIC_DELTA_CDF_PULSE - 21845
#endif

static const int delta_cdf [3] = {
  0,
  65535,
  65535
};

static const int normal_cdf [101] = {
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

static const int gpareto_cdf [21] = {
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

static const int uniform_cdf [100] = {
       0,
     661,
    1322,
    1983,
    2644,
    3305,
    3966,
    4627,
    5288,
    5949,
    6610,
    7271,
    7932,
    8593,
    9254,
    9915,
   10576,
   11237,
   11898,
   12559,
   13220,
   13881,
   14542,
   15203,
   15864,
   16525,
   17186,
   17847,
   18508,
   19169,
   19830,
   20491,
   21152,
   21813,
   22474,
   23135,
   23796,
   24457,
   25118,
   25779,
   26440,
   27101,
   27762,
   28423,
   29084,
   29745,
   30406,
   31067,
   31728,
   32389,
   33050,
   33711,
   34372,
   35033,
   35694,
   36355,
   37016,
   37677,
   38338,
   38999,
   39660,
   40321,
   40982,
   41643,
   42304,
   42965,
   43626,
   44287,
   44948,
   45609,
   46270,
   46931,
   47592,
   48253,
   48914,
   49575,
   50236,
   50897,
   51558,
   52219,
   52880,
   53541,
   54202,
   54863,
   55524,
   56185,
   56846,
   57507,
   58168,
   58829,
   59490,
   60151,
   60812,
   61473,
   62134,
   62795,
   63456,
   64117,
   64778,
   65439
};

#endif
