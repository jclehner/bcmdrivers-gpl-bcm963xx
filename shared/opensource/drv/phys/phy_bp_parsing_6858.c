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

#include "phy_drv.h"
#include "phy_drv_lport.h"
#include "phy_bp_parsing.h"
#include "boardparms.h"
#include "lport_drv.h"

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
    {
        bus_type = BUS_TYPE_6858_LPORT;
        break;
    }
    case MAC_IF_SGMII:
    case MAC_IF_HSGMII:
    case MAC_IF_XFI:
    {
        if (phy_id & PHY_EXTERNAL)
            bus_type = BUS_TYPE_6858_LPORT;
        break;
    }
    default:
        break;
    }

    return bus_type;
}

void bp_parse_cascade_phy(const EMAC_PORT_INFO *port_info, phy_dev_t *phy_dev)
{
    phy_dev->disable_hd = 1;
}
EXPORT_SYMBOL(bp_parse_cascade_phy);

void bp_parse_phy_driver(const EMAC_PORT_INFO *port_info, phy_drv_t *phy_drv)
{
    bus_type_t bus_type;
    uint32_t phy_id = port_info->phy_id;
    uint32_t intf = phy_id & MAC_IFACE;

    if ((bus_type = bp_parse_bus_type(port_info)) != BUS_TYPE_UNKNOWN)
        phy_drv->bus_drv = bus_drv_get(bus_type);

    if (intf == MAC_IF_MII || intf == MAC_IF_GMII)
    {
#ifndef _CFE_
        phy_drv->link_change_register = lport_link_change_register;
        phy_drv->link_change_unregister = lport_link_change_unregister;
#endif
    }
}

phy_type_t bp_parse_phy_type(const EMAC_PORT_INFO *port_info)
{
    phy_type_t phy_type = PHY_TYPE_UNKNOWN;
    uint32_t phy_id = port_info->phy_id;
    uint32_t intf = phy_id & MAC_IFACE;
    uint32_t addr = phy_id & BCM_PHY_ID_M;
    int port_flags = port_info->port_flags;
    int is_wan_only = port_flags & PORT_FLAG_WAN_ONLY ? 1 : 0;

    switch (intf)
    {
    case MAC_IF_MII:
    case MAC_IF_GMII:
    {
        phy_type = PHY_TYPE_6858_EGPHY;
        break;
    }
    case MAC_IF_RGMII:
    {
        if (phy_id & PHY_EXTERNAL)
        {
            if (addr == 0x18 || addr == 0x19)
                phy_type = PHY_TYPE_EXT2;
            else
                phy_type = PHY_TYPE_EXT1;
        }
        break;
    }
    case MAC_IF_SGMII:
        if (phy_id & PHY_EXTERNAL)
            phy_type = PHY_TYPE_EXT2;
        else
            phy_type = PHY_TYPE_LPORT_SERDES;
        break;
    case MAC_IF_HSGMII:
    case MAC_IF_XFI:
    {
        if (phy_id & PHY_EXTERNAL)
            phy_type = PHY_TYPE_EXT3;
        else if (!is_wan_only)
            phy_type = PHY_TYPE_LPORT_SERDES;
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
    return port < 8 ? MAC_TYPE_LPORT : MAC_TYPE_UNKNOWN;
}

void *bp_parse_mac_priv(const ETHERNET_MAC_INFO *emac_info, uint32_t port)
{
    LPORT_PORT_MUX_SELECT mux_sel = PORT_UNAVAIL;
    uint32_t phy_id = emac_info->sw.phy_id[port];
    uint32_t intf = phy_id & MAC_IFACE;

    if (port >= 8)
        return NULL;

    switch (intf)
    {
    case MAC_IF_MII:
    case MAC_IF_GMII:
    {
        mux_sel = PORT_GPHY;
        break;
    }
    case MAC_IF_RGMII:
    {
        mux_sel = PORT_RGMII;
        break;
    }
    case MAC_IF_SGMII:
    {
        if (phy_id & PHY_EXTERNAL)
            mux_sel = PORT_SGMII_AN_SLAVE;
        else
            mux_sel = PORT_SGMII_AN_IEEE_CL37;

        break;
    }
    case MAC_IF_HSGMII:
    {
        mux_sel = PORT_HSGMII;
        break;
    }
    case MAC_IF_XFI:
    {
        mux_sel = PORT_XFI;
        break;
    }
    default:
        break;
    }

    return (void *)mux_sel;
}
