---

layout: post
title: Android中mmap原理及应用简析
category: Android

---



mmap是Linux中常用的系统调用API，用途广泛，Android中也有不少地方用到，比如匿名共享内存，Binder机制等。本文简单记录下Android中mmap调用流程及原理。mmap函数原型如下：

	void *mmap(void *start,size_t length,int prot,int flags,int fd,off_t offsize);

几个重要参数

* 参数start：指向欲映射的内存起始地址，通常设为 NULL，代表让系统自动选定地址，映射成功后返回该地址。
* 参数length：代表将文件中多大的部分映射到内存。
* 参数prot：映射区域的保护方式。可以为以下几种方式的组合：

返回值是void *类型，分配成功后，被映射成虚拟内存地址。

mmap属于系统调用，用户控件间接通过swi指令触发软中断，进入内核态（各种环境的切换），进入内核态之后，便可以调用内核函数进行处理。 mmap->mmap64->__mmap2->sys_mmap2-> sys_mmap_pgoff ->do_mmap_pgoff


> /Users/personal/source_code/android/platform/bionic/libc/bionic/mmap.cpp:

![mmap用户空间系统调用](https://upload-images.jianshu.io/upload_images/1460468-a9cd40e1c9b1e5fc.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 
> /Users/personal/source_code/android/platform/bionic/libc/arch-arm/syscalls/__mmap2.S:

![mmap bionic汇编](https://upload-images.jianshu.io/upload_images/1460468-59a43e6f056deb40.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

而 __NR_mmap在系统函数调用表中对应的减值如下：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-70aa63460a87461e.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

通过系统调用，执行swi软中断，进入内核态，最终映射到call.S中的内核函数：sys_mmap2

![image.png](https://upload-images.jianshu.io/upload_images/1460468-42d3d362d003d8a6.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

sys_mmap2最终通过sys_mmap_pgoff在内核态完成后续逻辑。


![image.png](https://upload-images.jianshu.io/upload_images/1460468-4ef89b52abe69e8e.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

sys_mmap_pgoff通过宏定义实现


> /Users/personal/source_code/android/kernel/common/mm/mmap.c:

![image.png](https://upload-images.jianshu.io/upload_images/1460468-e627fb397a6ade9f.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

进而调用do_mmap_pgoff：

> /Users/personal/source_code/android/kernel/common/mm/mmap.c:

![image.png](https://upload-images.jianshu.io/upload_images/1460468-c9eae5619ae93a8c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

	unsigned long do_mmap_pgoff(struct file *file, unsigned long addr,
				unsigned long len, unsigned long prot,
				unsigned long flags, unsigned long pgoff,
				unsigned long *populate)
	{
		struct mm_struct * mm = current->mm;
		struct inode *inode;
		vm_flags_t vm_flags;
	
		*populate = 0;
	    ...
		<!--获取用户空间有效虚拟地址-->
		addr = get_unmapped_area(file, addr, len, pgoff, flags);
		...
		inode = file ? file_inode(file) : NULL;
	   ...
	   <!--分配，映射，更新页表-->
		addr = mmap_region(file, addr, len, vm_flags, pgoff);
		if (!IS_ERR_VALUE(addr) &&
		    ((vm_flags & VM_LOCKED) ||
		     (flags & (MAP_POPULATE | MAP_NONBLOCK)) == MAP_POPULATE))
			*populate = len;
		return addr;
	}

get_unmapped_area用于为用户空间找一块内存区域，
	
	
	unsigned long
	get_unmapped_area(struct file *file, unsigned long addr, unsigned long len,
			unsigned long pgoff, unsigned long flags)
	{
		unsigned long (*get_area)(struct file *, unsigned long,
					  unsigned long, unsigned long, unsigned long);
		...
		get_area = current->mm->get_unmapped_area;
		if (file && file->f_op && file->f_op->get_unmapped_area)
			get_area = file->f_op->get_unmapped_area;
		addr = get_area(file, addr, len, pgoff, flags);
		...
		return error ? error : addr;
	}

current->mm->get_unmapped_area一般被赋值为arch_get_unmapped_area_topdown，

![image.png](https://upload-images.jianshu.io/upload_images/1460468-dcb7e7483cd42796.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


	unsigned long
	arch_get_unmapped_area_topdown(struct file *filp, const unsigned long addr0,
				const unsigned long len, const unsigned long pgoff,
				const unsigned long flags)
	{
		struct vm_area_struct *vma;
		struct mm_struct *mm = current->mm;
		unsigned long addr = addr0;
		int do_align = 0;
		int aliasing = cache_is_vipt_aliasing();
		struct vm_unmapped_area_info info;
	
		...	
		
		addr = vm_unmapped_area(&info);
	   ...
		return addr;
	}

先找到合适的虚拟内存（用户空间），几经周转后，调用相应文件或者设备驱动中的mmap函数，完成该设备文件的mmap，至于如何处理处理虚拟空间，要看每个文件的自己的操作了。

![image.png](https://upload-images.jianshu.io/upload_images/1460468-9a12647d1429c569.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

这里有个很关键的结构体 

	const struct file_operations	*f_op;
	
它是文件驱动操作的入口，在open的时候，完成file_operations的绑定，open流程跟mmap类似

![open系统调用](https://upload-images.jianshu.io/upload_images/1460468-36566e152e2da304.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![open系统调用](https://upload-images.jianshu.io/upload_images/1460468-3e9c44bae99bcaa9.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![image.png](https://upload-images.jianshu.io/upload_images/1460468-44b106419077b570.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![image.png](https://upload-images.jianshu.io/upload_images/1460468-6a7a209b89039bd3.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![image.png](https://upload-images.jianshu.io/upload_images/1460468-0ab3ba2c158820b8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

先通过get_unused_fd_flags获取个未使用的fd，再通过do_file_open完成file结构体的创建及初始化，最后通过fd_install完成fd与file的绑定。

![image.png](https://upload-images.jianshu.io/upload_images/1460468-75d9d47396dc11d8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

重点看下path_openat：


	static struct file *path_openat(int dfd, struct filename *pathname,
			struct nameidata *nd, const struct open_flags *op, int flags)
	{
		struct file *base = NULL;
		struct file *file;
		struct path path;
		int opened = 0;
		int error;
	
		file = get_empty_filp();
		if (IS_ERR(file))
			return file;
	
		file->f_flags = op->open_flag;
	
		error = path_init(dfd, pathname->name, flags | LOOKUP_PARENT, nd, &base);
		if (unlikely(error))
			goto out;
	
		current->total_link_count = 0;
		error = link_path_walk(pathname->name, nd);
		if (unlikely(error))
			goto out;
	
		error = do_last(nd, &path, file, op, &opened, pathname);
		while (unlikely(error > 0)) { /* trailing symlink */
			struct path link = path;
			void *cookie;
			if (!(nd->flags & LOOKUP_FOLLOW)) {
				path_put_conditional(&path, nd);
				path_put(&nd->path);
				error = -ELOOP;
				break;
			}
			error = may_follow_link(&link, nd);
			if (unlikely(error))
				break;
			nd->flags |= LOOKUP_PARENT;
			nd->flags &= ~(LOOKUP_OPEN|LOOKUP_CREATE|LOOKUP_EXCL);
			error = follow_link(&link, nd, &cookie);
			if (unlikely(error))
				break;
			error = do_last(nd, &path, file, op, &opened, pathname);
			put_link(nd, &link, cookie);
		}
	out:
		if (nd->root.mnt && !(nd->flags & LOOKUP_ROOT))
			path_put(&nd->root);
		if (base)
			fput(base);
		if (!(opened & FILE_OPENED)) {
			BUG_ON(!error);
			put_filp(file);
		}
		if (unlikely(error)) {
			if (error == -EOPENSTALE) {
				if (flags & LOOKUP_RCU)
					error = -ECHILD;
				else
					error = -ESTALE;
			}
			file = ERR_PTR(error);
		}
		return file;
	}

拿Binder设备文件为例子，在注册该设备驱动的时候，对应的file_operations已经注册好了，

![image.png](https://upload-images.jianshu.io/upload_images/1460468-5fbd519c15766e9a.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![image.png](https://upload-images.jianshu.io/upload_images/1460468-d50aad05871bc774.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

open的时候，只需要根根inode节点，获取到file_operations既可，并且，在open成功后，要回调file_operations中的open函数

![image.png](https://upload-images.jianshu.io/upload_images/1460468-fc5ea69f7d8b9008.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

open后，就可以利用fd找到file，之后利用file中的file_operations *f_op调用相应驱动函数，接着看mmap。

# Binder mmap 的作用及原理（一次拷贝）


**Binder机制中mmap的最大特点是一次拷贝即可完成进程间通信**。Android应用在进程启动之初会创建一个单例的ProcessState对象，其构造函数执行时会同时完成binder mmap，为进程分配一块内存，专门用于Binder通信，如下。

	ProcessState::ProcessState(const char *driver)
	    : mDriverName(String8(driver))
	    , mDriverFD(open_driver(driver))
	    ...
	 {
	    if (mDriverFD >= 0) {
	        // mmap the binder, providing a chunk of virtual address space to receive transactions.
	        mVMStart = mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);
	        ...
	    }
	}
	
第一个参数是分配地址，为0意味着让系统自动分配，流程跟之前分子类似，先在用户空间找到一块合适的虚拟内存，之后，在内核空间也找到一块合适的虚拟内存，修改两个控件的页表，使得两者映射到同一块物力内存。

Linux的内存分用户空间跟内核空间，同时页表有也分两类，用户空间页表跟内核空间页表，每个进程有一个用户空间页表，但是系统只有一个内核空间页表。而Binder mmap的关键是：也更新用户空间对应的页表的同时也同步映射内核页表，让两个页表都指向同一块地址，这样一来，数据只需要从A进程的用户空间，直接拷贝拷贝到B所对应的内核空间，而B多对应的内核空间在B进程的用户空间也有相应的映射，这样就无需从内核拷贝到用户空间了。


	static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
	{
		int ret;
	    ...
		if ((vma->vm_end - vma->vm_start) > SZ_4M)
			vma->vm_end = vma->vm_start + SZ_4M;
       ...
		// 在内核空间找合适的虚拟内存块
		area = get_vm_area(vma->vm_end - vma->vm_start, VM_IOREMAP);
	   proc->buffer = area->addr;
	   <!--记录用户空间虚拟地址跟内核空间虚拟地址的差值-->
	   proc->user_buffer_offset = vma->vm_start - (uintptr_t)proc->buffer;
			...
		proc->pages = kzalloc(sizeof(proc->pages[0]) * ((vma->vm_end - vma->vm_start) / PAGE_SIZE), GFP_KERNEL);
	   ..<!--分配page，并更新用户空间及内核空间对应的页表-->
		ret = binder_update_page_range(proc, 1, proc->buffer, proc->buffer + PAGE_SIZE, vma);
		...
		return ret;
	}


binder_update_page_range完成了内存分配、页表修改等关键操作：

	static int binder_update_page_range(struct binder_proc *proc, int allocate,
	            void *start, void *end,
	            struct vm_area_struct *vma)
	{
    ...
	 <!--一页页分配-->
	for (page_addr = start; page_addr < end; page_addr += PAGE_SIZE) {
		int ret;
		struct page **page_array_ptr;
		<!--分配一页-->
		page = &proc->pages[(page_addr - proc->buffer) / PAGE_SIZE];
		*page = alloc_page(GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO);
		...
		<!-- 修改页表，让物理空间映射到内核空间-->
		ret = map_vm_area(&tmp_area, PAGE_KERNEL, &page_array_ptr);
		..
		 <!--根据之前记录过差值，计算用户空间对应的虚拟地址-->
		user_page_addr =
			(uintptr_t)page_addr + proc->user_buffer_offset;
		<!--修改页表，让物理空间映射到用户空间-->
		ret = vm_insert_page(vma, user_page_addr, page[0]);
	}
	...
	  return -ENOMEM;
	}

可以看到，binder一次拷贝的关键是，完成内存的时候，同时完成了内核空间跟用户空间的映射，也就是说，同一份物理内存，既可以在用户空间，用虚拟地址访问，也可以在内核空间用虚拟地址访问。

# 普通文件mmap原理

普通文件访问方式有两种，第一种是使用read/write系统调访问，首先在用户空间分配内存，然后，在内核将内容从磁盘读取到内核缓冲，最后，从内核拷贝到用户进程空间，至少拷贝两次；同时，多个进程同时访问一个文件，每个进程都有一个副本，存在资源浪费。

另一种是通过mmap来访问文件，mmap()将文件直接映射到用户空间，文件在mmap的时候，内存并未真正分配，只有在第一次读取跟写入的时候才会触发，这个时候，会引发缺页中断，在缺页中断处理的时候，完成内存也分配，同时也完成文件数据的拷贝。数据拷贝后，直接修改用户空间对应的页表，完成到用户空间的映射，只进行了一次数据拷贝，效率更高。同时多进程间通过mmap共享通信的时候，也仅仅需要一块物理内存就够了。 
 
# 共享内存mmap原理

共享内存是在普通文件mmap的基础上实现的，其实就是基于tmpfs文件系统的普通mmap。

# 参考文档

[mmap实例及原理分析](http://blog.rootk.com/post/mmap-example-and-kernel-implementation.html)       