* 创建线程失败
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

#### 创建线程失败

	void Thread::CreateNativeThread(JNIEnv* env, jobject java_peer, size_t stack_size, bool is_daemon)
	抛出时的错误信息：
	    "Could not allocate JNI Env"
	  或者
	    StringPrintf("pthread_create (%s stack) failed: %s", PrettySize(stack_size).c_str(), strerror(pthread_create_result)));
	    
	    
> 创建JNI失败：创建JNIEnv可以归为两个步骤：

通过Andorid的匿名共享内存（Anonymous Shared Memory）分配 4KB（一个page）内核态内存。
再通过Linux的mmap调用映射到用户态虚拟内存地址空间。

第一步创建匿名共享内存时，需要打开/dev/ashmem文件，所以需要一个FD（文件描述符）。此时，如果创建的FD数已经达到上限，则会导致创建JNIEnv失败，抛出错误信息如下：

	E/art: ashmem_create_region failed for 'indirect ref table': Too many open files
	 java.lang.OutOfMemoryError: Could not allocate JNI Env
	   at java.lang.Thread.nativeCreate(Native Method)
	   at java.lang.Thread.start(Thread.java:730)
	   

第二步调用mmap时，如果进程虚拟内存地址空间耗尽，也会导致创建JNIEnv失败，抛出错误信息如下：

E/art: Failed anonymous mmap(0x0, 8192, 0x3, 0x2, 116, 0): Operation not permitted. See process maps in the log.
java.lang.OutOfMemoryError: Could not allocate JNI Env
  at java.lang.Thread.nativeCreate(Native Method)
  at java.lang.Thread.start(Thread.java:1063)
  
>   创建线程失败
  
创建线程也可以归纳为两个步骤：

调用mmap分配栈内存。这里mmap flag中指定了MAP_ANONYMOUS，即匿名内存映射。这是在Linux中分配大块内存的常用方式。其分配的是虚拟内存，对应页的物理内存并不会立即分配，而是在用到的时候触发内核的缺页中断，然后中断处理函数再分配物理内存。
调用clone方法进行线程创建。

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
	  
	  
	  
	
### 分析

#### 堆OOM

堆内存分配失败，通常说明进程中大部分的内存已经被占用了，且不能被垃圾回收器回收，一般来说此时内存占用都存在一些问题，例如内存泄漏等。要想定位到问题所在，就需要知道进程中的内存都被哪些对象占用，以及这些对象的引用链路。而这些信息都可以在Java内存快照文件中得到，调用Debug.dumpHprofData(String fileName)函数就可以得到当前进程的Java内存快照文件（即HPROF文件）。所以，关键在于要获得进程的内存快照，由于dump函数比较耗时，在发生OOM之后再去执行dump操作，很可能无法得到完整的内存快照文件。



### 线程数超出限制

 
对于创建线程失败导致的OOM，Probe会获取当前进程所占用的虚拟内存、进程中的线程数量、每个线程的信息（线程名、所属线程组、堆栈信息）以及系统的线程数限制，并将这些信息上传用于分析问题。

/proc/sys/kernel/threads-max规定了每个进程创建线程数目的上限。在华为的部分机型上，这个上限被修改的很低（大约500），比较容易出现线程数溢出的问题，而大部分手机这个限制都很大（一般为1W多）。在这些手机上创建线程失败大多都是因为虚拟内存空间耗尽导致的，进程所使用的虚拟内存可以查看/proc/pid/status的VmPeak/VmSize记录。

然后通过Thread.getAllStackTraces()可以得到进程中的所有线程以及对应的堆栈信息。

一般来说，当进程中线程数异常增多时，都是某一类线程被大量的重复创建。所以我们只需要定位到这类线程的创建时机，就能知道问题所在。如果线程是有自定义名称的，那么直接就可以在代码中搜索到创建线程的位置，从而定位问题，如果线程创建时没有指定名称，那么就需要通过该线程的堆栈信息来辅助定位。下面这个例子，就是一个“crowdSource msg”的线程被大量重复创建，在代码中搜索名称很快就查出了问题。针对这类线程问题推荐的做法就是在项目中统一使用线程池，可以很大程度上避免线程数的溢出问题。
	  
	  
	  
	  
## 	  参考文档

[OOM问题定位](https://tech.meituan.com/2019/11/14/crash-oom-probe-practice.html)
