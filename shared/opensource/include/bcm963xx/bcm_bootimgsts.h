/*
<:copyright-BRCM:2017:DUAL/GPL:standard 

   Copyright (c) 2017 Broadcom 
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

/***************************************************************************/
/*                                                                         */
/*   MODULE:  bcm_bootimgsts.h                                             */
/*   PURPOSE: Define bits which survive a board reset but not power cycle. */
/*                                                                         */
/***************************************************************************/
#ifndef _BCM_BOOTIMGSTS_H
#define _BCM_BOOTIMGSTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bcm_map_part.h>

#if defined(_BCM963158_) || defined(CONFIG_BCM963158) || defined(_BCM96858_) || defined(CONFIG_BCM96858) || defined(_BCM96848_) || defined(CONFIG_BCM96848) || defined(_BCM96838_) || defined(CONFIG_BCM96838)
#define SET_NON_VOLATILE_REG HS_SPI->hs_spiGlobalCtrl
#define SET_NON_VOLATILE_MASK 0x400000
#define BOOT_INACTIVE_IMAGE_ONCE_REG HS_SPI_PROFILES[7].polling_and_mask
#define BOOT_INACTIVE_IMAGE_ONCE_MASK 0x80000000
#endif

#ifdef __cplusplus
}
#endif

#endif /* _BCM_BOOTIMGSTS_H */

