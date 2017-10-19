---
layout: post
title: Linux共享内存原理及Android中的应用
category: Android


---

阅读本文之前，不妨先思考一个问题，在Android系统中，APP端View视图的数据是如何传递SurfaceFlinger服务的呢？Android系统中，View绘制的数据最终是按照一帧一帧显示到屏幕的，而每一帧都会占用一定的存储空间，在APP端执行draw的时候，数据很明显是要绘制到APP的进程空间，但是视图窗口要经过SurfaceFlinger图层混排才会生成最终的帧，而SurfaceFlinger又运行在独立的服务进程，那么View视图的数据是如何在两个进程间传递的呢，普通的Binder通信肯定不行，因为Binder不太适合这种数据量比较大的通信，那么View数据的通信采用的是什么IPC手段呢？答案就是共享内存，更精确的说是Linux的匿名共享内存。共享内存是Linux自带的一种IPC机制，Android直接使用了该模型，在绘制图形的时候，APP进程同SurfaceFlinger共用一块内存，如此以来，就不需要进行数据拷贝，只要合理的处理同步机制，效率更高，APP端绘制完毕，通知SurfaceFlinger端合成，再输出到硬件进行显示，当然，个中细节会更复杂，本文主要分析下匿名共享内存的原理及在Android，就来看下个中细节：

 ![View绘制与共享内存.jpg](http://upload-images.jianshu.io/upload_images/1460468-103d49829291e1f7.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# Linux共享内存 

首先看下Linux的共享内存机制及使用，首先看一下两个关键函数，

* 	int shmget(key_t key, size_t size, int shmflg); 该函数用来创建共享内存
* 	void *shmat(int shm_id, const void *shm_addr, int shmflg); 要想访问共享内存，
必须将其映射到当前进程的地址空间

参考网上的一个demo，简单的看下，其中key_t是共享内存的唯一标识，
读取进程

	int main()  
	{  
	    void *shm = NULL;//分配的共享内存的原始首地址  
	    struct shared_use_st *shared;//指向shm  
	    int shmid;//共享内存标识符  
	    //创建共享内存  
	    shmid = shmget((key_t)12345, sizeof(struct shared_use_st), 0666|IPC_CREAT);   
	    //将共享内存映射到当前进程的地址空间  
	    shm = shmat(shmid, 0, 0);
	    //设置共享内存  
	    shared = (struct shared_use_st*)shm;  
	    shared->written = 0;  
	    //访问共享内存
	    while(1){
		    if(shared->written != 0)  { 
		    	printf("You wrote: %s", shared->text);
			     if(strncmp(shared->text, "end", 3) == 0)  
           			break;
			    }}
	    //把共享内存从当前进程中分离  
	    if(shmdt(shm) == -1)  { }  
	    //删除共享内存  
	    if(shmctl(shmid, IPC_RMID, 0) == -1)   {  }  
	    exit(EXIT_SUCCESS);  
	}  
		
写进程

	int main()  
	{  
	    void *shm = NULL;  
	    struct shared_use_st *shared = NULL;  
	    char buffer[BUFSIZ + 1];//用于保存输入的文本  
	    int shmid;  
	    //创建共享内存  
	    shmid = shmget((key_t) 12345, sizeof(struct shared_use_st), 0666|IPC_CREAT);  
	    //将共享内存连接到当前进程的地址空间  
	    shm = shmat(shmid, (void*)0, 0);  
	    printf("Memory attached at %X\n", (int)shm);  
	    //设置共享内存  
	    shared = (struct shared_use_st*)shm;  
	    while(1)//向共享内存中写数据  
	    {  
	        //数据还没有被读取，则等待数据被读取,不能向共享内存中写入文本  
	        while(shared->written == 1)  
	        {  
	            sleep(1);  
	        }  
	        //向共享内存中写入数据  
	        fgets(buffer, BUFSIZ, stdin);  
	        strncpy(shared->text, buffer, TEXT_SZ);  
	        shared->written = 1;  
	        if(strncmp(buffer, "end", 3) == 0)  
	            running = 0;  
	    }  
	    //把共享内存从当前进程中分离  
	    if(shmdt(shm) == -1)   {    }  
	    sleep(2);  
	    exit(EXIT_SUCCESS);  
	} 

使用共享内存在进行进程间的通信效率非常高，进程间不需要传递数据，可以直接访问内存，缺点也很明显，共享内存没有提供同步的机制，在使用时，要借助其他的手段来处理进程间同步。Anroid本身在核心态是支持System V的功能，但是bionic库删除了glibc的shmget等函数，使得android无法采用shmget的方式实现共享内存，同时Android在此基础上，创建了自己的匿名共享内存方式。

# Android的匿名共享内存

Android可以使用Linux的一切IPC通信方式，包括共享内存，不过Android主要使用的方式是匿名共享内存Ashmem（Anonymous Shared Memory），跟原生的不太一样，比如它添加了互斥锁，另外通过fd的传递来实现共享内存空间传递，MemoryFile是Android为匿名共享内存而风中的一个对象，这里通过使用MemoryFile来分析，Android中如何利用共享内存来实现大数据传递。

> IMemoryAidlInterface.aidl

	package com.snail.labaffinity;
	import android.os.ParcelFileDescriptor;
	
	interface IMemoryAidlInterface {
	    ParcelFileDescriptor getParcelFileDescriptor();
	}

> MemoryFetchService

	public class MemoryFetchService extends Service {
	    @Nullable
	    @Override
	    public IBinder onBind(Intent intent) {
	        return new MemoryFetchStub();
	    }
	    static class MemoryFetchStub extends IMemoryAidlInterface.Stub {
	        @Override
	        public ParcelFileDescriptor getParcelFileDescriptor() throws RemoteException {
	            MemoryFile memoryFile = null;
	            try {
	                memoryFile = new MemoryFile("test_memory", 1024);
	                memoryFile.getOutputStream().write(new byte[]{1, 2, 3, 4, 5});
	                Method method = MemoryFile.class.getDeclaredMethod("getFileDescriptor");
	                FileDescriptor des = (FileDescriptor) method.invoke(memoryFile);
	                return ParcelFileDescriptor.dup(des);
	            } catch (Exception e) {}
	            return null;
	     }}}

> TestActivity.java	

	 Intent intent = new Intent(MainActivity.this, MemoryFetchService.class);
	        bindService(intent, new ServiceConnection() {
	            @Override
	            public void onServiceConnected(ComponentName name, IBinder service) {
	
	                byte[] content = new byte[10];
	                IMemoryAidlInterface iMemoryAidlInterface
	                        = IMemoryAidlInterface.Stub.asInterface(service);
	                try {
	                    ParcelFileDescriptor parcelFileDescriptor = iMemoryAidlInterface.getParcelFileDescriptor();
	                    FileDescriptor descriptor = parcelFileDescriptor.getFileDescriptor();
	                    FileInputStream fileInputStream = new FileInputStream(descriptor);
	                    fileInputStream.read(content);
	                } catch (Exception e) {
	                }}
	
	            @Override
	            public void onServiceDisconnected(ComponentName name) {
	
	            }
	        }, Service.BIND_AUTO_CREATE);	
以上是应用层使用匿名共享内存的方法，可以看到，关键点就是文件描述符（FileDescriptor）的传递，文件描述符是Linux系统中访问与更新文件的主要方式。从MemoryFile字面上看出，共享内存被抽象成了文件，其实就是在tmpfs临时文件系统中创建一个临时文件，**（只是创建了节点，而没有实际的文件）** 该文件与Ashmem驱动程序创建的匿名共享内存对应，可以直接去proc/pid下查看：

![申请的共享内存在proc文件系统中位置.jpg](http://upload-images.jianshu.io/upload_images/1460468-9f56d81d05db4404.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

下面就主要分析两点，共享内存的分配与传递，先看下MemoryFile的构造函数

    public MemoryFile(String name, int length) throws IOException {
        mLength = length;
        mFD = native_open(name, length);
        if (length > 0) {
            mAddress = native_mmap(mFD, length, PROT_READ | PROT_WRITE);
        } else {
            mAddress = 0;
        }
    }
    
 可以看到 Java层只是简单的封装，具体实现在native层  ，首先是通过native_open调用ashmem_create_region创建共享内存，
    
	    static jobject android_os_MemoryFile_open(JNIEnv* env, jobject clazz, jstring name, jint length)
	{
	    const char* namestr = (name ? env->GetStringUTFChars(name, NULL) : NULL);
	
	    int result = ashmem_create_region(namestr, length);
	
	    if (name)
	        env->ReleaseStringUTFChars(name, namestr);
	
	    if (result < 0) {
	        jniThrowException(env, "java/io/IOException", "ashmem_create_region failed");
	        return NULL;
	    }
	
	    return jniCreateFileDescriptor(env, result);
	}
	        
接着通过native_mmap调用mmap将共享内存映射到当前进程空间，之后Java层就能利用FileDescriptor，像访问文件一样访问共享内存。

	static jint android_os_MemoryFile_mmap(JNIEnv* env, jobject clazz, jobject fileDescriptor,
	        jint length, jint prot)
	{
	    int fd = jniGetFDFromFileDescriptor(env, fileDescriptor);
	    <!--系统调用mmap，分配内存-->
	    jint result = (jint)mmap(NULL, length, prot, MAP_SHARED, fd, 0);
	    if (!result)
	        jniThrowException(env, "java/io/IOException", "mmap failed");
	    return result;
	}	        
	
ashmem_create_region这个函数是如何向Linux申请一块共享内存的呢？

	int ashmem_create_region(const char *name, size_t size)
	{
	    int fd, ret;
	    fd = open(ASHMEM_DEVICE, O_RDWR);
	    if (fd < 0)
	        return fd;
		    if (name) {
	        char buf[ASHMEM_NAME_LEN];
	        strlcpy(buf, name, sizeof(buf));
	        ret = ioctl(fd, ASHMEM_SET_NAME, buf);
	        if (ret < 0)
	            goto error;
	    }
	
	    ret = ioctl(fd, ASHMEM_SET_SIZE, size);
	    if (ret < 0)
	        goto error;
	
	    return fd;
	
	error:
	    close(fd);
	    return ret;
	}

ASHMEM_DEVICE其实就是抽象的共享内存设备，它是一个杂项设备（字符设备的一种），在驱动加载之后，就会在/dev下穿件ashem文件，之后用户就能够访问该设备文件，同一般的设备文件不同，它仅仅是通过内存抽象的，同普通的磁盘设备文件、串行端口字段设备文件不一样：

	#define ASHMEM_DEVICE   "/dev/ashmem"	
	static struct miscdevice ashmem_misc = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "ashmem",
		.fops = &ashmem_fops,
	};
	
接着进入驱动看一下，如何申请共享内存，open函数很普通，主要是创建一个ashmem_area对象
	
	static int ashmem_open(struct inode *inode, struct file *file)
	{
		struct ashmem_area *asma;
		int ret;
	
		ret = nonseekable_open(inode, file);
		if (unlikely(ret))
			return ret;
	
		asma = kmem_cache_zalloc(ashmem_area_cachep, GFP_KERNEL);
		if (unlikely(!asma))
			return -ENOMEM;
	
		INIT_LIST_HEAD(&asma->unpinned_list);
		memcpy(asma->name, ASHMEM_NAME_PREFIX, ASHMEM_NAME_PREFIX_LEN);
		asma->prot_mask = PROT_MASK;
		file->private_data = asma;
		return 0;
	}
	
接着利用ashmem_ioctl设置共享内存的大小，

	static long ashmem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
	{
		struct ashmem_area *asma = file->private_data;
		long ret = -ENOTTY;
		switch (cmd) {
		...
		case ASHMEM_SET_SIZE:
			ret = -EINVAL;
			if (!asma->file) {
				ret = 0;
				asma->size = (size_t) arg;
			}
			break;
		...
		}
	   return ret;
	}	

可以看到，其实并未真正的分配内存，这也符合Linux的风格，只有等到真正的使用的时候，才会通过缺页中断分配内存，接着mmap函数，它会分配内存吗？

	static int ashmem_mmap(struct file *file, struct vm_area_struct *vma)
	{
		struct ashmem_area *asma = file->private_data;
		int ret = 0;
		mutex_lock(&ashmem_mutex);
		...
		if (!asma->file) {
			char *name = ASHMEM_NAME_DEF;
			struct file *vmfile;
	
			if (asma->name[ASHMEM_NAME_PREFIX_LEN] != '\0')
				name = asma->name;
			// 这里创建的临时文件其实是备份用的临时文件，之类的临时文件有文章说只对内核态可见，用户态不可见，我们也没有办法通过命令查询到 文件存在哪个路径了呢？？是个隐藏文件，用户空间看不到！！
			<!--校准真正操作的文件-->
			vmfile = shmem_file_setup(name, asma->size, vma->vm_flags);
			asma->file = vmfile;
		}
		get_file(asma->file);
		if (vma->vm_flags & VM_SHARED)
			shmem_set_file(vma, asma->file);
		else {
			if (vma->vm_file)
				fput(vma->vm_file);
			vma->vm_file = asma->file;
		}
		vma->vm_flags |= VM_CAN_NONLINEAR;
	out:
		mutex_unlock(&ashmem_mutex);
		return ret;
	}

其实这里就复用了Linux的共享内存机制，虽然说是匿名共享内存，但底层其实还是给共享内存设置了名称（前缀ASHMEM_NAME_PREFIX+名字），如果名字未设置，那就默认使用ASHMEM_NAME_PREFIX作为名称。不过，在这里没直接看到内存分配的函数。但是，有两个函数shmem_file_setup与shmem_set_file很重要，也是共享内存比较不好理解的地方，shmem_file_setup是原生linux的共享内存机制，不过Android也修改Linux共享内存的驱动代码，匿名共享内存其实就是在Linux共享内存的基础上做了改进，比如，扩展基于Binder的文件描述符的传递。
 
	 struct file *shmem_file_setup(char *name, loff_t size, unsigned long flags)
	{
		int error;
		struct file *file;
		struct inode *inode;
		struct dentry *dentry, *root;
		struct qstr this;	
		error = -ENOMEM;
		this.name = name;
		this.len = strlen(name);
		this.hash = 0; /* will go */
		root = shm_mnt->mnt_root;
		dentry = d_alloc(root, &this);//分配dentry cat/proc/pid/maps可以查到
		error = -ENFILE;
		file = get_empty_filp();      //分配file
		error = -ENOSPC;
		inode = shmem_get_inode(root->d_sb, S_IFREG | S_IRWXUGO, 0, flags);//分配inode，分配成功就好比建立了文件，也许并未存在真实文件映射
		d_instantiate(dentry, inode);//绑定
		inode->i_size = size;
		inode->i_nlink = 0;	/* It is unlinked */
			// 文件操作符，这里似乎真的是不在内存里面创建什么东西？？？
		init_file(file, shm_mnt, dentry, FMODE_WRITE | FMODE_READ,
			  &shmem_file_operations);//绑定，并指定该文件操作指针为shmem_file_operations
		...
	}
	
如此以来，shmem_file_setup在tmpfs临时文件系统中创建一个临时文件（也许只是内存中的一个节点），该文件与Ashmem驱动程序创建的匿名共享内存对应，不过用户态并不能看到该临时文件，之后就能够使用该临时文件了，注意**共享内存机制真正使用map的对象其实是这个临时文件，而不是ashmem设备文件，通过vma->vm_file = asma->file完成map对象的替换，当映射的内存引起缺页中断的时候，就会调用shmem_file_setup创建的对象的函数，而不是ashmem**，看下赋值的操作

	void shmem_set_file(struct vm_area_struct *vma, struct file *file)
	{
		if (vma->vm_file)
			fput(vma->vm_file);
		vma->vm_file = file;
		vma->vm_ops = &shmem_vm_ops;
	}
	
在内核中，一块内存对应的数据结构是ashmem_area：

	struct ashmem_area {
		char name[ASHMEM_FULL_NAME_LEN];/* optional name for /proc/pid/maps */
		struct list_head unpinned_list;	/* list of all ashmem areas */
		struct file *file;		/* the shmem-based backing file */
		size_t size;			/* size of the mapping, in bytes */
		unsigned long prot_mask;	/* allowed prot bits, as vm_flags */
	};

	struct ashmem_range {
		struct list_head lru;		/* entry in LRU list */
		struct list_head unpinned;	/* entry in its area's unpinned list */
		struct ashmem_area *asma;	/* associated area */
		size_t pgstart;			/* starting page, inclusive */
		size_t pgend;			/* ending page, inclusive */
		unsigned int purged;		/* ASHMEM_NOT or ASHMEM_WAS_PURGED */
	};
	
当使用Ashmem分配了一块内存，其中某些部分却不会被使用时，就可以将这块内存unpin掉，内核可以将unpin对应的物理页面回收,回收后的内存还可以再次被获得(通过缺页handler)，因为unpin操作并不会改变已经mmap的地址空间,到这里回到之前的MemoryFile，看一下写操作：

    public void writeBytes(byte[] buffer, int srcOffset, int destOffset, int count)
            throws IOException {
        if (isDeactivated()) {
            throw new IOException("Can't write to deactivated memory file.");
        }
        if (srcOffset < 0 || srcOffset > buffer.length || count < 0
                || count > buffer.length - srcOffset
                || destOffset < 0 || destOffset > mLength
                || count > mLength - destOffset) {
            throw new IndexOutOfBoundsException();
        }
        native_write(mFD, mAddress, buffer, srcOffset, destOffset, count, mAllowPurging);
    }

进入native代码    
    
	static jint android_os_MemoryFile_write(JNIEnv* env, jobject clazz,
	        jobject fileDescriptor, jint address, jbyteArray buffer, jint srcOffset, jint destOffset,
	        jint count, jboolean unpinned)
	{
	    int fd = jniGetFDFromFileDescriptor(env, fileDescriptor);
	    if (unpinned && ashmem_pin_region(fd, 0, 0) == ASHMEM_WAS_PURGED) {
	        ashmem_unpin_region(fd, 0, 0);
	        return -1;
	    }
	    env->GetByteArrayRegion(buffer, srcOffset, count, (jbyte *)address + destOffset);
	    if (unpinned) {
	        ashmem_unpin_region(fd, 0, 0);
	    }
	    return count;
	}

MemoryFile只会操作整个共享内存，而不会分块访问，所以pin与unpin对于它没多大意义，后面再看，对于MemeoryFile分配的共享内存，可以看做整个区域都是pin或者unpin的，首次通过env->GetByteArrayRegion访问会引发缺页中断，进而调用tmpfs 文件的相应操作，分配物理页，在Android现在的内核中，缺页中断对应的vm_operations_struct中的函数是fault，在共享内存实现中，对应的是shmem_fault如下，

	static struct vm_operations_struct shmem_vm_ops = {
		.fault		= shmem_fault,
		
	#ifdef CONFIG_NUMA
		.set_policy     = shmem_set_policy,
		.get_policy     = shmem_get_policy,
	#endif
	};

当mmap的tmpfs文件引发缺页中断时，	就会调用shmem_fault函数，

	static int shmem_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
	{
		struct inode *inode = vma->vm_file->f_path.dentry->d_inode;
		int error;
		int ret;
	
		if (((loff_t)vmf->pgoff << PAGE_CACHE_SHIFT) >= i_size_read(inode))
			return VM_FAULT_SIGBUS;
	
		error = shmem_getpage(inode, vmf->pgoff, &vmf->page, SGP_CACHE, &ret);
		if (error)
			return ((error == -ENOMEM) ? VM_FAULT_OOM : VM_FAULT_SIGBUS);
	
		return ret | VM_FAULT_LOCKED;
	}

可以看到会继续调用shmem_getpage函数分配真实的物理页，具体的分配策略比较复杂，不在分析。

# Android匿名共享内存的pin与unpin

pin本身的意思是压住，定住，ashmem_pin_region和ashmem_unpin_region这两个函数从字面上来说，就是用来对匿名共享内存锁定和解锁，标识哪些内存正在使用需要锁定，哪些内存是不使用的，这样，ashmem驱动程序可以一定程度上辅助内存管理，提供一定的内存优化能力。匿名共享内存创建之初时，所有的内存都是pinned状态，只有用户主动申请，才会unpin一块内存，只有对于unpinned状态的内存块，用户才可以重新pin。现在仔细梳理一下驱动，看下pin与unpin的实现
 
	 static int __init ashmem_init(void)
	{
		int ret;
		<!--创建 ahemem_area 高速缓存-->
		ashmem_area_cachep = kmem_cache_create("ashmem_area_cache",
						  sizeof(struct ashmem_area),
						  0, 0, NULL);
		...
		<!--创建 ahemem_range高速缓存-->
		ashmem_range_cachep = kmem_cache_create("ashmem_range_cache",
						  sizeof(struct ashmem_range),
						  0, 0, NULL);
		...
		<!--注册杂项设备去送-->				  
		ret = misc_register(&ashmem_misc);
		...
		register_shrinker(&ashmem_shrinker);
		return 0;
	}

打开ashem的时候 ，会利用ashmem_area_cachep告诉缓存新建ashmem_area对象，并初始化unpinned_list，开始肯定为null

	static int ashmem_open(struct inode *inode, struct file *file)
	{
		struct ashmem_area *asma;
		int ret;
	
		ret = nonseekable_open(inode, file);
		asma = kmem_cache_zalloc(ashmem_area_cachep, GFP_KERNEL);
		<!--关键是初始化unpinned_list列表-->
		INIT_LIST_HEAD(&asma->unpinned_list);
		memcpy(asma->name, ASHMEM_NAME_PREFIX, ASHMEM_NAME_PREFIX_LEN);
		asma->prot_mask = PROT_MASK;
		file->private_data = asma;
		return 0;
	}

一开始都是pin的，看一下pin与unpin的调用范例：

	int ashmem_pin_region(int fd, size_t offset, size_t len)
	{
		struct ashmem_pin pin = { offset, len };
		return ioctl(fd, ASHMEM_PIN, &pin);
	}
	
	int ashmem_unpin_region(int fd, size_t offset, size_t len)
	{
		struct ashmem_pin pin = { offset, len };
		return ioctl(fd, ASHMEM_UNPIN, &pin);
	}
	
接着看ashmem_unpin

	static int ashmem_unpin(struct ashmem_area *asma, size_t pgstart, size_t pgend)
	{
		struct ashmem_range *range, *next;
		unsigned int purged = ASHMEM_NOT_PURGED;
		restart:
		list_for_each_entry_safe(range, next, &asma->unpinned_list, unpinned) {

			if (range_before_page(range, pgstart))
				break;

			if (page_range_subsumed_by_range(range, pgstart, pgend))
				return 0;
			if (page_range_in_range(range, pgstart, pgend)) {
				pgstart = min_t(size_t, range->pgstart, pgstart),
				pgend = max_t(size_t, range->pgend, pgend);
				purged |= range->purged;
				range_del(range);
				goto restart;
			}
		}
		return range_alloc(asma, range, purged, pgstart, pgend);
	}
	
这个函数主要作用是创建一个ashmem_range ，并插入ashmem_area的unpinned_list，在插入的时候可能会有合并为，这个时候要首先删除原来的unpin ashmem_range，之后新建一个合并后的ashmem_range插入unpinned_list。

![共享内存.jpg](http://upload-images.jianshu.io/upload_images/1460468-316969915c28af77.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

下面来看一下pin函数的实现，先理解了unpin，pin就很好理解了，其实就是将一块共享内存投入使用，如果它位于unpinedlist，就将它摘下来：

	static int ashmem_pin(struct ashmem_area *asma, size_t pgstart, size_t pgend)
	{
		struct ashmem_range *range, *next;
		int ret = ASHMEM_NOT_PURGED;
	
		list_for_each_entry_safe(range, next, &asma->unpinned_list, unpinned) {
			/* moved past last applicable page; we can short circuit */
			
			if (range_before_page(range, pgstart))
				break;
			if (page_range_in_range(range, pgstart, pgend)) {
				ret |= range->purged;
	
				if (page_range_subsumes_range(range, pgstart, pgend)) {
					range_del(range);
					continue;
				}
	
				if (range->pgstart >= pgstart) {
					range_shrink(range, pgend + 1, range->pgend);
					continue;
				}
				if (range->pgend <= pgend) {
					range_shrink(range, range->pgstart, pgstart-1);
					continue;
				}

				range_alloc(asma, range, range->purged,
					    pgend + 1, range->pgend);
				range_shrink(range, range->pgstart, pgstart - 1);
				break;
			}
		}
		return ret;
	}

![pin共享内存.jpg](http://upload-images.jianshu.io/upload_images/1460468-d8e15bd6fa8b5439.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# Android进程共享内存的传递-fd文件描述符的传递

原生Linux共享内存是通过传递已知的key来处理的，但是Android中不存在这种机制，Android是怎么处理的呢？那就是通过Binder传递文件描述符来处理，Android的Binder对于fd的传递也做了适配，原理其实就是**在内核层为要传递的目标进程转换fd**，因为在linux中fd只是对本进程是有效、且唯一，进程A打开一个文件得到一个fd，不能直接为进程B使用，因为B中那个fd可能压根无效、或者对应其他文件，不过，虽然同一个文件可以有多个文件描述符，但是文件只有一个，在内核层也只会对应一个inode节点与file对象，这也是内核层可以传递fd的基础，Binder驱动通过当前进程的fd找到对应的文件，然后为目标进程新建fd，并传递给目标进程，核心就是把进程A中的fd转化成进程B中的fd，看一下Android中binder的实现：

	void binder_transaction(){
	   ...
			case BINDER_TYPE_FD: {
			int target_fd;
			struct file *file;
			<!--关键点1 可以根据fd在当前进程获取到file ，多个进程打开同一文件，在内核中对应的file是一样-->
			file = fget(fp->handle);
			<!--关键点2,为目标进程获取空闲fd-->
			target_fd = task_get_unused_fd_flags(target_proc, O_CLOEXEC);
			<!--关键点3将目标进程的空闲fd与file绑定-->
			task_fd_install(target_proc, target_fd, file);
			fp->handle = target_fd;
		} break;	
		...
	 }
	
	<!--从当前进程打开的files中找到file在内核中的实例-->
	struct file *fget(unsigned int fd)
	{
		struct file *file;
		struct files_struct *files = current->files;
		rcu_read_lock();
		file = fcheck_files(files, fd);
		rcu_read_unlock();
		return file;
	}


	static void task_fd_install(
		struct binder_proc *proc, unsigned int fd, struct file *file)
	{
		struct files_struct *files = proc->files;
		struct fdtable *fdt;
		if (files == NULL)
			return;
		spin_lock(&files->file_lock);
		fdt = files_fdtable(files);
		rcu_assign_pointer(fdt->fd[fd], file);
		spin_unlock(&files->file_lock);
	}

![fd传递.jpg](http://upload-images.jianshu.io/upload_images/1460468-880df908d284fa9e.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# 为什么看不到匿名共享内存对应的文件呢

为什么Android用户看不到共享内存对应的文件，Google到的说法是：在内核没有定义defined(CONFIG_TMPFS) 情况下，tmpfs对用户不可见：

If CONFIG_TMPFS is not set, the user visible part of tmpfs is not build. But the internal mechanisms are always present.

而在Android的shmem.c驱动中确实没有defined(CONFIG_TMPFS) ，这里只是猜测，也许还有其他解释，如有了解，望能指导。

# 总结

Android匿名共享内存是基于Linux共享内存的，都是在tmpfs文件系统上新建文件，并将其映射到不同的进程空间，从而达到共享内存的目的，只是Android在Linux的基础上进行了改造，借助Binder+文件描述符实现了共享内存的传递。

# 参考文档

[Android Binder 分析——匿名共享内存（Ashmem）](http://light3moon.com/2015/01/28/Android%20Binder%20%E5%88%86%E6%9E%90%E2%80%94%E2%80%94%E5%8C%BF%E5%90%8D%E5%85%B1%E4%BA%AB%E5%86%85%E5%AD%98%5BAshmem%5D/)