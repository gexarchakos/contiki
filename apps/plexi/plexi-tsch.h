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
 *         plexi-tsch module: plexi interface for TSCH configuration (slotframes and links)
 *         - header file.
 *
 * \author Georgios Exarchakos <g.exarchakos@tue.nl>
 * \author Ilker Oztelcan <i.oztelcan@tue.nl>
 * \author Dimitris Sarakiotis <d.sarakiotis@tue.nl>
 *
 */

#ifndef __PLEXI_TSCH_H__
#define __PLEXI_TSCH_H__

#ifndef PLEXI_LINK_UPDATE_INTERVAL
/**
 * \brief plexi notifies observers of TSCH links every \ref PLEXI_LINK_UPDATE_INTERVAL secs
 *
 * \warning Not yet fully implemented. For now, slotframes and links are not observable
 */
#define PLEXI_LINK_UPDATE_INTERVAL          (10 * CLOCK_SECOND)
#endif

/**
 * \brief Landing initialization function of plexi-tsch module. Call from \link plexi_init \endlink to start plexi-tsch module.
 *
 * Registers TSCH slotframes and links resources to be exposed over CoAP. Both resources allow for GET, DELETE and POST commands.
 * Slotframes and links are not observable (not yet).
 *
 * \sa plexi_init
 */
void plexi_tsch_init();

#endif