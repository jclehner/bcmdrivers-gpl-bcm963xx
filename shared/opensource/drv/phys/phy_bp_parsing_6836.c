/*
   Copyright (c) 2016 Broadcom Corporation
   All Rights Reserved

    <:label-BRCM:2016:DUAL/GPL:standard
    
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
 *  Created on: Aug 2016
 *      Author: yuval.raviv@broadcom.com
 */

#include "phy_drv.h"
#include "phy_bp_parsing.h"
#include "mac_drv.h"
#include "mac_drv_unimac.h"
#include "boardparms.h"
#include "access_macros.h"
#include "bcm_map_part.h"

static bus_type_t bp_parse_bus_type(const EMAC_PORT_INFO *port_info)
{
    bus_type_t bus_type = BUS_TYPE_UNKNOWN;
    uint32_t phy_id = port_info->phy_id;
    uint32_t intf = phy_id & MAC_IFACE;

    switch (intf)
    {
    case MAC_IF_MII:
    case MAC_IF_GMII:
    case MAC_IF_RGMII:
    case MAC_IF_SGMII:
    case MAC_IF_HSGMII:
    {
        bus_type = BUS_TYPE_6836_INT;
        break;
    }
    default:
        break;
    }

    return bus_type;
}

#define RGMII_CTRL_REG              RGMII_BASE + 0x0000
#define RGMII_RX_CLOCK_DELAY_CNTRL  RGMII_BASE + 0x0008

void bp_parse_cascade_phy(const EMAC_PORT_INFO *port_info, phy_dev_t *phy_dev)
{
    phy_dev_t *phy_next;
    uint32_t phy_id = port_info->phy_id;
    uint32_t intf = phy_id & MAC_IFACE;
    uint32_t val;

    if ((phy_id & PHY_EXTERNAL) && (intf == MAC_IF_SGMII || intf == MAC_IF_HSGMII))
    {
        phy_next = phy_dev_add(PHY_TYPE_6836_SGMII, 6, NULL);
        phy_next->phy_drv->bus_drv = bus_drv_get(BUS_TYPE_6836_INT);
        phy_next->mii_type = intf == MAC_IF_SGMII ? PHY_MII_TYPE_SGMII : PHY_MII_TYPE_HSGMII;
        phy_drv_init(phy_next->phy_drv);
        phy_dev_init(phy_next);
        phy_dev->cascade_next = phy_next;
        phy_next->cascade_prev = phy_dev;
    }

    if ((phy_id & PHY_EXTERNAL) && (intf == MAC_IF_RGMII))
    {
        READ_32(RGMII_CTRL_REG, val);
        val |= (1 << 0); /* RGMII_MODE_EN=1 */
        val &= ~(7 << 2); /* Clear PORT_MODE */
        val |= (3 << 2); /* RGMII mode */

        if (phy_dev->delay_tx)
            val |= (1 << 1); /* ID_MODE_DIS=1 */
        else
            val &= ~(1 << 1); /* ID_MODE_DIS=0 */

        WRITE_32(RGMII_CTRL_REG, val);

        if (phy_dev->delay_rx)
            val = 0x28;
        else
            val = 0x08;

        WRITE_32(RGMII_RX_CLOCK_DELAY_CNTRL, val);
    }
}
EXPORT_SYMBOL(bp_parse_cascade_phy);

void bp_parse_phy_driver(const EMAC_PORT_INFO *port_info, phy_drv_t *phy_drv)
{
    bus_type_t bus_type;

    if ((bus_type = bp_parse_bus_type(port_info)) != BUS_TYPE_UNKNOWN)
        phy_drv->bus_drv = bus_drv_get(bus_type);

#if 0
    if (port >= 0 && port <= 5)
    {
#ifndef _CFE_
        phy_drv->link_change_register = acacia_link_change_register;
        phy_drv->link_change_unregister = acacia_link_change_unregister;
#endif
    }
#endif
}

phy_type_t bp_parse_phy_type(const EMAC_PORT_INFO *port_info)
{
    phy_type_t phy_type = PHY_TYPE_UNKNOWN;
    uint32_t phy_id = port_info->phy_id;
    uint32_t intf = phy_id & MAC_IFACE;

    switch (intf)
    {
    case MAC_IF_MII:
    case MAC_IF_GMII:
    {
        phy_type = PHY_TYPE_6836_EGPHY;
        break;
    }
    case MAC_IF_RGMII:
    {
        phy_type = PHY_TYPE_6836_54810;
        break;
    }
    case MAC_IF_SGMII:
    case MAC_IF_HSGMII:
    {
        if (phy_id & PHY_EXTERNAL)
            phy_type = PHY_TYPE_EXT3;
        else
            phy_type = PHY_TYPE_6836_SGMII;
        break;
    }
    default:
        break;
    }

    return phy_type;
}

void *bp_parse_phy_priv(const EMAC_PORT_INFO *port_info)
{
    return (void *)(uint64_t)port_info->switch_port;
}

mac_type_t bp_parse_mac_type(const ETHERNET_MAC_INFO *emac_info, uint32_t port)
{
    return port < 6 ? MAC_TYPE_UNIMAC : MAC_TYPE_UNKNOWN;
}

void *bp_parse_mac_priv(const ETHERNET_MAC_INFO *emac_info, uint32_t port)
{
    uint32_t priv = 0;

    /* Set the gmii_direct bit in the unimac cfg register */
    if (port >= 0 && port <= 4)
        priv = UNIMAC_DRV_PRIV_FLAG_GMII_DIRECT;

    return (void *)(uint64_t)priv;
}
