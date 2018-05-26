#define pr_fmt(fmt)	"DAS: " fmt
#define PROC_BUF_SIZE 128
#define DOMAIN_HASH_SIZE 6
#define SCHED_GRANULARITY 100
#define DOMAIN_MAX_CNT (2 << DOMAIN_HASH_SIZE)

#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/spinlock.h>

static ssize_t das_read(struct file *, char __user *, size_t, loff_t *);

static ssize_t das_write(struct file *, const char __user *, size_t, loff_t *);

struct domain {
	int domain_id;
	struct hlist_node hlist;
	struct member *members;
	struct spinlock member_lock;
};

struct member {
	struct task_struct *task;
	struct domain *domain;
	struct list_head list;
};

static int _init_proc_api(void);
static void _remove_proc_api(void);
static void timer_handler(unsigned long);
static int dispatch_thread(void *);
static int add_domain(int);
static struct domain *get_domain(int);
static void remove_domain(int);
static int add_member(int, int);

struct task_struct *find_task_by_pid(unsigned int nr)
{
	struct task_struct *task;
	rcu_read_lock();
	task = pid_task(find_vpid(nr), PIDTYPE_PID);
	rcu_read_unlock();
	return task;
}
