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
/*
 * pmc_wan.c
 *
 *  Created on: Nov 30 2015
 *      Author: yonatani
 */

#ifndef _CFE_
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#else
#include "bcmtypes.h"
#endif
#include "pmc_drv.h"
#include "pmc_wan.h"
#include "BPCM.h"
#include "bcm_ubus4.h"

int pmc_wan_init(void)
{
    int status;
    WAN_TOP_BPCM_SRESET_REG sreset;

    status =  PowerOnDevice(PMB_ADDR_WAN);

#if defined (CONFIG_BCM96858) || defined(_BCM96858_)
    ubus_register_port(UCB_NODE_ID_SLV_WAN);
#endif

    //take pins out of reset
    // can be modified later for smaller granularity

    sreset.Bits.wan_main_rst_n = 0;
    sreset.Bits.wan_top_bb_rst_n = 0;
    sreset.Bits.epon_core_rst_n = 0;
    sreset.Bits.epon_rx_rclk16_sw_reset_n = 0;
    sreset.Bits.epon_rx_rbc125_sw_reset_n = 0;
    sreset.Bits.epon_tx_tclk16_sw_reset_n = 0;
    sreset.Bits.epon_tx_clk125_sw_reset_n = 0;
    sreset.Bits.gpon_main_rst_n = 0;
    sreset.Bits.gpon_rx_rst_n = 0;
    sreset.Bits.gpon_tx_rst_n = 0;
    sreset.Bits.gpon_8khz_rst_n = 0;
    sreset.Bits.ngpon_main_rst_n = 0;
    sreset.Bits.ngpon_rx_rst_n = 0;
    sreset.Bits.ngpon_tx_rst_n = 0;
    sreset.Bits.ngpon_8khz_rst_n = 0;
    sreset.Bits.gpon_nco_rst_n = 0;
    sreset.Bits.apm_rst_n = 0;

    status = WriteBPCMRegister(PMB_ADDR_WAN, BPCMRegOffset(sr_control), sreset.Reg32);

    return status;
}

int pmc_wan_power_down(void)
{
#if defined (CONFIG_BCM96858) || defined(_BCM96858_)
    ubus_deregister_port(UCB_NODE_ID_SLV_WAN);
#endif

    return  PowerOffDevice(PMB_ADDR_WAN, 0);
}
#ifndef _CFE_
EXPORT_SYMBOL(pmc_wan_init);
EXPORT_SYMBOL(pmc_wan_power_down);
postcore_initcall(pmc_wan_init);
#endif

