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
 *  Created on: Dec 2016
 *      Author: steven.hsieh@broadcom.com
 */

/*
 * Phy drivers for 63138, 63148, 4908
 */
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include "bus_drv.h"
#include "phy_drv_sf2.h"
#include "phy_drv_mii.h"
#include "phy_drv_brcm.h"
#include "bcm_map_part.h"
#include "bcmnet.h"
#include "board.h"
#include "bcm_otp.h"
#include "bcm_OS_Deps.h"
#include "pmc_drv.h"
#include "bcmmii.h"
#include "bcmmii_xtn.h"
#include "bcmsfp_i2c.h"
#include "opticaldet.h"
#include "bcm_map_part.h"
#include "bcmnet.h"
#include "board.h"
#include "bcm_otp.h"
#include "linux/kernel.h"
#include "linux/spinlock.h"
#include "bcm_OS_Deps.h"
#include "pmc_drv.h"
#include "phy_drv_xgae.h"

#define SET_1GFD(p, v) do { \
    uint16 v16; \
    phy_bus_c45_read32(p, CL45_REG_1G_CTL, &v16); \
    v16 &= ~CL45_REG_1G_CTL_1G_ADV_MASK; \
    if (v>0) v16 |= CL45_REG_1G_CTL_1G_FD_ADV; \
    phy_bus_c45_write32(p, CL45_REG_1G_CTL, v16);} while(0)

#define SET_100MFD(p, v) do { \
    uint16 v16; \
    phy_bus_c45_read32(p, CL45_REG_COP_AN, &v16); \
    v16 &= ~CL45_REG_COP_AN_100M_ADV_MASK; \
    if (v>0) v16 |= CL45_REG_COP_AN_100M_FD_ADV; \
    phy_bus_c45_write32(p, CL45_REG_COP_AN, v16);} while(0)

#define SET_2P5G(p, v) do { \
    uint16 v16; \
    if (IsBrcm2P5GPhy(((phy_cl45_t *)(p->priv))->descriptor)) \
    { \
        phy_bus_c45_read32(p, CL45_REG_MGB_AN_CTL, &v16); \
        v16 = v>0? v16|CL45_REG_MGB_AN_2P5G_ADV|CL45_REG_MGB_ENABLE: \
                v16&~(CL45_REG_MGB_AN_2P5G_ADV|CL45_REG_MGB_ENABLE); \
        phy_bus_c45_write32(p, CL45_REG_MGB_AN_CTL, v16); \
    } \
    else \
    { \
        phy_bus_c45_read32(p, CL45REG_10GBASET_AN_DEF_CTL, &v16); \
        v16 = v>0? v16|CL45_10GAN_2P5G_ABL: v16&~CL45_10GAN_2P5G_ABL; \
        phy_bus_c45_write32(p, CL45REG_10GBASET_AN_DEF_CTL, v16); \
    } } while(0)

#define SET_10G(p, v) do { \
    uint16 v16; \
    phy_bus_c45_read32(p, CL45REG_10GBASET_AN_DEF_CTL, &v16); \
    v16 &= ~CL45_10GAN_5G_ABL; \
    v16 = v>0? v16|CL45_10GAN_10G_ABL : v16&~CL45_10GAN_10G_ABL; \
    phy_bus_c45_write32(p, CL45REG_10GBASET_AN_DEF_CTL, v16); \
    } while(0)


#define MII_CONTROL                         0x00
#define CL45_REG_IEEE_CTRL                  0x10000
    #define MII_CONTROL_RESET               (1 << 15)
    #define MII_CONTROL_LOOPBACK            (1 << 14)
    #define MII_CONTROL_SPEED_SEL13         (1 << 13)
    #define MII_CONTROL_AN_ENABLE           (1 << 12)

    #define MII_CONTROL_POWER_DOWN          (1 << 11)
    #define MII_CONTROL_ISOLATE_MII         (1 << 10)
    #define MII_CONTROL_RESTART_AUTONEG     (1 << 9)
    #define MII_CONTROL_DUPLEX_MODE         (1 << 8)

    #define MII_CONTROL_COLLISION_TEST      (1 << 7)
    #define MII_CONTROL_SPEED_SEL6          (1 << 6)

    #define MII_CONTROL_SPEED_MASK  (MII_CONTROL_SPEED_SEL13|MII_CONTROL_SPEED_SEL6)
    #define MII_CONTROL_SPEED_1000  (0<<13|1<<6)
    #define MII_CONTROL_SPEED_100   (1<<13|0<<6)
    #define MII_CONTROL_SPEED_10    (0)

#define MII_STATUS                          0x1
    #define MII_STATUS_LINK                 (1<<2)

#define MII_ASR                             0x19
#define MII_INTERRUPT                       0x1a
#define MII_INTERRUPT_MASK                  0x1b
#define MII_AS2SR                           0x1b  // Aux Status 2 Shadow Register (53101)
    #define MII_AS2SR_APD_EN                (1 << 5)
#define MII_REGISTER_1C                     0x1c
    #define MII_1C_WRITE_ENABLE             (1 << 15)
    #define MII_1C_RESERVED_CTRL3_SV        (0x05 << 10)
        #define MII_1C_RESERVED_CTRL3_DFLT  0x001e
        #define MII_1C_AUTO_PWRDN_DLL_DIS   (1 << 1)
        #define MII_1C_CLK125_OUTPUT_DIS    (1 << 0)
    #define MII_1C_AUTO_POWER_DOWN_SV       (0x0a << 10)
        #define MII_1C_APD_COMPATIBILITY    (1 << 8)
        #define MII_1C_AUTO_POWER_DOWN      (1 << 5)
        #define MII_1C_SLEEP_TIMER_SEL      (1 << 4)
        #define MII_1C_WAKEUP_TIMER_SEL_84  (1 << 0)
#define MII_1C_SHADOW_REG_SEL_S         10
#define MII_1C_SHADOW_REG_SEL_M         0x1F
#define MII_1C_SHADOW_CLK_ALIGN_CTRL        0x3
    #define GTXCLK_DELAY_BYPASS_DISABLE     (1 << 9)
#define MII_1C_SHADOW_LED_CONTROL           0x9
    #define ACT_LINK_LED_ENABLE             (1 << 4)
#define MII_1C_EXTERNAL_CONTROL_1           0xB
    #define LOM_LED_MODE                    (1 << 2)
#define MII_BRCM_TEST                       0x1f
    #define MII_BRCM_TEST_SHADOW_ENABLE     0x0080
    #define MII_BRCM_TEST_SHADOW2_ENABLE    0x0004

/* Shadow register 0x18 access */
#define MII_REGISTER_18                     0x18
   #define MII_REG_18_SEL(_s)               (((_s) << 12) | 0x7)
   #define MII_REG_18_WR(_s,_v)             (((_s) == 7 ? 0x8000 : 0) | (_v) | (_s)) /* Shadow 111 bit[15] = 1 for write */
      #define RGMII_RXD_TO_RXC_SKEW         (1 << 8) /* Based on 54616 but should be good for others */
#define MII_REG_18_SHADOW_MISC_CTRL         0x7

#define SERDES_1000X_CONTROL        0x8300
    #define SERDES_SIGNAL_DETECTE_EN    (1<<2)
    #define SERDES_INVERT_SIGNAL_DET    (1<<3)

static void ethsw_2p5g_serdes_link_stats(phy_dev_t *phy_dev);

DEFINE_MUTEX(bcm_phy_exp_mutex);
#if defined(PHY_SERDES_2P5G_CAPABLE)
typedef struct phyDesc {
    int devId;
    char *devName;
    int flag;
#define CL45PHY_10G_CAP (1<<0)
#define CL45PHY_BRCM2P5G_CAP (1<<1)
} phyDesc_t;

static phyDesc_t phyDesc[] = {
    {0xae025048, "84860", CL45PHY_BRCM2P5G_CAP},
    {0xae025040, "84861", CL45PHY_BRCM2P5G_CAP|CL45PHY_10G_CAP},
    {0xae025158, "84880"},
    {0xae025150, "84881", CL45PHY_10G_CAP},
};

#define IsBrcm2P5GPhy(pdesc) (((phyDesc_t *)(pdesc))->flag & CL45PHY_BRCM2P5G_CAP)

static int sf2_cl45phy_read_status(phy_dev_t *phy_dev);
static int ethsw_cl45phy_set_speed(phy_dev_t *phy_dev, phy_speed_t speed, phy_duplex_t duplex);
static int ethsw_cl45phy_get_config_speed(phy_dev_t *phy_dev, phy_speed_t *speed, phy_duplex_t *duplex);
static int ethsw_cl45phy_get_caps(phy_dev_t *phy_dev, uint32_t *caps);
#define MAX_CL45_PHYS 4
int dsl_runner_ext3_phy_init(phy_dev_t *phy_dev)
{
    static phy_cl45_t phys_cl45[MAX_CL45_PHYS];
    static int init;
    phy_drv_t *phy_drv = phy_dev->phy_drv;
    phy_cl45_t *phy_cl45;
    phy_drv_t _phy_drv = *phy_drv;
    uint32_t phy_id;
    int i;

    if (init > (MAX_CL45_PHYS - 1))
    {
        printk(" Error: More CL45 PHY %d requeted than supported %d\n", init+1, MAX_CL45_PHYS);
        BUG();
    }

    if (init++ == 0)
    {
        memset(phy_drv, 0, sizeof(*phy_drv));

        phy_drv->read_status = sf2_cl45phy_read_status;
        phy_drv->phy_type = _phy_drv.phy_type;
        phy_drv->name = _phy_drv.name;
        phy_drv->initialized = _phy_drv.initialized;
        phy_drv->bus_drv = _phy_drv.bus_drv;
        phy_drv->speed_set = ethsw_cl45phy_set_speed;
        phy_drv->caps_get = ethsw_cl45phy_get_caps;
        phy_drv->config_speed_get = ethsw_cl45phy_get_config_speed;
        phy_drv->phyid_get = _phy_drv.phyid_get;
        phy_drv->init = dsl_runner_ext3_phy_init;
    }

    phy_cl45 = phy_dev->priv = &phys_cl45[init++];
    phy_cl45->config_speed = -1;
    for (i=0; i<ARRAY_SIZE(phyDesc); i++)
    {
        phy_dev_phyid_get(phy_dev, &phy_id);
        if (phyDesc[i].devId == phy_id)
        {
            phy_cl45->descriptor = &phyDesc[i];
            break;
        }
    }

    if(i>=ARRAY_SIZE(phyDesc)) {
        printk("No matched PHY ID found\n");
        BUG();
    }

    phy_dev_speed_set(phy_dev, PHY_SPEED_AUTO, PHY_DUPLEX_FULL);
    return 0;
}

static u16 serdesRef50mVco6p25 [] =
{
    0x8000, 0x0c2f,
    0x8308, 0xc000,
    0x8050, 0x5740,
    0x8051, 0x01d0,
    0x8052, 0x19f0,
    0x8053, 0xaab0,
    0x8054, 0x8821,
    0x8055, 0x0044,
    0x8056, 0x8000,
    0x8057, 0x0872,
    0x8058, 0x0000,

    0x8106, 0x0020,
    0x8054, 0x8021,
    0x8054, 0x8821,
};

static u16 serdesSet2p5GFiber [] =
{
    0x0010, 0x0C2F,       /* disable pll start sequencer */
    0x8066, 0x0009,       /* Set AFE for 2.5G */
    0x8065, 0x1620,       
    0x8300, 0x0149,       /* enable fiber mode */
    0x8308, 0xC010,       /* Force 2.5G Fiber, enable 50MHz refclk */
    0x834a, 0x0001,       /* Set os2 mode */
    0x0000, 0x0140,       /* disable AN, set 1G mode */
    0x0010, 0x2C2F,       /* enable pll start sequencer */
};

static u16 serdesSet1GForcedFiber [] =
{
    0x0010, 0x0c2f,     /* disable pll start sequencer */
    0x8300, 0x0109,     /* Force Invert Signal Polarity */
    0x8066, 0x8009,     /* Set AFE to Default Values */
    0x8065, 0x3220,       
    0x8473, 0x1251,
    0x834a, 0x0003,
    0x0000, 0x0140,
    0x0010, 0x2c2f,     /* enable pll start sequencer */
};

static u16 serdesSet1GFiber [] =
{
    0x0010, 0x0c2f,     /* disable pll start sequencer */
    0x8300, 0x0149,     /* Force Auto Detect, Invert Signal Polarity */
    0x8066, 0x8009,     /* Set AFE to Default Values */
    0x8065, 0x3220,       
    0x8473, 0x1251,
    0x834a, 0x0003,
    0x0000, 0x1140,
    0x0010, 0x2c2f,     /* enable pll start sequencer */
};

static u16 serdesSet100MFiber [] =
{
    0x0010, 0x0c2f,     /* disable pll start sequencer */
    0x8300, 0x0009,     /* enable fiber mode */
    0x8400, 0x014b,     /* enable fxmode */
    0x8066, 0x8009,     /* Set AFE to Default Values */
    0x8065, 0x3220,       
    0x8402, 0x0880,     /* set disable_idle_correlator */
    0x8473, 0x1251,     /* Set step_one[1:0] */
    0x834a, 0x0003,     /* set os5 mode */
    0x0010, 0x2c2f,     /* enable pll start sequencer */
};

static u16 serdesSetAutoSGMII [] =
{
    0x0010, 0x0c2f,     /* disable pll start sequencer */
    0x8300, 0x0100,
    0x8066, 0x8009,     /* Set AFE to Default Values */
    0x8065, 0x3220,       
    0x8301, 0x0007,
    0x8473, 0x1251,
    0x834a, 0x0003,
    0x0000, 0x1140,
    0x0010, 0x2c2f,     /* enable pll start sequencer */
};

static u16 serdesSet100MForcedSGMII [] =
{
    0x0010, 0x0c2f,     /* disable pll start sequencer */
    0x8300, 0x0100,
    0x8066, 0x8009,     /* Set AFE to Default Values */
    0x8065, 0x3220,       
    0x8301, 0x0007,
    0x8473, 0x1251,
    0x834a, 0x0003,
    0x0000, 0x2100,
    0x0010, 0x2c2f,     /* enable pll start sequencer */
};
#else
static u16 serdesSet1GFiber [] =
{
    0x0010, 0x0c2f,
    0x8182, 0x4000,     /* This and next lines are for yield rate improvement */
    0x8186, 0x003c,
    0x8300, 0x015d,     /* Force Auto Detect, Invert Signal Polarity */
    0x8301, 0x7,
    0x0,    0x1140,
    0x0010, 0x2c2f
};

static u16 serdesSet100MFiber [] =
{
    0x0010, 0x0c2f,
    0x8182, 0x4000,     /* This and next lines are for yield rate improvement */
    0x8186, 0x003c,
    0x8300, 0xd,
    0x8400, 0x14b,
    0x8402, 0x880,
    0x0010, 0x2c2f,
};

static u16 serdesSetAutoSGMII [] =
{
    0x0010, 0x0c2f,
    0x8182, 0x4000,     /* This and next lines are for yield rate improvement */
    0x8186, 0x003c,
    0x8300, 0x0100,
    0x0,    0x1140,
    0x0010, 0x2c2f
};
#endif

static int ethsw_phy_reset(phy_dev_t *phy_dev);
static void config_serdes(phy_dev_t *phy_dev, u16 seq[], int seqSize);
static void ethsw_phy_autong_restart(phy_dev_t *phy_dev);

static void sf2_config_los_detected(phy_dev_t *phy_dev)
{
    u16 v16;

    phy_serdes_t *phy_serdes = phy_dev->priv;

    phy_bus_c45_read32(phy_dev, SERDES_1000X_CONTROL, &v16);
    if (phy_serdes->signal_detect_gpio != -1)
    {
        v16 |= SERDES_SIGNAL_DETECTE_EN;
        if (phy_serdes->signal_detect_invert)
            v16 |= SERDES_INVERT_SIGNAL_DET;
        else
            v16 &= ~SERDES_INVERT_SIGNAL_DET;
    }
    else
        v16 &= ~SERDES_SIGNAL_DETECTE_EN;
    phy_bus_c45_write32(phy_dev, SERDES_1000X_CONTROL, v16);
}

#if defined(PHY_SERDES_2P5G_CAPABLE)
static void ethsw_config_serdes_2p5g(phy_dev_t *phy_dev)
{
    ethsw_phy_reset(phy_dev);
    config_serdes(phy_dev, serdesRef50mVco6p25, sizeof(serdesRef50mVco6p25));
    msleep(1);
    config_serdes(phy_dev, serdesSet2p5GFiber, sizeof(serdesSet2p5GFiber));
    sf2_config_los_detected(phy_dev);
    ethsw_phy_autong_restart(phy_dev);
    msleep(50);
}

static void ethsw_config_serdes_1kx_forced(phy_dev_t *phy_dev)
{
    ethsw_phy_reset(phy_dev);
    config_serdes(phy_dev, serdesRef50mVco6p25, sizeof(serdesRef50mVco6p25));
    msleep(1);
    config_serdes(phy_dev, serdesSet1GForcedFiber, sizeof(serdesSet1GForcedFiber));
    msleep(50);
}

static void ethsw_config_serdes_100M_forced_sgmii(phy_dev_t *phy_dev)
{
    config_serdes(phy_dev, serdesSet100MForcedSGMII, sizeof(serdesSet100MForcedSGMII));
    ethsw_phy_autong_restart(phy_dev);
    msleep(50);
}

static int ethsw_cl45phy_set_speed(phy_dev_t *phy_dev, phy_speed_t speed, phy_duplex_t duplex)
{
    u16 v16;
    phy_cl45_t *phy_cl45 = phy_dev->priv;
    u32 caps;

    if (speed == phy_cl45->config_speed) return 1;

    caps = PHY_CAP_100_HALF|PHY_CAP_100_FULL|PHY_CAP_1000_HALF|PHY_CAP_1000_FULL|
        PHY_CAP_2500|PHY_CAP_5000|PHY_CAP_10000|PHY_CAP_AUTONEG;
    if (phy_dev->cascade_prev)
        phy_dev_caps_get(phy_dev->cascade_prev, &caps);

    SET_10G(phy_dev, 0);
    switch(speed)
    {
        case  PHY_SPEED_AUTO:
            if (caps & PHY_CAP_10000)
                SET_10G(phy_dev, 1);
            SET_2P5G(phy_dev, 1);
            SET_1GFD(phy_dev, 1);
            SET_100MFD(phy_dev, 1);
            break;

        case PHY_SPEED_10000:
            if (!(caps & PHY_CAP_10000))
                return -1;
            SET_10G(phy_dev, 1);
            SET_2P5G(phy_dev, 0);
            SET_1GFD(phy_dev, 0);
            SET_100MFD(phy_dev, 0);
            break;

        case PHY_SPEED_2500:
            SET_2P5G(phy_dev, 1);
            SET_1GFD(phy_dev, 0);
            SET_100MFD(phy_dev, 0);
            break;

        case PHY_SPEED_1000:
            SET_2P5G(phy_dev, 0);
            SET_1GFD(phy_dev, 1);
            SET_100MFD(phy_dev, 0);
            break;

        case PHY_SPEED_100:
            SET_2P5G(phy_dev, 0);
            SET_1GFD(phy_dev, 0);
            SET_100MFD(phy_dev, 1);
            break;
        default:
            printk("Not Supported Speed %x attempted to set on this interface\n", speed);
            return -1;
    }

    if (!IsBrcm2P5GPhy(phy_cl45->descriptor))
    {
        /* Restart 10G level AN for IEEE2.5G */
        phy_bus_c45_read32(phy_dev, CL45REG_10GBASET_AN_CTL, &v16);
        v16 |= CL45_REG_10G_AN_ENABLE | CL45_REG_10G_AN_RESTART;
        phy_bus_c45_write32(phy_dev, CL45REG_10GBASET_AN_CTL, v16);
    }

    /* Restart Auto Negotiation */
    phy_bus_c45_read32(phy_dev, CL45_REG_1G100M_CTL, &v16);
    v16 |= CL45_REG_1G100M_AN_ENABLED | CL45_REG_1G100M_AN_RESTART;
    phy_bus_c45_write32(phy_dev, CL45_REG_1G100M_CTL, v16);

    phy_cl45->config_speed = speed;

    return 0;
}

static int ethsw_cl45phy_get_caps(phy_dev_t *phy_dev, uint32_t *caps)
{
    if (phy_dev->cascade_prev)
        return phy_dev_caps_get(phy_dev->cascade_prev, caps);
    return phy_dev_caps_get(phy_dev, caps);
}

static int ethsw_cl45phy_get_config_speed(phy_dev_t *phy_dev, phy_speed_t *speed, phy_duplex_t *duplex)
{
    phy_cl45_t *phy_cl45 = phy_dev->priv;
    
    *speed = phy_cl45->config_speed;
    *duplex = PHY_DUPLEX_FULL;
    return 0;
}

static int sf2_cl45phy_read_status(phy_dev_t *phy_dev)
{
    u16 v16;

    phy_bus_c45_read32(phy_dev, CL45_REG_UDEF_STATUS, &v16);
    phy_dev->link = (v16 & CL45_UDEF_STATUS_COPPER_LINK)> 0;

    switch (v16 & CL45_UDEF_STATUS_COPPER_SPD_M)
    {
        case CL45_UDEF_STATUS_COPPER_SPD_10G:
            phy_dev->speed = PHY_SPEED_10000;
            break;
        case CL45_UDEF_STATUS_COPPER_SPD_2P5G:
            phy_dev->speed = PHY_SPEED_2500;
            break;
        case CL45_UDEF_STATUS_COPPER_SPD_1G:
            phy_dev->speed = PHY_SPEED_1000;
            break;
        case CL45_UDEF_STATUS_COPPER_SPD_100M:
            phy_dev->speed = PHY_SPEED_100;
            break;
    }

    phy_dev->duplex = PHY_DUPLEX_FULL;

    return phy_dev->link;
}
#endif

enum {SFP_MODULE_OUT, SFP_MODULE_IN, SFP_LINK_UP};
enum SERDES_POWER_LEVEL {SERDES_POWER_DOWN, SERDES_POWER_STANDBY, SERDES_POWER_ON, SERDES_POWER_DOWN_FORCE};
enum
{
    SFP_NO_MODULE,
    SFP_FIBER,
    SFP_COPPER,
};

#define ETHSW_POWERUP_SERDES(phyDev) ethsw_powerup_serdes(phyDev, SERDES_POWER_ON)
#define ETHSW_POWERSTANDBY_SERDES(phyDev) ethsw_powerup_serdes(phyDev, SERDES_POWER_STANDBY)
#define ETHSW_POWERDOWN_SERDES(phyDev) ethsw_powerup_serdes(phyDev, SERDES_POWER_DOWN)
#define ETHSW_POWERDOWN_FORCE_OFF_SERDES(phyDev) ethsw_powerup_serdes(phyDev, SERDES_POWER_DOWN_FORCE)

static int ethsw_powerup_serdes(phy_dev_t *phy_dev, int powerLevel);
int sf2_serdes_phy_read_status(phy_dev_t *phy_dev);
int sf2_serdes_init(phy_dev_t *phy_dev);
static int ethsw_serdes_caps_set(phy_dev_t *phy_dev, u32 caps);
static int ethsw_serdes_power_mode_set(phy_dev_t *phy_dev, int mode);

static int ethsw_serdes_caps_set(phy_dev_t *phy_dev, u32 caps)
{
    return ethsw_serdes_cfg_speed_set(phy_dev, caps, 0);
}

phy_drv_t phy_drv_i2c_phy =
{
    .phy_type = PHY_TYPE_I2C_PHY,
    .name = "I2C",
};

static int sfp_i2c_module_detect(phy_dev_t *phy_dev);
/****************************************************************************
    Broadcom Extended PHY registers
****************************************************************************/
#define BRCM_MIIEXT_BANK            0x1f
    #define BRCM_MIIEXT_BANK_MASK       0xfff0
    #define BRCM_MIIEXT_ADDR_RANGE      0xffe0
    #define BRCM_MIIEXT_DEF_BANK        0x8000
#define BRCM_MIIEXT_OFFSET          0x10
    #define BRCM_MIIEXT_OFF_MASK    0xf

#define MIIEX_DIGITAL_STATUS_1000X          0x8304
    #define MIIEX_SPEED_SHIFT       3
    #define MIIEX_SPEED             (3<<MIIEX_SPEED_SHIFT)
        #define MIIEX_SPD10         (0<<MIIEX_SPEED_SHIFT)
        #define MIIEX_SPD100        (1<<MIIEX_SPEED_SHIFT)
        #define MIIEX_SPD1000       (2<<MIIEX_SPEED_SHIFT)
        #define MIIEX_SPD2500       (3<<MIIEX_SPEED_SHIFT)
    #define MIIEX_DUPLEX            (1<<2)
    #define MIIEX_LINK              (1<<1)
    #define MIIEX_SGMII_MODE        (1<<0)

#define PLL_AFE_CTRL1               0x8051
    #define PLL_AFE_PLL_PON_SHIFT   6
    #define PLL_AFE_PLL_PON_MASK    (0xf << PLL_AFE_PLL_PON_SHIFT)

#define TX_AFE_CTRL2                0x8067
    #define TX_AFE_TX_PON_SHIFT     3
    #define TX_AFE_TX_PON_MASK      (0xf << TX_AFE_TX_PON_SHIFT)

#define RX_AFE_CTRL0                0x80b2
    #define RX_AFE_RX_PON_SHIFT     8
    #define RX_AFE_RX_PON_MASK      (0xf << RX_AFE_RX_PON_SHIFT)

#define RX_AFE_CTRL2                0x80b5
    #define RX_AFE_CTRL2_DIV4       0x2
    #define RX_AFE_CTRL2_DIV10      0x20

static void sgmiiResCal(phy_dev_t *phy_dev)
{
    int val;
    uint16_t v16;

    ethsw_phy_exp_write(phy_dev, RX_AFE_CTRL2, RX_AFE_CTRL2_DIV4 | RX_AFE_CTRL2_DIV10);

    if(GetRCalSetting(RCAL_1UM_VERT, &val) != kPMC_NO_ERROR)
    {
        printk("AVS is not turned on, leave SGMII termination resistor values as current default\n");
        ethsw_phy_exp_read(phy_dev, PLL_AFE_CTRL1, &v16);
        printk("    PLL_PON: 0x%04x; ", (v16 & PLL_AFE_PLL_PON_MASK) >> PLL_AFE_PLL_PON_SHIFT);
        ethsw_phy_exp_read(phy_dev, TX_AFE_CTRL2, &v16);
        printk("TX_PON: 0x%04x; ", (v16 & TX_AFE_TX_PON_MASK) >> TX_AFE_TX_PON_SHIFT);
        ethsw_phy_exp_read(phy_dev, RX_AFE_CTRL0, &v16);
        printk("RX_PON: 0x%04x\n", (v16 & RX_AFE_RX_PON_MASK) >> RX_AFE_RX_PON_SHIFT);
        return;
    }

    val &= 0xf;
    printk("Setting SGMII Calibration value to 0x%x\n", val);

    ethsw_phy_exp_read(phy_dev, PLL_AFE_CTRL1, &v16);
    v16 = (v16 & (~PLL_AFE_PLL_PON_MASK)) | (val << PLL_AFE_PLL_PON_SHIFT);
    ethsw_phy_exp_write(phy_dev, PLL_AFE_CTRL1, v16);

    ethsw_phy_exp_read(phy_dev, TX_AFE_CTRL2, &v16);
    v16 = (v16 & (~TX_AFE_TX_PON_MASK)) | (val << TX_AFE_TX_PON_SHIFT);
    ethsw_phy_exp_write(phy_dev, TX_AFE_CTRL2, v16);

    ethsw_phy_exp_read(phy_dev, RX_AFE_CTRL0, &v16);
    v16 = (v16 & (~RX_AFE_RX_PON_MASK)) | (val << RX_AFE_RX_PON_SHIFT);
    ethsw_phy_exp_write(phy_dev, RX_AFE_CTRL0, v16);
}

static int sf2_2p5g_serdes_init(phy_dev_t *phy_dev)
{
    uint16_t v16;
    ethsw_powerup_serdes(phy_dev, SERDES_POWER_ON);

    /* read back for testing */
    phy_bus_read(phy_dev, MII_CONTROL, &v16);

    /* Calibrate SGMII termination resistors */
    sgmiiResCal(phy_dev);

    /* If not a combo PHY, call the function to init state machine without leaving
       SFP on without control during initialization */
    if (phy_dev->cascade_next == NULL)
    {
        phy_dev->phy_drv->read_status(phy_dev);
    }

    return 0;
}

static int ethsw_serdes_speed_set(phy_dev_t *phy_dev, phy_speed_t speed, phy_duplex_t duplex);
#define MAX_SERDES_NUMBER 2
static phy_serdes_t serdes[MAX_SERDES_NUMBER] = {
    {
        .phy_type = PHY_TYPE_SF2_SERDES,
        .speed_caps = PHY_CAP_AUTONEG|PHY_CAP_2500|PHY_CAP_1000_FULL|PHY_CAP_100_FULL,
        .link_stats = ethsw_2p5g_serdes_link_stats,
        .config_speed = PHY_SPEED_AUTO,
        .serdes_init = sf2_2p5g_serdes_init,
        .power_mode = SERDES_NO_POWER_SAVING,
        .speed_set = ethsw_serdes_speed_set,
    },
#if defined(PHY_SERDES_10G_CAPABLE)
    {
        .phy_type = PHY_TYPE_XGAE,
        //.speed_caps = PHY_CAP_AUTONEG|PHY_CAP_10000|PHY_CAP_2500|PHY_CAP_1000_FULL|PHY_CAP_100_FULL,
        .speed_caps = PHY_CAP_AUTONEG|PHY_CAP_10000|PHY_CAP_2500|PHY_CAP_1000_FULL,
        .link_stats = ethsw_xgae_serdes_link_stats,
        .config_speed = PHY_SPEED_AUTO,
        //.config_speed = PHY_SPEED_10000,
        .serdes_init = sf2_xgae_serdes_init,
        .power_mode = SERDES_NO_POWER_SAVING,
        .speed_set = ethsw_xgae_speed_set,
    },
#endif
};

int sf2_serdes_init(phy_dev_t *phy_dev)
{
    static int i;
    phy_serdes_t *phy_serdes;
    int ret = 0;
#if defined(CONFIG_I2C)
    phy_dev_t *i2c_phy;
    int bus_num = -1;
#endif
    phy_drv_t *phy_drv = phy_dev->phy_drv;

    for (i = 0; i < MAX_SERDES_NUMBER; i++)
        if (!serdes[i].used && serdes[i].phy_type == phy_drv->phy_type)
            break;

    if (i == MAX_SERDES_NUMBER) return -1;
    phy_serdes = phy_dev->priv = &serdes[i];
    phy_serdes->used = 1;

#if defined(_BCM94908_) || defined(CONFIG_BCM94908)
    {
        int val;
        bcm_otp_is_sgmii_disabled(&val);
        if(val)
        {
            printk("****** Error: Invalid Serdes PHY defiend in board parameter - this chip does not support Serdes.\n");
            BUG();
        }
    }
#endif

#if defined(CONFIG_I2C)
    if (phy_dev->cascade_next == NULL)
    {
        phy_driver_set(&phy_drv_i2c_phy);
        switch (phy_drv->phy_type)
        {
            case PHY_TYPE_SF2_SERDES:
                 if( opticaldet_get_sgmii_i2c_bus_num(&bus_num) == OPTICALDET_NOBUS) {
                     printk("Error: ****** No I2C bus number defined for SGMII interface!\n");
                     printk("              Wrong board parameters or wrong board design.\n");
                     BUG();
                 }
                break;
            case PHY_TYPE_XGAE:
                if( opticaldet_get_xpon_i2c_bus_num(&bus_num) == OPTICALDET_NOBUS) {
                    printk("Error: ****** No I2C bus number defined for SGMII interface!\n");
                    printk("              Wrong board parameters or wrong board design.\n");
                    BUG();
                }
                break;
            default:
                printk(" Unknown Serdes Type: %d\n", phy_drv->phy_type);
                BUG();
        }
        i2c_phy = phy_dev_add(PHY_TYPE_I2C_PHY, bus_num, 0);

        phy_dev->cascade_next = i2c_phy;
        i2c_phy->cascade_prev = phy_dev;
        i2c_phy->flag = PHY_FLAG_NOT_PRESENTED;
        i2c_phy->phy_drv->bus_drv = bus_drv_get(BUS_TYPE_DSL_I2C);
    }
#endif
/* Get SFP MOD_ABS GPIO definition */
    if (BpGetSfpDetectGpio(&phy_serdes->sfp_module_detect_gpio) != BP_SUCCESS
#if defined(CONFIG_BP_PHYS_INTF)
        && BpGetSfpModDetectGpio(phy_serdes->phy_type == PHY_TYPE_SF2_SERDES? BP_INTF_TYPE_SGMII: BP_INTF_TYPE_xPON, 
        0, &phy_serdes->sfp_module_detect_gpio) != BP_SUCCESS
#endif
        ) {
        phy_serdes->sfp_module_detect_gpio = -1;
    }
    else
        phy_serdes->sfp_module_detect_gpio &= BP_GPIO_NUM_MASK;
        
    /* Get LOS GPIO definition */
    if (BpGetSgmiiGpios(&phy_serdes->signal_detect_gpio) != BP_SUCCESS
#if defined(CONFIG_BP_PHYS_INTF)
        && BpGetSfpSigDetect(phy_serdes->phy_type == PHY_TYPE_SF2_SERDES? BP_INTF_TYPE_SGMII: BP_INTF_TYPE_xPON, 
            0, &phy_serdes->signal_detect_gpio) != BP_SUCCESS
#endif
        ) {
        phy_serdes->signal_detect_gpio = -1;
    }
    else {
        phy_serdes->signal_detect_invert = (phy_serdes->signal_detect_gpio & BP_ACTIVE_LOW) > 0;
        phy_serdes->signal_detect_gpio &= BP_GPIO_NUM_MASK;
    }
 
    /* Validate MOD_ABS GPIO definition */
    if (phy_serdes->sfp_module_detect_gpio != -1) {
        printk("GPIO Pin %d is configured as SFP MOD_ABS for module insertion detection\n", 
            phy_serdes->sfp_module_detect_gpio);
        kerSysSetGpioDirInput(phy_serdes->sfp_module_detect_gpio);
    }
    else {
        if (!phy_dev->cascade_next || (phy_dev->cascade_next->flag & PHY_FLAG_NOT_PRESENTED)) /* SFP(No Copper PHY) case */
            printk("Warning: ***** No GPIO Pin defined for SFP module insertion. Energy detection will be used\n");
    }

    /* Validate LOS GPIO Definition */
    if (phy_serdes->signal_detect_gpio != -1) {
        printk("GPIO Pin %d is configured as Serdes Loss Of Signal Detection\n", 
            phy_serdes->signal_detect_gpio);

#if defined(CONFIG_BCM63138) || defined(CONFIG_BCM63148)
        if (phy_serdes->signal_detect_gpio == 36)
            MISC->miscSGMIIFiberDetect = MISC_SGMII_FIBER_GPIO36;
        else
            MISC->miscSGMIIFiberDetect = 0;
#endif
    }
    else {
        if (phy_drv->phy_type == PHY_TYPE_SF2_SERDES && 
                (!phy_dev->cascade_next || (phy_dev->cascade_next->flag & PHY_FLAG_NOT_PRESENTED))) { /* SFP(No Copper PHY) case */
            printk("Error: ****** No GPIO for Loss Of Signal Detection defined.\n");
            printk("              Wrong board parameters or wrong board design.\n");
            BUG();
        }
    }

    if (phy_serdes->speed_caps & PHY_CAP_10000) {
        phy_serdes->highest_speed_cap = PHY_CAP_10000;
        phy_serdes->highest_speed = PHY_SPEED_10000;
    } else if (phy_serdes->speed_caps & PHY_CAP_2500) {
        phy_serdes->highest_speed_cap = PHY_CAP_2500;
        phy_serdes->highest_speed = PHY_SPEED_2500;
    } else if (phy_serdes->speed_caps & PHY_CAP_1000_FULL) {
        phy_serdes->highest_speed_cap = PHY_CAP_1000_FULL;
        phy_serdes->highest_speed = PHY_SPEED_1000;
    } else if (phy_serdes->speed_caps & PHY_CAP_100_FULL) {
        phy_serdes->highest_speed_cap = PHY_CAP_100_FULL;
        phy_serdes->highest_speed = PHY_SPEED_100;
    }
    
    phy_serdes->serdes_init(phy_dev);
    return ret;
}

#include "crossbar_dev.h"
phy_dev_t *crossbar_get_phy_by_type(int phy_type);
int crossbar_phys_in_one_group(phy_dev_t *phy1, phy_dev_t *phy2);
/*
    static void serdes_work_around(int phy_id)
    Serdes work around during 100FX for dependency on Serdes status.
    When link is down, if crossbar is connected to Copper external port and
    Serdes is configured as 100FX, then 100FX Serdes will NEVER link up.
    The work around is to place crossbar back to Fibre external port when
    Copper port link down.
    phy_id:
        -1: Initialize work around.
        Others: current link down PHY ID.
*/
void serdes_work_around(phy_dev_t *phy_dev)
{
#if defined(SWITCH_REG_SINGLE_SERDES_CNTRL)
    static phy_dev_t *crossbar_serdes_phy;
    phy_serdes_t *phy_serdes = crossbar_serdes_phy ? crossbar_serdes_phy->priv : NULL;

    if(phy_dev == NULL)    /* Init work around */
    {
        crossbar_serdes_phy = crossbar_get_phy_by_type(PHY_TYPE_SF2_SERDES);
        if (!crossbar_serdes_phy)
            return;
    }

    if (!crossbar_serdes_phy || !crossbar_phys_in_one_group(phy_dev, crossbar_serdes_phy) ||
        (phy_serdes->config_speed != PHY_SPEED_AUTO &&
        phy_serdes->config_speed != PHY_SPEED_100) ||
        phy_dev->phy_drv->phy_type == PHY_TYPE_SF2_SERDES)
    {
        /* If no Serdes in layout, link down PHY in different Switch Group from Serdes, 
          no 100FX configured or Serdes is Link down, no action is taken */
        return;
    }
end:
    crossbar_plat_select(-1,-1,-1, crossbar_serdes_phy);
#endif /* SERDES */
}
EXPORT_SYMBOL(serdes_work_around);

int ethsw_phy_exp_rw(phy_dev_t *phy_dev, u32 reg, u16 *v16_p, int rd)
{
    u32 bank, offset;
    int rc = 0;

    if (reg < 0x20) {   /* CL22 space */
        if (IsC45Phy(phy_dev)) {
            printk("phy_id: %d does not support Clause 22\n", phy_dev->addr);
            return rc;
        }
        if (rd)
            rc = phy_bus_read(phy_dev, reg, v16_p);
        else
            rc = phy_bus_write(phy_dev, reg, *v16_p);
        return rc;
    }
    else if (reg < 0x10000) /* expanded MDIO space */
    {
        bank = reg & BRCM_MIIEXT_BANK_MASK;
        offset = (reg & BRCM_MIIEXT_OFF_MASK) + BRCM_MIIEXT_OFFSET;

        mutex_lock(&bcm_phy_exp_mutex);
        /* Set Bank Address */
        rc = phy_bus_write(phy_dev, BRCM_MIIEXT_BANK, bank);

        if (!rd)
            rc += phy_bus_write(phy_dev, offset, *v16_p);
        else
            rc += phy_bus_read(phy_dev, offset, v16_p);

        /* Set Bank back to default for standard access */
        if(bank != BRCM_MIIEXT_DEF_BANK || offset == BRCM_MIIEXT_OFFSET)
            rc += phy_bus_write(phy_dev, BRCM_MIIEXT_BANK, BRCM_MIIEXT_DEF_BANK);
        mutex_unlock(&bcm_phy_exp_mutex);
    }
    else if (reg < 0x200000) /* CL45 space */
    {
        if (!IsC45Phy(phy_dev)) {
            printk("phy_id=%d does not support Clause 45\n", phy_dev->addr);
            return rc;
        }

        if (rd)
            rc = phy_bus_c45_read32(phy_dev, reg, v16_p);
        else
            rc = phy_bus_c45_write32(phy_dev, reg, *v16_p);
    }

    return rc;
}
EXPORT_SYMBOL(ethsw_phy_exp_rw);


static void config_serdes(phy_dev_t *phy_dev, u16 seq[], int seqSize)
{
    int i;

    seqSize /= sizeof(seq[0]);
    for(i=0; i<seqSize; i+=2)
    {
        ethsw_phy_exp_write(phy_dev, seq[i], seq[i+1]);
    }
}

static int ethsw_phy_reset(phy_dev_t *phy_dev)
{
    u16 v16;
    u32 reg;
    int i=0;

    /* Reset PHY to clear status first */

    if (IsC45Phy(phy_dev))
    {
        reg = CL45_REG_IEEE_CTRL;
        v16= MII_CONTROL_RESET;
        phy_bus_c45_write32(phy_dev, reg, v16);
        for(phy_bus_c45_read32(phy_dev, reg, &v16); v16 & MII_CONTROL_RESET;
                phy_bus_c45_read32(phy_dev, reg, &v16));
        {
            if (++i > 20) {printk("Failed to reset 0x%x\n", phy_dev->addr); return 0;}
            msleep(100);
        }
    }
    else
    {
        reg = MII_CONTROL;
        v16= MII_CONTROL_RESET;
        phy_bus_write(phy_dev, reg, v16);
        for(phy_bus_read(phy_dev, reg, &v16); v16 & MII_CONTROL_RESET;
                phy_bus_read(phy_dev, reg, &v16));
        {
            if (++i > 20) {printk("Failed to reset 0x%x\n", phy_dev->addr); return 0;}
            msleep(100);
        }
    }

    return 1;
}

static int ethsw_powerup_serdes(phy_dev_t *phy_dev, int powerLevel)
{
    u32 val32;
    phy_serdes_t *phy_serdes = phy_dev->priv;
    u16 v16;

    if (powerLevel == phy_serdes->cur_power_level)
        return 0;

    val32 = *(u32 *)SWITCH_REG_SINGLE_SERDES_CNTRL;
    switch(powerLevel)
    {
        case SERDES_POWER_ON:
            val32 |= SWITCH_REG_SERDES_RESETPLL|SWITCH_REG_SERDES_RESETMDIO|SWITCH_REG_SERDES_RESET;
            val32 &= ~(SWITCH_REG_SERDES_IQQD|SWITCH_REG_SERDES_PWRDWN);
            *(u32 *)SWITCH_REG_SINGLE_SERDES_CNTRL = val32;
            msleep(1);
            val32 &= ~(SWITCH_REG_SERDES_RESETPLL|SWITCH_REG_SERDES_RESETMDIO|SWITCH_REG_SERDES_RESET);
            *(u32 *)SWITCH_REG_SINGLE_SERDES_CNTRL = val32;

            /* Do dummy MDIO read to work around ASIC problem */
            phy_bus_read(phy_dev, 0, &v16);
            break;
        case SERDES_POWER_STANDBY:
            if (val32 & SWITCH_REG_SERDES_IQQD) {
                val32 |= SWITCH_REG_SERDES_PWRDWN;
                val32 &= ~SWITCH_REG_SERDES_IQQD;
                *(u32 *)SWITCH_REG_SINGLE_SERDES_CNTRL = val32;
                msleep(1);
            }
            // note lack of break here
        case SERDES_POWER_DOWN:
            val32 |= SWITCH_REG_SERDES_PWRDWN|SWITCH_REG_SERDES_RESETPLL|
                SWITCH_REG_SERDES_RESETMDIO|SWITCH_REG_SERDES_RESET;
            *(u32 *)SWITCH_REG_SINGLE_SERDES_CNTRL = val32;
            break;
        case SERDES_POWER_DOWN_FORCE:
            val32 |= SWITCH_REG_SERDES_PWRDWN|SWITCH_REG_SERDES_RESETPLL|
                SWITCH_REG_SERDES_RESETMDIO|SWITCH_REG_SERDES_RESET|
                SWITCH_REG_SERDES_IQQD;
            *(u32 *)SWITCH_REG_SINGLE_SERDES_CNTRL = val32;
            break;
        default:
            printk("Wrong power level request to Serdes module\n");
            return 0;
    }
    phy_serdes->current_speed = -1;
    phy_serdes->cur_power_level = powerLevel;
    return 0;
}

static void ethsw_phy_autong_restart(phy_dev_t *phy_dev)
{
    u16 val16;

    phy_bus_read(phy_dev, MII_CONTROL, &val16);
    val16 |= MII_CONTROL_RESTART_AUTONEG;
    phy_bus_write(phy_dev, MII_CONTROL, val16);
}

void ethsw_config_serdes_1kx(phy_dev_t *phy_dev)
{
    ethsw_phy_reset(phy_dev);
#if defined(PHY_SERDES_2P5G_CAPABLE)
    config_serdes(phy_dev, serdesRef50mVco6p25, sizeof(serdesRef50mVco6p25));
    msleep(1);
#endif
    config_serdes(phy_dev, serdesSet1GFiber, sizeof(serdesSet1GFiber));
    sf2_config_los_detected(phy_dev);
    ethsw_phy_autong_restart(phy_dev);
    msleep(50);
}

void ethsw_config_serdes_100fx(phy_dev_t *phy_dev)
{
    ethsw_phy_reset(phy_dev);
#if defined(PHY_SERDES_2P5G_CAPABLE)
    config_serdes(phy_dev, serdesRef50mVco6p25, sizeof(serdesRef50mVco6p25));
    msleep(1);
#endif
    config_serdes(phy_dev, serdesSet100MFiber, sizeof(serdesSet100MFiber));
    sf2_config_los_detected(phy_dev);
    msleep(80);
}

static void ethsw_config_serdes_auto_sgmii(phy_dev_t *phy_dev)
{
    config_serdes(phy_dev, serdesSetAutoSGMII, sizeof(serdesSetAutoSGMII));
    ethsw_phy_autong_restart(phy_dev);
    msleep(50);
}

#define SERDES_AUTO_DETECT_INT_MS 200
#define SERDES_FIBRL_SETTLE_DELAY_MS 400

/*
    The testing result shows lower speed will be easier to link up
   during the fibre insertion, thus we are doing retry of the highest
   speed when linked in non highest speed.
*/
#define PHY_CAP_SPEED_MASK ((PHY_CAP_10000 << 1) - 1)
#define IS_PHY_HIGHEST_SPEED_CAP(phy_serdes, curSpeed) (phy_serdes->speed_caps & (PHY_CAP_SPEED_MASK)) & (~((curSpeed<<1)-1))
static void ethsw_serdes_speed_detect(phy_dev_t *phy_dev)
{
    static int retry = 0;
    phy_serdes_t *phy_serdes = phy_dev->priv;

    if (phy_serdes->sfp_module_type != SFP_FIBER ||
        phy_serdes->config_speed != PHY_SPEED_AUTO) return;

    if (phy_serdes->speed_caps & PHY_CAP_10000)
    {
        phy_serdes->speed_set(phy_dev, PHY_SPEED_10000, PHY_DUPLEX_FULL);
        msleep(SERDES_AUTO_DETECT_INT_MS);
        phy_serdes->link_stats(phy_dev);
        if (phy_dev->link) return;
    }

    if (phy_serdes->speed_caps & PHY_CAP_2500)
    {
        phy_serdes->speed_set(phy_dev, PHY_SPEED_2500, PHY_DUPLEX_FULL);
        msleep(SERDES_AUTO_DETECT_INT_MS);
        phy_serdes->link_stats(phy_dev);
        if (phy_dev->link)
        {
            if (IS_PHY_HIGHEST_SPEED_CAP(phy_serdes, PHY_CAP_2500))
                return;
            else
                goto LinkUp;
        }
    }

    if (phy_serdes->speed_caps & PHY_CAP_1000_FULL)
    {
        phy_serdes->speed_set(phy_dev, PHY_SPEED_1000, PHY_DUPLEX_FULL);
        msleep(SERDES_AUTO_DETECT_INT_MS);
        phy_serdes->link_stats(phy_dev);
        if (phy_dev->link)
        {
            if (IS_PHY_HIGHEST_SPEED_CAP(phy_serdes, PHY_CAP_1000_FULL)) 
                return;
            else
                goto LinkUp;
        }
    }

    if (phy_serdes->speed_caps & PHY_CAP_100_FULL)
    {
        phy_serdes->speed_set(phy_dev, PHY_SPEED_100, PHY_DUPLEX_FULL);
        msleep(SERDES_AUTO_DETECT_INT_MS);
        phy_serdes->link_stats(phy_dev);
        if (phy_dev->link)
        {
            if (IS_PHY_HIGHEST_SPEED_CAP(phy_serdes, PHY_CAP_100_FULL)) 
                return;
            else
                goto LinkUp;
        }
    }

    goto NoLinkUp;

LinkUp:
    if (retry) return; /* If we retried already, return; */

    /* Otherwise, take a sleep to let fibre settle down, then retry higher speed */
    retry++;
    msleep(SERDES_FIBRL_SETTLE_DELAY_MS);
    ethsw_serdes_speed_detect(phy_dev);
    goto end;

NoLinkUp:
    /*
        No link up here.
        Set speed to highest when in NO_POWER_SAVING_MODE until next detection
    */
    if( phy_serdes->power_mode == SERDES_NO_POWER_SAVING)
        phy_serdes->speed_set(phy_dev, phy_serdes->highest_speed, PHY_DUPLEX_FULL);
end:
    retry = 0;
}

int ethsw_serdes_caps_get(phy_dev_t *phy_dev, uint32_t *caps)
{
    phy_serdes_t *phy_serdes = phy_dev->priv;
    *caps = phy_serdes->speed_caps;
    return 0;
}

static int ethsw_serdes_speed_set(phy_dev_t *phy_dev, phy_speed_t speed, phy_duplex_t duplex)
{
    phy_serdes_t *phy_serdes = phy_dev->priv;

    if (phy_serdes->current_speed == speed) 
        return 0;

    switch ((int)speed)
    {
        case PHY_SPEED_AUTO:
            break;
#if defined(PHY_SERDES_2P5G_CAPABLE)
        case PHY_SPEED_2500:
            ethsw_config_serdes_2p5g(phy_dev);
            break;
#endif
        case PHY_SPEED_1000:
#if defined(PHY_SERDES_2P5G_CAPABLE)
            if (phy_dev->cascade_next && !(phy_dev->cascade_next->flag & PHY_FLAG_NOT_PRESENTED))
            {
                ethsw_config_serdes_1kx_forced(phy_dev);
            }
            else
#endif
            {
                ethsw_config_serdes_1kx(phy_dev);
            }
            break;
        case PHY_SPEED_100:
#if defined(PHY_SERDES_2P5G_CAPABLE)
            if (phy_dev->cascade_next && !(phy_dev->cascade_next->flag & PHY_FLAG_NOT_PRESENTED))
            {
                ethsw_config_serdes_100M_forced_sgmii(phy_dev);
            }
            else
#endif
            {
                ethsw_config_serdes_100fx(phy_dev);
            }
            break;
        default:
            printk("Not supported speed: 0x%x\n", speed);
            return -1;
    }
    phy_serdes->current_speed = speed;
    return 0;
}

int ethsw_serdes_cfg_speed_set(phy_dev_t *phy_dev, phy_speed_t speed, phy_duplex_t duplex)
{
    phy_serdes_t *phy_serdes = phy_dev->priv;

    if (speed != PHY_SPEED_AUTO && 
#if defined(PHY_SERDES_10G_CAPABLE)
        speed != PHY_SPEED_10000 &&
#endif
#if defined(PHY_SERDES_2P5G_CAPABLE)
        speed != PHY_SPEED_2500 &&
#endif
        speed != PHY_SPEED_1000 && speed != PHY_SPEED_100) {
            printk("Not supported speed: 0x%x\n", speed);
            return -1;
    }

    phy_serdes->config_speed = speed;
    phy_serdes->speed_set(phy_dev, speed, duplex);
    return 0;
}

static void ethsw_sfp_restore_from_power_saving(phy_dev_t *phy_dev)
{
    phy_serdes_t *phy_serdes = phy_dev->priv;

    if (phy_serdes->sfp_module_type == SFP_NO_MODULE)
        return;

#if defined(PHY_SERDES_2P5G_CAPABLE)
    config_serdes(phy_dev, serdesRef50mVco6p25, sizeof(serdesRef50mVco6p25));
    msleep(1);
#endif

    if(phy_serdes->sfp_module_type == SFP_COPPER)
    {
        /* Configure Serdes into SGMII mode */
        ethsw_config_serdes_auto_sgmii(phy_dev);
    }
    else
    {
        if (phy_serdes->config_speed == PHY_SPEED_AUTO)
            ethsw_serdes_speed_detect(phy_dev);
        else
            phy_serdes->speed_set(phy_dev, phy_dev->speed, PHY_DUPLEX_FULL);
    }
}

static void ethsw_2p5g_serdes_link_stats(phy_dev_t *phy_dev)
{
    uint16_t v16;

    phy_bus_read(phy_dev, MII_STATUS, &v16);
    phy_dev->link = (v16 & MII_STATUS_LINK) != 0;

    if(phy_dev->link)
    {
        ethsw_phy_exp_read(phy_dev, MIIEX_DIGITAL_STATUS_1000X, &v16);
        switch (v16 & MIIEX_SPEED)
        {
            case MIIEX_SPD2500:
                phy_dev->speed = PHY_SPEED_2500;
                break;
            case MIIEX_SPD1000:
                phy_dev->speed = PHY_SPEED_1000;
                break;
            case MIIEX_SPD100:
                phy_dev->speed = PHY_SPEED_100;
                break;
            case MIIEX_SPD10:
                phy_dev->speed = PHY_SPEED_10;
                break;
        }

        phy_dev->duplex = (v16 & MIIEX_DUPLEX)? PHY_DUPLEX_FULL: PHY_DUPLEX_HALF;
    }
}

static int ethsw_serdes_power_mode_set(phy_dev_t *phy_dev, int mode)
{
    phy_serdes_t *phy_serdes = phy_dev->priv;

    if (phy_serdes->power_mode == mode)
        return 0;

    phy_serdes->power_mode = mode;

    if (phy_serdes->sfp_status == SFP_LINK_UP)
        return 0;

    if(mode == SERDES_NO_POWER_SAVING)
    {
        ETHSW_POWERUP_SERDES(phy_dev);
        ethsw_sfp_restore_from_power_saving(phy_dev);
    }
    else if (mode == SERDES_FORCE_OFF)
        ETHSW_POWERDOWN_FORCE_OFF_SERDES(phy_dev);
    else
        ETHSW_POWERDOWN_SERDES(phy_dev);

    return 0;
}

static void ethsw_conf_copper_sfp(phy_dev_t *phy_dev, int speed)
{
    uint16 v16;

    phy_bus_write(phy_dev, MII_CONTROL, MII_CONTROL_RESET);     /* Software reset */

    /* Configure SFP PHY into SGMII mode */
    phy_bus_write(phy_dev, 0x1b, 0x9084);    /* Enable SGMII mode */
    phy_bus_write(phy_dev, MII_CONTROL, MII_CONTROL_RESET);     /* Software reset */

    if (speed == PHY_SPEED_AUTO)
    {
        phy_bus_write(phy_dev, 0x9, 0x0f00);     /* Advertise 1kBase-T Full/Half-Duplex */
        phy_bus_write(phy_dev, 0x0, 0x8140);     /* Software reset */
        phy_bus_write(phy_dev, 0x4, 0x0de1);     /* Adverstize 100/10Base-T Full/Half-Duplex */
        phy_bus_write(phy_dev, 0x0, 0x9140);     /* Software reset */

        return;
    }

    /* Fixed speed configuration */
    v16 = MII_CONTROL_RESET;
    if (speed == PHY_SPEED_1000)
    {
        v16 |= MII_CONTROL_SPEED_1000;
    }
    else if (speed == PHY_SPEED_100)
    {
        v16 |= MII_CONTROL_SPEED_100;
    }

    v16 |= MII_CONTROL_DUPLEX_MODE;
    phy_bus_write(phy_dev, MII_CONTROL, v16);
}

static int sfp_i2c_module_detect(phy_dev_t *phy_dev)
{
    u16 v16;
    phy_dev_t *cascade_next = phy_dev->cascade_next;
    phy_serdes_t *phy_serdes = phy_dev->priv;

    if (cascade_next == NULL) return 0;
   
#if defined(CONFIG_I2C)
    /* check if SFP i2c driver is fully initialized first */
    if( bcm_i2c_sfp_get_status(cascade_next->addr) != SFP_STATUS_INSERTED ) {
         /* sfp module is detected but bcmsfp driver is not finished initializing yet. 
         continue to wait */
         return 2;
    }
#endif

    /* If I2C read operation succeeds, I2C module is connected
       and which means it is a copper SFP module */
    if (phy_bus_read(cascade_next, MII_CONTROL, &v16) == 0 )
    {
        cascade_next->flag &= ~PHY_FLAG_NOT_PRESENTED;
        ETHSW_POWERDOWN_SERDES(phy_dev);  /* Power down and up Serdes to reset Serdes Status */
        ETHSW_POWERUP_SERDES(phy_dev);
        ethsw_conf_copper_sfp(cascade_next, phy_serdes->config_speed);
        ethsw_config_serdes_auto_sgmii(phy_dev);
        return 1;
    }

    cascade_next->flag |= PHY_FLAG_NOT_PRESENTED;
    return 0;
}

static int ethsw_sfp_module_detected(phy_dev_t *phy_dev)
{
    phy_serdes_t *phy_serdes = phy_dev->priv;

    if (phy_serdes->sfp_module_detect_gpio != -1)
    {
        return kerSysGetGpioValue(phy_serdes->sfp_module_detect_gpio) == 0;
    }
    else
    {
        return ((*(u32*)SWITCH_SINGLE_SERDES_STAT) & SWITCH_REG_SSER_RXSIG_DET) > 0;
    }
}

/*
   Module detection is not going through SGMII,
   so it can be down even under SGMII power down.
 */
static int ethsw_sfp_module_detect(phy_dev_t *phy_dev)
{
    int rc;
    phy_serdes_t *phy_serdes = phy_dev->priv;

    if (phy_dev->cascade_next && phy_serdes->i2cInitDetectDelay++ < I2CInitDetectDelay) 
        return 0;

    if ( ethsw_sfp_module_detected(phy_dev) == 0)
    {
        if(phy_serdes->sfp_module_type != SFP_NO_MODULE)
        {
            phy_serdes->sfp_module_type = SFP_NO_MODULE;
            printk("SFP module unplugged\n");
        }
        return 0;
    }

    if (phy_serdes->sfp_module_type == SFP_NO_MODULE)
    {
        /* This is a dynamic PHY */
        rc = sfp_i2c_module_detect(phy_dev);
        if( rc == 1 )
        {
            phy_serdes->sfp_module_type = SFP_COPPER;
            printk("SFP Module Plugged in\n");
            printk("Copper SFP Module Plugged in\n");
        }
        else if( rc == 0 )
        {
            phy_serdes->sfp_module_type = SFP_FIBER;
            if (phy_dev->cascade_next) phy_serdes->i2cDetectDelay = 0;
            printk("Fibre SFP Module Plugged in\n");
        } 
        /* otherwise continue to wait for sfp i2c driver to finish the initialization */
    }
#ifdef ADDITIONAL_I2C_SFP_DETECT  
    /* this is probably no longer needed as sfp_i2c_module_detect continue to wait until sfp driver
       init is done. */
    else if(phy_dev->cascade_next)   /* MODULE is plug in */
    {
        /* Work around for some I2C long initialization, do more I2C after Fiber type detected */
        if (phy_serdes->sfp_module_type == SFP_FIBER &&
                phy_serdes->i2cDetectDelay < I2CDetectDelay)
        {
            if (sfp_i2c_module_detect(phy_dev) == 1)
            {
                phy_serdes->sfp_module_type = SFP_COPPER;
                phy_serdes->i2cDetectDelay = I2CDetectDelay;
                printk("Copper SFP Module Detected\n");
            }
            else
            {
                phy_serdes->i2cDetectDelay++;
            }
        }
    }
#endif
    return 1;
}

int sf2_serdes_phy_read_status(phy_dev_t *phy_dev)
{
    phy_serdes_t *phy_serdes = phy_dev->priv;

    if (phy_serdes->power_mode > SERDES_NO_POWER_SAVING && phy_serdes->sfp_status < SFP_LINK_UP)
        ETHSW_POWERSTANDBY_SERDES(phy_dev);

    switch (phy_serdes->sfp_status)
    {
        case SFP_MODULE_OUT:
sfp_module_out:
            if(phy_serdes->sfp_status == SFP_MODULE_OUT && ethsw_sfp_module_detect(phy_dev))
                goto sfp_module_in;

            phy_serdes->sfp_status = SFP_MODULE_OUT;
            goto sfp_end;

        case SFP_MODULE_IN:
sfp_module_in:
            if(phy_serdes->sfp_status >= SFP_MODULE_IN && !ethsw_sfp_module_detect(phy_dev))
            {
                phy_serdes->sfp_status = SFP_MODULE_IN;
                goto sfp_module_out;
            }

            if(phy_serdes->sfp_status <= SFP_MODULE_IN)
            {
                if(phy_serdes->power_mode == SERDES_BASIC_POWER_SAVING)
                {
                    ETHSW_POWERUP_SERDES(phy_dev);
                    ethsw_sfp_restore_from_power_saving(phy_dev);
                }
                else if (phy_serdes->power_mode == SERDES_NO_POWER_SAVING)
                {
                    ethsw_serdes_speed_detect(phy_dev);
                }

                phy_serdes->link_stats(phy_dev);
                if (phy_dev->link)
                {
                    phy_serdes->sfp_status = SFP_MODULE_IN;
                    goto sfp_link_up;
                }
            }
            phy_serdes->sfp_status = SFP_MODULE_IN;
            goto sfp_end;

        case SFP_LINK_UP:
sfp_link_up:
            if(phy_serdes->sfp_status == SFP_LINK_UP)
            {
                if(!ethsw_sfp_module_detect(phy_dev)) goto sfp_module_out;
                phy_serdes->link_stats(phy_dev);
                if(!phy_dev->link)
                {
                    goto sfp_module_in;
                }
            }
            phy_serdes->sfp_status = SFP_LINK_UP;
            goto sfp_end;
    }

sfp_end:
    if( phy_serdes->power_mode > SERDES_NO_POWER_SAVING && phy_serdes->sfp_status != SFP_LINK_UP)
        ETHSW_POWERDOWN_SERDES(phy_dev);
    return phy_dev->link;
}

int ethsw_serdes_power_mode_get(phy_dev_t *phy_dev, int *mode)
{
    phy_serdes_t *phy_serdes = phy_dev->priv;
    *mode = phy_serdes->power_mode;
    return 0;
}

int ethsw_serdes_speed_get(phy_dev_t *phy_dev, phy_speed_t *speed, phy_duplex_t *duplex)
{
    phy_serdes_t *phy_serdes = phy_dev->priv;
    
    *speed = phy_serdes->config_speed;
    *duplex = PHY_DUPLEX_FULL;
    return 0;
}

int phy_isolate_phy(phy_dev_t *phy, int isolate)
{
    uint16 v16;

    phy_bus_read(phy, MII_BMCR, &v16);
    if (isolate) {
        v16 |= MII_CONTROL_ISOLATE_MII;
    } else {
        v16 &= ~MII_CONTROL_ISOLATE_MII;
    }
    phy_bus_write(phy, MII_CONTROL, v16);
    return 0;
}
EXPORT_SYMBOL(phy_isolate_phy);

static int _phy_init (phy_dev_t *phy_dev)
{
    return 0;
}

#if !(defined(_BCM94908_)  || defined(CONFIG_BCM94908) || \
      defined(_BCM963158_) || defined(CONFIG_BCM963158))
#define ETHSW_QPHY_CTRL_IDDQ_GLOBAL_PWR_MASK 0
#define ETHSW_SPHY_CTRL_IDDQ_GLOBAL_PWR_MASK 0
#endif
static void qphy_ctrl_adjust(uint32_t ext_pwr_down)
{
    uint32_t phy_ctrl;

    if (ext_pwr_down == 0)
        return;

    phy_ctrl = ETHSW_REG->qphy_ctrl;
    phy_ctrl |= ext_pwr_down << ETHSW_QPHY_CTRL_EXT_PWR_DOWN_SHIFT;
    if (ext_pwr_down == 0xf)
        phy_ctrl |= ETHSW_QPHY_CTRL_CK25_DIS_MASK |
                    ETHSW_QPHY_CTRL_IDDQ_GLOBAL_PWR_MASK |
                    ETHSW_QPHY_CTRL_IDDQ_BIAS_MASK;
    ETHSW_REG->qphy_ctrl = phy_ctrl;
    printk("Adjusted SF2 QGPHY: qphy_ctrl=0x%08x ext_pwr_down=0x%x\n",
                phy_ctrl, ext_pwr_down);
}

static void sphy_ctrl_adjust(uint32_t ext_pwr_down)
{
    uint32_t phy_ctrl;

    if (ext_pwr_down == 0)
        return;

    phy_ctrl = ETHSW_REG->sphy_ctrl;
    phy_ctrl |= ETHSW_SPHY_CTRL_CK25_DIS_MASK |
                ETHSW_SPHY_CTRL_IDDQ_GLOBAL_PWR_MASK |
                ETHSW_SPHY_CTRL_IDDQ_BIAS_MASK |
                ETHSW_SPHY_CTRL_EXT_PWR_DOWN_MASK;
    ETHSW_REG->sphy_ctrl = phy_ctrl;
    printk("Adjusted SF2 SGPHY: sphy_ctrl=0x%08x\n", phy_ctrl);
}

static unsigned short gphy_base_addr(void)
{
    static unsigned short base_addr;

    if (!base_addr)
        BpGetGphyBaseAddress(&base_addr);

    return base_addr;
}

static uint32_t enabled_phys;

static int _phy_dev_add(phy_dev_t *phy_dev)
{
    unsigned short ba = gphy_base_addr();

    if (ba == BP_NOT_DEFINED)
        return -1;

    enabled_phys |= 1 << (phy_dev->addr - ba);
    return 0;
}

static int _phy_drv_init(phy_drv_t *phy_drv)
{
    qphy_ctrl_adjust(~enabled_phys & 0x0f);
    sphy_ctrl_adjust(~enabled_phys & 0x10);

    phy_drv->initialized = 1;
    return 0;
}

phy_drv_t phy_drv_sf2_gphy =
{
    .phy_type = PHY_TYPE_SF2_GPHY,
    .name = "GPHY",

    .power_set = mii_power_set,
    .apd_set = brcm_egphy_apd_set,
    .eee_set = brcm_egphy_eee_set,
    .eee_get = brcm_egphy_eee_get,
    .read_status = brcm_read_status,
    .speed_set = mii_speed_set,
    .caps_get = mii_caps_get,
    .caps_set = mii_caps_set,
    .phyid_get = mii_phyid_get,
    .isolate_phy = phy_isolate_phy,

    .read = brcm_egphy_read,
    .write = brcm_egphy_write,

    .init = _phy_init,

    .dev_add = _phy_dev_add,
    .drv_init = _phy_drv_init,
};

phy_drv_t phy_drv_sf2_serdes =
{
    .init = sf2_serdes_init,
    .phy_type = PHY_TYPE_SF2_SERDES,
    .read_status = sf2_serdes_phy_read_status,
    .apd_get = ethsw_serdes_power_mode_get,
    .apd_set = ethsw_serdes_power_mode_set,
    .speed_set = ethsw_serdes_cfg_speed_set,
    .caps_set = ethsw_serdes_caps_set,
    .caps_get = ethsw_serdes_caps_get,
    .isolate_phy = phy_isolate_phy,
    .config_speed_get = ethsw_serdes_speed_get,

    .name = "SERDES",
};

