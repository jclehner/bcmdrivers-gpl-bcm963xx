/***************************************************************************
 *  <:copyright-BRCM:2016:DUAL/GPL:standard
 *  
 *     Copyright (c) 2016 Broadcom 
 *     All Rights Reserved
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2, as published by
 *  the Free Software Foundation (the "GPL").
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  
 *  A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
 *  writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *  
 * :>
 ***************************************************************************/

#ifndef TZIOC_IOCTLS_H
#define TZIOC_IOCTLS_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif
#include <asm/ioctl.h>

#ifndef TZIOC_CLIENT_DEFINES
#define TZIOC_CLIENT_DEFINES

#define TZIOC_CLIENT_NAME_LEN_MAX       32
#define TZIOC_CLIENT_PATH_LEN_MAX       128

#endif /* TZIOC_CLIENT_DEFINES */

#ifndef TZIOC_MSG_DEFINES
#define TZIOC_MSG_DEFINES

typedef struct tzioc_msg_hdr {
    uint8_t  ucType;                    /* msg type */
    uint8_t  ucOrig;                    /* originating client ID */
    uint8_t  ucDest;                    /* destination client ID */
    uint8_t  ucSeq;                     /* msg sequence number */
    uint32_t ulLen;                     /* byte length of msg payload */
} tzioc_msg_hdr;

#define TZIOC_MSG_PAYLOAD(pHdr)         ((uint8_t *)pHdr + sizeof(tzioc_msg_hdr))

#define TZIOC_MSG_SIZE_MAX              1024
#define TZIOC_MSG_PAYLOAD_MAX           (TZIOC_MSG_SIZE_MAX - \
                                         sizeof(struct tzioc_msg_hdr))
#endif /* TZIOC_MSG_DEFINES */

#define TZIOC_IOCTL_MAGIC               0xae /* 'T'(0x54) + 'Z'(0x5a) */

enum {
    TZIOC_IOCTL_FIRST = _IO(TZIOC_IOCTL_MAGIC, 0),

    TZIOC_IOCTL_CLIENT_OPEN = TZIOC_IOCTL_FIRST,
    TZIOC_IOCTL_CLIENT_CLOSE,
    TZIOC_IOCTL_MSG_SEND,
    TZIOC_IOCTL_MEM_ALLOC,
    TZIOC_IOCTL_MEM_FREE,
    TZIOC_IOCTL_CALL_SMC,

    TZIOC_IOCTL_LAST
};

struct tzioc_ioctl_client_open_data {
    int retVal;

    char name[TZIOC_CLIENT_NAME_LEN_MAX];
    uint32_t msgQ;

    uint32_t hClient;
    uint32_t id;
    uint32_t smemStart;
    uint32_t smemSize;
};

struct tzioc_ioctl_client_close_data {
    int retVal;

    uint32_t hClient;
};

struct tzioc_ioctl_client_getid_data {
    int retVal;

    uint32_t hClient;
    char name[TZIOC_CLIENT_NAME_LEN_MAX];

    uint32_t id;
};

struct tzioc_ioctl_msg_send_data {
    int retVal;

    uint32_t hClient;
    struct tzioc_msg_hdr hdr;
    uint32_t payloadAddr;
};

struct tzioc_ioctl_mem_alloc_data {
    int retVal;

    uint32_t hClient;
    uint32_t size;

    uint32_t buffOffset;
};

struct tzioc_ioctl_mem_free_data {
    int retVal;

    uint32_t hClient;
    uint32_t buffOffset;
};

struct tzioc_ioctl_call_smc_data {
    int retVal;

    uint32_t hClient;
    uint32_t mode;
};

#endif /* TZIOC_IOCTLS_H */
