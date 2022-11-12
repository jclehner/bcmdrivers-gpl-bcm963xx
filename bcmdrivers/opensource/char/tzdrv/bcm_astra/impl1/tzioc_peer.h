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

#ifndef TZIOC_PEER_H
#define TZIOC_PEER_H

#include <linux/types.h>

#include "tzioc_common.h"

int _tzioc_peer_start(
    struct tzioc_client *pClient,
    const char *pPeerName,
    const char *pPeerExec,
    bool bPeerShared);

int _tzioc_peer_stop(
    struct tzioc_client *pClient,
    const char *pPeerName);

int _tzioc_peer_getid(
    struct tzioc_client *pClient,
    const char *pPeerName);

#endif /* TZIOC_PEER_H */
