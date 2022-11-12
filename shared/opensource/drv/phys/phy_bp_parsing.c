/*
   Copyright (c) 2015 Broadcom Corporation
   All Rights Reserved

    <:label-BRCM:2015:DUAL/GPL:standard
    
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

/*
 *  Created on: Dec 2015
 *      Author: yuval.raviv@broadcom.com
 */

#include "phy_bp_parsing.h"
#include "phy_drv.h"
#include "boardparms.h"

static uint32_t bp_parse_phy_addr(const EMAC_PORT_INFO *port_info)
{
    return port_info->phy_id;
}

static phy_mii_type_t bp_parse_mii_type(const EMAC_PORT_INFO *port_info)
{
    uint32_t phy_id = port_info->phy_id;
    uint32_t intf = phy_id & MAC_IFACE;
    phy_mii_type_t mii_type = PHY_MII_TYPE_UNKNOWN;

    switch (intf)
    {
    case MAC_IF_MII:
        mii_type = PHY_MII_TYPE_MII;
        break;
    case MAC_IF_TMII:
        mii_type = PHY_MII_TYPE_TMII;
        break;
    case MAC_IF_INVALID:    /* treat as GMII as default, if not specified */
    case MAC_IF_GMII:
        mii_type = PHY_MII_TYPE_GMII;
        break;
    case MAC_IF_RGMII:
        mii_type = PHY_MII_TYPE_RGMII;
        break;
    case MAC_IF_SGMII:
        mii_type = PHY_MII_TYPE_SGMII;
        break;
    case MAC_IF_HSGMII:
        mii_type = PHY_MII_TYPE_HSGMII;
        break;
    case MAC_IF_SERDES:
        mii_type = PHY_MII_TYPE_SERDES;
        break;
    case MAC_IF_XFI:
        mii_type = PHY_MII_TYPE_XFI;
        break;
    default:
        mii_type = PHY_MII_TYPE_UNKNOWN;
        break;
    }

    return mii_type;
}

static void bp_parse_port_flags(const EMAC_PORT_INFO *port_info, phy_dev_t *phy_dev)
{
    int port_flags = port_info->port_flags;

    phy_dev->delay_rx = !IsPortRxInternalDelay(port_flags);
    phy_dev->delay_tx = !IsPortTxInternalDelay(port_flags);

    phy_dev->swap_pair = IsPortSwapPair(port_flags);
}

phy_dev_t *bp_parse_phy_dev(const EMAC_PORT_INFO *port_info)
{
    phy_dev_t *phy_dev = NULL;
    phy_type_t phy_type;
    uint32_t addr, meta_id;
    void *priv;

    phy_type = bp_parse_phy_type(port_info);

    if (phy_type == PHY_TYPE_UNKNOWN)
        goto Exit;

    meta_id = bp_parse_phy_addr(port_info);
    addr = meta_id & BCM_PHY_ID_M;
    priv = bp_parse_phy_priv(port_info);

    if ((phy_dev = phy_dev_add(phy_type, addr, priv)) == NULL)
        goto Exit;

    phy_dev->meta_id = meta_id;
    phy_dev->mii_type = bp_parse_mii_type(port_info);
    bp_parse_port_flags(port_info, phy_dev);
    bp_parse_phy_driver(port_info, phy_dev->phy_drv);


Exit:
    return phy_dev;
}
EXPORT_SYMBOL(bp_parse_phy_dev);

mac_dev_t *bp_parse_mac_dev(const ETHERNET_MAC_INFO *emac_info, uint32_t port)
{
    mac_dev_t *mac_dev = NULL;
    mac_type_t mac_type;
    void *priv;

    if ((mac_type = bp_parse_mac_type(emac_info, port)) == MAC_TYPE_UNKNOWN)
        goto Exit;

    priv = bp_parse_mac_priv(emac_info, port);

    if ((mac_dev = mac_dev_add(mac_type, port, priv)) == NULL)
        goto Exit;

Exit:
    return mac_dev;
}
EXPORT_SYMBOL(bp_parse_mac_dev);
