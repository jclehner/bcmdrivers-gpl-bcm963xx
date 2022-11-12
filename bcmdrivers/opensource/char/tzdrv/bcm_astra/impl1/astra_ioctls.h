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

#ifndef ASTRA_IOCTLS_H
#define ASTRA_IOCTLS_H

#ifdef __KERNEL__

/* kernel space */
#include <linux/types.h>
#include <asm/ioctl.h>

#include "astra_api.h"

typedef astra_client_handle astra_kclient_handle;
typedef astra_uapp_handle astra_kuapp_handle;
typedef astra_peer_handle astra_kpeer_handle;
typedef astra_file_handle astra_kfile_handle;

#else

/* user space */
#include <stdint.h>
#include <asm/ioctl.h>

#include "libastra_api.h"

typedef void *astra_kclient_handle;
typedef void *astra_kuapp_handle;
typedef void *astra_kpeer_handle;
typedef void *astra_kfile_handle;
#endif

#define ASTRA_IOCTL_MAGIC       0x94 /* 'A'(0x41) + 'S'(0x53) */

enum {
    ASTRA_IOCTL_FIRST = _IO(ASTRA_IOCTL_MAGIC, 0),

    ASTRA_IOCTL_EVENT_POLL = ASTRA_IOCTL_FIRST,
    ASTRA_IOCTL_EVENT_EXIT,

    ASTRA_IOCTL_VERSION_GET,
    ASTRA_IOCTL_CONFIG_GET,
    ASTRA_IOCTL_STATUS_GET,
    ASTRA_IOCTL_CALL_SMC,

    ASTRA_IOCTL_CLIENT_OPEN,
    ASTRA_IOCTL_CLIENT_CLOSE,

    ASTRA_IOCTL_UAPP_OPEN,
    ASTRA_IOCTL_UAPP_CLOSE,

    ASTRA_IOCTL_PEER_OPEN,
    ASTRA_IOCTL_PEER_CLOSE,

    ASTRA_IOCTL_MSG_SEND,
    ASTRA_IOCTL_MSG_RECEIVE,

    ASTRA_IOCTL_MEM_ALLOC,
    ASTRA_IOCTL_MEM_FREE,

    ASTRA_IOCTL_PMEM_ALLOC,
    ASTRA_IOCTL_PMEM_FREE,

    ASTRA_IOCTL_FILE_OPEN,
    ASTRA_IOCTL_FILE_CLOSE,
    ASTRA_IOCTL_FILE_READ,
    ASTRA_IOCTL_FILE_WRITE,

    ASTRA_IOCTL_UAPP_COREDUMP,

    ASTRA_IOCTL_LAST
};

struct astra_ioctl_event_poll_data {
    int retVal;

    astra_kclient_handle hClient;

    astra_event event;
    char eventData[16];
    size_t eventDataLen;
};

struct astra_ioctl_event_exit_data {
    int retVal;

    astra_kclient_handle hClient;
};

struct astra_ioctl_version_get_data {
    int retVal;

    struct astra_version version;
};

struct astra_ioctl_config_get_data {
    int retVal;

    struct astra_config config;
};

struct astra_ioctl_status_get_data {
    int retVal;

    struct astra_status status;
};

struct astra_ioctl_client_open_data {
    int retVal;

    char name[ASTRA_NAME_LEN_MAX];

    astra_kclient_handle hClient;
};

struct astra_ioctl_client_close_data {
    int retVal;

    astra_kclient_handle hClient;
};

struct astra_ioctl_uapp_open_data {
    int retVal;

    astra_kclient_handle hClient;
    char name[ASTRA_NAME_LEN_MAX];
    char path[ASTRA_PATH_LEN_MAX];

    astra_kuapp_handle hUapp;
};

struct astra_ioctl_uapp_close_data {
    int retVal;

    astra_kuapp_handle hUapp;
};

struct astra_ioctl_peer_open_data {
    int retVal;

    astra_kuapp_handle hUapp;
    char name[ASTRA_NAME_LEN_MAX];

    astra_kpeer_handle hPeer;
};

struct astra_ioctl_peer_close_data {
    int retVal;

    astra_kpeer_handle hPeer;
};

struct astra_ioctl_msg_send_data {
    int retVal;

    astra_kpeer_handle hPeer;
    const void *pMsg;
    size_t msgLen;
};

struct astra_ioctl_msg_receive_data {
    int retVal;

    astra_kclient_handle hClient;
    void *pMsg;
    size_t msgLen;
    int timeout;

    astra_kpeer_handle hPeer;
};

struct astra_ioctl_mem_alloc_data {
    int retVal;

    astra_kclient_handle hClient;
    size_t size;

    uint32_t buffOffset;
};

struct astra_ioctl_mem_free_data {
    int retVal;

    astra_kclient_handle hClient;
    uint32_t buffOffset;
};

struct astra_ioctl_call_smc_data {
    int retVal;

    astra_kclient_handle hClient;
    uint32_t mode;
};

struct astra_ioctl_file_open_data {
    int retVal;

    astra_kclient_handle hClient;
    char path[ASTRA_PATH_LEN_MAX];
    int flags;

    astra_kfile_handle hFile;
};

struct astra_ioctl_file_close_data {
    int retVal;

    astra_kfile_handle hFile;
};

struct astra_ioctl_file_write_data {
    int retVal;

    astra_kfile_handle hFile;
    astra_paddr_t paddr;
    size_t bytes;
};

struct astra_ioctl_file_read_data {
    int retVal;

    astra_kfile_handle hFile;
    astra_paddr_t paddr;
    size_t bytes;
};

struct astra_ioctl_uapp_coredump_data {
    int retVal;

    astra_kuapp_handle hUapp;
    astra_paddr_t paddr;
    size_t bytes;
};

#endif /* ASTRA_IOCTLS_H */
