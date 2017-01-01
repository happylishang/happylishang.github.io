/* binder.c
 *
 * Android IPC Subsystem
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *   
 */

 // //异常处理代码 ￥
#include <asm/cacheflush.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/freezer.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/nsproxy.h>
#include <linux/poll.h>
#include <linux/debugfs.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/security.h>
#include "binder.h"
#include "binder_trace.h"
static DEFINE_MUTEX(binder_main_lock);
static DEFINE_MUTEX(binder_deferred_lock);
static DEFINE_MUTEX(binder_mmap_lock);
static HLIST_HEAD(binder_procs);
static HLIST_HEAD(binder_deferred_list);
static HLIST_HEAD(binder_dead_nodes);
static struct dentry *binder_debugfs_dir_entry_root;
static struct dentry *binder_debugfs_dir_entry_proc;
static struct binder_node *binder_context_mgr_node;
static uid_t binder_context_mgr_uid = -1;
static int binder_last_id;
static struct workqueue_struct *binder_deferred_workqueue;
#define BINDER_DEBUG_ENTRY(name) \
static int binder_##name##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, binder_##name##_show, inode->i_private); \
} \
\
static const struct file_operations binder_##name##_fops = { \
	.owner = THIS_MODULE, \
	.open = binder_##name##_open, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
}
static int binder_proc_show(struct seq_file *m, void *unused);
BINDER_DEBUG_ENTRY(proc);
/* This is only defined in include/asm-arm/sizes.h */
#ifndef SZ_1K
#define SZ_1K                               0x400
#endif
#ifndef SZ_4M
#define SZ_4M                               0x400000
#endif
#define FORBIDDEN_MMAP_FLAGS                (VM_WRITE)
#define BINDER_SMALL_BUF_SIZE (PAGE_SIZE * 64)
enum {
	BINDER_DEBUG_USER_ERROR             = 1U << 0,
	BINDER_DEBUG_FAILED_TRANSACTION     = 1U << 1,
	BINDER_DEBUG_DEAD_TRANSACTION       = 1U << 2,
	BINDER_DEBUG_OPEN_CLOSE             = 1U << 3,
	BINDER_DEBUG_DEAD_BINDER            = 1U << 4,
	BINDER_DEBUG_DEATH_NOTIFICATION     = 1U << 5,
	BINDER_DEBUG_READ_WRITE             = 1U << 6,
	BINDER_DEBUG_USER_REFS              = 1U << 7,
	BINDER_DEBUG_THREADS                = 1U << 8,
	BINDER_DEBUG_TRANSACTION            = 1U << 9,
	BINDER_DEBUG_TRANSACTION_COMPLETE   = 1U << 10,
	BINDER_DEBUG_FREE_BUFFER            = 1U << 11,
	BINDER_DEBUG_INTERNAL_REFS          = 1U << 12,
	BINDER_DEBUG_BUFFER_ALLOC           = 1U << 13,
	BINDER_DEBUG_PRIORITY_CAP           = 1U << 14,
	BINDER_DEBUG_BUFFER_ALLOC_ASYNC     = 1U << 15,
};
static uint32_t binder_debug_mask = BINDER_DEBUG_USER_ERROR |
	BINDER_DEBUG_FAILED_TRANSACTION | BINDER_DEBUG_DEAD_TRANSACTION;
module_param_named(debug_mask, binder_debug_mask, uint, S_IWUSR | S_IRUGO);
static bool binder_debug_no_lock;
module_param_named(proc_no_lock, binder_debug_no_lock, bool, S_IWUSR | S_IRUGO);
static DECLARE_WAIT_QUEUE_HEAD(binder_user_error_wait);
static int binder_stop_on_user_error;
static int binder_set_stop_on_user_error(const char *val,
					 struct kernel_param *kp)
{
	int ret;
	ret = param_set_int(val, kp);
	if (binder_stop_on_user_error < 2)
		wake_up(&binder_user_error_wait);
	return ret;
}
module_param_call(stop_on_user_error, binder_set_stop_on_user_error,
	param_get_int, &binder_stop_on_user_error, S_IWUSR | S_IRUGO);
#define binder_debug(mask, x...) \
	do { \
		if (binder_debug_mask & mask) \
			printk(KERN_INFO x); \
	} while (0)
#define binder_user_error(x...) \
	do { \
		if (binder_debug_mask & BINDER_DEBUG_USER_ERROR) \
			printk(KERN_INFO x); \
		if (binder_stop_on_user_error) \
			binder_stop_on_user_error = 2; \
	} while (0)
enum binder_stat_types {
	BINDER_STAT_PROC,
	BINDER_STAT_THREAD,
	BINDER_STAT_NODE,
	BINDER_STAT_REF,
	BINDER_STAT_DEATH,
	BINDER_STAT_TRANSACTION,
	BINDER_STAT_TRANSACTION_COMPLETE,
	BINDER_STAT_COUNT
};
// 
struct binder_stats {
	int br[_IOC_NR(BR_FAILED_REPLY) + 1];
	int bc[_IOC_NR(BC_DEAD_BINDER_DONE) + 1];
	int obj_created[BINDER_STAT_COUNT];
	int obj_deleted[BINDER_STAT_COUNT];
};
// 内核静态变量binder_stats 整体记录用
static struct binder_stats binder_stats;
// 删除
static inline void binder_stats_deleted(enum binder_stat_types type)
{
	binder_stats.obj_deleted[type]++;
}
// 添加
static inline void binder_stats_created(enum binder_stat_types type)
{
	binder_stats.obj_created[type]++;
}
struct binder_transaction_log_entry {
	int debug_id;
	int call_type;
	int from_proc;
	int from_thread;
	int target_handle;
	int to_proc;
	int to_thread;
	int to_node;
	int data_size;
	int offsets_size;
};
struct binder_transaction_log {
	int next;
	int full;
	struct binder_transaction_log_entry entry[32];
};
static struct binder_transaction_log binder_transaction_log;
static struct binder_transaction_log binder_transaction_log_failed;
static struct binder_transaction_log_entry *binder_transaction_log_add(
	struct binder_transaction_log *log)
{
	struct binder_transaction_log_entry *e;
	e = &log->entry[log->next];
	memset(e, 0, sizeof(*e));
	log->next++;
	if (log->next == ARRAY_SIZE(log->entry)) {
		log->next = 0;
		log->full = 1;
	}
	return e;
}
struct binder_work {
	struct list_head entry;
	enum {
		BINDER_WORK_TRANSACTION = 1,
		BINDER_WORK_TRANSACTION_COMPLETE,
		BINDER_WORK_NODE,
		BINDER_WORK_DEAD_BINDER,
		BINDER_WORK_DEAD_BINDER_AND_CLEAR,
		BINDER_WORK_CLEAR_DEATH_NOTIFICATION,
	} type;
};
struct binder_node {
	int debug_id;
	struct binder_work work;
	union {
		struct rb_node rb_node;
		struct hlist_node dead_node;
	};
	struct binder_proc *proc;
	struct hlist_head refs;
	int internal_strong_refs;
	int local_weak_refs;
	int local_strong_refs;
	void __user *ptr;
	void __user *cookie;
	unsigned has_strong_ref:1;
	unsigned pending_strong_ref:1;
	unsigned has_weak_ref:1;
	unsigned pending_weak_ref:1;
	unsigned has_async_transaction:1;
	unsigned accept_fds:1;
	unsigned min_priority:8;
	struct list_head async_todo;
};
struct binder_ref_death {
	struct binder_work work;
	void __user *cookie;
};
struct binder_ref {
	/* Lookups needed: */
	/*   node + proc => ref (transaction) */
	/*   desc + proc => ref (transaction, inc/dec ref) */
	/*   node => refs + procs (proc exit) */
	int debug_id;
	struct rb_node rb_node_desc;
	struct rb_node rb_node_node;
	struct hlist_node node_entry;
	struct binder_proc *proc;
	struct binder_node *node;
	uint32_t desc;
	int strong;
	int weak;
	struct binder_ref_death *death;
};
struct binder_buffer {
	struct list_head entry; /* free and allocated entries by address */
	struct rb_node rb_node; /* free entry by size or allocated entry */
				/* by address */
	unsigned free:1;
	unsigned allow_user_free:1;
	unsigned async_transaction:1;
	unsigned debug_id:29;
	struct binder_transaction *transaction;
	struct binder_node *target_node;
	size_t data_size;
	size_t offsets_size;
	uint8_t data[0];
};
enum binder_deferred_state {
	BINDER_DEFERRED_PUT_FILES    = 0x01,
	BINDER_DEFERRED_FLUSH        = 0x02,
	BINDER_DEFERRED_RELEASE      = 0x04,
};
struct binder_proc {
	struct hlist_node proc_node;
	struct rb_root threads;
	struct rb_root nodes;
	struct rb_root refs_by_desc;
	struct rb_root refs_by_node;
	int pid;
	struct vm_area_struct *vma;
	struct mm_struct *vma_vm_mm;
	struct task_struct *tsk;
	struct files_struct *files;
	struct hlist_node deferred_work_node;
	int deferred_work;
	void *buffer;
	ptrdiff_t user_buffer_offset;
	struct list_head buffers;
	struct rb_root free_buffers;
	struct rb_root allocated_buffers;
	size_t free_async_space;
	struct page **pages;
	size_t buffer_size;
	uint32_t buffer_free;
	struct list_head todo;
	wait_queue_head_t wait;
	struct binder_stats stats;
	struct list_head delivered_death;
	int max_threads;
	int requested_threads;
	int requested_threads_started;
	int ready_threads;
	long default_priority;
	struct dentry *debugfs_entry;
};
enum {
	BINDER_LOOPER_STATE_REGISTERED  = 0x01,
	BINDER_LOOPER_STATE_ENTERED     = 0x02,
	BINDER_LOOPER_STATE_EXITED      = 0x04,
	BINDER_LOOPER_STATE_INVALID     = 0x08,
	BINDER_LOOPER_STATE_WAITING     = 0x10,
	BINDER_LOOPER_STATE_NEED_RETURN = 0x20
};
struct binder_thread {
	struct binder_proc *proc;
	struct rb_node rb_node;
	int pid;
	int looper;
	struct binder_transaction *transaction_stack;
	struct list_head todo;
	uint32_t return_error; /* Write failed, return error code in read buf */
	uint32_t return_error2; /* Write failed, return error code in read */
		/* buffer. Used when sending a reply to a dead process that */
		/* we are also waiting on */
	wait_queue_head_t wait;
	struct binder_stats stats;
};
struct binder_transaction {
	int debug_id;
	struct binder_work work;
	struct binder_thread *from;
	struct binder_transaction *from_parent;
	struct binder_proc *to_proc;
	struct binder_thread *to_thread;
	struct binder_transaction *to_parent;
	unsigned need_reply:1;
	/* unsigned is_dead:1; */	/* not used at the moment */
	struct binder_buffer *buffer;
	unsigned int	code;
	unsigned int	flags;
	long	priority;
	long	saved_priority;
	uid_t	sender_euid;
};
static void
binder_defer_work(struct binder_proc *proc, enum binder_deferred_state defer);
/*
 * copied from get_unused_fd_flags
 */
int task_get_unused_fd_flags(struct binder_proc *proc, int flags)
{
	struct files_struct *files = proc->files;
	int fd, error;
	struct fdtable *fdt;
	unsigned long rlim_cur;
	unsigned long irqs;
	if (files == NULL)
		return -ESRCH;
	error = -EMFILE;
	spin_lock(&files->file_lock);
repeat:
	fdt = files_fdtable(files);
	fd = find_next_zero_bit(fdt->open_fds, fdt->max_fds, files->next_fd);
	/*
	 * N.B. For clone tasks sharing a files structure, this test
	 * will limit the total number of files that can be opened.
	 */
	rlim_cur = 0;
	if (lock_task_sighand(proc->tsk, &irqs)) {
		rlim_cur = proc->tsk->signal->rlim[RLIMIT_NOFILE].rlim_cur;
		unlock_task_sighand(proc->tsk, &irqs);
	}
	if (fd >= rlim_cur)
		goto out;
	/* Do we need to expand the fd array or fd set?  */
	error = expand_files(files, fd);
	if (error < 0)
		goto out;
	if (error) {
		/*
		 * If we needed to expand the fs array we
		 * might have blocked - try again.
		 */
		error = -EMFILE;
		goto repeat;
	}
	__set_open_fd(fd, fdt);
	if (flags & O_CLOEXEC)
		__set_close_on_exec(fd, fdt);
	else
		__clear_close_on_exec(fd, fdt);
	files->next_fd = fd + 1;
#if 1
	/* Sanity check */
	if (fdt->fd[fd] != NULL) {
		printk(KERN_WARNING "get_unused_fd: slot %d not NULL!\n", fd);
		fdt->fd[fd] = NULL;
	}
#endif
	error = fd;
out:
	spin_unlock(&files->file_lock);
	return error;
}
/*
 * copied from fd_install
 */
static void task_fd_install(
	struct binder_proc *proc, unsigned int fd, struct file *file)
{
	struct files_struct *files = proc->files;
	struct fdtable *fdt;
	if (files == NULL)
		return;
	spin_lock(&files->file_lock);
	fdt = files_fdtable(files);
	BUG_ON(fdt->fd[fd] != NULL);
	rcu_assign_pointer(fdt->fd[fd], file);
	spin_unlock(&files->file_lock);
}
/*
 * copied from __put_unused_fd in open.c
 */
static void __put_unused_fd(struct files_struct *files, unsigned int fd)
{
	struct fdtable *fdt = files_fdtable(files);
	__clear_open_fd(fd, fdt);
	if (fd < files->next_fd)
		files->next_fd = fd;
}
/*
 * copied from sys_close
 */
static long task_close_fd(struct binder_proc *proc, unsigned int fd)
{
	struct file *filp;
	struct files_struct *files = proc->files;
	struct fdtable *fdt;
	int retval;
	if (files == NULL)
		return -ESRCH;
	spin_lock(&files->file_lock);
	fdt = files_fdtable(files);
	if (fd >= fdt->max_fds)
		goto out_unlock;
	filp = fdt->fd[fd];
	if (!filp)
		goto out_unlock;
	rcu_assign_pointer(fdt->fd[fd], NULL);
	__clear_close_on_exec(fd, fdt);
	__put_unused_fd(files, fd);
	spin_unlock(&files->file_lock);
	retval = filp_close(filp, files);
	/* can't restart close syscall because file table entry was cleared */
	if (unlikely(retval == -ERESTARTSYS ||
		     retval == -ERESTARTNOINTR ||
		     retval == -ERESTARTNOHAND ||
		     retval == -ERESTART_RESTARTBLOCK))
		retval = -EINTR;
	return retval;
out_unlock:
	spin_unlock(&files->file_lock);
	return -EBADF;
}
static inline void binder_lock(const char *tag)
{
	trace_binder_lock(tag);
	mutex_lock(&binder_main_lock);
	trace_binder_locked(tag);
}
static inline void binder_unlock(const char *tag)
{
	trace_binder_unlock(tag);
	mutex_unlock(&binder_main_lock);
}
static void binder_set_nice(long nice)
{
	long min_nice;
	if (can_nice(current, nice)) {
		set_user_nice(current, nice);
		return;
	}
	min_nice = 20 - current->signal->rlim[RLIMIT_NICE].rlim_cur;
	binder_debug(BINDER_DEBUG_PRIORITY_CAP,
		     "binder: %d: nice value %ld not allowed use "
		     "%ld instead\n", current->pid, nice, min_nice);
	set_user_nice(current, min_nice);
	if (min_nice < 20)
		return;
	binder_user_error("binder: %d RLIMIT_NICE not set\n", current->pid);
}
static size_t binder_buffer_size(struct binder_proc *proc,
				 struct binder_buffer *buffer)
{
	if (list_is_last(&buffer->entry, &proc->buffers))
		return proc->buffer + proc->buffer_size - (void *)buffer->data;
	else
		return (size_t)list_entry(buffer->entry.next,
			struct binder_buffer, entry) - (size_t)buffer->data;
}
static void binder_insert_free_buffer(struct binder_proc *proc,
				      struct binder_buffer *new_buffer)
{
	struct rb_node **p = &proc->free_buffers.rb_node;
	struct rb_node *parent = NULL;
	struct binder_buffer *buffer;
	size_t buffer_size;
	size_t new_buffer_size;
	BUG_ON(!new_buffer->free);
	new_buffer_size = binder_buffer_size(proc, new_buffer);
	binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
		     "binder: %d: add free buffer, size %zd, "
		     "at %p\n", proc->pid, new_buffer_size, new_buffer);
	while (*p) {
		parent = *p;
		buffer = rb_entry(parent, struct binder_buffer, rb_node);
		BUG_ON(!buffer->free);
		buffer_size = binder_buffer_size(proc, buffer);
		if (new_buffer_size < buffer_size)
			p = &parent->rb_left;
		else
			p = &parent->rb_right;
	}
	rb_link_node(&new_buffer->rb_node, parent, p);
	rb_insert_color(&new_buffer->rb_node, &proc->free_buffers);
}
static void binder_insert_allocated_buffer(struct binder_proc *proc,
					   struct binder_buffer *new_buffer)
{
	struct rb_node **p = &proc->allocated_buffers.rb_node;
	struct rb_node *parent = NULL;
	struct binder_buffer *buffer;
	BUG_ON(new_buffer->free);
	while (*p) {
		parent = *p;
		buffer = rb_entry(parent, struct binder_buffer, rb_node);
		BUG_ON(buffer->free);
		if (new_buffer < buffer)
			p = &parent->rb_left;
		else if (new_buffer > buffer)
			p = &parent->rb_right;
		else
			BUG();
	}
	rb_link_node(&new_buffer->rb_node, parent, p);
	rb_insert_color(&new_buffer->rb_node, &proc->allocated_buffers);
}
static struct binder_buffer *binder_buffer_lookup(struct binder_proc *proc,
						  void __user *user_ptr)
{
	struct rb_node *n = proc->allocated_buffers.rb_node;
	struct binder_buffer *buffer;
	struct binder_buffer *kern_ptr;
	kern_ptr = user_ptr - proc->user_buffer_offset
		- offsetof(struct binder_buffer, data);
	while (n) {
		buffer = rb_entry(n, struct binder_buffer, rb_node);
		BUG_ON(buffer->free);
		if (kern_ptr < buffer)
			n = n->rb_left;
		else if (kern_ptr > buffer)
			n = n->rb_right;
		else
			return buffer;
	}
	return NULL;
}
static int binder_update_page_range(struct binder_proc *proc, int allocate,
				    void *start, void *end,
				    struct vm_area_struct *vma)
{
	void *page_addr;
	unsigned long user_page_addr;
	struct vm_struct tmp_area;
	struct page **page;
	struct mm_struct *mm;
	binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
		     "binder: %d: %s pages %p-%p\n", proc->pid,
		     allocate ? "allocate" : "free", start, end);
	if (end <= start)
		return 0;
	trace_binder_update_page_range(proc, allocate, start, end);
	if (vma)
		mm = NULL;
	else
		mm = get_task_mm(proc->tsk);
	if (mm) {
		down_write(&mm->mmap_sem);
		vma = proc->vma;
		if (vma && mm != proc->vma_vm_mm) {
			pr_err("binder: %d: vma mm and task mm mismatch\n",
				proc->pid);
			vma = NULL;
		}
	}
	if (allocate == 0)
		goto free_range;
	if (vma == NULL) {
		printk(KERN_ERR "binder: %d: binder_alloc_buf failed to "
		       "map pages in userspace, no vma\n", proc->pid);
		goto err_no_vma;
	}
	for (page_addr = start; page_addr < end; page_addr += PAGE_SIZE) {
		int ret;
		struct page **page_array_ptr;
		page = &proc->pages[(page_addr - proc->buffer) / PAGE_SIZE];
		BUG_ON(*page);
		*page = alloc_page(GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO);
		if (*page == NULL) {
			printk(KERN_ERR "binder: %d: binder_alloc_buf failed "
			       "for page at %p\n", proc->pid, page_addr);
			goto err_alloc_page_failed;
		}
		tmp_area.addr = page_addr;
		tmp_area.size = PAGE_SIZE + PAGE_SIZE /* guard page? */;
		page_array_ptr = page;
		ret = map_vm_area(&tmp_area, PAGE_KERNEL, &page_array_ptr);
		if (ret) {
			printk(KERN_ERR "binder: %d: binder_alloc_buf failed "
			       "to map page at %p in kernel\n",
			       proc->pid, page_addr);
			goto err_map_kernel_failed;
		}
		user_page_addr =
			(uintptr_t)page_addr + proc->user_buffer_offset;
		ret = vm_insert_page(vma, user_page_addr, page[0]);
		if (ret) {
			printk(KERN_ERR "binder: %d: binder_alloc_buf failed "
			       "to map page at %lx in userspace\n",
			       proc->pid, user_page_addr);
			goto err_vm_insert_page_failed;
		}
		/* vm_insert_page does not seem to increment the refcount */
	}
	if (mm) {
		up_write(&mm->mmap_sem);
		mmput(mm);
	}
	return 0;
free_range:
	for (page_addr = end - PAGE_SIZE; page_addr >= start;
	     page_addr -= PAGE_SIZE) {
		page = &proc->pages[(page_addr - proc->buffer) / PAGE_SIZE];
		if (vma)
			zap_page_range(vma, (uintptr_t)page_addr +
				proc->user_buffer_offset, PAGE_SIZE, NULL);
err_vm_insert_page_failed:
		unmap_kernel_range((unsigned long)page_addr, PAGE_SIZE);
err_map_kernel_failed:
		__free_page(*page);
		*page = NULL;
err_alloc_page_failed:
		;
	}
err_no_vma:
	if (mm) {
		up_write(&mm->mmap_sem);
		mmput(mm);
	}
	return -ENOMEM;
}
static struct binder_buffer *binder_alloc_buf(struct binder_proc *proc,
					      size_t data_size,
					      size_t offsets_size, int is_async)
{
	struct rb_node *n = proc->free_buffers.rb_node;
	struct binder_buffer *buffer;
	size_t buffer_size;
	struct rb_node *best_fit = NULL;
	void *has_page_addr;
	void *end_page_addr;
	size_t size;
	if (proc->vma == NULL) {
		printk(KERN_ERR "binder: %d: binder_alloc_buf, no vma\n",
		       proc->pid);
		return NULL;
	}
	size = ALIGN(data_size, sizeof(void *)) +
		ALIGN(offsets_size, sizeof(void *));
	if (size < data_size || size < offsets_size) {
		binder_user_error("binder: %d: got transaction with invalid "
			"size %zd-%zd\n", proc->pid, data_size, offsets_size);
		return NULL;
	}
	if (is_async &&
	    proc->free_async_space < size + sizeof(struct binder_buffer)) {
		binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
			     "binder: %d: binder_alloc_buf size %zd"
			     "failed, no async space left\n", proc->pid, size);
		return NULL;
	}
	while (n) {
		buffer = rb_entry(n, struct binder_buffer, rb_node);
		BUG_ON(!buffer->free);
		buffer_size = binder_buffer_size(proc, buffer);
		if (size < buffer_size) {
			best_fit = n;
			n = n->rb_left;
		} else if (size > buffer_size)
			n = n->rb_right;
		else {
			best_fit = n;
			break;
		}
	}
	if (best_fit == NULL) {
		printk(KERN_ERR "binder: %d: binder_alloc_buf size %zd failed, "
		       "no address space\n", proc->pid, size);
		return NULL;
	}
	if (n == NULL) {
		buffer = rb_entry(best_fit, struct binder_buffer, rb_node);
		buffer_size = binder_buffer_size(proc, buffer);
	}
	binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
		     "binder: %d: binder_alloc_buf size %zd got buff"
		     "er %p size %zd\n", proc->pid, size, buffer, buffer_size);
	has_page_addr =
		(void *)(((uintptr_t)buffer->data + buffer_size) & PAGE_MASK);
	if (n == NULL) {
		if (size + sizeof(struct binder_buffer) + 4 >= buffer_size)
			buffer_size = size; /* no room for other buffers */
		else
			buffer_size = size + sizeof(struct binder_buffer);
	}
	end_page_addr =
		(void *)PAGE_ALIGN((uintptr_t)buffer->data + buffer_size);
	if (end_page_addr > has_page_addr)
		end_page_addr = has_page_addr;
	if (binder_update_page_range(proc, 1,
	    (void *)PAGE_ALIGN((uintptr_t)buffer->data), end_page_addr, NULL))
		return NULL;
	rb_erase(best_fit, &proc->free_buffers);
	buffer->free = 0;
	binder_insert_allocated_buffer(proc, buffer);
	if (buffer_size != size) {
		struct binder_buffer *new_buffer = (void *)buffer->data + size;
		list_add(&new_buffer->entry, &buffer->entry);
		new_buffer->free = 1;
		binder_insert_free_buffer(proc, new_buffer);
	}
	binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
		     "binder: %d: binder_alloc_buf size %zd got "
		     "%p\n", proc->pid, size, buffer);
	buffer->data_size = data_size;
	buffer->offsets_size = offsets_size;
	buffer->async_transaction = is_async;
	if (is_async) {
		proc->free_async_space -= size + sizeof(struct binder_buffer);
		binder_debug(BINDER_DEBUG_BUFFER_ALLOC_ASYNC,
			     "binder: %d: binder_alloc_buf size %zd "
			     "async free %zd\n", proc->pid, size,
			     proc->free_async_space);
	}
	return buffer;
}
static void *buffer_start_page(struct binder_buffer *buffer)
{
	return (void *)((uintptr_t)buffer & PAGE_MASK);
}
static void *buffer_end_page(struct binder_buffer *buffer)
{
	return (void *)(((uintptr_t)(buffer + 1) - 1) & PAGE_MASK);
}
static void binder_delete_free_buffer(struct binder_proc *proc,
				      struct binder_buffer *buffer)
{
	struct binder_buffer *prev, *next = NULL;
	int free_page_end = 1;
	int free_page_start = 1;
	BUG_ON(proc->buffers.next == &buffer->entry);
	prev = list_entry(buffer->entry.prev, struct binder_buffer, entry);
	BUG_ON(!prev->free);
	if (buffer_end_page(prev) == buffer_start_page(buffer)) {
		free_page_start = 0;
		if (buffer_end_page(prev) == buffer_end_page(buffer))
			free_page_end = 0;
		binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
			     "binder: %d: merge free, buffer %p "
			     "share page with %p\n", proc->pid, buffer, prev);
	}
	if (!list_is_last(&buffer->entry, &proc->buffers)) {
		next = list_entry(buffer->entry.next,
				  struct binder_buffer, entry);
		if (buffer_start_page(next) == buffer_end_page(buffer)) {
			free_page_end = 0;
			if (buffer_start_page(next) ==
			    buffer_start_page(buffer))
				free_page_start = 0;
			binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
				     "binder: %d: merge free, buffer"
				     " %p share page with %p\n", proc->pid,
				     buffer, prev);
		}
	}
	list_del(&buffer->entry);
	if (free_page_start || free_page_end) {
		binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
			     "binder: %d: merge free, buffer %p do "
			     "not share page%s%s with with %p or %p\n",
			     proc->pid, buffer, free_page_start ? "" : " end",
			     free_page_end ? "" : " start", prev, next);
		binder_update_page_range(proc, 0, free_page_start ?
			buffer_start_page(buffer) : buffer_end_page(buffer),
			(free_page_end ? buffer_end_page(buffer) :
			buffer_start_page(buffer)) + PAGE_SIZE, NULL);
	}
}
static void binder_free_buf(struct binder_proc *proc,
			    struct binder_buffer *buffer)
{
	size_t size, buffer_size;
	buffer_size = binder_buffer_size(proc, buffer);
	size = ALIGN(buffer->data_size, sizeof(void *)) +
		ALIGN(buffer->offsets_size, sizeof(void *));
	binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
		     "binder: %d: binder_free_buf %p size %zd buffer"
		     "_size %zd\n", proc->pid, buffer, size, buffer_size);
	BUG_ON(buffer->free);
	BUG_ON(size > buffer_size);
	BUG_ON(buffer->transaction != NULL);
	BUG_ON((void *)buffer < proc->buffer);
	BUG_ON((void *)buffer > proc->buffer + proc->buffer_size);
	if (buffer->async_transaction) {
		proc->free_async_space += size + sizeof(struct binder_buffer);
		binder_debug(BINDER_DEBUG_BUFFER_ALLOC_ASYNC,
			     "binder: %d: binder_free_buf size %zd "
			     "async free %zd\n", proc->pid, size,
			     proc->free_async_space);
	}
	binder_update_page_range(proc, 0,
		(void *)PAGE_ALIGN((uintptr_t)buffer->data),
		(void *)(((uintptr_t)buffer->data + buffer_size) & PAGE_MASK),
		NULL);
	rb_erase(&buffer->rb_node, &proc->allocated_buffers);
	buffer->free = 1;
	if (!list_is_last(&buffer->entry, &proc->buffers)) {
		struct binder_buffer *next = list_entry(buffer->entry.next,
						struct binder_buffer, entry);
		if (next->free) {
			rb_erase(&next->rb_node, &proc->free_buffers);
			binder_delete_free_buffer(proc, next);
		}
	}
	if (proc->buffers.next != &buffer->entry) {
		struct binder_buffer *prev = list_entry(buffer->entry.prev,
						struct binder_buffer, entry);
		if (prev->free) {
			binder_delete_free_buffer(proc, buffer);
			rb_erase(&prev->rb_node, &proc->free_buffers);
			buffer = prev;
		}
	}
	binder_insert_free_buffer(proc, buffer);
}
static struct binder_node *binder_get_node(struct binder_proc *proc,
					   void __user *ptr)
{
	struct rb_node *n = proc->nodes.rb_node;
	struct binder_node *node;
	while (n) {
		node = rb_entry(n, struct binder_node, rb_node);
		if (ptr < node->ptr)
			n = n->rb_left;
		else if (ptr > node->ptr)
			n = n->rb_right;
		else
			return node;
	}
	return NULL;
}
static struct binder_node *binder_new_node(struct binder_proc *proc,
					   void __user *ptr,
					   void __user *cookie)
{
	struct rb_node **p = &proc->nodes.rb_node;
	struct rb_node *parent = NULL;
	struct binder_node *node;
	while (*p) {
		parent = *p;
		node = rb_entry(parent, struct binder_node, rb_node);
		if (ptr < node->ptr)
			p = &(*p)->rb_left;
		else if (ptr > node->ptr)
			p = &(*p)->rb_right;
		else
			return NULL;
	}
	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (node == NULL)
		return NULL;
	binder_stats_created(BINDER_STAT_NODE);
	rb_link_node(&node->rb_node, parent, p);
	rb_insert_color(&node->rb_node, &proc->nodes);
	node->debug_id = ++binder_last_id;
	node->proc = proc;
	node->ptr = ptr;
	node->cookie = cookie;
	node->work.type = BINDER_WORK_NODE;
	INIT_LIST_HEAD(&node->work.entry);
	INIT_LIST_HEAD(&node->async_todo);
	binder_debug(BINDER_DEBUG_INTERNAL_REFS,
		     "binder: %d:%d node %d u%p c%p created\n",
		     proc->pid, current->pid, node->debug_id,
		     node->ptr, node->cookie);
	return node;
}
// 增加引用计数 target 1 0 null
static int binder_inc_node(struct binder_node *node, int strong, int internal,
			   struct list_head *target_list)
{
	if (strong) {
		if (internal) {
			if (target_list == NULL &&
			    node->internal_strong_refs == 0 &&
			    !(node == binder_context_mgr_node &&
			    node->has_strong_ref)) {
				printk(KERN_ERR "binder: invalid inc strong "
					"node for %d\n", node->debug_id);
				return -EINVAL;
			}
			node->internal_strong_refs++;
		} else
			node->local_strong_refs++;
		if (!node->has_strong_ref && target_list) {
			list_del_init(&node->work.entry);
			list_add_tail(&node->work.entry, target_list);
		}
	} else {
		if (!internal)
			node->local_weak_refs++;
		if (!node->has_weak_ref && list_empty(&node->work.entry)) {
			if (target_list == NULL) {
				printk(KERN_ERR "binder: invalid inc weak node "
					"for %d\n", node->debug_id);
				return -EINVAL;
			}
			list_add_tail(&node->work.entry, target_list);
		}
	}
	return 0;
}
static int binder_dec_node(struct binder_node *node, int strong, int internal)
{
	if (strong) {
		if (internal)
			node->internal_strong_refs--;
		else
			node->local_strong_refs--;
		if (node->local_strong_refs || node->internal_strong_refs)
			return 0;
	} else {
		if (!internal)
			node->local_weak_refs--;
		if (node->local_weak_refs || !hlist_empty(&node->refs))
			return 0;
	}
	if (node->proc && (node->has_strong_ref || node->has_weak_ref)) {
		if (list_empty(&node->work.entry)) {
			list_add_tail(&node->work.entry, &node->proc->todo);
			wake_up_interruptible(&node->proc->wait);
		}
	} else {
		if (hlist_empty(&node->refs) && !node->local_strong_refs &&
		    !node->local_weak_refs) {
			list_del_init(&node->work.entry);
			if (node->proc) {
				rb_erase(&node->rb_node, &node->proc->nodes);
				binder_debug(BINDER_DEBUG_INTERNAL_REFS,
					     "binder: refless node %d deleted\n",
					     node->debug_id);
			} else {
				hlist_del(&node->dead_node);
				binder_debug(BINDER_DEBUG_INTERNAL_REFS,
					     "binder: dead node %d deleted\n",
					     node->debug_id);
			}
			kfree(node);
			binder_stats_deleted(BINDER_STAT_NODE);
		}
	}
	return 0;
}
static struct binder_ref *binder_get_ref(struct binder_proc *proc,
					 uint32_t desc)
{
	struct rb_node *n = proc->refs_by_desc.rb_node;
	struct binder_ref *ref;
	while (n) {
		ref = rb_entry(n, struct binder_ref, rb_node_desc);
		if (desc < ref->desc)
			n = n->rb_left;
		else if (desc > ref->desc)
			n = n->rb_right;
		else
			return ref;
	}
	return NULL;
}
static struct binder_ref *binder_get_ref_for_node(struct binder_proc *proc,
						  struct binder_node *node)
{
	struct rb_node *n;
	struct rb_node **p = &proc->refs_by_node.rb_node;
	struct rb_node *parent = NULL;
	struct binder_ref *ref, *new_ref;
	while (*p) {
		parent = *p;
		ref = rb_entry(parent, struct binder_ref, rb_node_node);
		if (node < ref->node)
			p = &(*p)->rb_left;
		else if (node > ref->node)
			p = &(*p)->rb_right;
		else
			return ref;
	}
	new_ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (new_ref == NULL)
		return NULL;
	binder_stats_created(BINDER_STAT_REF);
	new_ref->debug_id = ++binder_last_id;
	new_ref->proc = proc;
	new_ref->node = node;
	rb_link_node(&new_ref->rb_node_node, parent, p);
	rb_insert_color(&new_ref->rb_node_node, &proc->refs_by_node);
	// 是不是ServiceManager的
	new_ref->desc = (node == binder_context_mgr_node) ? 0 : 1;

	for (n = rb_first(&proc->refs_by_desc); n != NULL; n = rb_next(n)) {
		ref = rb_entry(n, struct binder_ref, rb_node_desc);
		if (ref->desc > new_ref->desc)
			break;
		new_ref->desc = ref->desc + 1;
	}
	p = &proc->refs_by_desc.rb_node;
	while (*p) {
		parent = *p;
		ref = rb_entry(parent, struct binder_ref, rb_node_desc);
		if (new_ref->desc < ref->desc)
			p = &(*p)->rb_left;
		else if (new_ref->desc > ref->desc)
			p = &(*p)->rb_right;
		else
			BUG();
	}
	rb_link_node(&new_ref->rb_node_desc, parent, p);
	rb_insert_color(&new_ref->rb_node_desc, &proc->refs_by_desc);
	if (node) {
		hlist_add_head(&new_ref->node_entry, &node->refs);
		binder_debug(BINDER_DEBUG_INTERNAL_REFS,
			     "binder: %d new ref %d desc %d for "
			     "node %d\n", proc->pid, new_ref->debug_id,
			     new_ref->desc, node->debug_id);
	} else {
		binder_debug(BINDER_DEBUG_INTERNAL_REFS,
			     "binder: %d new ref %d desc %d for "
			     "dead node\n", proc->pid, new_ref->debug_id,
			      new_ref->desc);
	}
	return new_ref;
}
static void binder_delete_ref(struct binder_ref *ref)
{
	binder_debug(BINDER_DEBUG_INTERNAL_REFS,
		     "binder: %d delete ref %d desc %d for "
		     "node %d\n", ref->proc->pid, ref->debug_id,
		     ref->desc, ref->node->debug_id);
	rb_erase(&ref->rb_node_desc, &ref->proc->refs_by_desc);
	rb_erase(&ref->rb_node_node, &ref->proc->refs_by_node);
	if (ref->strong)
		binder_dec_node(ref->node, 1, 1);
	hlist_del(&ref->node_entry);
	binder_dec_node(ref->node, 0, 1);
	if (ref->death) {
		binder_debug(BINDER_DEBUG_DEAD_BINDER,
			     "binder: %d delete ref %d desc %d "
			     "has death notification\n", ref->proc->pid,
			     ref->debug_id, ref->desc);
		list_del(&ref->death->work.entry);
		kfree(ref->death);
		binder_stats_deleted(BINDER_STAT_DEATH);
	}
	kfree(ref);
	binder_stats_deleted(BINDER_STAT_REF);
}
static int binder_inc_ref(struct binder_ref *ref, int strong,
			  struct list_head *target_list)
{
	int ret;
	if (strong) {
		if (ref->strong == 0) {
			ret = binder_inc_node(ref->node, 1, 1, target_list);
			if (ret)
				return ret;
		}
		ref->strong++;
	} else {
		if (ref->weak == 0) {
			ret = binder_inc_node(ref->node, 0, 1, target_list);
			if (ret)
				return ret;
		}
		ref->weak++;
	}
	return 0;
}
static int binder_dec_ref(struct binder_ref *ref, int strong)
{
	if (strong) {
		if (ref->strong == 0) {
			binder_user_error("binder: %d invalid dec strong, "
					  "ref %d desc %d s %d w %d\n",
					  ref->proc->pid, ref->debug_id,
					  ref->desc, ref->strong, ref->weak);
			return -EINVAL;
		}
		ref->strong--;
		if (ref->strong == 0) {
			int ret;
			ret = binder_dec_node(ref->node, strong, 1);
			if (ret)
				return ret;
		}
	} else {
		if (ref->weak == 0) {
			binder_user_error("binder: %d invalid dec weak, "
					  "ref %d desc %d s %d w %d\n",
					  ref->proc->pid, ref->debug_id,
					  ref->desc, ref->strong, ref->weak);
			return -EINVAL;
		}
		ref->weak--;
	}
	if (ref->strong == 0 && ref->weak == 0)
		binder_delete_ref(ref);
	return 0;
}
static void binder_pop_transaction(struct binder_thread *target_thread,
				   struct binder_transaction *t)
{
	// 这里只是简单的出栈操作
	if (target_thread) {
		BUG_ON(target_thread->transaction_stack != t);
		BUG_ON(target_thread->transaction_stack->from != target_thread);
		// 出栈操作，frome->parent往下移动一层
		target_thread->transaction_stack =
			target_thread->transaction_stack->from_parent;
		t->from = NULL;
	}
	t->need_reply = 0;
	if (t->buffer)
		t->buffer->transaction = NULL;
	kfree(t);
	binder_stats_deleted(BINDER_STAT_TRANSACTION);
}
static void binder_send_failed_reply(struct binder_transaction *t,
				     uint32_t error_code)
{
	struct binder_thread *target_thread;
	BUG_ON(t->flags & TF_ONE_WAY);
	while (1) {
		target_thread = t->from;
		if (target_thread) {
			if (target_thread->return_error != BR_OK &&
			   target_thread->return_error2 == BR_OK) {
				target_thread->return_error2 =
					target_thread->return_error;
				target_thread->return_error = BR_OK;
			}
			if (target_thread->return_error == BR_OK) {
				binder_debug(BINDER_DEBUG_FAILED_TRANSACTION,
					     "binder: send failed reply for "
					     "transaction %d to %d:%d\n",
					      t->debug_id, target_thread->proc->pid,
					      target_thread->pid);
				binder_pop_transaction(target_thread, t);
				target_thread->return_error = error_code;
				wake_up_interruptible(&target_thread->wait);
			} else {
				printk(KERN_ERR "binder: reply failed, target "
					"thread, %d:%d, has error code %d "
					"already\n", target_thread->proc->pid,
					target_thread->pid,
					target_thread->return_error);
			}
			return;
		} else {
			struct binder_transaction *next = t->from_parent;
			binder_debug(BINDER_DEBUG_FAILED_TRANSACTION,
				     "binder: send failed reply "
				     "for transaction %d, target dead\n",
				     t->debug_id);
			binder_pop_transaction(target_thread, t);
			if (next == NULL) {
				binder_debug(BINDER_DEBUG_DEAD_BINDER,
					     "binder: reply failed,"
					     " no target thread at root\n");
				return;
			}
			t = next;
			binder_debug(BINDER_DEBUG_DEAD_BINDER,
				     "binder: reply failed, no target "
				     "thread -- retry %d\n", t->debug_id);
		}
	}
}
static void binder_transaction_buffer_release(struct binder_proc *proc,
					      struct binder_buffer *buffer,
					      size_t *failed_at)
{
	size_t *offp, *off_end;
	int debug_id = buffer->debug_id;
	binder_debug(BINDER_DEBUG_TRANSACTION,
		     "binder: %d buffer release %d, size %zd-%zd, failed at %p\n",
		     proc->pid, buffer->debug_id,
		     buffer->data_size, buffer->offsets_size, failed_at);
	if (buffer->target_node)
		binder_dec_node(buffer->target_node, 1, 0);
	offp = (size_t *)(buffer->data + ALIGN(buffer->data_size, sizeof(void *)));
	if (failed_at)
		off_end = failed_at;
	else
		off_end = (void *)offp + buffer->offsets_size;
	for (; offp < off_end; offp++) {
		struct flat_binder_object *fp;
		if (*offp > buffer->data_size - sizeof(*fp) ||
		    buffer->data_size < sizeof(*fp) ||
		    !IS_ALIGNED(*offp, sizeof(void *))) {
			printk(KERN_ERR "binder: transaction release %d bad"
					"offset %zd, size %zd\n", debug_id,
					*offp, buffer->data_size);
			continue;
		}
		fp = (struct flat_binder_object *)(buffer->data + *offp);
		switch (fp->type) {
		case BINDER_TYPE_BINDER:
		case BINDER_TYPE_WEAK_BINDER: {
			struct binder_node *node = binder_get_node(proc, fp->binder);
			if (node == NULL) {
				printk(KERN_ERR "binder: transaction release %d"
				       " bad node %p\n", debug_id, fp->binder);
				break;
			}
			binder_debug(BINDER_DEBUG_TRANSACTION,
				     "        node %d u%p\n",
				     node->debug_id, node->ptr);
			binder_dec_node(node, fp->type == BINDER_TYPE_BINDER, 0);
		} break;
		case BINDER_TYPE_HANDLE:
		case BINDER_TYPE_WEAK_HANDLE: {
			struct binder_ref *ref = binder_get_ref(proc, fp->handle);
			if (ref == NULL) {
				printk(KERN_ERR "binder: transaction release %d"
				       " bad handle %ld\n", debug_id,
				       fp->handle);
				break;
			}
			binder_debug(BINDER_DEBUG_TRANSACTION,
				     "        ref %d desc %d (node %d)\n",
				     ref->debug_id, ref->desc, ref->node->debug_id);
			binder_dec_ref(ref, fp->type == BINDER_TYPE_HANDLE);
		} break;
		case BINDER_TYPE_FD:
			binder_debug(BINDER_DEBUG_TRANSACTION,
				     "        fd %ld\n", fp->handle);
			if (failed_at)
				task_close_fd(proc, fp->handle);
			break;
		default:
			printk(KERN_ERR "binder: transaction release %d bad "
			       "object type %lx\n", debug_id, fp->type);
			break;
		}
	}
}
// 这个函数只会在binder_thread_write中调用
// 调用该函数传递下去的参数有：当前task所在进程在binder驱动中对应的binder_proc结构体、
// 和当前task在binder驱动中对应的binder_thread结构体；struct binder_transaction_data结构体指针，
// 最后一个参数表示当前发送的是请求还是回复。
// 这个函数将发送请求和发送回复放在了一个函数，而且包含了数据包中有binder在传输的处理情况，
// 所以显得很复杂，不太容易看懂，大概有400多行吧！
static void binder_transaction(struct binder_proc *proc,
			       struct binder_thread *thread,
			       struct binder_transaction_data *tr, int reply)
{
	// // 发送请求时使用的内核传输数据结构
	struct binder_transaction *t;
	struct binder_work *tcomplete;
	size_t *offp, *off_end;
	 // 目标进程对应的binder_proc
	struct binder_proc *target_proc;
	// 目标线程对应的binder_thread
	struct binder_thread *target_thread = NULL;
	//目标binder实体在内核中的节点结构体
	struct binder_node *target_node = NULL;
	// 
	struct list_head *target_list;
	wait_queue_head_t *target_wait;
	struct binder_transaction *in_reply_to = NULL;
	struct binder_transaction_log_entry *e;
	uint32_t return_error;
	e = binder_transaction_log_add(&binder_transaction_log);
	e->call_type = reply ? 2 : !!(tr->flags & TF_ONE_WAY);
	e->from_proc = proc->pid;
	e->from_thread = thread->pid;
	e->target_handle = tr->target.handle;
	e->data_size = tr->data_size;
	e->offsets_size = tr->offsets_size;
	// 如过是返回
	if (reply) {
		// 这里的in_reply_to其实是在Server进程或者线程
		in_reply_to = thread->transaction_stack;
	   // ￥
		binder_set_nice(in_reply_to->saved_priority);
		// 这里是判断是否是正确的返回线程
		if (in_reply_to->to_thread != thread) {
         // ￥
		}
		thread->transaction_stack = in_reply_to->to_parent;
		// 获取目标线程，这里是在请求发送是写入的
		target_thread = in_reply_to->from;
 
		if (target_thread->transaction_stack != in_reply_to) {
        //$
		}
		// 根据目标线程获取目标进程
		target_proc = target_thread->proc;
	} 
	   // 如过是请求
	else {
		// 如果这里目标服务是普通服务 不是SMgr管理类进程 
		if (tr->target.handle) {
			struct binder_ref *ref;
	    // 从proc进程中，这里其实是自己的进程，查询出bind_node的引用bind_ref,
		// 对于一般的进程，会在getService的时候有Servicemanager为自己添加
		// 注意handle 是记录在本地的，用来本地索引ref用的 
			ref = binder_get_ref(proc, tr->target.handle);
		//￥
			target_node = ref->node;
		} else {
		// 如果这里目标服务是Servicemanager服务 
		// 如果ioctl想和SMgr的binder实体建立联系，需要使tr->target.handle = 0
			target_node = binder_context_mgr_node;
	    //￥
		}
		e->to_node = target_node->debug_id;
		// 获取目标进程
		target_proc = target_node->proc;
	    //￥ 错误处理
	    // 这里如何获取 target->thread（如果有的化）， 目标进程或者线程，ref 对应node会有记录
		if (!(tr->flags & TF_ONE_WAY) && thread->transaction_stack) {
			struct binder_transaction *tmp;
			// 目前肯定只有自己，因为阻塞，只能有自己
			tmp = thread->transaction_stack;
			if (tmp->to_thread != thread) {
		    // $
			}

/*			 这里尝试解释一下 http://blog.csdn.net/universus/article/details/6211589 不知道是不是正确
			 关于工作线程的启动，Binder驱动还做了一点小小的优化。当进程P1的线程T1向进程P2发送请求时，
			 驱动会先查看一下线程T1是否也正在处理来自P2某个线程请求但尚未完成（没有发送回复）。这种情况
			  通常发生在两个进程都有Binder实体并互相对发时请求时。假如驱动在进程P2中发现了这样的线程，比如说T2
			 ，就会要求T2来处理T1的这次请求。因为T2既然向T1发送了请求尚未得到返回包，说明T2肯定（或将会）阻塞在读取返
			  回包的状态。这时候可以让T2顺便做点事情，总比等在那里闲着好。
			  而且如果T2不是线程池中的线程还可以为线程池分担部分工作，减少线程池使用率。*/

/*			规则1：Client发给Server的请求数据包都提交到Server进程的全局to-do队列。不过有个特例，就
			是上节谈到的Binder对工作线程启动的优化。经过优化，来自T1的请求不是提交给P2的全局to-do队列，而是送入了T2
			的私有to-do队列。规则2：对同步请求的返回数据包（由BC_REPLY发送的包）都发送到发起请求的线程的私有to-do队列
			中。如上面的例子，如果进程P1的线程T1发给进程P2的线程T2的是同步请求，那么T2返回的数据包将送进T1的私有to-do队
			列而不会提交到P1的全局to-do队列。
			数据包进入接收队列的潜规则也就决定了线程进入等待队列的潜规则，
			即一个线程只要不接收返回数据包则应该在全局等待队列中等待新任务，
			否则就应该在其私有等待队列中等待Server的返回数据。还是上面的例子，
			T1在向T2发送同步请求后就必须等待在它私有等待队列中，
			而不是在P1的全局等待队列中排队，否则将得不到T2的返回的数据包。

			只有相互请求，才会在请求的时候发送到某个特定的线程。
*/			while (tmp) {
				if (tmp->from && tmp->from->proc == target_proc)
					target_thread = tmp->from;
				tmp = tmp->from_parent;
			}
		}
	}
	// 这里如果目标是线程，那么唤醒的就是线程的等待队列，如果是进程，就唤醒进程等待队列
	// 其实唤醒的时候，对应是queuen ，
	if (target_thread) {
		e->to_thread = target_thread->pid;
		target_list = &target_thread->todo;
		target_wait = &target_thread->wait;
	} else {
		target_list = &target_proc->todo;
		target_wait = &target_proc->wait;
	}
	e->to_proc = target_proc->pid;
	/* TODO: reuse incoming transaction for reply 如何复用incoming transaction*/
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	//￥
	binder_stats_created(BINDER_STAT_TRANSACTION);
	//分配本次单边传输需要使用的binder_work结构体内存
	tcomplete = kzalloc(sizeof(*tcomplete), GFP_KERNEL);
	//￥
	binder_stats_created(BINDER_STAT_TRANSACTION_COMPLETE);
	t->debug_id = ++binder_last_id;
	e->debug_id = t->debug_id;
    //￥ ;
    //   reply 是不是回复 如果是同步传输的发送边，这里将当前的binder_thre
	if (!reply && !(tr->flags & TF_ONE_WAY))
	//  如果是同步传输的发送边，这里将当前的binder_thread记录
	// 在binder_transaction.from中，以供在同步传输的回复边时，驱动可以根据这个找到回复的目的task。
		t->from = thread;
	else
		/* 如果是BC_REPLY或者是异步传输，这里不需要记录和返回信息相关的信息。*/
		t->from = NULL;

	t->sender_euid = proc->tsk->cred->euid;
	//事务的目标进程  
	t->to_proc = target_proc;    
    //事物的目标线程  
    t->to_thread = target_thread;
	 // 这个保持不变，驱动不会关心它
	t->code = tr->code;
	t->flags = tr->flags;
	//线程的优先级的迁移
	t->priority = task_nice(current);

	trace_binder_transaction(reply, t, target_node);
	
	/**
	在通过进行binder事物的传递时，如果一个binder事物（用struct binder_transaction结构体表示）需要使用到内存，
	就会调用binder_alloc_buf函数分配此次binder事物需要的内存空间。
	需要注意的是：从目标进程所在binder内存空间分配所需的内存
	*/
	//从target进程的binder内存空间分配所需的内存大小,这也是一次拷贝，完成通信的关键，直接拷贝到目标进程的内核空间
	//由于用户空间跟内核空间仅仅存在一个偏移地址，所以也算拷贝到用户空间
	t->buffer = binder_alloc_buf(target_proc, tr->data_size,
		tr->offsets_size, !reply && (t->flags & TF_ONE_WAY));
    //￥
	t->buffer->allow_user_free = 0;
	t->buffer->debug_id = t->debug_id;
	//该binder_buffer对应的事务    
	t->buffer->transaction = t;
	//该事物对应的目标binder实体 ,因为目标进程中可能不仅仅有一个Binder实体
	t->buffer->target_node = target_node;

	trace_binder_transaction_alloc_buf(t->buffer);
	// 
	if (target_node)
		binder_inc_node(target_node, 1, 0, NULL);
	// 计算出存放flat_binder_object结构体偏移数组的起始地址，4字节对齐。
	offp = (size_t *)(t->buffer->data + ALIGN(tr->data_size, sizeof(void *)));
	   /* struct flat_binder_object是binder在进程之间传输的表示方式 */
       /* 这里就是完成binder通讯单边时候在用户进程同内核buffer之间的一次拷贝动作 */
	  // 这里的数据拷贝，其实是拷贝到目标进程中去，因为t本身就是在目标进程的内核空间中分配的，
	if (copy_from_user(t->buffer->data, tr->data.ptr.buffer, tr->data_size)) {
		binder_user_error("binder: %d:%d got transaction with invalid "
			"data ptr\n", proc->pid, thread->pid);
		return_error = BR_FAILED_REPLY;
		goto err_copy_data_failed;
	}
	 // 拷贝内嵌在传输数据中的flat_binder_object结构体偏移数组
	if (copy_from_user(offp, tr->data.ptr.offsets, tr->offsets_size)) {
    //￥
	}
	if (!IS_ALIGNED(tr->offsets_size, sizeof(size_t))) {
 	// ￥
	}
	//这里不一定一定执行，比如 MediaPlayer getMediaplayerservie的时候，传递数据中不包含对象, off_end为null 
	// 这里就不会执行，一定要携带传输的数据才会走到这里
	//off_end = (void *)offp + tr->offsets_size; 
	//flat_binder_object结构体偏移数组的结束地址
	off_end = (void *)offp + tr->offsets_size;
	for (; offp < off_end; offp++) {
		struct flat_binder_object *fp;
   /* *offp是t->buffer中第一个flat_binder_object结构体的位置偏移,相
   当于t->buffer->data的偏移，这里的if语句用来判断binder偏移数组的第一个
   元素所指向的flat_binder_object结构体地址是否是在t->buffer->data的有效范
   围内，或者数据的总大小就小于一个flat_binder_object的大小，或者说这个数组的元
   素根本就没有4字节对齐(一个指针在32位平台上用4个字节表示)。*/
		if (*offp > t->buffer->data_size - sizeof(*fp) ||
		    t->buffer->data_size < sizeof(*fp) ||
		    !IS_ALIGNED(*offp, sizeof(void *))) {
			 // $
		}
		// 取得第一个flat_binder_object结构体指针
		fp = (struct flat_binder_object *)(t->buffer->data + *offp);
		switch (fp->type) {
        // 只有具有binder实体的进程才有权利发送这类binder。这里其实是在binder实体中
		case BINDER_TYPE_BINDER:
		case BINDER_TYPE_WEAK_BINDER: {
			struct binder_ref *ref;
			// 根据flat_binder_object.binder这个binder实体在进程间的地
			// 址搜索当前进程的binder_proc->nodes红黑树，看看是否已经创建了binder_node内核节点
			// 如果没创建，在自己的进程中为自己创建node节点 
			struct binder_node *node = binder_get_node(proc, fp->binder);
			if (node == NULL) {
				// 如果为空，创建
				node = binder_new_node(proc, fp->binder, fp->cookie);
				if (node == NULL) {
					return_error = BR_FAILED_REPLY;
					goto err_binder_new_node_failed;
				}
				node->min_priority = fp->flags & FLAT_BINDER_FLAG_PRIORITY_MASK;
				node->accept_fds = !!(fp->flags & FLAT_BINDER_FLAG_ACCEPTS_FDS);
			}
			// 校验
			if (fp->cookie != node->cookie) {
			 // #
			}
			if (security_binder_transfer_binder(proc->tsk, target_proc->tsk)) {
				return_error = BR_FAILED_REPLY;
				goto err_binder_get_ref_for_node_failed;
			}
			// 在目标进程中为它创建引用，其实类似（在ServiceManager中创建bind_ref其实可以说，Servicemanager拥有全部Service的引用）
			ref = binder_get_ref_for_node(target_proc, node);
 			// ￥
 			// 修改flat_binder_object数据结构的type和handle域，接下来要传给接收方
			if (fp->type == BINDER_TYPE_BINDER)
				fp->type = BINDER_TYPE_HANDLE;
			else
				fp->type = BINDER_TYPE_WEAK_HANDLE;
			fp->handle = ref->desc;
			binder_inc_ref(ref, fp->type == BINDER_TYPE_HANDLE,
				       &thread->todo);
			trace_binder_transaction_node_to_ref(t, node, ref);
			binder_debug(BINDER_DEBUG_TRANSACTION,
				     "        node %d u%p -> ref %d desc %d\n",
				     node->debug_id, node->ptr, ref->debug_id,
				     ref->desc);
		} break;
		case BINDER_TYPE_HANDLE:
		case BINDER_TYPE_WEAK_HANDLE: {
		     // 通过引用号取得当前进程中对应的binder_ref结构体
			struct binder_ref *ref = binder_get_ref(proc, fp->handle);
	         // ￥
			if (security_binder_transfer_binder(proc->tsk, target_proc->tsk)) {
				return_error = BR_FAILED_REPLY;
				goto err_binder_get_ref_failed;
			}
			/* 如果目标进程正好是提供该引用号对应的binder实体的进程，那么按照下面的方式
			修改flat_binder_object的相应域: type 和 binder，cookie。*/ 
			// 不太理解？？？
			if (ref->node->proc == target_proc) {
				if (fp->type == BINDER_TYPE_HANDLE)
					fp->type = BINDER_TYPE_BINDER;
				else
					fp->type = BINDER_TYPE_WEAK_BINDER;
				fp->binder = ref->node->ptr;
				fp->cookie = ref->node->cookie;
				binder_inc_node(ref->node, fp->type == BINDER_TYPE_BINDER, 0, NULL);
				trace_binder_transaction_ref_to_node(t, ref);
				 // $
			} else {
 			/* 否则会在目标进程的refs_by_node红黑树中先搜索看是否之前有创建过对应的b
 			inder_ref，如果没有找到，那么就需要为ref->node节点在目标进程中新建一个目标进程的bi
 			nder_ref挂入红黑树refs_by_node中。*/
				struct binder_ref *new_ref;
				new_ref = binder_get_ref_for_node(target_proc, ref->node);
				if (new_ref == NULL) {
					return_error = BR_FAILED_REPLY;
					goto err_binder_get_ref_for_node_failed;
				}
				//此时只需要将此域修改为新建binder_ref的引用号
				fp->handle = new_ref->desc;
				binder_inc_ref(new_ref, fp->type == BINDER_TYPE_HANDLE, NULL);
				trace_binder_transaction_ref_to_ref(t, ref,
								    new_ref);
			}
		} break;
		case BINDER_TYPE_FD: {
			int target_fd;
			struct file *file;
			if (reply) {
				if (!(in_reply_to->flags & TF_ACCEPT_FDS)) {
					binder_user_error("binder: %d:%d got reply with fd, %ld, but target does not allow fds\n",
						proc->pid, thread->pid, fp->handle);
					return_error = BR_FAILED_REPLY;
					goto err_fd_not_allowed;
				}
			} else if (!target_node->accept_fds) {
				binder_user_error("binder: %d:%d got transaction with fd, %ld, but target does not allow fds\n",
					proc->pid, thread->pid, fp->handle);
				return_error = BR_FAILED_REPLY;
				goto err_fd_not_allowed;
			}
			file = fget(fp->handle);
			if (file == NULL) {
				binder_user_error("binder: %d:%d got transaction with invalid fd, %ld\n",
					proc->pid, thread->pid, fp->handle);
				return_error = BR_FAILED_REPLY;
				goto err_fget_failed;
			}
			if (security_binder_transfer_file(proc->tsk, target_proc->tsk, file) < 0) {
				fput(file);
				return_error = BR_FAILED_REPLY;
				goto err_get_unused_fd_failed;
			}
			target_fd = task_get_unused_fd_flags(target_proc, O_CLOEXEC);
			if (target_fd < 0) {
				fput(file);
				return_error = BR_FAILED_REPLY;
				goto err_get_unused_fd_failed;
			}
			task_fd_install(target_proc, target_fd, file);
			trace_binder_transaction_fd(t, fp->handle, target_fd);
			binder_debug(BINDER_DEBUG_TRANSACTION,
				     "        fd %ld -> %d\n", fp->handle, target_fd);
			/* TODO: fput? */
			fp->handle = target_fd;
		} break;
		default:
	 
			goto err_bad_object_type;
		}
	}
if (reply) {
		BUG_ON(t->buffer->async_transaction != 0);
		// 这里是出栈操作
		binder_pop_transaction(target_thread, in_reply_to);

	} 
	// 如果是同步传输，请求方需要获取返回
	else if (!(t->flags & TF_ONE_WAY)) {
		BUG_ON(t->buffer->async_transaction != 0);
		// 需要带返回数据的标记
		t->need_reply = 1;
		// binder_transaction的的上一层任务，由于是堆栈，所以抽象为parent，惹歧义啊！这里是入栈操作
		t->from_parent = thread->transaction_stack;
		// 正在处理的事务栈，为何会有事务栈呢？因为在等待返回的过程中，还会有事务插入进去，比如null的reply
		thread->transaction_stack = t;
  
	/* 

	如果本次发起传输之前，当前task没有处于通讯过程中的话，这里必然为
	NULL。而且第一次发起传输时，这里也是为NULl。如果之前有异步传输没处理完，没处理完，就还没有release，
	那么这里不为null（为了自己，因为自己之前的任务还在执行，还没完），如果之前本task正在处理接收请求，这里也不为NULL(为了其他进程)，
    这里将传输中间数据结构保存在binder_transaction链表顶部。这个transaction_stack实际
	上是管理这一个链表，只不过这个指针时时指向最新加入该链表的成员，最先加入的成员在最底部，有点类似于
	stack，所以这里取名叫transaction_stack。

	*/
	} 
	// 异步传输，不需要返回
	else {
		// 不需要返回的情况下 ，下面是对异步通讯的分流处理
		BUG_ON(target_node == NULL);
		BUG_ON(t->buffer->async_transaction != 1);
		/* 如果目标task仍然还有一个异步需要处理的话，该标志为1。*/
		if (target_node->has_async_transaction) {
			/* 将当前的这个的异步传输任务转入目标进程binder_node的异步等待队列async_todo中。为了加到异步中秋*/ 
			target_list = &target_node->async_todo;
			// 将后来的异步交互转入异步等待队列。就不唤醒了，因为有在执行的
			target_wait = NULL;
		} else
		// 如果没有，设置目标节点存在异步任务，设置也是在这里设置的，
			target_node->has_async_transaction = 1;
	}
	//
	t->work.type = BINDER_WORK_TRANSACTION;
	// 添加到目标的任务列表
	list_add_tail(&t->work.entry, target_list);
	tcomplete->type = BINDER_WORK_TRANSACTION_COMPLETE;
	// 添加到请求线程，也就是自己的待处理任务列表
	list_add_tail(&tcomplete->entry, &thread->todo);
	// 唤醒目标进程，查看是否需要唤醒，如果target_wait！=null
	/* 当前task等待在task自己的等待队列中(binder_thread.todo)，永远只有其自己。*/
	if (target_wait)
		wake_up_interruptible(target_wait);
	return;
 	//err: ￥异常处理 。。。
}
int binder_thread_write(struct binder_proc *proc, struct binder_thread *thread,
			void __user *buffer, int size, signed long *consumed)
{
/* ioctl传进来的只是结构体binder_write_read，而对于其中write和read的buffer却还是存在于用户空间，
  下面用get_user来获取用户空间buffer中的值 */
	uint32_t cmd;
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;
	// 一次ioctl的发送过程，可以在bwr.write_buffer中按照格式：命令+参数，
	// 组织多个命令包发送给接收方，所以这里使用了循环的方式来一个一个地处理这些命令包。
	while (ptr < end && thread->return_error == BR_OK) {
		// 取得用户空间buffer中每个命令包的命令字
		if (get_user(cmd, (uint32_t __user *)ptr))
			return -EFAULT;
		  // ptr 指针前移，命令字是一个字的长度
		ptr += sizeof(uint32_t);
		trace_binder_command(cmd);
		 // 提取出cmd中的命令序号，对全局统计数据结构中bc对应命令使用域加1
		if (_IOC_NR(cmd) < ARRAY_SIZE(binder_stats.bc)) {
			binder_stats.bc[_IOC_NR(cmd)]++;
			proc->stats.bc[_IOC_NR(cmd)]++;
			thread->stats.bc[_IOC_NR(cmd)]++;
		}
		switch (cmd) {
		case BC_INCREFS:
		case BC_ACQUIRE:
		case BC_RELEASE:
		case BC_DECREFS: {
			uint32_t target;
			struct binder_ref *ref;
			const char *debug_string;
			if (get_user(target, (uint32_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(uint32_t);
			if (target == 0 && binder_context_mgr_node &&
			    (cmd == BC_INCREFS || cmd == BC_ACQUIRE)) {
				ref = binder_get_ref_for_node(proc,
					       binder_context_mgr_node);
				if (ref->desc != target) {
					binder_user_error("binder: %d:"
						"%d tried to acquire "
						"reference to desc 0, "
						"got %d instead\n",
						proc->pid, thread->pid,
						ref->desc);
				}
			} else
				ref = binder_get_ref(proc, target);
			if (ref == NULL) {
				binder_user_error("binder: %d:%d refcou"
					"nt change on invalid ref %d\n",
					proc->pid, thread->pid, target);
				break;
			}
			switch (cmd) {
			case BC_INCREFS:
				debug_string = "IncRefs";
				binder_inc_ref(ref, 0, NULL);
				break;
			case BC_ACQUIRE:
				debug_string = "Acquire";
				binder_inc_ref(ref, 1, NULL);
				break;
			case BC_RELEASE:
				debug_string = "Release";
				binder_dec_ref(ref, 1);
				break;
			case BC_DECREFS:
			default:
				debug_string = "DecRefs";
				binder_dec_ref(ref, 0);
				break;
			}
			binder_debug(BINDER_DEBUG_USER_REFS,
				     "binder: %d:%d %s ref %d desc %d s %d w %d for node %d\n",
				     proc->pid, thread->pid, debug_string, ref->debug_id,
				     ref->desc, ref->strong, ref->weak, ref->node->debug_id);
			break;
		}
		case BC_INCREFS_DONE:
		case BC_ACQUIRE_DONE: {
			void __user *node_ptr;
			void *cookie;
			struct binder_node *node;
			if (get_user(node_ptr, (void * __user *)ptr))
				return -EFAULT;
			ptr += sizeof(void *);
			if (get_user(cookie, (void * __user *)ptr))
				return -EFAULT;
			ptr += sizeof(void *);
			node = binder_get_node(proc, node_ptr);
			if (node == NULL) {
				binder_user_error("binder: %d:%d "
					"%s u%p no match\n",
					proc->pid, thread->pid,
					cmd == BC_INCREFS_DONE ?
					"BC_INCREFS_DONE" :
					"BC_ACQUIRE_DONE",
					node_ptr);
				break;
			}
			if (cookie != node->cookie) {
				binder_user_error("binder: %d:%d %s u%p node %d"
					" cookie mismatch %p != %p\n",
					proc->pid, thread->pid,
					cmd == BC_INCREFS_DONE ?
					"BC_INCREFS_DONE" : "BC_ACQUIRE_DONE",
					node_ptr, node->debug_id,
					cookie, node->cookie);
				break;
			}
			if (cmd == BC_ACQUIRE_DONE) {
				if (node->pending_strong_ref == 0) {
					binder_user_error("binder: %d:%d "
						"BC_ACQUIRE_DONE node %d has "
						"no pending acquire request\n",
						proc->pid, thread->pid,
						node->debug_id);
					break;
				}
				node->pending_strong_ref = 0;
			} else {
				if (node->pending_weak_ref == 0) {
					binder_user_error("binder: %d:%d "
						"BC_INCREFS_DONE node %d has "
						"no pending increfs request\n",
						proc->pid, thread->pid,
						node->debug_id);
					break;
				}
				node->pending_weak_ref = 0;
			}
			binder_dec_node(node, cmd == BC_ACQUIRE_DONE, 0);
			binder_debug(BINDER_DEBUG_USER_REFS,
				     "binder: %d:%d %s node %d ls %d lw %d\n",
				     proc->pid, thread->pid,
				     cmd == BC_INCREFS_DONE ? "BC_INCREFS_DONE" : "BC_ACQUIRE_DONE",
				     node->debug_id, node->local_strong_refs, node->local_weak_refs);
			break;
		}
		case BC_ATTEMPT_ACQUIRE:
			printk(KERN_ERR "binder: BC_ATTEMPT_ACQUIRE not supported\n");
			return -EINVAL;
		case BC_ACQUIRE_RESULT:
			printk(KERN_ERR "binder: BC_ACQUIRE_RESULT not supported\n");
			return -EINVAL;
		case BC_FREE_BUFFER: {
			void __user *data_ptr;
			struct binder_buffer *buffer;
			if (get_user(data_ptr, (void * __user *)ptr))
				return -EFAULT;
			ptr += sizeof(void *);
			buffer = binder_buffer_lookup(proc, data_ptr);
			if (buffer == NULL) {
				binder_user_error("binder: %d:%d "
					"BC_FREE_BUFFER u%p no match\n",
					proc->pid, thread->pid, data_ptr);
				break;
			}
			if (!buffer->allow_user_free) {
				binder_user_error("binder: %d:%d "
					"BC_FREE_BUFFER u%p matched "
					"unreturned buffer\n",
					proc->pid, thread->pid, data_ptr);
				break;
			}
			binder_debug(BINDER_DEBUG_FREE_BUFFER,
				     "binder: %d:%d BC_FREE_BUFFER u%p found buffer %d for %s transaction\n",
				     proc->pid, thread->pid, data_ptr, buffer->debug_id,
				     buffer->transaction ? "active" : "finished");
			if (buffer->transaction) {
				buffer->transaction->buffer = NULL;
				buffer->transaction = NULL;
			}
			if (buffer->async_transaction && buffer->target_node) {
				BUG_ON(!buffer->target_node->has_async_transaction);
				if (list_empty(&buffer->target_node->async_todo))
					buffer->target_node->has_async_transaction = 0;
				else
					list_move_tail(buffer->target_node->async_todo.next, &thread->todo);
			}
			trace_binder_transaction_buffer_release(buffer);
			binder_transaction_buffer_release(proc, buffer, NULL);
			binder_free_buf(proc, buffer);
			break;
		}
		case BC_TRANSACTION:
		case BC_REPLY: {
   /* 
    理解binder驱动的关键之一在于认清下面两个结构体的区别和联系：
    struct  binder_transaction_data 和struct binder_transaction ，
    前者是用于在用户空间中表示传输的数据，而后者是binder驱动在内核空间中
    来表示传输的数据，接下来所做的工作很大部分就是完成前者向后者转换，而对于
    binder读取函数binder_thread_read()则主要是完成从后者向前者转换。
    */
			struct binder_transaction_data tr;
			// 先将传输数据结构从用户空间拷贝到内核空间
			if (copy_from_user(&tr, ptr, sizeof(tr)))
				return -EFAULT;
			// 指针向前移动
			ptr += sizeof(tr);
			// cmd == BC_REPLY 就表示需要异步传输，不需要等待回复数据
			binder_transaction(proc, thread, &tr, cmd == BC_REPLY);
			break;
		}
		case BC_REGISTER_LOOPER:
			binder_debug(BINDER_DEBUG_THREADS,
				     "binder: %d:%d BC_REGISTER_LOOPER\n",
				     proc->pid, thread->pid);
			if (thread->looper & BINDER_LOOPER_STATE_ENTERED) {
				thread->looper |= BINDER_LOOPER_STATE_INVALID;
				binder_user_error("binder: %d:%d ERROR:"
					" BC_REGISTER_LOOPER called "
					"after BC_ENTER_LOOPER\n",
					proc->pid, thread->pid);
			} else if (proc->requested_threads == 0) {
				thread->looper |= BINDER_LOOPER_STATE_INVALID;
				binder_user_error("binder: %d:%d ERROR:"
					" BC_REGISTER_LOOPER called "
					"without request\n",
					proc->pid, thread->pid);
			} else {
				proc->requested_threads--;
				proc->requested_threads_started++;
			}
			thread->looper |= BINDER_LOOPER_STATE_REGISTERED;
			break;
		case BC_ENTER_LOOPER:
			binder_debug(BINDER_DEBUG_THREADS,
				     "binder: %d:%d BC_ENTER_LOOPER\n",
				     proc->pid, thread->pid);
			if (thread->looper & BINDER_LOOPER_STATE_REGISTERED) {
				thread->looper |= BINDER_LOOPER_STATE_INVALID;
				binder_user_error("binder: %d:%d ERROR:"
					" BC_ENTER_LOOPER called after "
					"BC_REGISTER_LOOPER\n",
					proc->pid, thread->pid);
			}
			thread->looper |= BINDER_LOOPER_STATE_ENTERED;
			break;
		case BC_EXIT_LOOPER:
			binder_debug(BINDER_DEBUG_THREADS,
				     "binder: %d:%d BC_EXIT_LOOPER\n",
				     proc->pid, thread->pid);
			thread->looper |= BINDER_LOOPER_STATE_EXITED;
			break;
		case BC_REQUEST_DEATH_NOTIFICATION:
		case BC_CLEAR_DEATH_NOTIFICATION: {
			uint32_t target;
			void __user *cookie;
			struct binder_ref *ref;
			struct binder_ref_death *death;
			if (get_user(target, (uint32_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(uint32_t);
			if (get_user(cookie, (void __user * __user *)ptr))
				return -EFAULT;
			ptr += sizeof(void *);
			ref = binder_get_ref(proc, target);
			if (ref == NULL) {
				binder_user_error("binder: %d:%d %s "
					"invalid ref %d\n",
					proc->pid, thread->pid,
					cmd == BC_REQUEST_DEATH_NOTIFICATION ?
					"BC_REQUEST_DEATH_NOTIFICATION" :
					"BC_CLEAR_DEATH_NOTIFICATION",
					target);
				break;
			}
			binder_debug(BINDER_DEBUG_DEATH_NOTIFICATION,
				     "binder: %d:%d %s %p ref %d desc %d s %d w %d for node %d\n",
				     proc->pid, thread->pid,
				     cmd == BC_REQUEST_DEATH_NOTIFICATION ?
				     "BC_REQUEST_DEATH_NOTIFICATION" :
				     "BC_CLEAR_DEATH_NOTIFICATION",
				     cookie, ref->debug_id, ref->desc,
				     ref->strong, ref->weak, ref->node->debug_id);
			if (cmd == BC_REQUEST_DEATH_NOTIFICATION) {
				if (ref->death) {
					binder_user_error("binder: %d:%"
						"d BC_REQUEST_DEATH_NOTI"
						"FICATION death notific"
						"ation already set\n",
						proc->pid, thread->pid);
					break;
				}
				death = kzalloc(sizeof(*death), GFP_KERNEL);
				if (death == NULL) {
					thread->return_error = BR_ERROR;
					binder_debug(BINDER_DEBUG_FAILED_TRANSACTION,
						     "binder: %d:%d "
						     "BC_REQUEST_DEATH_NOTIFICATION failed\n",
						     proc->pid, thread->pid);
					break;
				}
				binder_stats_created(BINDER_STAT_DEATH);
				INIT_LIST_HEAD(&death->work.entry);
				death->cookie = cookie;
				ref->death = death;
				if (ref->node->proc == NULL) {
					ref->death->work.type = BINDER_WORK_DEAD_BINDER;
					if (thread->looper & (BINDER_LOOPER_STATE_REGISTERED | BINDER_LOOPER_STATE_ENTERED)) {
						list_add_tail(&ref->death->work.entry, &thread->todo);
					} else {
						list_add_tail(&ref->death->work.entry, &proc->todo);
						wake_up_interruptible(&proc->wait);
					}
				}
			} else {
				if (ref->death == NULL) {
					binder_user_error("binder: %d:%"
						"d BC_CLEAR_DEATH_NOTIFI"
						"CATION death notificat"
						"ion not active\n",
						proc->pid, thread->pid);
					break;
				}
				death = ref->death;
				if (death->cookie != cookie) {
					binder_user_error("binder: %d:%"
						"d BC_CLEAR_DEATH_NOTIFI"
						"CATION death notificat"
						"ion cookie mismatch "
						"%p != %p\n",
						proc->pid, thread->pid,
						death->cookie, cookie);
					break;
				}
				ref->death = NULL;
				if (list_empty(&death->work.entry)) {
					death->work.type = BINDER_WORK_CLEAR_DEATH_NOTIFICATION;
					if (thread->looper & (BINDER_LOOPER_STATE_REGISTERED | BINDER_LOOPER_STATE_ENTERED)) {
						list_add_tail(&death->work.entry, &thread->todo);
					} else {
						list_add_tail(&death->work.entry, &proc->todo);
						wake_up_interruptible(&proc->wait);
					}
				} else {
					BUG_ON(death->work.type != BINDER_WORK_DEAD_BINDER);
					death->work.type = BINDER_WORK_DEAD_BINDER_AND_CLEAR;
				}
			}
		} break;
		case BC_DEAD_BINDER_DONE: {
			struct binder_work *w;
			void __user *cookie;
			struct binder_ref_death *death = NULL;
			if (get_user(cookie, (void __user * __user *)ptr))
				return -EFAULT;
			ptr += sizeof(void *);
			list_for_each_entry(w, &proc->delivered_death, entry) {
				struct binder_ref_death *tmp_death = container_of(w, struct binder_ref_death, work);
				if (tmp_death->cookie == cookie) {
					death = tmp_death;
					break;
				}
			}
			binder_debug(BINDER_DEBUG_DEAD_BINDER,
				     "binder: %d:%d BC_DEAD_BINDER_DONE %p found %p\n",
				     proc->pid, thread->pid, cookie, death);
			if (death == NULL) {
				binder_user_error("binder: %d:%d BC_DEAD"
					"_BINDER_DONE %p not found\n",
					proc->pid, thread->pid, cookie);
				break;
			}
			list_del_init(&death->work.entry);
			if (death->work.type == BINDER_WORK_DEAD_BINDER_AND_CLEAR) {
				death->work.type = BINDER_WORK_CLEAR_DEATH_NOTIFICATION;
				if (thread->looper & (BINDER_LOOPER_STATE_REGISTERED | BINDER_LOOPER_STATE_ENTERED)) {
					list_add_tail(&death->work.entry, &thread->todo);
				} else {
					list_add_tail(&death->work.entry, &proc->todo);
					wake_up_interruptible(&proc->wait);
				}
			}
		} break;
		default:
			printk(KERN_ERR "binder: %d:%d unknown command %d\n",
			       proc->pid, thread->pid, cmd);
			return -EINVAL;
		}
		*consumed = ptr - buffer;
	}
	return 0;
}
void binder_stat_br(struct binder_proc *proc, struct binder_thread *thread,
		    uint32_t cmd)
{
	trace_binder_return(cmd);
	if (_IOC_NR(cmd) < ARRAY_SIZE(binder_stats.br)) {
		binder_stats.br[_IOC_NR(cmd)]++;
		proc->stats.br[_IOC_NR(cmd)]++;
		thread->stats.br[_IOC_NR(cmd)]++;
	}
}
static int binder_has_proc_work(struct binder_proc *proc,
				struct binder_thread *thread)
{
	return !list_empty(&proc->todo) ||
		(thread->looper & BINDER_LOOPER_STATE_NEED_RETURN);
}
static int binder_has_thread_work(struct binder_thread *thread)
{
	return !list_empty(&thread->todo) || thread->return_error != BR_OK ||
		(thread->looper & BINDER_LOOPER_STATE_NEED_RETURN);
}
// 读取
// 当前task所在的进程对应的binder_proc和binder_thread结构体指针，
// 用户空间的read_buffer地址，想读取数据的大小，已经打开binder节点
// 的时候是否是非阻塞打开，默认情况下是阻塞打开文件的。
static int binder_thread_read(struct binder_proc *proc,
			      struct binder_thread *thread,
			      void  __user *buffer, int size,
			      signed long *consumed, int non_block)
{
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;
	int ret = 0;
	int wait_for_proc_work;
	
	// 如果实际读取到的大小等于0，那么将会在返回的数据包中插入BR_NOOP的命令字。
	if (*consumed == 0) {
		if (put_user(BR_NOOP, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
	}
retry:
   /* 
   该标志表示当前task是要去等待处理proc中全局的todo还是自己本task的todo队列中的任务。
   两个条件决定这个标志是否为1，当前task的binder_transaction这个链表为NULL, 它记录着
   本task上是否有传输正在进行；第二个条件是当前task的私有任务队列为NULL。
   对于自己，一定是线程，对于目标，不一定是线程，可能是进程
   */
	wait_for_proc_work = thread->transaction_stack == NULL &&
				list_empty(&thread->todo);
	if (thread->return_error != BR_OK && ptr < end) {
	 // $￥
	}
	// 表明线程即将进入等待状态。
	thread->looper |= BINDER_LOOPER_STATE_WAITING;
	// 就绪等待任务的空闲线程数加1。
	if (wait_for_proc_work)
		proc->ready_threads++;
	binder_unlock(__func__);
	trace_binder_wait_for_work(wait_for_proc_work,
				   !!thread->transaction_stack,
				   !list_empty(&thread->todo));
	if (wait_for_proc_work) {
		// 进程等待
		if (!(thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
					BINDER_LOOPER_STATE_ENTERED))) {
			binder_user_error("binder: %d:%d ERROR: Thread waiting "
				"for process work before calling BC_REGISTER_"
				"LOOPER or BC_ENTER_LOOPER (state %x)\n",
				proc->pid, thread->pid, thread->looper);
			wait_event_interruptible(binder_user_error_wait,
						 binder_stop_on_user_error < 2);
		}
		binder_set_nice(proc->default_priority);
		if (non_block) {
			if (!binder_has_proc_work(proc, thread))
				// 返回try again的提示。
				ret = -EAGAIN;
		} else
			// 当前task互斥等待在进程全局的等待队列中。 
			ret = wait_event_freezable_exclusive(proc->wait, binder_has_proc_work(proc, thread));
	} else {
		// 线程等待
		if (non_block) {
			if (!binder_has_thread_work(thread))
				ret = -EAGAIN;
		} else
		 /* 当前task等待在task自己的等待队列中(binder_thread.todo)，永远只有其自己。，只有自己*/
			ret = wait_event_freezable(thread->wait, binder_has_thread_work(thread));
	}
	binder_lock(__func__);
	if (wait_for_proc_work)
		proc->ready_threads--;
	thread->looper &= ~BINDER_LOOPER_STATE_WAITING;
	if (ret)
		return ret;
	while (1) {
		uint32_t cmd;
		struct binder_transaction_data tr;
		struct binder_work *w;
		struct binder_transaction *t = NULL;
		// 当前task私有todo任务队列里有任务
		if (!list_empty(&thread->todo))
			// 取出todo队列中第一个binder_work结构体
			w = list_first_entry(&thread->todo, struct binder_work, entry);
		else if (!list_empty(&proc->todo) && wait_for_proc_work)
			// proc->todo是当前task所属进程的公共todo任务队列
			w = list_first_entry(&proc->todo, struct binder_work, entry);
		else {
			if (ptr - buffer == 4 && !(thread->looper & BINDER_LOOPER_STATE_NEED_RETURN)) /* no data added */
			/* buffer中只有一个BR_NOOP,  同时当前task的BINDER_LOOPER_STATE_NEED_RETURN标志已被清除，那么就跳转回去重新睡眠等待。*/
				goto retry;
				// 否则就跳出这个while循环，读函数返回。
			break;
		}
		if (end - ptr < sizeof(tr) + 4)
			break;
		switch (w->type) {
		case BINDER_WORK_TRANSACTION: {
			// 通过binder_work：w反向找到所属的binder_transaction数据结构指针
			t = container_of(w, struct binder_transaction, work);
		} break;
		// 返回的completetask
		case BINDER_WORK_TRANSACTION_COMPLETE: {
			cmd = BR_TRANSACTION_COMPLETE;
			if (put_user(cmd, (uint32_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(uint32_t);
			binder_stat_br(proc, thread, cmd);
			binder_debug(BINDER_DEBUG_TRANSACTION_COMPLETE,
				     "binder: %d:%d BR_TRANSACTION_COMPLETE\n",
				     proc->pid, thread->pid);
			list_del(&w->entry);
			kfree(w);
			binder_stats_deleted(BINDER_STAT_TRANSACTION_COMPLETE);
		} break;
		case BINDER_WORK_NODE: {
			struct binder_node *node = container_of(w, struct binder_node, work);
			uint32_t cmd = BR_NOOP;
			const char *cmd_name;
			int strong = node->internal_strong_refs || node->local_strong_refs;
			int weak = !hlist_empty(&node->refs) || node->local_weak_refs || strong;
			if (weak && !node->has_weak_ref) {
				cmd = BR_INCREFS;
				cmd_name = "BR_INCREFS";
				node->has_weak_ref = 1;
				node->pending_weak_ref = 1;
				node->local_weak_refs++;
			} else if (strong && !node->has_strong_ref) {
				cmd = BR_ACQUIRE;
				cmd_name = "BR_ACQUIRE";
				node->has_strong_ref = 1;
				node->pending_strong_ref = 1;
				node->local_strong_refs++;
			} else if (!strong && node->has_strong_ref) {
				cmd = BR_RELEASE;
				cmd_name = "BR_RELEASE";
				node->has_strong_ref = 0;
			} else if (!weak && node->has_weak_ref) {
				cmd = BR_DECREFS;
				cmd_name = "BR_DECREFS";
				node->has_weak_ref = 0;
			}
			if (cmd != BR_NOOP) {
				if (put_user(cmd, (uint32_t __user *)ptr))
					return -EFAULT;
				ptr += sizeof(uint32_t);
				if (put_user(node->ptr, (void * __user *)ptr))
					return -EFAULT;
				ptr += sizeof(void *);
				if (put_user(node->cookie, (void * __user *)ptr))
					return -EFAULT;
				ptr += sizeof(void *);
				binder_stat_br(proc, thread, cmd);
				binder_debug(BINDER_DEBUG_USER_REFS,
					     "binder: %d:%d %s %d u%p c%p\n",
					     proc->pid, thread->pid, cmd_name, node->debug_id, node->ptr, node->cookie);
			} else {
				list_del_init(&w->entry);
				if (!weak && !strong) {
					binder_debug(BINDER_DEBUG_INTERNAL_REFS,
						     "binder: %d:%d node %d u%p c%p deleted\n",
						     proc->pid, thread->pid, node->debug_id,
						     node->ptr, node->cookie);
					rb_erase(&node->rb_node, &proc->nodes);
					kfree(node);
					binder_stats_deleted(BINDER_STAT_NODE);
				} else {
					binder_debug(BINDER_DEBUG_INTERNAL_REFS,
						     "binder: %d:%d node %d u%p c%p state unchanged\n",
						     proc->pid, thread->pid, node->debug_id, node->ptr,
						     node->cookie);
				}
			}
		} break;
		case BINDER_WORK_DEAD_BINDER:
		case BINDER_WORK_DEAD_BINDER_AND_CLEAR:
		case BINDER_WORK_CLEAR_DEATH_NOTIFICATION: {
			struct binder_ref_death *death;
			uint32_t cmd;
			death = container_of(w, struct binder_ref_death, work);
			if (w->type == BINDER_WORK_CLEAR_DEATH_NOTIFICATION)
				cmd = BR_CLEAR_DEATH_NOTIFICATION_DONE;
			else
				cmd = BR_DEAD_BINDER;
			if (put_user(cmd, (uint32_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(uint32_t);
			if (put_user(death->cookie, (void * __user *)ptr))
				return -EFAULT;
			ptr += sizeof(void *);
			binder_stat_br(proc, thread, cmd);
			binder_debug(BINDER_DEBUG_DEATH_NOTIFICATION,
				     "binder: %d:%d %s %p\n",
				      proc->pid, thread->pid,
				      cmd == BR_DEAD_BINDER ?
				      "BR_DEAD_BINDER" :
				      "BR_CLEAR_DEATH_NOTIFICATION_DONE",
				      death->cookie);
			if (w->type == BINDER_WORK_CLEAR_DEATH_NOTIFICATION) {
				list_del(&w->entry);
				kfree(death);
				binder_stats_deleted(BINDER_STAT_DEATH);
			} else
				list_move(&w->entry, &proc->delivered_death);
			if (cmd == BR_DEAD_BINDER)
				goto done; /* DEAD_BINDER notifications can cause transactions */
		} break;
		}
		if (!t)
			continue;
		/* 这里的t所指向的binder_transaction结构体就是前面发送者task建立的binder_transaction数据结构，
		   所以这里如果为NULL，说明有异常。*/
		BUG_ON(t->buffer == NULL);
		/* 可以为NULL，如: BC_REPLY的时候。这个值是发送者task填入，对发送者来说才有意义。*/
		if (t->buffer->target_node) {
			// 下面开始将binder_transaction转换成binder_transaction_data结构了。
			struct binder_node *target_node = t->buffer->target_node;
			 // binder实体的用户空间指针
			tr.target.ptr = target_node->ptr;
			// binder实体的额外数据
			tr.cookie =  target_node->cookie;
			t->saved_priority = task_nice(current);
			if (t->priority < target_node->min_priority &&
			    !(t->flags & TF_ONE_WAY))
				binder_set_nice(t->priority);
			else if (!(t->flags & TF_ONE_WAY) ||
				 t->saved_priority > target_node->min_priority)
				binder_set_nice(target_node->min_priority);
			cmd = BR_TRANSACTION;
		} else {
			 // 如果是发送者自己发送的回复数据：BC_REPLY
			tr.target.ptr = NULL;
			tr.cookie = NULL;
			cmd = BR_REPLY;  // 收到BR_REPLY
		}
		tr.code = t->code;
		tr.flags = t->flags;
		tr.sender_euid = t->sender_euid;
		if (t->from) {
			struct task_struct *sender = t->from->proc->tsk;
			 // 记录发送线程的binder_thread
			tr.sender_pid = task_tgid_nr_ns(sender,
							current->nsproxy->pid_ns);
		} else {
			tr.sender_pid = 0;
		}
		//事务对应的数据的大小  
		tr.data_size = t->buffer->data_size;
		//偏移
		tr.offsets_size = t->buffer->offsets_size;
		/* 接收方在这里完成数据从内核空间到用户空间的转移，其实没有实际的数据移动，
		而是buffer地址在内核空间和用户空间中的转换: user_buffer_offset。*/
		//得到事物对应的内核数据在用户空间的访问地址  
		tr.data.ptr.buffer = (void *)t->buffer->data +
					proc->user_buffer_offset;;/* 需要加上这个偏移量才是用户空间
的地址，这个偏移量是在mmap函数中计算出来的。*/

		tr.data.ptr.offsets = tr.data.ptr.buffer +
					ALIGN(t->buffer->data_size,
					    sizeof(void *));

		/* 拷贝cmd和binder_transaction_data数据回用户空间,上层只需要准备这二者
			的内存空间即可。其余的数据均在binder_buffer之中呆着。
			两个数据结构
		*/
		if (put_user(cmd, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
		if (copy_to_user(ptr, &tr, sizeof(tr)))
			return -EFAULT;
		ptr += sizeof(tr);
		trace_binder_transaction_received(t);
		binder_stat_br(proc, thread, cmd);
 		// $
		 // 从todo队列中删除对应的binder_work。
		list_del(&t->work.entry);
		t->buffer->allow_user_free = 1;
		{//同步，请求数据 接收方
		if (cmd == BR_TRANSACTION && !(t->flags & TF_ONE_WAY)) {
		/* vvvvv
		这表示了同一个binder_transaction在发送task和接收task中都
		有修改的部分。 发送task和接收task的binder_thread.transaction_stack
		指向的是同一个binder_transcation结构体。
		*/
			t->to_parent = thread->transaction_stack;
			// 这里给返回的请求端用
			t->to_thread = thread;
			// 这句很重要，为了server写返回的时候用的，看看是哪个线程处理的，另外将它设置在栈顶，栈顶的一定先处理，其他的都睡眠
			// t->from已经存在 from_parent也存在，
			thread->transaction_stack = t;
		} else {
			// 发送方，不等待回复方
			t->buffer->transaction = NULL;
			kfree(t);
			binder_stats_deleted(BINDER_STAT_TRANSACTION);
		}
		break;
	}
done:
	*consumed = ptr - buffer;
	if (proc->requested_threads + proc->ready_threads == 0 &&
	    proc->requested_threads_started < proc->max_threads &&
	    (thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
	     BINDER_LOOPER_STATE_ENTERED)) /* the user-space code fails to */
	     /*spawn a new thread if we leave this out */) {
		proc->requested_threads++;
		binder_debug(BINDER_DEBUG_THREADS,
			     "binder: %d:%d BR_SPAWN_LOOPER\n",
			     proc->pid, thread->pid);
		if (put_user(BR_SPAWN_LOOPER, (uint32_t __user *)buffer))
			return -EFAULT;
		binder_stat_br(proc, thread, BR_SPAWN_LOOPER);
	}
	return 0;
}
static void binder_release_work(struct list_head *list)
{
	struct binder_work *w;
	while (!list_empty(list)) {
		w = list_first_entry(list, struct binder_work, entry);
		list_del_init(&w->entry);
		switch (w->type) {
		case BINDER_WORK_TRANSACTION: {
			struct binder_transaction *t;
			t = container_of(w, struct binder_transaction, work);
			if (t->buffer->target_node &&
			    !(t->flags & TF_ONE_WAY)) {
				binder_send_failed_reply(t, BR_DEAD_REPLY);
			} else {
				binder_debug(BINDER_DEBUG_DEAD_TRANSACTION,
					"binder: undelivered transaction %d\n",
					t->debug_id);
				t->buffer->transaction = NULL;
				kfree(t);
				binder_stats_deleted(BINDER_STAT_TRANSACTION);
			}
		} break;
		case BINDER_WORK_TRANSACTION_COMPLETE: {
			binder_debug(BINDER_DEBUG_DEAD_TRANSACTION,
				"binder: undelivered TRANSACTION_COMPLETE\n");
			kfree(w);
			binder_stats_deleted(BINDER_STAT_TRANSACTION_COMPLETE);
		} break;
		case BINDER_WORK_DEAD_BINDER_AND_CLEAR:
		case BINDER_WORK_CLEAR_DEATH_NOTIFICATION: {
			struct binder_ref_death *death;
			death = container_of(w, struct binder_ref_death, work);
			binder_debug(BINDER_DEBUG_DEAD_TRANSACTION,
				"binder: undelivered death notification, %p\n",
				death->cookie);
			kfree(death);
			binder_stats_deleted(BINDER_STAT_DEATH);
		} break;
		default:
			pr_err("binder: unexpected work type, %d, not freed\n",
			       w->type);
			break;
		}
	}
}
static struct binder_thread *binder_get_thread(struct binder_proc *proc)
{
	struct binder_thread *thread = NULL;
	struct rb_node *parent = NULL;
	struct rb_node **p = &proc->threads.rb_node;
	// 搜索红黑树binder_proc.threads  // *p是一个rb_node的指针
	while (*p) {
		parent = *p;
		 // 通过结构体成员的指针地址，得到这个结构体的地址
		thread = rb_entry(parent, struct binder_thread, rb_node);
		// 这颗红黑树是以task的pid为索引值 // 按照大小排列，有点类似二分法 //如果找到这个binder_thread结构体，立即退出查找，否则*p最终为NULL
		if (current->pid < thread->pid)
			p = &(*p)->rb_left;
		else if (current->pid > thread->pid)
			p = &(*p)->rb_right;
		else
			break;
	}
	// 如果没找到，也就是当前task第一次调用ioctl的时候，会新建binder_thread数据结构
	if (*p == NULL) {
		thread = kzalloc(sizeof(*thread), GFP_KERNEL);
		if (thread == NULL)
			return NULL;
		// 在binder_stats全局统计数据中，为新建的binder_thread对应的统计项加1
		binder_stats_created(BINDER_STAT_THREAD);
		 // 记录下binder驱动中对主进程描述的结构体
		thread->proc = proc;
        // 当前task的PID
		thread->pid = current->pid;
		// 初始化每一个task的私有等待队列
		init_waitqueue_head(&thread->wait);
		 // 初始化每一个task的私有任务队列
		INIT_LIST_HEAD(&thread->todo);
		rb_link_node(&thread->rb_node, parent, p);
		// 将新建的binder_thread结构体加入当前进程的红黑树binder_proc.threads中
		rb_insert_color(&thread->rb_node, &proc->threads);
		thread->looper |= BINDER_LOOPER_STATE_NEED_RETURN;
		thread->return_error = BR_OK;
		thread->return_error2 = BR_OK;
	}
	 // 返回查找到的或者新建的binder_thread结构体指针
	return thread;
}
static int binder_free_thread(struct binder_proc *proc,
			      struct binder_thread *thread)
{
	struct binder_transaction *t;
	struct binder_transaction *send_reply = NULL;
	int active_transactions = 0;
	rb_erase(&thread->rb_node, &proc->threads);
	t = thread->transaction_stack;
	if (t && t->to_thread == thread)
		send_reply = t;
	while (t) {
		active_transactions++;
		binder_debug(BINDER_DEBUG_DEAD_TRANSACTION,
			     "binder: release %d:%d transaction %d "
			     "%s, still active\n", proc->pid, thread->pid,
			     t->debug_id,
			     (t->to_thread == thread) ? "in" : "out");
		if (t->to_thread == thread) {
			t->to_proc = NULL;
			t->to_thread = NULL;
			if (t->buffer) {
				t->buffer->transaction = NULL;
				t->buffer = NULL;
			}
			t = t->to_parent;
		} else if (t->from == thread) {
			t->from = NULL;
			t = t->from_parent;
		} else
			BUG();
	}
	if (send_reply)
		binder_send_failed_reply(send_reply, BR_DEAD_REPLY);
	binder_release_work(&thread->todo);
	kfree(thread);
	binder_stats_deleted(BINDER_STAT_THREAD);
	return active_transactions;
}
static unsigned int binder_poll(struct file *filp,
				struct poll_table_struct *wait)
{
	struct binder_proc *proc = filp->private_data;
	struct binder_thread *thread = NULL;
	int wait_for_proc_work;
	binder_lock(__func__);
	thread = binder_get_thread(proc);
	wait_for_proc_work = thread->transaction_stack == NULL &&
		list_empty(&thread->todo) && thread->return_error == BR_OK;
	binder_unlock(__func__);
	if (wait_for_proc_work) {
		if (binder_has_proc_work(proc, thread))
			return POLLIN;
		poll_wait(filp, &proc->wait, wait);
		if (binder_has_proc_work(proc, thread))
			return POLLIN;
	} else {
		if (binder_has_thread_work(thread))
			return POLLIN;
		poll_wait(filp, &thread->wait, wait);
		if (binder_has_thread_work(thread))
			return POLLIN;
	}
	return 0;
}
 // 对于异步传输，在上层空间传下来的数据结构binder_transcation_data中的flags域中可以体现出来，
// 也就是flags的TF_ONE_WAY位为1，就表示需要异步传输，不需要等待回复数据。
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	// 当前进程对应的binder_proc 
	struct binder_proc *proc = filp->private_data;
	 // 进程的每个线程在binder驱动中的表示
	struct binder_thread *thread;
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;
	/*printk(KERN_INFO "binder_ioctl: %d:%d %x %lx\n", proc->pid, current->pid, cmd, arg);*/
	trace_binder_ioctl(cmd, arg);
	ret = wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
	if (ret)
		goto err_unlocked;
	binder_lock(__func__);
	/* 查找当前task对应的binder_thread结构体，如果没找到就新建一个binder_thread，同时将其加入binder_proc的threads的红黑树中。*/
	thread = binder_get_thread(proc);
    // ￥
	switch (cmd) {
	case BINDER_WRITE_READ: {
		struct binder_write_read bwr;
		if (size != sizeof(struct binder_write_read)) {
			ret = -EINVAL;
			goto err;
		}
		if (copy_from_user(&bwr, ubuf, sizeof(bwr))) {
			ret = -EFAULT;
			goto err;
		}
		binder_debug(BINDER_DEBUG_READ_WRITE,
			     "binder: %d:%d write %ld at %08lx, read %ld at %08lx\n",
			     proc->pid, thread->pid, bwr.write_size, bwr.write_buffer,
			     bwr.read_size, bwr.read_buffer);
		//  > 0, 表示本次ioctl有待发送的数据
		if (bwr.write_size > 0) {
			// 写数据
			ret = binder_thread_write(proc, thread, (void __user *)bwr.write_buffer, bwr.write_size, &bwr.write_consumed);
			trace_binder_write_done(ret);
			// 成功返回0，出错小于0
			if (ret < 0) {
				bwr.read_consumed = 0;
				if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
					ret = -EFAULT;
				goto err;
			}
		}
		//> 0表示本次ioctl想接收数据
		if (bwr.read_size > 0) {
			// binder驱动接收读数据函数，这里会阻塞，然后被唤醒
			ret = binder_thread_read(proc, thread, (void __user *)bwr.read_buffer, bwr.read_size, &bwr.read_consumed, filp->f_flags & O_NONBLOCK);
			trace_binder_read_done(ret);
			 /* 读返回的时候如果发现todo任务队列中有待处理的任务，那么将会唤醒binder_proc.wait中下一个等待着的空闲线程。*/
			if (!list_empty(&proc->todo))
				wake_up_interruptible(&proc->wait);
			if (ret < 0) {
				if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
					ret = -EFAULT;
				goto err;
			}
		}
      //   返回binder_write_read
		if (copy_to_user(ubuf, &bwr, sizeof(bwr))) {
			ret = -EFAULT;
			goto err;
		}
		break;
	}
	case BINDER_SET_MAX_THREADS:
		if (copy_from_user(&proc->max_threads, ubuf, sizeof(proc->max_threads))) {
			ret = -EINVAL;
			goto err;
		}
		break;
	case BINDER_SET_CONTEXT_MGR:
		if (binder_context_mgr_node != NULL) {
			printk(KERN_ERR "binder: BINDER_SET_CONTEXT_MGR already set\n");
			ret = -EBUSY;
			goto err;
		}
		ret = security_binder_set_context_mgr(proc->tsk);
		if (ret < 0)
			goto err;
		if (binder_context_mgr_uid != -1) {
			if (binder_context_mgr_uid != current->cred->euid) {
				printk(KERN_ERR "binder: BINDER_SET_"
				       "CONTEXT_MGR bad uid %d != %d\n",
				       current->cred->euid,
				       binder_context_mgr_uid);
				ret = -EPERM;
				goto err;
			}
		} else
			binder_context_mgr_uid = current->cred->euid;
		binder_context_mgr_node = binder_new_node(proc, NULL, NULL);
		if (binder_context_mgr_node == NULL) {
			ret = -ENOMEM;
			goto err;
		}
		binder_context_mgr_node->local_weak_refs++;
		binder_context_mgr_node->local_strong_refs++;
		binder_context_mgr_node->has_strong_ref = 1;
		binder_context_mgr_node->has_weak_ref = 1;
		break;
	case BINDER_THREAD_EXIT:
		binder_debug(BINDER_DEBUG_THREADS, "binder: %d:%d exit\n",
			     proc->pid, thread->pid);
		binder_free_thread(proc, thread);
		thread = NULL;
		break;
	case BINDER_VERSION:
		if (size != sizeof(struct binder_version)) {
			ret = -EINVAL;
			goto err;
		}
		if (put_user(BINDER_CURRENT_PROTOCOL_VERSION, &((struct binder_version *)ubuf)->protocol_version)) {
			ret = -EINVAL;
			goto err;
		}
		break;
	default:
		ret = -EINVAL;
		goto err;
	}
	ret = 0;
err:
	if (thread)
		thread->looper &= ~BINDER_LOOPER_STATE_NEED_RETURN;
	binder_unlock(__func__);
	wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
	if (ret && ret != -ERESTARTSYS)
		printk(KERN_INFO "binder: %d:%d ioctl %x %lx returned %d\n", proc->pid, current->pid, cmd, arg, ret);
err_unlocked:
	trace_binder_ioctl_done(ret);
	return ret;
}
static void binder_vma_open(struct vm_area_struct *vma)
{
	struct binder_proc *proc = vma->vm_private_data;
	binder_debug(BINDER_DEBUG_OPEN_CLOSE,
		     "binder: %d open vm area %lx-%lx (%ld K) vma %lx pagep %lx\n",
		     proc->pid, vma->vm_start, vma->vm_end,
		     (vma->vm_end - vma->vm_start) / SZ_1K, vma->vm_flags,
		     (unsigned long)pgprot_val(vma->vm_page_prot));
}
static void binder_vma_close(struct vm_area_struct *vma)
{
	struct binder_proc *proc = vma->vm_private_data;
	binder_debug(BINDER_DEBUG_OPEN_CLOSE,
		     "binder: %d close vm area %lx-%lx (%ld K) vma %lx pagep %lx\n",
		     proc->pid, vma->vm_start, vma->vm_end,
		     (vma->vm_end - vma->vm_start) / SZ_1K, vma->vm_flags,
		     (unsigned long)pgprot_val(vma->vm_page_prot));
	proc->vma = NULL;
	proc->vma_vm_mm = NULL;
	binder_defer_work(proc, BINDER_DEFERRED_PUT_FILES);
}
static struct vm_operations_struct binder_vm_ops = {
	.open = binder_vma_open,
	.close = binder_vma_close,
};
static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	struct vm_struct *area;
	struct binder_proc *proc = filp->private_data;
	const char *failure_string;
	struct binder_buffer *buffer;
	if ((vma->vm_end - vma->vm_start) > SZ_4M)
		vma->vm_end = vma->vm_start + SZ_4M;
	binder_debug(BINDER_DEBUG_OPEN_CLOSE,
		     "binder_mmap: %d %lx-%lx (%ld K) vma %lx pagep %lx\n",
		     proc->pid, vma->vm_start, vma->vm_end,
		     (vma->vm_end - vma->vm_start) / SZ_1K, vma->vm_flags,
		     (unsigned long)pgprot_val(vma->vm_page_prot));
	if (vma->vm_flags & FORBIDDEN_MMAP_FLAGS) {
		ret = -EPERM;
		failure_string = "bad vm_flags";
		goto err_bad_arg;
	}
	vma->vm_flags = (vma->vm_flags | VM_DONTCOPY) & ~VM_MAYWRITE;
	mutex_lock(&binder_mmap_lock);
	if (proc->buffer) {
		ret = -EBUSY;
		failure_string = "already mapped";
		goto err_already_mapped;
	}
	area = get_vm_area(vma->vm_end - vma->vm_start, VM_IOREMAP);
	if (area == NULL) {
		ret = -ENOMEM;
		failure_string = "get_vm_area";
		goto err_get_vm_area_failed;
	}
	proc->buffer = area->addr;
	proc->user_buffer_offset = vma->vm_start - (uintptr_t)proc->buffer;
	mutex_unlock(&binder_mmap_lock);
#ifdef CONFIG_CPU_CACHE_VIPT
	if (cache_is_vipt_aliasing()) {
		while (CACHE_COLOUR((vma->vm_start ^ (uint32_t)proc->buffer))) {
			printk(KERN_INFO "binder_mmap: %d %lx-%lx maps %p bad alignment\n", proc->pid, vma->vm_start, vma->vm_end, proc->buffer);
			vma->vm_start += PAGE_SIZE;
		}
	}
#endif
	proc->pages = kzalloc(sizeof(proc->pages[0]) * ((vma->vm_end - vma->vm_start) / PAGE_SIZE), GFP_KERNEL);
	if (proc->pages == NULL) {
		ret = -ENOMEM;
		failure_string = "alloc page array";
		goto err_alloc_pages_failed;
	}
	proc->buffer_size = vma->vm_end - vma->vm_start;
	vma->vm_ops = &binder_vm_ops;
	vma->vm_private_data = proc;
	if (binder_update_page_range(proc, 1, proc->buffer, proc->buffer + PAGE_SIZE, vma)) {
		ret = -ENOMEM;
		failure_string = "alloc small buf";
		goto err_alloc_small_buf_failed;
	}
	buffer = proc->buffer;
	INIT_LIST_HEAD(&proc->buffers);
	list_add(&buffer->entry, &proc->buffers);
	buffer->free = 1;
	binder_insert_free_buffer(proc, buffer);
	proc->free_async_space = proc->buffer_size / 2;
	barrier();
	proc->files = get_files_struct(proc->tsk);
	proc->vma = vma;
	proc->vma_vm_mm = vma->vm_mm;
	/*printk(KERN_INFO "binder_mmap: %d %lx-%lx maps %p\n",
		 proc->pid, vma->vm_start, vma->vm_end, proc->buffer);*/
	return 0;
err_alloc_small_buf_failed:
	kfree(proc->pages);
	proc->pages = NULL;
err_alloc_pages_failed:
	mutex_lock(&binder_mmap_lock);
	vfree(proc->buffer);
	proc->buffer = NULL;
err_get_vm_area_failed:
err_already_mapped:
	mutex_unlock(&binder_mmap_lock);
err_bad_arg:
	printk(KERN_ERR "binder_mmap: %d %lx-%lx %s failed %d\n",
	       proc->pid, vma->vm_start, vma->vm_end, failure_string, ret);
	return ret;
}
static int binder_open(struct inode *nodp, struct file *filp)
{
	struct binder_proc *proc;
	binder_debug(BINDER_DEBUG_OPEN_CLOSE, "binder_open: %d:%d\n",
		     current->group_leader->pid, current->pid);
	proc = kzalloc(sizeof(*proc), GFP_KERNEL);
	if (proc == NULL)
		return -ENOMEM;
	get_task_struct(current);
	proc->tsk = current;
	INIT_LIST_HEAD(&proc->todo);
	init_waitqueue_head(&proc->wait);
	proc->default_priority = task_nice(current);
	binder_lock(__func__);
	binder_stats_created(BINDER_STAT_PROC);
	hlist_add_head(&proc->proc_node, &binder_procs);
	proc->pid = current->group_leader->pid;
	INIT_LIST_HEAD(&proc->delivered_death);
	filp->private_data = proc;
	binder_unlock(__func__);
	if (binder_debugfs_dir_entry_proc) {
		char strbuf[11];
		snprintf(strbuf, sizeof(strbuf), "%u", proc->pid);
		proc->debugfs_entry = debugfs_create_file(strbuf, S_IRUGO,
			binder_debugfs_dir_entry_proc, proc, &binder_proc_fops);
	}
	return 0;
}
static int binder_flush(struct file *filp, fl_owner_t id)
{
	struct binder_proc *proc = filp->private_data;
	binder_defer_work(proc, BINDER_DEFERRED_FLUSH);
	return 0;
}
static void binder_deferred_flush(struct binder_proc *proc)
{
	struct rb_node *n;
	int wake_count = 0;
	for (n = rb_first(&proc->threads); n != NULL; n = rb_next(n)) {
		struct binder_thread *thread = rb_entry(n, struct binder_thread, rb_node);
		thread->looper |= BINDER_LOOPER_STATE_NEED_RETURN;
		if (thread->looper & BINDER_LOOPER_STATE_WAITING) {
			wake_up_interruptible(&thread->wait);
			wake_count++;
		}
	}
	wake_up_interruptible_all(&proc->wait);
	binder_debug(BINDER_DEBUG_OPEN_CLOSE,
		     "binder_flush: %d woke %d threads\n", proc->pid,
		     wake_count);
}
static int binder_release(struct inode *nodp, struct file *filp)
{
	struct binder_proc *proc = filp->private_data;
	debugfs_remove(proc->debugfs_entry);
	binder_defer_work(proc, BINDER_DEFERRED_RELEASE);
	return 0;
}
static void binder_deferred_release(struct binder_proc *proc)
{
	struct hlist_node *pos;
	struct binder_transaction *t;
	struct rb_node *n;
	int threads, nodes, incoming_refs, outgoing_refs, buffers, active_transactions, page_count;
	BUG_ON(proc->vma);
	BUG_ON(proc->files);
	hlist_del(&proc->proc_node);
	if (binder_context_mgr_node && binder_context_mgr_node->proc == proc) {
		binder_debug(BINDER_DEBUG_DEAD_BINDER,
			     "binder_release: %d context_mgr_node gone\n",
			     proc->pid);
		binder_context_mgr_node = NULL;
	}
	threads = 0;
	active_transactions = 0;
	while ((n = rb_first(&proc->threads))) {
		struct binder_thread *thread = rb_entry(n, struct binder_thread, rb_node);
		threads++;
		active_transactions += binder_free_thread(proc, thread);
	}
	nodes = 0;
	incoming_refs = 0;
	while ((n = rb_first(&proc->nodes))) {
		struct binder_node *node = rb_entry(n, struct binder_node, rb_node);
		nodes++;
		rb_erase(&node->rb_node, &proc->nodes);
		list_del_init(&node->work.entry);
		binder_release_work(&node->async_todo);
		if (hlist_empty(&node->refs)) {
			kfree(node);
			binder_stats_deleted(BINDER_STAT_NODE);
		} else {
			struct binder_ref *ref;
			int death = 0;
			node->proc = NULL;
			node->local_strong_refs = 0;
			node->local_weak_refs = 0;
			hlist_add_head(&node->dead_node, &binder_dead_nodes);
			hlist_for_each_entry(ref, pos, &node->refs, node_entry) {
				incoming_refs++;
				if (ref->death) {
					death++;
					if (list_empty(&ref->death->work.entry)) {
						ref->death->work.type = BINDER_WORK_DEAD_BINDER;
						list_add_tail(&ref->death->work.entry, &ref->proc->todo);
						wake_up_interruptible(&ref->proc->wait);
					} else
						BUG();
				}
			}
			binder_debug(BINDER_DEBUG_DEAD_BINDER,
				     "binder: node %d now dead, "
				     "refs %d, death %d\n", node->debug_id,
				     incoming_refs, death);
		}
	}
	outgoing_refs = 0;
	while ((n = rb_first(&proc->refs_by_desc))) {
		struct binder_ref *ref = rb_entry(n, struct binder_ref,
						  rb_node_desc);
		outgoing_refs++;
		binder_delete_ref(ref);
	}
	binder_release_work(&proc->todo);
	binder_release_work(&proc->delivered_death);
	buffers = 0;
	while ((n = rb_first(&proc->allocated_buffers))) {
		struct binder_buffer *buffer = rb_entry(n, struct binder_buffer,
							rb_node);
		t = buffer->transaction;
		if (t) {
			t->buffer = NULL;
			buffer->transaction = NULL;
			printk(KERN_ERR "binder: release proc %d, "
			       "transaction %d, not freed\n",
			       proc->pid, t->debug_id);
			/*BUG();*/
		}
		binder_free_buf(proc, buffer);
		buffers++;
	}
	binder_stats_deleted(BINDER_STAT_PROC);
	page_count = 0;
	if (proc->pages) {
		int i;
		for (i = 0; i < proc->buffer_size / PAGE_SIZE; i++) {
			if (proc->pages[i]) {
				void *page_addr = proc->buffer + i * PAGE_SIZE;
				binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
					     "binder_release: %d: "
					     "page %d at %p not freed\n",
					     proc->pid, i,
					     page_addr);
				unmap_kernel_range((unsigned long)page_addr,
					PAGE_SIZE);
				__free_page(proc->pages[i]);
				page_count++;
			}
		}
		kfree(proc->pages);
		vfree(proc->buffer);
	}
	put_task_struct(proc->tsk);
	binder_debug(BINDER_DEBUG_OPEN_CLOSE,
		     "binder_release: %d threads %d, nodes %d (ref %d), "
		     "refs %d, active transactions %d, buffers %d, "
		     "pages %d\n",
		     proc->pid, threads, nodes, incoming_refs, outgoing_refs,
		     active_transactions, buffers, page_count);
	kfree(proc);
}
static void binder_deferred_func(struct work_struct *work)
{
	struct binder_proc *proc;
	struct files_struct *files;
	int defer;
	do {
		binder_lock(__func__);
		mutex_lock(&binder_deferred_lock);
		if (!hlist_empty(&binder_deferred_list)) {
			proc = hlist_entry(binder_deferred_list.first,
					struct binder_proc, deferred_work_node);
			hlist_del_init(&proc->deferred_work_node);
			defer = proc->deferred_work;
			proc->deferred_work = 0;
		} else {
			proc = NULL;
			defer = 0;
		}
		mutex_unlock(&binder_deferred_lock);
		files = NULL;
		if (defer & BINDER_DEFERRED_PUT_FILES) {
			files = proc->files;
			if (files)
				proc->files = NULL;
		}
		if (defer & BINDER_DEFERRED_FLUSH)
			binder_deferred_flush(proc);
		if (defer & BINDER_DEFERRED_RELEASE)
			binder_deferred_release(proc); /* frees proc */
		binder_unlock(__func__);
		if (files)
			put_files_struct(files);
	} while (proc);
}
static DECLARE_WORK(binder_deferred_work, binder_deferred_func);
static void
binder_defer_work(struct binder_proc *proc, enum binder_deferred_state defer)
{
	mutex_lock(&binder_deferred_lock);
	proc->deferred_work |= defer;
	if (hlist_unhashed(&proc->deferred_work_node)) {
		hlist_add_head(&proc->deferred_work_node,
				&binder_deferred_list);
		queue_work(binder_deferred_workqueue, &binder_deferred_work);
	}
	mutex_unlock(&binder_deferred_lock);
}
static void print_binder_transaction(struct seq_file *m, const char *prefix,
				     struct binder_transaction *t)
{
	seq_printf(m,
		   "%s %d: %p from %d:%d to %d:%d code %x flags %x pri %ld r%d",
		   prefix, t->debug_id, t,
		   t->from ? t->from->proc->pid : 0,
		   t->from ? t->from->pid : 0,
		   t->to_proc ? t->to_proc->pid : 0,
		   t->to_thread ? t->to_thread->pid : 0,
		   t->code, t->flags, t->priority, t->need_reply);
	if (t->buffer == NULL) {
		seq_puts(m, " buffer free\n");
		return;
	}
	if (t->buffer->target_node)
		seq_printf(m, " node %d",
			   t->buffer->target_node->debug_id);
	seq_printf(m, " size %zd:%zd data %p\n",
		   t->buffer->data_size, t->buffer->offsets_size,
		   t->buffer->data);
}
static void print_binder_buffer(struct seq_file *m, const char *prefix,
				struct binder_buffer *buffer)
{
	seq_printf(m, "%s %d: %p size %zd:%zd %s\n",
		   prefix, buffer->debug_id, buffer->data,
		   buffer->data_size, buffer->offsets_size,
		   buffer->transaction ? "active" : "delivered");
}
static void print_binder_work(struct seq_file *m, const char *prefix,
			      const char *transaction_prefix,
			      struct binder_work *w)
{
	struct binder_node *node;
	struct binder_transaction *t;
	switch (w->type) {
	case BINDER_WORK_TRANSACTION:
		t = container_of(w, struct binder_transaction, work);
		print_binder_transaction(m, transaction_prefix, t);
		break;
	case BINDER_WORK_TRANSACTION_COMPLETE:
		seq_printf(m, "%stransaction complete\n", prefix);
		break;
	case BINDER_WORK_NODE:
		node = container_of(w, struct binder_node, work);
		seq_printf(m, "%snode work %d: u%p c%p\n",
			   prefix, node->debug_id, node->ptr, node->cookie);
		break;
	case BINDER_WORK_DEAD_BINDER:
		seq_printf(m, "%shas dead binder\n", prefix);
		break;
	case BINDER_WORK_DEAD_BINDER_AND_CLEAR:
		seq_printf(m, "%shas cleared dead binder\n", prefix);
		break;
	case BINDER_WORK_CLEAR_DEATH_NOTIFICATION:
		seq_printf(m, "%shas cleared death notification\n", prefix);
		break;
	default:
		seq_printf(m, "%sunknown work: type %d\n", prefix, w->type);
		break;
	}
}
static void print_binder_thread(struct seq_file *m,
				struct binder_thread *thread,
				int print_always)
{
	struct binder_transaction *t;
	struct binder_work *w;
	size_t start_pos = m->count;
	size_t header_pos;
	seq_printf(m, "  thread %d: l %02x\n", thread->pid, thread->looper);
	header_pos = m->count;
	t = thread->transaction_stack;
	while (t) {
		if (t->from == thread) {
			print_binder_transaction(m,
						 "    outgoing transaction", t);
			t = t->from_parent;
		} else if (t->to_thread == thread) {
			print_binder_transaction(m,
						 "    incoming transaction", t);
			t = t->to_parent;
		} else {
			print_binder_transaction(m, "    bad transaction", t);
			t = NULL;
		}
	}
	list_for_each_entry(w, &thread->todo, entry) {
		print_binder_work(m, "    ", "    pending transaction", w);
	}
	if (!print_always && m->count == header_pos)
		m->count = start_pos;
}
static void print_binder_node(struct seq_file *m, struct binder_node *node)
{
	struct binder_ref *ref;
	struct hlist_node *pos;
	struct binder_work *w;
	int count;
	count = 0;
	hlist_for_each_entry(ref, pos, &node->refs, node_entry)
		count++;
	seq_printf(m, "  node %d: u%p c%p hs %d hw %d ls %d lw %d is %d iw %d",
		   node->debug_id, node->ptr, node->cookie,
		   node->has_strong_ref, node->has_weak_ref,
		   node->local_strong_refs, node->local_weak_refs,
		   node->internal_strong_refs, count);
	if (count) {
		seq_puts(m, " proc");
		hlist_for_each_entry(ref, pos, &node->refs, node_entry)
			seq_printf(m, " %d", ref->proc->pid);
	}
	seq_puts(m, "\n");
	list_for_each_entry(w, &node->async_todo, entry)
		print_binder_work(m, "    ",
				  "    pending async transaction", w);
}
static void print_binder_ref(struct seq_file *m, struct binder_ref *ref)
{
	seq_printf(m, "  ref %d: desc %d %snode %d s %d w %d d %p\n",
		   ref->debug_id, ref->desc, ref->node->proc ? "" : "dead ",
		   ref->node->debug_id, ref->strong, ref->weak, ref->death);
}
static void print_binder_proc(struct seq_file *m,
			      struct binder_proc *proc, int print_all)
{
	struct binder_work *w;
	struct rb_node *n;
	size_t start_pos = m->count;
	size_t header_pos;
	seq_printf(m, "proc %d\n", proc->pid);
	header_pos = m->count;
	for (n = rb_first(&proc->threads); n != NULL; n = rb_next(n))
		print_binder_thread(m, rb_entry(n, struct binder_thread,
						rb_node), print_all);
	for (n = rb_first(&proc->nodes); n != NULL; n = rb_next(n)) {
		struct binder_node *node = rb_entry(n, struct binder_node,
						    rb_node);
		if (print_all || node->has_async_transaction)
			print_binder_node(m, node);
	}
	if (print_all) {
		for (n = rb_first(&proc->refs_by_desc);
		     n != NULL;
		     n = rb_next(n))
			print_binder_ref(m, rb_entry(n, struct binder_ref,
						     rb_node_desc));
	}
	for (n = rb_first(&proc->allocated_buffers); n != NULL; n = rb_next(n))
		print_binder_buffer(m, "  buffer",
				    rb_entry(n, struct binder_buffer, rb_node));
	list_for_each_entry(w, &proc->todo, entry)
		print_binder_work(m, "  ", "  pending transaction", w);
	list_for_each_entry(w, &proc->delivered_death, entry) {
		seq_puts(m, "  has delivered dead binder\n");
		break;
	}
	if (!print_all && m->count == header_pos)
		m->count = start_pos;
}
static const char *binder_return_strings[] = {
	"BR_ERROR",
	"BR_OK",
	"BR_TRANSACTION",
	"BR_REPLY",
	"BR_ACQUIRE_RESULT",
	"BR_DEAD_REPLY",
	"BR_TRANSACTION_COMPLETE",
	"BR_INCREFS",
	"BR_ACQUIRE",
	"BR_RELEASE",
	"BR_DECREFS",
	"BR_ATTEMPT_ACQUIRE",
	"BR_NOOP",
	"BR_SPAWN_LOOPER",
	"BR_FINISHED",
	"BR_DEAD_BINDER",
	"BR_CLEAR_DEATH_NOTIFICATION_DONE",
	"BR_FAILED_REPLY"
};
static const char *binder_command_strings[] = {
	"BC_TRANSACTION",
	"BC_REPLY",
	"BC_ACQUIRE_RESULT",
	"BC_FREE_BUFFER",
	"BC_INCREFS",
	"BC_ACQUIRE",
	"BC_RELEASE",
	"BC_DECREFS",
	"BC_INCREFS_DONE",
	"BC_ACQUIRE_DONE",
	"BC_ATTEMPT_ACQUIRE",
	"BC_REGISTER_LOOPER",
	"BC_ENTER_LOOPER",
	"BC_EXIT_LOOPER",
	"BC_REQUEST_DEATH_NOTIFICATION",
	"BC_CLEAR_DEATH_NOTIFICATION",
	"BC_DEAD_BINDER_DONE"
};
static const char *binder_objstat_strings[] = {
	"proc",
	"thread",
	"node",
	"ref",
	"death",
	"transaction",
	"transaction_complete"
};
static void print_binder_stats(struct seq_file *m, const char *prefix,
			       struct binder_stats *stats)
{
	int i;
	BUILD_BUG_ON(ARRAY_SIZE(stats->bc) !=
		     ARRAY_SIZE(binder_command_strings));
	for (i = 0; i < ARRAY_SIZE(stats->bc); i++) {
		if (stats->bc[i])
			seq_printf(m, "%s%s: %d\n", prefix,
				   binder_command_strings[i], stats->bc[i]);
	}
	BUILD_BUG_ON(ARRAY_SIZE(stats->br) !=
		     ARRAY_SIZE(binder_return_strings));
	for (i = 0; i < ARRAY_SIZE(stats->br); i++) {
		if (stats->br[i])
			seq_printf(m, "%s%s: %d\n", prefix,
				   binder_return_strings[i], stats->br[i]);
	}
	BUILD_BUG_ON(ARRAY_SIZE(stats->obj_created) !=
		     ARRAY_SIZE(binder_objstat_strings));
	BUILD_BUG_ON(ARRAY_SIZE(stats->obj_created) !=
		     ARRAY_SIZE(stats->obj_deleted));
	for (i = 0; i < ARRAY_SIZE(stats->obj_created); i++) {
		if (stats->obj_created[i] || stats->obj_deleted[i])
			seq_printf(m, "%s%s: active %d total %d\n", prefix,
				binder_objstat_strings[i],
				stats->obj_created[i] - stats->obj_deleted[i],
				stats->obj_created[i]);
	}
}
static void print_binder_proc_stats(struct seq_file *m,
				    struct binder_proc *proc)
{
	struct binder_work *w;
	struct rb_node *n;
	int count, strong, weak;
	seq_printf(m, "proc %d\n", proc->pid);
	count = 0;
	for (n = rb_first(&proc->threads); n != NULL; n = rb_next(n))
		count++;
	seq_printf(m, "  threads: %d\n", count);
	seq_printf(m, "  requested threads: %d+%d/%d\n"
			"  ready threads %d\n"
			"  free async space %zd\n", proc->requested_threads,
			proc->requested_threads_started, proc->max_threads,
			proc->ready_threads, proc->free_async_space);
	count = 0;
	for (n = rb_first(&proc->nodes); n != NULL; n = rb_next(n))
		count++;
	seq_printf(m, "  nodes: %d\n", count);
	count = 0;
	strong = 0;
	weak = 0;
	for (n = rb_first(&proc->refs_by_desc); n != NULL; n = rb_next(n)) {
		struct binder_ref *ref = rb_entry(n, struct binder_ref,
						  rb_node_desc);
		count++;
		strong += ref->strong;
		weak += ref->weak;
	}
	seq_printf(m, "  refs: %d s %d w %d\n", count, strong, weak);
	count = 0;
	for (n = rb_first(&proc->allocated_buffers); n != NULL; n = rb_next(n))
		count++;
	seq_printf(m, "  buffers: %d\n", count);
	count = 0;
	list_for_each_entry(w, &proc->todo, entry) {
		switch (w->type) {
		case BINDER_WORK_TRANSACTION:
			count++;
			break;
		default:
			break;
		}
	}
	seq_printf(m, "  pending transactions: %d\n", count);
	print_binder_stats(m, "  ", &proc->stats);
}
static int binder_state_show(struct seq_file *m, void *unused)
{
	struct binder_proc *proc;
	struct hlist_node *pos;
	struct binder_node *node;
	int do_lock = !binder_debug_no_lock;
	if (do_lock)
		binder_lock(__func__);
	seq_puts(m, "binder state:\n");
	if (!hlist_empty(&binder_dead_nodes))
		seq_puts(m, "dead nodes:\n");
	hlist_for_each_entry(node, pos, &binder_dead_nodes, dead_node)
		print_binder_node(m, node);
	hlist_for_each_entry(proc, pos, &binder_procs, proc_node)
		print_binder_proc(m, proc, 1);
	if (do_lock)
		binder_unlock(__func__);
	return 0;
}
static int binder_stats_show(struct seq_file *m, void *unused)
{
	struct binder_proc *proc;
	struct hlist_node *pos;
	int do_lock = !binder_debug_no_lock;
	if (do_lock)
		binder_lock(__func__);
	seq_puts(m, "binder stats:\n");
	print_binder_stats(m, "", &binder_stats);
	hlist_for_each_entry(proc, pos, &binder_procs, proc_node)
		print_binder_proc_stats(m, proc);
	if (do_lock)
		binder_unlock(__func__);
	return 0;
}
static int binder_transactions_show(struct seq_file *m, void *unused)
{
	struct binder_proc *proc;
	struct hlist_node *pos;
	int do_lock = !binder_debug_no_lock;
	if (do_lock)
		binder_lock(__func__);
	seq_puts(m, "binder transactions:\n");
	hlist_for_each_entry(proc, pos, &binder_procs, proc_node)
		print_binder_proc(m, proc, 0);
	if (do_lock)
		binder_unlock(__func__);
	return 0;
}
static int binder_proc_show(struct seq_file *m, void *unused)
{
	struct binder_proc *itr;
	struct binder_proc *proc = m->private;
	struct hlist_node *pos;
	int do_lock = !binder_debug_no_lock;
	bool valid_proc = false;
	if (do_lock)
		binder_lock(__func__);
	hlist_for_each_entry(itr, pos, &binder_procs, proc_node) {
		if (itr == proc) {
			valid_proc = true;
			break;
		}
	}
	if (valid_proc) {
		seq_puts(m, "binder proc state:\n");
		print_binder_proc(m, proc, 1);
	}
	if (do_lock)
		binder_unlock(__func__);
	return 0;
}
static void print_binder_transaction_log_entry(struct seq_file *m,
					struct binder_transaction_log_entry *e)
{
	seq_printf(m,
		   "%d: %s from %d:%d to %d:%d node %d handle %d size %d:%d\n",
		   e->debug_id, (e->call_type == 2) ? "reply" :
		   ((e->call_type == 1) ? "async" : "call "), e->from_proc,
		   e->from_thread, e->to_proc, e->to_thread, e->to_node,
		   e->target_handle, e->data_size, e->offsets_size);
}
static int binder_transaction_log_show(struct seq_file *m, void *unused)
{
	struct binder_transaction_log *log = m->private;
	int i;
	if (log->full) {
		for (i = log->next; i < ARRAY_SIZE(log->entry); i++)
			print_binder_transaction_log_entry(m, &log->entry[i]);
	}
	for (i = 0; i < log->next; i++)
		print_binder_transaction_log_entry(m, &log->entry[i]);
	return 0;
}
static const struct file_operations binder_fops = {
	.owner = THIS_MODULE,
	.poll = binder_poll,
	.unlocked_ioctl = binder_ioctl,
	.mmap = binder_mmap,
	.open = binder_open,
	.flush = binder_flush,
	.release = binder_release,
};
static struct miscdevice binder_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "binder",
	.fops = &binder_fops
};
BINDER_DEBUG_ENTRY(state);
BINDER_DEBUG_ENTRY(stats);
BINDER_DEBUG_ENTRY(transactions);
BINDER_DEBUG_ENTRY(transaction_log);
static int __init binder_init(void)
{
	int ret;
	binder_deferred_workqueue = create_singlethread_workqueue("binder");
	if (!binder_deferred_workqueue)
		return -ENOMEM;
	binder_debugfs_dir_entry_root = debugfs_create_dir("binder", NULL);
	if (binder_debugfs_dir_entry_root)
		binder_debugfs_dir_entry_proc = debugfs_create_dir("proc",
						 binder_debugfs_dir_entry_root);
	ret = misc_register(&binder_miscdev);
	if (binder_debugfs_dir_entry_root) {
		debugfs_create_file("state",
				    S_IRUGO,
				    binder_debugfs_dir_entry_root,
				    NULL,
				    &binder_state_fops);
		debugfs_create_file("stats",
				    S_IRUGO,
				    binder_debugfs_dir_entry_root,
				    NULL,
				    &binder_stats_fops);
		debugfs_create_file("transactions",
				    S_IRUGO,
				    binder_debugfs_dir_entry_root,
				    NULL,
				    &binder_transactions_fops);
		debugfs_create_file("transaction_log",
				    S_IRUGO,
				    binder_debugfs_dir_entry_root,
				    &binder_transaction_log,
				    &binder_transaction_log_fops);
		debugfs_create_file("failed_transaction_log",
				    S_IRUGO,
				    binder_debugfs_dir_entry_root,
				    &binder_transaction_log_failed,
				    &binder_transaction_log_fops);
	}
	return ret;
}
device_initcall(binder_init);
#define CREATE_TRACE_POINTS
#include "binder_trace.h"
MODULE_LICENSE("GPL v2");