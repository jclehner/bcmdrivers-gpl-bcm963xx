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
 * pmc_wan.h
 *
 *  Created on: Nov 30 2015
 *      Author: yonatani
 */


#ifndef _PMC_WAN_H_
#define _PMC_WAN_H_

#include <bcmtypes.h>


typedef union
{
    struct
    {
#ifdef PMC_LITTLE_ENDIAN
    uint32 wan_main_rst_n:1;
    uint32 wan_top_bb_rst_n:1;
    uint32 epon_core_rst_n:1;
    uint32 epon_rx_rclk16_sw_reset_n:1;
    uint32 epon_rx_rbc125_sw_reset_n:1;
    uint32 epon_tx_tclk16_sw_reset_n:1;
    uint32 epon_tx_clk125_sw_reset_n:1;
    uint32 gpon_main_rst_n:1;
    uint32 gpon_rx_rst_n:1;
    uint32 gpon_tx_rst_n:1;
    uint32 gpon_8khz_rst_n:1;
    uint32 ngpon_main_rst_n:1;
    uint32 ngpon_rx_rst_n:1;
    uint32 ngpon_tx_rst_n:1;
    uint32 ngpon_8khz_rst_n:1;
    uint32 gpon_nco_rst_n:1;
    uint32 apm_rst_n:1;
    uint32 reserved:15;
#else
    uint32 reserved:15;
    uint32 apm_rst_n:1;
    uint32 gpon_nco_rst_n:1;
    uint32 ngpon_8khz_rst_n:1;
    uint32 ngpon_tx_rst_n:1;
    uint32 ngpon_rx_rst_n:1;
    uint32 ngpon_main_rst_n:1;
    uint32 gpon_8khz_rst_n:1;
    uint32 gpon_tx_rst_n:1;
    uint32 gpon_rx_rst_n:1;
    uint32 gpon_main_rst_n:1;
    uint32 epon_tx_clk125_sw_reset_n:1;
    uint32 epon_tx_tclk16_sw_reset_n:1;
    uint32 epon_rx_rbc125_sw_reset_n:1;
    uint32 epon_rx_rclk16_sw_reset_n:1;
    uint32 epon_core_rst_n:1;
    uint32 wan_top_bb_rst_n:1;
    uint32 wan_main_rst_n:1;
#endif
    }Bits;
    uint32 Reg32;
}WAN_TOP_BPCM_SRESET_REG;

int pmc_wan_init(void);
int pmc_wan_power_down(void);
#endif /* _PMC_WAN_H_ */
