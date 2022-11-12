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
 *  Created on: Dec 2017
 *      Author: yuval.raviv@broadcom.com
 */

/*
 * Phy driver for external 1G phy BCM54810 when connected to a BCM96836 chip (workaround required)
 */

#include "bus_drv.h"
#include "phy_drv.h"
#include "phy_drv_mii.h"
#include "phy_drv_brcm.h"
#include "access_macros.h"
#include "bcm_map_part.h"

#define RGMII_CTRL_REG              RGMII_BASE + 0x0000
#define REG_18_MISC_CTRL            0x07

static int _phy_init(phy_dev_t *phy_dev)
{
    int ret;

    /* Access IEEE register set instead of LRE */
    if ((ret = phy_bus_write(phy_dev, 0x0e, 0x06)))
        goto Exit;

    if ((ret = mii_init(phy_dev)))
        goto Exit;

    /* Disable BroadR-Reach function */
    if ((ret = brcm_exp_write(phy_dev, 0x90, 0x00)))
        goto Exit;

    if ((ret = brcm_shadow_rgmii_init(phy_dev)))
        goto Exit;

    if ((ret = brcm_shadow_18_force_auto_mdix_set(phy_dev, 1)))
        goto Exit;

    if ((ret = brcm_shadow_18_eth_wirespeed_set(phy_dev, 1)))
        goto Exit;

Exit:
    return ret;
}

static int _phy_mode_set(phy_dev_t *phy_dev)
{
    int ret;
    uint32_t val32;
    uint16_t val16;
    int is_rgmii = phy_dev->speed == PHY_SPEED_1000;

    READ_32(RGMII_CTRL_REG, val32);

    val32 &= ~(7 << 2); /* Clear PORT_MODE */

    if (is_rgmii)
    {
        val32 |= (3 << 2); /* RGMII mode */
        val32 &= ~(1 << 5); /* RVMII_REF_SEL=0 */
        val32 &= ~(1 << 14); /* RX_ERR_MASK=0 */
    }
    else
    {
        val32 |= (4 << 2); /* RvMII mode */
        val32 |= (1 << 5); /* RVMII_REF_SEL=1 */
        val32 |= (1 << 14); /* RX_ERR_MASK=1 */
    }

    WRITE_32(RGMII_CTRL_REG, val32);

    if ((ret = brcm_shadow_18_read(phy_dev, REG_18_MISC_CTRL, &val16)))
        goto Exit;

    if (is_rgmii)
    {
        val16 |= (1 << 7); /* Enable RGMII Mode */
        val16 |= (1 << 8); /* Enable RGMII RXC Delay */
    }
    else
    {
        val16 &= ~(1 << 7); /* Disable RGMII Mode */
        val16 &= ~(1 << 8); /* Disable RGMII RXC Delay */
    }

    if ((ret = brcm_shadow_18_write(phy_dev, REG_18_MISC_CTRL, val16)))
        goto Exit;

    if ((ret = brcm_exp_write(phy_dev, 0x9f, is_rgmii ? 0x00 : 0x0f)))
        goto Exit;

Exit:
    return ret;
}

static int _phy_read_status(phy_dev_t *phy_dev)
{
    int ret;
    phy_speed_t speed = phy_dev->speed;

    if ((ret = brcm_read_status(phy_dev)))
        goto Exit;

    if (!phy_dev->link || phy_dev->speed == speed)
        goto Exit;

    _phy_mode_set(phy_dev);

Exit:
    return ret;
}

phy_drv_t phy_drv_6836_54810 =
{
    .phy_type = PHY_TYPE_6836_54810,
    .name = "EXT1",
    .power_set = mii_power_set,
    .apd_set = brcm_shadow_1c_apd_set,
    .read_status = _phy_read_status,
    .speed_set = mii_speed_set,
    .caps_get = mii_caps_get,
    .caps_set = mii_caps_set,
    .phyid_get = mii_phyid_get,
    .init = _phy_init,
};
