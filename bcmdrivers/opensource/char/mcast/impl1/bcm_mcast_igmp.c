/*
*    Copyright (c) 2015 Broadcom Corporation
*    All Rights Reserved
* 
<:label-BRCM:2015:DUAL/GPL:standard

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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/etherdevice.h>
#include <linux/jhash.h>

#include "bcm_mcast_priv.h"

/*****
  all functions in this file must be called with the lock already held 
 *****/

int bcm_mcast_igmp_control_filter(__be32 dest_ip)
{
   struct bcm_mcast_control_filter_entry *filter_entry = NULL;

   // The range 224.0.0.x is always blocked
   if ((dest_ip & htonl(0xFFFFFF00)) == htonl(0xE0000000))
   {
      return 0;
   }
   spin_lock_bh(&mcast_ctrl->cfgLock);
   hlist_for_each_entry(filter_entry, &mcast_ctrl->igmp_snoopExceptionList, node)
   {
      if ( (dest_ip & filter_entry->mask.s6_addr32[0]) == (filter_entry->group.s6_addr32[0] & filter_entry->mask.s6_addr32[0]) )
      {
         spin_unlock_bh(&mcast_ctrl->cfgLock);
         return 0;
      }
   }
   spin_unlock_bh(&mcast_ctrl->cfgLock);
   return 1;
} /* bcm_mcast_igmp_control_filter */


void bcm_mcast_igmp_wipe_ignore_group_list ( void )
{
   struct bcm_mcast_control_filter_entry *filter_entry = NULL;
   struct hlist_node *n_group;

   hlist_for_each_entry_safe(filter_entry, n_group, &mcast_ctrl->igmp_snoopExceptionList, node)
   {
      hlist_del(&filter_entry->node);
      kmem_cache_free(mcast_ctrl->ipv4_exception_cache, filter_entry);
   }
}

int bcm_mcast_igmp_process_ignore_group_list (int count, t_BCM_MCAST_IGNORE_GROUP_ENTRY* ignoreGroupEntries)
{
   int inputIndex = 0;
   int ret = 0;
   
   spin_lock_bh(&mcast_ctrl->cfgLock);
   bcm_mcast_igmp_wipe_ignore_group_list();
   
   for ( ; inputIndex < count; inputIndex ++) {
      if ((ignoreGroupEntries[inputIndex].mask.s6_addr32[0] & htonl(0xF0000000)) != htonl(0xF0000000) ) 
      {
         ret = -EINVAL;
      }
      else if ((ignoreGroupEntries[inputIndex].address.s6_addr32[0] & htonl(0xF0000000)) != htonl(0xE0000000) ) 
      {
         ret = -EINVAL;
      }
      else 
      {
         struct bcm_mcast_control_filter_entry *newFilter = kmem_cache_alloc(mcast_ctrl->ipv4_exception_cache, GFP_ATOMIC);
         if (newFilter) 
         {
            newFilter->group.s6_addr32[0] = ignoreGroupEntries[inputIndex].address.s6_addr32[0];
            newFilter->mask.s6_addr32[0] = ignoreGroupEntries[inputIndex].mask.s6_addr32[0];
            hlist_add_head(&newFilter->node, &mcast_ctrl->igmp_snoopExceptionList);
         }
         else 
         {
            ret = -ENOMEM;
         }
      }
   }
   spin_unlock_bh(&mcast_ctrl->cfgLock);

   return ret;
}

static inline int bcm_mcast_igmp_hash(const u32 grp)
{
   return (jhash_1word(grp, mcast_ctrl->ipv4_hash_salt) & (BCM_MCAST_HASH_SIZE - 1));
}

void bcm_mcast_igmp_del_entry(bcm_mcast_ifdata *pif, 
                              t_igmp_grp_entry *igmp_fdb,
                              struct in_addr   *rep,
                              unsigned char    *repMac)
{
   t_igmp_rep_entry *rep_entry = NULL;
   t_igmp_rep_entry *rep_entry_n = NULL;

   list_for_each_entry_safe(rep_entry, 
                            rep_entry_n, &igmp_fdb->rep_list, list) 
   {
      if (((NULL == rep) && (NULL == repMac)) ||
          (rep && (rep_entry->rep.s_addr == rep->s_addr)) ||
          (repMac && (0 == memcmp(rep_entry->repMac, repMac, ETH_ALEN))))
      {
         if ( pif->igmp_snooping )
         {
            bcm_mcast_netlink_send_igmp_purge_entry(pif, igmp_fdb, rep_entry);
         }
         bcm_mcast_notify_event(BCM_MCAST_EVT_SNOOP_DEL, BCM_MCAST_PROTO_IPV4, igmp_fdb, rep_entry);
         list_del(&rep_entry->list);
         kmem_cache_free(mcast_ctrl->ipv4_rep_cache, rep_entry);
         if (rep || repMac)
         {
            break;
         }
      }
   }
   if(list_empty(&igmp_fdb->rep_list)) 
   {
      hlist_del(&igmp_fdb->hlist);
#if defined(CONFIG_BLOG) 
      bcm_mcast_blog_release(BCM_MCAST_PROTO_IPV4, (void *)igmp_fdb);
#endif
      kmem_cache_free(mcast_ctrl->ipv4_grp_cache, igmp_fdb);
   }

   return;
}

static void bcm_mcast_igmp_set_timer( bcm_mcast_ifdata *pif)
{
   t_igmp_grp_entry *mcast_group;
   int               i;
   unsigned long     tstamp = jiffies + (mcast_ctrl->igmp_general_query_timeout_secs * HZ * 2);
   unsigned int      found = 0;
   int               pendingIndex;
   unsigned long     skbTimeout = jiffies + (BCM_MCAST_NETLINK_SKB_TIMEOUT_MS*2);

   found = 0;
   for ( pendingIndex = 0; pendingIndex < BCM_MCAST_MAX_DELAYED_SKB_COUNT; pendingIndex ++)
   {
      if ( (pif->igmp_delayed_skb[pendingIndex].skb != NULL) && 
           (time_after(skbTimeout, pif->igmp_delayed_skb[pendingIndex].expiryTime)) )
      {
         skbTimeout = pif->igmp_delayed_skb[pendingIndex].expiryTime;
         found = 1;
      }
   } 

   if ( (BCM_MCAST_SNOOPING_DISABLED_FLOOD == pif->igmp_snooping) && (0 == found))
   {
      del_timer(&pif->igmp_timer);
      return;
   }

   if (found) 
   {
      if (time_after(tstamp, skbTimeout) )
      {
         tstamp = skbTimeout;
      }
   }

   if ( pif->igmp_snooping != 0 ) 
   {
      for (i = 0; i < BCM_MCAST_HASH_SIZE; i++) 
      {
         hlist_for_each_entry(mcast_group, &pif->mc_ipv4_hash[i], hlist) 
         {
            t_igmp_rep_entry *reporter_group;
            list_for_each_entry(reporter_group, &mcast_group->rep_list, list)
            {
               if ( time_after(tstamp, reporter_group->tstamp) )
               {
                  tstamp = reporter_group->tstamp;
                  found  = 1;
               }
            }
         }
      }
   }
  
   if ( 0 == found )
   {
      del_timer(&pif->igmp_timer);
   }
   else
   {
      mod_timer(&pif->igmp_timer, (tstamp + BCM_MCAST_FDB_TIMER_TIMEOUT));
   }
}

int bcm_mcast_igmp_wipe_group(bcm_mcast_ifdata *parent_if, int dest_ifindex, struct in_addr *gpAddr)
{
   t_igmp_grp_entry *mcast_group;
   int               i;
   int               retVal = -EINVAL;
 
   struct net_device* destDev = dev_get_by_index (&init_net, dest_ifindex);
   if (NULL != destDev)
   {
      spin_lock_bh(&parent_if->mc_igmp_lock);
      retVal = 0;
      for (i = 0; i < BCM_MCAST_HASH_SIZE; i++)
      {
         struct hlist_node *n_group;
         hlist_for_each_entry_safe(mcast_group, n_group, &parent_if->mc_ipv4_hash[i], hlist)
         {
            t_igmp_rep_entry *reporter_group, *n_reporter;
            list_for_each_entry_safe(reporter_group, n_reporter, &mcast_group->rep_list, list)
            {
               if ((mcast_group->rxGrp.s_addr == gpAddr->s_addr) && (mcast_group->dst_dev == destDev))
               {
                  bcm_mcast_igmp_del_entry(parent_if, mcast_group, &reporter_group->rep, NULL);
               }
            }
         }
      }
      bcm_mcast_igmp_set_timer(parent_if);
      spin_unlock_bh(&parent_if->mc_igmp_lock);
      dev_put(destDev);
   }
   return retVal;
}

static void bcm_mcast_igmp_reporter_timeout(bcm_mcast_ifdata *pif)
{
   t_igmp_grp_entry *mcast_group;
   int               i;
    
   for (i = 0; i < BCM_MCAST_HASH_SIZE; i++) 
   {
      struct hlist_node *n_group;
      hlist_for_each_entry_safe(mcast_group, n_group, &pif->mc_ipv4_hash[i], hlist) 
      {
         t_igmp_rep_entry *reporter_group, *n_reporter;
         list_for_each_entry_safe(reporter_group, n_reporter, &mcast_group->rep_list, list)
         {
            if (time_after_eq(jiffies, reporter_group->tstamp)) 
            {
               bcm_mcast_igmp_del_entry(pif, mcast_group, &reporter_group->rep, NULL);
            }
         }
      }
   }

}

static void bcm_mcast_igmp_admission_timeout(bcm_mcast_ifdata *pif)
{
   int pendingIndex;

   for (pendingIndex = 0; pendingIndex < BCM_MCAST_MAX_DELAYED_SKB_COUNT; pendingIndex ++)
   {
      if (( pif->igmp_delayed_skb[pendingIndex].skb != NULL ) && 
          ( time_before(pif->igmp_delayed_skb[pendingIndex].expiryTime, jiffies) ) )
      {
         kfree_skb(pif->igmp_delayed_skb[pendingIndex].skb);
         pif->igmp_delayed_skb[pendingIndex].skb = NULL;
         pif->igmp_delayed_skb[pendingIndex].expiryTime = 0;
      }
   }    
}

static void bcm_mcast_igmp_timer(unsigned long param)
{
   int               ifindex = (int)param;
   bcm_mcast_ifdata *pif;

   rcu_read_lock();
   pif = bcm_mcast_if_lookup(ifindex);
   if ( NULL == pif )
   {
      rcu_read_unlock();
      __logError("unknown interface");
      return;
   }

   spin_lock_bh(&pif->mc_igmp_lock);
   bcm_mcast_igmp_reporter_timeout(pif);
   bcm_mcast_igmp_admission_timeout(pif);
   bcm_mcast_igmp_set_timer(pif);
   spin_unlock_bh(&pif->mc_igmp_lock);
   rcu_read_unlock();
}

void bcm_mcast_igmp_init(bcm_mcast_ifdata *pif)
{
   int i;
   spin_lock_init(&pif->mc_igmp_lock);
   for (i = 0; i < BCM_MCAST_HASH_SIZE; i++ )
   {
      INIT_HLIST_HEAD(&pif->mc_ipv4_hash[i]);
   }

   setup_timer(&pif->igmp_timer, bcm_mcast_igmp_timer, (unsigned long)pif->ifindex);
   pif->igmp_snooping = BCM_MCAST_SNOOPING_BLOCKING_MODE;
   pif->igmp_lan2lan_mc_enable = 0;
   pif->igmp_rate_limit = 0;
}

void bcm_mcast_igmp_fini(bcm_mcast_ifdata *pif)
{
   del_timer_sync(&pif->igmp_timer);
}

int bcm_mcast_igmp_admission_queue(bcm_mcast_ifdata *pif, struct sk_buff *skb)
{
   int q_index;
   int index = -1;
   for (q_index = 0; q_index < BCM_MCAST_MAX_DELAYED_SKB_COUNT; q_index ++) 
   {
      if ( pif->igmp_delayed_skb[q_index].skb == NULL ) 
      {
         pif->igmp_delayed_skb[q_index].skb = skb;
         pif->igmp_delayed_skb[q_index].expiryTime = jiffies + BCM_MCAST_NETLINK_SKB_TIMEOUT_MS;
         index = q_index;
         bcm_mcast_igmp_set_timer(pif);
         break;
      }
   }
   if ( -1 == index )
   {
      __logNotice("unable to queue skb"); 
   }
   return index;
}

void bcm_mcast_igmp_admission_update_bydev(bcm_mcast_ifdata *pif, struct net_device *dev)
{
   int pendingIndex;
   __logDebug("flushing skbs from queue for dev %s", dev ? dev->name : "all");
   for ( pendingIndex = 0; pendingIndex < BCM_MCAST_MAX_DELAYED_SKB_COUNT; pendingIndex++)
   {
      if ( (pif->igmp_delayed_skb[pendingIndex].skb != NULL) &&
           ((NULL == dev) || (pif->igmp_delayed_skb[pendingIndex].skb->dev == dev)) )
      {
         kfree_skb(pif->igmp_delayed_skb[pendingIndex].skb);
         pif->igmp_delayed_skb[pendingIndex].skb = NULL;
         pif->igmp_delayed_skb[pendingIndex].expiryTime = 0;
      }
   }
}

int bcm_mcast_igmp_admission_process(bcm_mcast_ifdata *pif, int packet_index, int admitted)
{
   struct sk_buff *skb;

   if ( (packet_index < 0) || (packet_index >= BCM_MCAST_MAX_DELAYED_SKB_COUNT))
   {
       __logNotice("skb for index %d not set", packet_index);
       return -1;
   }

   skb = pif->igmp_delayed_skb[packet_index].skb;
   if ( skb )
   {
      if (true == admitted)
      {
         /* send the packet on - called with rcu read lock */
         __logNotice("skb for index %d has been admitted", packet_index);
         br_bcm_mcast_flood_forward(pif->dev, skb);
      }
      else
      {
         __logNotice("skb for index %d has been dropped", packet_index);
         kfree_skb(skb);
      }
      pif->igmp_delayed_skb[packet_index].skb = NULL;
   }
   else
   {
      __logNotice("skb for index %d not set", packet_index);
   }

   return 0;
}

void bcm_mcast_igmp_update_bydev( bcm_mcast_ifdata *pif, struct net_device *dev, int activate)
{
   t_igmp_grp_entry *mc_fdb;
   int i;
#if defined(CONFIG_BLOG)
   int ret;
#endif

   __logDebug("flushing entries for dev %s", dev ? dev->name : "all");

   if(NULL == pif)
   {
      return;
   }


   /* remove all entries if dev is NULL or activate is false otherwise 
      deactivate root entries and remove all other entries involving dev */
   for (i = 0; i < BCM_MCAST_HASH_SIZE; i++) 
   {
      struct hlist_node *n;
      hlist_for_each_entry_safe(mc_fdb, n, &pif->mc_ipv4_hash[i], hlist) 
      {
         if (NULL == dev)
         {
            bcm_mcast_igmp_del_entry(pif, mc_fdb, NULL, NULL);
         }
         else if ( (mc_fdb->dst_dev == dev) ||
                   (mc_fdb->from_dev == dev) )
         {
#if defined(CONFIG_BLOG)
            if ((0 == mc_fdb->root) || (0 == activate))
            {
               bcm_mcast_igmp_del_entry(pif, mc_fdb, NULL, NULL);
            }
            else
            {
               bcm_mcast_blog_release(BCM_MCAST_PROTO_IPV4, (void *)mc_fdb);
               mc_fdb->blog_idx.fc.word = BLOG_KEY_FC_INVALID;
               mc_fdb->blog_idx.mc.word = BLOG_KEY_MCAST_INVALID;
            }
#else
            bcm_mcast_igmp_del_entry(pif, mc_fdb, NULL, NULL);
#endif
         }
      }
   }

   /* activate root entries for dev */
#if defined(CONFIG_BLOG)
   if ((dev != NULL) && (1 == activate))
   {
      for (i = 0; i < BCM_MCAST_HASH_SIZE; i++) 
      {
         struct hlist_node *n;
         hlist_for_each_entry_safe(mc_fdb, n, &pif->mc_ipv4_hash[i], hlist) 
         { 
            if ( (1 == mc_fdb->root) && 
                 ((mc_fdb->dst_dev == dev) ||
                  (mc_fdb->from_dev == dev)) )
            {
               mc_fdb->wan_tci  = 0;
               mc_fdb->num_tags = 0; 
               ret = bcm_mcast_blog_process(pif, (void*)mc_fdb, BCM_MCAST_PROTO_IPV4, 
                                            &pif->mc_ipv4_hash[bcm_mcast_igmp_hash(mc_fdb->txGrp.s_addr)]);
               if(ret < 0)
               {
                  /* bcm_mcast_blog_process may return -1 if there are no blog rules
                   * which may be a valid scenario, in which case we delete the
                   * multicast entry.
                   */
                  bcm_mcast_igmp_del_entry(pif, mc_fdb, NULL, NULL);
                  __logInfo("Unable to activate blog");
               }
            }
         }
      }
   }
#endif   
   bcm_mcast_igmp_set_timer(pif);

   return;
}

static t_igmp_rep_entry *bcm_mcast_igmp_rep_find(const t_igmp_grp_entry *mc_fdb,
                                                     const struct in_addr *rep,
                                                     unsigned char *repMac)
{
   t_igmp_rep_entry *rep_entry;

   list_for_each_entry(rep_entry, &mc_fdb->rep_list, list)
   {
      if ((rep && (rep_entry->rep.s_addr == rep->s_addr)) ||
          (repMac && (0 == memcmp(rep_entry->repMac, repMac, ETH_ALEN))))
      {
         return rep_entry;
      }
   }

   return NULL;
}

void bcm_mcast_igmp_wipe_reporter_for_port (bcm_mcast_ifdata *pif,
                                            struct in_addr *rep, 
                                            struct net_device *rep_dev)
{
    int hashIndex;
    struct hlist_node *n = NULL;
    struct hlist_head *head = NULL;
    t_igmp_grp_entry *mc_fdb;

    for (hashIndex = 0; hashIndex < BCM_MCAST_HASH_SIZE ; hashIndex++)
    {
        head = &pif->mc_ipv4_hash[hashIndex];
        hlist_for_each_entry_safe(mc_fdb, n, head, hlist)
        {
            if ((mc_fdb->dst_dev == rep_dev) &&
                (bcm_mcast_igmp_rep_find(mc_fdb, rep, NULL) != NULL))
            {
                /* The reporter we're looking for has been found
                   in a record pointing to its old port */
                bcm_mcast_igmp_del_entry(pif, mc_fdb, rep, NULL);
            }
        }
    }
    bcm_mcast_igmp_set_timer(pif);
}

void bcm_mcast_igmp_wipe_reporter_by_mac(bcm_mcast_ifdata *pif,
                                         unsigned char *repMac)
{
    int hashIndex;
    struct hlist_node *n = NULL;
    struct hlist_head *head = NULL;
    t_igmp_grp_entry *mc_fdb;

    for ( hashIndex = 0; hashIndex < BCM_MCAST_HASH_SIZE ; hashIndex++)
    {
        head = &pif->mc_ipv4_hash[hashIndex];
        hlist_for_each_entry_safe(mc_fdb, n, head, hlist)
        {
            if ((bcm_mcast_igmp_rep_find(mc_fdb, NULL, repMac) != NULL))
            {
                bcm_mcast_igmp_del_entry(pif, mc_fdb, NULL, repMac);
            }
        }
    }
    bcm_mcast_igmp_set_timer(pif);
}

#if defined(CONFIG_BLOG)
t_igmp_grp_entry *bcm_mcast_igmp_fdb_copy(bcm_mcast_ifdata       *pif, 
                                          const t_igmp_grp_entry *igmp_fdb)
{
   t_igmp_grp_entry *new_igmp_fdb = NULL;
   t_igmp_rep_entry *rep_entry = NULL;
   t_igmp_rep_entry *rep_entry_n = NULL;
   int success = 1;
   struct hlist_head *head = NULL;

   new_igmp_fdb = kmem_cache_alloc(mcast_ctrl->ipv4_grp_cache, GFP_ATOMIC);
   if (new_igmp_fdb)
   {
      memcpy(new_igmp_fdb, igmp_fdb, sizeof(*new_igmp_fdb));
      new_igmp_fdb->blog_idx.fc.word = BLOG_KEY_FC_INVALID;
      new_igmp_fdb->blog_idx.mc.word = BLOG_KEY_MCAST_INVALID;
      new_igmp_fdb->root = 0;
      INIT_LIST_HEAD(&new_igmp_fdb->rep_list);

      list_for_each_entry(rep_entry, &igmp_fdb->rep_list, list)
      {
         rep_entry_n = kmem_cache_alloc(mcast_ctrl->ipv4_rep_cache, GFP_ATOMIC);
         if(rep_entry_n)
         {
            memcpy(rep_entry_n, rep_entry, sizeof(*rep_entry));
            list_add_tail(&rep_entry_n->list, &new_igmp_fdb->rep_list);
         }
         else 
         {
            success = 0;
            break;
         }
      }

      if(success)
      {
         head = &pif->mc_ipv4_hash[bcm_mcast_igmp_hash(igmp_fdb->txGrp.s_addr)];
         hlist_add_head(&new_igmp_fdb->hlist, head);
      }
      else
      {
         list_for_each_entry_safe(rep_entry, rep_entry_n, &new_igmp_fdb->rep_list, list)
         {
            list_del(&rep_entry->list);
            kmem_cache_free(mcast_ctrl->ipv4_rep_cache, rep_entry);
         }
         kmem_cache_free(mcast_ctrl->ipv4_grp_cache, new_igmp_fdb);
         new_igmp_fdb = NULL;
      }
   }

   return new_igmp_fdb;
} /* bcm_mcast_igmp_fdb_copy */

void bcm_mcast_igmp_process_blog_enable( bcm_mcast_ifdata *pif, int enable )
{
   t_igmp_grp_entry *mc_fdb;
   int i;
   int ret;

   __logDebug("process blog enable setting %d", enable);

   if(NULL == pif)
   {
      return;
   }

   /* remove all entries if dev is NULL or activate is false otherwise 
      deactivate root entries and remove all other entries involving dev */
   for (i = 0; i < BCM_MCAST_HASH_SIZE; i++) 
   {
      struct hlist_node *n;
      hlist_for_each_entry_safe(mc_fdb, n, &pif->mc_ipv4_hash[i], hlist) 
      {
         if ( 0 == enable )
         {
            if (0 == mc_fdb->root)
            {
               bcm_mcast_igmp_del_entry(pif, mc_fdb, NULL, NULL);
            }
            else
            {
               bcm_mcast_blog_release(BCM_MCAST_PROTO_IPV4, (void *)mc_fdb);
               mc_fdb->blog_idx.fc.word = BLOG_KEY_FC_INVALID;
               mc_fdb->blog_idx.mc.word = BLOG_KEY_MCAST_INVALID;
            }
         }
         else
         {
            if (1 == mc_fdb->root)
            {
               mc_fdb->wan_tci  = 0;
               mc_fdb->num_tags = 0; 
               ret = bcm_mcast_blog_process(pif, (void*)mc_fdb, BCM_MCAST_PROTO_IPV4, 
                                            &pif->mc_ipv4_hash[bcm_mcast_igmp_hash(mc_fdb->txGrp.s_addr)]);
               if(ret < 0)
               {
                  /* bcm_mcast_blog_process may return -1 if there are no blog rules
                   * which may be a valid scenario, in which case we delete the
                   * multicast entry.
                   */
                  bcm_mcast_igmp_del_entry(pif, mc_fdb, NULL, NULL);
                  __logInfo("Unable to activate blog");
               }
            }
         }
      }
   }
   bcm_mcast_igmp_set_timer(pif);

   return;
}
#endif

static int bcm_mcast_igmp_update(bcm_mcast_ifdata *pif, 
                                 struct net_device *dst_dev, 
                                 struct in_addr *rxGrp,
                                 struct in_addr *txGrp,
                                 struct in_addr *rep,
                                 unsigned char *repMac,
                                 unsigned char rep_proto_ver,
                                 int mode, 
                                 struct in_addr *src,
                                 struct net_device *from_dev,
                                 uint32_t info)
{
   t_igmp_grp_entry *dst;
   int ret = 0;
   struct hlist_head *head;

   head = &pif->mc_ipv4_hash[bcm_mcast_igmp_hash(txGrp->s_addr)];
   hlist_for_each_entry(dst, head, hlist) {
      if ((dst->txGrp.s_addr == txGrp->s_addr) && (dst->rxGrp.s_addr == rxGrp->s_addr))
      {
         if((src->s_addr == dst->src_entry.src.s_addr) &&
            (mode == dst->src_entry.filt_mode) && 
            (dst->from_dev == from_dev) &&
            (dst->dst_dev == dst_dev) &&
            (dst->info == info))
         {
            /* found entry - update TS */
            t_igmp_rep_entry *reporter = bcm_mcast_igmp_rep_find(dst, rep, NULL);
            if(reporter == NULL)
            {
               reporter = kmem_cache_alloc(mcast_ctrl->ipv4_rep_cache, GFP_ATOMIC);
               if(reporter)
               {
                  reporter->rep.s_addr = rep->s_addr;
                  reporter->tstamp = jiffies + (mcast_ctrl->igmp_general_query_timeout_secs * HZ);
                  memcpy(reporter->repMac, repMac, ETH_ALEN);
                  reporter->rep_proto_ver = rep_proto_ver;
                  list_add_tail(&reporter->list, &dst->rep_list);
                  bcm_mcast_notify_event(BCM_MCAST_EVT_SNOOP_ADD, BCM_MCAST_PROTO_IPV4, dst, reporter);
                  bcm_mcast_igmp_set_timer(pif);
               }
            }
            else
            {
               reporter->tstamp = jiffies + (mcast_ctrl->igmp_general_query_timeout_secs * HZ);
               bcm_mcast_igmp_set_timer(pif);
            }
            ret = 1;
         }
      }
   }

   return ret;
}

int bcm_mcast_igmp_add(struct net_device *from_dev,
                       int wan_ops,
                       bcm_mcast_ifdata *pif, 
                       struct net_device *dst_dev, 
                       struct in_addr *rxGrp, 
                       struct in_addr *txGrp, 
                       struct in_addr *rep,
                       unsigned char *repMac,
                       unsigned char rep_proto_ver,
                       int mode, 
                       uint16_t tci, 
                       struct in_addr *src,
                       int lanppp,
                       int excludePort,
                       char enRtpSeqCheck,
                       uint32_t info)
{
   t_igmp_grp_entry *mc_fdb = NULL;
   t_igmp_rep_entry *rep_entry = NULL;
   struct hlist_head *head = NULL;
#if defined(CONFIG_BLOG)
   int ret = 1;
#endif

   if(!pif || !dst_dev || !rxGrp || !txGrp || !rep || !from_dev)
      return 0;

   if( !bcm_mcast_igmp_control_filter(rxGrp->s_addr) || 
       !bcm_mcast_igmp_control_filter(txGrp->s_addr) )
      return 0;

   if((MCAST_INCLUDE != mode) && (MCAST_EXCLUDE != mode))
      return 0;

   spin_lock_bh(&pif->mc_igmp_lock);
   if (bcm_mcast_igmp_update(pif, dst_dev, rxGrp, txGrp, rep, repMac, rep_proto_ver, mode, src, from_dev, info))
   {
      spin_unlock_bh(&pif->mc_igmp_lock);
      return 0;
   }

   mc_fdb = kmem_cache_alloc(mcast_ctrl->ipv4_grp_cache, GFP_ATOMIC);
   if ( !mc_fdb )
   {
      spin_unlock_bh(&pif->mc_igmp_lock);
      return -ENOMEM;
   }

   rep_entry = kmem_cache_alloc(mcast_ctrl->ipv4_rep_cache, GFP_ATOMIC);
   if ( !rep_entry )
   {
      kmem_cache_free(mcast_ctrl->ipv4_grp_cache, mc_fdb);
      spin_unlock_bh(&pif->mc_igmp_lock);
      return -ENOMEM;
   }

   mc_fdb->txGrp.s_addr = txGrp->s_addr;
   mc_fdb->rxGrp.s_addr = rxGrp->s_addr;
   memcpy(&mc_fdb->src_entry, src, sizeof(struct in_addr));
   mc_fdb->src_entry.filt_mode = mode;
   mc_fdb->dst_dev = dst_dev;
   mc_fdb->lan_tci = tci;
   mc_fdb->wan_tci = 0;
   mc_fdb->num_tags = 0;
   mc_fdb->from_dev = from_dev;
   mc_fdb->type = wan_ops;
   mc_fdb->excludePort = excludePort;
   mc_fdb->enRtpSeqCheck = enRtpSeqCheck;
#if defined(CONFIG_BLOG)
   mc_fdb->root = 1;
   mc_fdb->blog_idx.fc.word = BLOG_KEY_FC_INVALID;
   mc_fdb->blog_idx.mc.word = BLOG_KEY_MCAST_INVALID;
#endif
   mc_fdb->info = info;
   mc_fdb->lanppp = lanppp;
   INIT_LIST_HEAD(&mc_fdb->rep_list);
   rep_entry->rep.s_addr = rep->s_addr;
   rep_entry->tstamp = jiffies + mcast_ctrl->igmp_general_query_timeout_secs * HZ;
   memcpy(rep_entry->repMac, repMac, ETH_ALEN);
   rep_entry->rep_proto_ver = rep_proto_ver;
   list_add_tail(&rep_entry->list, &mc_fdb->rep_list);
   head = &pif->mc_ipv4_hash[bcm_mcast_igmp_hash(txGrp->s_addr)];
   hlist_add_head(&mc_fdb->hlist, head);

#if defined(CONFIG_BLOG)
   ret = bcm_mcast_blog_process(pif, (void*)mc_fdb, BCM_MCAST_PROTO_IPV4, head);
   if(ret == -1)
   {
      hlist_del(&mc_fdb->hlist);
      kmem_cache_free(mcast_ctrl->ipv4_grp_cache, mc_fdb);
      kmem_cache_free(mcast_ctrl->ipv4_rep_cache, rep_entry);
      spin_unlock_bh(&pif->mc_igmp_lock);
      __logInfo("Unable to activate blog");
      return ret;
   }
   else if (ret == -2)
   {
      /* VLAN conflict must be cleaned up */
      struct igmp_grp_entry *dst;
      int filt_mode;
      struct hlist_node *n;
      if(mode == BCM_MCAST_SNOOP_IN_ADD)
         filt_mode = MCAST_INCLUDE;
      else
         filt_mode = MCAST_EXCLUDE;
    
      hlist_for_each_entry_safe(dst, n, head, hlist)
      {
         if ((dst->txGrp.s_addr == txGrp->s_addr) && (dst->rxGrp.s_addr == rxGrp->s_addr) &&
               (src->s_addr == dst->src_entry.src.s_addr) &&
               (filt_mode == dst->src_entry.filt_mode) &&
               (dst->from_dev == from_dev))
         {
            // HANDLE CONFLICT - REMOVE CONFLICTER UNLESS ROOT
            if (0 == dst->root)
            {
               bcm_mcast_igmp_del_entry(pif, dst, NULL, NULL);
            }
            else if (dst->blog_idx.mc.word != BLOG_KEY_INVALID)
            {
               bcm_mcast_blog_release(BCM_MCAST_PROTO_IPV4, (void *)dst);
               dst->blog_idx.mc.word = BLOG_KEY_INVALID;
            }
         }
      }
   }
#endif
   bcm_mcast_notify_event(BCM_MCAST_EVT_SNOOP_ADD, BCM_MCAST_PROTO_IPV4, mc_fdb, rep_entry);
   bcm_mcast_igmp_set_timer(pif);
   spin_unlock_bh(&pif->mc_igmp_lock);
   return 1;
}

int bcm_mcast_igmp_remove(struct net_device *from_dev,
                              bcm_mcast_ifdata *pif, 
                              struct net_device *dst_dev, 
                              struct in_addr *rxGrp, 
                              struct in_addr *txGrp, 
                              struct in_addr *rep, 
                              int mode, 
                              struct in_addr *src,
                              uint32_t info)
{
   t_igmp_grp_entry *mc_fdb;
   struct hlist_head *head = NULL;
   struct hlist_node *n;

   if(!pif || !dst_dev || !txGrp || !rxGrp || !rep || !from_dev)
      return 0;

   if(!bcm_mcast_igmp_control_filter(txGrp->s_addr))
      return 0;

   if(!bcm_mcast_igmp_control_filter(rxGrp->s_addr))
      return 0;

   if((MCAST_INCLUDE != mode) && (MCAST_EXCLUDE != mode))
      return 0;

   spin_lock_bh(&pif->mc_igmp_lock);
   head = &pif->mc_ipv4_hash[bcm_mcast_igmp_hash(txGrp->s_addr)];
   hlist_for_each_entry_safe(mc_fdb, n, head, hlist)
   {
      if ((mc_fdb->rxGrp.s_addr == rxGrp->s_addr) && 
          (mc_fdb->txGrp.s_addr == txGrp->s_addr) && 
          (mode == mc_fdb->src_entry.filt_mode) && 
          (mc_fdb->src_entry.src.s_addr == src->s_addr) &&
          (mc_fdb->from_dev == from_dev) &&
          (mc_fdb->dst_dev == dst_dev) &&
          (mc_fdb->info == info))
      {
         bcm_mcast_igmp_del_entry(pif, mc_fdb, rep, NULL);
      }
   }
   bcm_mcast_igmp_set_timer(pif);
   spin_unlock_bh(&pif->mc_igmp_lock);
   return 0;
}

int bcm_mcast_igmp_should_deliver(bcm_mcast_ifdata *pif, 
                                  const struct iphdr *pip,
                                  struct net_device *src_dev,
                                  struct net_device *dst_dev)
{
   t_igmp_grp_entry  *dst;
   struct hlist_head *head = NULL;
   int                should_deliver;
   int                is_routed;

   __logDebug("source device %s, dst device %s", src_dev->name, dst_dev->name);

   if (0 == bcm_mcast_igmp_control_filter(pip->daddr))
   {
      /* accept packet */
      return 1;
   }

   /* source for routed packets will be the bridge device */
   if ( src_dev == pif->dev )
   {
      is_routed = 1;
   }
   else
   {
      is_routed = 0;
   }

   /* drop traffic by default when snooping is enabled
      in blocking mode */
   if (BCM_MCAST_SNOOPING_BLOCKING_MODE == pif->igmp_snooping)
   {
      should_deliver = 0;
   }
   else
   {
      should_deliver = 1;
   }

   /* adhere to forwarding entries regardless of snooping mode */

   spin_lock_bh(&pif->mc_igmp_lock);
   head = &pif->mc_ipv4_hash[bcm_mcast_igmp_hash(pip->daddr)];
   hlist_for_each_entry(dst, head, hlist)
   {
      if ( (dst->txGrp.s_addr != pip->daddr) ||
           (dst->dst_dev != dst_dev) )
      {
         continue;
      }

      if (is_routed)
      {
         if (dst->type != BCM_MCAST_IF_ROUTED)
         {
            continue;
         }
      }
      else
      {
         if (dst->type != BCM_MCAST_IF_BRIDGED)
         {
            continue;
         }

         if (src_dev->priv_flags & IFF_WANDEV)
         {
            /* match exactly if src device is a WAN device - otherwise continue */
            if (dst->from_dev != src_dev)
            {
               continue;
            }
         }
         else 
         {
            /* if this is not an L2L mc_fdb entry continue */
            if (dst->from_dev != pif->dev)
            {
               continue;            
            }
         }
      }

      /* if this is an include entry source must match */
      if(dst->src_entry.filt_mode == MCAST_INCLUDE)
      {
         if (pip->saddr == dst->src_entry.src.s_addr)
         {
            /* matching entry */
            should_deliver = 1;
            break;
         }
      }
      else
      {
         /* exclude filter - exclude source not supported RFC4607/5771 */
         should_deliver = 1;
      }
   }
   spin_unlock_bh(&pif->mc_igmp_lock);
   return should_deliver;
}

static void bcm_mcast_igmp_display_entry(struct seq_file *seq,
                                         bcm_mcast_ifdata *pif,
                                         t_igmp_grp_entry *dst)
{
   t_igmp_rep_entry *rep_entry;
   int               first;
   int               tstamp;
   unsigned char    *txAddressP = (unsigned char *)&(dst->txGrp.s_addr);
   unsigned char    *rxAddressP = (unsigned char *)&(dst->rxGrp.s_addr);
   unsigned char    *srcAddressP = (unsigned char *)&(dst->src_entry.src.s_addr);

   seq_printf(seq, "%-6s %-6s %-7s %02d    0x%04x   0x%04x%04x", 
              pif->dev->name, 
              dst->dst_dev->name, 
              dst->from_dev->name, 
              dst->num_tags,
              ntohs(dst->lan_tci),
              ((dst->wan_tci >> 16) & 0xFFFF),
              (dst->wan_tci & 0xFFFF));

   seq_printf(seq, " %03u.%03u.%03u.%03u", txAddressP[0],txAddressP[1],txAddressP[2],txAddressP[3]);

   seq_printf(seq, " %-4s %03u.%03u.%03u.%03u %03u.%03u.%03u.%03u", 
              (dst->src_entry.filt_mode == MCAST_EXCLUDE) ? 
              "EX" : "IN",  
              rxAddressP[0],rxAddressP[1],rxAddressP[2],rxAddressP[3], 
              srcAddressP[0],srcAddressP[1],srcAddressP[2],srcAddressP[3] );

   first = 1;
   list_for_each_entry(rep_entry, &dst->rep_list, list)
   { 
      unsigned char *repAddressP = (unsigned char *)&(rep_entry->rep.s_addr);

      if ( 0 == pif->igmp_snooping )
      {
         tstamp = 0;
      }
      else
      {
         tstamp = (int)(rep_entry->tstamp - jiffies) / HZ;
      }

      if(first)
      {
#if defined(CONFIG_BLOG)
         seq_printf(seq, " %03u.%03u.%03u.%03u %-7d %-5d    0x%08x/0x%08x\n", 
                    repAddressP[0],repAddressP[1],repAddressP[2],repAddressP[3],
                    tstamp, dst->excludePort, dst->blog_idx.fc.word, dst->blog_idx.mc.word);
#else
         seq_printf(seq, " %03u.%03u.%03u.%03u %-7d %d\n",
                    repAddressP[0],repAddressP[1],repAddressP[2],repAddressP[3],
                    tstamp, dst->excludePort);
#endif
         first = 0;
      }
      else
      {
         seq_printf(seq, "%100s %03u.%03u.%03u.%03u %-7d\n", " ", 
                    repAddressP[0],repAddressP[1],repAddressP[2],repAddressP[3],
                    tstamp);
      }
   }
}

int bcm_mcast_igmp_display(struct seq_file *seq, bcm_mcast_ifdata *pif)
{
   int i;
   bcm_mcast_lower_port* lowerPort = NULL;

   seq_printf(seq, "igmp snooping %d  lan2lan-snooping %d/%d, rate-limit %dpps, priority %d\n",
              pif->igmp_snooping, 
              pif->igmp_lan2lan_mc_enable,
              bcm_mcast_get_lan2lan_snooping(BCM_MCAST_PROTO_IPV4, pif),
              pif->igmp_rate_limit,
              mcast_ctrl->mcastPriQueue);
   seq_printf(seq, " Port Name    Querier      Timeout\n");
   hlist_for_each_entry_rcu (lowerPort, &pif->lower_port_list, hlist)
   {
      int tstamp = lowerPort->querying_port ? (int)(lowerPort->querying_port_timer.expires - jiffies) / HZ : 0;
      struct net_device* lowerDev = dev_get_by_index(&init_net, lowerPort->port_ifindex);
      if (lowerDev) 
      {
         seq_printf(seq, "    %-6s        %-3s          %03d\n", lowerDev->name,
                                        lowerPort->querying_port ? "YES" : " NO", 
                                        tstamp);
         dev_put(lowerDev);
      }
   }
   seq_printf(seq, "bridge device src-dev #tags lan-tci  wan-tci");
#if defined(CONFIG_BLOG)
   seq_printf(seq, "    group           mode RxGroup         source          reporter        timeout ExcludPt Index      \n");
#else
   seq_printf(seq, "    group           mode RxGroup         source          reporter        timeout ExcludPt\n");
#endif

   for (i = 0; i < BCM_MCAST_HASH_SIZE; i++) 
   {
      t_igmp_grp_entry *entry;
      hlist_for_each_entry(entry, &pif->mc_ipv4_hash[i], hlist) 
      {
         bcm_mcast_igmp_display_entry(seq, pif, entry);
      }
   }

   return 0;
}

