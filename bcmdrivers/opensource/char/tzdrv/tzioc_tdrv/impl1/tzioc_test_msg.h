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
#ifndef TZIOC_TEST_MSG_H
#define TZIOC_TEST_MSG_H

/* echo uses invalid client id of 255 */
#define TZIOC_CLIENT_ID_ECHO            255

/* hello string (greet/reply) max */
#define TZIOC_TEST_MSG_HELLO_MAX        64

enum
{
    TZIOC_TEST_MSG_START = 0,           /* unused */
    TZIOC_TEST_MSG_ECHO,                /* echo msg, to echo client */
    TZIOC_TEST_MSG_HELLO,               /* hello msg, to tzioc_tapp */
    TZIOC_TEST_MSG_MEM_ALLOC,           /* mem alloc msg, to tzioc_tapp */
    TZIOC_TEST_MSG_MAP_PADDR,           /* map paddr msg, to tzioc_tapp */
    TZIOC_TEST_MSG_MAP_PADDRS,          /* map paddr msg, to tzioc_tapp */
    TZIOC_TEST_MSG_LAST
};

struct tzioc_test_msg_echo
{
    uint32_t value;
};

struct tzioc_test_msg_hello_cmd
{
    char greet[TZIOC_TEST_MSG_HELLO_MAX];
};

struct tzioc_test_msg_hello_rpy
{
    char reply[TZIOC_TEST_MSG_HELLO_MAX];
};

struct tzioc_test_msg_mem_alloc_cmd
{
    uint32_t offset;
    uint32_t size;
};

struct tzioc_test_msg_mem_alloc_rpy
{
    uint32_t checksum;
};

struct tzioc_test_msg_map_paddr_cmd
{
    uint32_t paddr;
    uint32_t size;
    uint32_t flags;
};

struct tzioc_test_msg_map_paddr_rpy
{
    uint32_t checksum;
};

struct tzioc_test_msg_map_paddrs_cmd
{
    uint32_t count;
    uint32_t paddrs[32];
    uint32_t sizes[32];
    uint32_t flags[32];
};

struct tzioc_test_msg_map_paddrs_rpy
{
    uint32_t checksum;
};

#endif /* TZIOC_TEST_MSG_H */
