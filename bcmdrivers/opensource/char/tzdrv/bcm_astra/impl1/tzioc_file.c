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
#include "tzioc_file.h"
#include "uappd_msg.h"

int _tzioc_file_open(
    struct tzioc_client *pClient,
    const char *pPath,
    uint32_t ulFlags)
{
    uint8_t msg[sizeof(struct tzioc_msg_hdr) +
                sizeof(struct uappd_msg_file_open_cmd)];
    struct tzioc_msg_hdr *pHdr =
        (tzioc_msg_hdr *)&msg;
    struct uappd_msg_file_open_cmd *pCmd =
        (struct uappd_msg_file_open_cmd *)TZIOC_MSG_PAYLOAD(pHdr);
    int err = 0;

    pHdr->ucType = UAPPD_MSG_FILE_OPEN;
    pHdr->ucOrig = pClient->id;
    pHdr->ucDest = TZIOC_CLIENT_ID_UAPPD;
    pHdr->ucSeq  = 0;
    pHdr->ulLen  = sizeof(*pCmd);

    strncpy(pCmd->path, pPath, UAPPD_PATH_LEN_MAX);
    pCmd->flags = ulFlags;
    pCmd->cookie = (uint32_t)pClient;

    err = _tzioc_msg_send(
        pClient,
        pHdr, (uint8_t *)pCmd);

    if (err) {
        LOGE("failed to send file open msg");
    }

    /* immediately switch to TZOS */
    _tzioc_call_smc(0x7);
    return 0;
}

int _tzioc_file_close(
    struct tzioc_client *pClient,
    const char *pPath)
{
    uint8_t msg[sizeof(struct tzioc_msg_hdr) +
                sizeof(struct uappd_msg_file_close_cmd)];
    struct tzioc_msg_hdr *pHdr =
        (tzioc_msg_hdr *)&msg;
    struct uappd_msg_file_close_cmd *pCmd =
        (struct uappd_msg_file_close_cmd *)TZIOC_MSG_PAYLOAD(pHdr);
    int err = 0;

    pHdr->ucType = UAPPD_MSG_FILE_CLOSE;
    pHdr->ucOrig = pClient->id;
    pHdr->ucDest = TZIOC_CLIENT_ID_UAPPD;
    pHdr->ucSeq  = 0;
    pHdr->ulLen  = sizeof(*pCmd);

    strncpy(pCmd->path, pPath, UAPPD_PATH_LEN_MAX);
    pCmd->cookie = (uint32_t)pClient;

    err = _tzioc_msg_send(
        pClient,
        pHdr, (uint8_t *)pCmd);

    if (err) {
        LOGE("failed to send file close msg");
    }

    /* immediately switch to TZOS */
    _tzioc_call_smc(0x7);
    return 0;
}

int _tzioc_file_write(
    struct tzioc_client *pClient,
    const char *pPath,
    uint32_t ulPaddr,
    uint32_t ulBytes)
{
    uint8_t msg[sizeof(struct tzioc_msg_hdr) +
                sizeof(struct uappd_msg_file_write_cmd)];
    struct tzioc_msg_hdr *pHdr =
        (tzioc_msg_hdr *)&msg;
    struct uappd_msg_file_write_cmd *pCmd =
        (struct uappd_msg_file_write_cmd *)TZIOC_MSG_PAYLOAD(pHdr);
    int err = 0;

    pHdr->ucType = UAPPD_MSG_FILE_WRITE;
    pHdr->ucOrig = pClient->id;
    pHdr->ucDest = TZIOC_CLIENT_ID_UAPPD;
    pHdr->ucSeq  = 0;
    pHdr->ulLen  = sizeof(*pCmd);

    strncpy(pCmd->path, pPath, UAPPD_PATH_LEN_MAX);
    pCmd->paddr  = ulPaddr;
    pCmd->bytes  = ulBytes;
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

int _tzioc_file_read(
    struct tzioc_client *pClient,
    const char *pPath,
    uint32_t ulPaddr,
    uint32_t ulBytes)
{
    uint8_t msg[sizeof(struct tzioc_msg_hdr) +
                sizeof(struct uappd_msg_file_read_cmd)];
    struct tzioc_msg_hdr *pHdr =
        (tzioc_msg_hdr *)&msg;
    struct uappd_msg_file_read_cmd *pCmd =
        (struct uappd_msg_file_read_cmd *)TZIOC_MSG_PAYLOAD(pHdr);
    int err = 0;

    pHdr->ucType = UAPPD_MSG_FILE_READ;
    pHdr->ucOrig = pClient->id;
    pHdr->ucDest = TZIOC_CLIENT_ID_UAPPD;
    pHdr->ucSeq  = 0;
    pHdr->ulLen  = sizeof(*pCmd);

    strncpy(pCmd->path, pPath, UAPPD_PATH_LEN_MAX);
    pCmd->paddr  = ulPaddr;
    pCmd->bytes  = ulBytes;
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
