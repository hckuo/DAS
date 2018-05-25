#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include "das.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hsuan-Chi Kuo");
MODULE_DESCRIPTION("A domain-aware scheduler implementation.");

int __init das_init(void)
{
  printk(KERN_INFO "LOADING\n");
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
  remove_proc_entry("status", das_parent);
  remove_proc_entry("das", NULL);
}
static ssize_t das_read (struct file *file, char __user *buffer,
                         size_t count, loff_t *data)
{
  if((long)*data > 0) return 0;
  int copied = 0;
  char kbuf[PROC_BUF_SIZE] = "\0";
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
    if(did >= MAX_DOMAIN_COUNT)
      goto Inval;
    switch(action){
    case 'R':

      if(!has_domain(did))
        add_domain(did);

      while(!sscanf(kbuf, "%d", &mid)) {
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
  set_current_state(TASK_INTERRUPTIBLE);
  while(!kthread_should_stop()) {
    printk(KERN_INFO "kthread slept\n");
    schedule();
    set_current_state(TASK_INTERRUPTIBLE);
  }
  printk(KERN_INFO "kthread stopped\n");
}

static void timer_handler(unsigned long data)
{
  wake_up_process(dispatcher);
}

static int has_domain(int did)
{
  return ds[did] != NULL;
}

static int add_domain(int did)
{
  ds[did] = kzalloc(MAX_DOMAIN_CAP, GFP_KERNEL);
  ds[did]->size = 0;
  if(!ds[did])
    return -ENOMEM;
  return 0;
}

static int remove_domain(int did)
{
  kfree(ds[did]);
  ds[did] = NULL;
  return 0;
}

static int add_member(int did, int mid)
{
  ds[did]->members[ds[did]->size] = mid;
  ds[did]->size++;
  return ds[did]->size;
}

static int remove_member(int did, int mid)
{
  return 0;
}
