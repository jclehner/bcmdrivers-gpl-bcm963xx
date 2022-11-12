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
 *  Created on: Nov/2015
 *      Author: ido@broadcom.com
 */

#ifndef _RUNNER_H_
#define _RUNNER_H_

#include "port.h"
#include "enet.h"
#include <rdpa_api.h>

extern rdpa_system_init_cfg_t init_cfg;
extern int wan_port_id;

extern sw_ops_t port_runner_sw;
extern port_ops_t port_runner_port;
#ifdef EPON
extern port_ops_t port_runner_epon;
#endif
#ifdef GPON
extern port_ops_t port_runner_gpon;
#endif

/* common port_runner_*() definitions */
int port_runner_pause_get(enetx_port_t *self, int *rx_enable, int *tx_enable);
int port_runner_pause_set(enetx_port_t *self, int rx_enable, int tx_enable);
void port_runner_port_stats_get(enetx_port_t *self, struct rtnl_link_stats64 *net_stats);
int port_runner_port_uninit(enetx_port_t *self);

int port_runner_sw_init(enetx_port_t *self);
int port_runner_sw_uninit(enetx_port_t *self);
int port_runner_sw_config_trunk(enetx_port_t *sw, enetx_port_t *port, int grp_no, int add);


/* runner API implemented by rdp_ring/enet_ring */
extern int runner_ring_create_delete(enetx_channel *chan, int q_id, int size, rdpa_cpu_rxq_cfg_t *rxq_cfg);
extern int runner_get_pkt_from_ring(int hw_q_id, rdpa_cpu_rx_info_t *info);

extern int configure_bc_rate_limit_meter(int port_id, unsigned int rate_limit);

static inline bdmf_object_handle _port_rdpa_object_by_port(enetx_port_t *port)
{
    if (!port)
        return NULL;

    switch (port->port_type)
    {
    case PORT_TYPE_RUNNER_PORT:
    case PORT_TYPE_G9991_PORT:
    case PORT_TYPE_RUNNER_GPON:
    case PORT_TYPE_RUNNER_EPON:
    case PORT_TYPE_SF2_PORT:
        return (bdmf_object_handle)port->priv;
    default:
        return NULL;
    }
}

static inline int _port_rdpa_if_by_port(enetx_port_t *port, rdpa_if *index)
{
    bdmf_object_handle port_obj = _port_rdpa_object_by_port(port);

    if (port_obj)
        return rdpa_port_index_get(port_obj, index);

    return -1;
}

// based on enet\shared\bcmenet_common.h
#define BCM_ENET_IFG        20 /* bytes */
#define BCM_ENET_CRC_LEN    4  /* bytes */
#define BCM_ENET_OVERHEAD   (BCM_ENET_CRC_LEN + BCM_ENET_IFG) /* bytes */

#if defined(DSL_RUNNER_DEVICE)
void link_change_handler(enetx_port_t *port, int linkstatus, int speed, int duplex);

/*
    This is general guide of return code across
    Ethernet hardware dependent API. The exact meaning
    of code set depend on each API local context.
*/

#define SW_PORT_M   (0x07)
#define SW_PORT_S   (0)
#define SW_UNIT_M   (0x08)
#define SW_UNIT_S   (3)

#define LOGICAL_PORT_TO_UNIT_NUMBER(port)   ( ((port) & SW_UNIT_M) >> SW_UNIT_S  )
#define LOGICAL_PORT_TO_PHYSICAL_PORT(port) ( (port) & SW_PORT_M )
#define PHYSICAL_PORT_TO_LOGICAL_PORT(port, unit) ( (port) | ((unit << SW_UNIT_S) & SW_UNIT_M) )

#endif /* defined(DSL_RUNNER_DEVICE) */
#endif

