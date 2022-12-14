/*
<:copyright-BRCM:2013:DUAL/GPL:standard

   Copyright (c) 2013 Broadcom 
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

#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include "bcmenet_ethtool.h"

#if defined(SUPPORT_ETHTOOL)

/* Implementation specific Ethernet Driver private ethtool_ops
 * Driver should register private ethtool_ops here for any
 * functionality that differs from generic implementation */
const struct ethtool_ops *bcmenet_private_ethtool_ops = NULL;


static char ethtool_stats_strings[][ETH_GSTRING_LEN] = {
    [ET_TX_BYTES] =     "TxOctetsOK          ",
    [ET_TX_PACKETS] =   "TxPacketsOK         ",
    [ET_TX_ERRORS] =    "TxErrors            ",
    [ET_TX_CAPACITY] =  "TxCapacity          ",
    [ET_RX_BYTES] =     "RxOctetsOK          ",
    [ET_RX_PACKETS] =   "RxPacketsOK         ",
    [ET_RX_ERRORS] =    "RxErrors            "
};

static char ethtool_pflags_strings[][ETH_GSTRING_LEN] = {
    [ET_PF_802_1Q_VLAN]             = "802_1Q_VLAN          ",
    [ET_PF_EBRIDGE]                 = "EBRIDGE              ",
    [ET_PF_SLAVE_INACTIVE]          = "SLAVE_INACTIVE       ",
    [ET_PF_MASTER_8023AD]           = "MASTER_8023AD        ",
    [ET_PF_MASTER_ALB]              = "MASTER_ALB           ",
    [ET_PF_BONDING]                 = "BONDING              ",
    [ET_PF_SLAVE_NEEDARP]           = "SLAVE_NEEDARP        ",
    [ET_PF_ISATAP]                  = "ISATAP               ",
    [ET_PF_MASTER_ARPMON]           = "MASTER_ARPMON        ",
    [ET_PF_WAN_HDLC]                = "WAN_HDLC             ",
    [ET_PF_XMIT_DST_RELEASE]        = "XMIT_DST_RELEASE     ",
    [ET_PF_DONT_BRIDGE]             = "DONT_BRIDGE          ",
    [ET_PF_DISABLE_NETPOLL]         = "DISABLE_NETPOLL      ",
    [ET_PF_MACVLAN_PORT]            = "MACVLAN_PORT         ",
    [ET_PF_BRIDGE_PORT]             = "BRIDGE_PORT          ",
    [ET_PF_OVS_DATAPATH]            = "OVS_DATAPATH         ",
    [ET_PF_TX_SKB_SHARING]          = "TX_SKB_SHARING       ",
    [ET_PF_UNICAST_FLT]             = "UNICAST_FLT          ",
    [ET_PF_TEAM_PORT]               = "TEAM_PORT            ",
    [ET_PF_SUPP_NOFCS]              = "SUPP_NOFCS           ",
    [ET_PF_LIVE_ADDR_CHANGE]        = "LIVE_ADDR_CHANGE     ",
    [ET_PF_MACVLAN]                 = "UNICAST_FLT          ",
    [ET_PF_XMIT_DST_RELEASE_PERM]   = "XMIT_DST_RELEASE_PERM",
    [ET_PF_IPVLAN_MASTER]           = "IPVLAN_MASTER        ",
    [ET_PF_IPVLAN_SLAVE]            = "IPVLAN_SLAVE         ",
#if defined(CONFIG_BCM_KF_WL)
    [ET_PF_BCM_WLANDEV]             = "BCM_WLANDEV          ",
#endif
#if defined(CONFIG_BCM_KF_WANDEV)
    [ET_PF_WANDEV]                  = "WANDEV               ",
#endif
#if defined(CONFIG_BCM_KF_VLAN)
    [ET_PF_BCM_VLAN]                = "BCM_VLAN             ",
#endif
#if defined(CONFIG_BCM_KF_PPP)
    [ET_PF_PPP]                     = "PPP                  ",
#endif
#if defined(CONFIG_BCM_KF_RUNNER)
    [ET_PF_RNR]                     = "RUNNER               ",
#endif
#if defined(CONFIG_BCM_KF_ENET_SWITCH)  
    [ET_PF_HW_SWITCH]               = "HW_SWITCH            ",
    [ET_PF_EXT_SWITCH]              = "EXT_SWITCH           ",
#endif /* CONFIG_BCM_KF_ENET_SWITCH */
};

static int bcm63xx_get_ts_info(struct net_device *dev, struct ethtool_ts_info *info)
{
    if (bcmenet_private_ethtool_ops && bcmenet_private_ethtool_ops->get_ts_info) {
        return bcmenet_private_ethtool_ops->get_ts_info(dev, info);
    }
    return -EOPNOTSUPP;
}

static u32 bcm63xx_ethtool_get_priv_flags(struct net_device *dev)
{
    if (bcmenet_private_ethtool_ops && bcmenet_private_ethtool_ops->get_priv_flags) {
        return bcmenet_private_ethtool_ops->get_priv_flags(dev);
    }
    return (u32)(dev->priv_flags);
}

static int bcm63xx_ethtool_get_settings(struct net_device *dev, struct ethtool_cmd *ecmd)
{
    if (bcmenet_private_ethtool_ops && bcmenet_private_ethtool_ops->get_settings) {
        return bcmenet_private_ethtool_ops->get_settings(dev, ecmd);
    }
    return -EOPNOTSUPP;
}


static int bcm63xx_get_ethtool_sset_count(struct net_device *dev, int sset)
{
    switch (sset) {
    case ETH_SS_STATS:
        return ARRAY_SIZE(ethtool_stats_strings);
    case ETH_SS_PRIV_FLAGS:
        return ARRAY_SIZE(ethtool_pflags_strings);
    default:
        return -EOPNOTSUPP;
    }
}

static void bcm63xx_get_ethtool_strings(struct net_device *netdev, u32 stringset, u8 *data)
{
    switch (stringset) {
    case ETH_SS_STATS:
        memcpy(data, *ethtool_stats_strings, sizeof(ethtool_stats_strings));
        break;
    case ETH_SS_PRIV_FLAGS:
        memcpy(data, *ethtool_pflags_strings, sizeof(ethtool_pflags_strings));
        break;
    }
}

static void bcm63xx_get_ethtool_stats(struct net_device *dev, struct ethtool_stats *stats, u64 *data)
{
    if (bcmenet_private_ethtool_ops && bcmenet_private_ethtool_ops->get_ethtool_stats) {
        return bcmenet_private_ethtool_ops->get_ethtool_stats(dev, stats, data);
    }
}

#define COMPILE_TIME_CHECK(condition) ((void)sizeof(char[1 - 2*(!(condition))]))

void bcm63xx_ethtool_dummy(void)
{
    // this function is never invoked.  It is being used as a placeholder for the
    // compile time check
    COMPILE_TIME_CHECK(ARRAY_SIZE(ethtool_stats_strings) == ET_MAX);  // these two should be kept in sync

    COMPILE_TIME_CHECK(ARRAY_SIZE(ethtool_pflags_strings) == ET_PF_MAX);  // these two should be kept in sync
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_802_1Q_VLAN) == IFF_802_1Q_VLAN);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_EBRIDGE) == IFF_EBRIDGE);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_SLAVE_INACTIVE) == IFF_SLAVE_INACTIVE);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_MASTER_8023AD) == IFF_MASTER_8023AD);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_MASTER_ALB) == IFF_MASTER_ALB);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_BONDING) == IFF_BONDING);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_SLAVE_NEEDARP) == IFF_SLAVE_NEEDARP);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_ISATAP) == IFF_ISATAP);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_MASTER_ARPMON) == IFF_MASTER_ARPMON);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_WAN_HDLC) == IFF_WAN_HDLC);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_XMIT_DST_RELEASE) == IFF_XMIT_DST_RELEASE);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_DONT_BRIDGE) == IFF_DONT_BRIDGE);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_DISABLE_NETPOLL) == IFF_DISABLE_NETPOLL);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_MACVLAN_PORT) == IFF_MACVLAN_PORT);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_BRIDGE_PORT) == IFF_BRIDGE_PORT);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_OVS_DATAPATH) == IFF_OVS_DATAPATH);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_TX_SKB_SHARING) == IFF_TX_SKB_SHARING);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_UNICAST_FLT) == IFF_UNICAST_FLT);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_TEAM_PORT) == IFF_TEAM_PORT);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_SUPP_NOFCS) == IFF_SUPP_NOFCS);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_LIVE_ADDR_CHANGE) == IFF_LIVE_ADDR_CHANGE);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_MACVLAN) == IFF_MACVLAN);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_XMIT_DST_RELEASE_PERM) == IFF_XMIT_DST_RELEASE_PERM);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_IPVLAN_MASTER) == IFF_IPVLAN_MASTER);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_IPVLAN_SLAVE) == IFF_IPVLAN_SLAVE);
#if defined(CONFIG_BCM_KF_WL)
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_BCM_WLANDEV) == IFF_BCM_WLANDEV);
#endif
#if defined(CONFIG_BCM_KF_WANDEV)
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_WANDEV) == IFF_WANDEV);
#endif
#if defined(CONFIG_BCM_KF_VLAN)
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_BCM_VLAN) == IFF_BCM_VLAN);
#endif
#if defined(CONFIG_BCM_KF_PPP)
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_PPP) == IFF_PPP);
#endif
#if defined(CONFIG_BCM_KF_RUNNER)
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_RNR) == IFF_RNR);
#endif
#if defined(CONFIG_BCM_KF_ENET_SWITCH)  
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_HW_SWITCH) == IFF_HW_SWITCH);
    COMPILE_TIME_CHECK(ET_PF_2_IFF(ET_PF_EXT_SWITCH) == IFF_EXT_SWITCH);
#endif
}

const struct ethtool_ops bcm63xx_enet_ethtool_ops = {
    .get_settings =         bcm63xx_ethtool_get_settings,
    .get_ethtool_stats =    bcm63xx_get_ethtool_stats,
    .get_sset_count =       bcm63xx_get_ethtool_sset_count,
    .get_strings =          bcm63xx_get_ethtool_strings,
    .get_link     =         ethtool_op_get_link,
    .get_priv_flags =       bcm63xx_ethtool_get_priv_flags,
    .get_ts_info =          bcm63xx_get_ts_info,
};

#endif // ETHTOOL_SUPPORT

