/*
   <:copyright-BRCM:2010:DUAL/GPL:standard
   
      Copyright (c) 2010 Broadcom 
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


//**************************************************************************
// File Name  : bcmenet.c
//
// Description: This is Linux network driver for Broadcom Ethernet controller
//
//**************************************************************************

#define VERSION     "0.1"
#define VER_STR     "v" VERSION

#define _BCMENET_LOCAL_

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/init.h>
#include <asm/io.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/mii.h>
#include <linux/skbuff.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/kmod.h>
#include <linux/rtnetlink.h>
#include "linux/if_bridge.h"
#include <net/arp.h>
#include <board.h>
#include <spidevices.h>
#include <bcmnetlink.h>
#include <bcm_intr.h>
#include "linux/bcm_assert_locks.h"
#include <linux/bcm_realtime.h>
#include <linux/stddef.h>
#include <asm/atomic.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/nbuff.h>
#include <net/sch_generic.h>

#include <net/net_namespace.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
#include <linux/module.h>
#endif
#include <linux/version.h>

typedef struct BcmEnet_devctrl BcmEnet_devctrl;

#include "bcm_map_part.h"
#include "bcmPktDma.h"
#include "bcmenet.h"

#if defined(_CONFIG_BCM_BPM)
#include <linux/gbpm.h>
#include "bpm.h"
#endif

#include "bcmgmac_4908.h"
#include "bcmnet.h"
#include <bcm/bcmswapitypes.h>
#include "bcmmii.h"

#define TRACE printk
#define BCM_ENET_TX_DEBUG printk
#define BCM_ENET_RX_DEBUG printk
#define BCM_ENET_DEBUG printk

#define ETH_CRC_LEN             4
#define ENET_POLL_DONE        0x80000000
#define ETH_MULTICAST_BIT       0x01

#ifndef ERROR
#define ERROR(x)        printk x
#endif
#ifndef ASSERT
#define ASSERT(x)       if (x); else ERROR(("assert: "__FILE__" line %d\n", __LINE__)); 
#endif

#ifdef CONFIG_BLOG
extern int bcm_tcp_v4_recv(pNBuff_t pNBuff, struct net_device *dev);
#endif

static int __init bcmenet_module_init(void);

static int bcm63xx_enet_open(struct net_device * dev);
static int bcm63xx_enet_close(struct net_device * dev);
static int bcm63xx_enet_xmit(pNBuff_t pNBuff, struct net_device * dev);
static int bcm_set_mac_addr(struct net_device *dev, void *p);
static int bcm63xx_enet_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static void bcm63xx_enet_timeout(struct net_device * dev);
static struct rtnl_link_stats64 * bcm63xx_enet_query(struct net_device * dev,
                                                     struct rtnl_link_stats64 *stats);
static int bcm63xx_enet_change_mtu(struct net_device *dev, int new_mtu);
static uint32 bcm63xx_rx(void *ptr, uint32 budget);

#define ENET_TX_LOCK()      spin_lock_bh(&pVnetDev0_g->ethlock_tx)
#define ENET_TX_UNLOCK()    spin_unlock_bh(&pVnetDev0_g->ethlock_tx)
#define ENET_RX_LOCK()      spin_lock_bh(&pVnetDev0_g->ethlock_rx)
#define ENET_RX_UNLOCK()    spin_unlock_bh(&pVnetDev0_g->ethlock_rx)

struct kmem_cache *enetSkbCache;

#define IUDMA_RX_CHAN (0)
#define IUDMA_TX_CHAN (1)

#define BcmPktDma_LocalEthRxDma BcmPktDma_LocalEthRxDma
#define BcmPktDma_EthTxDma BcmPktDma_LocalEthTxDma

static const struct net_device_ops bcm96xx_netdev_ops = {
    .ndo_open   = bcm63xx_enet_open,
    .ndo_stop   = bcm63xx_enet_close,
    .ndo_start_xmit   = (HardStartXmitFuncP)bcm63xx_enet_xmit,
    .ndo_set_mac_address  = bcm_set_mac_addr,
    .ndo_do_ioctl   = bcm63xx_enet_ioctl,
    .ndo_tx_timeout   = bcm63xx_enet_timeout,
    .ndo_get_stats64      = bcm63xx_enet_query,
    .ndo_change_mtu     = bcm63xx_enet_change_mtu
};

BcmEnet_devctrl *pVnetDev0_g = NULL;

int bcmenet_in_init_dev = 0;

static inline volatile DmaRegs *get_dmaCtrl( void )
{
    volatile DmaRegs *dmaCtrl;

    dmaCtrl= (DmaRegs *)(GMAC_DMA_BASE);

    return dmaCtrl;
}

unsigned short bcm_type_trans(struct sk_buff *skb, struct net_device *dev)
{
    struct ethhdr *eth;
    unsigned char *rawp;
    unsigned int hdrlen = sizeof(struct ethhdr);

    skb_reset_mac_header(skb);

    skb_pull(skb, hdrlen);
    eth = (struct ethhdr *)skb_mac_header(skb);

    if(*eth->h_dest&1)
    {
        if(memcmp(eth->h_dest,dev->broadcast, ETH_ALEN)==0)
            skb->pkt_type=PACKET_BROADCAST;
        else
            skb->pkt_type=PACKET_MULTICAST;
    }

    /*
     *  This ALLMULTI check should be redundant by 1.4
     *  so don't forget to remove it.
     *
     *  Seems, you forgot to remove it. All silly devices
     *  seems to set IFF_PROMISC.
     */

    else if(1 /*dev->flags&IFF_PROMISC*/)
    {
        if(memcmp(eth->h_dest,dev->dev_addr, ETH_ALEN))
            skb->pkt_type=PACKET_OTHERHOST;
    }

    if (ntohs(eth->h_proto) >= 1536)
        return eth->h_proto;

    rawp = skb->data;

    /*
     *  This is a magic hack to spot IPX packets. Older Novell breaks
     *  the protocol design and runs IPX over 802.3 without an 802.2 LLC
     *  layer. We look for FFFF which isn't a used 802.2 SSAP/DSAP. This
     *  won't work for fault tolerant netware but does for the rest.
     */
    if (*(unsigned short *)rawp == 0xFFFF)
        return htons(ETH_P_802_3);

    /*
     *  Real 802.2 LLC
     */
    return htons(ETH_P_802_2);
}

void extsw_wreg_mmap(int page, int reg, uint8 *data_in, int len);
void extsw_rreg_mmap(int page, int reg, uint8 *data_out, int len);

static void setup_switch(void)
{    
    uint32 v32;
    uint32 port;

    /* Force IMP port link up for 4908 */
    extsw_rreg_mmap(PAGE_CONTROL, REG_CONTROL_MII1_PORT_STATE_OVERRIDE, (void*)&v32, sizeof(v32));
    v32 |= REG_CONTROL_MPSO_LINKPASS;
    extsw_wreg_mmap(PAGE_CONTROL, REG_CONTROL_MII1_PORT_STATE_OVERRIDE, (void*)&v32, sizeof(v32));

    /* Enable MAC RX/TX for all switch ports and no-STP*/
    extsw_rreg_mmap(PAGE_CONTROL, REG_PORT_CTRL, (void*)&v32, sizeof(v32)); /* Read first */
    v32 &= ~REG_PORT_CTRL_DISABLE;
    v32 &= ~REG_PORT_STP_MASK;  /* No spanning tree */
    for (port = 0 ; port < 4; port++)
    {
        extsw_wreg_mmap(PAGE_CONTROL, REG_PORT_CTRL+port, (void*)&v32, sizeof(v32)); /* Port#0 is base*/
    }

    /* Set PBVLAN MAP  */
    v32 = 0x1ff;
    for (port = 0 ; port < 4; port++)
    {
        extsw_wreg_mmap(PAGE_PORT_BASED_VLAN, REG_VLAN_CTRL_P0 + (port * 2), (void*)&v32, sizeof(v32)); /* Port#0 is base*/
    }
}

int  bcmeapi_open_dev(BcmEnet_devctrl *pDevCtrl, struct net_device *dev)
{
    ENET_RX_LOCK();
    pDevCtrl->dmaCtrl->controller_cfg |= DMA_MASTER_EN;
#if defined(IUDMA_RX_CHAN_FLOWC)
    pDevCtrl->dmaCtrl->controller_cfg |= DMA_FLOWC_CH1_EN;
#else
    pDevCtrl->dmaCtrl->controller_cfg &= ~DMA_FLOWC_CH1_EN;
#endif
    /*  Enable the Rx DMA channels and their interrupts  */
    bcmPktDma_EthRxEnable(&pDevCtrl->rxdma->pktDmaRxInfo);
    //bcmPktDma_BcmHalInterruptEnable(channel, rxdma->rxIrq);
    ENET_RX_UNLOCK();

    ENET_TX_LOCK();
    /*  Enable the Tx DMA channels  */
    bcmPktDma_EthTxEnable(pDevCtrl->txdma);
    ENET_TX_UNLOCK();

    /* Setup switch to enable all ports */
    setup_switch();

    return 0; /* success */
}


/* --------------------------------------------------------------------------
Name: bcm63xx_enet_open
Purpose: Open and Initialize the EMAC on the chip
-------------------------------------------------------------------------- */
static int bcm63xx_enet_open(struct net_device * dev)
{
    BcmEnet_devctrl *pDevCtrl = netdev_priv(dev);

    set_bit(__LINK_STATE_START, &dev->state);
	dev->flags |= IFF_UP;	/* have to mark the flag earlier */
    bcmeapi_open_dev(pDevCtrl, dev);
    netif_carrier_on(dev);
    netif_start_queue(dev);
    /* Not sure why I need to call dev_activate but this is needed to start the TX queues in Kernel */
	dev_activate(dev);


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 1)
    /* enet does not use NAPI in 3.4 */
#else
    /* napi_enable must be called before the interrupts are enabled
       if an interrupt comes in before napi_enable is called the napi
       handler will not run and the interrupt will not be re-enabled */
    napi_enable(&pDevCtrl->napi);
#endif

    return 0;
}

void bcmeapi_del_dev_intr(BcmEnet_devctrl *pDevCtrl)
{
    int channel = IUDMA_RX_CHAN;
    BcmEnet_RxDma *rxdma;

    ENET_RX_LOCK();
    rxdma = pDevCtrl->rxdma;
    bcmPktDma_BcmHalInterruptDisable(channel, rxdma->rxIrq);
    bcmPktDma_EthRxDisable(&rxdma->pktDmaRxInfo);
    ENET_RX_UNLOCK();

    ENET_TX_LOCK();

    bcmPktDma_EthTxDisable(pDevCtrl->txdma);
    ENET_TX_UNLOCK();
}

/* --------------------------------------------------------------------------
Name: bcm63xx_enet_close
Purpose: Stop communicating with the outside world
Note: Caused by 'ifconfig ethX down'
-------------------------------------------------------------------------- */
static int bcm63xx_enet_close(struct net_device * dev)
{
    BcmEnet_devctrl *pDevCtrl = netdev_priv(dev);

    ASSERT(pDevCtrl != NULL);

    bcmeapi_del_dev_intr(pDevCtrl);

    netif_stop_queue(dev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 1)
    /* enet does not use NAPI in 3.4 */
#else
    napi_disable(&pDevCtrl->napi);
#endif

    return 0;
}

/* --------------------------------------------------------------------------
Name: bcm63xx_enet_timeout
Purpose:
-------------------------------------------------------------------------- */
static void bcm63xx_enet_timeout(struct net_device * dev)
{
    ASSERT(dev != NULL);
    TRACE("%s: bcm63xx_enet_timeout\n", dev->name);

    dev->trans_start = jiffies;
    netif_wake_queue(dev);
}


/* --------------------------------------------------------------------------
Name: bcm63xx_enet_query
Purpose: Return the current statistics. This may be called with the card
open or closed.
-------------------------------------------------------------------------- */
static struct rtnl_link_stats64 * bcm63xx_enet_query(struct net_device * dev,
                                                     struct rtnl_link_stats64 *stats)
{
    gmac_hw_stats( stats );

    return stats;
}

static int bcm63xx_enet_change_mtu(struct net_device *dev, int new_mtu)
{

    int max_mtu = ENET_MAX_MTU_PAYLOAD_SIZE;

    if (new_mtu < ETH_ZLEN || new_mtu > max_mtu)
        return -EINVAL;
    dev->mtu = new_mtu;

    return 0;
}


/*
 * Recycling context definition
 */
#define DELAYED_RECLAIM_ARRAY_LEN 8

#define DO_DELAYED_RECLAIM(arrayIdx, arrayPtr) \
    do { \
        uint32 tmp_idx=0; \
        while (tmp_idx < (arrayIdx)) { \
            nbuff_free((pNBuff_t) (arrayPtr)[tmp_idx]); \
            tmp_idx++; } \
    } while (0)

typedef struct EnetXmitParams {
    unsigned int len; 
    unsigned int mark; 
    unsigned int priority; 
    unsigned int r_flags; 
    uint16 port_id; 
    uint8 * data; 
    BcmEnet_devctrl *pDevPriv; 
    uint32_t blog_chnl, blog_phy;       /* used if CONFIG_BLOG enabled */ \
    pNBuff_t pNBuff; 
    uint32 dqm;
    DmaDesc dmaDesc;
    BcmPktDma_EthTxDma *txdma;
    int channel;
    FkBuff_t *pFkb;
    struct sk_buff *skb;
    uint32 reclaim_idx;
    uintptr_t delayed_reclaim_array[DELAYED_RECLAIM_ARRAY_LEN];
}EnetXmitParams;

static inline void bcmeapi_buf_reclaim(EnetXmitParams *pParam)
{
    BcmPktDma_txRecycle_t txRecycle;
    BcmPktDma_txRecycle_t *txRecycle_p;

    pParam->txdma = pVnetDev0_g->txdma;
    ENET_TX_LOCK();
    pParam->reclaim_idx = 0;

    while((txRecycle_p = bcmPktDma_EthFreeXmitBufGet(pParam->txdma, &txRecycle)) != NULL)
    {
        pParam->delayed_reclaim_array[pParam->reclaim_idx] = txRecycle_p->key;

        pParam->reclaim_idx++;
        /*
         * only unlock and do reclaim if we have collected many free
         * buffers, otherwise, wait until end of function when we have
         * already released the tx lock to do reclaim.
         */
        if (pParam->reclaim_idx >= DELAYED_RECLAIM_ARRAY_LEN) {
            ENET_TX_UNLOCK();
            DO_DELAYED_RECLAIM(pParam->reclaim_idx, pParam->delayed_reclaim_array);
            pParam->reclaim_idx = 0;
            ENET_TX_LOCK();
        }
    }   /* end while(...) */
}

static inline int bcmeapi_queue_select(EnetXmitParams *pParam)
{
    BcmPktDma_EthTxDma *txdma = pVnetDev0_g->txdma;

    pParam->dqm = 0;

    if (!txdma->txEnabled || !bcmPktDma_EthXmitAvailable(txdma, pParam->dqm) )
    {
        BCM_ENET_TX_DEBUG("No more Tx Free BDs\n");
        pVnetDev0_g->stats.tx_dropped++;
        return 1;
    }

    return 0;
}
static inline void bcmeapi_config_tx_queue(EnetXmitParams *pParam)
{
    pParam->dmaDesc.status = DMA_OWN | DMA_SOP | DMA_EOP | DMA_APPEND_CRC;
}
static inline void dumpHexData1(uint8_t *pHead, uint32_t len)
{
    uint32_t i;
    uint8_t *c = pHead;
    for (i = 0; i < len; ++i) {
        if (i % 16 == 0)
            printk("\n");
        printk("0x%02X, ", *c++);
    }
    printk("\n");
}

static inline void dump_pkt(const char *pStr, uint8_t * pBuf, uint32_t len)
{
    if (0)
    {
        //int dump_len = ( len < 64) ? len : 64;
        int dump_len = len ;
        printk("%s: data<0x%p> len<%u>",pStr, pBuf, len);
        dumpHexData1(pBuf, dump_len);
        cache_flush_len((void*)pBuf, dump_len);
    }
}


static inline int bcmeapi_pkt_xmt_dispatch(EnetXmitParams *pParam)
{
    int bufSource;
    uintptr_t key;
    int param1;
    BcmPktDma_EthTxDma *txdma = pParam->txdma;
    int param2 = -1;

    key = (uintptr_t)pParam->pNBuff;
    param1 = 0;

    /* FAP is compiled out */
    bufSource = HOST_VIA_LINUX;

    dump_pkt("TX PKT: ", pParam->data, pParam->len);

#if defined(ENET_CACHE_SMARTFLUSH)
	if(!pFkb) {	/* skb */
		struct sk_buff *skbp = pParam->skb;
        uint8 *dirty_p = skb_shinfo(skbp)->dirty_p;
        uint8 *pData = pParam->data;
        uint8 *pEnd = pData + pParam->len;
				
        // If driver returned this buffer to us with a valid dirty_p,
        // then we can shorten the flush length.
        if (dirty_p) {
            if ((dirty_p < skbp->head) || (dirty_p > pEnd))
                printk("invalid dirty_p detected: %p valid=[%p %p]\n", dirty_p, skbp->head, pEnd);
            else
                pEnd = (dirty_p < pData) ? pData : dirty_p;
        }
        nbuff_flush(pParam->pNBuff, pParam->data, pEnd - pData);
    } else {
        nbuff_flush(pParam->pNBuff, pParam->data, pParam->len);
    }
#else
    nbuff_flush(pParam->pNBuff, pParam->data, pParam->len);
#endif /* ENET_CACHE_SMARTFLUSH */

        bcmPktDma_EthXmitNoCheck_Iudma(txdma,
                          pParam->data, pParam->len, bufSource,
                          pParam->dmaDesc.status, key, param1, 
                          param2);

    return 0;
}

static inline void bcmeapi_xmit_unlock_exit_post(EnetXmitParams *pXmitParams)
{
    ENET_TX_UNLOCK();
    DO_DELAYED_RECLAIM(pXmitParams->reclaim_idx, pXmitParams->delayed_reclaim_array);
}

static inline void bcmeapi_xmit_unlock_drop_exit_post(EnetXmitParams *pXmitParams)
{
    ENET_TX_UNLOCK();
    DO_DELAYED_RECLAIM(pXmitParams->reclaim_idx, pXmitParams->delayed_reclaim_array);
    nbuff_flushfree(pXmitParams->pNBuff);
}


/* --------------------------------------------------------------------------
Name: bcm63xx_enet_xmit
Purpose: Send ethernet traffic
-------------------------------------------------------------------------- */
static int bcm63xx_enet_xmit(pNBuff_t pNBuff, struct net_device *dev)
{
    EnetXmitParams param, *pParam;

    memset(&param, 0, sizeof(param));
    pParam = &param;

    //printk("bcm63xx_enet_xmit called\n");
    param.pDevPriv = netdev_priv(dev);
    param.port_id  = 0;
    param.pNBuff = pNBuff;

    if (nbuff_get_params_ext(pNBuff, &param.data, &param.len,
                &param.mark, &param.priority, &param.r_flags) == NULL)
    {
        pVnetDev0_g->stats.tx_dropped++;
        printk("%s() : nbuff_get_params_ext() reutned error... Tx pkt drop\n",__FUNCTION__);
        return 0;
    }

    if(IS_FKBUFF_PTR(pParam->pNBuff))
    {
        pParam->pFkb = PNBUFF_2_FKBUFF(pParam->pNBuff);
    }
    else
    {
        pParam->skb = PNBUFF_2_SKBUFF(pParam->pNBuff);
    }

    param.blog_chnl = param.port_id;
    param.blog_phy  = BLOG_ENETPHY;

#ifdef CONFIG_BLOG
    /*
     * Pass to blog->fcache, so it can construct the customized
     * fcache based execution stack.
     */
    blog_emit( pNBuff, dev, TYPE_ETH, param.blog_chnl, param.blog_phy ); /* CONFIG_BLOG */
#endif

    bcmeapi_buf_reclaim(pParam);

    if(bcmeapi_queue_select(pParam))
    {
        printk("%s() : bcmeapi_queue_select() reutned error... Tx pkt drop\n",__FUNCTION__);
        goto unlock_drop_exit;
    }

    bcmeapi_config_tx_queue(pParam);

    if ( pParam->len < ETH_ZLEN )
    {
        nbuff_pad(pParam->pNBuff, ETH_ZLEN - pParam->len);
        pParam->len = ETH_ZLEN;
    }

    bcmeapi_pkt_xmt_dispatch(pParam);

    pVnetDev0_g->stats.tx_bytes += pParam->len + ETH_CRC_LEN;
    pVnetDev0_g->stats.tx_packets++;
    pVnetDev0_g->dev->trans_start = jiffies;

    bcmeapi_xmit_unlock_exit_post(pParam);
    return 0;

unlock_drop_exit:
    pVnetDev0_g->stats.tx_dropped++;
    bcmeapi_xmit_unlock_drop_exit_post(pParam);
    return 0;

}

static inline void bcmeapi_napi_post(BcmEnet_devctrl *pDevCtrl)
{
    BcmEnet_RxDma *rxdma;

    /* Enable the interrupts from all RX DMA channels */
    ENET_RX_LOCK();
    rxdma = pDevCtrl->rxdma;
    pDevCtrl->rxdma->pktDmaRxInfo.rxDma->intStat = DMA_DONE | DMA_NO_DESC | DMA_BUFF_DONE;
    pDevCtrl->rxdma->pktDmaRxInfo.rxDma->intMask = DMA_DONE | DMA_NO_DESC | DMA_BUFF_DONE;
    ENET_RX_UNLOCK();
}

static int bcm63xx_enet_rx_thread(void *arg)
{
    struct BcmEnet_devctrl *pDevCtrl=(struct BcmEnet_devctrl *) arg;
    uint32 work_done;
    uint32 ret_done;
    int budget = 32;


    while (1)
    {
        wait_event_interruptible(pDevCtrl->rx_thread_wqh,
                pDevCtrl->rx_work_avail);

        if (kthread_should_stop())
        {
            printk(KERN_INFO "kthread_should_stop detected on bcmsw-rx\n");
            break;
        }

        local_bh_disable();

        work_done = bcm63xx_rx(pDevCtrl, budget);
        ret_done = work_done & ENET_POLL_DONE;
        work_done &= ~ENET_POLL_DONE;
        local_bh_enable();

        //BCM_ENET_RX_DEBUG("Work Done: %d \n", (int)work_done);

        if (ret_done == ENET_POLL_DONE)
        {
            /*
             * No more packets.  Indicate we are done (rx_work_avail=0) and
             * re-enable interrupts (bcmeapi_napi_post) and go to top of
             * loop to wait for more work.
             */
            pDevCtrl->rx_work_avail = 0;
            bcmeapi_napi_post(pDevCtrl);
        }
        else
        {
            /* We have either exhausted our budget or there are
               more packets on the DMA (or both).  Yield CPU to allow
               others to have a chance, then continue to top of loop for more
               work.  */
            if (current->policy == SCHED_FIFO || current->policy == SCHED_RR)
                yield();
        }
    }

    return 0;
}

#if defined(_CONFIG_BCM_BPM)
/* Frees a buffer for an Eth RX channel to global BPM */
static inline int enet_bpm_free_buf(BcmEnet_devctrl *pDevCtrl, int channel,
        uint8 *pData)
{
    BcmPktDma_LocalEthRxDma *rxdma = &pDevCtrl->rxdma[channel]->pktDmaRxInfo;
    gbpm_free_buf((void *) PDATA_TO_PFKBUFF(pData,BCM_PKT_HEADROOM));
    rxdma->free++;

    return GBPM_SUCCESS;
}
#endif  /* defined(_CONFIG_BCM_BPM)  */

static inline void _assign_rx_buffer(BcmEnet_devctrl *pDevCtrl, int channel, uint8 * pData)
{
    BcmPktDma_LocalEthRxDma *pktDmaRxInfo_p =
                                &pDevCtrl->rxdma->pktDmaRxInfo;

    int buf_freed = 0;

    /*
     * Disable preemption so that my cpuid will not change in this func.
     * Not possible for the state of bulk_rx_lock_active to change
     * underneath this function on the same cpu.
     */
    preempt_disable();

    ENET_RX_LOCK();

#if defined(_CONFIG_BCM_BPM)
    {
        if (pktDmaRxInfo_p->numRxBds == pktDmaRxInfo_p->rxAssignedBds)
        {
            buf_freed = 1;
            enet_bpm_free_buf(pDevCtrl, channel, pData);
        }
    }
#endif
    if (buf_freed == 0)
    {
        if (bcmPktDma_EthFreeRecvBuf(pktDmaRxInfo_p, pData) == BCM_PKTDMA_SUCCESS)
        {
#if defined(IUDMA_RX_CHAN_FLOWC)
            pDevCtrl->dmaCtrl->flowctl_ch1_alloc = 1; /* One Buffer is allocated to the DMA */
#endif
        }
    }



    ENET_RX_UNLOCK();
    preempt_enable();
}

static inline void flush_assign_rx_buffer(BcmEnet_devctrl *pDevCtrl, int channel,
                                   uint8 * pData, uint8 * pEnd)
{
    cache_flush_region(pData, pEnd);
    _assign_rx_buffer( pDevCtrl, channel, pData );
}

static inline int bcmeapi_rx_pkt(BcmEnet_devctrl *pDevCtrl, unsigned char **pBuf, FkBuff_t **pFkb, 
				   int *len, int *gemid, int *phy_port_id, int *is_wifi_port, struct net_device **dev,
                                   uint32 *rxpktgood, uint32 *context_p, int *rxQueue)
{
	DmaDesc dmaDesc;
	BcmEnet_RxDma *rxdma = pDevCtrl->rxdma;
    static unsigned int err_count = 0;
        
    *is_wifi_port = 0;

	/* rxAssignedBds is only local for non-FAP builds */
	if (rxdma->pktDmaRxInfo.rxAssignedBds == 0) {
        *rxpktgood |= ENET_POLL_DONE;
		ENET_RX_UNLOCK();
		printk("No RxAssignedBDs for this channel <%d>\n",err_count++);
		return 1;
	}


	/* Read <status,length> from Rx BD at head of ring */
	dmaDesc.word0 = bcmPktDma_EthRecv(&rxdma->pktDmaRxInfo, pBuf, len); 

	/* If no more rx packets, we are done for this channel */
	if (dmaDesc.status & DMA_OWN) {
		//BCM_ENET_RX_DEBUG("No Rx Pkts on this channel\n");
        *rxpktgood |= ENET_POLL_DONE;
		ENET_RX_UNLOCK();
		return 1;
	}

#if defined(BCM_ENET_UNIMAC)
	/* If packet is marked as "FCS error" by UNIMAC, skip it,
	 * free it and stop processing for this packet */
	if (dmaDesc.status & DMA_DESC_ERROR) {
		bcmPktDma_EthFreeRecvBuf(&rxdma->pktDmaRxInfo, *pBuf);
		ENET_RX_UNLOCK();
		return 1;
	}
#endif

	if ((*len < ENET_MIN_MTU_SIZE) ||
		(dmaDesc.status & (DMA_SOP | DMA_EOP)) != (DMA_SOP | DMA_EOP)) {
		ENET_RX_UNLOCK();
		flush_assign_rx_buffer(pDevCtrl, 0, *pBuf, *pBuf);
		pDevCtrl->stats.rx_dropped++;
		return 1;
	}

#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
    cache_invalidate_len(pBuf, BCM_MAX_PKT_LEN);
#endif /* ARM */

    dump_pkt("RX PKT: ", *pBuf, *len );
    *phy_port_id = 0;

	return 0;
}
static inline void bcmeapi_kfree_buf_irq(BcmEnet_devctrl *pDevCtrl, struct fkbuff  *pFkb, unsigned char *pBuf)
{
    flush_assign_rx_buffer(pDevCtrl, 0, pBuf, pBuf);
}

static inline void bcmeapi_blog_drop(BcmEnet_devctrl *pDevCtrl, struct fkbuff  *pFkb, unsigned char *pBuf)
{
    bcmeapi_kfree_buf_irq(pDevCtrl, NULL, pBuf);
}

#if defined(_CONFIG_BCM_BPM)
static uintptr_t eth_alloc_buf_addr[BPM_ENET_BULK_ALLOC_COUNT];
static inline int enet_bpm_alloc_buf(BcmEnet_devctrl *pDevCtrl, int channel)
{
    unsigned char *pFkBuf, *pData;
    int buf_ix;
    uint32_t *pBuf = eth_alloc_buf_addr;
    BcmPktDma_EthRxDma *rxdma = &pDevCtrl->rxdma[channel]->pktDmaRxInfo;

    if((rxdma->numRxBds - rxdma->rxAssignedBds) >= rxdma->allocTrig)
    { /* number of used buffers has crossed the trigger threshold */
        if ( (gbpm_alloc_mult_buf( rxdma->bulkAlloc, (void **)pBuf ) )
                == GBPM_ERROR)
        {
            /* may be temporarily global buffer pool is depleted.
             * Later try again */
            return GBPM_ERROR;
        }

        rxdma->alloc += rxdma->bulkAlloc;

        for (buf_ix=0; buf_ix < rxdma->bulkAlloc; buf_ix++, pBuf++)
        {
            pFkBuf = (uint8_t *) (*pBuf);
            pData = PFKBUFF_TO_PDATA(pFkBuf,BCM_PKT_HEADROOM);
            flush_assign_rx_buffer(pDevCtrl, channel, pData,
                    (uint8_t*)pFkBuf + BCM_PKTBUF_SIZE);
        }
    }

    return GBPM_SUCCESS;
}
#endif
static inline void bcmeapi_buf_alloc(BcmEnet_devctrl *pDevCtrl)
{
#if defined(_CONFIG_BCM_BPM)
    //alloc a new buf from bpm
    enet_bpm_alloc_buf( pDevCtrl, 0 );
#endif
}

static inline int bcmeapi_alloc_skb(BcmEnet_devctrl *pDevCtrl, struct sk_buff **skb)
{
	BcmEnet_RxDma *rxdma = pDevCtrl->rxdma;

	if (rxdma->freeSkbList) {
		*skb = rxdma->freeSkbList;
		rxdma->freeSkbList = rxdma->freeSkbList->next_free;
	}
	else {
		*skb = kmem_cache_alloc(enetSkbCache, GFP_ATOMIC);

		if (!(*skb)) {
			return 1;
		}
	}

	return 0;
}

/*
 * Recycling context definition
 */
typedef union {
    struct {
        /* fapQuickFree handling removed - Oct 2010 */
#if defined(CONFIG_BCM_GMAC)
        uint32 reserved     : 29;
        uint32 channel      :  3;
#else
        uint32 reserved     : 30;
        uint32 channel      :  2;
#endif
    };
    uint32 u32;
} enet_recycle_context_t;
#define RECYCLE_CONTEXT(_context)  ( (enet_recycle_context_t *)(&(_context)) )
#define FKB_RECYCLE_CONTEXT(_pFkb) RECYCLE_CONTEXT((_pFkb)->recycle_context)

static inline int bcmeapi_free_skb(BcmEnet_devctrl *pDevCtrl, 
    struct sk_buff *skb, int free_flag, int channel)
{
    BcmEnet_RxDma * rxdma;

    if( !(free_flag & SKB_RECYCLE ))
    {
        return 1;
    }

    /*
     * Disable preemption so that my cpuid will not change in this func.
     * Not possible for the state of bulk_rx_lock_active to change
     * underneath this function on the same cpu.
     */
    preempt_disable();

    ENET_RX_LOCK();

    rxdma = pDevCtrl->rxdma;
    if ((unsigned char *)skb < rxdma->skbs_p || (unsigned char *)skb >= rxdma->end_skbs_p)
    {
        kmem_cache_free(enetSkbCache, skb);
    }
    else
    {
        skb->next_free = rxdma->freeSkbList;
        rxdma->freeSkbList = skb;      
    }

    ENET_RX_UNLOCK();

    preempt_enable();
    return 0;
}

static inline void bcm63xx_enet_recycle_skb_or_data(struct sk_buff *skb,
                                             uintptr_t context, uint32 free_flag)
{
    int channel  = RECYCLE_CONTEXT(context)->channel;
    BcmEnet_devctrl *pDevCtrl = (BcmEnet_devctrl *)netdev_priv( pVnetDev0_g->dev );

    if (bcmeapi_free_skb(pDevCtrl, skb, free_flag, channel))
    { // free data
        uint8 *pData = skb->head + BCM_PKT_HEADROOM;
        uint8 *pEnd;
#if defined(ENET_CACHE_SMARTFLUSH)
        uint8 *dirty_p = skb_shinfo(skb)->dirty_p;
        uint8 *shinfoBegin = (uint8 *)skb_shinfo(skb);
        uint8 *shinfoEnd;
        if (skb_shinfo(skb)->nr_frags == 0) {
            // no frags was used on this skb, so can shorten amount of data
            // flushed on the skb_shared_info structure
            shinfoEnd = shinfoBegin + offsetof(struct skb_shared_info, frags);
        }
        else {
            shinfoEnd = shinfoBegin + sizeof(struct skb_shared_info);
        }
        cache_flush_region(shinfoBegin, shinfoEnd);

        // If driver returned this buffer to us with a valid dirty_p,
        // then we can shorten the flush length.
        if (dirty_p) {
            if ((dirty_p < skb->head) || (dirty_p > shinfoBegin)) {
                printk("invalid dirty_p detected: %p valid=[%p %p]\n",
                        dirty_p, skb->head, shinfoBegin);
                pEnd = shinfoBegin;
            } else {
                pEnd = (dirty_p < pData) ? pData : dirty_p;
            }
        } else {
            pEnd = shinfoBegin;
        }
#else
        pEnd = pData + BCM_MAX_PKT_LEN;
#endif
        flush_assign_rx_buffer(pDevCtrl, channel, pData, pEnd);
    }
}

#if defined(ENET_CACHE_SMARTFLUSH)
#error "ENET_CACHE_SMARTFLUSH defined"
#endif

static inline int bcmeapi_skb_headerinit(int len, BcmEnet_devctrl *pDevCtrl, struct sk_buff *skb, 
    FkBuff_t * pFkb, unsigned char *pBuf)
{
    uintptr_t recycle_context = 0;

    RECYCLE_CONTEXT(recycle_context)->channel = 0;

    skb_headerinit(BCM_PKT_HEADROOM,
#if defined(ENET_CACHE_SMARTFLUSH)
            SKB_DATA_ALIGN(len+BCM_SKB_TAILROOM),
#else
            BCM_MAX_PKT_LEN,
#endif
            skb, pBuf, (RecycleFuncP)bcm63xx_enet_recycle_skb_or_data,
            recycle_context,(void*)pFkb->blog_p);

    skb_trim(skb, len - ETH_CRC_LEN);

    return 0;
}
/* Callback: fkb and data recycling */
static inline void __bcm63xx_enet_recycle_fkb(struct fkbuff * pFkb,
                                              uint32 context)
{
    int channel = FKB_RECYCLE_CONTEXT(pFkb)->channel;
    BcmEnet_devctrl *pDevCtrl = (BcmEnet_devctrl *)netdev_priv(pVnetDev0_g->dev);
    uint8 *pData = PFKBUFF_TO_PDATA(pFkb,BCM_PKT_HEADROOM);

    _assign_rx_buffer(pDevCtrl, channel, pData); /* No cache flush */
}

/* Common recycle callback for fkb, skb or data */
static inline void bcm63xx_enet_recycle(pNBuff_t pNBuff, uint32 context, uint32 flags)
{
    if ( IS_FKBUFF_PTR(pNBuff) ) {
        __bcm63xx_enet_recycle_fkb(PNBUFF_2_FKBUFF(pNBuff), context);
    } else { /* IS_SKBUFF_PTR(pNBuff) */
        bcm63xx_enet_recycle_skb_or_data(PNBUFF_2_SKBUFF(pNBuff),context,flags);
    }
}

/*
 *  bcm63xx_rx: Process all received packets.
 */
static uint32 bcm63xx_rx(void *ptr, uint32 budget)
{
    BcmEnet_devctrl *pDevCtrl = ptr;
    struct net_device *dev = NULL;
    unsigned char *pBuf = NULL;
    struct sk_buff *skb = NULL;
    int len=0, phy_port_id = -1, ret;
    uint32 rxpktgood = 0, rxpktprocessed = 0;
    uint32 rxpktmax = budget + (budget / 2);
    FkBuff_t * pFkb = NULL;
    uint32_t blog_chnl, blog_phy; /* used if CONFIG_BLOG enabled */
    uint32 rxContext1 = 0; /* Dummy variable used to hold the value returned from called function -
                              MUST not be changed from caller */
    int is_wifi_port = 0;
    int rxQueue;
    int gemid = 0;
#if defined(CONFIG_BLOG)
    BlogAction_t blogAction;
    struct net_device *txdev = NULL;
#endif
    /* bulk blog locking optimization only used in SMP builds */

    // TBD -- this can be looked into but is not being done for now
    /* When the Kernel is upgraded to 2.6.24 or above, the napi call will
       tell you the received queue to be serviced. So, loop across queues
       can be removed. */
    /* RR loop across channels until either no more packets in any channel or
       we have serviced budget number of packets. The logic is to keep the
       channels to be serviced in next_channel array with channels_tbd
       tracking the number of channels that still need to be serviced. */
    for(; --budget > 0 && (rxpktgood & ENET_POLL_DONE) == 0; dev = NULL, pBuf = NULL, skb = NULL)
    {

        /* as optimization on SMP, hold blog lock across multiple pkts */
        /* must grab blog_lock before enet_rx_lock */
#if defined(CONFIG_BLOG)
        blog_lock();
#endif
        ENET_RX_LOCK();

        ret = bcmeapi_rx_pkt(pDevCtrl, &pBuf, &pFkb, &len, &gemid, &phy_port_id, &is_wifi_port, &dev, &rxpktgood, &rxContext1, &rxQueue);


        if(ret)
        {
            /* bcmeapi_rx_pkt MUST have released the lock upon error */
#if defined(CONFIG_BLOG)
            blog_unlock();
#endif
            continue;
        }

        //BCM_ENET_RX_DEBUG("Processing Rx packet\n");
        rxpktprocessed++;


        dev = pVnetDev0_g->dev;

        /* Store packet & byte count in switch structure */
        pDevCtrl->stats.rx_packets++;
        pDevCtrl->stats.rx_bytes += len;


        blog_chnl = phy_port_id;/* blog rx channel is switch port */
        blog_phy = BLOG_ENETPHY;/* blog rx phy type is ethernet */

        /* FkBuff_t<data,len> in-placed leaving headroom */
        pFkb = fkb_init(pBuf, BCM_PKT_HEADROOM,
                pBuf, len - ETH_CRC_LEN );
        {
            uint32 context = 0;

            RECYCLE_CONTEXT(context)->channel = 0;

            pFkb->recycle_hook = (RecycleFuncP)bcm63xx_enet_recycle;
            pFkb->recycle_context = context;
        }


#ifdef CONFIG_BLOG
        /* SMP: bulk rx, bulk blog optimization */
        blogAction = blog_finit_locked( pFkb, dev, TYPE_ETH, blog_chnl, blog_phy, (void **)&txdev);

        if ( blogAction == PKT_DROP )
        {
            ENET_RX_UNLOCK();
#if defined(CONFIG_BLOG)
            blog_unlock();
#endif
            bcmeapi_blog_drop(pDevCtrl, pFkb, pBuf);

            /* Store dropped packet count in our portion of the device structure */
            pDevCtrl->stats.rx_dropped++;
            continue;
        }
        else
        {
            bcmeapi_buf_alloc(pDevCtrl);
        }

        /* packet consumed, proceed to next packet*/
        if ( blogAction == PKT_DONE )
        {
            ENET_RX_UNLOCK();
            blog_unlock();
            continue;
        }
        else if ( blogAction == PKT_TCP4_LOCAL)
        {
            ENET_RX_UNLOCK();
            blog_unlock();
           
            bcm_tcp_v4_recv((void*)CAST_REAL_TO_VIRT_PNBUFF(pFkb,FKBUFF_PTR) , txdev);
            continue;
        }

#endif /* CONFIG_BLOG */

        /*allocate skb & initialize it using fkb */

        if (bcmeapi_alloc_skb(pDevCtrl, &skb)) {
            ENET_RX_UNLOCK();
#if defined(CONFIG_BLOG)
            blog_unlock();
#endif
            fkb_release(pFkb);
            pDevCtrl->stats.rx_dropped++;
            bcmeapi_kfree_buf_irq(pDevCtrl, pFkb, pBuf);
            if ( rxpktprocessed < rxpktmax )
                continue;
            break;
        }

        /*
         * We are outside of the fast path and not touching any
         * critical variables, so release all locks.
         */
        ENET_RX_UNLOCK();

#if defined(CONFIG_BLOG)
        blog_unlock();
#endif

        if(bcmeapi_skb_headerinit(len, pDevCtrl, skb, pFkb, pBuf))
        {
            bcmeapi_kfree_buf_irq(pDevCtrl, pFkb, pBuf);
            continue;
        }

        skb->protocol = bcm_type_trans(skb, dev);
        skb->dev = dev;
        
        netif_receive_skb(skb);

    } /* end while (budget > 0) */

    pDevCtrl->dev->last_rx = jiffies;

    BCM_ASSERT_NOT_HAS_SPINLOCK_C(&pVnetDev0_g->ethlock_rx);


    return rxpktgood;
}


/*
 * Set the hardware MAC address.
 */
static int bcm_set_mac_addr(struct net_device *dev, void *p)
{
    struct sockaddr *addr = p;

    if(netif_running(dev))
        return -EBUSY;

    memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);
    return 0;
}

static int bcm63xx_init_txdma_structures(int channel, BcmEnet_devctrl *pDevCtrl)
{
    BcmPktDma_LocalEthTxDma *txdma;

    pDevCtrl->txdma = (BcmPktDma_LocalEthTxDma *) (kzalloc(
                           sizeof(BcmPktDma_LocalEthTxDma), GFP_KERNEL));
    if (pDevCtrl->txdma == NULL) {
        printk("Unable to allocate memory for tx dma rings \n");
        return -ENXIO;
    }

    BCM_ENET_DEBUG("The txdma is 0x%p \n", pDevCtrl->txdma);

    txdma = pDevCtrl->txdma;
    txdma->channel = channel;




    /* init number of Tx BDs in each tx ring */
    txdma->numTxBds = bcmPktDma_EthGetTxBds( txdma, channel );

    BCM_ENET_DEBUG("Enet: txbds=%u \n", txdma->numTxBds);
    return 0;
}

static inline int get_rxIrq( int channel )
{
    int rxIrq;

#if defined(CONFIG_BCM_GMAC)
    rxIrq = INTERRUPT_ID_GMAC_DMA_0;
#else
    rxIrq = bcmPktDma_EthSelectRxIrq_Iudma(channel);
#endif
    return rxIrq;
}

#define BCMENET_WAKEUP_RXWORKER(x) do { \
           if ((x)->rx_work_avail == 0) { \
               (x)->rx_work_avail = 1; \
               wake_up_interruptible(&((x)->rx_thread_wqh)); }} while (0)

static FN_HANDLER_RT bcmeapi_enet_isr(int irq, void * pContext)
{
    /* this code should not run in DQM operation !!! */

    BcmEnet_devctrl *pDevCtrl;

    pDevCtrl = pVnetDev0_g;

    pDevCtrl->rxdma->pktDmaRxInfo.rxDma->intMask = 0;

    /* Only rx channels owned by the Host come through this ISR */
    bcmPktDma_EthClrRxIrq_Iudma(&pDevCtrl->rxdma->pktDmaRxInfo);

    BCMENET_WAKEUP_RXWORKER(pDevCtrl);

    return BCM_IRQ_HANDLED;
}
#define CONTEXT_CHAN_MASK   0x3
#define BUILD_CONTEXT(pDevCtrl,channel) \
            (uintptr_t)((uintptr_t)(pDevCtrl) | ((uintptr_t)(channel) & CONTEXT_CHAN_MASK))

static int bcm63xx_init_rxdma_structures(int channel, BcmEnet_devctrl *pDevCtrl)
{
    BcmEnet_RxDma *rxdma;

    /* init rx dma channel structures */
    pDevCtrl->rxdma = (BcmEnet_RxDma *) (kzalloc(
                           sizeof(BcmEnet_RxDma), GFP_KERNEL));
    if (pDevCtrl->rxdma == NULL) {
        printk("Unable to allocate memory for rx dma rings \n");
        return -ENXIO;
    }
    BCM_ENET_DEBUG("The rxdma is 0x%p \n", pDevCtrl->rxdma);

    rxdma = pDevCtrl->rxdma;
    rxdma->pktDmaRxInfo.channel = channel;


    /* init number of Rx BDs in each rx ring */
    rxdma->pktDmaRxInfo.numRxBds =
                    bcmPktDma_EthGetRxBds( &rxdma->pktDmaRxInfo, channel );


    {
        /* request IRQs only once at module init */
        {
            int rxIrq = bcmPktDma_EthSelectRxIrq(channel);

            rxIrq = get_rxIrq(channel);
            BcmHalMapInterrupt(bcmeapi_enet_isr,
                (void*)(BUILD_CONTEXT(pDevCtrl,channel)), rxIrq);

            printk("ENET map Rx Interrupt ID = %d\n",rxIrq);
        }
    }

    return 0;
}

static int bcm63xx_alloc_txdma_bds(int channel, BcmEnet_devctrl *pDevCtrl)
{
   BcmPktDma_EthTxDma *txdma;
   int nr_tx_bds;
   bool do_align = TRUE;
#if ( defined(CONFIG_ARM) || defined(CONFIG_ARM64) )
   uint32 phy_addr;
#endif

   txdma = pDevCtrl->txdma;
   nr_tx_bds = txdma->numTxBds;

   /* BDs allocated in bcmPktDma lib in PSM or in DDR */
#if ( defined(CONFIG_ARM) || defined(CONFIG_ARM64) )
   txdma->txBdsBase = bcmPktDma_EthAllocTxBds(&pDevCtrl->dev->dev, channel, nr_tx_bds, &phy_addr);
   txdma->txBdsPhysBase = (volatile DmaDesc *)(uintptr_t)phy_addr;
   /* Assumption : allocated BDs are 16 Byte aligned */
   txdma->txRecycleBase = kmalloc(nr_tx_bds * sizeof(BcmPktDma_txRecycle_t) + BCM_DCACHE_LINE_LEN, GFP_ATOMIC) ;
   if (txdma->txRecycleBase !=NULL) {
       memset(txdma->txRecycleBase, 0, nr_tx_bds * sizeof(BcmPktDma_txRecycle_t) + BCM_DCACHE_LINE_LEN);
   }
   txdma->txRecycle = (BcmPktDma_txRecycle_t*)(((uintptr_t)txdma->txRecycleBase + BCM_DCACHE_ALIGN_LEN) & ~BCM_DCACHE_ALIGN_LEN); 
   do_align = FALSE; /* No further alignment needed */
   txdma->txBds = txdma->txBdsBase;
#else
   txdma->txBdsBase = bcmPktDma_EthAllocTxBds(channel, nr_tx_bds);
   if ( txdma->txBdsBase == NULL )
   {
      printk("Unable to allocate memory for Tx Descriptors \n");
      return -ENOMEM;
   }

   BCM_ENET_DEBUG("bcm63xx_alloc_txdma_bds txdma->txBdsBase 0x%p nr_tx_bds = %d\n", txdma->txBdsBase,nr_tx_bds);

   txdma->txBds = txdma->txBdsBase;
   txdma->txRecycle = (BcmPktDma_txRecycle_t *)((uintptr_t)txdma->txBds + (nr_tx_bds * sizeof(DmaDesc)));

        /* Align BDs to a 16/32 byte boundary - Apr 2010 */
    if(do_align) {
        txdma->txBds = (volatile void *)(((uintptr_t)txdma->txBds + 0xF) & ~0xF);
        txdma->txBds = (volatile void *)CACHE_TO_NONCACHE(txdma->txBds);
        txdma->txRecycle = (BcmPktDma_txRecycle_t *)((uintptr_t)txdma->txBds + (nr_tx_bds * sizeof(DmaDesc)));
        txdma->txRecycle = (BcmPktDma_txRecycle_t *)NONCACHE_TO_CACHE(txdma->txRecycle);
    }
    /* BDs allocated in bcmPktDma lib in PSM or in DDR */
    memset((char *) txdma->txBds, 0, sizeof(DmaDesc) * nr_tx_bds );

#endif

   txdma->txFreeBds = nr_tx_bds;
   txdma->txHeadIndex = txdma->txTailIndex = 0;
   nr_tx_bds = txdma->numTxBds;

   return 0;
}
static void setup_txdma_channel(int channel)
{
    BcmPktDma_EthTxDma *txdma;
    volatile DmaRegs *dmaCtrl = get_dmaCtrl( );

    txdma = pVnetDev0_g->txdma;

    BCM_ENET_DEBUG("setup_txdma_channel: %d, baseDesc 0x%x\n",
        (int)channel, (unsigned int)VIRT_TO_PHYS((uint32 *)txdma->txBds));

    txdma->txDma->cfg = 0;
    txdma->txDma->maxBurst = DMA_MAX_BURST_LENGTH;
    txdma->txDma->intMask = 0;

#if ( defined(CONFIG_ARM) || defined(CONFIG_ARM64) )
    dmaCtrl->stram.s[IUDMA_TX_CHAN].baseDescPtr = (uint32)(uintptr_t)txdma->txBdsPhysBase;
#else
    dmaCtrl->stram.s[IUDMA_TX_CHAN].baseDescPtr = (uint32)VIRT_TO_PHYS((uint32 *)txdma->txBds);
#endif
}

static int init_tx_channel(BcmEnet_devctrl *pDevCtrl, int channel)
{
    BcmPktDma_LocalEthTxDma *txdma;
    volatile DmaRegs *dmaCtrl = get_dmaCtrl( );
    int phy_chan = 0;

    TRACE("bcm63xxenet: init_txdma\n");
    BCM_ENET_DEBUG("Initializing Tx channel %d \n", channel);

    /* Reset the DMA channel */
    dmaCtrl->ctrl_channel_reset = 1 << ((phy_chan * 2) + 1);
    dmaCtrl->ctrl_channel_reset = 0;

    txdma = pDevCtrl->txdma;
    txdma->txDma = &dmaCtrl->chcfg[(phy_chan * 2) + 1];

    /* allocate and assign tx buffer descriptors */
    if (bcm63xx_alloc_txdma_bds(channel,pDevCtrl) < 0)
    {
        printk("Allocate Tx BDs Failed ! ch %d \n", channel);
        return -1;
    }
    BCM_ENET_DEBUG("bcm63xx_alloc_txdma_bds() DONE :  channel %d \n", channel);

    setup_txdma_channel( channel );

    printk("ETH Init: Ch:%d - %d tx BDs at 0x%p\n", channel, txdma->numTxBds, txdma->txBds);

    bcmPktDma_EthInitTxChan(txdma->numTxBds, txdma);

    return 0;
}

/* Note: this may be called from an atomic context */
static int bcm63xx_alloc_rxdma_bds(int channel, BcmEnet_devctrl *pDevCtrl)
{
   BcmEnet_RxDma *rxdma;
   rxdma = pDevCtrl->rxdma;

#if ( defined(CONFIG_ARM) || defined(CONFIG_ARM64) )
{
   uint32 phy_addr;
   rxdma->pktDmaRxInfo.rxBdsBase = bcmPktDma_EthAllocRxBds(&pDevCtrl->dev->dev, channel, rxdma->pktDmaRxInfo.numRxBds, &phy_addr);
   if ( rxdma->pktDmaRxInfo.rxBdsBase == NULL )
   {
      printk("Unable to allocate memory for Rx Descriptors \n");
      return -ENOMEM;
   }
   rxdma->pktDmaRxInfo.rxBds = (volatile DmaDesc *)(((uintptr_t)rxdma->pktDmaRxInfo.rxBdsBase + BCM_DCACHE_ALIGN_LEN) & ~BCM_DCACHE_ALIGN_LEN);
   rxdma->pktDmaRxInfo.rxBdsPhysBase = (volatile DmaDesc *)(uintptr_t)phy_addr;
}
#else
   rxdma->pktDmaRxInfo.rxBdsBase = bcmPktDma_EthAllocRxBds(channel, rxdma->pktDmaRxInfo.numRxBds);
   if ( rxdma->pktDmaRxInfo.rxBdsBase == NULL )
   {
      printk("Unable to allocate memory for Rx Descriptors \n");
      return -ENOMEM;
   }
   /* Align BDs to a 16-byte boundary - Apr 2010 */
   rxdma->pktDmaRxInfo.rxBds = (volatile DmaDesc *)(((uintptr_t)rxdma->pktDmaRxInfo.rxBdsBase + 0xF) & ~0xF);
   rxdma->pktDmaRxInfo.rxBds = (volatile DmaDesc *)CACHE_TO_NONCACHE(rxdma->pktDmaRxInfo.rxBds);
#endif
   /* Local copy of these vars also initialized to zero in bcmPktDma channel init */
   rxdma->pktDmaRxInfo.rxAssignedBds = 0;
   rxdma->pktDmaRxInfo.rxHeadIndex = rxdma->pktDmaRxInfo.rxTailIndex = 0;

   return 0;
}
static void setup_rxdma_channel(int channel)
{
    BcmEnet_RxDma *rxdma = pVnetDev0_g->rxdma;
    volatile DmaRegs *dmaCtrl = get_dmaCtrl( );

    rxdma->pktDmaRxInfo.rxDma->cfg = 0;
    rxdma->pktDmaRxInfo.rxDma->maxBurst = DMA_MAX_BURST_LENGTH;
    rxdma->pktDmaRxInfo.rxDma->intMask = 0;
    rxdma->pktDmaRxInfo.rxDma->intStat = DMA_DONE | DMA_NO_DESC | DMA_BUFF_DONE;
    rxdma->pktDmaRxInfo.rxDma->intMask = DMA_DONE | DMA_NO_DESC | DMA_BUFF_DONE;

#if defined(IUDMA_RX_CHAN_FLOWC)
    /* Enable Flow control on RX iuDMA channel */
    dmaCtrl->controller_cfg = DMA_FLOWC_CH1_EN;
    dmaCtrl->flowctl_ch1_thresh_lo = (rxdma->pktDmaRxInfo.numRxBds/8); /* Low threshold = 1/8th */
    dmaCtrl->flowctl_ch1_thresh_hi = (rxdma->pktDmaRxInfo.numRxBds/4); /* Hi threshold = 1/4th */
    dmaCtrl->flowctl_ch1_alloc = 0; /* Allocated BDs will be incremented from _assign_rx_buffer() */
#endif

#if ( defined(CONFIG_ARM) || defined(CONFIG_ARM64) )
    dmaCtrl->stram.s[IUDMA_RX_CHAN].baseDescPtr = (uint32)(uintptr_t)rxdma->pktDmaRxInfo.rxBdsPhysBase;
    BCM_ENET_DEBUG("Setup rxdma channel %d, baseDesc 0x%p\n", (int)channel,rxdma->pktDmaRxInfo.rxBdsPhysBase);
#else
    dmaCtrl->stram.s[IUDMA_RX_CHAN].baseDescPtr = (uint32)VIRT_TO_PHYS((uint32 *)rxdma->pktDmaRxInfo.rxBds);
    BCM_ENET_DEBUG("Setup rxdma channel %d, baseDesc 0x%p\n", (int)channel,(void*)VIRT_TO_PHYS((uint32 *)rxdma->pktDmaRxInfo.rxBds));
#endif
}

static int init_buffers(BcmEnet_devctrl *pDevCtrl, int channel)
{
#if !defined(_CONFIG_BCM_BPM)
    const unsigned long BlockSize = (64 * 1024);
    const unsigned long BufsPerBlock = BlockSize / BCM_PKTBUF_SIZE;
    unsigned long AllocAmt;
    unsigned char *pFkBuf;
    int j=0;
#endif
    int i;
    unsigned char *pSkbuff;
    unsigned long BufsToAlloc;
    BcmEnet_RxDma *rxdma;
    uint32 context = 0;
    char *data;

    RECYCLE_CONTEXT(context)->channel = channel;

    TRACE(("bcm63xxenet: init_buffers\n"));

    /* allocate recieve buffer pool */
    rxdma = pDevCtrl->rxdma;
    /* Local copy of these vars also initialized to zero in bcmPktDma channel init */
    rxdma->pktDmaRxInfo.rxAssignedBds = 0;
    rxdma->pktDmaRxInfo.rxHeadIndex = rxdma->pktDmaRxInfo.rxTailIndex = 0;
    BufsToAlloc = rxdma->pktDmaRxInfo.numRxBds;

#if defined(_CONFIG_BCM_BPM)
    if (enet_bpm_alloc_buf_ring(pDevCtrl, channel, BufsToAlloc) == GBPM_ERROR)
    {
        printk(KERN_NOTICE "Eth: Low memory.\n");

        /* release all allocated receive buffers */
        enet_bpm_free_buf_ring(rxdma, channel);
        return -ENOMEM;
    }
#else
    if ( (rxdma->buf_pool = kzalloc(BufsToAlloc * sizeof(uint32_t) + BCM_DCACHE_LINE_LEN,
                                    GFP_ATOMIC)) == NULL )
    {
        printk(KERN_NOTICE "Eth: Low memory.\n");
        return -ENOMEM;
    }

    while ( BufsToAlloc )
    {
        AllocAmt = (BufsPerBlock < BufsToAlloc) ? BufsPerBlock : BufsToAlloc;
        if ( (data = kmalloc(AllocAmt * BCM_PKTBUF_SIZE + BCM_DCACHE_LINE_LEN, GFP_ATOMIC)) == NULL )
        {
            /* release all allocated receive buffers */
            printk(" ERROR : Low memory.\n");
            for (i = 0; i < j; i++)
            {
                if (rxdma->buf_pool[i])
                {
                    kfree(rxdma->buf_pool[i]);
                    rxdma->buf_pool[i] = NULL;
                }
            }
            return -ENOMEM;
        }

        rxdma->buf_pool[j++] = data;
        /* Align data buffers on 16-byte boundary - Apr 2010 */
        data = (unsigned char *) (((uintptr_t) data + BCM_DCACHE_ALIGN_LEN) & ~BCM_DCACHE_ALIGN_LEN);
        for (i = 0, pFkBuf = data; i < AllocAmt; i++, pFkBuf += BCM_PKTBUF_SIZE)
        {
            /* Place a FkBuff_t object at the head of pFkBuf */
            fkb_preinit(pFkBuf, (RecycleFuncP)bcm63xx_enet_recycle, context);
            flush_assign_rx_buffer(pDevCtrl, channel, /* headroom not flushed */
                                   PFKBUFF_TO_PDATA(pFkBuf,BCM_PKT_HEADROOM),
                                   (uint8_t*)pFkBuf + BCM_PKTBUF_SIZE);
        }
        BufsToAlloc -= AllocAmt;
    }
#endif


    if (!rxdma->skbs_p)
    { /* CAUTION!!! DONOT reallocate SKB pool */
        /*
         * Dynamic allocation of skb logic assumes that all the skb-buffers
         * in 'freeSkbList' belong to the same contiguous address range. So if you do any change
         * to the allocation method below, make sure to rework the dynamic allocation of skb
         * logic. look for kmem_cache_create, kmem_cache_alloc and kmem_cache_free functions 
         * in this file 
        */
        if ( (rxdma->skbs_p = kmalloc(
                                     (rxdma->pktDmaRxInfo.numRxBds * BCM_SKB_ALIGNED_SIZE) + BCM_DCACHE_LINE_LEN,
                                     GFP_ATOMIC)) == NULL )
            return -ENOMEM;

        memset(rxdma->skbs_p, 0,
               (rxdma->pktDmaRxInfo.numRxBds * BCM_SKB_ALIGNED_SIZE) + BCM_DCACHE_LINE_LEN);

        rxdma->freeSkbList = NULL;

        /* Chain socket skbs */
        for (i = 0, pSkbuff = (unsigned char *)
             (((unsigned long) rxdma->skbs_p + BCM_DCACHE_ALIGN_LEN) & ~BCM_DCACHE_ALIGN_LEN);
            i < rxdma->pktDmaRxInfo.numRxBds; i++, pSkbuff += BCM_SKB_ALIGNED_SIZE)
        {
            ((struct sk_buff *) pSkbuff)->next_free = rxdma->freeSkbList;
            rxdma->freeSkbList = (struct sk_buff *) pSkbuff;
        }
    }
    rxdma->end_skbs_p = rxdma->skbs_p + (rxdma->pktDmaRxInfo.numRxBds * BCM_SKB_ALIGNED_SIZE) + BCM_DCACHE_LINE_LEN;

    return 0;
}
void uninit_buffers(BcmEnet_devctrl *pDevCtrl)
{
    int i;
    BcmEnet_RxDma *rxdma = pDevCtrl->rxdma;

#if defined(_CONFIG_BCM_BPM)
    uint32 rxAddr=0;

    /* release all allocated receive buffers */
    for (i = 0; i < rxdma->pktDmaRxInfo.numRxBds; i++)
    {
        if (bcmPktDma_EthRecvBufGet_Iudma(&rxdma->pktDmaRxInfo, &rxAddr) == TRUE)
        {
            if ((uint8 *) rxAddr != NULL)
            {
                gbpm_free_buf((void *) PDATA_TO_PFKBUFF(rxAddr,BCM_PKT_HEADROOM));
            }
        }
    }

      gbpm_unresv_rx_buf(GBPM_PORT_ETH, pDevCtrl->vport_id);
#else
    /* release all allocated receive buffers */
    for (i = 0; i < rxdma->pktDmaRxInfo.numRxBds; i++) {
        if (rxdma->buf_pool[i]) {
            kfree(rxdma->buf_pool[i]);
            rxdma->buf_pool[i] = NULL;
        }
    }
    kfree(rxdma->buf_pool);
#endif

}


static int init_rx_channel(BcmEnet_devctrl *pDevCtrl, int channel)
{
    BcmEnet_RxDma *rxdma;
    volatile DmaRegs *dmaCtrl = get_dmaCtrl(  );
    int phy_chan = IUDMA_RX_CHAN;

    TRACE(("bcm63xxenet: init_rx_channel\n"));
    BCM_ENET_DEBUG("Initializing Rx channel %d \n", channel);

    /* setup the RX DMA channel */
    rxdma = pDevCtrl->rxdma;

    /* init rxdma structures */
    rxdma->pktDmaRxInfo.rxDma = &dmaCtrl->chcfg[phy_chan * 2];
    rxdma->rxIrq = get_rxIrq( channel );


    /* Reset the DMA channel */
    dmaCtrl->ctrl_channel_reset = 1 << (phy_chan * 2);
    dmaCtrl->ctrl_channel_reset = 0;

    /* allocate RX BDs */
    if (bcm63xx_alloc_rxdma_bds(channel,pDevCtrl) < 0)
        return -1;

   printk("ETH Init: Ch:%d - %d rx BDs at 0x%p\n",
          channel, rxdma->pktDmaRxInfo.numRxBds, rxdma->pktDmaRxInfo.rxBds);

    setup_rxdma_channel( channel );

    bcmPktDma_EthInitRxChan(rxdma->pktDmaRxInfo.numRxBds, &rxdma->pktDmaRxInfo);

#if defined(_CONFIG_BCM_BPM)
    enet_rx_set_bpm_alloc_trig( pDevCtrl, channel );
#endif

    /* initialize the receive buffers */
    if (init_buffers(pDevCtrl, channel)) {
        printk("ERROR : Low memory.\n");
        uninit_buffers(pDevCtrl);
        return -ENOMEM;
    }
#if defined(_CONFIG_BCM_BPM)
    gbpm_resv_rx_buf( GBPM_PORT_ETH, channel, rxdma->pktDmaRxInfo.numRxBds,
        (rxdma->pktDmaRxInfo.numRxBds * BPM_ENET_ALLOC_TRIG_PCT/100) );
#endif

//    bcm63xx_dump_rxdma(channel, rxdma);
    return 0;
}

static void reset_iudma(BcmEnet_devctrl *pDevCtrl)
{
    volatile DmaRegs *dmaCtrl = pDevCtrl->dmaCtrl;    
    volatile DmaStateRam *StateRam;

    /* Disable the DMA controller and channel */
    dmaCtrl->chcfg[IUDMA_RX_CHAN].cfg = 0;
    dmaCtrl->chcfg[IUDMA_TX_CHAN].cfg = 0;
    dmaCtrl->controller_cfg &= ~DMA_MASTER_EN;
    /* Reset RX Channel state data */
    StateRam = &dmaCtrl->stram.s[IUDMA_RX_CHAN];
    StateRam->baseDescPtr = 0 ;
    StateRam->state_data = 0 ;
    StateRam->desc_len_status = 0 ;
    StateRam->desc_base_bufptr = 0 ;
    /* Reset TX Channel state data */
    StateRam = &dmaCtrl->stram.s[IUDMA_TX_CHAN];
    StateRam->baseDescPtr = 0 ;
    StateRam->state_data = 0 ;
    StateRam->desc_len_status = 0 ;
    StateRam->desc_base_bufptr = 0 ;
}

/*
 * bcm63xx_init_dev: initialize Ethernet MACs,
 * allocate Tx/Rx buffer descriptors pool, Tx header pool.
 * Note that freeing memory upon failure is handled by calling
 * bcm63xx_uninit_dev, so no need of explicit freeing.
 */
static int bcm63xx_init_dev(BcmEnet_devctrl *pDevCtrl)
{
    int rc = 0;
    TRACE(("bcm63xxenet: bcm63xx_init_dev\n"));

    bcmenet_in_init_dev = 1;

    pDevCtrl->dmaCtrl = (DmaRegs *)(GMAC_DMA_BASE);

    reset_iudma(pDevCtrl);

    /* Initialize the Tx DMA software structures */
    rc = bcm63xx_init_txdma_structures(0, pDevCtrl);
    if (rc < 0)
        return rc;

    /* Initialize the Rx DMA software structures */
    rc = bcm63xx_init_rxdma_structures(0, pDevCtrl);

    if (rc < 0)
        return rc;

    /* allocate and assign tx buffer descriptors */
    rc = init_tx_channel(pDevCtrl, 0);
    if (rc < 0)
    {
        return rc;
    }

    rc = init_rx_channel(pDevCtrl, 0);
    if (rc < 0)
    {
        return rc;
    }
   
    bcmenet_in_init_dev = 0;
    /* if we reach this point, we've init'ed successfully */
    return 0;
}
extern BcmPktDma_Bds *bcmPktDma_Bds_p;

static void bcm63xx_uninit_txdma_structures(int channel, BcmEnet_devctrl *pDevCtrl)
{
    BcmPktDma_EthTxDma *txdma;
    int nr_tx_bds = bcmPktDma_Bds_p->host.eth_txbds[channel];

    txdma = pDevCtrl->txdma;

    /* disable DMA */
    txdma->txEnabled = 0;
    txdma->txDma->cfg = 0;
    (void) bcmPktDma_EthTxDisable(txdma);

    /* if any, free the tx skbs */
    while (txdma->txFreeBds < nr_tx_bds) {
        txdma->txFreeBds++;
        nbuff_free((void *)txdma->txRecycle[txdma->txHeadIndex++].key);
        if (txdma->txHeadIndex == nr_tx_bds)
            txdma->txHeadIndex = 0;
    }

    /* free the transmit buffer descriptor ring */
    txdma = pDevCtrl->txdma;
    /* remove the tx bd ring */
    if (txdma->txBdsBase) {
        kfree((void *)txdma->txBdsBase);
    }

    /* free the txdma channel structures */
    if (pDevCtrl->txdma) {
        kfree((void *)(pDevCtrl->txdma));
    }
}
static void bcm63xx_uninit_rxdma_structures(int channel, BcmEnet_devctrl *pDevCtrl)
{
    BcmEnet_RxDma *rxdma;

    rxdma = pDevCtrl->rxdma;
    rxdma->pktDmaRxInfo.rxDma->cfg = 0;
    (void) bcmPktDma_EthRxDisable_Iudma(&rxdma->pktDmaRxInfo);

    {
        /* free the IRQ */
        {
            int rxIrq = bcmPktDma_EthSelectRxIrq_Iudma(channel);

            free_irq(rxIrq, (BcmEnet_devctrl *)BUILD_CONTEXT(pDevCtrl,channel));

            rxIrq = get_rxIrq(channel);

            free_irq(rxIrq,
                    (BcmEnet_devctrl *)BUILD_CONTEXT(pDevCtrl,channel));
        }
    }

    /* release allocated receive buffer memory */
    uninit_buffers(pDevCtrl);

    /* free the receive buffer descriptor ring */
#if !defined(ENET_RX_BDS_IN_PSM)
    if (rxdma->pktDmaRxInfo.rxBdsBase) {
        kfree((void *)rxdma->pktDmaRxInfo.rxBdsBase);
    }
#endif

    /* free the rxdma channel structures */
    if (pDevCtrl->rxdma) {
        kfree((void *)(pDevCtrl->rxdma));
    }
}

void bcmeapi_free_queue(BcmEnet_devctrl *pDevCtrl)
{
    /* Free the Tx DMA software structures */
    bcm63xx_uninit_txdma_structures(0, pDevCtrl);

    /* Free the Rx DMA software structures and packet buffers*/
    bcm63xx_uninit_rxdma_structures(0, pDevCtrl);
#if defined(_CONFIG_BCM_BPM)
    gbpm_unresv_rx_buf( GBPM_PORT_ETH, 0 );
#endif

}

/* Uninitialize tx/rx buffer descriptor pools */
static int bcm63xx_uninit_dev(BcmEnet_devctrl *pDevCtrl)
{
    if (pDevCtrl) {

        bcmeapi_free_queue(pDevCtrl);

        /* Deleate the proc files */
        //ethsw_del_proc_files();

        /* unregister and free the net device */
        if (pDevCtrl->dev) {
            if (pDevCtrl->dev->reg_state != NETREG_UNINITIALIZED) {
                kerSysReleaseMacAddress(pDevCtrl->dev->dev_addr);
                unregister_netdev(pDevCtrl->dev);
            }
            free_netdev(pDevCtrl->dev);
        }
    }

    return 0;
}
#define ETHERNET_DEVICE_NAME     "eth0"
static atomic_t poll_lock = ATOMIC_INIT(1);
static DECLARE_COMPLETION(poll_done);

static int bcm63xx_xmit_reclaim(void)
{
    pNBuff_t pNBuff;
    BcmEnet_devctrl *pDevCtrl = (BcmEnet_devctrl *)netdev_priv(pVnetDev0_g->dev);
    BcmPktDma_txRecycle_t txRecycle;
    BcmPktDma_txRecycle_t *txRecycle_p;

    /* Obtain exclusive access to transmitter.  This is necessary because
    * we might have more than one stack transmitting at once. */
    ENET_TX_LOCK();
    while ((txRecycle_p = bcmPktDma_EthFreeXmitBufGet_Iudma(pDevCtrl->txdma, &txRecycle)) != NULL)
    {
       pNBuff = (pNBuff_t)txRecycle_p->key;

       //BCM_ENET_RX_DEBUG("bcmPktDma_EthFreeXmitBufGet TRUE! (reclaim) key 0x%p\n", pNBuff);
       if (pNBuff != PNBUFF_NULL) {
           ENET_TX_UNLOCK();
           nbuff_free(pNBuff);
           ENET_TX_LOCK();
       }
    }   /* end while(...) */
    ENET_TX_UNLOCK();

    return 0;
}

static int bcm63xx_enet_poll_timer(void * arg)
{
    int ephy_sleep_delay = 0;


    set_current_state(TASK_INTERRUPTIBLE);
    /* Sleep for 1 tick) */
    schedule_timeout(HZ/100);
    /* */
    while (atomic_read(&poll_lock) > 0)
    {
        bcm63xx_xmit_reclaim();

        /*   */
        set_current_state(TASK_INTERRUPTIBLE);

        /* Sleep for HZ jiffies (1sec), minus the time that was already */
        /* spent waiting for EPHY PLL  */
        schedule_timeout(HZ - ephy_sleep_delay);
    }

    complete_and_exit(&poll_done, 0);
    printk("bcm63xx_enet_poll_timer: thread exits!\n");

    return 0;
}

static int poll_pid = -1;
/*
 *      bcm63xx_enet_probe: - Probe Ethernet switch and allocate device
 */
int __init bcm63xx_enet_probe(void)
{
    static int probed = 0;
    struct net_device *dev = NULL;
    BcmEnet_devctrl *pDevCtrl = NULL;
    struct task_struct * bcmsw_task_struct;
    unsigned char macAddr[ETH_ALEN];
    int status = 0;

    TRACE(("bcm63xxenet: bcm63xx_enet_probe\n"));

    if (probed)
    {
        /* device has already been initialized */
        return -ENXIO;
    }
    probed++;

    dev = alloc_etherdev(sizeof(*pDevCtrl));
    if (dev == NULL)
    {
        printk("ERROR: Unable to allocate net_device!\n");
        return -ENOMEM;
    }

    pDevCtrl = netdev_priv(dev);
    memset(pDevCtrl, 0, sizeof(BcmEnet_devctrl));

    pDevCtrl->dev = dev;
    dma_set_coherent_mask(&dev->dev, DMA_BIT_MASK(32));
    pVnetDev0_g = pDevCtrl;

    spin_lock_init(&pDevCtrl->ethlock_tx);
    spin_lock_init(&pDevCtrl->ethlock_rx);

    gmac_init();

    if ((status = bcm63xx_init_dev(pDevCtrl)))
    {
        printk(("ERROR: device initialization error!\n"));
        bcm63xx_uninit_dev(pDevCtrl);
        return -ENXIO;
    }

    dev_alloc_name(dev, dev->name);
    SET_MODULE_OWNER(dev);
    sprintf(dev->name, ETHERNET_DEVICE_NAME);

    //dev->base_addr = -1;    /* Set the default invalid address to identify bcmsw device */
    //bcmeapi_add_proc_files(dev, pDevCtrl);
    dev->base_addr  = (unsigned long)pDevCtrl->rxdma->pktDmaRxInfo.rxDma;

    //ethsw_add_proc_files(dev);

    dev->netdev_ops = &bcm96xx_netdev_ops;
#if defined(SUPPORT_ETHTOOL)
    dev->ethtool_ops = &bcm63xx_enet_ethtool_ops;
    bcmenet_private_ethtool_ops = &enet_ethtool_ops;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 1)
    /*
     * In Linux 3.4, we do not use softirq or NAPI.  We create a thread to
     * do the rx processing work.
     */
    pDevCtrl->rx_work_avail = 0;
    init_waitqueue_head(&pDevCtrl->rx_thread_wqh);
    pDevCtrl->rx_thread = kthread_create(bcm63xx_enet_rx_thread, pDevCtrl, "bcmsw_rx");
    wake_up_process(pDevCtrl->rx_thread);
#else
    netif_napi_add(dev, &pDevCtrl->napi, bcm63xx_enet_poll_napi, NETDEV_WEIGHT);
#endif

    netdev_path_set_hw_port(dev, 0, BLOG_ENETPHY);

    dev->watchdog_timeo     = 2 * HZ;
    /* setting this flag will cause the Linux bridge code to not forward
       broadcast packets back to other hardware ports */
    //dev->priv_flags         = IFF_HW_SWITCH;
    dev->mtu = BCM_ENET_DEFAULT_MTU_SIZE; /* bcmsw dev : Explicitly assign the MTU size based on buffer size allocated */


    status = register_netdev(dev);

    if (status != 0)
    {
        bcm63xx_uninit_dev(pDevCtrl);
        printk("bcm63xx_enet_probe failed, returns %d\n", status);
        return status;
    }

    macAddr[0] = 0xff;
    kerSysGetMacAddress(macAddr, dev->ifindex);

    if((macAddr[0] & ETH_MULTICAST_BIT) == ETH_MULTICAST_BIT)
    {
        memcpy(macAddr, "\x00\x10\x18\x63\x00\x00", ETH_ALEN);
        printk((KERN_CRIT "%s: MAC address has not been initialized in NVRAM.\n"), dev->name);
    }

    memmove(dev->dev_addr, macAddr, ETH_ALEN);

    bcmsw_task_struct = kthread_run(bcm63xx_enet_poll_timer, NULL, "bcmsw");
    poll_pid = bcmsw_task_struct->pid;
    gmac_set_active();

    bcm63xx_enet_open(dev);

    return ((poll_pid < 0)? -ENOMEM: 0);
}

static void __exit bcmenet_module_cleanup(void)
{
    TRACE(("bcm63xxenet: bcmenet_module_cleanup\n"));

}


void display_software_stats(BcmEnet_devctrl * pDevCtrl)
{

    printk("\n\nSoftware Stats\n\n");
    printk("TxPkts:       %10lu \n", pDevCtrl->stats.tx_packets);
    printk("TxOctets:     %10lu \n", pDevCtrl->stats.tx_bytes);
    printk("TxDropPkts:   %10lu \n", pDevCtrl->stats.tx_dropped);
    printk("\n");
    printk("RxPkts:       %10lu \n", pDevCtrl->stats.rx_packets);
    printk("RxOctets:     %10lu \n", pDevCtrl->stats.rx_bytes);
    printk("RxDropPkts:   %10lu \n", pDevCtrl->stats.rx_dropped);
    printk("\n");

    //display_enet_stats(pDevCtrl);
}

#define BIT_15 0x8000
void extsw_wreg_mmap(int page, int reg, uint8 *data_in, int len)
{
    uint32 addr = ((page << 8) + reg) << 2;
    uint32 val = 0;
    uint64 data64;
    //volatile unsigned *switch_directWriteReg  = (unsigned int *) (SWITCH_BASE + SF2_DIRECT_DATA_WRITE_REG);
    void *data = &data64;
    // Add ASSERTs if address is not aligned for the requested len.

    volatile uint32 *base = (volatile uint32 *) (SWITCH_BASE + addr);
    memcpy(data, data_in, len);

    switch (len) {
        case 1:
            val = *(uint8 *)data;
            break;
        case 2:
            val = *(uint16 *)data;
            break;
        case 4:
            val = *(uint32 *)data;
            break;
            #if 0
        case 6:
            //[] = m0m1m2m3m4m5????
            *switch_directWriteReg = (data64 >> 32) & 0xffff;
            val = (uint32 )data64;
            break;
        case 8:
            // for mac vid case, you have
            // m0m1m2m3m4m5,vidL,vidM
            *switch_directWriteReg = data64 >> 32;
            val = (uint32 )data64;
            break;
            #endif
        default:
            printk("%s: len = %d NOT Handled !! \n", __FUNCTION__, len);
            return;
    }
    *base = val;
}

void extsw_rreg_mmap(int page, int reg, uint8 *data_out, int len)
{
    uint32 addr = ((page << 8) + reg) << 2;
    uint32 val;
    uint64 data64 = 0;
    //volatile unsigned *switch_directReadReg   = (unsigned int *) (SWITCH_BASE + SF2_DIRECT_DATA_READ_REG);
    void *data = &data64;
    // Add ASSERTs if address is not aligned for the requested len.

    volatile uint32 *base = (volatile uint32 *) (SWITCH_BASE + addr);

    val = *base;

    switch (len) {
        case 1:
            *(uint32 *)data = (uint8)val;
            break;
        case 2:
            *(uint32 *)data = (uint16)val;
            break;
        case 4:
            *(uint32 *)data = val;
            break;
        case 6:
            //*(uint64 *)data    = val | ((uint64)(*switch_directReadReg & 0xffff)) << 32;
            break;
        case 8:
            //*(uint64 *)data = val | ((uint64)*switch_directReadReg) << 32;
            break;
        default:
            printk("%s: len = %d NOT Handled !! \n", __FUNCTION__, len);
            break;
    }
    memcpy(data_out, data, len);
}
#define extsw_rreg_wrap(a, b, v, l) extsw_rreg_mmap(a, b, (uint8*)v, (int)l)

int sf2_bcmsw_dump_mib_ext(int port, int type)
{
    unsigned int v32, errcnt;
    //uint8 data[8] = {0};

    /* Display Tx statistics */
    printk("External Switch Stats : Port# %d\n",port);
    extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXUPKTS, &v32, 4);  // Get TX unicast packet count
    printk("TxUnicastPkts:          %10u \n", v32);
    extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXMPKTS, &v32, 4);  // Get TX multicast packet count
    printk("TxMulticastPkts:        %10u \n",  v32);
    extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXBPKTS, &v32, 4);  // Get TX broadcast packet count
    printk("TxBroadcastPkts:        %10u \n", v32);
    extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXDROPS, &v32, 4);
    printk("TxDropPkts:             %10u \n", v32);

    if (type)
    {
    /*
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXOCTETS, data, DATA_TYPE_MIB_COUNT|8);
        v32 = (((uint32*)data)[0]);
        printk("TxOctetsLo:             %10u \n", v32);
        v32 = (((uint32*)data)[1]);
        printk("TxOctetsHi:             %10u \n", v32);
        */
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TX64OCTPKTS, &v32, 4);
        printk("TxPkts64Octets:         %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TX127OCTPKTS, &v32, 4);
        printk("TxPkts65to127Octets:    %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TX255OCTPKTS, &v32, 4);
        printk("TxPkts128to255Octets:   %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TX511OCTPKTS, &v32, 4);
        printk("TxPkts256to511Octets:   %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TX1023OCTPKTS, &v32, 4);
        printk("TxPkts512to1023Octets:  %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXMAXOCTPKTS, &v32, 4);
        printk("TxPkts1024OrMoreOctets: %10u \n", v32);
//
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXQ0PKT, &v32, 4);
        printk("TxQ0Pkts:               %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXQ1PKT, &v32, 4);
        printk("TxQ1Pkts:               %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXQ2PKT, &v32, 4);
        printk("TxQ2Pkts:               %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXQ3PKT, &v32, 4);
        printk("TxQ3Pkts:               %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXQ4PKT, &v32, 4);
        printk("TxQ4Pkts:               %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXQ5PKT, &v32, 4);
        printk("TxQ5Pkts:               %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXQ6PKT, &v32, 4);
        printk("TxQ6Pkts:               %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXQ7PKT, &v32, 4);
        printk("TxQ7Pkts:               %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXCOL, &v32, 4);
        printk("TxCol:                  %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXSINGLECOL, &v32, 4);
        printk("TxSingleCol:            %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXMULTICOL, &v32, 4);
        printk("TxMultipleCol:          %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXDEFERREDTX, &v32, 4);
        printk("TxDeferredTx:           %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXLATECOL, &v32, 4);
        printk("TxLateCol:              %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXEXCESSCOL, &v32, 4);
        printk("TxExcessiveCol:         %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXFRAMEINDISC, &v32, 4);
        printk("TxFrameInDisc:          %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXPAUSEPKTS, &v32, 4);
        printk("TxPausePkts:            %10u \n", v32);
    }
    else
    {
        errcnt=0;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXCOL, &v32, 4);
        errcnt += v32;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXSINGLECOL, &v32, 4);
        errcnt += v32;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXMULTICOL, &v32, 4);
        errcnt += v32;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXDEFERREDTX, &v32, 4);
        errcnt += v32;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXLATECOL, &v32, 4);
        errcnt += v32;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXEXCESSCOL, &v32, 4);
        errcnt += v32;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_TXFRAMEINDISC, &v32, 4);
        errcnt += v32;
        printk("TxOtherErrors:          %10u \n", errcnt);
    }

    /* Display Rx statistics */
    extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXUPKTS, &v32, 4);  // Get RX unicast packet count
    printk("RxUnicastPkts:          %10u \n", v32);
    extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXMPKTS, &v32, 4);  // Get RX multicast packet count
    printk("RxMulticastPkts:        %10u \n",v32);
    extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXBPKTS, &v32, 4);  // Get RX broadcast packet count
    printk("RxBroadcastPkts:        %10u \n",v32);
    extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXDROPS, &v32, 4);
    printk("RxDropPkts:             %10u \n",v32);
    extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXDISCARD, &v32, 4);
    printk("RxDiscard:              %10u \n", v32);

    if (type)
    {
    /*
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXOCTETS, data, DATA_TYPE_MIB_COUNT|8);
        v32 = (((uint32*)data)[0]);
        printk("RxOctetsLo:             %10u \n", v32);
        v32 = (((uint32*)data)[1]);
        printk("RxOctetsHi:             %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXGOODOCT, data, DATA_TYPE_MIB_COUNT|8);
        v32 = (((uint32*)data)[0]);
        printk("RxGoodOctetsLo:         %10u \n", v32);
        v32 = (((uint32*)data)[1]);
        */
        printk("RxGoodOctetsHi:         %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXJABBERS, &v32, 4);
        printk("RxJabbers:              %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXALIGNERRORS, &v32, 4);
        printk("RxAlignErrs:            %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXFCSERRORS, &v32, 4);
        printk("RxFCSErrs:              %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXFRAGMENTS, &v32, 4);
        printk("RxFragments:            %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXOVERSIZE, &v32, 4);
        printk("RxOversizePkts:         %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXUNDERSIZEPKTS, &v32, 4);
        printk("RxUndersizePkts:        %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXPAUSEPKTS, &v32, 4);
        printk("RxPausePkts:            %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXSACHANGES, &v32, 4);
        printk("RxSAChanges:            %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXSYMBOLERRORS, &v32, 4);
        printk("RxSymbolError:          %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RX64OCTPKTS, &v32, 4);
        printk("RxPkts64Octets:         %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RX127OCTPKTS, &v32, 4);
        printk("RxPkts65to127Octets:    %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RX255OCTPKTS, &v32, 4);
        printk("RxPkts128to255Octets:   %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RX511OCTPKTS, &v32, 4);
        printk("RxPkts256to511Octets:   %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RX1023OCTPKTS, &v32, 4);
        printk("RxPkts512to1023Octets:  %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXMAXOCTPKTS, &v32, 4);
        printk("RxPkts1024OrMoreOctets: %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXJUMBOPKT , &v32, 4);
        printk("RxJumboPkts:            %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXOUTRANGEERR, &v32, 4);
        printk("RxOutOfRange:           %10u \n", v32);
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXINRANGEERR, &v32, 4);
        printk("RxInRangeErr:           %10u \n", v32);
    }
    else
    {
        errcnt=0;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXJABBERS, &v32, 4);
        errcnt += v32;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXALIGNERRORS, &v32, 4);
        errcnt += v32;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXFCSERRORS, &v32, 4);
        errcnt += v32;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXFRAGMENTS, &v32, 4);
        errcnt += v32;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXOVERSIZE, &v32, 4);
        errcnt += v32;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXUNDERSIZEPKTS, &v32, 4);
        errcnt += v32;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXSYMBOLERRORS, &v32, 4);
        errcnt += v32;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXOUTRANGEERR, &v32, 4);
        errcnt += v32;
        extsw_rreg_wrap(PAGE_MIB_P0 + (port), SF2_REG_MIB_P0_RXINRANGEERR, &v32, 4);
        errcnt += v32;
        printk("RxOtherErrors:          %10u \n", errcnt);

    }

    return 0;
}

static int bcm63xx_enet_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
    BcmEnet_devctrl *pDevCtrl;
    char *buf, *ubuf;
    int *data=(int*)rq->ifr_data/*, cb_port*/;
    ifreq_ext_t *ifx;
    struct ethswctl_data *e=(struct ethswctl_data*)rq->ifr_data;
    //struct ethctl_data *ethctl=(struct ethctl_data*)rq->ifr_data;
    //struct interface_data *enetif_data=(struct interface_data*)rq->ifr_data;
    //struct mii_ioctl_data *mii;
    int val = 0, /*mask = 0,*/ bufLen = 0, /*cum_len = 0,*/ portMap = 0;
    //int vport, phy_id;
    //struct net_device_stats *vstats;
    //struct sockaddr sockaddr;
    //int swPort;
    //int phyId;

    union {
        struct ethswctl_data ethswctl_data;
        struct ethctl_data ethctl_data;
        struct interface_data interface_data;
        ifreq_ext_t ifre;
    } rq_data;

    /* pointers pointing to ifr_data */
    e = (struct ethswctl_data*)&rq_data;
    ifx = (ifreq_ext_t *)&rq_data;

    pDevCtrl = netdev_priv(pVnetDev0_g->dev);
    ASSERT(pDevCtrl != NULL);

    switch (cmd)
    {
        case SIOCGLINKSTATE:
            if (dev == pVnetDev0_g->dev)
            {
                val = 1;
            }
            else
            {
                return -EFAULT;
            }
            if (copy_to_user((void*)data, (void*)&val, sizeof(int)))
                return -EFAULT;

            val = 0;
            break;

        case SIOCETHSWCTLOPS:
            if (copy_from_user(e, rq->ifr_data, sizeof(*e))) return -EFAULT;
            switch(e->op)
            {
                case ETHSWUNITPORT:
                    {
                        val = 0;
                        if (copy_to_user((void*)&e->unit, (void*)&val, sizeof(e->unit)))
                            return -EFAULT;
                        val = 0x1;
                        if (copy_to_user((void*)&e->port_map, (void*)&val, sizeof(e->port_map)))
                            return -EFAULT;
                    }
                    break;

                case ETHSWOAMIDXMAPPING:
                    {
                        val = 0;
                        if (copy_to_user((void*)&e->unit, (void*)&val, sizeof(e->unit)))
                            return -EFAULT;
                        val = 0x0;
                        if (copy_to_user((void*)&e->port, (void*)&val, sizeof(e->port)))
                            return -EFAULT;
                    }
                    break;

            case ETHSWDUMPMIB:
                    if (e->unit && ( e->port >= 0 && e->port <= 8 && e->port != 6) )
                    {
                        sf2_bcmsw_dump_mib_ext(e->port, e->type);
                    }
                    else
                    {
                        gmac_dump_mib(e->type);
                    }

                    display_software_stats(pDevCtrl);

                    break;

                case ETHSWPORTPAUSECAPABILITY:
                    val = 0;
                    if (copy_to_user((void*)&e->val, (void*)&val, sizeof(e->val)))
                        return -EFAULT;
                    break;

                default:
                    printk("bcm63xx_enet_ioctl() : dev=%s SIOCETHSWCTLOPS op= < %d >\n",dev->name, e->op);
                    break;
            }
            break;
        case SIOCGWANPORT:
            if (copy_from_user(e, rq->ifr_data, sizeof(*e))) return -EFAULT;
            portMap = 0;
            ubuf = e->up_len.uptr;
            bufLen = e->up_len.len;
            goto PORTMAPIOCTL;

        case SIOCIFREQ_EXT:
            if (copy_from_user(ifx, rq->ifr_data, sizeof(*ifx))) return -EFAULT;

            BCM_IOC_PTR_ZERO_EXT(ifx->stringBuf);
            ubuf = ifx->stringBuf;
            bufLen = ifx->bufLen;

            switch (ifx->opcode)
            {
                case SIOCGPORTWANONLY:
                    portMap = 0;
                    break;
                case SIOCGPORTWANPREFERRED:
                    portMap = 0;
                    break;
                case SIOCGPORTLANONLY:
                    portMap = 0x1;
                    break;
            }

PORTMAPIOCTL:   /* Common fall through code to return inteface name string based on port bit map */
            val = 0;
            if (ubuf == NULL) {
                val = -EFAULT;
                break;
            }

            buf = kmalloc(bufLen, GFP_KERNEL);
            if( buf == NULL )
            {
                printk(KERN_ERR "bcmenet:SIOCGWANPORT: kmalloc of %d bytes failed\n", bufLen);
                return -ENOMEM;
            }
            buf[0] = 'e';
            buf[1] = 't';
            buf[2] = 'h';
            buf[3] = '0';
            buf[4] = 0;

            if (portMap && copy_to_user((void*)ubuf, (void*)buf, 5))
            {
                val = -EFAULT;
            }

            kfree(buf);
            break;

        default: 
            printk("UNHANDLED !! bcm63xx_enet_ioctl() : dev=%s cmd < 0x%x %d >\n",dev->name, cmd, cmd - SIOCDEVPRIVATE);
            break;
    }
    return 0;
}

static int __init bcmenet_module_init(void)
{
    int status;

    TRACE(("bcm63xxenet: bcmenet_module_init\n"));

    if ( BCM_SKB_ALIGNED_SIZE != skb_aligned_size() )
    {
        printk("skb_aligned_size mismatch. Need to recompile enet module\n");
        return -ENOMEM;
    }

    /* create a slab cache for device descriptors */
    enetSkbCache = kmem_cache_create("bcm_EnetSkbCache",
            BCM_SKB_ALIGNED_SIZE,
            0, /* align */
            SLAB_HWCACHE_ALIGN, /* flags */
            NULL); /* ctor */
    if(enetSkbCache == NULL)
    {
        printk(KERN_NOTICE "Eth: Unable to create skb cache\n");

        return -ENOMEM;
    }

    status = bcm63xx_enet_probe();

    return status;
}
void ethsw_get_txrx_imp_port_pkts(unsigned int *tx, unsigned int *rx)
{
    *tx = 0;
    *rx = 0;

    return;
}
EXPORT_SYMBOL(ethsw_get_txrx_imp_port_pkts);

module_init(bcmenet_module_init);
module_exit(bcmenet_module_cleanup);
MODULE_LICENSE("GPL");

