/*
<:copyright-BRCM:2016:DUAL/GPL:standard

   Copyright (c) 2016 Broadcom 
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


#ifndef TRX_DESCR_GEN_H_INCLUDED
#define TRX_DESCR_GEN_H_INCLUDED

#include <linux/types.h>
#include <bdmf_system.h>
#include <bdmf_dev.h>
#include "opticaldet.h"

#define TRX_EEPROM_OFFSET_TYPE  0
#define TRX_EEPROM_OFFSET_CODE  3
#define TRX_EEPROM_OFFSET_CODE2 36
#define TRX_EEPROM_OFFSET_NAME 20
#define TRX_EEPROM_OFFSET_OUI  37
#define TRX_EEPROM_OFFSET_REV  56
#define TRX_EEPROM_OFFSET_PN   40
#define TRX_EEPROM_LEN_NAME    16
#define TRX_EEPROM_LEN_CODE     8
#define TRX_EEPROM_LEN_OUI      3
#define TRX_EEPROM_LEN_REV      4
#define TRX_EEPROM_LEN_PN      16
#define SOURCEPHOTONICS_FIXUP_PN "SPPS2748FN2CDFA"
#define TRX_EEPROM_CC_10GETH_MASK 0xf0
#define TRX_EEPROM_CC_ETH_MASK    0xff

#define TRX_XFP_EEPROM_PAGE_SELECT   127
#define TRX_XFP_EEPROM_PAGE_1        128
#define TRX_XFP_EEPROM_OFFSET_NAME   148
#define TRX_XFP_EEPROM_OFFSET_PN     168
#define TRX_XFP_EEPROM_LEN_REV       2
#define TRX_XFP_EEPROM_CC_10GETH_MASK 0xff
#define TRX_XFP_EEPROM_CC_ETH_MASK    0xc0

typedef void (*f_activation) (int bus); 
void ltw2601_activation(int bus);

typedef struct
{
    TRX_FORM_FACTOR form_factor;
    TRX_TYPE type;
    uint8_t  vendor_name[TRX_EEPROM_LEN_NAME+1];
    uint8_t  vendor_pn[TRX_EEPROM_LEN_PN+1];
    uint8_t  vendor_rev[TRX_EEPROM_LEN_REV+1];
    TRX_SIG_ACTIVE_POLARITY    lbe_polarity;
    TRX_SIG_ACTIVE_POLARITY    tx_sd_polarity;
    TRX_SIG_ACTIVE_POLARITY    tx_pwr_down_polarity;
    bdmf_boolean               tx_pwr_down_cfg_req;
    TRX_SIG_PRESENCE           tx_sd_supported;
    f_activation               activation_func;
}  TRX_DESCRIPTOR;


/*
 * List of xPON and AE transcievers used by BRCM.
 * To extend/override this list put the entries to trx_usr[] array in ./trx_descr_usr.h
 */

static TRX_DESCRIPTOR trx_lst[] = {
  {
    .form_factor           = TRX_SFP,
    .type                  = TRX_TYPE_XPON,
    .vendor_name           = "SOURCEPHOTONICS",
    .vendor_pn             = "SPPS2748FN2CDFA",
    .lbe_polarity          = TRX_ACTIVE_LOW,
    .tx_sd_polarity        = TRX_ACTIVE_HIGH,
    .tx_pwr_down_polarity  = TRX_ACTIVE_LOW,
    .tx_pwr_down_cfg_req   = BDMF_TRUE,
    .tx_sd_supported       = TRX_SIGNAL_SUPPORTED,
    .activation_func       = (f_activation) NULL
  },
  {
    .form_factor           = TRX_XFP,
    .type                  = TRX_TYPE_XPON,
    .vendor_name           = "Hisense",
    .vendor_pn             = "LTW2601C-BC+",
    .lbe_polarity          = TRX_ACTIVE_LOW,
    .tx_sd_polarity        = TRX_ACTIVE_HIGH,
    .tx_pwr_down_polarity  = TRX_ACTIVE_LOW,
    .tx_pwr_down_cfg_req   = BDMF_FALSE,
    .tx_sd_supported       = TRX_SIGNAL_SUPPORTED,
    .activation_func       = (f_activation) ltw2601_activation
  },
  {
    .form_factor           = TRX_XFP,
    .type                  = TRX_TYPE_XPON,
    .vendor_name           = "Hisense",
    .vendor_pn             = "LTW2601-BC",
    .lbe_polarity          = TRX_ACTIVE_LOW,
    .tx_sd_polarity        = TRX_ACTIVE_HIGH,
    .tx_pwr_down_polarity  = TRX_ACTIVE_LOW,
    .tx_pwr_down_cfg_req   = BDMF_FALSE,
    .tx_sd_supported       = TRX_SIGNAL_SUPPORTED,
    .activation_func       = (f_activation) ltw2601_activation
  },
  {
    .form_factor           = TRX_SFP,
    .type                  = TRX_TYPE_XPON,
    .vendor_name           = "Ligent Photonics",
    .vendor_pn             = "LTF7219-BC",
    .lbe_polarity          = TRX_ACTIVE_LOW,
    .tx_sd_polarity        = TRX_ACTIVE_HIGH,
    .tx_pwr_down_polarity  = TRX_ACTIVE_LOW,
    .tx_pwr_down_cfg_req   = BDMF_FALSE,
    .tx_sd_supported       = TRX_SIGNAL_SUPPORTED,
    .activation_func       = (f_activation) NULL
  },
  {
    .form_factor           = TRX_SFP,
    .type                  = TRX_TYPE_XPON,
    .vendor_name           = "NEOPHOTONICS",
    .vendor_pn             = "PTNEN3-41CP-ST+",
    .lbe_polarity          = TRX_ACTIVE_LOW,
    .tx_sd_polarity        = TRX_ACTIVE_HIGH,
    .tx_pwr_down_polarity  = TRX_ACTIVE_LOW,
    .tx_pwr_down_cfg_req   = BDMF_FALSE,
    .tx_sd_supported       = TRX_SIGNAL_SUPPORTED,
    .activation_func       = (f_activation) NULL
  },
  {
    .form_factor           = TRX_SFF,
    .type                  = TRX_TYPE_XPON,        
    .vendor_name           = "DELTA",
    .vendor_pn             = "OPGP-34-A4B3SN",
    .lbe_polarity          = TRX_ACTIVE_HIGH,
    .tx_sd_polarity        = TRX_ACTIVE_HIGH,
    .tx_pwr_down_polarity  = TRX_ACTIVE_LOW,
    .tx_pwr_down_cfg_req   = BDMF_FALSE,
    .tx_sd_supported       = TRX_SIGNAL_NOT_SUPPORTED,
    .activation_func         = (f_activation) NULL
  },
  {
    .form_factor           = TRX_XFP,
    .type                  = TRX_TYPE_XPON,
    .vendor_name           = "Ligent",
    .vendor_pn             = "LTW2601C-BC",
    .lbe_polarity          = TRX_ACTIVE_LOW,
    .tx_sd_polarity        = TRX_ACTIVE_HIGH,
    .tx_pwr_down_polarity  = TRX_ACTIVE_LOW,
    .tx_pwr_down_cfg_req   = BDMF_FALSE,
    .tx_sd_supported       = TRX_SIGNAL_SUPPORTED,
    .activation_func       = (f_activation) ltw2601_activation
  },
  {
    .form_factor           = TRX_SFP,
    .type                  = TRX_TYPE_XPON,
    .vendor_name           = "Ligent Photonics",
    .vendor_pn             = "LTF7221-BH",
    .lbe_polarity          = TRX_ACTIVE_LOW,
    .tx_sd_polarity        = TRX_ACTIVE_HIGH,
    .tx_pwr_down_polarity  = TRX_ACTIVE_LOW,
    .tx_pwr_down_cfg_req   = BDMF_FALSE,
    .tx_sd_supported       = TRX_SIGNAL_SUPPORTED,
    .activation_func         = (f_activation) NULL
  },
  {
    .form_factor           = TRX_SFP,
    .type                  = TRX_TYPE_XPON,
    .vendor_name           = "Ligent Photonics",
    .vendor_pn             = "LTF7221-BC",
    .lbe_polarity          = TRX_ACTIVE_LOW,
    .tx_sd_polarity        = TRX_ACTIVE_HIGH,
    .tx_pwr_down_polarity  = TRX_ACTIVE_LOW,
    .tx_pwr_down_cfg_req   = BDMF_FALSE,
    .tx_sd_supported       = TRX_SIGNAL_SUPPORTED,
    .activation_func       = (f_activation) NULL
  },
  {
    .form_factor           = TRX_SFP,
    .type                  = TRX_TYPE_XPON,
    .vendor_name           = "Ligent Photonics",
    .vendor_pn             = "LTF7225-BC",
    .lbe_polarity          = TRX_ACTIVE_LOW,
    .tx_sd_polarity        = TRX_ACTIVE_HIGH,
    .tx_pwr_down_polarity  = TRX_ACTIVE_LOW,
    .tx_pwr_down_cfg_req   = BDMF_FALSE,
    .tx_sd_supported       = TRX_SIGNAL_SUPPORTED,
    .activation_func       = (f_activation) NULL
  },
  {
    .form_factor           = TRX_SFP,      
    .type                  = TRX_TYPE_XPON,        
    .vendor_name           = "ZKTEL",
    .vendor_pn             = "ZP5342034-KCST",
    .lbe_polarity          = TRX_ACTIVE_HIGH,
    .tx_sd_polarity        = TRX_ACTIVE_HIGH,
    .tx_pwr_down_polarity  = TRX_ACTIVE_LOW,
    .tx_pwr_down_cfg_req   = BDMF_FALSE,
    .tx_sd_supported       = TRX_SIGNAL_NOT_SUPPORTED,
    .activation_func       = (f_activation) NULL
  },
  {
    .form_factor           = TRX_SFP,      
    .type                  = TRX_TYPE_ETHERNET, 
    .vendor_name           = "FiberStore",
    .vendor_pn             = "SFP-10GSR-85",
  },
  {
    .form_factor           = TRX_SFP,      
    .type                  = TRX_TYPE_ETHERNET, 
    .vendor_name           = "JDSU",
    .vendor_pn             = "PLRXPLSCS4322N",
  },
  {
    .form_factor           = TRX_SFP,      
    .type                  = TRX_TYPE_ETHERNET, 
    .vendor_name           = "FINISAR CORP.",
    .vendor_pn             = "FCLF-8521-3",
  },
  {
    .form_factor           = TRX_SFP,      
    .type                  = TRX_TYPE_ETHERNET, 
    .vendor_name           = "SOURCEPHOTONICS",
    .vendor_pn             = "SPP10ESRCDFF",
  },
  {
    .form_factor           = TRX_SFP,      
    .type                  = TRX_TYPE_XPON,        
    .vendor_name           = "ZKTEL",
    .vendor_pn             = "ZP5342033-HCSY",
    .lbe_polarity          = TRX_ACTIVE_LOW,
    .tx_sd_polarity        = TRX_ACTIVE_HIGH,
    .tx_pwr_down_polarity  = TRX_ACTIVE_LOW,
    .tx_pwr_down_cfg_req   = BDMF_FALSE,
    .tx_sd_supported       = TRX_SIGNAL_NOT_SUPPORTED,
    .activation_func       = (f_activation) NULL
  },
  {
    .form_factor           = TRX_SFF,
    .type                  = TRX_TYPE_XPON,        
    .vendor_name           = "DELTA",
    .vendor_pn             = "OPGP-34-A4B3SV",
    .lbe_polarity          = TRX_ACTIVE_HIGH,
    .tx_sd_polarity        = TRX_ACTIVE_HIGH,
    .tx_pwr_down_polarity  = TRX_ACTIVE_LOW,
    .tx_pwr_down_cfg_req   = BDMF_FALSE,
    .tx_sd_supported       = TRX_SIGNAL_NOT_SUPPORTED,
    .activation_func         = (f_activation) NULL
  },
} ;

#endif /* TRX_DESCR_GEN_H_INCLUDED */
