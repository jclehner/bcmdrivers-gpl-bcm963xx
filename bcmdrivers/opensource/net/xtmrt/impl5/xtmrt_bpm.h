/*
<:copyright-BRCM:2011:DUAL/GPL:standard

   Copyright (c) 2011 Broadcom 
   All Rights Reserved

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2, as published by
the Free Software Foundation (the "GPL").

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.


A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.

:>
*/
/**************************************************************************
 * File Name  : xtmrt_bpm.h
 *
 * Description: This file contains constant definitions and structure
 *              definitions for the BCM6368 ATM/PTM network device driver.
 ***************************************************************************/

#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))

#if !defined(_BCMXTMRTBPM_H)
#define _BCMXTMRTBPM_H


#include <linux/gbpm.h>
#include "bpm.h"

/**** Externs ****/
extern uint32_t xtm_alloc_buf_addr[];

/**** Prototypes ****/
void xtm_bpm_status(void);
void xtm_bpm_dump_txq_thresh(void);
void xtm_bpm_free_buf_ring( BcmXtm_RxDma *rxdma );
int xtm_bpm_alloc_buf_ring( BcmXtm_RxDma *rxdma, UINT32 num );
int xtm_bpm_txq_thresh( PBCMXTMRT_DEV_CONTEXT pDevCtx,
                        PXTMRT_TRANSMIT_QUEUE_ID pTxQId);

static inline int xtm_bpm_alloc_buf(BcmXtm_RxDma *rxdma);
static inline int xtm_bpm_free_buf(BcmXtm_RxDma *rxdma, UINT8 *pData);
                        

/**** Inline functions ****/

/* Allocates BPM_XTM_BULK_ALLOC_COUNT number of bufs and assigns to the
 * DMA ring of an XTM RX channel. The allocation is done in groups for
 * optimization.
 */
static inline int xtm_bpm_alloc_buf( BcmXtm_RxDma *rxdma )
{
    UINT8 *data, *pFkBuf;
    int buf_ix;
    uint32_t *pBuf = xtm_alloc_buf_addr;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    BcmPktDma_XtmRxDma *pktDmaRxInfo_p =
                        &pGi->rxdma[rxdma->pktDmaRxInfo.channel]->pktDmaRxInfo;

    if ( (pktDmaRxInfo_p->numRxBds - pktDmaRxInfo_p->rxAssignedBds)
            >= pktDmaRxInfo_p->allocTrig )
    { /* number of used buffers has crossed the trigger threshold */

        if (gbpm_alloc_mult_buf(pktDmaRxInfo_p->bulkAlloc, (void **)pBuf) == GBPM_ERROR)
        {
            /* may be temporarily global buffer pool is depleted.
             * Later try again */
            return GBPM_ERROR;
        }

        pktDmaRxInfo_p->alloc += pktDmaRxInfo_p->bulkAlloc;

        for (buf_ix=0; buf_ix < pktDmaRxInfo_p->bulkAlloc; buf_ix++, pBuf++)
        {
            pFkBuf = (UINT8 *) (*pBuf);

            /* Align data buffers on 16-byte boundary - Apr 2010 */
            data = PFKBUFF_TO_PDATA(pFkBuf, BCM_PKT_HEADROOM);
            FlushAssignRxBuffer(rxdma->pktDmaRxInfo.channel, data,
                                 (UINT8*) pFkBuf + BCM_PKTBUF_SIZE);
        }
    }

    return GBPM_SUCCESS;
}

static inline int xtm_bpm_free_buf(BcmXtm_RxDma *rxdma, UINT8 *pData)
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    BcmPktDma_XtmRxDma *pktDmaRxInfo_p =
                        &pGi->rxdma[rxdma->pktDmaRxInfo.channel]->pktDmaRxInfo;
    gbpm_free_buf((void *) PDATA_TO_PFKBUFF(pData,BCM_PKT_HEADROOM));
    pktDmaRxInfo_p->free--;

    return GBPM_SUCCESS;
}

#endif /* _BCMXTMRTBPM_H */
#endif
