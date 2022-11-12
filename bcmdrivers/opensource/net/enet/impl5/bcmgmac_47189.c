/*
<:copyright-BRCM:2012:DUAL/GPL:standard

   Copyright (c) 2012 Broadcom 
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


//**************************************************************************
// File Name  : bcmgmac.c
//
// Description: This is Linux network driver for Broadcom GMAC controller
//
//**************************************************************************

#define VERSION     "0.1"
#define VER_STR     "v" VERSION

#define _BCMENET_LOCAL_

#include <linux/types.h>
#include <linux/bcm_log_mod.h>
#include <bcm_map_part.h>
#include <bcm_intr.h>
#include <board.h>
#include "bcm_misc_hw_init.h"

#include "bcmgmacctl.h"
#include "bcmmii.h"
#include "bcm_gpio.h"
#include "bcmgmac_47189.h"
#include <bcm_pkt_lengths.h>


#define GMAC_RESET_DELAY        2

/* PMU clock/power control */
#define PMUCTL_ENAB                     (MISC->capabilities & CC_CAP_PMU)

/* 53537 series moved switch_type and gmac_if_type to CC4 [15:14] and [13:12] */
#define PMU_CC4_IF_TYPE_MASK            0x00003000
#define PMU_CC4_IF_TYPE_RMII            0x00000000
#define PMU_CC4_IF_TYPE_MII             0x00001000
#define PMU_CC4_IF_TYPE_RGMII           0x00002000

#define PMU_CC4_SW_TYPE_MASK            0x0000c000
#define PMU_CC4_SW_TYPE_EPHY            0x00000000
#define PMU_CC4_SW_TYPE_EPHYMII         0x00004000
#define PMU_CC4_SW_TYPE_EPHYRMII        0x00008000

/* PMU chip control4 register */
#define PMU_CHIPCTL4                    4
#define PMU_CC4_SW_TYPE_RGMII           0x0000c000

/*
 * Spin at most 'us' microseconds while 'exp' is true.
 * Caller should explicitly test 'exp' when this completes
 * and take appropriate error action if 'exp' is still true.
 */
#define SPINWAIT_POLL_PERIOD	10

#define SPINWAIT(exp, us) { \
        uint32 countdown = (us) + (SPINWAIT_POLL_PERIOD - 1); \
        while ((exp) && (countdown >= SPINWAIT_POLL_PERIOD)) { \
                udelay(SPINWAIT_POLL_PERIOD); \
                countdown -= SPINWAIT_POLL_PERIOD; \
        } \
}


/* Helper functions */
static inline volatile EnetCoreMisc* gmac_misc_regs(int ethcore)
{
    if (ethcore == 0) {
        return ENET_CORE0_MISC;
    } else if (ethcore == 1) {
        return ENET_CORE1_MISC;
    } else {
        printk("Fatal error: Ethernet core %d doesn't exist\n", ethcore);
        /* Loop here forever */
        while (1) ;
    }

    /* Never reached */
    return 0;
}

inline volatile EnetCoreMib* gmac_mib_regs(int ethcore)
{
    if (ethcore == 0) {
        return ENET_CORE0_MIB;
    } else if (ethcore == 1) {
        return ENET_CORE1_MIB;
    } else {
        printk("Fatal error: Ethernet core %d doesn't exist\n", ethcore);
        /* Loop here forever */
        while (1) ;
    }

    /* Never reached */
    return 0;
}

static inline volatile EnetCoreUnimac* gmac_unimac_regs(int ethcore)
{
    if (ethcore == 0) {
        return ENET_CORE0_UNIMAC;
    } else if (ethcore == 1) {
        return ENET_CORE1_UNIMAC;
    } else {
        printk("Fatal error: Ethernet core %d doesn't exist\n", ethcore);
        /* Loop here forever */
        while (1) ;
    }

    /* Never reached */
    return 0;
}

static int ether_gphy_reset(int dmaPort)
{
    uint16_t reset;

    /* set gpio pad to floating state */
    GPIO->gpiopullup = 0;
    GPIO->gpiopulldown = 0;

    /* reset the external phy */
    if (BpGetPhyResetGpio(0, dmaPort, &reset) != BP_SUCCESS)
    {
        printk(KERN_EMERG "Phy reset gpio not found\n");
        /* put the core back into reset */
        /*
        if (softc->dmaPort == 0) {
            ethercore_disable(ENET_CORE0_WRAP);
        } else if (softc->dmaPort == 1) {
            ethercore_disable(ENET_CORE1_WRAP);
        }
        */
        return -1;
    }

    /* keep RESET low for 2 us */
    bcm_gpio_set_data(reset, 0);
    bcm_gpio_set_dir(reset, GPIO_OUT);
    udelay(2);

    /* keep RESET high for at least 2 us */
    bcm_gpio_set_data(reset, 1);
    udelay(2);

    return 0;
}

void ethercore_enable(volatile Aidmp *wrap)
{
    int loop_counter = 10;

    /* Put core into reset state */
    wrap->resetctrl = AIRC_RESET;
    udelay(1000);

    /* Ensure there are no pending backplane operations */
    SPINWAIT(wrap->resetstatus, 300);

    wrap->ioctrl = SICF_FGC | SICF_CLOCK_EN;

    /* Ensure there are no pending backplane operations */
    SPINWAIT(wrap->resetstatus, 300);

    while (wrap->resetctrl != 0 && --loop_counter) {
        SPINWAIT(wrap->resetstatus, 300);
        /* Take core out of reset */
        wrap->resetctrl = 0;
        SPINWAIT(wrap->resetstatus, 300);
    }

    wrap->ioctrl = SICF_CLOCK_EN;
    udelay(1000);
}

/*
 * Configures the Ethernet core clock (from PMU)
 */
static void ethercore_clock_init(int ethcore)
{
    volatile EnetCoreMisc *misc_regs = gmac_misc_regs(ethcore);

    /* set gmac into loopback mode to ensure no rx traffic */
    //gmac_macloopback(TRUE);
    //udelay(1);

    /* ethernet clock is generated by the PMU */
    misc_regs->clk_ctl_st |= CS_ER;
    SPINWAIT((misc_regs->clk_ctl_st & CS_ES) != CS_ES, 1000);

    /* configure gmac and switch data for PMU */
    if (PMUCTL_ENAB) {
        PMU->chipcontrol_addr = PMU_CHIPCTL4;
        PMU->chipcontrol_data &= ~(PMU_CC4_IF_TYPE_MASK | PMU_CC4_SW_TYPE_MASK);
        PMU->chipcontrol_data |= PMU_CC4_IF_TYPE_RGMII | PMU_CC4_SW_TYPE_RGMII;
    }

    /* set phy control: set smi_master to drive mdc_clk */
    misc_regs->phycontrol |= PC_MTE;

    /* Read the devstatus to figure out the configuration mode of
     * the interface. Set the speed to 100 if the switch interface
     * is mii/rmii. We know that we have rgmii, just maintained for
     * completeness.
     */
    /* NOT REALLY NECESSARY, REMOVE */
    //gmac_miiconfig();
}

static void unimac_init_reset(volatile EnetCoreUnimac *unimac_regs)
{
    /* put mac in software reset */
    unimac_regs->cmdcfg |= CC_SR;
    udelay(GMAC_RESET_DELAY);
}



static void unimac_clear_reset(volatile EnetCoreUnimac *unimac_regs)
{
    /* bring mac out of software reset */
    unimac_regs->cmdcfg &= ~CC_SR;
    udelay(GMAC_RESET_DELAY);
}

static void unimac_flowcontrol(volatile EnetCoreUnimac *unimac_regs,
                             bool tx_flowctrl, bool rx_flowctrl)
{
    uint32 cmdcfg;

    cmdcfg = unimac_regs->cmdcfg;

    /* put the mac in reset */
    unimac_init_reset(unimac_regs);

    /* to enable tx flow control clear the rx pause ignore bit */
    if (tx_flowctrl)
        cmdcfg &= ~CC_RPI;
    else
        cmdcfg |= CC_RPI;

    /* to enable rx flow control clear the tx pause transmit ignore bit */
    if (rx_flowctrl)
        cmdcfg &= ~CC_TPI;
    else
        cmdcfg |= CC_TPI;

    unimac_regs->cmdcfg = cmdcfg;

    /* bring mac out of reset */
    unimac_clear_reset(unimac_regs);
}


static void unimac_promisc(volatile EnetCoreUnimac *unimac_regs, int mode)
{
    uint32 cmdcfg;

    cmdcfg = unimac_regs->cmdcfg;

    /* put the mac in reset */
    unimac_init_reset(unimac_regs);

    /* enable or disable promiscuous mode */
    if (mode)
        cmdcfg |= CC_PROM;
    else
        cmdcfg &= ~CC_PROM;

    unimac_regs->cmdcfg = cmdcfg;

    /* bring mac out of reset */
    unimac_clear_reset(unimac_regs);
}

//fixme
void gmac_reset_mib(void)
{
    volatile EnetCoreMib *mib_regs = gmac_mib_regs(0);
    volatile uint32_t *p;

    /*
     * mib_regs->tx_good_octets is the first counter, mib_regs->rx_uni_pkts is
     * the last one and the register space between them is filled with the rest
     * of the 32-bit counters.
     * Watch out for the 32-bit gap after mib_regs->tx_q3_octets_high!
     */
    for (p = &mib_regs->tx_good_octets; p <= &mib_regs->rx_uni_pkts; p++) {
        *p = 0;
        if (p == &mib_regs->tx_q3_octets_high) {
            /* Skip a hole in the register space to avoid a bus error */
            p++;
        }
    }
}

static void gmac_enable(int ethcore)
{
    uint32 cmdcfg, rxqctl, bp_clk, mdp, mode;
    volatile EnetCoreMisc *misc_regs = gmac_misc_regs(ethcore);
    volatile EnetCoreUnimac *unimac_regs = gmac_unimac_regs(ethcore);

    cmdcfg = unimac_regs->cmdcfg;

    /* put mac in reset */
    unimac_init_reset(unimac_regs);

    /* initialize default config */
    cmdcfg = unimac_regs->cmdcfg;

    cmdcfg &= ~(CC_TE | CC_RE | CC_RPI | CC_TAI | CC_HD | CC_ML |
                CC_CFE | CC_RL | CC_RED | CC_PE | CC_TPI | CC_PAD_EN | CC_PF);
    cmdcfg |= (CC_PROM | CC_NLC | CC_CFE | CC_TPI | CC_AT);

    unimac_regs->cmdcfg = cmdcfg;

    /* bring mac out of reset */
    unimac_clear_reset(unimac_regs);

    /* Other default configurations */
    /* Enable RX and TX flow control */
    unimac_flowcontrol(unimac_regs, 1, 1);
    /* Disable promiscuous mode */
    unimac_promisc(unimac_regs, 0);

    /* Enable the mac transmit and receive paths now */
    udelay(2);
    cmdcfg &= ~CC_SR;
    //cmdcfg |= (CC_RE | CC_TE); move to gmac_set_active

    /* assert rx_ena and tx_ena when out of reset to enable the mac */
    unimac_regs->cmdcfg = cmdcfg;

    /* not force ht when gmac is in rev mii mode (we have rgmii mode) */
    mode = ((misc_regs->devstatus & DS_MM_MASK) >> DS_MM_SHIFT);
    if (mode != 0)
        /* request ht clock */
        misc_regs->clk_ctl_st |= CS_FH;

    /* Adjust RGMII TX delay time to meet the standard hold time limitation */
    misc_regs->devcontrol |= (0x3 << DC_TDS_SHIFT);

    /* init the mac data period. the value is set according to expr
     * ((128ns / bp_clk) - 3). */
    rxqctl = misc_regs->rxqctl;
    rxqctl &= ~RC_MDP_MASK;

    bp_clk = pmu_clk(PMU_PLL_CTRL_M3DIV_SHIFT) / 1000000;
    mdp = ((bp_clk * 128) / 1000) - 3;
    misc_regs->rxqctl = rxqctl | (mdp << RC_MDP_SHIFT);

    gmac_reset_mib();
}

int gmac_init(void)
{
    volatile Aidmp *wrap;
    volatile EnetCoreMisc *misc_regs;
    volatile EnetCoreUnimac *unimac_regs;
    ETHERNET_MAC_INFO EnetInfo[BP_MAX_ENET_MACS];
    int index;

    if(BpGetEthernetMacInfo(EnetInfo, BP_MAX_ENET_MACS) != BP_SUCCESS)
    {
        printk(KERN_DEBUG " board id not set\n");
        return -1;
    }

    for (index=0; index < BP_MAX_ENET_MACS; index++)
    {
        if ((EnetInfo[0].sw.port_map & (1 << index)) == 0)
            continue;

        if (index == 0)
            wrap = ENET_CORE0_WRAP;
        else 
            wrap = ENET_CORE1_WRAP;

        /*
         * Enable the Ethernet core in order to have access to the Ethernet core
         * registers.
         */
        ethercore_enable(wrap);
        ethercore_clock_init(index);

        misc_regs = gmac_misc_regs(index);
        unimac_regs = gmac_unimac_regs(index);
        ether_gphy_reset(index);
        /* enable one rx interrupt per received frame */
        misc_regs->intrecvlazy = 1 << IRL_FC_SHIFT;

        /* Set the MAC address */
        //unimac_regs->macaddrhigh = htonl(*(uint32 *)&hwaddr[0]);
        //unimac_regs->macaddrlow = htons(*(uint32 *)&hwaddr[4]);

        /* set max frame lengths - account for possible vlan tag */
        unimac_regs->rxmaxlength = BCM_MAX_PKT_LEN;

        /* Clear interrupts, don't enable yet */
        misc_regs->intmask = 0;
        misc_regs->intstatus = DEF_INTMASK;

        /* Turn ON the GMAC */
        gmac_enable(index);
    }
    return 0;
}

/* sets the GMAC to be active, and ROBO port to be inactive */
//fixme
int gmac_set_active( void )
{
    ETHERNET_MAC_INFO EnetInfo[BP_MAX_ENET_MACS];
    int index;
    uint32 cmdcfg;
    volatile EnetCoreUnimac *unimac_regs;

#if defined(CONFIG_BLOG) 
    BLOG_LOCK_BH();
#endif

    if(BpGetEthernetMacInfo(EnetInfo, BP_MAX_ENET_MACS) == BP_SUCCESS)
    {
        for (index=0; index < BP_MAX_ENET_MACS; index++)
        {
            if ((EnetInfo[0].sw.port_map & (1 << index)) == 0)
                continue;

            unimac_regs = gmac_unimac_regs(index);

            cmdcfg = unimac_regs->cmdcfg;
            /* Enable GMAC Tx and Rx */
            printk("Enable MAC core %d Rx & Tx (set bitMask 0x03)\n", index); 
            cmdcfg |= (CC_RE | CC_TE);
            unimac_regs->cmdcfg = cmdcfg;
        }
    }

#if defined(CONFIG_BLOG) 
    BLOG_UNLOCK_BH();
#endif

    return 0;
}

EXPORT_SYMBOL( gmac_init );
EXPORT_SYMBOL( gmac_set_active );

