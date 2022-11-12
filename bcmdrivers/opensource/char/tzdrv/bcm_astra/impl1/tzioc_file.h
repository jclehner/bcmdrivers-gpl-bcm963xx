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

#ifndef TZIOC_FILE_H
#define TZIOC_FILE_H

#include <linux/types.h>

#include "tzioc_common.h"

int _tzioc_file_open(
    struct tzioc_client *pClient,
    const char *pPath,
    uint32_t ulFlags);

int _tzioc_file_close(
    struct tzioc_client *pClient,
    const char *pPath);

int _tzioc_file_write(
    struct tzioc_client *pClient,
    const char *pPath,
    uint32_t ulPaddr,
    uint32_t ulBytes);

int _tzioc_file_read(
    struct tzioc_client *pClient,
    const char *pPath,
    uint32_t ulPaddr,
    uint32_t ulBytes);

#endif /* TZIOC_FILE_H */
