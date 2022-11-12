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


int _tzioc_peer_coredump(
	struct tzioc_client *pClient,
	const char *pPeerName,
	uint32_t ulPaddr,
    uint32_t ulBytes)
{
	uint8_t msg[sizeof(struct tzioc_msg_hdr) +
				sizeof(struct uappd_msg_uapp_coredump_cmd)];
	struct tzioc_msg_hdr *pHdr =
		(tzioc_msg_hdr *)&msg;
	struct uappd_msg_uapp_coredump_cmd *pCmd =
		(struct uappd_msg_uapp_coredump_cmd *)TZIOC_MSG_PAYLOAD(pHdr);
	int err = 0;

	pHdr->ucType = UAPPD_MSG_UAPP_COREDUMP;
	pHdr->ucOrig = pClient->id;
	pHdr->ucDest = TZIOC_CLIENT_ID_UAPPD;
	pHdr->ucSeq  = 0;
	pHdr->ulLen  = sizeof(*pCmd);

	strncpy(pCmd->name, pPeerName, UAPPD_NAME_LEN_MAX);
	pCmd->paddr  = ulPaddr;
	pCmd->bytes  = ulBytes;
	pCmd->cookie = (uint32_t)pClient;

	LOGI("Sending Coredump Message pAddr=0x%x size=0x%x",pCmd->paddr,pCmd->bytes);
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
