/*
 * <:copyright-BRCM:2013:DUAL/GPL:standard
 * 
 *    Copyright (c) 2013 Broadcom 
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

#include "boardparms.h"

#ifdef _CFE_
#include "lib_types.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "cfe_timer.h"
#include "bcm_map.h"
#define printk  printf
#define udelay cfe_usleep
#else // Linux
#include <linux/kernel.h>
#include <linux/module.h>
#include <bcm_map_part.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/sched.h>
#endif

#define _EXT_SWITCH_INIT_
#include "mii_shared.h"
#include "pmc_drv.h"
#include "pmc_switch.h"
#include "bcm_ethsw.h"
#include "shared_utils.h"

#include "bcm_led.h"

#define K1CTL_REPEATER_DTE 0x400
#if !defined(K1CTL_1000BT_FDX)
#define K1CTL_1000BT_FDX 	0x200
#endif
#if !defined(K1CTL_1000BT_HDX)
#define K1CTL_1000BT_HDX 	0x100
#endif

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
/*
  This file implement the switch and phy related init and low level function that can be shared between
  cfe and linux.These are functions called from CFE or from the Linux ethernet driver.

  The Linux ethernet driver handles any necessary locking so these functions should not be called
  directly from elsewhere.
*/

/* SF2 switch phy register access function */
uint16_t bcm_ethsw_phy_read_reg(int phy_id, int reg)
{
    int reg_in, reg_out = 0;
    int i = 0;

    phy_id &= BCM_PHY_ID_M;

    reg_in = ((phy_id << ETHSW_MDIO_C22_PHY_ADDR_SHIFT)&ETHSW_MDIO_C22_PHY_ADDR_MASK) |
                ((reg << ETHSW_MDIO_C22_PHY_REG_SHIFT)&ETHSW_MDIO_C22_PHY_REG_MASK) |
                (ETHSW_MDIO_CMD_C22_READ << ETHSW_MDIO_CMD_SHIFT);
    ETHSW_MDIO->mdio_cmd = reg_in | ETHSW_MDIO_BUSY;
    do {
         if (++i >= 10)  {
             printk("%s MDIO No Response! phy 0x%x reg 0x%x \n", __FUNCTION__, phy_id, reg);
             return 0;
         }
         udelay(60);
         reg_out = ETHSW_MDIO->mdio_cmd;
    } while (reg_out & ETHSW_MDIO_BUSY);

    /* Read a second time to ensure it is reliable */
    ETHSW_MDIO->mdio_cmd = reg_in | ETHSW_MDIO_BUSY;
    i = 0;
    do {
         if (++i >= 10)  {
             printk("%s MDIO No Response! phy 0x%x reg 0x%x \n", __FUNCTION__, phy_id, reg);
             return 0;
         }
         udelay(60);
         reg_out = ETHSW_MDIO->mdio_cmd;
    } while (reg_out & ETHSW_MDIO_BUSY);

    return (uint16_t)reg_out;
}

void bcm_ethsw_phy_write_reg(int phy_id, int reg, uint16 data)
{
    int reg_value = 0;
    int i = 0;

    phy_id &= BCM_PHY_ID_M;
 
    reg_value = ((phy_id << ETHSW_MDIO_C22_PHY_ADDR_SHIFT)&ETHSW_MDIO_C22_PHY_ADDR_MASK) |
                ((reg << ETHSW_MDIO_C22_PHY_REG_SHIFT)& ETHSW_MDIO_C22_PHY_REG_MASK) |
                (ETHSW_MDIO_CMD_C22_WRITE << ETHSW_MDIO_CMD_SHIFT) | (data&ETHSW_MDIO_PHY_DATA_MASK);
    ETHSW_MDIO->mdio_cmd = reg_value | ETHSW_MDIO_BUSY;

    do {
         if (++i >= 10)  {
             printk("%s MDIO No Response! phy 0x%x reg 0x%x\n", __FUNCTION__, phy_id, reg);
             return;
         }
         udelay(60);
         reg_value = ETHSW_MDIO->mdio_cmd;
    } while (reg_value & ETHSW_MDIO_BUSY);

    return;
}

#if defined(_BCM963138_) || defined(CONFIG_BCM963138) || defined(_BCM963148_) || defined(CONFIG_BCM963148) || defined(_BCM94908_) || defined(CONFIG_BCM94908) || defined(_BCM963158_) || defined(CONFIG_BCM963158)
/* EGPHY28 phy misc and expansion register indirect access function. Hard coded MII register
number, define them in bcmmii.h. Should consider move the bcmmii.h to shared folder as well */
uint16 bcm_ethsw_phy_read_exp_reg(int phy_id, int reg)
{
    bcm_ethsw_phy_write_reg(phy_id, 0x17, reg|0xf00);
    return bcm_ethsw_phy_read_reg(phy_id, 0x15);
}


void bcm_ethsw_phy_write_exp_reg(int phy_id, int reg, uint16 data)
{
    bcm_ethsw_phy_write_reg(phy_id, 0x17, reg|0xf00);
    bcm_ethsw_phy_write_reg(phy_id, 0x15, data);

    return;
}

uint16 bcm_ethsw_phy_read_misc_reg(int phy_id, int reg, int chn)
{
    uint16 temp;
    bcm_ethsw_phy_write_reg(phy_id, 0x18, 0x7);
    temp = bcm_ethsw_phy_read_reg(phy_id, 0x18);
    temp |= 0x800;
    bcm_ethsw_phy_write_reg(phy_id, 0x18, temp);

    temp = (chn << 13)|reg;
    bcm_ethsw_phy_write_reg(phy_id, 0x17, temp);
    return  bcm_ethsw_phy_read_reg(phy_id, 0x15);
}

void bcm_ethsw_phy_write_misc_reg(int phy_id, int reg, int chn, uint16 data)
{
    uint16 temp;
    bcm_ethsw_phy_write_reg(phy_id, 0x18, 0x7);
    temp = bcm_ethsw_phy_read_reg(phy_id, 0x18);
    temp |= 0x800;
    bcm_ethsw_phy_write_reg(phy_id, 0x18, temp);

    temp = (chn << 13)|reg;
    bcm_ethsw_phy_write_reg(phy_id, 0x17, temp);
    bcm_ethsw_phy_write_reg(phy_id, 0x15, data);
    
    return;
}
#endif

/* FIXME - same code exists in robosw_reg.c */
static void phy_advertise_caps(unsigned int phy_id)
{
    uint16 cap_mask = 0;

    /* control advertising if boardparms says so */
    if(IsPhyConnected(phy_id) && IsPhyAdvCapConfigValid(phy_id))
    {
        cap_mask = bcm_ethsw_phy_read_reg(phy_id, MII_ANAR);
        cap_mask &= ~(ANAR_TXFD | ANAR_TXHD | ANAR_10FD | ANAR_10HD);
        if (phy_id & ADVERTISE_10HD)
            cap_mask |= ANAR_10HD;
        if (phy_id & ADVERTISE_10FD)
            cap_mask |= ANAR_10FD;
        if (phy_id & ADVERTISE_100HD)
            cap_mask |= ANAR_TXHD;
        if (phy_id & ADVERTISE_100FD)
            cap_mask |= ANAR_TXFD;
        bcm_ethsw_phy_write_reg(phy_id, MII_ANAR, cap_mask);

        cap_mask = bcm_ethsw_phy_read_reg(phy_id, MII_K1CTL);
        cap_mask &= (~(K1CTL_1000BT_FDX | K1CTL_1000BT_HDX));
        if (phy_id & ADVERTISE_1000HD)
            cap_mask |= K1CTL_1000BT_HDX;
        if (phy_id & ADVERTISE_1000FD)
            cap_mask |= K1CTL_1000BT_FDX;
        bcm_ethsw_phy_write_reg(phy_id, MII_K1CTL, cap_mask);
    }

    /* Always enable repeater mode */
    cap_mask = bcm_ethsw_phy_read_reg(phy_id, MII_K1CTL);
    cap_mask |= K1CTL_REPEATER_DTE;
    bcm_ethsw_phy_write_reg(phy_id, MII_K1CTL, cap_mask);
}

#if defined(_BCM963138_) || defined(CONFIG_BCM963138)
static void phy_adjust_afe(unsigned int phy_id_base, int is_quad)
{
    unsigned int phy_id;
    unsigned int phy_id_end = is_quad ? (phy_id_base + 4) : (phy_id_base + 1);

    for( phy_id = phy_id_base; phy_id < phy_id_end; phy_id++ )
    {
        //reset phy
        bcm_ethsw_phy_write_reg(phy_id, 0x0, 0x9140);
        udelay(100);

        //AFE_RXCONFIG_1 Provide more margin for INL/DNL measurement on ATE  
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x38, 0x1, 0x9b2f);
        //AFE_TX_CONFIG Set 100BT Cfeed=011 to improve rise/fall time
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x39, 0x0, 0x0431);
        //AFE_VDAC_ICTRL_0 Set Iq=1101 instead of 0111 for improving AB symmetry 
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x39, 0x1, 0xa7da);
        //AFE_HPF_TRIM_OTHERS Set 100Tx/10BT to -4.5% swing & Set rCal offset for HT=0 code
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x3a, 0x0, 0x00e3);
    }

    //CORE_BASE1E Force trim overwrite and set I_ext trim to 0000
    bcm_ethsw_phy_write_reg(phy_id_base, 0x1e, 0x10);
    for( phy_id = phy_id_base; phy_id < phy_id_end; phy_id++ )
    {
        //Adjust bias current trim by +4% swing, +2 tick 'DSP_TAP10
        bcm_ethsw_phy_write_misc_reg(phy_id, 0xa, 0x0, 0x011b);
    }

    //Reset R_CAL/RC_CAL Engine 'CORE_EXPB0
    bcm_ethsw_phy_write_exp_reg(phy_id_base, 0xb0, 0x10);
    //Disable Reset R_CAL/RC_CAL Engine 'CORE_EXPB0
    bcm_ethsw_phy_write_exp_reg(phy_id_base, 0xb0, 0x0);

    return;
}

#endif

#if defined(_BCM963148_) || defined(CONFIG_BCM963148)
static void phy_adjust_afe(unsigned int phy_id_base, int is_quad)
{
    unsigned int phy_id;
    unsigned int phy_id_end = is_quad ? (phy_id_base + 4) : (phy_id_base + 1);

    for( phy_id = phy_id_base; phy_id < phy_id_end; phy_id++ )
    {
        //reset phy
        bcm_ethsw_phy_write_reg(phy_id, 0x0, 0x9140);
        udelay(100);
        //Write analog control registers
        //AFE_RXCONFIG_0
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x38, 0x0, 0xeb15);
        //AFE_RXCONFIG_1. Replacing the previously suggested 0x9AAF for SS part. See JIRA HW63148-31
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x38, 0x1, 0x9b2f);
        //AFE_RXCONFIG_2
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x38, 0x2, 0x2003);
        //AFE_RX_LP_COUNTER
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x38, 0x3, 0x7fc0);
        //AFE_TX_CONFIG
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x39, 0x0, 0x0060);
        //AFE_VDAC_ICTRL_0
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x39, 0x1, 0xa7da);
        //AFE_VDAC_OTHERS_0
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x39, 0x3, 0xa020);
        //AFE_HPF_TRIM_OTHERS
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x3a, 0x0, 0x00e3);
    }

    //CORE_BASE1E Force trim overwrite and set I_ext trim to 0000
    bcm_ethsw_phy_write_reg(phy_id_base, 0x1e, 0x0010);
    for( phy_id = phy_id_base; phy_id < phy_id_end; phy_id++ )
    {
       //Adjust bias current trim by +4% swing, +2 tick, increase PLL BW in GPHY link start up training 'DSP_TAP10
       bcm_ethsw_phy_write_misc_reg(phy_id, 0xa, 0x0, 0x111b);
    }

    //Reset R_CAL/RC_CAL Engine 'CORE_EXPB0
    bcm_ethsw_phy_write_exp_reg(phy_id_base, 0xb0, 0x10);
    //Disable Reset R_CAL/RC_CAL Engine 'CORE_EXPB0
    bcm_ethsw_phy_write_exp_reg(phy_id_base, 0xb0, 0x0);
}

#endif

#if defined(_BCM94908_) || defined(CONFIG_BCM94908)
static void phy_adjust_afe(unsigned int phy_id_base, int is_quad)
{
    unsigned int phy_id;
    unsigned int phy_id_end = is_quad ? (phy_id_base + 4) : (phy_id_base + 1);

    for( phy_id = phy_id_base; phy_id < phy_id_end; phy_id++ )
    {
        //reset phy
        bcm_ethsw_phy_write_reg(phy_id, 0x0, 0x9140);
    }
    udelay(100);

    //CORE_BASE1E Force trim overwrite and set I_ext trim to 0000
    bcm_ethsw_phy_write_reg(phy_id_base, 0x1e, 0x0010);
    for( phy_id = phy_id_base; phy_id < phy_id_end; phy_id++ )
    {
        //AFE_RXCONFIG_1 Provide more margin for INL/DNL measurement on ATE  
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x38, 0x1, 0x9b2f);
        //AFE_TX_CONFIG Set 1000BT Cfeed=011 to improve rise/fall time
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x39, 0x0, 0x0431);
        //AFE_VDAC_ICTRL_0 Set Iq=1101 instead of 0111 for improving AB symmetry 
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x39, 0x1, 0xa7da);
        //AFE_HPF_TRIM_OTHERS Set 100Tx/10BT to -4.5% swing & Set rCal offset for HT=0 code
        bcm_ethsw_phy_write_misc_reg(phy_id, 0x3a, 0x0, 0x00e3);
        //DSP_TAP10  Adjust I_int bias current trim by +0% swing, +0 tick
        bcm_ethsw_phy_write_misc_reg(phy_id, 0xa, 0x0, 0x011b);
    }

    //Reset R_CAL/RC_CAL Engine 'CORE_EXPB0
    bcm_ethsw_phy_write_exp_reg(phy_id_base, 0xb0, 0x10);
    //Disable Reset R_CAL/RC_CAL Engine 'CORE_EXPB0
    bcm_ethsw_phy_write_exp_reg(phy_id_base, 0xb0, 0x0);

    return;
}
#endif

#if defined(_BCM963158_) || defined(CONFIG_BCM963158)
static void phy_adjust_afe(unsigned int phy_id_base, int is_quad)
{
    unsigned int phy_id;
    unsigned int phy_id_end = is_quad ? (phy_id_base + 4) : (phy_id_base + 1);

    for( phy_id = phy_id_base; phy_id < phy_id_end; phy_id++ )
    {
        //reset phy
        bcm_ethsw_phy_write_reg(phy_id, 0x0, 0x9140);
    }
    udelay(100);


    for( phy_id = phy_id_base; phy_id < phy_id_end; phy_id++ )
    {
        //Turn off AOF
        bcm_ethsw_phy_write_misc_reg(  phy_id,  0x39, 0x1, 0x0000 );//AFE_TX_CONFIG_0

        //1g AB symmetry Iq
        bcm_ethsw_phy_write_misc_reg(  phy_id,  0x3a, 0x2, 0x0BCC );//AFE_TX_CONFIG_1

        //LPF BW
        bcm_ethsw_phy_write_misc_reg(  phy_id,  0x39, 0x0, 0x233F );//AFE_TX_IQ_RX_LP

        //RCAL +6LSB to make impedance from 112 to 100ohm
        bcm_ethsw_phy_write_misc_reg(  phy_id,  0x3b, 0x0, 0xAD40 );//AFE_TEMPSEN_OTHERS

        //since rcal make R smaller, make master current -4%
        bcm_ethsw_phy_write_misc_reg(  phy_id,  0x0a, 0x0, 0x091B );//DSP_TAP10

        //From EEE excel config file for Vitesse fix
        bcm_ethsw_phy_write_misc_reg(  phy_id,  0x0021, 0x0002, 0x87F6 );// rx_on_tune 8 -> 0xf
        bcm_ethsw_phy_write_misc_reg(  phy_id,  0x0022, 0x0002, 0x017D );// 100tx EEE bandwidth
        bcm_ethsw_phy_write_misc_reg(  phy_id,  0x0026, 0x0002, 0x0015 );// enable ffe zero det for Vitesse interop

    }

    //Reset R_CAL/RC_CAL Engine 'CORE_EXPB0
    bcm_ethsw_phy_write_exp_reg(phy_id_base, 0xb0, 0x10);
    //Disable Reset R_CAL/RC_CAL Engine 'CORE_EXPB0
    bcm_ethsw_phy_write_exp_reg(phy_id_base, 0xb0, 0x0);

    return;

}
#endif

static void phy_fixup(void)
{
    int phy_id;

    /* Internal QUAD PHY and Single PHY require some addition fixup on the PHY AFE */
    phy_id = (ETHSW_REG->qphy_ctrl&ETHSW_QPHY_CTRL_PHYAD_BASE_MASK)>>ETHSW_QPHY_CTRL_PHYAD_BASE_SHIFT;
    phy_adjust_afe(phy_id, 1);

    phy_id = (ETHSW_REG->sphy_ctrl&ETHSW_SPHY_CTRL_PHYAD_MASK)>>ETHSW_SPHY_CTRL_PHYAD_SHIFT;
    phy_adjust_afe(phy_id, 0);
}

static void extsw_register_save_restore(int save)
{
    static int saved = 0; 
    static uint32_t portCtrl[BP_MAX_SWITCH_PORTS], pbvlan[BP_MAX_SWITCH_PORTS], reg;
    int i;
    int offset_jump=ARRAY_SIZE(ETHSW_CORE->port_vlan_ctrl)/9;

    if (save) {
        saved = 1;
        for( i = 0; i < BP_MAX_SWITCH_PORTS; i++ )
        {
            portCtrl[i] = ETHSW_CORE->port_traffic_ctrl[i] & 0xff;
            pbvlan[i] = ETHSW_CORE->port_vlan_ctrl[offset_jump*i] & 0xffff;
        }
    }
    else
    {
        if (saved)
        {
            for( i = 0; i < BP_MAX_SWITCH_PORTS; i++ )
            {
                reg = ETHSW_CORE->port_traffic_ctrl[i] & PORT_CTRL_SWITCH_RESERVE;
                reg |= portCtrl[i] & ~PORT_CTRL_SWITCH_RESERVE;
                ETHSW_CORE->port_traffic_ctrl[i] = reg;
                ETHSW_CORE->port_vlan_ctrl[offset_jump*i] = pbvlan[i];
            }
        }
    }
}

#if defined(CONFIG_BCM963158) || defined(_BCM963158_)
void all_gphy_init_power_workaround(void)
{

    unsigned int phy_ctrl;

    ETHSW_REG->phy_test_ctrl=1;

    phy_ctrl = ETHSW_REG->qphy_ctrl;
    phy_ctrl &= ~(ETHSW_QPHY_CTRL_IDDQ_BIAS_MASK|ETHSW_QPHY_CTRL_IDDQ_GLOBAL_PWR_MASK);
    ETHSW_REG->qphy_ctrl = phy_ctrl;

    phy_ctrl = ETHSW_REG->sphy_ctrl;
    phy_ctrl &= ~(ETHSW_SPHY_CTRL_IDDQ_BIAS_MASK|ETHSW_SPHY_CTRL_IDDQ_GLOBAL_PWR_MASK);
    ETHSW_REG->sphy_ctrl = phy_ctrl;
    udelay(1000);
    
    ETHSW_REG->qphy_ctrl |= ETHSW_QPHY_CTRL_RESET_MASK;
    ETHSW_REG->sphy_ctrl |= ETHSW_SPHY_CTRL_RESET_MASK;
    udelay(1000);

    phy_ctrl = ETHSW_REG->qphy_ctrl;
    phy_ctrl &= ~(ETHSW_QPHY_CTRL_RESET_MASK);
    ETHSW_REG->qphy_ctrl = phy_ctrl;

    phy_ctrl = ETHSW_REG->sphy_ctrl;
    phy_ctrl &= ~(ETHSW_SPHY_CTRL_RESET_MASK);
    ETHSW_REG->sphy_ctrl = phy_ctrl;
    udelay(1000);


    ETHSW_REG->qphy_ctrl |= (ETHSW_QPHY_CTRL_IDDQ_BIAS_MASK|ETHSW_QPHY_CTRL_IDDQ_GLOBAL_PWR_MASK);
    ETHSW_REG->sphy_ctrl |= (ETHSW_SPHY_CTRL_IDDQ_BIAS_MASK|ETHSW_SPHY_CTRL_IDDQ_GLOBAL_PWR_MASK);
    udelay(1000);

    ETHSW_REG->phy_test_ctrl=0;
}
#endif


/* SF2 switch low level init */
void bcm_ethsw_init(void)
{
    const ETHERNET_MAC_INFO   *pMacInfo = BpGetEthernetMacInfoArrayPtr();
    unsigned int phy_ctrl;
    unsigned short phy_base;
    int i, sw;

    printk("Initalizing switch low level hardware.\n");
    /* power up the switch block */
    pmc_switch_power_up();

    /* power up unimac for CFE */
#if defined(_CFE_) && defined(_BCM94908_)
    PowerOnDevice(PMB_ADDR_GMAC);
#endif

    /* Reset switch */
    printk("Software Resetting Switch ... ");
    ETHSW_CORE->software_reset |= SOFTWARE_RESET|EN_SW_RST;
    for (;ETHSW_CORE->software_reset & SOFTWARE_RESET;) udelay(100);
    printk("Done.\n");
    udelay(1000);

    if( BpGetGphyBaseAddress(&phy_base) != BP_SUCCESS )
        phy_base = 1;

#if defined(CONFIG_BCM963158) || defined(_BCM963158_)
    all_gphy_init_power_workaround();
#endif
    /* power on and reset the quad and single phy */
    phy_ctrl = ETHSW_REG->qphy_ctrl;
    phy_ctrl &= ~(ETHSW_QPHY_CTRL_IDDQ_BIAS_MASK|ETHSW_QPHY_CTRL_EXT_PWR_DOWN_MASK|ETHSW_QPHY_CTRL_PHYAD_BASE_MASK);
    phy_ctrl |= ETHSW_QPHY_CTRL_RESET_MASK|(phy_base<<ETHSW_QPHY_CTRL_PHYAD_BASE_SHIFT);

#if defined(_BCM94908_) || defined(CONFIG_BCM94908) || defined(_BCM963158_) || defined(CONFIG_BCM963158)
    phy_ctrl &= ~(ETHSW_QPHY_CTRL_IDDQ_GLOBAL_PWR_MASK);
#endif

    ETHSW_REG->qphy_ctrl = phy_ctrl;

    phy_ctrl = ETHSW_REG->sphy_ctrl;
    phy_ctrl &= ~(ETHSW_SPHY_CTRL_IDDQ_BIAS_MASK| ETHSW_SPHY_CTRL_EXT_PWR_DOWN_MASK|ETHSW_SPHY_CTRL_PHYAD_MASK);
    phy_ctrl |= ETHSW_SPHY_CTRL_RESET_MASK|((phy_base+4)<<ETHSW_SPHY_CTRL_PHYAD_SHIFT);

#if defined(_BCM94908_) || defined(CONFIG_BCM94908) || defined(_BCM963158_) || defined(CONFIG_BCM963158)
    phy_ctrl &= ~(ETHSW_SPHY_CTRL_IDDQ_GLOBAL_PWR_MASK);
#endif

    ETHSW_REG->sphy_ctrl = phy_ctrl;

    udelay(1000);

    ETHSW_REG->qphy_ctrl &= ~ETHSW_QPHY_CTRL_RESET_MASK;
    ETHSW_REG->sphy_ctrl &= ~ETHSW_SPHY_CTRL_RESET_MASK;

    udelay(1000);


    /* add dummy read to workaround first MDIO read/write issue after power on */
    bcm_ethsw_phy_read_reg(phy_base, 0x2);

    phy_fixup();

#if defined(_BCM963158_) || defined(CONFIG_BCM963158)
    gen2_leds_init();
#else
    bcm_ethsw_set_led();
#endif

    printk("Waiting MAC port Rx/Tx to be enabled by hardware ...");
    for( i = 0; i < BP_MAX_SWITCH_PORTS; i++ )
    {
        /* Wait until hardware enable the ports, or we will kill the hardware */
        for(;ETHSW_CORE->port_traffic_ctrl[i] & PORT_CTRL_RX_DISABLE; udelay(100));
    }

    for (sw = 0 ; sw < BP_MAX_ENET_MACS ; sw++) 
    {
        for(i = 0; i < BP_MAX_SWITCH_PORTS; i++ )
        {
            phy_advertise_caps(pMacInfo[sw].sw.phy_id[i]);
        }
        for (i = 0; i < BP_MAX_CROSSBAR_EXT_PORTS ; i++ )
        {
            if (pMacInfo[sw].sw.crossbar[i].switch_port != BP_CROSSBAR_NOT_DEFINED)
            {
                phy_advertise_caps(pMacInfo[sw].sw.crossbar[i].phy_id);
            }
        }
    }
    printk("Done\n");

    /* disabled all MAC TX/RX. */
    printk("Disable Switch All MAC port Rx/Tx\n");
    for( i = 0; i < BP_MAX_SWITCH_PORTS; i++ )
    {
        ETHSW_CORE->port_traffic_ctrl[i] = (ETHSW_CORE->port_traffic_ctrl[i] & 0xff) | PORT_CTRL_RXTX_DISABLE;
    }

    /* Set switch to unmanaged mode and enable forwarding */
    ETHSW_CORE->switch_mode = ((ETHSW_CORE->switch_mode & 0xff) | ETHSW_SM_FORWARDING_EN |
            ETHSW_SM_RETRY_LIMIT_DIS) & (~ETHSW_SM_MANAGED_MODE);
    ETHSW_CORE->brcm_hdr_ctrl = 0;
    ETHSW_CORE->switch_ctrl = (ETHSW_CORE->switch_ctrl & 0xffb0) | 
        ETHSW_SC_MII_DUMP_FORWARDING_EN | ETHSW_SC_MII2_VOL_SEL;

    ETHSW_CORE->imp_port_state = ETHSW_IPS_USE_REG_CONTENTS | ETHSW_IPS_TXFLOW_PAUSE_CAPABLE | 
        ETHSW_IPS_RXFLOW_PAUSE_CAPABLE | ETHSW_IPS_SW_PORT_SPEED_1000M_2000M | 
        ETHSW_IPS_DUPLEX_MODE | ETHSW_IPS_LINK_PASS;


    return;
}

#if defined(_BCM94908_)

/* In 4908 GMAC/UNIMAC is attached to the SF2 using MAC to MAC connection as IMP port. There is no
   PHY block. Sneak in some simple GMAC init routine in the driver file. 
   TODO: Move to seperate file for sharing with linux driver */
static void gmac_init(void);
static void gmac_enable_port(int enable);

static void gmac_init(void)
{
    /* Reset GMAC */
    GMAC_MAC->Cmd.sw_reset = 1;
    cfe_usleep(20);
    GMAC_MAC->Cmd.sw_reset = 0;
   
    GMAC_INTF->Flush.txfifo_flush = 1;
    GMAC_INTF->Flush.rxfifo_flush = 1;

    GMAC_INTF->Flush.txfifo_flush = 0;
    GMAC_INTF->Flush.rxfifo_flush = 0;

    GMAC_INTF->MibCtrl.clrMib = 1;
    GMAC_INTF->MibCtrl.clrMib = 0;

    /* default CMD configuration */
    GMAC_MAC->Cmd.eth_speed = CMD_ETH_SPEED_1000;

    /* Disable Tx and Rx */
    GMAC_MAC->Cmd.tx_ena = 0;
    GMAC_MAC->Cmd.rx_ena = 0;

    GMAC_INTF->GmacStatus.auto_cfg_en = 1;
    GMAC_INTF->GmacStatus.hd = 0;
    GMAC_INTF->GmacStatus.eth_speed = 2;
    GMAC_INTF->GmacStatus.link_up = 1;

    return;
}

static void gmac_enable_port(int enable)
{
    if( enable )
    {
        GMAC_MAC->Cmd.tx_ena = 1;
        GMAC_MAC->Cmd.rx_ena = 1;
    }
    else
    {
        GMAC_MAC->Cmd.tx_ena = 0;
        GMAC_MAC->Cmd.rx_ena = 0;
    }

    return;
}
#endif

/* SF2 switch init for CFE networking */
/* Only Called by CFE Command Line */
void bcm_ethsw_open(void)
{
    int i;
    int offset_jump=ARRAY_SIZE(ETHSW_CORE->port_vlan_ctrl)/9;

    /* Save MAC port and PBVLAN registers to save boot strap status */
    extsw_register_save_restore(1);

    /* Enable MAC port Tx/Rx for CFE local traffic */
    printk ("Enable Switch MAC Port Rx/Tx, set PBVLAN to FAN out, set switch to NO-STP.\n");
    for( i = 0; i < BP_MAX_SWITCH_PORTS; i++ )
    {
        /* Set Port VLAN to allow CPU traffic only */
        ETHSW_CORE->port_vlan_ctrl[offset_jump*i] = PBMAP_MIPS;

        /* Setting switch to NO-STP mode; enable port TX/RX. */
        ETHSW_CORE->port_traffic_ctrl[i] = ((ETHSW_CORE->port_traffic_ctrl[i] & 0xff) & 
                (~(PORT_CTRL_RXTX_DISABLE|PORT_CTRL_PORT_STATUS_M))) | PORT_CTRL_NO_STP;
    }

#if defined(_BCM94908_)
    gmac_init();
    gmac_enable_port(1);
#endif
    return;
}

/* SF2 switch post process to restore switch status too boot strap status */
void bcm_ethsw_close(void)
{
    printk("Restore Switch's MAC port Rx/Tx, PBVLAN back.\n");
#if defined(_BCM94908_)
    gmac_enable_port(0);
#endif
    extsw_register_save_restore(0);
}



