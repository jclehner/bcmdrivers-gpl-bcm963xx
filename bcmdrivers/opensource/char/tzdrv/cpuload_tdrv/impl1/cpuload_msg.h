/***************************************************************************
 * <:copyright-BRCM:2016:DUAL/GPL:standard
 * 
 *    Copyright (c) 2016 Broadcom 
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
 ***************************************************************************/
#ifndef CPU_LOAD_MSG_H
#define CPU_LOAD_MSG_H

/* echo uses invalid client id of 255 */
#define TZIOC_CLIENT_ID_ECHO            255

/* hello string (greet/reply) max */
#define CPU_LOAD_MSG_HELLO_MAX        64

enum
{
    CPU_LOAD_MSG_START = 0,           /* unused */
    CPU_LOAD_MSG_ECHO,                /* echo msg */
    CPU_LOAD_MSG_HELLO,               /* hello msg */
    CPU_LOAD_MSG_STOP,                /* stop msg */
    CPU_LOAD_MSG_LAST
};

struct cpu_load_msg_echo
{
    uint32_t value;
};

struct cpu_load_msg_hello_cmd
{
    char greet[CPU_LOAD_MSG_HELLO_MAX];
};

struct cpu_load_msg_hello_rpy
{
    char reply[CPU_LOAD_MSG_HELLO_MAX];
};

struct cpu_load_msg_stop
{
    uint32_t value;
};

#endif /* CPU_LOAD_MSG_H */
