#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/hashtable.h>
#include "das.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hsuan-Chi Kuo");
MODULE_DESCRIPTION("A domain-aware scheduler implementation.");

static DEFINE_HASHTABLE(domains, DOMAIN_HASH_SIZE);
static struct spinlock domain_lock;
static struct file_operations api_fops = {
  .read  = das_read,
  .write = das_write
};
static struct proc_dir_entry *das_parent, *api_file;
struct task_struct *dispatcher;

int __init das_init(void)
{
  printk(KERN_INFO "LOADING\n");
  hash_init(domains);
  _init_proc_api();
  dispatcher = kthread_run(dispatch_thread, NULL, "dispatcher");
  printk(KERN_INFO "LOADED\n");
  return 0;
}

void __exit das_exit(void)
{
  printk(KERN_INFO "UNLOADING\n");
  _remove_proc_api();
  kthread_stop(dispatcher);
  printk(KERN_INFO "UNLOADED\n");
}

module_init(das_init);
module_exit(das_exit);

/* FUNCTION IMPLEMENTATIONS */

static int _init_proc_api(void)
{
  das_parent = proc_mkdir("das", NULL); // Null beacuse it's under /proc
  if(!das_parent) {
    printk(KERN_INFO "Error creating proc entry");
    return -ENOMEM;
  }
  api_file =  proc_create("api", 0666, das_parent, &api_fops);
  if(!api_file) {
    printk(KERN_INFO "Error creating api entry");
    return -ENOMEM;
  }
  return 0;
}

static void _remove_proc_api(void)
{
  remove_proc_entry("api", das_parent);
  remove_proc_entry("das", NULL);
}

static ssize_t das_read (struct file *file, char __user *buffer,
                         size_t count, loff_t *data)
{
  int copied;
  char kbuf[PROC_BUF_SIZE];

  if((long)*data > 0) return 0;

  copied = 0;
  copy_to_user(buffer, kbuf, copied);
  *data += copied;
  return copied ;
}
static ssize_t das_write (struct file *file, const char __user *buffer,
                          size_t count, loff_t *data)
{
  if((long)*data > 0) return 0;
  char kbuf[PROC_BUF_SIZE], action;
  int copied, did, mid;
  int err = copy_from_user(kbuf, buffer, count);
  if (!err) {
    sscanf(kbuf, "%c", &action);
    sscanf(kbuf, "%d", &did);
    if(did >= DOMAIN_MAX_CNT)
      goto Inval;
    switch(action){
    case 'R':
      if (!get_domain(did)) {
        printk(KERN_INFO "Adding a domain %d\n", did);
        add_domain(did);
      }

      while(!sscanf(kbuf, "%d", &mid)) {
        printk(KERN_INFO "Adding a member %d\n", mid);
        add_member(did, mid);
      }

      break;

    case 'D':
      remove_domain(did);
      break;

    default:
      goto Inval;
    }
  } else
    return err;
  copied = count;
  *data += copied;
  return copied;

Inval:
  return -EINVAL;
}

static int dispatch_thread(void* data)
{
  while(!kthread_should_stop()) {
    schedule_timeout(msecs_to_jiffies(SCHED_GRANULARITY));
  }
  printk(KERN_INFO "kthread stopped\n");
}

static int add_domain(int did)
{
  struct domain *d = kmalloc(sizeof(struct domain), GFP_KERNEL);
  if(!d)
    return -ENOMEM;
  d->domain_id = did;
  d->members = NULL;
  hash_add(domains, &d->hlist, did);
  return 0;
}

static struct domain* get_domain(int did)
{
  struct domain *d = NULL;
  spin_lock(&domain_lock);
  hash_for_each_possible(domains, d, hlist, did) {
    if (d->domain_id == did)
      break;
  }
  spin_unlock(&domain_lock);
  return d;
}

static int add_member(int did, int mid)
{
  struct domain *d;
  struct member *m;
  d = get_domain(did);
  if (!d)
    return -EINVAL;
  m = kmalloc(sizeof(struct member), GFP_KERNEL);
  m->task = find_task_by_pid(mid);
  m->domain = d;
  spin_lock(&d->member_lock);
  list_add_tail(&m->list, &d->members->list);
  spin_unlock(&d->member_lock);
  return 0;
}

static void remove_domain(int did)
{
  struct domain *d;
  struct member *m, *tmp;
  d = get_domain(did);

  spin_lock(&d->member_lock);
  list_for_each_entry_safe(m, tmp, &d->members, list) {
    list_del(&m->list);
    kfree(m); 
  }
  spin_unlock(&d->member_lock);

  hash_del(&d->hlist);
  kfree(d);
}
