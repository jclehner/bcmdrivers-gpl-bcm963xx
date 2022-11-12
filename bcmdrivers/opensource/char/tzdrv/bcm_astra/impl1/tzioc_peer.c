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

#include <linux/string.h>

#include "tzioc_drv.h"
#include "tzioc_msg.h"
#include "tzioc_peer.h"
#include "uappd_msg.h"

#if (TZIOC_CLIENT_NAME_LEN_MAX < UAPPD_NAME_LEN_MAX) || \
    (TZIOC_CLIENT_PATH_LEN_MAX < UAPPD_PATH_LEN_MAX)
#error "TZIOC client name/path maybe too long for uappd!"
#endif

int _tzioc_peer_start(
    struct tzioc_client *pClient,
    const char *pPeerName,
    const char *pPeerExec,
    bool bPeerShared)
{
    uint8_t msg[sizeof(struct tzioc_msg_hdr) +
                sizeof(struct uappd_msg_uapp_start_cmd)];
    struct tzioc_msg_hdr *pHdr =
        (tzioc_msg_hdr *)&msg;
    struct uappd_msg_uapp_start_cmd *pCmd =
        (struct uappd_msg_uapp_start_cmd *)TZIOC_MSG_PAYLOAD(pHdr);
    int err = 0;

    pHdr->ucType = UAPPD_MSG_UAPP_START;
    pHdr->ucOrig = pClient->id;
    pHdr->ucDest = TZIOC_CLIENT_ID_UAPPD;
    pHdr->ucSeq  = 0;
    pHdr->ulLen  = sizeof(*pCmd);

    strncpy(pCmd->name, pPeerName, UAPPD_NAME_LEN_MAX);
    strncpy(pCmd->exec, pPeerExec, UAPPD_PATH_LEN_MAX);
    pCmd->shared = bPeerShared;
    pCmd->cookie = (uint32_t)pClient;

    err = _tzioc_msg_send(
        pClient,
        pHdr, (uint8_t *)pCmd);

    if (err) {
        LOGE("failed to send user app start msg");
    }

    /* immediately switch to TZOS */
    _tzioc_call_smc(0x7);
    return 0;
}

int _tzioc_peer_stop(
    struct tzioc_client *pClient,
    const char *pPeerName)
{
    uint8_t msg[sizeof(struct tzioc_msg_hdr) +
                sizeof(struct uappd_msg_uapp_stop_cmd)];
    struct tzioc_msg_hdr *pHdr =
        (tzioc_msg_hdr *)&msg;
    struct uappd_msg_uapp_stop_cmd *pCmd =
        (struct uappd_msg_uapp_stop_cmd *)TZIOC_MSG_PAYLOAD(pHdr);
    int err = 0;

    pHdr->ucType = UAPPD_MSG_UAPP_STOP;
    pHdr->ucOrig = pClient->id;
    pHdr->ucDest = TZIOC_CLIENT_ID_UAPPD;
    pHdr->ucSeq  = 0;
    pHdr->ulLen  = sizeof(*pCmd);

    strncpy(pCmd->name, pPeerName, UAPPD_NAME_LEN_MAX);
    pCmd->cookie = (uint32_t)pClient;

    err = _tzioc_msg_send(
        pClient,
        pHdr, (uint8_t *)pCmd);

    if (err) {
        LOGE("failed to send user app stop msg");
    }

    /* immediately switch to TZOS */
    _tzioc_call_smc(0x7);
    return 0;
}

int _tzioc_peer_getid(
    struct tzioc_client *pClient,
    const char *pPeerName)
{
    uint8_t msg[sizeof(struct tzioc_msg_hdr) +
                sizeof(struct uappd_msg_uapp_getid_cmd)];
    struct tzioc_msg_hdr *pHdr =
        (tzioc_msg_hdr *)&msg;
    struct uappd_msg_uapp_getid_cmd *pCmd =
        (struct uappd_msg_uapp_getid_cmd *)TZIOC_MSG_PAYLOAD(pHdr);
    int err = 0;

    pHdr->ucType = UAPPD_MSG_UAPP_GETID;
    pHdr->ucOrig = pClient->id;
    pHdr->ucDest = TZIOC_CLIENT_ID_UAPPD;
    pHdr->ucSeq  = 0;
    pHdr->ulLen  = sizeof(*pCmd);

    strncpy(pCmd->name, pPeerName, UAPPD_NAME_LEN_MAX);
    pCmd->cookie = (uint32_t)pClient;

    err = _tzioc_msg_send(
        pClient,
        pHdr, (uint8_t *)pCmd);

    if (err) {
        LOGE("failed to send user app get id msg");
    }

    /* immediately switch to TZOS */
    _tzioc_call_smc(0x7);
    return 0;
}
