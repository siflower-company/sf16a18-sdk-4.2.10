#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/rbtree.h>

struct module_log
{
	struct rb_node node;
	char *module_name;
	int log_enabled;
};

struct rb_root log_root = RB_ROOT;

int logctl_rb_insert(struct rb_root *root, struct module_log *data)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct module_log *this = container_of(*new, struct module_log, node);
		int result = strcmp(data->module_name, this->module_name);

		parent = *new;
		if (result < 0)
			new = &((*new)->rb_left);
		else if (result > 0)
			new = &((*new)->rb_right);
		else
			return -1;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, root);

	return 0;
}

struct module_log *logctl_rb_search(struct rb_root *root, char *string)
{
	struct rb_node *node = root->rb_node;

	while (node) {
		struct module_log *data = container_of(node, struct module_log, node);
		int result;

		result = strcmp(string, data->module_name);

		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else
			return data;
	}
	return NULL;
}

char *sf_log_entry[] = {
	"startcore",
};

static ssize_t sf_logctl_read(struct file *file, char __user *buffer,
							size_t count, loff_t *f_ops)
{
	char *buf = kmalloc(count, GFP_KERNEL);
	struct rb_node *tmp;
	int n = 0;

	if(!buf)
		return -ENOMEM;

	if(*f_ops > 0)
		return 0;

	for(tmp = rb_first(&log_root); tmp; tmp = rb_next(tmp))
	{
		n += snprintf(buf + n, count, "%s %d\n",
				rb_entry(tmp, struct module_log, node)->module_name,
				rb_entry(tmp, struct module_log, node)->log_enabled);
	}

	if(copy_to_user(buffer, buf, n))
		n = -EFAULT;

	*f_ops += n;
	kfree(buf);

	return n;
}

static ssize_t sf_logctl_write(struct file *file, const char __user *buffer,
							size_t count, loff_t *f_ops)
{
	struct module_log *entry;
	int enable;
	char sf_module_name[20];

	sscanf(buffer, "%s %d", sf_module_name, &enable);
	entry = logctl_rb_search(&log_root, sf_module_name);
	*f_ops += count;

	if(entry)
		entry->log_enabled = enable;
	else
		pr_err("module %s not usable in logctl!\n", sf_module_name);

	return count;
}

static struct file_operations sf_logctl_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= sf_logctl_read,
	.write		= sf_logctl_write,
	.llseek		= seq_lseek,
};

static int sf_log_enabled(char *sf_module_name)
{
	struct module_log *tmp;

	tmp = logctl_rb_search(&log_root, sf_module_name);
	if(!tmp) {
		return 0;
	}

	return tmp->log_enabled;
}
EXPORT_SYMBOL_GPL(sf_log_enabled);

static int sf_logctl_init(void)
{
	struct proc_dir_entry *file;
	struct module_log *module;
	int i;

	file = proc_create("logctl", 0644, NULL, &sf_logctl_ops);
	if (!file)
		return -ENOMEM;

	for(i = 0; i < ARRAY_SIZE(sf_log_entry); i++)
	{
		module = (struct module_log *)kmalloc(sizeof(struct module_log), GFP_KERNEL);
		module->module_name = sf_log_entry[i];
		module->log_enabled = 0;
		if(logctl_rb_insert(&log_root, module) != 0)
			return -1;
	}
	pr_info("SiFlower log system enabled!\n");

	return 0;
}

static void sf_logctl_exit(void)
{
	struct module_log *module;
	int i;

	for(i = 0; i < ARRAY_SIZE(sf_log_entry); i++) {
		module = logctl_rb_search(&log_root, sf_log_entry[i]);
		rb_erase(&(module->node), &log_root);
		kfree(module);
	}
	pr_err("SiFlower log system disabled!\n");
	return;
}

module_init(sf_logctl_init);
module_exit(sf_logctl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nevermore Wang <nevermore.wang@siflower.com.cn>");
MODULE_DESCRIPTION("SiFlower log system.");
