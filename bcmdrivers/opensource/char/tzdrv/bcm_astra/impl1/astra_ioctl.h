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

#ifndef ASTRA_IOCTL_H
#define ASTRA_IOCTL_H

#include <linux/fs.h>
#include "astra_ioctls.h"

/* ioctl handler type */
typedef int (*astra_ioctl_handler)(struct file *file, void *arg);

/* ioctl module */
struct astra_ioctl_module {
    /* ioctl handlers */
    astra_ioctl_handler handlers[ASTRA_IOCTL_LAST - ASTRA_IOCTL_FIRST];
};

int __init _astra_ioctl_module_init(void);
int _astra_ioctl_module_deinit(void);

int _astra_ioctl_do_ioctl(struct file *file, uint32_t cmd, void *arg);

#endif /* ASTRA_IOCTL_H */
