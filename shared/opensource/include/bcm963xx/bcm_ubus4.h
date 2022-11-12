/*
 * <:copyright-BRCM:2017:DUAL/GPL:standard
 * 
 *    Copyright (c) 2017 Broadcom 
 *    All Rights Reserved
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as published by
 * the Free Software Foundation (the "GPL").
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * 
 * A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
 * writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * :>
 */

#ifndef __BCM_UBUS4_H__
#define __BCM_UBUS4_H__

#if defined (CONFIG_BCM96858) || defined(_BCM96858_) 
#define UCB_NODE_ID_SLV_SYS     0
#define UCB_NODE_ID_MST_PCIE0   3
#define UCB_NODE_ID_SLV_PCIE0   4
#define UCB_NODE_ID_MST_PCIE2   5
#define UCB_NODE_ID_SLV_PCIE2   6
#define UCB_NODE_ID_MST_SATA    UCB_NODE_ID_MST_PCIE2
#define UCB_NODE_ID_SLV_SATA    UCB_NODE_ID_SLV_PCIE2
#define UCB_NODE_ID_MST_USB     14
#define UCB_NODE_ID_SLV_USB     15
#define UCB_NODE_ID_SLV_LPORT   19
#define UCB_NODE_ID_SLV_WAN     21

typedef enum
{
    MST_PORT_NODE_PCIE0,
    MST_PORT_NODE_PCIE1,
    MST_PORT_NODE_B53,
    MST_PORT_NODE_SATA,
    MST_PORT_NODE_USB,
    MST_PORT_NODE_PMC,
    MST_PORT_NODE_APM,
    MST_PORT_NODE_PER,
    MST_PORT_NODE_DMA0,
    MST_PORT_NODE_DMA1,
    MST_PORT_NODE_RQ0,
    MST_PORT_NODE_RQ1,
    MST_PORT_NODE_RQ2,
    MST_PORT_NODE_RQ3,
    MST_PORT_NODE_NATC,
    MST_PORT_NODE_DQM,
    MST_PORT_NODE_QM,
    MST_PORT_NODE_LAST
}MST_PORT_NODE;

int ubus_master_set_rte_addr(MST_PORT_NODE node, int port, int val);
#endif


#if defined(CONFIG_BCM96836) || defined(_BCM96836_) || defined(CONFIG_BCM96856) || defined(_BCM96856_)
#define UCB_NODE_ID_SLV_SYS     0
#define UCB_NODE_ID_MST_PCIE0   3
#define UCB_NODE_ID_SLV_PCIE0   4
#if !defined(CONFIG_BCM96856) && !defined(_BCM96856_)
#define UCB_NODE_ID_MST_SATA    10 
#define UCB_NODE_ID_SLV_SATA    11 
#endif
#define UCB_NODE_ID_MST_USB     14
#define UCB_NODE_ID_SLV_USB     15
#define UCB_NODE_ID_SLV_MEMC    16
#elif defined(CONFIG_BCM96846) || defined(_BCM96846_)
#define UCB_NODE_ID_MST_PCIE0   3
#define UCB_NODE_ID_SLV_PCIE0   4
#define UCB_NODE_ID_MST_USB     14
#define UCB_NODE_ID_SLV_USB     15
#define UCB_NODE_ID_SLV_MEMC    16
#endif


#if defined (CONFIG_BCM96836) || defined(_BCM96836_) || defined(CONFIG_BCM96846) || defined(_BCM96846_) || defined (CONFIG_BCM96856) || defined(_BCM96856_) 
extern unsigned int g_board_size_power_of_2;

typedef enum
{
    MST_PORT_NODE_PCIE0, 
#if defined (CONFIG_BCM96836) || defined(_BCM96836_)
    MST_PORT_NODE_SATA, 
#endif
    MST_PORT_NODE_USB,   
    MST_PORT_NODE_PER,   
    MST_PORT_NODE_PERDMA,
    MST_PORT_NODE_DMA0,     
    MST_PORT_NODE_RQ0,   
    MST_PORT_NODE_NATC,  
    MST_PORT_NODE_DQM,      
    MST_PORT_NODE_QM,    
    MST_PORT_NODE_LAST   
}MST_PORT_NODE;

#endif

#if defined (CONFIG_BCM963158) || defined(_BCM963158_)
typedef enum
{
    MST_PORT_NODE_B53,
    MST_PORT_NODE_USB,
    MST_PORT_NODE_PCIE0,
    MST_PORT_NODE_PCIE3,
    MST_PORT_NODE_PCIE2,
    MST_PORT_NODE_PMC,
    MST_PORT_NODE_PER,
    MST_PORT_NODE_PERDMA,
    MST_PORT_NODE_DSLCPU,
    MST_PORT_NODE_DSL,
    MST_PORT_NODE_SPU,
    MST_PORT_NODE_QM,
    MST_PORT_NODE_DQM,
    MST_PORT_NODE_NATC,
    MST_PORT_NODE_DMA0,
    MST_PORT_NODE_RQ0,
    MST_PORT_NODE_SWH,
    MST_PORT_NODE_DMA1,
    MST_PORT_NODE_LAST
}MST_PORT_NODE;

#define UBUS_MAX_PORT_NUM        32

#define UBUS_PORT_ID_BIU         2
#define UBUS_PORT_ID_DMA         24
#define UBUS_PORT_ID_DQM         23
#define UBUS_PORT_ID_DSLCPU      11
#define UBUS_PORT_ID_DSL         6
#define UBUS_PORT_ID_FPM         21
#define UBUS_PORT_ID_MEMC        1
#define UBUS_PORT_ID_NATC        26
#define UBUS_PORT_ID_PCIE0       8
#define UBUS_PORT_ID_PCIE2       9
#define UBUS_PORT_ID_PCIE3       10
#define UBUS_PORT_ID_PERDMA      7
#define UBUS_PORT_ID_PER         3
#define UBUS_PORT_ID_PMC         13
#define UBUS_PORT_ID_PSRAM       16
#define UBUS_PORT_ID_QM          22
#define UBUS_PORT_ID_RQ0         32
#define UBUS_PORT_ID_SPU         5
#define UBUS_PORT_ID_SWH         14
#define UBUS_PORT_ID_SYS         31
#define UBUS_PORT_ID_SYSXRDP     27
#define UBUS_PORT_ID_USB         4
#define UBUS_PORT_ID_VPB         20
#define UBUS_PORT_ID_WAN         12

#endif

#ifdef CONFIG_BCM_GLB_COHERENCY
#define IS_DDR_COHERENT 1
#else
#define IS_DDR_COHERENT 0
#endif


typedef struct ubus_credit_cfg {
    int port_id;
    int credit;
}ubus_credit_cfg_t;

#if defined (CONFIG_BCM96858) || defined(_BCM96858_) || defined (CONFIG_BCM96836) || defined(_BCM96836_) || \
    defined (CONFIG_BCM96846) || defined(_BCM96846_) || defined(CONFIG_BCM963158) || \
    defined (CONFIG_BCM96856) || defined(_BCM96856_)
int ubus_master_decode_wnd_cfg(MST_PORT_NODE node, int win, unsigned int phys_addr, unsigned int size_power_of_2, int port_id, unsigned int cache_bit_en);
int log2_32 (unsigned int value);
int ubus_master_set_token_credits(MST_PORT_NODE node, int token, int credits);
void ubus_deregister_port(int ucbid);
void ubus_register_port(int ucbid);
#endif
#if defined(CONFIG_BCM963158)
void apply_ubus_credit_each_master(int master);
#endif
void bcm_ubus_config(void);

#endif
