OutOfMemoryError是一种Error类型，OutOfMemoryError extends VirtualMachineError ，此种错误不应该被捕获，友好的退出，并找到问题是比较合理的处理方案。Android中发生OOM常见场景主要有两种

*  JVM堆中创建对象或者分配内存失败
*  JVM创建线程失败

JAVA堆内存使用超限是最常见的，虽然native的内存也会超，但这个是偶已经接近系统极限了，一般会直接崩掉，不会再抛出JAVA OOM。虽然native层向上抛出OOM之后，进程还是存活的，甚至UI界面还能继续操作，但是后续的分配内存之类的操作都会失败，因为虚拟机的内存已经不足了，而且已经经历了各种GC，还是无法满足需求，最好是搜集问题，然后友好退出。


## Android的OOM的异常如何抛出的？哪里抛出的？

Android 虚拟机最终抛出OutOfMemoryError的地方实在thread.cc代码中

		thread.cc代码
		 
		void Thread::ThrowOutOfMemoryError(const char* msg) {
		  LOG(WARNING) << "Throwing OutOfMemoryError "
		               << '"' << msg << '"'
		               << " (VmSize " << GetProcessStatus("VmSize")
		               << (tls32_.throwing_OutOfMemoryError ? ", recursive case)" : ")");
		  ScopedTrace trace("OutOfMemoryError");
		  if (!tls32_.throwing_OutOfMemoryError) {
		    tls32_.throwing_OutOfMemoryError = true;
		    ThrowNewException("Ljava/lang/OutOfMemoryError;", msg);
		    tls32_.throwing_OutOfMemoryError = false;
		  } else {
		    Dump(LOG_STREAM(WARNING));  // The pre-allocated OOME has no stack, so help out and log one.
		    SetException(Runtime::Current()->GetPreAllocatedOutOfMemoryErrorWhenThrowingOOME());
		  }
		}

JVM虚拟机会在native层抛出OOM异常，并封装成OutOfMemoryError。

	void Thread::ThrowNewWrappedException(const char* exception_class_descriptor,
	                                      const char* msg) {
	  DCHECK_EQ(this, Thread::Current());
	  ScopedObjectAccessUnchecked soa(this);
	  StackHandleScope<3> hs(soa.Self());
		  ScopedDisablePublicSdkChecker sdpsc;
	  Handle<mirror::ClassLoader> class_loader(hs.NewHandle(GetCurrentClassLoader(soa.Self())));
	  ScopedLocalRef<jobject> cause(GetJniEnv(), soa.AddLocalReference<jobject>(GetException()));
	  ClearException();
	  Runtime* runtime = Runtime::Current();
	  auto* cl = runtime->GetClassLinker();
	  Handle<mirror::Class> exception_class(
	  <!--构造java的对象，-->
	      hs.NewHandle(cl->FindClass(this, exception_class_descriptor, class_loader)));
	  if (UNLIKELY(exception_class == nullptr)) {
	    CHECK(IsExceptionPending());
	    return;
	  }


## 最常见的JVM堆内存分配失败调用ThrowOutOfMemoryError

> /art/runtime/gc/heap.cc

	void Heap::ThrowOutOfMemoryError(Thread* self, size_t byte_count, AllocatorType allocator_type) {
	
	
	  // If we're in a stack overflow, do not create a new exception. It would require running the
	  // constructor, which will of course still be in a stack overflow.
	  if (self->IsHandlingStackOverflow()) {
	    self->SetException(
	        Runtime::Current()->GetPreAllocatedOutOfMemoryErrorWhenHandlingStackOverflow());
	    return;
	  }
	  // Allow plugins to intercept out of memory errors.
	  Runtime::Current()->OutOfMemoryErrorHook();
	
	  std::ostringstream oss;
	  
	  <!--先获取可用内存-->
	  size_t total_bytes_free = GetFreeMemory();
	  oss << "Failed to allocate a " << byte_count << " byte allocation with " << total_bytes_free
	      << " free bytes and " << PrettySize(GetFreeMemoryUntilOOME()) << " until OOM,"
	      << " target footprint " << target_footprint_.load(std::memory_order_relaxed)
	      << ", growth limit "
	      << growth_limit_;
	   
	   <!--内存碎片化严重 ，打印出最大的连续内存-->
	  // If the allocation failed due to fragmentation, print out the largest continuous allocation.
	  <!--此时 可分配的内存大于待分配内存，只是，缺少连续的虚拟内存块-->
	  if (total_bytes_free >= byte_count) {
	    space::AllocSpace* space = nullptr;
	    if (allocator_type == kAllocatorTypeNonMoving) {
	      space = non_moving_space_;
	    } else if (allocator_type == kAllocatorTypeRosAlloc ||
	               allocator_type == kAllocatorTypeDlMalloc) {
	      space = main_space_;
	    } else if (allocator_type == kAllocatorTypeBumpPointer ||
	               allocator_type == kAllocatorTypeTLAB) {
	      space = bump_pointer_space_;
	    } else if (allocator_type == kAllocatorTypeRegion ||
	               allocator_type == kAllocatorTypeRegionTLAB) {
	      space = region_space_;
	    }
	
	    // There is no fragmentation info to log for large-object space.
	    if (allocator_type != kAllocatorTypeLOS) {
	      CHECK(space != nullptr) << "allocator_type:" << allocator_type
	                              << " byte_count:" << byte_count
	                              << " total_bytes_free:" << total_bytes_free;
	      if (!space->LogFragmentationAllocFailure(oss, byte_count)) {
	        oss << "; giving up on allocation because <"
	            << kMinFreeHeapAfterGcForAlloc * 100
	            << "% of heap free after GC.";
	      }
	    }
	  }
	  self->ThrowOutOfMemoryError(oss.str().c_str());
	}

JVM分配堆内存失败后，会抛出OOM：

* 1、首先获取可用内存数量，GetFreeMemory，准备好打印的日志，如果剩余内存不足直接打印，否则区分是碎片问题，还是余量问题
* 2、其次判断total_bytes_free是否大于待分配内存，可以辨别是否是因为碎片化问题导致的分配失败，如果是多增加一行碎片的问题。
* 3、如果也不是连续内存不足导致的失败，打印出来是不是不满足最小余量的要求：分配后，还要保持一个可用的余量。kMinFreeHeapAfterGcForAlloc不满足

如果没有足够大小的连续地址空间。这种情况一般是进程中存在大量的内存碎片导致的，其堆栈信息会比第一种OOM堆栈多出一段信息：主要是duo to fragmentation，并展示最大的连续内存块。

	failed due to fragmentation (required continguous free “<< required_bytes << “ bytes for a new buffer where largest contiguous free ” << largest_continuous_free_pages << “ bytes)”; 

其详细代码在art/runtime/gc/allocator/rosalloc.cc中

	bool RosAlloc::LogFragmentationAllocFailure(std::ostream& os, size_t failed_alloc_bytes) {
	  Thread* self = Thread::Current();
	  size_t largest_continuous_free_pages = 0;
	  WriterMutexLock wmu(self, bulk_free_lock_);
	  MutexLock mu(self, lock_);
	  uint64_t total_free = 0;
	  for (FreePageRun* fpr : free_page_runs_) {
	    largest_continuous_free_pages = std::max(largest_continuous_free_pages,
	                                             fpr->ByteSize(this));
	    total_free += fpr->ByteSize(this);
	  }
	  size_t required_bytes = 0;
	  const char* new_buffer_msg = "";
	  <!--大内存有个阈值进行区分-->
	  if (failed_alloc_bytes > kLargeSizeThreshold) {
	    // Large allocation.
	    required_bytes = RoundUp(failed_alloc_bytes, gPageSize);
	  } else {
	    // Non-large allocation.
	    required_bytes = numOfPages[SizeToIndex(failed_alloc_bytes)] * gPageSize;
	    new_buffer_msg = " for a new buffer";
	  }
	  <!--需要的内存大于最大的连续内存-->
	  if (required_bytes > largest_continuous_free_pages) {
	    os << "; failed due to fragmentation ("
	       << "required contiguous free " << required_bytes << " bytes" << new_buffer_msg
	       << ", largest contiguous free " << largest_continuous_free_pages << " bytes"
	       << ", total free pages " << total_free << " bytes"
	       << ", space footprint " << footprint_ << " bytes"
	       << ", space max capacity " << max_capacity_ << " bytes"
	       << ")" << std::endl;
	    return true;
	  }
	  return false;
	}

如果有足够的连续内存，但是分配后剩余的可用内存不足，也会OOM，提醒剩余的不足1%

	
	java.lang.OutOfMemoryError: Failed to allocate a 16 byte allocation with 18384 free bytes and 17KB until OOM, target footprint 536870912, growth limit 536870912; giving up on allocation because <1% of heap free after GC.


但是基本上都是Java内存用完，细分意义不大。

## new Thread创建线程失败抛出的OOM：创建线程过程中发生OOM,多是因为进程内的**虚拟内存**耗尽了

**虚拟内存不足，并不一定是实际内存不足，仅仅是虚拟内存不足**，现在一般是64位的芯片，操作系统可以用的虚拟内存很多，linux中一般是用48位，Android其实默认使用的是39位，实际可能会设置的更低。但是也远超实际物理内存，线程创建的异常日志一般如下：

	Throwing OutOfMemoryError "pthread_create (1040KB stack) failed: Try again" (VmSize 145637572 kB)

	 java.lang.OutOfMemoryError: pthread_create (1040KB stack) failed: Try again
	                                                                                                    
一般是创建的线程太多，导致虚拟内存不足，创建失败导致的。 对应的代码：

> java_lang_Thread.cc

	static void Thread_nativeCreate(JNIEnv* env, jclass, jobject java_thread, jlong stack_size,
	                                jboolean daemon) {
	  // There are sections in the zygote that forbid thread creation.
	  Runtime* runtime = Runtime::Current();
	  if (runtime->IsZygote() && runtime->IsZygoteNoThreadSection()) {
	    jclass internal_error = env->FindClass("java/lang/InternalError");
	    CHECK(internal_error != nullptr);
	    env->ThrowNew(internal_error, "Cannot create threads in zygote");
	    return;
	  }
	  Thread::CreateNativeThread(env, java_thread, stack_size, daemon == JNI_TRUE);
	}

Thread.cpp实现

		void Thread::CreateNativeThread(JNIEnv* env, jobject java_peer, size_t stack_size, bool is_daemon) {
		  CHECK(java_peer != nullptr);
		  Thread* self = static_cast<JNIEnvExt*>(env)->GetSelf();
		
		...
		<!--先分配JNIEnvExt 看看内存是否足够，看看是否已经OOM -->
		  // Try to allocate a JNIEnvExt for the thread. We do this here as we might be out of memory and
		  // do not have a good way to report this on the child's side.
		  std::string error_msg;
		  std::unique_ptr<JNIEnvExt> child_jni_env_ext(
		      JNIEnvExt::Create(child_thread, Runtime::Current()->GetJavaVM(), &error_msg));
		
		  int pthread_create_result = 0;
		  
		  <!--JNIEnvExt如果分配成功，一定程度说明内存还是有的，再调用pthread_create分配线程-->
		  if (child_jni_env_ext.get() != nullptr) {
		    pthread_t new_pthread;
		    pthread_attr_t attr;
		    child_thread->tlsPtr_.tmp_jni_env = child_jni_env_ext.get();
		    CHECK_PTHREAD_CALL(pthread_attr_init, (&attr), "new thread");
		    CHECK_PTHREAD_CALL(pthread_attr_setdetachstate, (&attr, PTHREAD_CREATE_DETACHED),
		                       "PTHREAD_CREATE_DETACHED");
		    CHECK_PTHREAD_CALL(pthread_attr_setstacksize, (&attr, stack_size), stack_size);
		    pthread_create_result = pthread_create(&new_pthread,
		                                           &attr,
		                                           gUseUserfaultfd ? Thread::CreateCallbackWithUffdGc
		                                                           : Thread::CreateCallback,
		                                           child_thread);
		    CHECK_PTHREAD_CALL(pthread_attr_destroy, (&attr), "new thread");
		<!--如果成功就直接返回-->
		    if (pthread_create_result == 0) {
		      // pthread_create started the new thread. The child is now responsible for managing the
		      // JNIEnvExt we created.
		      // Note: we can't check for tmp_jni_env == nullptr, as that would require synchronization
		      //       between the threads.
		      child_jni_env_ext.release();  // NOLINT pthreads API.
		      return;
		    }
		  }
		...
		  SetNativePeer(env, java_peer, nullptr);
		  {
		  <!--根据child_jni_env_ext.get的返回判断是哪种错误导致的异常但是最终都会抛出ThrowOutOfMemoryError错误 -->
		    std::string msg(child_jni_env_ext.get() == nullptr ?
		        StringPrintf("Could not allocate JNI Env: %s", error_msg.c_str()) :
		        StringPrintf("pthread_create (%s stack) failed: %s",
		                                 PrettySize(stack_size).c_str(), strerror(pthread_create_result)));
		    ScopedObjectAccess soa(env);
		    soa.Self()->ThrowOutOfMemoryError(msg.c_str());
		  }
		}

 
可以看到主要分为两步，第一步创建JNIEnv，第二步调用pthread_create真正的clone一个线程，这两块也是抛出OOM的节点，如果失败，在结尾的地方会抛出不同的异常，其实就两种，第一种JNIENV对象分配失败，直接说是内存OOM，另一种pthread_create失败，也是OOM，但是可能虚拟内存不足，也可能数量超限，对应错误的枚举如下

* 	__BIONIC_ERRDEF( EBADF          ,   9, "Bad file descriptor" )
* 	__BIONIC_ERRDEF( ECHILD         ,  10, "No child processes"	
* 	__BIONIC_ERRDEF( EAGAIN         ,  11, "Try again" )
* 	__BIONIC_ERRDEF( ENOMEM         ,  12, "Out of memory" )

* 	__BIONIC_ERRDEF( EMFILE         ,  24, "Too many open files" )

先看第一类	JNIEnvExt创建失败，一般会包含”Could not allocate JNI Env“这样的日志
                                                           
         Could not allocate JNI Env: %s", error_msg.c_str()                                         
 
	    
创建JNIEnv可以归为两个步骤：

* 通过Andorid的匿名共享内存（Anonymous Shared Memory）分配 4KB（一个page）内核态内存。失败的话一般对应fd不足
* 再通过Linux的mmap调用映射到用户态虚拟内存地址空间，既然是共享内存，失败的话一般对应用户 虚拟内存不足


创建共享内存的时候，art/runtime/mem_map.cc： 调用ashmem_create_region打开/dev/ashmem设备文件【linux驱动】，需要一个FD（文件描述符）
		
		ashmemfd.reset(ashmem_create_region(debug_friendly_name.c_str(), page_aligned_byte_count));

ashmem_create_region打开设备文件，并分配新的文件描述符，创建的fd数已经达到上限，怎失败，系统允许的文件描述数量有限，每个进程分配的业有限，超出会抛出OOM一场，此时对应fd不足，可以通过adb 查看fd数量上限，系统的设置

	 cat /proc/sys/fs/file-max
	171817   //系统
	ulimit -n
	32768 //进程
	
对应日志一般如下

		E/art: ashmem_create_region failed for 'indirect ref table': Too many open files
		 java.lang.OutOfMemoryError: Could not allocate JNI Env

     		
第二步，调用mmap映射到用户态内存地址空间也可能出错：
		
		void* actual = MapInternal(..., fd.get(), ...);	
		E/art: Failed anonymous mmap(0x0, 8192, 0x3, 0x2, 116, 0): Operation not permitted. See process maps in the log.
		java.lang.OutOfMemoryError: Could not allocate JNI Env

这个时候可能是虚拟内存不足MemMap MapAnonymous代码
	  
	MemMap MemMap::MapAnonymous(const char* name,
	                            uint8_t* addr,
	                            size_t byte_count,
	                            int prot,
	                            bool low_4gb,
	                            bool reuse,
	                            /*inout*/MemMap* reservation,
	                            /*out*/std::string* error_msg,
	                            bool use_debug_name) {
		..
	
	  unique_fd fd;
	
	  // We need to store and potentially set an error number for pretty printing of errors
	  int saved_errno = 0;
	 <!--映射-->
	  void* actual = MapInternal(addr,
	                             page_aligned_byte_count,
	                             prot,
	                             flags,
	                             fd.get(),
	                             0,
	                             low_4gb);
	  saved_errno = errno;
	
	  if (actual == MAP_FAILED) {
	  <!--映射失败-->
	    if (error_msg != nullptr) {
	      PrintFileToLog("/proc/self/maps", LogSeverity::WARNING);
	      *error_msg = StringPrintf("Failed anonymous mmap(%p, %zd, 0x%x, 0x%x, %d, 0): %s. "
	                                    "See process maps in the log.",
	                                addr,
	                                page_aligned_byte_count,
	                                prot,
	                                flags,
	                                fd.get(),
	                                strerror(saved_errno));
	    }
	    return Invalid();
	  }
	  <!--映射成功-->
	  return MemMap(name,
	                reinterpret_cast<uint8_t*>(actual),
	                byte_count,
	                actual,
	                page_aligned_byte_count,
	                prot,
	                reuse);
	}	  
	   
如果JNI Env创建成功，则会走  pthread_create开始真正创建线程。

>   pthread_create 创建线程失败
  
pthread_create创建线程也可以归纳为两个步骤：

* 1 调用mmap分配为线程分配栈内存。这里mmap flag中指定了MAP_ANONYMOUS，即匿名内存映射。这是在Linux中分配大块内存的常用方式。其分配的是虚拟内存，对应页的物理内存并不会立即分配，而是在用到的时候触发内核的缺页中断，然后中断处理函数再分配物理内存。

* 2 调用clone方法进行线程创建。

第一步分配栈内存失败是由于进程的虚拟内存不足，抛出错误信息如下： 

	W/libc: pthread_create failed: couldn't allocate 1073152-bytes mapped space: Out of memory
	W/tch.crowdsourc: Throwing OutOfMemoryError with VmSize  4191668 kB  "pthread_create (1040KB stack) failed: Try again"
	java.lang.OutOfMemoryError: pthread_create (1040KB stack) failed: Try again
	        at java.lang.Thread.nativeCreate(Native Method)
	        at java.lang.Thread.start(Thread.java:753)
        
第二步clone方法失败是因为线程数超出了限制，抛出错误信息如下：

	W/libc: pthread_create failed: clone failed: Out of memory
	W/art: Throwing OutOfMemoryError "pthread_create (1040KB stack) failed: Out of memory"
	java.lang.OutOfMemoryError: pthread_create (1040KB stack) failed: Out of memory
	  at java.lang.Thread.nativeCreate(Native Method)
	  at java.lang.Thread.start(Thread.java:1078)
	  
有failed: Try again一半是虚拟内存，如果线程超过直接是 Out of memory

	  
可以通过adb 获取当前进程的虚拟内存大小
	
	cat /proc/pid/status  
	
 输出
 
	Name:	ail.labaffinity
	Umask:	0077
	State:	S (sleeping)
	Tgid:	15164
	Ngid:	0
	Pid:	15164
	PPid:	353
	TracerPid:	0
	Uid:	10194	10194	10194	10194
	Gid:	10194	10194	10194	10194
	FDSize:	1024
	Groups:	3003 9997 20194 50194 
	VmPeak:	22982960 kB
	VmSize:	21760228 kB
	VmLck:	       0 kB
	VmPin:	       0 kB
	VmHWM:	  141276 kB
	VmRSS:	  141044 kB  		实际使用内存  对于单个进程的内存使用大小， RSS 不是一个精确的描述。RSS易被误导的原因在于， 它包括了该进程所使用的所有共享库的全部内存大小。对于单个共享库， 尽管无论多少个进程使用，实际该共享库只会被装入内存一次。
	RssAnon:	   48788 kB
	RssFile:	   91416 kB
	RssShmem:	     840 kB
	VmData:	 5986008 kB  虚拟内存
	VmStk:	    8192 kB
	VmExe:	       4 kB
	VmLib:	  146964 kB
	VmPTE:	    1704 kB
	VmSwap:	   21296 kB
	CoreDumping:	0
	THP_enabled:	1
	Threads:	135
	SigQ:	0/6717
	SigPnd:	0000000000000000
	ShdPnd:	0000000000000000
	SigBlk:	0000000080001204
	SigIgn:	0000000000000001
	SigCgt:	0000006e400084f8
	CapInh:	0000000000000000
	CapPrm:	0000000000000000
	CapEff:	0000000000000000
	CapBnd:	0000000000000000
	CapAmb:	0000000000000000
	NoNewPrivs:	0
	Seccomp:	2
	Seccomp_filters:	1
	Speculation_Store_Bypass:	thread vulnerable
	SpeculationIndirectBranch:	unknown
	Cpus_allowed:	f
	Cpus_allowed_list:	0-3
	Mems_allowed:	1
	Mems_allowed_list:	0
	voluntary_ctxt_switches:	226
	nonvoluntary_ctxt_switches:	218
	
	Name：       进程的名称，例如"java"或"bash"。
	State：      进程的状态，例如"running"或"sleeping"。
	Tgid：       进程组ID，即进程的ID号。
	Pid：        进程的ID号。
	PPid：       父进程的ID号。
	TracerPid：  跟踪进程的ID号。
	Uid：        进程的用户ID号。
	Gid：        进程的组ID号。
	FDSize： 进程的文件描述符数。
	Groups： 进程所属的组ID号列表。
	VmPeak： 进程的虚拟内存峰值，即进程使用的最大内存大小。
	VmSize： 进程的虚拟内存大小，即进程实际使用的内存大小。
	VmLck：      进程的虚拟内存锁定大小，即进程被锁定的内存大小。
	VmHWM：      进程的虚拟内存高水位线，即进程使用的最大内存大小。
	VmRSS：      进程的实际内存大小，即进程在物理内存中的大小。
	RssAnon：    进程的非映射内存大小，即进程的匿名内存大小。
	RssFile：    进程的映射文件大小，即进程的文件映射内存大小。
	RssShmem：   进程的共享内存大小，即进程的共享内存大小。
	VmData： 	   进程的数据段内存大小，即进程使用的数据段内存大小。 虚拟内存的大小,不是实际
	VmStk：      进程的堆栈内存大小，即进程使用的堆栈内存大小。
	VmExe：      进程的可执行文件大小，即进程使用的可执行文件大小。
	VmLib：      进程的库文件大小，即进程使用的库文件大小。
	VmPTE：      进程的页表项大小，即进程使用的页表项大小。
	Threads：    进程的线程数。
	SigQ：       进程的信号队列大小。
	SigPnd： 		进程的等待信号列表。
	ShdPnd： 		进程的等待共享内存列表。
	SigBlk： 		进程的阻塞信号列表。
	SigIgn： 		进程的忽略信号列表。
	SigCgt： 进程的当前信号掩码。
	CapInh： 进程的继承能力。
	CapPrm： 进程的 permitted 能力。
	CapEff： 进程的有效能力。
	Cpus_allowed：进程可以使用的CPU列表。
	Mems_allowed：进程可以使用的内存列表。
	Voluntary_ctxt_switches：进程主动进行的上下文切换次数。
	Nonvoluntary_ctxt_switches：进程被动进行的上下文切换次数。


一般而言，都是虚拟内存先消耗完 ，而不是达到Thread的上限，虽然线程创建之初虚拟内存说是占用1M，但是那个应该只是栈内存，实际要大于他，测试发现 在pixel4 ，android 13的手机上，一个线程会平均增加20M的虚拟内存，注意是虚拟内存，VmSize会增加，但实际上VmRSS增加有限。


### 一个线程占用的内存

首先有个概念：虚拟内存跟实际占用的物理内存不同，比如线程创建，会分配给栈1M的虚拟内存，但是其实并未直接就分配1M物理内存给他，

cat /proc/pid/maps可以查看maps虚拟内存信息 ，可以看到一个线程的栈占用的虚拟内存1M

	73b566f000-73b5670000 ---p 00000000 00:00 0                              [anon:stack_and_tls:15295]
	73b5670000-73b5777000 rw-p 00000000 00:00 0                              [anon:stack_and_tls:15295]
	73b5777000-73b5779000 ---p 00000000 00:00 0 
	73b5779000-73b577a000 ---p 00000000 00:00 0                              [anon:stack_and_tls:15294]
	73b577a000-73b5881000 rw-p 00000000 00:00 0                              [anon:stack_and_tls:15294]
 
 cat /proc/pid/smaps更详细 
 
	73b73f2000-73b74f9000 rw-p 00000000 00:00 0                              [anon:stack_and_tls:15282]  本段虚拟内存的地址范围
	Size:               1052 kB    相应虚拟地址空间的大小
	KernelPageSize:        4 kB
	MMUPageSize:           4 kB
	Rss:                  48 kB    正在使用的物理内存的大小
	Pss:                  48 kB
	Pss_Dirty:            48 kB
	Shared_Clean:          0 kB  Rss中和其他进程共享的未使用页数
	Shared_Dirty:          0 kB  Rss和其他进程共享已经使用的页数
	Private_Clean:         0 kB  Rss私有区域未使用的页数
	Private_Dirty:        48 kB   Rss私有区域已经使用的页数
	Referenced:           48 kB
	Anonymous:            48 kB
	LazyFree:              0 kB

可以看到1M的虚拟内存，真是使用的	Rss只有48K，但是一个线程其实使用的虚拟内存不止1M，甚至是10M 20M。
	

## dumpsys meminfo查看内存信息查看

	dumpsys meminfo 15164
	Applications Memory Usage (in Kilobytes):
	Uptime: 24885051 Realtime: 24885051
	
	** MEMINFO in pid 15164 [  ] **
	
	                   Pss  Private  Private  SwapPss      Rss     Heap     Heap     Heap
	                 Total    Dirty    Clean    Dirty    Total     Size    Alloc     Free
	                ------   ------   ------   ------   ------   ------   ------   ------
	  Native Heap    16502    16380       40       57    17428  4121408  4115303     2038
	  Dalvik Heap     3364     3316       12      226     3832     7207     3604     3603
	 Dalvik Other     2449     2372        0       11     2704                           
	        Stack     5776     5776        0        0     5784    
		  //共享                       
	       Ashmem       29        0        0        0      392                           
	    Other dev       22       12        4        0      316    
	     //存在很多共享                      
	     .so mmap    20937      564    15824       25    48896   
	     //存在很多共享                        
	    .jar mmap     5736        0     1456        0    36224                           
	    .apk mmap      969       24      260        0     2824                           
	    .ttf mmap      323        0        0        0      976                           
	    .dex mmap       84       20       44        0      708                           
	    .oat mmap      100        0        8        0     1528                           
	    .art mmap    10216     8452      900      219    20264                           
	   Other mmap     3462        8     3408        0     4308                           
	      Unknown    13348    12596      740        0    13692                           
        	TOTAL    83855    49520    22696      538   159876  4128615  4118907     5641
        	
比较关键的三个Pss  Rss   Uss

* VSS - Virtual Set Size 虚拟耗用内存（包含共享库占用的内存）  
* RSS - Resident Set Size 实际使用物理内存（包含共享库占用的内存） 100%包含 
* PSS - Proportional Set Size 实际使用的物理内存（比例分配共享库占用的内存）按比例 PSS (Proportional Set Size) = 进程独占的内存 + 进程程共享的内存 / 映射次数。
内存的管理是以 page 为单位的, 如果 page 的 _refcount或者 _mapcount为 1, 那么就是进程独占的内存. 也叫 private. 如果 page 的 _mapcount 为 n ( n >= 2), 这块内存以 page / n 来统计 
* USS - Unique Set Size 进程独自占用的物理内存（不包含共享库占用的内存）

App Summary其实就关心两个Pss Rss ，跟上面的有换算关系 尤其关注Private Dirty Private Clean
  
	 App Summary
	                       Pss(KB)                        Rss(KB)
	                        ------                         ------
	           Java Heap:    12668                          24096
	         Native Heap:    16380                          17428
	                Code:    18200                          91308    所有私有静态资源求和。
	               Stack:     5776                           5784    进程本身栈占用的大小。
	            Graphics:        0                              0
	       Private Other:    19192 
	              System:    11639     			Pss Total 的和-Private Dirty 和Private Clean
	             Unknown:                                   21260
	 
	           TOTAL PSS:    83855            TOTAL RSS:   159876       TOTAL SWAP PSS:      538
	 
	 Objects
	               Views:       23         ViewRootImpl:        1
	         AppContexts:        5           Activities:        1
	              Assets:       18        AssetManagers:        0
	       Local Binders:       12        Proxy Binders:       37
	       Parcel memory:        4         Parcel count:       18
	    Death Recipients:        0             WebViews:        0
	 
	 SQL
	         MEMORY_USED:        0
	  PAGECACHE_OVERFLOW:        0          MALLOC_SIZE:        0
  
*   Java Heap  :除了分配到的对象，还是有一些初始化时候带的，Zygote堆+Active堆，分配对象都在Active，OOM也多是这原因，但是也有固定的损耗，比如系统共享加载的东西
   
		// dalvik private_dirty
		 dalvikPrivateDirty
		// art mmap private_dirty + private_clean  
		+ getOtherPrivate(OTHER_ART);
  
dalvik private dirty包含任何写过zygote分配的页面(应用是从zygote fork 出来的), 和应用本身分配的。art mmap是应用的bootimage,任何private页面也算在应用上。

.art mmap，这是 Heap 映像（Image） 占用的 RAM 容量，根据由多个应用共用的预加载类计算，此映像（Image）在所有应用之间共享，不受特定应用影响。尽管 ART 映像（Image）包含 Object 实例，但它不会计入您的堆（Heap）占用空间。

*   Native Heap 通过libc_malloc库分配的大小
 
		 nativePrivateDirty; // libc_malloc
  
*   Code：所有私有静态资源求和。
 
		 	// so mmap private_dirty + private_clean  
			 getOtherPrivate(OTHER_SO)  
			// jar mmap private_dirty + private_clean    
			+ getOtherPrivate(OTHER_JAR)
			// apk mmap private_dirty + private_clean      
			+ getOtherPrivate(OTHER_APK)
			// ttf mmap private_dirty + private_clean      
			+ getOtherPrivate(OTHER_TTF)
			// dex mmap private_dirty + private_clean      
			+ getOtherPrivate(OTHER_DEX)
			// oat mmap private_dirty + private_clean
			+ getOtherPrivate(OTHER_OAT);

*  Stack 进程本身栈占用的大小。

		getOtherPrivateDirty(OTHER_STACK);


* Graphic

		//Gfx Dev private_dirty + private_clean
		getOtherPrivate(OTHER_GL_DEV)
		// EGL mtrack private_dirty + private_clean      
		+ getOtherPrivate(OTHER_GRAPHICS)
		// GL mtrack private_dirty + private_clean      
		+ getOtherPrivate(OTHER_GL);
		 

进程在GPU上分配的内存。

* System


		Pss Total 的和- Private Dirty 和Private Clean 的和

系统占用的内存,例如一些共享的字体,图像资源等。


      public int getSummaryJavaHeap() {
            return dalvikPrivateDirty + getOtherPrivate(OTHER_ART);
        }
      
       public int getOtherPrivate(int which) {
          return getOtherPrivateClean(which) + getOtherPrivateDirty(which);
        }
        
	  
###  FD数超出限制如何监听

在后台启动一个线程，每隔一段时间读取一次当前进程创建的FD数量，当检测到FD数量达到阈值时（FD最大限制的95%），读取当前进程的所有FD信息归并后上报。FD的用途主要有打开文件、创建socket连接、创建handlerThread等。

### 线程创建过多如何监听 

参考FD，每隔一段时间获取线程数量，超过一定的阈值可以进行线程搜集，并归因定位。一般来说，当进程中线程数异常增多时，都是某一类线程被大量的重复创建。所以我们只需要定位到这类线程的创建时机，就能知道问题所在。如果线程是有自定义名称的，那么直接就可以在代码中搜索到创建线程的位置，从而定位问题，如果线程创建时没有指定名称，那么就需要通过该线程的堆栈信息来辅助定位。
	  
### 	OOM能捕获吗？之后系统就会死吗？

通过JVM 抛出的OOM能捕获，以为抛出的时候进程还活着呢，如果不处理，还不会死，甚至UI线程还可以继续操作，但是后续的内存分配会挂掉，运行环境已经岌岌可危，而且这个时候的很多操作也会因为内存不足而失败。

### OOM监听机制

起一个服务，定时查询，有必要就聚合上报。Debug的meminfo能拿到基本的memx信息

    public static class MemoryInfo implements Parcelable {
        /** The proportional set size for dalvik heap.  (Doesn't include other Dalvik overhead.) */
        public int dalvikPss;
        /** The proportional set size that is swappable for dalvik heap. */
        /** @hide We may want to expose this, eventually. */
        @UnsupportedAppUsage
        public int dalvikSwappablePss;
        /** @hide The resident set size for dalvik heap.  (Without other Dalvik overhead.) */
        @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.R, trackingBug = 170729553)
        public int dalvikRss;
        /** The private dirty pages used by dalvik heap. */
        public int dalvikPrivateDirty;
        /** The shared dirty pages used by dalvik heap. */
        public int dalvikSharedDirty;
        /** The private clean pages used by dalvik heap. */
        /** @hide We may want to expose this, eventually. */
        @UnsupportedAppUsage
        public int dalvikPrivateClean;
        /** The shared clean pages used by dalvik heap. */
        /** @hide We may want to expose this, eventually. */
        @UnsupportedAppUsage
        public int dalvikSharedClean;
        /** The dirty dalvik pages that have been swapped out. */
        /** @hide We may want to expose this, eventually. */
        @UnsupportedAppUsage
        public int dalvikSwappedOut;
        /** The dirty dalvik pages that have been swapped out, proportional. */
        /** @hide We may want to expose this, eventually. */
        @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.R, trackingBug = 170729553)
        public int dalvikSwappedOutPss;

        /** The proportional set size for the native heap. */
        public int nativePss;
        /** The proportional set size that is swappable for the native heap. */
        /** @hide We may want to expose this, eventually. */
        @UnsupportedAppUsage
        public int nativeSwappablePss;
        /** @hide The resident set size for the native heap. */
        @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.R, trackingBug = 170729553)
        public int nativeRss;
        /** The private dirty pages used by the native heap. */
        public int nativePrivateDirty;
        /** The shared dirty pages used by the native heap. */
        public int nativeSharedDirty;
        /** The private clean pages used by the native heap. */
        /** @hide We may want to expose this, eventually. */
        @UnsupportedAppUsage
        public int nativePrivateClean;
        /** The shared clean pages used by the native heap. */
        /** @hide We may want to expose this, eventually. */
        @UnsupportedAppUsage
        public int nativeSharedClean;
        /** The dirty native pages that have been swapped out. */
        /** @hide We may want to expose this, eventually. */
        @UnsupportedAppUsage
        public int nativeSwappedOut;
        /** The dirty native pages that have been swapped out, proportional. */
        /** @hide We may want to expose this, eventually. */
        @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.R, trackingBug = 170729553)
        public int nativeSwappedOutPss;

        /** The proportional set size for everything else. */
        public int otherPss;
        /** The proportional set size that is swappable for everything else. */
        /** @hide We may want to expose this, eventually. */
        @UnsupportedAppUsage
        public int otherSwappablePss;
        /** @hide The resident set size for everything else. */
        @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.R, trackingBug = 170729553)
        public int otherRss;
        /** The private dirty pages used by everything else. */
        public int otherPrivateDirty;
        /** The shared dirty pages used by everything else. */
        public int otherSharedDirty;
        /** The private clean pages used by everything else. */
        /** @hide We may want to expose this, eventually. */
        @UnsupportedAppUsage
        public int otherPrivateClean;
        /** The shared clean pages used by everything else. */
        /** @hide We may want to expose this, eventually. */
        @UnsupportedAppUsage
        public int otherSharedClean;
        /** The dirty pages used by anyting else that have been swapped out. */
        /** @hide We may want to expose this, eventually. */
        @UnsupportedAppUsage
        public int otherSwappedOut;
        /** The dirty pages used by anyting else that have been swapped out, proportional. */
        /** @hide We may want to expose this, eventually. */
        @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.R, trackingBug = 170729553)
        public int otherSwappedOutPss;

        /** Whether the kernel reports proportional swap usage */
        /** @hide */
        @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.R, trackingBug = 170729553)
        public boolean hasSwappedOutPss;

        /** @hide */
        public static final int HEAP_UNKNOWN = 0;
        /** @hide */
        public static final int HEAP_DALVIK = 1;
        /** @hide */
        public static final int HEAP_NATIVE = 2;

        /** @hide */
        public static final int OTHER_DALVIK_OTHER = 0;
        /** @hide */
        public static final int OTHER_STACK = 1;
        /** @hide */
        public static final int OTHER_CURSOR = 2;
        /** @hide */
        public static final int OTHER_ASHMEM = 3;
        /** @hide */
        public static final int OTHER_GL_DEV = 4;
        /** @hide */
        public static final int OTHER_UNKNOWN_DEV = 5;
        /** @hide */
        public static final int OTHER_SO = 6;
        /** @hide */
        public static final int OTHER_JAR = 7;
        /** @hide */
        public static final int OTHER_APK = 8;
        /** @hide */
        public static final int OTHER_TTF = 9;
        /** @hide */
        public static final int OTHER_DEX = 10;
        /** @hide */
        public static final int OTHER_OAT = 11;
        /** @hide */
        public static final int OTHER_ART = 12;
        /** @hide */
        public static final int OTHER_UNKNOWN_MAP = 13;
        /** @hide */
        public static final int OTHER_GRAPHICS = 14;
        /** @hide */
        public static final int OTHER_GL = 15;
        /** @hide */
        public static final int OTHER_OTHER_MEMTRACK = 16;

        // Needs to be declared here for the DVK_STAT ranges below.
        /** @hide */
        @UnsupportedAppUsage
        public static final int NUM_OTHER_STATS = 17;

        // Dalvik subsections.
        /** @hide */
        public static final int OTHER_DALVIK_NORMAL = 17;
        /** @hide */
        public static final int OTHER_DALVIK_LARGE = 18;
        /** @hide */
        public static final int OTHER_DALVIK_ZYGOTE = 19;
        /** @hide */
        public static final int OTHER_DALVIK_NON_MOVING = 20;
        // Section begins and ends for dumpsys, relative to the DALVIK categories.
        /** @hide */
        public static final int OTHER_DVK_STAT_DALVIK_START =
                OTHER_DALVIK_NORMAL - NUM_OTHER_STATS;
        /** @hide */
        public static final int OTHER_DVK_STAT_DALVIK_END =
                OTHER_DALVIK_NON_MOVING - NUM_OTHER_STATS;

        // Dalvik Other subsections.
        /** @hide */
        public static final int OTHER_DALVIK_OTHER_LINEARALLOC = 21;
        /** @hide */
        public static final int OTHER_DALVIK_OTHER_ACCOUNTING = 22;
        /** @hide */
        public static final int OTHER_DALVIK_OTHER_ZYGOTE_CODE_CACHE = 23;
        /** @hide */
        public static final int OTHER_DALVIK_OTHER_APP_CODE_CACHE = 24;
        /** @hide */
        public static final int OTHER_DALVIK_OTHER_COMPILER_METADATA = 25;
        /** @hide */
        public static final int OTHER_DALVIK_OTHER_INDIRECT_REFERENCE_TABLE = 26;
        /** @hide */
        public static final int OTHER_DVK_STAT_DALVIK_OTHER_START =
                OTHER_DALVIK_OTHER_LINEARALLOC - NUM_OTHER_STATS;
        /** @hide */
        public static final int OTHER_DVK_STAT_DALVIK_OTHER_END =
                OTHER_DALVIK_OTHER_INDIRECT_REFERENCE_TABLE - NUM_OTHER_STATS;

        // Dex subsections (Boot vdex, App dex, and App vdex).
        /** @hide */
        public static final int OTHER_DEX_BOOT_VDEX = 27;
        /** @hide */
        public static final int OTHER_DEX_APP_DEX = 28;
        /** @hide */
        public static final int OTHER_DEX_APP_VDEX = 29;
        /** @hide */
        public static final int OTHER_DVK_STAT_DEX_START = OTHER_DEX_BOOT_VDEX - NUM_OTHER_STATS;
        /** @hide */
        public static final int OTHER_DVK_STAT_DEX_END = OTHER_DEX_APP_VDEX - NUM_OTHER_STATS;

        // Art subsections (App image, boot image).
        /** @hide */
        public static final int OTHER_ART_APP = 30;
        /** @hide */
        public static final int OTHER_ART_BOOT = 31;
        /** @hide */
        public static final int OTHER_DVK_STAT_ART_START = OTHER_ART_APP - NUM_OTHER_STATS;
        /** @hide */
        public static final int OTHER_DVK_STAT_ART_END = OTHER_ART_BOOT - NUM_OTHER_STATS;

        /** @hide */
        @UnsupportedAppUsage
        public static final int NUM_DVK_STATS = OTHER_ART_BOOT + 1 - OTHER_DALVIK_NORMAL;

        /** @hide */
        public static final int NUM_CATEGORIES = 9;

        /** @hide */
        public static final int OFFSET_PSS = 0;
        /** @hide */
        public static final int OFFSET_SWAPPABLE_PSS = 1;
        /** @hide */
        public static final int OFFSET_RSS = 2;
        /** @hide */
        public static final int OFFSET_PRIVATE_DIRTY = 3;
        /** @hide */
        public static final int OFFSET_SHARED_DIRTY = 4;
        /** @hide */
        public static final int OFFSET_PRIVATE_CLEAN = 5;
        /** @hide */
        public static final int OFFSET_SHARED_CLEAN = 6;
        /** @hide */
        public static final int OFFSET_SWAPPED_OUT = 7;
        /** @hide */
        public static final int OFFSET_SWAPPED_OUT_PSS = 8;

        @UnsupportedAppUsage
        private int[] otherStats = new int[(NUM_OTHER_STATS+NUM_DVK_STATS)*NUM_CATEGORIES];

        public MemoryInfo() {
        }

        /**
         * @hide Copy contents from another object.
         */
        public void set(MemoryInfo other) {
            dalvikPss = other.dalvikPss;
            dalvikSwappablePss = other.dalvikSwappablePss;
            dalvikRss = other.dalvikRss;
            dalvikPrivateDirty = other.dalvikPrivateDirty;
            dalvikSharedDirty = other.dalvikSharedDirty;
            dalvikPrivateClean = other.dalvikPrivateClean;
            dalvikSharedClean = other.dalvikSharedClean;
            dalvikSwappedOut = other.dalvikSwappedOut;
            dalvikSwappedOutPss = other.dalvikSwappedOutPss;

            nativePss = other.nativePss;
            nativeSwappablePss = other.nativeSwappablePss;
            nativeRss = other.nativeRss;
            nativePrivateDirty = other.nativePrivateDirty;
            nativeSharedDirty = other.nativeSharedDirty;
            nativePrivateClean = other.nativePrivateClean;
            nativeSharedClean = other.nativeSharedClean;
            nativeSwappedOut = other.nativeSwappedOut;
            nativeSwappedOutPss = other.nativeSwappedOutPss;

            otherPss = other.otherPss;
            otherSwappablePss = other.otherSwappablePss;
            otherRss = other.otherRss;
            otherPrivateDirty = other.otherPrivateDirty;
            otherSharedDirty = other.otherSharedDirty;
            otherPrivateClean = other.otherPrivateClean;
            otherSharedClean = other.otherSharedClean;
            otherSwappedOut = other.otherSwappedOut;
            otherSwappedOutPss = other.otherSwappedOutPss;

            hasSwappedOutPss = other.hasSwappedOutPss;

            System.arraycopy(other.otherStats, 0, otherStats, 0, otherStats.length);
        }

        /**
         * Return total PSS memory usage in kB.
         */
        public int getTotalPss() {
            return dalvikPss + nativePss + otherPss + getTotalSwappedOutPss();
        }

        /**
         * @hide Return total PSS memory usage in kB.
         */
        @UnsupportedAppUsage
        public int getTotalUss() {
            return dalvikPrivateClean + dalvikPrivateDirty
                    + nativePrivateClean + nativePrivateDirty
                    + otherPrivateClean + otherPrivateDirty;
        }

        /**
         * Return total PSS memory usage in kB mapping a file of one of the following extension:
         * .so, .jar, .apk, .ttf, .dex, .odex, .oat, .art .
         */
        public int getTotalSwappablePss() {
            return dalvikSwappablePss + nativeSwappablePss + otherSwappablePss;
        }

        /**
         * @hide Return total RSS memory usage in kB.
         */
        public int getTotalRss() {
            return dalvikRss + nativeRss + otherRss;
        }

        /**
         * Return total private dirty memory usage in kB.
         */
        public int getTotalPrivateDirty() {
            return dalvikPrivateDirty + nativePrivateDirty + otherPrivateDirty;
        }

        /**
         * Return total shared dirty memory usage in kB.
         */
        public int getTotalSharedDirty() {
            return dalvikSharedDirty + nativeSharedDirty + otherSharedDirty;
        }

        /**
         * Return total shared clean memory usage in kB.
         */
        public int getTotalPrivateClean() {
            return dalvikPrivateClean + nativePrivateClean + otherPrivateClean;
        }

        /**
         * Return total shared clean memory usage in kB.
         */
        public int getTotalSharedClean() {
            return dalvikSharedClean + nativeSharedClean + otherSharedClean;
        }

        /**
         * Return total swapped out memory in kB.
         * @hide
         */
        public int getTotalSwappedOut() {
            return dalvikSwappedOut + nativeSwappedOut + otherSwappedOut;
        }

        /**
         * Return total swapped out memory in kB, proportional.
         * @hide
         */
        public int getTotalSwappedOutPss() {
            return dalvikSwappedOutPss + nativeSwappedOutPss + otherSwappedOutPss;
        }

        /** @hide */
        @UnsupportedAppUsage
        public int getOtherPss(int which) {
            return otherStats[which * NUM_CATEGORIES + OFFSET_PSS];
        }

        /** @hide */
        public int getOtherSwappablePss(int which) {
            return otherStats[which * NUM_CATEGORIES + OFFSET_SWAPPABLE_PSS];
        }

        /** @hide */
        public int getOtherRss(int which) {
            return otherStats[which * NUM_CATEGORIES + OFFSET_RSS];
        }

        /** @hide */
        @UnsupportedAppUsage
        public int getOtherPrivateDirty(int which) {
            return otherStats[which * NUM_CATEGORIES + OFFSET_PRIVATE_DIRTY];
        }

        /** @hide */
        @UnsupportedAppUsage
        public int getOtherSharedDirty(int which) {
            return otherStats[which * NUM_CATEGORIES + OFFSET_SHARED_DIRTY];
        }

        /** @hide */
        public int getOtherPrivateClean(int which) {
            return otherStats[which * NUM_CATEGORIES + OFFSET_PRIVATE_CLEAN];
        }

        /** @hide */
        @UnsupportedAppUsage
        public int getOtherPrivate(int which) {
          return getOtherPrivateClean(which) + getOtherPrivateDirty(which);
        }

        /** @hide */
        public int getOtherSharedClean(int which) {
            return otherStats[which * NUM_CATEGORIES + OFFSET_SHARED_CLEAN];
        }

        /** @hide */
        public int getOtherSwappedOut(int which) {
            return otherStats[which * NUM_CATEGORIES + OFFSET_SWAPPED_OUT];
        }

        /** @hide */
        public int getOtherSwappedOutPss(int which) {
            return otherStats[which * NUM_CATEGORIES + OFFSET_SWAPPED_OUT_PSS];
        }

        /** @hide */
        @UnsupportedAppUsage
        public static String getOtherLabel(int which) {
            switch (which) {
                case OTHER_DALVIK_OTHER: return "Dalvik Other";
                case OTHER_STACK: return "Stack";
                case OTHER_CURSOR: return "Cursor";
                case OTHER_ASHMEM: return "Ashmem";
                case OTHER_GL_DEV: return "Gfx dev";
                case OTHER_UNKNOWN_DEV: return "Other dev";
                case OTHER_SO: return ".so mmap";
                case OTHER_JAR: return ".jar mmap";
                case OTHER_APK: return ".apk mmap";
                case OTHER_TTF: return ".ttf mmap";
                case OTHER_DEX: return ".dex mmap";
                case OTHER_OAT: return ".oat mmap";
                case OTHER_ART: return ".art mmap";
                case OTHER_UNKNOWN_MAP: return "Other mmap";
                case OTHER_GRAPHICS: return "EGL mtrack";
                case OTHER_GL: return "GL mtrack";
                case OTHER_OTHER_MEMTRACK: return "Other mtrack";
                case OTHER_DALVIK_NORMAL: return ".Heap";
                case OTHER_DALVIK_LARGE: return ".LOS";
                case OTHER_DALVIK_ZYGOTE: return ".Zygote";
                case OTHER_DALVIK_NON_MOVING: return ".NonMoving";
                case OTHER_DALVIK_OTHER_LINEARALLOC: return ".LinearAlloc";
                case OTHER_DALVIK_OTHER_ACCOUNTING: return ".GC";
                case OTHER_DALVIK_OTHER_ZYGOTE_CODE_CACHE: return ".ZygoteJIT";
                case OTHER_DALVIK_OTHER_APP_CODE_CACHE: return ".AppJIT";
                case OTHER_DALVIK_OTHER_COMPILER_METADATA: return ".CompilerMetadata";
                case OTHER_DALVIK_OTHER_INDIRECT_REFERENCE_TABLE: return ".IndirectRef";
                case OTHER_DEX_BOOT_VDEX: return ".Boot vdex";
                case OTHER_DEX_APP_DEX: return ".App dex";
                case OTHER_DEX_APP_VDEX: return ".App vdex";
                case OTHER_ART_APP: return ".App art";
                case OTHER_ART_BOOT: return ".Boot art";
                default: return "????";
            }
        }

      /**
       * Returns the value of a particular memory statistic or {@code null} if no
       * such memory statistic exists.
       *
       * <p>The following table lists the memory statistics that are supported.
       * Note that memory statistics may be added or removed in a future API level.</p>
       *
       * <table>
       *     <thead>
       *         <tr>
       *             <th>Memory statistic name</th>
       *             <th>Meaning</th>
       *             <th>Example</th>
       *             <th>Supported (API Levels)</th>
       *         </tr>
       *     </thead>
       *     <tbody>
       *         <tr>
       *             <td>summary.java-heap</td>
       *             <td>The private Java Heap usage in kB. This corresponds to the Java Heap field
       *                 in the App Summary section output by dumpsys meminfo.</td>
       *             <td>{@code 1442}</td>
       *             <td>23</td>
       *         </tr>
       *         <tr>
       *             <td>summary.native-heap</td>
       *             <td>The private Native Heap usage in kB. This corresponds to the Native Heap
       *                 field in the App Summary section output by dumpsys meminfo.</td>
       *             <td>{@code 1442}</td>
       *             <td>23</td>
       *         </tr>
       *         <tr>
       *             <td>summary.code</td>
       *             <td>The memory usage for static code and resources in kB. This corresponds to
       *                 the Code field in the App Summary section output by dumpsys meminfo.</td>
       *             <td>{@code 1442}</td>
       *             <td>23</td>
       *         </tr>
       *         <tr>
       *             <td>summary.stack</td>
       *             <td>The stack usage in kB. This corresponds to the Stack field in the
       *                 App Summary section output by dumpsys meminfo.</td>
       *             <td>{@code 1442}</td>
       *             <td>23</td>
       *         </tr>
       *         <tr>
       *             <td>summary.graphics</td>
       *             <td>The graphics usage in kB. This corresponds to the Graphics field in the
       *                 App Summary section output by dumpsys meminfo.</td>
       *             <td>{@code 1442}</td>
       *             <td>23</td>
       *         </tr>
       *         <tr>
       *             <td>summary.private-other</td>
       *             <td>Other private memory usage in kB. This corresponds to the Private Other
       *                 field output in the App Summary section by dumpsys meminfo.</td>
       *             <td>{@code 1442}</td>
       *             <td>23</td>
       *         </tr>
       *         <tr>
       *             <td>summary.system</td>
       *             <td>Shared and system memory usage in kB. This corresponds to the System
       *                 field output in the App Summary section by dumpsys meminfo.</td>
       *             <td>{@code 1442}</td>
       *             <td>23</td>
       *         </tr>
       *         <tr>
       *             <td>summary.total-pss</td>
       *             <td>Total PSS memory usage in kB.</td>
       *             <td>{@code 1442}</td>
       *             <td>23</td>
       *         </tr>
       *         <tr>
       *             <td>summary.total-swap</td>
       *             <td>Total swap usage in kB.</td>
       *             <td>{@code 1442}</td>
       *             <td>23</td>
       *         </tr>
       *     </tbody>
       * </table>
       */
       public String getMemoryStat(String statName) {
            switch(statName) {
                case "summary.java-heap":
                    return Integer.toString(getSummaryJavaHeap());
                case "summary.native-heap":
                    return Integer.toString(getSummaryNativeHeap());
                case "summary.code":
                    return Integer.toString(getSummaryCode());
                case "summary.stack":
                    return Integer.toString(getSummaryStack());
                case "summary.graphics":
                    return Integer.toString(getSummaryGraphics());
                case "summary.private-other":
                    return Integer.toString(getSummaryPrivateOther());
                case "summary.system":
                    return Integer.toString(getSummarySystem());
                case "summary.total-pss":
                    return Integer.toString(getSummaryTotalPss());
                case "summary.total-swap":
                    return Integer.toString(getSummaryTotalSwap());
                default:
                    return null;
            }
        }

        /**
         * Returns a map of the names/values of the memory statistics
         * that {@link #getMemoryStat(String)} supports.
         *
         * @return a map of the names/values of the supported memory statistics.
         */
        public Map<String, String> getMemoryStats() {
            Map<String, String> stats = new HashMap<String, String>();
            stats.put("summary.java-heap", Integer.toString(getSummaryJavaHeap()));
            stats.put("summary.native-heap", Integer.toString(getSummaryNativeHeap()));
            stats.put("summary.code", Integer.toString(getSummaryCode()));
            stats.put("summary.stack", Integer.toString(getSummaryStack()));
            stats.put("summary.graphics", Integer.toString(getSummaryGraphics()));
            stats.put("summary.private-other", Integer.toString(getSummaryPrivateOther()));
            stats.put("summary.system", Integer.toString(getSummarySystem()));
            stats.put("summary.total-pss", Integer.toString(getSummaryTotalPss()));
            stats.put("summary.total-swap", Integer.toString(getSummaryTotalSwap()));
            return stats;
        }

        /**
         * Pss of Java Heap bytes in KB due to the application.
         * Notes:
         *  * OTHER_ART is the boot image. Anything private here is blamed on
         *    the application, not the system.
         *  * dalvikPrivateDirty includes private zygote, which means the
         *    application dirtied something allocated by the zygote. We blame
         *    the application for that memory, not the system.
         *  * Does not include OTHER_DALVIK_OTHER, which is considered VM
         *    Overhead and lumped into Private Other.
         *  * We don't include dalvikPrivateClean, because there should be no
         *    such thing as private clean for the Java Heap.
         * @hide
         */
        @UnsupportedAppUsage
        public int getSummaryJavaHeap() {
            return dalvikPrivateDirty + getOtherPrivate(OTHER_ART);
        }

        /**
         * Pss of Native Heap bytes in KB due to the application.
         * Notes:
         *  * Includes private dirty malloc space.
         *  * We don't include nativePrivateClean, because there should be no
         *    such thing as private clean for the Native Heap.
         * @hide
         */
        @UnsupportedAppUsage
        public int getSummaryNativeHeap() {
            return nativePrivateDirty;
        }

        /**
         * Pss of code and other static resource bytes in KB due to
         * the application.
         * @hide
         */
        @UnsupportedAppUsage
        public int getSummaryCode() {
            return getOtherPrivate(OTHER_SO)
              + getOtherPrivate(OTHER_JAR)
              + getOtherPrivate(OTHER_APK)
              + getOtherPrivate(OTHER_TTF)
              + getOtherPrivate(OTHER_DEX)
                + getOtherPrivate(OTHER_OAT)
                + getOtherPrivate(OTHER_DALVIK_OTHER_ZYGOTE_CODE_CACHE)
                + getOtherPrivate(OTHER_DALVIK_OTHER_APP_CODE_CACHE);
        }

        /**
         * Pss in KB of the stack due to the application.
         * Notes:
         *  * Includes private dirty stack, which includes both Java and Native
         *    stack.
         *  * Does not include private clean stack, because there should be no
         *    such thing as private clean for the stack.
         * @hide
         */
        @UnsupportedAppUsage
        public int getSummaryStack() {
            return getOtherPrivateDirty(OTHER_STACK);
        }

        /**
         * Pss in KB of graphics due to the application.
         * Notes:
         *  * Includes private Gfx, EGL, and GL.
         *  * Warning: These numbers can be misreported by the graphics drivers.
         *  * We don't include shared graphics. It may make sense to, because
         *    shared graphics are likely buffers due to the application
         *    anyway, but it's simpler to implement to just group all shared
         *    memory into the System category.
         * @hide
         */
        @UnsupportedAppUsage
        public int getSummaryGraphics() {
            return getOtherPrivate(OTHER_GL_DEV)
              + getOtherPrivate(OTHER_GRAPHICS)
              + getOtherPrivate(OTHER_GL);
        }

        /**
         * Pss in KB due to the application that haven't otherwise been
         * accounted for.
         * @hide
         */
        @UnsupportedAppUsage
        public int getSummaryPrivateOther() {
            return getTotalPrivateClean()
              + getTotalPrivateDirty()
              - getSummaryJavaHeap()
              - getSummaryNativeHeap()
              - getSummaryCode()
              - getSummaryStack()
              - getSummaryGraphics();
        }

        /**
         * Pss in KB due to the system.
         * Notes:
         *  * Includes all shared memory.
         * @hide
         */
        @UnsupportedAppUsage
        public int getSummarySystem() {
            return getTotalPss()
              - getTotalPrivateClean()
              - getTotalPrivateDirty();
        }

        /**
         * Rss of Java Heap bytes in KB due to the application.
         * @hide
         */
        public int getSummaryJavaHeapRss() {
            return dalvikRss + getOtherRss(OTHER_ART);
        }

        /**
         * Rss of Native Heap bytes in KB due to the application.
         * @hide
         */
        public int getSummaryNativeHeapRss() {
            return nativeRss;
        }

        /**
         * Rss of code and other static resource bytes in KB due to
         * the application.
         * @hide
         */
        public int getSummaryCodeRss() {
            return getOtherRss(OTHER_SO)
                + getOtherRss(OTHER_JAR)
                + getOtherRss(OTHER_APK)
                + getOtherRss(OTHER_TTF)
                + getOtherRss(OTHER_DEX)
                + getOtherRss(OTHER_OAT)
                + getOtherRss(OTHER_DALVIK_OTHER_ZYGOTE_CODE_CACHE)
                + getOtherRss(OTHER_DALVIK_OTHER_APP_CODE_CACHE);
        }

        /**
         * Rss in KB of the stack due to the application.
         * @hide
         */
        public int getSummaryStackRss() {
            return getOtherRss(OTHER_STACK);
        }

        /**
         * Rss in KB of graphics due to the application.
         * @hide
         */
        public int getSummaryGraphicsRss() {
            return getOtherRss(OTHER_GL_DEV)
                + getOtherRss(OTHER_GRAPHICS)
                + getOtherRss(OTHER_GL);
        }

        /**
         * Rss in KB due to either the application or system that haven't otherwise been
         * accounted for.
         * @hide
         */
        public int getSummaryUnknownRss() {
            return getTotalRss()
                - getSummaryJavaHeapRss()
                - getSummaryNativeHeapRss()
                - getSummaryCodeRss()
                - getSummaryStackRss()
                - getSummaryGraphicsRss();
        }

        /**
         * Total Pss in KB.
         * @hide
         */
        public int getSummaryTotalPss() {
            return getTotalPss();
        }

        /**
         * Total Swap in KB.
         * Notes:
         *  * Some of this memory belongs in other categories, but we don't
         *    know if the Swap memory is shared or private, so we don't know
         *    what to blame on the application and what on the system.
         *    For now, just lump all the Swap in one place.
         *    For kernels reporting SwapPss {@link #getSummaryTotalSwapPss()}
         *    will report the application proportional Swap.
         * @hide
         */
        public int getSummaryTotalSwap() {
            return getTotalSwappedOut();
        }

        /**
         * Total proportional Swap in KB.
         * Notes:
         *  * Always 0 if {@link #hasSwappedOutPss} is false.
         * @hide
         */
        public int getSummaryTotalSwapPss() {
            return getTotalSwappedOutPss();
        }

        /**
         * Return true if the kernel is reporting pss swapped out...  that is, if
         * {@link #getSummaryTotalSwapPss()} will return non-0 values.
         * @hide
         */
        public boolean hasSwappedOutPss() {
            return hasSwappedOutPss;
        }

        public int describeContents() {
            return 0;
        }

        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(dalvikPss);
            dest.writeInt(dalvikSwappablePss);
            dest.writeInt(dalvikRss);
            dest.writeInt(dalvikPrivateDirty);
            dest.writeInt(dalvikSharedDirty);
            dest.writeInt(dalvikPrivateClean);
            dest.writeInt(dalvikSharedClean);
            dest.writeInt(dalvikSwappedOut);
            dest.writeInt(dalvikSwappedOutPss);
            dest.writeInt(nativePss);
            dest.writeInt(nativeSwappablePss);
            dest.writeInt(nativeRss);
            dest.writeInt(nativePrivateDirty);
            dest.writeInt(nativeSharedDirty);
            dest.writeInt(nativePrivateClean);
            dest.writeInt(nativeSharedClean);
            dest.writeInt(nativeSwappedOut);
            dest.writeInt(nativeSwappedOutPss);
            dest.writeInt(otherPss);
            dest.writeInt(otherSwappablePss);
            dest.writeInt(otherRss);
            dest.writeInt(otherPrivateDirty);
            dest.writeInt(otherSharedDirty);
            dest.writeInt(otherPrivateClean);
            dest.writeInt(otherSharedClean);
            dest.writeInt(otherSwappedOut);
            dest.writeInt(hasSwappedOutPss ? 1 : 0);
            dest.writeInt(otherSwappedOutPss);
            dest.writeIntArray(otherStats);
        }

        public void readFromParcel(Parcel source) {
            dalvikPss = source.readInt();
            dalvikSwappablePss = source.readInt();
            dalvikRss = source.readInt();
            dalvikPrivateDirty = source.readInt();
            dalvikSharedDirty = source.readInt();
            dalvikPrivateClean = source.readInt();
            dalvikSharedClean = source.readInt();
            dalvikSwappedOut = source.readInt();
            dalvikSwappedOutPss = source.readInt();
            nativePss = source.readInt();
            nativeSwappablePss = source.readInt();
            nativeRss = source.readInt();
            nativePrivateDirty = source.readInt();
            nativeSharedDirty = source.readInt();
            nativePrivateClean = source.readInt();
            nativeSharedClean = source.readInt();
            nativeSwappedOut = source.readInt();
            nativeSwappedOutPss = source.readInt();
            otherPss = source.readInt();
            otherSwappablePss = source.readInt();
            otherRss = source.readInt();
            otherPrivateDirty = source.readInt();
            otherSharedDirty = source.readInt();
            otherPrivateClean = source.readInt();
            otherSharedClean = source.readInt();
            otherSwappedOut = source.readInt();
            hasSwappedOutPss = source.readInt() != 0;
            otherSwappedOutPss = source.readInt();
            otherStats = source.createIntArray();
        }

        public static final @android.annotation.NonNull Creator<MemoryInfo> CREATOR = new Creator<MemoryInfo>() {
            public MemoryInfo createFromParcel(Parcel source) {
                return new MemoryInfo(source);
            }
            public MemoryInfo[] newArray(int size) {
                return new MemoryInfo[size];
            }
        };

        private MemoryInfo(Parcel source) {
            readFromParcel(source);
        }
    }
    
##   能超过限定内存吗？


![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e9d58c1aa2fb4c8ab4a20555ea28e0d4~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=1596&h=888&s=245069&e=png&b=2c2e31)

看图，是可以短时间超过，然后回调回来就没问题：如下，可以看到有频繁的GC发生，GC后满足虚拟机的需求即可。


![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/2e1fe1913af64344ad1ddc8fc254c220~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=1576&h=1024&s=170927&e=png&b=2c2e31)

 
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/3a05cc1f1d0d48988d7a1bb4d8362c97~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=788&h=700&s=57480&e=png&a=1&b=d5cdcc)

甚至略微超点都没事，不过多了就oom了，


![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/4ec3d7a7d663478095658e68133cb2e5~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=1508&h=696&s=113569&e=png&b=2a849f)

栈内存不算在内，主要是Java堆内存 ，进程整体的内存占用其实是可以远超过JVM虚拟机设定的内存的。

	dalvikPrivateDirty + getOtherPrivate(OTHER_ART)

 
##   总结

* OOM对应的是Dalvik虚拟机，或者JVM限定的内存超过限制xxm设定那种，物理内存并一定不够用
* Java层New对象的时候，内存溢出是哪部分超过了512呢？
    
## 	  参考文档

[OOM问题定位](https://tech.meituan.com/2019/11/14/crash-oom-probe-practice.html)
Android 启动线程OOM[https://blog.csdn.net/LiC_07093128/article/details/79451851](https://blog.csdn.net/LiC_07093128/article/details/79451851)

[参考文档 内存空间 https://www.cnblogs.com/binlovetech/p/16824522.html](https://www.cnblogs.com/binlovetech/p/16824522.html)

[参考文档  OOM类型 https://www.wuyifei.cc/android-oom/](https://www.wuyifei.cc/android-oom/)

[被问懵了：一个进程最多可以创建多少个线程？https://www.cnblogs.com/xiaolincoding/p/15013929.html](https://www.cnblogs.com/xiaolincoding/p/15013929.html)

 [Probe：Android线上OOM问题定位组件https://tech.meituan.com/2019/11/14/crash-oom-probe-practice.html](https://tech.meituan.com/2019/11/14/crash-oom-probe-practice.html)
 
[ Android内存申请分析 https://mp.weixin.qq.com/s?__biz=MzAwNDY1ODY2OQ==&mid=2649286327&idx=1&sn=b69513e3dfd1de848daefe03ab6719c2&scene=26#wechat_redirect}]()