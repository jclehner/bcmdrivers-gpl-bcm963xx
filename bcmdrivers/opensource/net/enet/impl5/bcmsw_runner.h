/*
    Copyright 2004-2010 Broadcom Corporation

    <:label-BRCM:2011:DUAL/GPL:standard
    
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

#ifndef _BCMSW_RUNNER_H_
#define _BCMSW_RUNNER_H_

#include <bcm/bcmswapitypes.h>
#include <bcm/bcmswapistat.h>
#include "bcmenet_common.h"

#define bcmeapi_ioctl_ethsw_control(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_prio_control(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_vlan(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_pmdioaccess(dev, e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_cosq_txchannel_mapping(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_cosq_sched(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_spiaccess(bus_num, spi_id, chip_id, e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_cosq_config(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_port_jumbo_control(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_port_default_tag_config(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_clear_stats(portmap) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_cosq_txq_sel(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_counter_get(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_cosq_rxchannel_mapping(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_info(dev, e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_regaccess(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_pbvlan(e) -(EOPNOTSUPP)

#define bcmeapi_ioctl_debug_conf(e)
#define bcmeapi_ethsw_dump_page(page) {}
#define bcmeapi_ioctl_ethsw_cosq_port_mapping(e) -(EOPNOTSUPP)
#define enet_arl_read( mac, vid, val ) FALSE
#define enet_arl_write(mac, vid, val) {}
#define enet_arl_dump() {}  /* This should return status actually ?? */
#define enet_arl_dump_multiport_arl() {}
#define fast_age_all(age_static) {}
void bcmeapi_ethsw_init_hw(int unit, uint32_t portMap,  int wanPortMap);

#define bcmsw_spi_rreg(bus_num, spi_ss, chip_id, page, reg, data, len) {}
#define bcmsw_spi_wreg(bus_num, spi_ss, chip_id, page, reg, data, len) {}
static inline int enet_arl_remove(char *mac) {return 0;}
#define bcmeapi_set_multiport_address(addr) {}
int bcmeapi_init_ext_sw_if(extsw_info_t *extSwInfo);
int bcmeapi_ioctl_ethsw_clear_port_emac(struct ethswctl_data *e);
int bcmeapi_ioctl_ethsw_get_port_emac(struct ethswctl_data *e);
int bcmeapi_ioctl_ethsw_clear_port_stats(struct ethswctl_data *e);
int bcmeapi_ioctl_ethsw_get_port_stats(struct ethswctl_data *e);
#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM94908)
int bcmeapi_ioctl_que_map(BcmEnet_devctrl *pDevCtrl, struct ethswctl_data *e);
int bcmeapi_ioctl_que_mon(BcmEnet_devctrl *pDevCtrl, struct ethswctl_data *e);
int bcmeapi_ioctl_ethsw_port_pause_capability(struct ethswctl_data *e);
int bcmeapi_ioctl_ethsw_phy_mode(struct ethswctl_data *e, int phy_id);
int bcmeapi_ioctl_ethsw_combophy_mode(struct ethswctl_data *e, int phy_id, int phy_id_ext);
int bcmsw_unimac_rxtx_op(int port, int get, int *disable);
int bcmeapi_ioctl_ethsw_port_irc_get(struct ethswctl_data *e);
int bcmeapi_ioctl_ethsw_port_irc_set(struct ethswctl_data *e);
#define bcmeapi_ioctl_ethsw_port_erc_get(ethswctl_data) (-EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_port_erc_set(ethswctl_data) (-EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_port_loopback(e, phy_id) (-EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_cpu_meter_set(e) (-EOPNOTSUPP)
#else
#define bcmeapi_ioctl_que_map(pDevCtrl, e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_que_mon(pDevCtrl, e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_port_pause_capability(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_port_traffic_control(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_port_loopback(e, phy_id) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_port_irc_set(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_port_irc_get(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_cpu_meter_set(e) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_phy_mode(e, phy_id) -(EOPNOTSUPP)
#define bcmeapi_ioctl_ethsw_port_erc_set(e) -(EOPNOTSUPP)
#endif
#define bcmeapi_link_check(unit, link, speed) {}
int bcmeapi_ethsw_dump_mib(int port, int type, int queue);
int ethsw_get_hw_stats(int port, struct rtnl_link_stats64 *stats);
int bcmeapi_ioctl_ethsw_sal_dal_set (struct ethswctl_data *e);
void extsw_rreg(int page, int reg, uint8 *data, int len);
void extsw_wreg(int page, int reg, uint8 *data_in, int len);
int bcmeapi_ioctl_ethsw_mtu_set (struct ethswctl_data *e);
int ethsw_set_mac_hw(int port, PHY_STAT ps);
int bcmeapi_ioctl_ethsw_ifname_to_rdpaif(struct ethswctl_data *e);
void bcmeapi_ethsw_set_stp_mode(unsigned int unit, unsigned int port, unsigned char stpState);
#define ethsw_get_pbvlan(port) 0
#define ethsw_set_pbvlan(port, fwdMap) {}

int bcm_enet_rdp_config_bond(int add, int grp_no, int unit, uint16_t port);

#endif /* _BCMSW_RUNNER_H_ */
