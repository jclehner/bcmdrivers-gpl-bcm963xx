/***************************************************************************
 *  <:copyright-BRCM:2016:DUAL/GPL:standard
 *  
 *     Copyright (c) 2016 Broadcom 
 *     All Rights Reserved
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2, as published by
 *  the Free Software Foundation (the "GPL").
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  
 *  A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
 *  writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *  
 * :>
 ***************************************************************************/

#ifndef TZIOC_MSG_H
#define TZIOC_MSG_H

#include <linux/types.h>
#include <linux/spinlock.h>

#include "tzioc_common.h"

/* msg module */
struct tzioc_msg_module {
    /* spinlocks */
    spinlock_t sndLock;
    spinlock_t rcvLock;

    /* msg control block */
    struct tzioc_msg_cb msgCB;
};

int __init _tzioc_msg_module_init(void);
int _tzioc_msg_module_deinit(void);

int _tzioc_msg_send(
    struct tzioc_client *pClient,
    struct tzioc_msg_hdr *pHdr,
    uint8_t *pPayload);

int _tzioc_msg_receive(
    struct tzioc_client *pClient,
    struct tzioc_msg_hdr *pHdr,
    uint8_t *pPayload,
    uint32_t ulSize);

#endif /* TZIOC_MSG_H */
