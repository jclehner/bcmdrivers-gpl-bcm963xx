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
#ifndef ASTRA_TEST_MSG_H
#define ASTRA_TEST_MSG_H

typedef struct astra_test_msg_hdr {
    uint8_t  ucType;                    /* msg type */
    uint8_t  ucSeq;                     /* msg sequence number */
    uint8_t  ucUnused[2];
} astra_test_msg_hdr;

#define ASTRA_TEST_MSG_PAYLOAD(pHdr)    ((uint8_t *)pHdr + sizeof(astra_test_msg_hdr))

enum
{
    ASTRA_TEST_MSG_START = 0,           /* unused */
    ASTRA_TEST_MSG_HELLO,               /* hello msg, to astra_tapp */
    ASTRA_TEST_MSG_MEM_ALLOC,           /* mem alloc msg, to astra_tapp */
    ASTRA_TEST_MSG_MAP_PADDR,           /* map paddr msg, to astra_tapp */
    ASTRA_TEST_MSG_MAP_PADDRS,          /* map paddr msg, to astra_tapp */
    ASTRA_TEST_MSG_LAST
};

struct astra_test_msg_echo
{
    uint32_t value;
};

struct astra_test_msg_hello_cmd
{
    char greet[64];
};

struct astra_test_msg_hello_rpy
{
    char reply[64];
};

struct astra_test_msg_mem_alloc_cmd
{
    uint32_t offset;
    uint32_t size;
};

struct astra_test_msg_mem_alloc_rpy
{
    uint32_t checksum;
};

struct astra_test_msg_map_paddr_cmd
{
    uint32_t paddr;
    uint32_t size;
    uint32_t flags;
};

struct astra_test_msg_map_paddr_rpy
{
    uint32_t checksum;
};

struct astra_test_msg_map_paddrs_cmd
{
    uint32_t count;
    uint32_t paddrs[32];
    uint32_t sizes[32];
    uint32_t flags[32];
};

struct astra_test_msg_map_paddrs_rpy
{
    uint32_t checksum;
};

#endif /* ASTRA_TEST_MSG_H */
