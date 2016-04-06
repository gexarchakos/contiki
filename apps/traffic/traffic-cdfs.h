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
 *
 * \file
 *         
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 */

#ifndef __TRAFFIC_CDF_H__
#define __TRAFFIC_CDF_H__

#if TRAFFIC_CDF == STDNORMAL
#define TRAFFIC_CDF_SIZE 101
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

#endif