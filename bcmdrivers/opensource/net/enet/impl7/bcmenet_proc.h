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
 *      Author: dima.mamut@broadcom.com
 */

#ifndef _BCMENET_PROC_H_
#define _BCMENET_PROC_H_

int debug_mode_set(int argc, char *argv[]);
int debug_mode_get_pps(int argc, char *argv[]);
int create_debug_mode_proc(void);

extern uint32_t g_debug_mode;
#ifdef CONFIG_ARM64
extern uint64_t g_debug_mode_pckt_rx;
#else
extern uint32_t g_debug_mode_pckt_rx;
#endif
extern struct timeval g_start_time;
extern struct timeval g_end_time;

#endif//_BCMENET_PROC_H_

