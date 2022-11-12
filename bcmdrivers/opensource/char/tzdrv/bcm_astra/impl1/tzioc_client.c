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
#include "tzioc_client.h"

/* static pointer to client module */
static struct tzioc_client_module *pClientMod;
static struct tzioc_client_cb *pClientCB;

/* exported client control block to common code */
struct tzioc_client_cb *pTziocClientCB;

int __init _tzioc_client_module_init(void)
{
    /* alloc client module */
    pClientMod = kzalloc(sizeof(struct tzioc_client_module), GFP_KERNEL);
    if (!pClientMod) {
        LOGE("Failed to alloc TZIOC client module");
        return -ENOMEM;
    }

    /* remember client module in TZIOC device */
    tdev->pClientMod = pClientMod;

    /* init spinlock */
    spin_lock_init(&pClientMod->lock);

    /* init client control block */
    pClientCB = &pClientMod->clientCB;
    pClientCB->lastId = -1;

    /* export client control block to common code */
    pTziocClientCB = pClientCB;

    LOGI("TZIOC client module initialized");
    return 0;
}

int _tzioc_client_module_deinit(void)
{
    uint8_t idx;

    /* close all clients */
    for (idx = 0; idx < TZIOC_CLIENT_NUM_MAX; idx++) {
        if (pClientCB->pClients[idx]) {
            struct tzioc_client *pClient = pClientCB->pClients[idx];

            if (pClient->kernel)
                _tzioc_kernel_client_close(pClient);
            else
                _tzioc_user_client_close(pClient);
        }
    }

    /* reset exported client control block to common code */
    pTziocClientCB = NULL;

    /* deinit client control block */
    pClientCB = NULL;

    /* reset client module in TZIOC device */
    tdev->pClientMod = NULL;

    /* free client module control block */
    kfree(pClientMod);

    LOGI("TZIOC client module deinitialized");
    return 0;
}

struct tzioc_client *_tzioc_client_find_by_id(uint8_t id)
{
    struct tzioc_client *pClient;

    spin_lock(&pClientMod->lock);
    pClient = __tzioc_client_find_by_id(id);
    spin_unlock(&pClientMod->lock);

    return pClient;
}

struct tzioc_client *_tzioc_client_find_by_name(const char *pName)
{
    struct tzioc_client *pClient;

    spin_lock(&pClientMod->lock);
    pClient = __tzioc_client_find_by_name(pName);
    spin_unlock(&pClientMod->lock);

    return pClient;
}

struct tzioc_client *_tzioc_client_find_by_task(const struct task_struct *pTask)
{
    struct tzioc_client *pClient;

    spin_lock(&pClientMod->lock);
    pClient = __tzioc_client_find_by_task((uint32_t)pTask);
    spin_unlock(&pClientMod->lock);

    return pClient;
}

struct tzioc_client *_tzioc_kernel_client_open(
    const char *pName,
    tzioc_msg_proc_pfn pMsgProc,
    uint32_t ulPrivData)
{
    struct tzioc_client *pClient;
    int err = 0;

    /* alloc client */
    pClient = kzalloc(sizeof(struct tzioc_client), GFP_KERNEL);
    if (!pClient) {
        LOGE("Failed to alloc TZIOC client");
        return NULL;
    }

    spin_lock(&pClientMod->lock);
    err = __tzioc_kernel_client_open(
        pClient,
        pName,
        pMsgProc,
        ulPrivData);
    spin_unlock(&pClientMod->lock);

    if (err) {
        LOGE("Failed to open kernel client %s", pName);
        kfree(pClient);
        return NULL;
    }

    return pClient;
}

void _tzioc_kernel_client_close(
    struct tzioc_client *pClient)
{
    spin_lock(&pClientMod->lock);
    __tzioc_kernel_client_close(pClient);
    spin_unlock(&pClientMod->lock);

    /* free client */
    kfree(pClient);
}

struct tzioc_client *_tzioc_user_client_open(
    const char *pName,
    struct task_struct *pTask,
    int msgQ)
{
    struct tzioc_client *pClient;
    int err = 0;

    /* alloc client */
    pClient = kzalloc(sizeof(struct tzioc_client), GFP_KERNEL);
    if (!pClient) {
        LOGE("Failed to alloc TZIOC client");
        return NULL;
    }

    spin_lock(&pClientMod->lock);
    err = __tzioc_user_client_open(
        pClient,
        pName,
        (uint32_t)pTask,
        msgQ);
    spin_unlock(&pClientMod->lock);

    if (err) {
        LOGE("Failed to open user client %s", pName);
        kfree(pClient);
        return NULL;
    }

    return pClient;
}

void _tzioc_user_client_close(
    struct tzioc_client *pClient)
{
    spin_lock(&pClientMod->lock);
    __tzioc_user_client_close(pClient);
    spin_unlock(&pClientMod->lock);

    /* free client */
    kfree(pClient);
}
