# mmap系统调用流程

mmap是Linux中常用的系统调用API，用途广泛，函数原型如下

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

几经周转后，最终调用相应文件或者设备驱动中的mmap函数，完成该设备文件的mmap。

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

# Binder mmap 的作用及原理


Android应用在进程启动之初会创建一个单例的ProcessState对象，其构造函数执行时会同时完成binder mmap，为进程分配一块内存，专门用于Binder通信，如下。

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
	
第一个参数是分配地址，为0意味着让系统自动分配，

用空间，内核空间，用户空间页表，内核空间页表，真正访问及写内存的时候，缺页中断，分配内存。如果已经有了，则直接访问。而Binder mmap的关键是在内核分配内存后，更新内核页表的同时也更新用户空间对应的页表，并且让两个页表都指向同一块地址，这样的话，数据只需要从A进程的用户空间，拷贝到内核空间，而内核空间的这部分内存在B进程的用户空间也有映射（已经建立了页表）。所谓虚拟地址内核1G，其实是页表记录的访问范围。



修改页表，用户空间，内核空间，用户态，内核态。

mmap主要为了一次拷贝，完成用户空间及内核空间之间的映射。

# 文件mmap原理


mmap为了方便操作，效率高，延迟写入

# 共享内存mmap原理

mmap主要为了进程间通信，不过可能需要处理同步互斥问题