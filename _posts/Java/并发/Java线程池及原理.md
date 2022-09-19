Java语言虽然内置了多线程支持，启动一个新线程非常方便，但是，创建线程需要操作系统资源（线程资源，栈空间等），频繁创建和销毁大量线程需要消耗大量时间。简单地说，线程池内部维护了若干个线程，没有任务的时候，这些线程都处于等待状态。如果有新任务，就分配一个空闲线程执行。如果所有线程都处于忙碌状态，新任务要么放入队列等待，要么增加一个新线程进行处理。所以线程池逃不开两个东西，队列跟线程。

## 使用


JAVA中创建线程池主要有两类方法，一类是通过**Executors工厂类**提供的方法，该类提供了4种不同的线程池可供使用。另一类是通过**ThreadPoolExecutor实现类**进行自定义创建。

JAVA通过Executors工厂类提供了四种线程池，单线程化线程池(newSingleThreadExecutor)、可控最大并发数线程池(newFixedThreadPool)、可回收缓存线程池(newCachedThreadPool)、支持定时与周期性任务的线程池(newScheduledThreadPool)


### 利用Executors工厂创建的线程池有如下三种

* FixedThreadPool：线程数固定的线程池；
* CachedThreadPool：线程数根据任务动态调整的线程池； 理论上无限大
* SingleThreadExecutor：仅单线程执行的线程池。

上面三种内部用的都是ThreadPoolExecutor

    public static ExecutorService newSingleThreadExecutor() {
        return new FinalizableDelegatedExecutorService
            (new ThreadPoolExecutor(1, 1,
                                    0L, TimeUnit.MILLISECONDS,
                                    new LinkedBlockingQueue<Runnable>()));
    }


不过用FinalizableDelegatedExecutorService封装了一下


    static class FinalizableDelegatedExecutorService
        extends DelegatedExecutorService {
        FinalizableDelegatedExecutorService(ExecutorService executor) {
            super(executor);
        }
        protected void finalize() {
            super.shutdown();
        }
    }
    
 应该是JVM为了防止浪费，在GC前利用   finalize将线程池关闭，回收资源。
 
###  利用ThreadPoolExecutor直接定制创建
 


 
    /**
     * Creates a new {@code ThreadPoolExecutor} with the given initial
     * parameters.
     *
     * @param corePoolSize the number of threads to keep in the pool, even
     *        if they are idle, unless {@code allowCoreThreadTimeOut} is set
     * @param maximumPoolSize the maximum number of threads to allow in the
     *        pool
     * @param keepAliveTime when the number of threads is greater than
     *        the core, this is the maximum time that excess idle threads
     *        will wait for new tasks before terminating.
     * @param unit the time unit for the {@code keepAliveTime} argument
     * @param workQueue the queue to use for holding tasks before they are
     *        executed.  This queue will hold only the {@code Runnable}
     *        tasks submitted by the {@code execute} method.
     * @param threadFactory the factory to use when the executor
     *        creates a new thread
     * @param handler the handler to use when execution is blocked
     *        because the thread bounds and queue capacities are reached
     * @throws IllegalArgumentException if one of the following holds:<br>
     *         {@code corePoolSize < 0}<br>
     *         {@code keepAliveTime < 0}<br>
     *         {@code maximumPoolSize <= 0}<br>
     *         {@code maximumPoolSize < corePoolSize}
     * @throws NullPointerException if {@code workQueue}
     *         or {@code threadFactory} or {@code handler} is null
     */
     
    public ThreadPoolExecutor(int corePoolSize,
                              int maximumPoolSize,
                              long keepAliveTime,
                              TimeUnit unit,
                              BlockingQueue<Runnable> workQueue,
                              ThreadFactory threadFactory,
                              RejectedExecutionHandler handler) {
        if (corePoolSize < 0 ||
            maximumPoolSize <= 0 ||
            maximumPoolSize < corePoolSize ||
            keepAliveTime < 0)
            throw new IllegalArgumentException();
        if (workQueue == null || threadFactory == null || handler == null)
            throw new NullPointerException();
        this.acc = System.getSecurityManager() == null ?
                null :
                AccessController.getContext();
        this.corePoolSize = corePoolSize;
        this.maximumPoolSize = maximumPoolSize;
        this.workQueue = workQueue;
        this.keepAliveTime = unit.toNanos(keepAliveTime);
        this.threadFactory = threadFactory;
        this.handler = handler;
    }

 
 ThreadPoolExecutor有7个参数，参数之间可能会有影响：
 
* corePoolSize核心线程池大小，默认情况下，即使它们处于idle状态也不销毁，除非用户设置了allowCoreThreadTimeOut，设置后，核心线程允许超时，超时时间就是keepAliveTime*unit，在这种情况下，核心线程数量可以缩减，甚至为0。

	      public void allowCoreThreadTimeOut(boolean value) {
	        if (value && keepAliveTime <= 0)
	            throw new IllegalArgumentException("Core threads must have nonzero keep alive times");
	        if (value != allowCoreThreadTimeOut) {
	            allowCoreThreadTimeOut = value;
	            if (value)
	                interruptIdleWorkers();
	        }
	    }
	    
* maximumPoolSize线程池中最大的存活线程数，对于超出corePoolSize部分的线程，如果处于空闲状态，都会超时机制，超时时间keepAliveTime*unit。
* keepAliveTime  unit 共同定义超时时间
* workQueue【BlockingQueue】作用就是让暂时无法获取线程的任务进入队列，等待执行，当调用**execute【最终调用】**方法时，如果线程池中没有空闲可用线程，任务就会入队。

		ArrayBlockingQueue	一个由数组结构组成的有界阻塞队列。
		LinkedBlockingQueue	一个由链表结构组成的有界阻塞队列。
		SynchronousQueue	一个不存储元素的阻塞队列，即直接提交给线程不保持它们。
		PriorityBlockingQueue	一个支持优先级排序的无界阻塞队列。
		DelayQueue	一个使用优先级队列实现的无界阻塞队列，只有在延迟期满时才能从中提取元素。
		LinkedTransferQueue	一个由链表结构组成的无界阻塞队列。与SynchronousQueue类似，还含有非阻塞方法。
		LinkedBlockingDeque	一个由链表结构组成的双向阻塞队列。

* threadFactory 【ThreadFactory】线程工厂类，一般都是默认Executors.defaultThreadFactory()
* handler【RejectedExecutionHandler】 这个参数是用来执行拒绝策略的，当提交任务时既没有空闲线程，任务队列也满了【有些BlockingQueue可以设置数量上限】，就会执行拒绝操作。


线程池在程序结束的时候要关闭。使用shutdown()方法关闭线程池的时候，它会等待正在执行的任务先完成，然后再关闭。shutdownNow()会立刻停止正在执行的任务，awaitTermination()则会等待指定的时间让线程池关闭。



Executor执行器体系  

![](https://s2.51cto.com/images/blog/202107/09/0b45d217c971425c8b3a276e6d7f4e89.png?x-oss-process=image/watermark,size_16,text_QDUxQ1RP5Y2a5a6i,color_FFFFFF,t_30,g_se,x_10,y_10,shadow_20,type_ZmFuZ3poZW5naGVpdGk=)


参考文档

https://www.cnblogs.com/pcheng/p/13540619.html