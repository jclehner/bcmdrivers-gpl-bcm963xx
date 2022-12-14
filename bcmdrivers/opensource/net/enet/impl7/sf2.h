/*
   <:copyright-BRCM:2015:DUAL/GPL:standard
   
      Copyright (c) 2015 Broadcom 
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
 *  Created on: May/2017
 *      Author: steven.hsieh@broadcom.com
 */

#ifndef _SF2_H_
#define _SF2_H_

#include "port.h"
#include "enet.h"

extern enetx_port_t *sf2_sw;   /* external SF2 switch */
#define PORT_ON_EXTERNAL_SW(port) ((port)->p.parent_sw == sf2_sw)

extern sw_ops_t port_sf2_sw;
extern port_ops_t port_sf2_port;

int port_sf2_sw_demux(enetx_port_t *sw, enetx_rx_info_t *rx_info, FkBuff_t *fkb, enetx_port_t **out_port);
int port_sf2_port_init(enetx_port_t *self);
void port_sf2_print_status(enetx_port_t *p);

void ioctl_extsw_dump_page0(void);
int ioctl_pwrmngt_get_deepgreenmode(int mode);
int ioctl_pwrmngt_set_deepgreenmode(int enable);
int ioctl_extsw_info(struct ethswctl_data *e);
int ioctl_extsw_arl_access(struct ethswctl_data *e);
int ioctl_extsw_regaccess(struct ethswctl_data *e, enetx_port_t *port);
int ioctl_extsw_cfg_acb(struct ethswctl_data *e);
int ioctl_extsw_port_mirror_ops(struct ethswctl_data *e);
int ioctl_extsw_port_trunk_ops(struct ethswctl_data *e);
int ioctl_extsw_que_map(struct ethswctl_data *e);
int ioctl_extsw_port_storm_ctrl(struct ethswctl_data *e);
int ioctl_extsw_cosq_port_mapping(struct ethswctl_data *e);
int ioctl_extsw_control(struct ethswctl_data *e);
int ioctl_extsw_pcp_to_priority_mapping(struct ethswctl_data *e);
int ioctl_extsw_pid_to_priority_mapping(struct ethswctl_data *e);
int ioctl_extsw_set_multiport_address(uint8_t *addr);
int ioctl_extsw_que_mon(struct ethswctl_data *e);
int ioctl_extsw_prio_control(struct ethswctl_data *e);
int ioctl_extsw_port_jumbo_control(struct ethswctl_data *e);
int ioctl_extsw_pbvlan(struct ethswctl_data *e);
int ioctl_extsw_dscp_to_priority_mapping(struct ethswctl_data *e);
int ioctl_extsw_dos_ctrl(struct ethswctl_data *e);
int ioctl_extsw_cos_priority_method_cfg(struct ethswctl_data *e);
int ioctl_extsw_cosq_sched(enetx_port_t *self, struct ethswctl_data *e);
int ioctl_extsw_port_erc_config(struct ethswctl_data *e);
int ioctl_extsw_port_irc_get(struct ethswctl_data *e);
int ioctl_extsw_port_irc_set(struct ethswctl_data *e);
int ioctl_extsw_port_shaper_config(struct ethswctl_data *e);
int ioctl_extsw_cfp(struct ethswctl_data *e);    /* in sf2_cfp.c */
void extsw_set_mac_address(uint8_t *addr);


#define IS_PHY_ADDR_FLAG 0x80000000
#define IS_SPI_ACCESS    0x40000000

#define PORT_ID_M 0xF
#define PORT_ID_S 0
#define PHY_REG_M 0x1F
#define PHY_REG_S 4

#endif

