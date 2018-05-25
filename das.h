#define pr_fmt(fmt)	"DAS: " fmt
#define PROC_BUF_SIZE 128
#define MAX_DOMAIN_COUNT 32
#define MAX_DOMAIN_CAP 63

static ssize_t das_read (struct file *, char __user *,
                         size_t, loff_t *);

static ssize_t das_write (struct file *, const char __user *,
                          size_t, loff_t *);

static struct file_operations api_fops = {
  .read  = das_read,
  .write = das_write
};

static struct proc_dir_entry *das_parent, *api_file;

struct domain {
  int members[MAX_DOMAIN_CAP];
  size_t size;
};

struct domain* ds[MAX_DOMAIN_COUNT];
struct task_struct *dispatcher;

static int _init_proc_api(void);
static void _remove_proc_api(void);
static void timer_handler(unsigned long);
static int dispatch_thread(void*);
static int has_domain(int);
static int add_domain(int);
static int remove_domain(int);
static int add_member(int, int);
static int remove_member(int, int);

