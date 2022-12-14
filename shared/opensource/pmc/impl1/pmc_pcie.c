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
#ifndef _CFE_
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#endif
#include <boardparms.h>

#include "pmc_drv.h"
#include "pmc_pcie.h"
#include "BPCM.h"
#include "bcm_ubus4.h"
#include "bcm_map_part.h"

extern unsigned int g_board_size_power_of_2;

static const int pmc_pcie_pmb_addr[]= {
#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148) ||  defined(CONFIG_BCM96846)
	PMB_ADDR_PCIE0,
	PMB_ADDR_PCIE1
#elif defined(CONFIG_BCM94908)
	PMB_ADDR_PCIE0,
	PMB_ADDR_PCIE1,
	PMB_ADDR_PCIE2
#elif defined(CONFIG_BCM963158)
	PMB_ADDR_PCIE0,
	PMB_ADDR_PCIE1,
	PMB_ADDR_PCIE2,
	PMB_ADDR_PCIE3,
#elif defined(CONFIG_BCM96858) || defined(CONFIG_BCM96836) || defined(CONFIG_BCM96856)
	PMB_ADDR_PCIE0,
	PMB_ADDR_PCIE1,
	PMB_ADDR_PCIE_UBUS
#else /* CONFIG_BCM963381, CONFIG_BCM96848 */
	PMB_ADDR_PCIE0
#endif
};

#define MAX_PCIE_UNIT	(sizeof(pmc_pcie_pmb_addr)/sizeof(int))

#if defined (CONFIG_BCM96858) || defined (CONFIG_BCM96836) || defined(CONFIG_BCM96856)
/* registration/deregistration is automatic enable if needed */
//#define UBUS_NODE_REGISTRATION            1
#endif

#define PCIE_INVALID_PORT                 0
#define PCIE_STANDALONE_PORT              1
#define PCIE_BIFURCATED_PORT              2

typedef struct {
    int port_type;
    int slv_node;
    int mst_node;
} ubus_node_info_t;

/* PCIe port UBUS */
static const ubus_node_info_t pmc_pcie_ubus_node[] = {
#if defined (CONFIG_BCM96858)
	{PCIE_BIFURCATED_PORT, UCB_NODE_ID_SLV_PCIE0, UCB_NODE_ID_MST_PCIE0},
	{PCIE_BIFURCATED_PORT, UCB_NODE_ID_SLV_PCIE0, UCB_NODE_ID_MST_PCIE0},
	{PCIE_STANDALONE_PORT, UCB_NODE_ID_SLV_PCIE2, UCB_NODE_ID_MST_PCIE2}
#elif defined (CONFIG_BCM96836)
	{PCIE_BIFURCATED_PORT, UCB_NODE_ID_SLV_PCIE0, UCB_NODE_ID_MST_PCIE0},
	{PCIE_BIFURCATED_PORT, UCB_NODE_ID_SLV_PCIE0, UCB_NODE_ID_MST_PCIE0},
	{PCIE_STANDALONE_PORT, UCB_NODE_ID_SLV_SATA, UCB_NODE_ID_MST_SATA}
#elif defined(CONFIG_BCM96856)
	{PCIE_BIFURCATED_PORT, UCB_NODE_ID_SLV_PCIE0, UCB_NODE_ID_MST_PCIE0},
	{PCIE_BIFURCATED_PORT, UCB_NODE_ID_SLV_PCIE0, UCB_NODE_ID_MST_PCIE0}
#else
	/* SoC that does not require UBUS registration */
	{PCIE_INVALID_PORT, -1, -1},
	{PCIE_INVALID_PORT, -1, -1},
	{PCIE_INVALID_PORT, -1, -1},
	{PCIE_INVALID_PORT, -1, -1}
#endif
};

void pmc_pcie_register_ubus(int unit)
{
	/* Skip registration for ubus earlier than UBUS4 */
	if (pmc_pcie_ubus_node[unit].port_type == PCIE_INVALID_PORT) {
	    return;
	}

#if defined(UBUS_NODE_REGISTRATION)
	/* Register slave node */
	ubus_register_port(pmc_pcie_ubus_node[unit].slv_node);

	/* Register master node */
	ubus_register_port(pmc_pcie_ubus_node[unit].mst_node);
#endif /* UBUS_NODE_REGISTRATION */

	return;
}

void pmc_pcie_deregister_ubus(int unit)
{

	/* Skip deregistration for ubus earlier than UBUS4 */
	/* skip deregister for bifurcated ports */
	if (pmc_pcie_ubus_node[unit].port_type != PCIE_STANDALONE_PORT) {
	    return;
	}

#if defined(UBUS_NODE_REGISTRATION)
	/* unegister slave node */
	ubus_deregister_port(pmc_pcie_ubus_node[unit].slv_node);

	/* Unegister master node */
	ubus_deregister_port(pmc_pcie_ubus_node[unit].mst_node);
#endif /* UBUS_NODE_REGISTRATION */

	return;
}

/**
 * Re-configure UBUS4 for the given PCI-E core.
 * - master token credits for PCIE -> Runner access
 * - master decode window
 */
static void pmc_pcie_ubus_init(int unit)
{
#if defined(CONFIG_BCM96858)
    MST_PORT_NODE unit2mst_node_tbl[] = {MST_PORT_NODE_PCIE0, MST_PORT_NODE_PCIE0, MST_PORT_NODE_PCIE1};
    /* These credits of PCIe to Runner quads are requiered to Wakeup runner
     * in case of DHD Offload RxComplete */

    ubus_master_set_token_credits(unit2mst_node_tbl[unit], 32, 1);
    ubus_master_set_token_credits(unit2mst_node_tbl[unit], 33, 1);
    ubus_master_set_token_credits(unit2mst_node_tbl[unit], 34, 1);
    ubus_master_set_token_credits(unit2mst_node_tbl[unit], 35, 1);
#elif defined(CONFIG_BCM96836)
    MST_PORT_NODE unit2mst_node_tbl[] = {MST_PORT_NODE_PCIE0, MST_PORT_NODE_PCIE0, MST_PORT_NODE_SATA};
    /* These credits of PCIe to Runner quads are requiered to Wakeup runner
     * in case of DHD Offload RxComplete */    
    ubus_master_set_token_credits(unit2mst_node_tbl[unit], 9, 1);
#elif defined(CONFIG_BCM96846)
    /*In 6846 both PCIe shares the same UBUS Master*/
    MST_PORT_NODE unit2mst_node_tbl[] = {MST_PORT_NODE_PCIE0, MST_PORT_NODE_PCIE0};
    ubus_master_set_token_credits(unit2mst_node_tbl[unit], 9, 1);
#elif defined(CONFIG_BCM963158)
    /*In 63158 PCIe#0,1 are bifurcated ports and shares the same UBUS Master */
    MST_PORT_NODE unit2mst_node_tbl[] = {MST_PORT_NODE_PCIE0, MST_PORT_NODE_PCIE0, MST_PORT_NODE_PCIE2, MST_PORT_NODE_PCIE3};
    apply_ubus_credit_each_master(unit2mst_node_tbl[unit]);
#elif defined(CONFIG_BCM96856)
    MST_PORT_NODE unit2mst_node_tbl[] = {MST_PORT_NODE_PCIE0, MST_PORT_NODE_PCIE0};
    /* These credits of PCIe to Runner quads are requiered to Wakeup runner
     * in case of DHD Offload RxComplete */    
    ubus_master_set_token_credits(unit2mst_node_tbl[unit], 9, 1);
#endif

#if defined(CONFIG_BCM_UBUS_DECODE_REMAP) && (defined(CONFIG_BCM96858) || defined(CONFIG_BCM96836) || defined(CONFIG_BCM963158) || defined(CONFIG_BCM96856))
    ubus_master_decode_wnd_cfg(unit2mst_node_tbl[unit],
								  0,
								  0,
								  g_board_size_power_of_2,
								  DECODE_CFG_PID_B53,
								  IS_DDR_COHERENT ? 1 : 0);
#endif
}

void pmc_pcie_power_up(int unit)
{
	BPCM_SR_CONTROL sr_ctrl = {
		.Bits.sr = 0, // Only iddq
	};
	int addr, dual_lane;

	if (unit >= MAX_PCIE_UNIT)
		BUG_ON(1);

	addr = pmc_pcie_pmb_addr[unit];

	if (PowerOnZone(addr, 0))
		BUG_ON(1);

	mdelay(10);

	pmc_pcie_register_ubus(unit);

	if ((BpGetPciPortDualLane(unit, &dual_lane) == BP_SUCCESS) && dual_lane) {
#if defined(CONFIG_BCM963158)
	    int bifur_addr;

	    /* Power up the other bi-furcated port */
	    bifur_addr = pmc_pcie_pmb_addr[unit+1];
	    if (PowerOnZone(bifur_addr, 0))
		    BUG_ON(1);
	    mdelay(10);
	    if (WriteBPCMRegister(bifur_addr, BPCMRegOffset(sr_control), sr_ctrl.Reg32))
		    BUG_ON(1);
#endif
	    sr_ctrl.Reg32 = 0x80; /* bit7: 1 - Strap override for dual lane support */
	                          /* bit6: Strap value, 0 - dual lane, 1 - single lane */
	    /* reset */
	    if (WriteBPCMRegister(addr, BPCMRegOffset(sr_control), 0xff))
	        BUG_ON(1);

	    mdelay(10);
	}

	if (WriteBPCMRegister(addr, BPCMRegOffset(sr_control), sr_ctrl.Reg32))
		BUG_ON(1);

	pmc_pcie_ubus_init(unit);
}

void pmc_pcie_power_down(int unit)
{
	BPCM_SR_CONTROL sr_ctrl = {
		.Bits.sr = 4, // Only iddq
	};
	int addr;

	if (unit >= MAX_PCIE_UNIT)
		BUG_ON(1);

	addr = pmc_pcie_pmb_addr[unit];

#if	 defined(CONFIG_BCM96858) || defined(CONFIG_BCM96836) || defined(CONFIG_BCM96856)
	/*On Bifurcation mode we must not shut down and de-register the ubus port of
	 * bifurcated port (0) since we don't know the future status of core 1 */
	if (pmc_pcie_ubus_node[unit].port_type == PCIE_BIFURCATED_PORT)
		return;
#endif
#if defined(CONFIG_BCM963158)
	// To power down PLL and PLL-LDO for PCIe gen 3
	if (addr == PMB_ADDR_PCIE3) {
		sr_ctrl.Reg32 |= 1 << 29 | 1 << 30;
	}

	{
		int dual_lane, bifur_addr;
		/* Power down the other bi-furcated port if this supports dual lane */
		if ((BpGetPciPortDualLane(unit, &dual_lane) == BP_SUCCESS) && dual_lane) {
			/* Identify other bifurcated port address */
			bifur_addr = pmc_pcie_pmb_addr[unit+1];
			if (WriteBPCMRegister(bifur_addr, BPCMRegOffset(sr_control), sr_ctrl.Reg32))
				BUG_ON(1);
			if (PowerOffZone(bifur_addr, 0))
				BUG_ON(1);
		}
	}
#endif
	if (WriteBPCMRegister(addr, BPCMRegOffset(sr_control), sr_ctrl.Reg32))
		BUG_ON(1);

	mdelay(10);

	pmc_pcie_deregister_ubus(unit);

	if (PowerOffZone(addr, 0))
		BUG_ON(1);
}

#ifndef _CFE_
EXPORT_SYMBOL(pmc_pcie_power_up);
EXPORT_SYMBOL(pmc_pcie_power_down);
#endif
