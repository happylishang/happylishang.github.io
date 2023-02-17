Kotlin协程不是线程，但协程的实现还是离不开线程，协程最大的作用感觉是讲耗时任务的回调转变成了同步的调用，或者说：更加方便的写阻塞任务。


### Java与kotlin协程使用的对比：Java相对冗余

kotlin的协程看起来是个新概念，其核心实现还是Java的封装，它好像干掉了回调，但是其实不过是封装成了阻塞，但是Java中如果是阻塞，则必定会牵扯Task及线程，由于Java不是函数式语言，任务也必须封装成对象，所以使用起来有些繁琐，但是如果加以对比，其实发现Java也可以做到：

	<!--Java框架中的实现-->
	fun javaCoroutines() {
	    val executor: ExecutorService = Executors.newCachedThreadPool()
	    <!--类似于启动协程-->
	    executor.execute {
	        println(
	        <!--相比而言多了一层，因为需要获取一个可以阻塞等待的对象-->
	        executor.submit(Callable {
	            println("子线程在进行计算");
	            Thread.sleep(2000)
	            "结果" + System.currentTimeMillis()
	        }).get()
	        )
	    }
	}

如果将协程函数看做是一个Task对象其实就很好跟Java对应起来，
	
		suspend fun task() {
	    println("子线程在进行计算");
	    delay(2000)
	    "结果" + System.currentTimeMillis()
	}

	<!--kotlin实现-->
	fun kotlinCoroutines() {
	
	<!--kotlin的协程框架将上述Java的工作进行了封装，所以写起来简化-->
	    runBlocking(Dispatchers.IO) {
	        val result = task()
	        println(result)
	    }
	}  
  通过对比其实可以看出：kotlin的协程在用法上，其实就是更好的Task封装，或者说少了一层，Java没法直接传递函数，只能用Runable封装一下。
  


### kotlin协程封装原理

GlobalScope.launch或者runBlocking究竟发生了什么

	   runBlocking(block = {
	        
	    })
	    
	    
	    #Builders.kt

    public fun <T> runBlocking(context: CoroutineContext = EmptyCoroutineContext, block: suspend CoroutineScope.() -> T): T {

        //当前线程

        val currentThread = Thread.currentThread()

        //先看有没有拦截器

        val contextInterceptor = context[ContinuationInterceptor]

        val eventLoop: EventLoop?

        val newContext: CoroutineContext

        //----------①

        if (contextInterceptor == null) {

            //不特别指定的话没有拦截器，使用loop构建Context

            eventLoop = ThreadLocalEventLoop.eventLoop

            newContext = GlobalScope.newCoroutineContext(context + eventLoop)

        } else {

            eventLoop = (contextInterceptor as? EventLoop)?.takeIf { it.shouldBeProcessedFromContext() }

                ?: ThreadLocalEventLoop.currentOrNull()

            newContext = GlobalScope.newCoroutineContext(context)

        }

        //BlockingCoroutine 顾名思义，阻塞的协程

        val coroutine = BlockingCoroutine<T>(newContext, currentThread, eventLoop)

        //开启

        coroutine.start(CoroutineStart.DEFAULT, coroutine, block)

        //等待协程执行完成----------②

        return coroutine.joinBlocking()

    }
    
    
    

GlobalScope.launch{
		}
		

Tools –> Kotlin –> Show Kotlin Bytecode->Decompile可以看到转换后的字节码，看下GlobalScope定义，它是个单利，继承自CoroutineScope，实现方法是get返回的值是EmptyCoroutineContext，GlobalScope被废弃了，但是对于理解协程还是有帮助的

	public object GlobalScope : CoroutineScope {
	
	    override val coroutineContext: CoroutineContext
	        get() = EmptyCoroutineContext
	}

	@SinceKotlin("1.3")
	public object EmptyCoroutineContext : CoroutineContext, Serializable {
	    private const val serialVersionUID: Long = 0
	    private fun readResolve(): Any = EmptyCoroutineContext
	
	    public override fun <E : Element> get(key: Key<E>): E? = null
	    public override fun <R> fold(initial: R, operation: (R, Element) -> R): R = initial
	    public override fun plus(context: CoroutineContext): CoroutineContext = context
	    public override fun minusKey(key: Key<*>): CoroutineContext = this
	    public override fun hashCode(): Int = 0
	    public override fun toString(): String = "EmptyCoroutineContext"
	}
		
EmptyCoroutineContext是一个			CoroutineContext，这里有了一个CoroutineContext[协程上下文]的概念，继续看CoroutineScope.launch，它是CoroutineScope的扩展函数，参数有三个，使用时前两个有默认参数，一般可以省略，返回Job

	public fun CoroutineScope.launch(
	    context: CoroutineContext = EmptyCoroutineContext,
	    start: CoroutineStart = CoroutineStart.DEFAULT,
	    block: suspend CoroutineScope.() -> Unit
	): Job {
	    val newContext = newCoroutineContext(context)
	    val coroutine = if (start.isLazy)
	        LazyStandaloneCoroutine(newContext, block) else
	        StandaloneCoroutine(newContext, active = true)
	    coroutine.start(start, coroutine, block)
	    return coroutine
	}
	
launch函数首先会调用	 newCoroutineContext，构建一个新的上下文，之后基于新的上下文构建一个   StandaloneCoroutine，启动它，之后将StandaloneCoroutine作为返回值返回给调用者。

	
	private open class StandaloneCoroutine(
	    parentContext: CoroutineContext,
	    active: Boolean
	) : AbstractCoroutine<Unit>(parentContext, initParentJob = true, active = active) {
	    override fun handleJobException(exception: Throwable): Boolean {
	        handleCoroutineException(context, exception)
	        return true
	    }
	}
StandaloneCoroutine内部值重写了	handleJobException异常处理方法，


###   Android的  lifecycleScope.launch 


### LifecycleCoroutineScopeImpl

### CoroutineScheduler 最终还是要借助线程池而协程最终也会走向Task Runable封装


	internal class CoroutineScheduler(
	    @JvmField val corePoolSize: Int,
	    @JvmField val maxPoolSize: Int,
	    @JvmField val idleWorkerKeepAliveNs: Long = IDLE_WORKER_KEEP_ALIVE_NS,
	    @JvmField val schedulerName: String = DEFAULT_SCHEDULER_NAME
	) : Executor, Closeable {
	    //...
	    override fun execute(command: Runnable) = dispatch(command)

	 fun dispatch(block: Runnable, taskContext: TaskContext = NonBlockingContext, tailDispatch: Boolean = false) {
	        trackTask() // this is needed for virtual time support
	        val task = createTask(block, taskContext)
	        // try to submit the task to the local queue and act depending on the result
	        val currentWorker = currentWorker()
	        val notAdded = currentWorker.submitToLocalQueue(task, tailDispatch)
	        if (notAdded != null) {
	            if (!addToGlobalQueue(notAdded)) {
	                // Global queue is closed in the last step of close/shutdown -- no more tasks should be accepted
	                throw RejectedExecutionException("$schedulerName was terminated")
	            }
	        }
	        val skipUnpark = tailDispatch && currentWorker != null
	        // Checking 'task' instead of 'notAdded' is completely okay
	        if (task.mode == TASK_NON_BLOCKING) {
	            if (skipUnpark) return
	            signalCpuWork()
	        } else {
	            // Increment blocking tasks anyway
	            signalBlockingWork(skipUnpark = skipUnpark)
	        }
	    }






