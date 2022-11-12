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

#include <linux/slab.h>

#include "tzioc_drv.h"
#include "tzioc_msg.h"

/* static pointer to msg module */
static struct tzioc_msg_module *pMsgMod;
static struct tzioc_msg_cb *pMsgCB;

/* exported msg control block to common code */
struct tzioc_msg_cb *pTziocMsgCB;

int __init _tzioc_msg_module_init(void)
{
    /* alloc msg module */
    pMsgMod = kzalloc(sizeof(struct tzioc_msg_module), GFP_KERNEL);
    if (!pMsgMod) {
        LOGE("Failed to alloc TZIOC msg module");
        return -ENOMSG;
    }

    /* remsgber msg module in TZIOC device */
    tdev->pMsgMod = pMsgMod;

    /* init spinlocks */
    spin_lock_init(&pMsgMod->sndLock);
    spin_lock_init(&pMsgMod->rcvLock);

    /* init shared memory */
    __tzioc_ring_init(
        &tdev->psmem->n2tRing,
        0,
        0,
        TZIOC_RING_WRITE,
        _tzioc_offset2addr);

    __tzioc_ring_init(
        &tdev->psmem->t2nRing,
        0,
        0,
        TZIOC_RING_READ,
        _tzioc_offset2addr);

    /* init msg control block */
    pMsgCB = &pMsgMod->msgCB;
    pMsgCB->pSndRing = &tdev->psmem->n2tRing;
    pMsgCB->pRcvRing = &tdev->psmem->t2nRing;

    /* export msg control block to common code */
    pTziocMsgCB = pMsgCB;

    LOGI("TZIOC msg module initialized");
    return 0;
}

int _tzioc_msg_module_deinit(void)
{
    /* reset exported msg control block to common code */
    pTziocMsgCB = NULL;

    /* deinit msg control block */
    pMsgCB = NULL;

    /* reset msg module in TZIOC device */
    tdev->pMsgMod = NULL;

    /* free msg module control block */
    kfree(pMsgMod);

    LOGI("TZIOC msg module deinitialized");
    return 0;
}

int _tzioc_msg_send(
    struct tzioc_client *pClient,
    struct tzioc_msg_hdr *pHdr,
    uint8_t *pPayload)
{
    int err = 0;

    spin_lock(&pMsgMod->sndLock);
    err = __tzioc_msg_send(pHdr, pPayload);
    spin_unlock(&pMsgMod->sndLock);

    if (err) {
        LOGE("Failed to send msg, client %d", pClient->id);
        return err;
    }
    return err;
}

int _tzioc_msg_receive(
    struct tzioc_client *pClient,
    struct tzioc_msg_hdr *pHdr,
    uint8_t *pPayload,
    uint32_t ulSize)
{
    int err = 0;

    spin_lock(&pMsgMod->rcvLock);
    err = __tzioc_msg_receive(pHdr, pPayload, ulSize);
    spin_unlock(&pMsgMod->rcvLock);

    if (err &&
        err != -ENOMSG &&
        err != -ENOSPC) {
        LOGE("Failed to receive msg, client %d", pClient->id);
        return err;
    }
    return err;
}
