/*
 * <:copyright-BRCM:2017:DUAL/GPL:standard 
 * 
 *    Copyright (c) 2017 Broadcom 
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <linux/printk.h>
#include <net/netfilter/nf_conntrack.h>

#include <linux/blog.h>
#include <bcmdpi.h>

#include "dpi_local.h"

#define DPI_PARENTAL_MAX	32

/* ----- local variables ----- */
static DpictlParentalConfig_t parental[DPI_PARENTAL_MAX] = { 0 };


/* ----- local functions ----- */
static DpictlParentalConfig_t *dpi_find_parental(int app_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(parental); i++) {
		if (parental[i].app_id == app_id)
			return &parental[i];
	}

	return NULL;
}

int dpi_add_parental(DpictlParentalConfig_t *cfg)
{
	DpictlParentalConfig_t *entry = dpi_find_parental(cfg->app_id);

	if (entry) {
		pr_info("parental entry exists\n");
		return 0;
	}

	/* find first free entry in parental table */
	entry = dpi_find_parental(DPI_APPID_INVALID);
	if (!entry) {
		pr_err("no space for new parental control entry\n");
		return -1;
	}

	entry->app_id = cfg->app_id;
#if defined(CONFIG_BCM_KF_BLOG)
	blog_dm(DPI_PARENTAL, cfg->app_id, 0);
#endif
	return 0;
}

int dpi_del_parental(DpictlParentalConfig_t *cfg)
{
	DpictlParentalConfig_t *entry = dpi_find_parental(cfg->app_id);

	if (!entry)
		pr_info("parental entry doesn't exist\n");
	else
		entry->app_id = DPI_APPID_INVALID;

	return 0;
}

uint32_t dpi_parental_filter(struct sk_buff *skb)
{
	struct dpi_app *app = dpi_info(skb).app;

	if (!app)
		return NF_ACCEPT;
	/* if app exists in parental control table, drop by default */
	if (dpi_find_parental(app->app_id))
		return NF_DROP;

	return NF_ACCEPT;
}
