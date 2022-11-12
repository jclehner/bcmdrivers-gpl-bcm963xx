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

#ifndef TZIOC_MEM_H
#define TZIOC_MEM_H

#include <linux/types.h>
#include <linux/spinlock.h>

#include "tzioc_common.h"

/* mem module */
struct tzioc_mem_module {
    /* spinlock */
    spinlock_t lock;

    /* mem control block */
    struct tzioc_mem_cb memCB;
};

int __init _tzioc_mem_module_init(void);
int _tzioc_mem_module_deinit(void);

void *_tzioc_mem_alloc(
    struct tzioc_client *pClient,
    uint32_t ulSize);

void _tzioc_mem_free(
    struct tzioc_client *pClient,
    void *pBuff);

#endif /* TZIOC_MEM_H */
