
	
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




### OOM的异常如何抛出的？哪里抛出的？

*  创建线程失败
*  创建对象或者分配内存失败


最终还是JVM虚拟机在native层抛出的，只不过是要上层进行封装一下

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

如果应用在native crash一般会直接崩掉，不会抛出OOM


## 堆内存分配失败

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
	  size_t total_bytes_free = GetFreeMemory();
	  oss << "Failed to allocate a " << byte_count << " byte allocation with " << total_bytes_free
	      << " free bytes and " << PrettySize(GetFreeMemoryUntilOOME()) << " until OOM,"
	      << " target footprint " << target_footprint_.load(std::memory_order_relaxed)
	      << ", growth limit "
	      << growth_limit_;
	  // If the allocation failed due to fragmentation, print out the largest continuous allocation.
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
	      // LogFragmentationAllocFailure returns true if byte_count is greater than
	      // the largest free contiguous chunk in the space. Return value false
	      // means that we are throwing OOME because the amount of free heap after
	      // GC is less than kMinFreeHeapAfterGcForAlloc in proportion of the heap-size.
	      // Log an appropriate message in that case.
	      if (!space->LogFragmentationAllocFailure(oss, byte_count)) {
	        oss << "; giving up on allocation because <"
	            << kMinFreeHeapAfterGcForAlloc * 100
	            << "% of heap free after GC.";
	      }
	    }
	  }
	  self->ThrowOutOfMemoryError(oss.str().c_str());
	}

为对象分配内存时达到进程的内存上限。由Runtime.getRuntime.MaxMemory()可以得到Android中每个进程被系统分配的内存上限，当进程占用内存达到这个上限时就会发生OOM，这也是Android中最常见的OOM类型。

没有足够大小的连续地址空间。这种情况一般是进程中存在大量的内存碎片导致的，其堆栈信息会比第一种OOM堆栈多出一段信息：failed due to fragmentation (required continguous free “<< required_bytes << “ bytes for a new buffer where largest contiguous free ” << largest_continuous_free_pages << “ bytes)”; 其详细代码在art/runtime/gc/allocator/rosalloc.cc中，这里不作详述。




#### 创建线程失败 ：创建线程过程中发生OOM是因为进程内的**虚拟内存地址空间耗尽了**

**虚拟内存不足，并不一定是实际内存不足，仅仅是虚拟内存不足**

看一个线程创建过多而失败的例子，打印的日志

	Throwing OutOfMemoryError "pthread_create (1040KB stack) failed: Try again" (VmSize 145637572 kB)

	 java.lang.OutOfMemoryError: pthread_create (1040KB stack) failed: Try again
	                                                                                                    	 at java.lang.Thread.nativeCreate(Native Method)
	                                                                                                    	 at java.lang.Thread.start(Thread.java:976)
                                                                                                                                                                                                   

对应的代码

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

而后会调用art虚拟机中的Thread.cpp实现

		void Thread::CreateNativeThread(JNIEnv* env, jobject java_peer, size_t stack_size, bool is_daemon) {
		  CHECK(java_peer != nullptr);
		  Thread* self = static_cast<JNIEnvExt*>(env)->GetSelf();
		
		  if (VLOG_IS_ON(threads)) {
		    ScopedObjectAccess soa(env);
		
		    ArtField* f = WellKnownClasses::java_lang_Thread_name;
		    ObjPtr<mirror::String> java_name =
		        f->GetObject(soa.Decode<mirror::Object>(java_peer))->AsString();
		    std::string thread_name;
		    if (java_name != nullptr) {
		      thread_name = java_name->ToModifiedUtf8();
		    } else {
		      thread_name = "(Unnamed)";
		    }
		
		    VLOG(threads) << "Creating native thread for " << thread_name;
		    self->Dump(LOG_STREAM(INFO));
		  }
		
		  Runtime* runtime = Runtime::Current();
		
		  // Atomically start the birth of the thread ensuring the runtime isn't shutting down.
		  bool thread_start_during_shutdown = false;
		  {
		    MutexLock mu(self, *Locks::runtime_shutdown_lock_);
		    if (runtime->IsShuttingDownLocked()) {
		      thread_start_during_shutdown = true;
		    } else {
		      runtime->StartThreadBirth();
		    }
		  }
		  if (thread_start_during_shutdown) {
		    ScopedLocalRef<jclass> error_class(env, env->FindClass("java/lang/InternalError"));
		    env->ThrowNew(error_class.get(), "Thread starting during runtime shutdown");
		    return;
		  }
		
		  Thread* child_thread = new Thread(is_daemon);
		  // Use global JNI ref to hold peer live while child thread starts.
		  child_thread->tlsPtr_.jpeer = env->NewGlobalRef(java_peer);
		  stack_size = FixStackSize(stack_size);
		
		  // Thread.start is synchronized, so we know that nativePeer is 0, and know that we're not racing
		  // to assign it.
		  SetNativePeer(env, java_peer, child_thread);
		
		<!--先分配JNIEnvExt 看看内存是否足够，看看是否已经OOM -->
		  // Try to allocate a JNIEnvExt for the thread. We do this here as we might be out of memory and
		  // do not have a good way to report this on the child's side.
		  std::string error_msg;
		  std::unique_ptr<JNIEnvExt> child_jni_env_ext(
		      JNIEnvExt::Create(child_thread, Runtime::Current()->GetJavaVM(), &error_msg));
		
		  int pthread_create_result = 0;
		  <!--JNIEnvExt如果分配成功，一定程度说明内存还是有的，再分配线程-->
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
		
		  // Either JNIEnvExt::Create or pthread_create(3) failed, so clean up.
		  {
		    MutexLock mu(self, *Locks::runtime_shutdown_lock_);
		    runtime->EndThreadBirth();
		  }
		  // Manually delete the global reference since Thread::Init will not have been run. Make sure
		  // nothing can observe both opeer and jpeer set at the same time.
		  child_thread->DeleteJPeer(env);
		  delete child_thread;
		  child_thread = nullptr;
		  // TODO: remove from thread group?
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

 在结尾的地方会看见如何抛出不同哦的异常，其实就两种，第一种JNIENV对象分配失败，直接说是内存OOM，
                                                           
         Could not allocate JNI Env: %s", error_msg.c_str()                                         
 
JNIEnvExt 创建失败的原因也有两个，JNIEnvExt::Create        

*          fd超限
*          虚拟内存不足
         	
         	路径:art/runtime/mem_map.cc：

		// 1. 创建 
		
		ashmemfd.reset(ashmem_create_region(debug_friendly_name.c_str(), page_aligned_byte_count));

		E/art: ashmem_create_region failed for 'indirect ref table': Too many open files
		 java.lang.OutOfMemoryError: Could not allocate JNI Env
		   at java.lang.Thread.nativeCreate(Native Method)
		   at java.lang.Thread.start(Thread.java:730)
     		
		// 2. 调用mmap映射到用户态内存地址空间
		
		void* actual = MapInternal(..., fd.get(), ...);	
		E/art: Failed anonymous mmap(0x0, 8192, 0x3, 0x2, 116, 0): Operation not permitted. See process maps in the log.
		java.lang.OutOfMemoryError: Could not allocate JNI Env
		  at java.lang.Thread.nativeCreate(Native Method)
		  at java.lang.Thread.start(Thread.java:1063)


步骤1失败的话,fd.get()返回-1,步骤2仍然会正常执行,只不过其行为有所不同。

如果步骤1成功的话,两个步骤则是：

1.通过Andorid的匿名共享内存(Anonymous Shared Memory)分配 4KB(一个page)内核态内存

2.再通过 Linux 的 mmap 调用映射到用户态虚拟内存地址空间.

如果步骤1失败的话，步骤2则是：

    通过 Linux 的 mmap 调用创建一段虚拟内存.分配虚拟内存失败了

考察失败的场景：

    步骤1 失败的情况一般是内核分配内存失败，这种情况下，整个设备/OS的内存应该都处于非常紧张的状态。

    步骤2 失败的情况一般是 进程虚拟内存地址空间耗尽
 
第二种就是之前举例的那种
 		     
         StringPrintf("pthread_create (%s stack) failed: %s", PrettySize(stack_size).c_str(), strerror(pthread_create_result)));
         
 *          虚拟内存不足
 
		 W/libc: pthread_create failed: couldn't allocate 1073152-bytes mapped space: Out of memory
		W/tch.crowdsourc: Throwing OutOfMemoryError with VmSize  4191668 kB "pthread_create (1040KB stack) failed: Try again"
		java.lang.OutOfMemoryError: pthread_create (1040KB stack) failed: Try again
		        at java.lang.Thread.nativeCreate(Native Method)
		        at java.lang.Thread.start(Thread.java:753)
        
 
 *          线程数超限

 
		 W/libc: pthread_create failed: clone failed: Out of memory
		W/art: Throwing OutOfMemoryError "pthread_create (1040KB stack) failed: Out of memory"
		java.lang.OutOfMemoryError: pthread_create (1040KB stack) failed: Out of memory
		  at java.lang.Thread.nativeCreate(Native Method)
		  at java.lang.Thread.start(Thread.java:1078)



	__BIONIC_ERRDEF( EBADF          ,   9, "Bad file descriptor" )
	
	__BIONIC_ERRDEF( ECHILD         ,  10, "No child processes" )
	
	__BIONIC_ERRDEF( EAGAIN         ,  11, "Try again" )
	
	__BIONIC_ERRDEF( ENOMEM         ,  12, "Out of memory" )
	
	__BIONIC_ERRDEF( EMFILE         ,  24, "Too many open files" )

 
	    
> 创建JNI失败：创建JNIEnv可以归为两个步骤：

通过Andorid的匿名共享内存（Anonymous Shared Memory）分配 4KB（一个page）内核态内存。
再通过Linux的mmap调用映射到用户态虚拟内存地址空间，既然是共享内存，内核必有参与

第一步创建匿名共享内存时，需要打开/dev/ashmem文件，所以需要一个FD（文件描述符）。此时，如果创建的FD数已经达到上限，则会导致创建JNIEnv失败，抛出错误信息如下：

	E/art: ashmem_create_region failed for 'indirect ref table': Too many open files
	 java.lang.OutOfMemoryError: Could not allocate JNI Env
	   at java.lang.Thread.nativeCreate(Native Method)
	   at java.lang.Thread.start(Thread.java:730)

查看fd数量，系统的设置

	 cat /proc/sys/fs/file-max
	171817   

第二步调用mmap时，如果进程虚拟内存地址空间耗尽，也会导致创建JNIEnv失败，抛出错误信息如下：

	E/art: Failed anonymous mmap(0x0, 8192, 0x3, 0x2, 116, 0): Operation not permitted. See process maps in the log.
	java.lang.OutOfMemoryError: Could not allocate JNI Env
	  at java.lang.Thread.nativeCreate(Native Method)
	  at java.lang.Thread.start(Thread.java:1063)
	
MemMap MapAnonymous代码
	  
	MemMap MemMap::MapAnonymous(const char* name,
	                            uint8_t* addr,
	                            size_t byte_count,
	                            int prot,
	                            bool low_4gb,
	                            bool reuse,
	                            /*inout*/MemMap* reservation,
	                            /*out*/std::string* error_msg,
	                            bool use_debug_name) {
	#ifndef __LP64__
	  UNUSED(low_4gb);
	#endif
	  if (byte_count == 0) {
	    *error_msg = "Empty MemMap requested.";
	    return Invalid();
	  }
	  size_t page_aligned_byte_count = RoundUp(byte_count, GetPageSize());
	
	  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
	  if (reuse) {
	    // reuse means it is okay that it overlaps an existing page mapping.
	    // Only use this if you actually made the page reservation yourself.
	    CHECK(addr != nullptr);
	    DCHECK(reservation == nullptr);
	
	    DCHECK(ContainedWithinExistingMap(addr, byte_count, error_msg)) << *error_msg;
	    flags |= MAP_FIXED;
	  } else if (reservation != nullptr) {
	    CHECK(addr != nullptr);
	    if (!CheckReservation(addr, byte_count, name, *reservation, error_msg)) {
	      return MemMap::Invalid();
	    }
	    flags |= MAP_FIXED;
	  }
	
	  unique_fd fd;
	
	  // We need to store and potentially set an error number for pretty printing of errors
	  int saved_errno = 0;
	
	  void* actual = MapInternal(addr,
	                             page_aligned_byte_count,
	                             prot,
	                             flags,
	                             fd.get(),
	                             0,
	                             low_4gb);
	  saved_errno = errno;
	
	  if (actual == MAP_FAILED) {
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
	  if (!CheckMapRequest(addr, actual, page_aligned_byte_count, error_msg)) {
	    return Invalid();
	  }
	
	  if (use_debug_name) {
	    SetDebugName(actual, name, page_aligned_byte_count);
	  }
	
	  if (reservation != nullptr) {
	    // Re-mapping was successful, transfer the ownership of the memory to the new MemMap.
	    DCHECK_EQ(actual, reservation->Begin());
	    reservation->ReleaseReservedMemory(byte_count);
	  }
	  return MemMap(name,
	                reinterpret_cast<uint8_t*>(actual),
	                byte_count,
	                actual,
	                page_aligned_byte_count,
	                prot,
	                reuse);
	}	  
	  
	  
  
>   创建线程失败
  
创建线程也可以归纳为两个步骤：

* 1 调用mmap分配栈内存。这里mmap flag中指定了MAP_ANONYMOUS，即匿名内存映射。这是在Linux中分配大块内存的常用方式。其分配的是虚拟内存，对应页的物理内存并不会立即分配，而是在用到的时候触发内核的缺页中断，然后中断处理函数再分配物理内存。

* 2 调用clone方法进行线程创建。

第一步分配栈内存失败是由于进程的虚拟内存不足，抛出错误信息如下：

	W/libc: pthread_create failed: couldn't allocate 1073152-bytes mapped space: Out of memory
	W/tch.crowdsourc: Throwing OutOfMemoryError with VmSize  4191668 kB "pthread_create (1040KB stack) failed: Try again"
	java.lang.OutOfMemoryError: pthread_create (1040KB stack) failed: Try again
	        at java.lang.Thread.nativeCreate(Native Method)
	        at java.lang.Thread.start(Thread.java:753)
        
第二步clone方法失败是因为线程数超出了限制，抛出错误信息如下：

	W/libc: pthread_create failed: clone failed: Out of memory
	W/art: Throwing OutOfMemoryError "pthread_create (1040KB stack) failed: Out of memory"
	java.lang.OutOfMemoryError: pthread_create (1040KB stack) failed: Out of memory
	  at java.lang.Thread.nativeCreate(Native Method)
	  at java.lang.Thread.start(Thread.java:1078)
	  
	  
获取当前进程的虚拟内存大小
	
	cat /proc/pid/status  
	
 输出
 
	VmPeak:	32423148 kB  进程运行过程中虚拟内存的峰值
	VmSize:	32423148 kB  代表进程现在正在占用的虚拟内存
	VmLck:	       0 kB     进程已锁住的物理内存大小
	VmPin:	       0 kB
	VmHWM:	  140760 kB
	VmRSS:	  139392 kB    应用程序正在使用的物理内存的大小
	RssAnon:	   65452 kB
	RssFile:	   73164 kB
	RssShmem:	     776 kB
	VmData:	 6637624 kB  程序数据段的大小
	VmStk:	    8192 kB     进程在用户态的栈的大小
	VmExe:	       4 kB
	VmLib:	  141020 kB   虚拟内存库，动态链接库所使用的虚拟内存
	VmPTE:	    3340 kB   可执行的虚拟内存，可执行的和静态链接库所使用的虚拟内存
	VmSwap:	   21312 kB
	CoreDumping:	0
	THP_enabled:	1
	Threads:	730

 

### 分析

#### 堆OOM

堆内存分配失败，通常说明进程中大部分的内存已经被占用了，且不能被垃圾回收器回收，一般来说此时内存占用都存在一些问题，例如内存泄漏等。要想定位到问题所在，就需要知道进程中的内存都被哪些对象占用，以及这些对象的引用链路。而这些信息都可以在Java内存快照文件中得到，调用Debug.dumpHprofData(String fileName)函数就可以得到当前进程的Java内存快照文件（即HPROF文件）。所以，关键在于要获得进程的内存快照，由于dump函数比较耗时，在发生OOM之后再去执行dump操作，很可能无法得到完整的内存快照文件。



### 线程数超出限制

 
对于创建线程失败导致的OOM，Probe会获取当前进程所占用的虚拟内存、进程中的线程数量、每个线程的信息（线程名、所属线程组、堆栈信息）以及系统的线程数限制，并将这些信息上传用于分析问题。

/proc/sys/kernel/threads-max规定了每个进程创建线程数目的上限。在华为的部分机型上，这个上限被修改的很低（大约500），比较容易出现线程数溢出的问题，而大部分手机这个限制都很大（一般为1W多）。在这些手机上创建线程失败大多都是因为虚拟内存空间耗尽导致的，进程所使用的虚拟内存可以查看/proc/pid/status的VmPeak/VmSize记录。

然后通过Thread.getAllStackTraces()可以得到进程中的所有线程以及对应的堆栈信息。

一般来说，当进程中线程数异常增多时，都是某一类线程被大量的重复创建。所以我们只需要定位到这类线程的创建时机，就能知道问题所在。如果线程是有自定义名称的，那么直接就可以在代码中搜索到创建线程的位置，从而定位问题，如果线程创建时没有指定名称，那么就需要通过该线程的堆栈信息来辅助定位。下面这个例子，就是一个“crowdSource msg”的线程被大量重复创建，在代码中搜索名称很快就查出了问题。针对这类线程问题推荐的做法就是在项目中统一使用线程池，可以很大程度上避免线程数的溢出问题。
	  
### 	  FD数超出限制

前面介绍了，当进程中的FD数量达到最大限制时，再去新建线程，在创建JNIEnv时会抛出OOM错误。但是FD数量超出限制除了会导致创建线程抛出OOM以外，还会导致很多其它的异常，为了能够统一处理这类FD数量溢出的问题，Probe中对进程中的FD数量做了监控。在后台启动一个线程，每隔1s读取一次当前进程创建的FD数量，当检测到FD数量达到阈值时（FD最大限制的95%），读取当前进程的所有FD信息归并后上报。

在/proc/pid/limits描述着Linux系统对对应进程的限制，其中Max open files就代表可创建FD的最大数目。

进程中创建的FD记录在/proc/pid/fd中，通过遍历/proc/pid/fd，可以得到FD的信息。

获取FD信息：

		File fdFile=new File("/proc/" + Process.myPid() + "/fd");
		File[] files = fdFile.listFiles();  
		int length = files.length; //即进程中的fd数量
		for (int i = 0; i < length ; i++) {
		  if (Build.VERSION.SDK_INT >= 21) {
		         Os.readlink(files[i].getAbsolutePath()); /最近工作：
大话西游接入讨论
账号及用户反馈问题跟踪排查
首猜视频及埋点的一些问题
三方登录异常监听、Bugly日志分类 李尚
推送扩容，提升上限
商品分享埋点新增参数
urs 严选 账号411问题处理
五月份已知要做的
【Pro】会员中心页新增Pro星选固定板块-425
大话西游接入讨论/得到软链接实际指向的文件
		     } else {
		      //6.0以下系统可以通过执行readlink命令去得到软连接实际指向文件，但是耗时较久
		  }
		}
		
得到进程中所有的FD信息后，我们会先按照FD的类型进行一个归并，FD的用途主要有打开文件、创建socket连接、创建handlerThread等。
	  

#### native oom自动补全

Studio写例子，可以自动补全

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/87d7bbbde2fc4c25be530f0e00f08ded~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=1770&h=272&s=111782&e=png&b=2a2c30)
	  
动态注册模板

	//动态注册
	static JNINativeMethod gMethods[] = {
	        {"oomCrash", "([I)V", (void *) Java_com_snail_labaffinity_activity_MainActivity_oomCrash},
	};
	
	JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
	    JNIEnv *env = NULL;
	    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
	        return JNI_ERR;
	    }
	    jclass clazz = env->FindClass("com/snail/labaffinity/activity/MainActivity");
	    if (clazz == NULL) {
	        return JNI_ERR;
	    }
	    if (env->RegisterNatives(clazz, gMethods, sizeof(gMethods) / sizeof(gMethods[0])) < 0) {
	        return JNI_ERR;
	    }
	    return JNI_VERSION_1_6;
	}	  
	
### 	OOM能捕获吗？之后系统就会死吗？

OOM能捕获，如果不处理，还不会死，UI线程甚至还可以继续运行，但是后续的内存分配会挂掉

### 线程创建过多

一个进程能创建线程的数量有限，可以通过命令查看

	cat /proc/sys/kernel/threads-max
	
	13435
	
可以看到该手机上限是13435，其实是很高的，一般而言OOM的时候，虚拟内存会先于上限挂掉。
	
![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7140382d7d33487498fd7d1d8627e4df~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=1348&h=936&s=132653&e=png&b=2b2d30)

真实内存并未用多少，

如何判断线程过多，并定位出问题的线程来源

参考[https://juejin.cn/post/6868916019746996237](https://juejin.cn/post/6868916019746996237)

### * 最基本的JVM 堆分配抛出


Failed to allocate a后面跟的数字很大，说明是需要一大块内存，也就是大对象，此时检查代码中是否存在大对象，如果有则想办法降低内存的使用或者在Native中处理；如果数字很小，说明此时堆内存已经不足，很有可能出现内存泄露，也有可能是已经存在有大对象，此时dump内存快照无疑是最方便直接的方式

参考 https://www.jianshu.com/p/3233c33f6a79
 
	  std::ostringstream oss;
	  size_t total_bytes_free = GetFreeMemory();
	  oss << "Failed to allocate a " << byte_count << " byte allocation with " << total_bytes_free
	      << " free bytes and " << PrettySize(GetFreeMemoryUntilOOME()) << " until OOM,"
	      << " target footprint " << target_footprint_.load(std::memory_order_relaxed)
	      << ", growth limit "
	      << growth_limit_;
	  // If the allocation failed due to fragmentation, print out the largest continuous allocation.
	  if (total_bytes_free >= byte_count) {
	    // 
	    // There is no fragmentation info to log for large-object space.
	    if (allocator_type != kAllocatorTypeLOS) {
	      CHECK(space != nullptr) << "allocator_type:" << allocator_type
	                              << " byte_count:" << byte_count
	                              << " total_bytes_free:" << total_bytes_free;
	      space->LogFragmentationAllocFailure(oss, byte_count);
	    }
	  }
	  self->ThrowOutOfMemoryError(oss.str().c_str());
	}
	  
## 	  参考文档

[OOM问题定位](https://tech.meituan.com/2019/11/14/crash-oom-probe-practice.html)
Android 启动线程OOM[https://blog.csdn.net/LiC_07093128/article/details/79451851](https://blog.csdn.net/LiC_07093128/article/details/79451851)

[参考文档 内存空间 https://www.cnblogs.com/binlovetech/p/16824522.html](https://www.cnblogs.com/binlovetech/p/16824522.html)

[参考文档  OOM类型 https://www.wuyifei.cc/android-oom/](https://www.wuyifei.cc/android-oom/)