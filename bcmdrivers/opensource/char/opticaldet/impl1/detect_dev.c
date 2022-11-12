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

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/i2c.h>
#include <linux/slab.h>  /* kzalloc() */
#include <linux/types.h>
#include <bcmsfp_i2c.h>
#include "opticaldet.h"
#include <bcm_intr.h>
#include <boardparms.h>
#include <linux/interrupt.h>
#include <board.h>
#include "detect_dev_trx_data.h"

static int i2c_sfp_cb(struct notifier_block *nb, unsigned long action, void *data);
static struct notifier_block i2c_sfp_nb = {
    .notifier_call = i2c_sfp_cb,
};

static int _file_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int _file_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long _detect_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
#ifdef CONFIG_BCM96838
    unsigned long *val = (unsigned long *)arg;
#endif

    switch (cmd)
    {
#ifdef CONFIG_BCM96838
        case OPTICALDET_IOCTL_DETECT:
            *val = opticaldetect();
            break;
        case OPTICALDET_IOCTL_SD:
            *val = signalDetect();
            break;
#endif
        default:
            printk("%s: ERROR: No such IOCTL", __FILE__);
            return -1;
    }

    return 0;
}

static const struct file_operations detect_file_ops =
{
    .owner = THIS_MODULE,
    .open = _file_open,
    .release = _file_release,
    .unlocked_ioctl = _detect_ioctl,
#if defined(CONFIG_COMPAT)
    .compat_ioctl = _detect_ioctl,
#endif
};

static void trx_init(int bus) 
{
    if( bus >= MAX_I2C_BUS )
        return;

    if( i2c_read_trx_data(bus) == 0 ) {
        trx_fixup(bus);
        trx_activate (bus);
    }
    return;
}

static void trx_deinit(int bus) 
{
    if( bus >= MAX_I2C_BUS )
        return;

    i2c_clear_trx_data(bus);
    return;
}

static int i2c_sfp_cb(struct notifier_block *nb, unsigned long action, void *data)
{
    int bus;

    bus = *((int*)data);
    if( action == SFP_STATUS_INSERTED )
        trx_init(bus);
    else 
        trx_deinit(bus);

    return NOTIFY_OK;
}

int __init detect_init(void)
{
    int ret, i;

    bcm_i2c_sfp_register_notifier(&i2c_sfp_nb);

    for( i = 0; i < MAX_I2C_BUS; i++ ) {
        if ( bcm_i2c_sfp_get_status(i) == SFP_STATUS_INSERTED ) {
            trx_init(i);
        }
    }

    ret = register_chrdev(DEV_MAJOR, DEV_CLASS, &detect_file_ops);
    pr_info(KERN_ALERT "Optical detection module %s.\n", ret ?
        "failed to load" : "loaded");

    return ret;
}
module_init(detect_init);

static void __exit detect_exit(void)
{
    bcm_i2c_sfp_unregister_notifier(&i2c_sfp_nb);

    unregister_chrdev(DEV_MAJOR, DEV_CLASS);
}
module_exit(detect_exit);

MODULE_AUTHOR("Broadcom");
MODULE_DESCRIPTION("Optical WAN detect driver");
MODULE_LICENSE("GPL");

