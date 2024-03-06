JVM 运行时内存布局

![](https://static001.infoq.cn/resource/image/b4/62/b4ff890142874a6cbef1ad7a80eb7462.png)

分两类，Thread独享的内存

Thread独享的内存：线程创建时，相应的区域分配内存，线程销毁时，释放相应内存
Thread共享的内存： Heap： GC 垃圾回收的主站场、Method Area：方法区、Runtime Constant Pool

除了 PC Register 区不会抛出 StackOverflowError 或 OutOfMemoryError ，其它 5 个区域，当请求分配的内存不足时，均会抛出 OutOfMemoryError（即：OOM），其中 thread 独立的 JVM Stack 区及 Native Method Stack 区还会抛出 StackOverflowError

## **如何判断一个Java对象是可回收的? **

![](https://static001.infoq.cn/resource/image/e3/71/e36c624e8b4300775123f95a34b86571.png)

###  引用计数法  

无法解决循环引用的问题，A引用B，B同时引用A，AB都无用的时候，两者无法回收。

### 可达性分析法：对象是否可达

目前的虚拟机基本都是采用可达性分析算法来判断对象是否存活，这种算法以GC Root对象为起点，遍历出所有的引用子节点，再以子节点为起点，引出此节点指向的下一个结点，直到所有的结点都遍历完毕,任何在在这个引用链上的节点都可以认为是可达的，否则就是不可达的。那么GC ROOT是怎么定义呢：

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/aa9caf1956c34223b36b87f21ed8d6ef~tplv-k3u1fbpfcp-watermark.image?)

上图中的A 以及 AA的静态变量都可以作为GC root，它们引出的强引用链都是可达的对象。而GG GG2没有在GC Root的引用链上，就可以被回收。哪些对象可以作为GC ROOT。

### 常见的GC Root种类 ：两栈两方法

> GC Root：A garbage collection root is an object that is accessible from outside the heap. 

**GC管理的主要区域是Java堆**，方法区、栈和本地方法区不被GC所管理,也正是这些不被管理的对象可以作为GC roots,被GC roots引用的对象不被GC回收。

* 运行线程JVM栈上（栈帧）引用的对象
* 运行线程上Native方法栈中JNI引用的对象
* 方法区中类**静态属性变量引用的对象**
* 方法区中**常量引用的对象**
* 虚拟机内部的引用，比如系统类加载器加载的对象等，无需考虑

### 方法区如何判断是否需要回收

方法区主要回收的内容有：废弃常量和无用的类。对于废弃常量也可通过引用的可达性来判断，但是对于无用的类则需要同时满足下面3个条件：

* 该类所有的实例都已经被回收，也就是Java堆中不存在该类的任何实例；
* 加载该类的ClassLoader已经被回收；
* 该类对应的java.lang.Class对象没有在任何地方被引用，无法在任何地方通过反射访问该类的方法。
 
## **如何处理回收**

### mark-sweep 标记清除法：缺点碎片化严重，存储小对象

1. 根据可达性算法(根搜索) 所标记的不可达对象
2. 当所有的待回收的“垃圾对象”标记完成之后， 统一清除。


### mark-compact 标记-整理（也称标记-压缩）法 ：不会再有碎片问题，但是时间复杂度较高【需要调整引用的地址】，并暂停应用 

1. 根据可达性算法(根搜索) 所标记的不可达对象，
2. 当所有的待回收的“垃圾对象”标记完成之后，不直接清除掉可回收对象 ，而是让所有的对象都向一端移动，然后将端边界以外的内存全部清理掉

也可以看做标记-压缩-清理算法，标记-整理算法主要是针对老年代来设计，内存变动较少

### mark-copy 标记复制法：高效，可用的内存减小了一半，空间是连续的，并且效率较高


1. 将内存区域均分为了两块（记为S0和S1）
2. 一块满了复制到另一块

复制算法 一般会用于对象存活时间比较短的区域，例如 年轻代，复制过多，影响效率

### generation-collect 分代收集算法：分代不是具体的算法，而是策略，实际算法的选取策略

背景：由于每个收集的算法都没办法符合所有的场景，就好比每个对象所在的内存阶段不一样，被回收的概率也不一样，比如在新生代，基本可以说90%以上的都会被回收，而到老年代接近一半以上的对象则是一半存活的，所以针对这两种不同的场景，回收的策略肯定有所不一样，所以引发而出的就是分代收集算法，根据新生代和老年代不同的场景而用不同的算法，比如新生代用复制算法，而老年代则用标记-整理算法。

新生代（Yong Gen）

    年轻代特点：区域相对老年代较小，对象生存周期短，存活率低，回收频繁。所以适合-标记-复制算法;

老年代（Tenured Gen）

    老年代特点：区域较大，对象生命周期长、存活率高，回收不频繁，所以更适合-标记-整理算法;

像CMS、G1这些垃圾收集器都属于这个分代思想演化而来，JDK8默认的收集器是CMS新生代区域使用标记-复制算法，老年代区域使用标记-整理算法


## Android GC ：页表是虚拟内存与物理内存的映射关键

* Android的堆包含Active堆+Zygote堆，采用的是copy-on-write机制，在写Active堆的时候，才会引发缺页中断，真正的为Activie堆分配内存，至于Zygote堆，它的页表是固定的，对应的物理内存也是固定的，不会改变。 

* 在Java堆分配内存前后，要对Java堆进行加锁和解锁，避免多个线程同时对Java堆进行操作。这分别是通过函数dvmLockHeap和dvmunlockHeap来实现的


早期Android四种类的GC：
	
	/* Not enough space for an "ordinary" Object to be allocated. */  //分配内存不足导致的GC
	extern const GcSpec *GC_FOR_MALLOC;  
	  
	/* Automatic GC triggered by exceeding a heap occupancy threshold. */   //超过堆占用阈值触发自动GC
	extern const GcSpec *GC_CONCURRENT;  
	  
	/* Explicit GC via Runtime.gc(), VMRuntime.gc(), or SIGUSR1. */    //Runtime.gc(), VMRuntime.gc() 主动触发的GC
	extern const GcSpec *GC_EXPLICIT;  
	  
	/* Final attempt to reclaim memory before throwing an OOM. */  
	extern const GcSpec *GC_BEFORE_OOM;  

GC_FOR_MALLOC、GC_CONCURRENT和GC_BEFORE_OOM三种类型的GC都是在分配对象的过程触发的，根据是否回收软引用，执行的GC不同

	static void gcForMalloc(bool clearSoftReferences)  
	{  
	    ......  
	  
	    const GcSpec *spec = clearSoftReferences ? GC_BEFORE_OOM : GC_FOR_MALLOC;  
	    dvmCollectGarbageInternal(spec);  
	}  

dvmHeapSourceAlloc成功地在Active堆上分配到一个对象之后，就会检查Active堆当前已经分配的内存（heap->bytesAllocated）是否大于预设的阀值（heap->concurrentStartBytes）如果大于，那么就会通过条件变量gHs->gcThreadCond唤醒GC线程进行垃圾回收。预设的阀值（heap->concurrentStartBytes）是一个比指定的堆最小空闲内存小128K的数值。也就是说，当堆的空闲内不足时，就会触发GC_CONCURRENT类型的GC

GC线程是Dalvik虚拟机启动的过程中创建的，它的执行体函数是gcDaemonThread，实现如下所示：   dvmCollectGarbageInternal(GC_CONCURRENT);  

	static void *gcDaemonThread(void* arg)  
	{  
	    dvmChangeStatus(NULL, THREAD_VMWAIT);  
	    dvmLockMutex(&gHs->gcThreadMutex);  
	    while (gHs->gcThreadShutdown != true) {  
	        bool trim = false;  
	        if (gHs->gcThreadTrimNeeded) {  
	            int result = dvmRelativeCondWait(&gHs->gcThreadCond, &gHs->gcThreadMutex,  
	                    HEAP_TRIM_IDLE_TIME_MS, 0);  
	            if (result == ETIMEDOUT) {  
	                /* Timed out waiting for a GC request, schedule a heap trim. */  
	                trim = true;  
	            }  
	        } else {  
	            dvmWaitCond(&gHs->gcThreadCond, &gHs->gcThreadMutex);  
	        }  
	  
	        ......  
	  
	        dvmLockHeap();  
	  
	        if (!gDvm.gcHeap->gcRunning) {  
	            dvmChangeStatus(NULL, THREAD_RUNNING);  
	            if (trim) {  
	                trimHeaps();  
	                gHs->gcThreadTrimNeeded = false;  
	            } else {  
	                dvmCollectGarbageInternal(GC_CONCURRENT);  
	                gHs->gcThreadTrimNeeded = true;  
	            }  
	            dvmChangeStatus(NULL, THREAD_VMWAIT);  
	        }  
	        dvmUnlockHeap();  
	    }  
	    dvmChangeStatus(NULL, THREAD_RUNNING);  
	    return NULL;  
	}


 非并行GC 并行GC的执行过程。它们的总体流程是相似的，主要差别在于前者在执行的过程中一直是挂起**非GC线程的**，而后者是**有条件地挂起非GC线程**。

### 参考文档

[GC Roots 是什么？哪些对象可以作为 GC Root？看完秒懂！](https://blog.csdn.net/weixin_38007185/article/details/108093716)
[Android R常见GC类型与问题案例](https://blog.csdn.net/feelabclihu/article/details/120574383)
[一文看懂 JVM 内存布局及 GC 原理](https://www.infoq.cn/article/3wyretkqrhivtw4frmr3)
[JVM之垃圾回收机制（GC）](https://juejin.cn/post/7123853933801373733) 