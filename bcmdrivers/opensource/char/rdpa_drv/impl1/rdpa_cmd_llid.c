
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

/*
 *******************************************************************************
 * File Name  : rdpa_cmd_llid.c
 *
 * Description: 
 *
 *******************************************************************************
 */

#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/bcm_log.h>
#include "bcmenet.h"
#include "bcmtypes.h"
#include "bcmnet.h"
#include "rdpa_types.h"
#include "rdpa_api.h"
#include "rdpa_ag_port.h"
#include "rdpa_drv.h"
#include "rdpa_cmd_llid.h"
#include "rdpa_cmd_misc.h"
#ifdef CONFIG_BCM_PON
#include <rdpa_epon.h>
#include "rdpa_ag_epon.h"
#endif

#define __BDMF_LOG__

#define CMD_LLID_LOG_ID_RDPA_CMD_DRV BCM_LOG_ID_RDPA_CMD_DRV

#if defined(__BDMF_LOG__)
#define CMD_LLID_LOG_ERROR(fmt, args...)                                             \
    do {                                                                           \
        if (bdmf_global_trace_level >= bdmf_trace_level_error)                     \
            bdmf_trace("ERR: %s#%d: " fmt "\n", __FUNCTION__, __LINE__, ## args);  \
    } while(0)
#define CMD_LLID_LOG_INFO(fmt, args...)                                              \
    do {                                                                           \
        if (bdmf_global_trace_level >= bdmf_trace_level_info)                      \
            bdmf_trace("INF: %s#%d: " fmt "\n", __FUNCTION__, __LINE__, ## args);  \
    } while(0)
#define CMD_LLID_LOG_DEBUG(fmt, args...)                                             \
    do {                                                                           \
        if (bdmf_global_trace_level >= bdmf_trace_level_debug)                     \
            bdmf_trace("DBG: %s#%d: " fmt "\n", __FUNCTION__, __LINE__, ## args);  \
    } while(0)
#else
#define CMD_LLID_LOG_ERROR(fmt, arg...) BCM_LOG_ERROR(fmt, arg...)
#define CMD_LLID_LOG_INFO(fmt, arg...)  BCM_LOG_INFO(fmt, arg...)
#define CMD_LLID_LOG_DEBUG(fmt, arg...) BCM_LOG_DEBUG(fmt, arg...)
#endif


/*******************************************************************************/
/* static routines Functions                                                   */
/*******************************************************************************/
#ifdef CONFIG_BCM_PON_XRDP

static Bool epon_mode_get(rdpa_epon_mode *mode)
{
    rdpa_wan_type wan_type;
    bdmf_object_handle epon_obj = NULL;
    Bool ret = FALSE;
    rdpa_if wan_if = rdpa_wan_type_to_if(rdpa_wan_epon);

    get_rdpa_wan_type(wan_if, &wan_type);
    if ((wan_type == rdpa_wan_epon || wan_type == rdpa_wan_xepon) && !is_ae_enable(wan_if))
    {
        if (rdpa_epon_get(&epon_obj))
        {
            CMD_LLID_LOG_ERROR("rdpa_epon_get failed\n");
        }
        else
        {
            if (rdpa_epon_mode_get(epon_obj, mode))
                CMD_LLID_LOG_ERROR("rdpa_epon_mode_get failed\n");
            else
                ret = TRUE;
            bdmf_put(epon_obj);
        }
    }
    return ret;
}

#endif

/*******************************************************************************/
/* global routines                                                             */
/*******************************************************************************/

/*******************************************************************************
 *
 * Function: rdpa_cmd_llid_ioctl
 *
 * IOCTL interface to the RDPA LLID API.
 *
 *******************************************************************************/
int rdpa_cmd_llid_ioctl(unsigned long arg)
{
    rdpa_drv_ioctl_llid_t *userLlid_p = (rdpa_drv_ioctl_llid_t *)arg;
    rdpa_drv_ioctl_llid_t llid;
    bdmf_error_t rc = BDMF_ERR_OK;
    int ret = 0;

    copy_from_user(&llid, userLlid_p, sizeof(rdpa_drv_ioctl_llid_t));

    CMD_LLID_LOG_DEBUG("RDPA LLID CMD(%d)", llid.cmd);

    bdmf_lock();

    switch(llid.cmd)
    {
        case RDPA_IOCTL_LLID_CMD_NEW: 
        {
            rdpa_tm_queue_cfg_t queue_cfg = {};
            bdmf_object_handle tmp_llid = NULL;
            bdmf_object_handle port_obj = NULL;    
            bdmf_object_handle egress_tm = NULL;
            rdpa_port_tm_cfg_t tm_cfg;
    
            BDMF_MATTR(tm_mattrs, rdpa_egress_tm_drv());
            BDMF_MATTR(link_attrs, rdpa_llid_drv());


            if ((rc = rdpa_port_get(rdpa_wan_type_to_if(rdpa_wan_epon), &port_obj)))
            {
                CMD_LLID_LOG_ERROR("rdpa_port_get FAILED: rc(%d)", rc);
                ret = RDPA_DRV_NEW_LLID_ALLOC;
                goto ioctl_exit;
            }
            
            if ((rc = rdpa_port_tm_cfg_get(port_obj, &tm_cfg)))
            {
                CMD_LLID_LOG_ERROR("rdpa_port_tm_cfg_get FAILED rc=%d", rc);
                ret = RDPA_DRV_NEW_LLID_ALLOC;
                goto ioctl_exit;
            }

            if (tm_cfg.sched != NULL)
            {
                /* In case of 6858 HGU DPOE, LLID share TM of WAN port */
                rdpa_llid_control_egress_tm_set(link_attrs, tm_cfg.sched);
            }
            
            rdpa_llid_index_set(link_attrs, llid.llid_index);
            if ((rc = bdmf_new_and_set(rdpa_llid_drv(), NULL, link_attrs, &tmp_llid))) 
            {
                CMD_LLID_LOG_ERROR("bdmf_new_and_set() llid failed: llid(%u) rc(%d)", llid.llid_index, rc);
                ret = RDPA_DRV_NEW_LLID_ALLOC;
                goto ioctl_exit;
            }
            
            if (tm_cfg.sched != NULL && llid.llid_index == 0)
            {
                // Create queue onece
                /* 6858 dpoe control and data share tm,  so need special setting here */
                memset(&queue_cfg, 0, sizeof(queue_cfg));
                queue_cfg.queue_id           = 101;
                queue_cfg.drop_alg           = rdpa_tm_drop_alg_dt;
                /* 96858/96836 */
                queue_cfg.drop_threshold     = 16384; /*  256 * 64  */
                queue_cfg.weight             = 0;
                queue_cfg.high_class.min_threshold = 0; 
                queue_cfg.high_class.max_threshold = 0; 
                queue_cfg.low_class.min_threshold = 0; 
                queue_cfg.low_class.max_threshold = 0; 
    
                if ((rc = rdpa_egress_tm_queue_cfg_set(tm_cfg.sched, 0, &queue_cfg)))
                {
                    CMD_LLID_LOG_ERROR("rdpa_egress_tm_queue_cfg_set() failed: link(%u) rc(%d)", llid.llid_index, rc);
                    ret = RDPA_DRV_Q_CFG_SET;
                    goto ioctl_exit;
                }
            }

            rc = rdpa_llid_control_egress_tm_get(tmp_llid, &egress_tm);
            
            if (rc == 0 && egress_tm == NULL)
            {
#ifdef CONFIG_BCM_PON_XRDP
                rdpa_epon_mode epon_mode = rdpa_epon_none;
#endif

                rdpa_egress_tm_dir_set(tm_mattrs, rdpa_dir_us);
                rdpa_egress_tm_level_set(tm_mattrs, rdpa_tm_level_queue); 
                rdpa_egress_tm_mode_set(tm_mattrs, rdpa_tm_sched_sp);
                 
#ifdef CONFIG_BCM_PON_XRDP
                /* 6858 dpoe control and data share tm,  so need special setting here */
                epon_mode_get(&epon_mode);

                if (epon_mode == rdpa_epon_dpoe)
                { 
                    rdpa_egress_tm_mode_set(tm_mattrs, rdpa_tm_sched_sp_wrr);
#if defined(CONFIG_BCM96846)
                    rdpa_egress_tm_num_queues_set(tm_mattrs, 8);
                    rdpa_egress_tm_num_sp_elements_set(tm_mattrs, 8);
#else
                    rdpa_egress_tm_num_queues_set(tm_mattrs, 32);
                    rdpa_egress_tm_num_sp_elements_set(tm_mattrs, 16);
#endif
                    rdpa_egress_tm_rl_rate_mode_set(tm_mattrs, rdpa_tm_rl_dual_rate);
                }
#endif
                if ((rc = bdmf_new_and_set(rdpa_egress_tm_drv(), NULL, tm_mattrs, &egress_tm)))
                {
                    CMD_LLID_LOG_ERROR("bdmf_new_and_set() tm failed: link(%u) rc(%d)", llid.llid_index, rc);
                    ret = RDPA_DRV_NEW_TM_ALLOC;
                    goto ioctl_exit;
                }
                
                memset(&queue_cfg, 0, sizeof(queue_cfg));
                queue_cfg.queue_id           = 101 + llid.llid_index;
                queue_cfg.drop_alg           = rdpa_tm_drop_alg_dt;
#ifdef CONFIG_BCM_PON_XRDP
                queue_cfg.drop_threshold     = 16384;/*  256 * 64  */
#else
                queue_cfg.drop_threshold     = 256;
#endif
                queue_cfg.weight             = 0;
                queue_cfg.high_class.min_threshold = 0; 
                queue_cfg.high_class.max_threshold = 0; 
                queue_cfg.low_class.min_threshold = 0; 
                queue_cfg.low_class.max_threshold = 0; 

                if ((rc = rdpa_egress_tm_queue_cfg_set(egress_tm, 0, &queue_cfg)))
                {
                    CMD_LLID_LOG_ERROR("rdpa_egress_tm_queue_cfg_set() failed: link(%u) rc(%d)", llid.llid_index, rc);
                    ret = RDPA_DRV_Q_CFG_SET;
                    goto ioctl_exit;
                } 
                if ((rc = rdpa_llid_control_egress_tm_set(tmp_llid, egress_tm)))
                {
                    CMD_LLID_LOG_ERROR("rdpa_llid_control_egress_tm_set() failed: link(%u) rc(%d)", llid.llid_index, rc);
                    ret = RDPA_DRV_LLID_CTRL_TM_SET;
                    goto ioctl_exit;
                }
            }

            if((rc = rdpa_llid_control_enable_set(tmp_llid, 1)))
            {
                CMD_LLID_LOG_ERROR("rdpa_llid_control_enable_set() failed: link(%u) rc(%d)", llid.llid_index, rc);
                ret = RDPA_DRV_LLID_CTRL_EN_SET;
                goto ioctl_exit;
            }
            
            break;
        }
        
        default:
        {
            CMD_LLID_LOG_ERROR("Invalid IOCTL cmd %d", llid.cmd);
            ret = RDPA_DRV_ERROR;
            break;
        }
    }

ioctl_exit:

    bdmf_unlock();

    copy_to_user(userLlid_p, &llid, sizeof(rdpa_drv_ioctl_llid_t));

    return ret;
}

/*******************************************************************************
 *
 * Function: rdpa_cmd_llid_init
 *
 * Initializes the RDPA LLID API.
 *
 *******************************************************************************/
void rdpa_cmd_llid_init(void)
{
    CMD_LLID_LOG_DEBUG("RDPA LLID INIT");
}
