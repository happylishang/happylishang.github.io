在java的java.util.concurrent包中定义，可以看到组要适用于并发，Future本身是一个接口，仅仅是一个接口，一个规范，内部如何实现，如何处理，是需要用户自己操作的，使用Future接口的其实就是想要规范的告诉别人，我这里要定义一些同步等待的框架出来。

另外Future模式是多线程开发中非常常见的一种设计模式，它的核心思想是异步调用。Future模式可以这样来描述：我有一个任务，提交给了Future，Future替我完成这个任务。期间我自己可以去做任何想做的事情。一段时间之后，我就便可以从Future那儿取出结果。就相当于下了一张订货单，一段时间后可以拿着提订单来提货，这期间可以干别的任何事情。其中Future 接口就是订货单，真正处理**订单的是Executor类**，它根据Future接口的要求来生产产品。

示例用法

	   void work(){
	   	futureTask= executor.submit(FutureTask(TaskA)) // Future封装了一把
	   	doSomethingelse();
	      doSomethingelse();
	      	...
	   	future.get() // 看看完成没有，没完成，说不定还要继续等待，看用户自己的定义。
	   }

但是注意：Future本身只是一个接口，具体的灵活处理看其实现类自己，所以Future本身并不会单独使用，而是会被作为一个任务转接口在Executor框架的里面使用。Future主要包含下面四个接口：

	public interface Future<V> {
	 
	    boolean cancel(boolean mayInterruptIfRunning);
	
	    boolean isDone();
 
	    V get() throws InterruptedException, ExecutionException;
	
	    V get(long timeout, TimeUnit unit)
	        throws InterruptedException, ExecutionException, TimeoutException;
	}

实现接口相应的功能，并注意扩展，就可以结合线程池，实现Future模式，以FutureTask为例，ExecutorService在submit任务后，返回一个封装的FutureTask，我们构造一个单线程池，submit任务试试：

	    public static ExecutorService newSingleThreadExecutor() {
	        return new FinalizableDelegatedExecutorService
	            (new ThreadPoolExecutor(1, 1,
	                                    0L, TimeUnit.MILLISECONDS,
	                                    new LinkedBlockingQueue<Runnable>()));
	    }
	    
	   public static ExecutorService newFixedThreadPool(int nThreads, ThreadFactory threadFactory) {
        return new ThreadPoolExecutor(nThreads, nThreads,
                                      0L, TimeUnit.MILLISECONDS,
                                      new LinkedBlockingQueue<Runnable>(),
                                      threadFactory);
    }
    
FinalizableDelegatedExecutorService这个类继承DelegatedExecutorService，DelegatedExecutorService利用了代理设计模式，是对ExecutorService进行了一个包装，不直接暴露ExecutorService的接口能力，而FinalizableDelegatedExecutorService加上了finalize方法保证线程池的正确关闭。ThreadPoolExecutor始终是线程池最终的实现，其他都是代理封装而已。不过为了简化，直接用newFixedThreadPool来观察，它返回的直接是ThreadPoolExecutor，ThreadPoolExecutor 继承AbstractExecutorService，里面实现了submit

	    public Future<?> submit(Runnable task) {
	        if (task == null) throw new NullPointerException();
	        RunnableFuture<Void> ftask = newTaskFor(task, null);
	        execute(ftask);
	        return ftask;
	    }
	
可以看到，先封装成RunnableFuture，其实是new 了一个FutureTask，然后execute，假如到LinkedBlockQueue，Runnable一般不需要返回值，

	    protected <T> RunnableFuture<T> newTaskFor(Runnable runnable, T value) {
	        return new FutureTask<T>(runnable, value);
	    }

	    public FutureTask(Runnable runnable, V result) {
	        this.callable = Executors.callable(runnable, result);
	        this.state = NEW;      
	    }
    
	    public static <T> Callable<T> callable(Runnable task, T result) {
	        if (task == null)
	            throw new NullPointerException();
	        return new RunnableAdapter<T>(task, result);
	    }
	    
构建了一个Callable，或者说将Runnable转换成一个result null的callable ,没有自己设定result的，默认返回是null。

            val future = threadPoolExecutor.submit({
                Thread.sleep(2000)
                runOnUiThread {
                    ToastUtil.show("Future");
                }

            }, true)

Runable，submit会返回一个特定的返回值，用来标识当前的任务完成了，或者说区分哪个任务完成了，实际的意义不是很大。而且无法在task中用，线程池还是多靠Runable自身中的回调来自洽。用Callable呢，Callable的call必须有返回值，这个值就是Future将来get到的值，这里可以自己定制值。

      val future = threadPoolExecutor.submit(Callable {
                Thread.sleep(2000)
                "结果"
            })
       ToastUtil.show(future.get());

这样通过Callable可以获得返回值，而Runable说实话，就是Run，自己处理自己的回调逻辑。Callable需要主动关心。再看看封装了callable的FutureTask怎么走的，    execute(ftask);

 
    private final AtomicInteger ctl = new AtomicInteger(ctlOf(RUNNING, 0));

    public void execute(Runnable command) {
        if (command == null)
            throw new NullPointerException();
        /*
         * Proceed in 3 steps:
         *
         * 1. If fewer than corePoolSize threads are running, try to
         * start a new thread with the given command as its first
         * task.  The call to addWorker atomically checks runState and
         * workerCount, and so prevents false alarms that would add
         * threads when it shouldn't, by returning false.
         *
         * 2. If a task can be successfully queued, then we still need
         * to double-check whether we should have added a thread
         * (because existing ones died since last checking) or that
         * the pool shut down since entry into this method. So we
         * recheck state and if necessary roll back the enqueuing if
         * stopped, or start a new thread if there are none.
         *
         * 3. If we cannot queue task, then we try to add a new
         * thread.  If it fails, we know we are shut down or saturated
         * and so reject the task.
         */
        int c = ctl.get();
        if (workerCountOf(c) < corePoolSize) {
            if (addWorker(command, true))
                return;
            c = ctl.get();
        }
        if (isRunning(c) && workQueue.offer(command)) {
            int recheck = ctl.get();
            if (! isRunning(recheck) && remove(command))
                reject(command);
            else if (workerCountOf(recheck) == 0)
                addWorker(null, false);
        }
        else if (!addWorker(command, false))
            reject(command);
    }
    
假设没达到	coreSize，直接添加一个线程执行：
	
	 private boolean addWorker(Runnable firstTask, boolean core) {
	        retry:
	        for (int c = ctl.get();;) {
	            // Check if queue empty only if necessary.
	            if (runStateAtLeast(c, SHUTDOWN)
	                && (runStateAtLeast(c, STOP)
	                    || firstTask != null
	                    || workQueue.isEmpty()))
	                return false;
	
	            for (;;) {
	                if (workerCountOf(c)
	                    >= ((core ? corePoolSize : maximumPoolSize) & COUNT_MASK))
	                    return false;
	                if (compareAndIncrementWorkerCount(c))
	                    break retry;
	                c = ctl.get();  // Re-read ctl
	                if (runStateAtLeast(c, SHUTDOWN))
	                    continue retry;
	                // else CAS failed due to workerCount change; retry inner loop
	            }
	        }
	
	        boolean workerStarted = false;
	        boolean workerAdded = false;
	        Worker w = null;
	        try {
	            w = new Worker(firstTask);    // while (task != null || (task = getTask()) != null)   Worker的第一个对象是直接给的，后续的是从队列里拿的。
	            final Thread t = w.thread;
	            if (t != null) {
	                final ReentrantLock mainLock = this.mainLock;
	                mainLock.lock();
	                try {
	                    // Recheck while holding lock.
	                    // Back out on ThreadFactory failure or if
	                    // shut down before lock acquired.
	                    int c = ctl.get();
	
	                    if (isRunning(c) ||
	                        (runStateLessThan(c, STOP) && firstTask == null)) {
	                        if (t.getState() != Thread.State.NEW)
	                            throw new IllegalThreadStateException();
	                        workers.add(w);
	                        workerAdded = true;
	                        int s = workers.size();
	                        if (s > largestPoolSize)
	                            largestPoolSize = s;
	                    }
	                } finally {
	                    mainLock.unlock();
	                }
	                if (workerAdded) {
	                    t.start();
	                    workerStarted = true;
	                }
	            }
	        } finally {
	            if (! workerStarted)
	                addWorkerFailed(w);
	        }
	        return workerStarted;
	    }
之后进入FutureTask可能被执行，调用run

    public void run() {
        if (state != NEW ||
            !RUNNER.compareAndSet(this, null, Thread.currentThread()))
            return;
        try {
            Callable<V> c = callable;
            if (c != null && state == NEW) {
                V result;
                boolean ran;
                try {
                <!--调用-->
                    result = c.call();
                    <!--设置标签-->
                    ran = true;
                } catch (Throwable ex) {
                    result = null;
                    ran = false;
                    setException(ex);
                }
                if (ran)
                    set(result);
            }
        } finally {
            // runner must be non-null until state is settled to
            // prevent concurrent calls to run()
            runner = null;
            // state must be re-read after nulling runner to prevent
            // leaked interrupts
            int s = state;
            if (s >= INTERRUPTING)
                handlePossibleCancellationInterrupt(s);
        }
    }
在run的时候会根据条件判断执行，并设置一些设置，最后会将结果设置

    protected void set(V v) {
        if (STATE.compareAndSet(this, NEW, COMPLETING)) {
            outcome = v;
            STATE.setRelease(this, NORMAL); // final state 执行完毕的标识
            finishCompletion();
        }
    }
    	    
future对象get的时候，awaitDone会保证执行完毕，如果未完则等待。

	    public V get() throws InterruptedException, ExecutionException {
	        int s = state;
	        <!--如果此时已经执行完毕，直接返回-->
	        if (s <= COMPLETING)
	            s = awaitDone(false, 0L);
	        return report(s);
	    }
	
需要等待，看设置超时时间没有
	    
	     private int awaitDone(boolean timed, long nanos)
        throws InterruptedException {
        // The code below is very delicate, to achieve these goals:
        // - call nanoTime exactly once for each call to park
        // - if nanos <= 0L, return promptly without allocation or nanoTime
        // - if nanos == Long.MIN_VALUE, don't underflow
        // - if nanos == Long.MAX_VALUE, and nanoTime is non-monotonic
        //   and we suffer a spurious wakeup, we will do no worse than
        //   to park-spin for a while
        long startTime = 0L;    // Special value 0L means not yet parked
        WaitNode q = null;
        boolean queued = false;
        for (;;) {
            int s = state;
            if (s > COMPLETING) {
                if (q != null)
                    q.thread = null;
                return s;
            }
            else if (s == COMPLETING)
                // We may have already promised (via isDone) that we are done
                // so never return empty-handed or throw InterruptedException
                Thread.yield();
            else if (Thread.interrupted()) {
                removeWaiter(q);
                throw new InterruptedException();
            }
            else if (q == null) {
                if (timed && nanos <= 0L)
                    return s;
                q = new WaitNode();
            }
            else if (!queued)
                queued = WAITERS.weakCompareAndSet(this, q.next = waiters, q);   //会将等待的线程们入栈
            else if (timed) {
                final long parkNanos;
                if (startTime == 0L) { // first time
                    startTime = System.nanoTime();
                    if (startTime == 0L)
                        startTime = 1L;
                    parkNanos = nanos;
                } else {
                    long elapsed = System.nanoTime() - startTime;
                    if (elapsed >= nanos) {
                        removeWaiter(q);
                        return state;
                    }
                    parkNanos = nanos - elapsed;
                }
                // nanoTime may be slow; recheck before parking
                if (state < COMPLETING)
                    LockSupport.parkNanos(this, parkNanos);
            }
            else
                LockSupport.park(this);
        }
    }

finishCompletion会通知等待的线程队列唤起

	 private void finishCompletion() {
	        // assert state > COMPLETING;
	        for (WaitNode q; (q = waiters) != null;) {
	            if (WAITERS.weakCompareAndSet(this, q, null)) {
	                for (;;) {
	                    Thread t = q.thread;
	                    if (t != null) {
	                        q.thread = null;
	                        LockSupport.unpark(t);
	                    }
	                    WaitNode next = q.next;
	                    if (next == null)
	                        break;
	                    q.next = null; // unlink to help gc
	                    q = next;
	                }
	                break;
	            }
	        }
	        done();
	        callable = null;        // to reduce footprint
	    }	    
	    
最终还是 LockSupport.unpark	    、LockSupport.park来执行的。	    