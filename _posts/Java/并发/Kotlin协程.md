协程的基本使用


### kotlin runBlocking与Delay执行原理

runBlocking 的官方解释是：

> Runs a new coroutine and blocks the current thread interruptibly until its completion. This function should not be used from a coroutine. It is designed to bridge regular blocking code to libraries that are written in suspending style, to be used in main functions and in tests.


大意是：在当前线程启动一个可中断的协程，runBlocking会保证协程中的任务完成后才返回，不过runBlocking一般是用来测试代码的，不应该在正常的编码中使用。runBlocking默认使用 CoroutineDispatcher使用模型是一个Loop，也可以选择其他，先看看简单使用：

	fun main() {
	    runBlocking {
	        delay(500)
	        println("Current Thread ++ " + Thread.currentThread().name)
	    }
	}

runBlocking的实现在Builders.kt中

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
	            <!--默认是当前线程的，没有loop构建loop-->
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
    
**coroutine.joinBlocking()**会阻塞等待所有的任务完成才会结束，执行runBlocking后面的。 包括自己内部的直接协程，还有子协程，当然delay负责挂起协程，真正负责阻塞的是joinBlocking自身，而不是delay函数自身， delay只是将协程体挂起而已，等待被某个契机唤醒，runblock有自己的唤醒手段。
    
    @Suppress("UNCHECKED_CAST")
    fun joinBlocking(): T {
        registerTimeLoopThread()
        try {
            eventLoop?.incrementUseCount()
            try {
                while (true) {
                    @Suppress("DEPRECATION")
                    if (Thread.interrupted()) throw InterruptedException().also { cancelCoroutine(it) }
                    val parkNanos = eventLoop?.processNextEvent() ?: Long.MAX_VALUE
                    // note: process next even may loose unpark flag, so check if completed before parking
                    if (isCompleted) break
                    parkNanos(this, parkNanos)
                }
            } finally { // paranoia
                eventLoop?.decrementUseCount()
            }
        } finally { // paranoia
            unregisterTimeLoopThread()
        }
        // now return result
        val state = this.state.unboxState()
        (state as? CompletedExceptionally)?.let { throw it.cause }
        return state as T
    }



看一下反编译后的生成Java代码，携程block本地会被转化成SuspendLambda   Function2 对象，这步依靠kotlin编译插件完成，kotlin最终还是在Java的肩膀上跳舞，没有任何超越java框架的东西：

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/84bb1d18f73c47f09e3c777efe68f987~tplv-k3u1fbpfcp-watermark.image?)

而block会被转化成Function2 的实现类，封装了block的执行代码，runBlocking启动的协程代码快，当然也包含核心的suspend状态机，suspendlambda本质就是一个ContinuationImpl对象。
		
	final class RunBlockingTestKt$main$1 extends  implements Function2<CoroutineScope, Continuation<? super Unit>, Object> {
	    int label;
	
	    RunBlockingTestKt$main$1(Continuation<? super RunBlockingTestKt$main$1> continuation) {
	        super(2, continuation);
	    }
	
	    @Override // kotlin.coroutines.jvm.internal.BaseContinuationImpl
	    public final Continuation<Unit> create(Object obj, Continuation<?> continuation) {
	        return new RunBlockingTestKt$main$1(continuation);
	    }
	
	    public final Object invoke(CoroutineScope coroutineScope, Continuation<? super Unit> continuation) {
	        return ((RunBlockingTestKt$main$1) create(coroutineScope, continuation)).invokeSuspend(Unit.INSTANCE);
	    }
	
	    @Override // kotlin.coroutines.jvm.internal.BaseContinuationImpl
	    public final Object invokeSuspend(Object obj) {
	    <!--封装了block中的代码逻辑，当然核心的就是状态机-->
	        Object coroutine_suspended = IntrinsicsKt.getCOROUTINE_SUSPENDED();
	        int i = this.label;
	        if (i == 0) {
	            ResultKt.throwOnFailure(obj);
	            this.label = 1;
	            if (DelayKt.delay(500, this) == coroutine_suspended) {
	                return coroutine_suspended;
	            }
	        } else if (i == 1) {
	            ResultKt.throwOnFailure(obj);
	        } else {
	            throw new IllegalStateException("call to 'resume' before 'invoke' with coroutine");
	        }
	        System.out.println((Object) Intrinsics.stringPlus("Current Thread ++ ", Thread.currentThread().getName()));
	        return Unit.INSTANCE;
	    }
	}

kotlin库代码也会可被转化为Java方式，main中的调用实现在BuildersKt__BuildersKt中：

	public final /* synthetic */ class BuildersKt__BuildersKt {
	    public static /* synthetic */ Object runBlocking$default(CoroutineContext coroutineContext, Function2 function2, int i, Object obj) throws InterruptedException {
	        if ((i & 1) != 0) {
	            coroutineContext = EmptyCoroutineContext.INSTANCE;
	        }
	        return BuildersKt.runBlocking(coroutineContext, function2);
	    }
	
	    public static final <T> T runBlocking(CoroutineContext coroutineContext, Function2<? super CoroutineScope, ? super Continuation<? super T>, ? extends Object> function2) throws InterruptedException {
	        CoroutineContext coroutineContext2;
	        EventLoop eventLoop;
	        <!--找到当前线程-->
	        Thread currentThread = Thread.currentThread();
	        <!--找当前continuationInterceptor EmptyCoroutineContext就是null -->
	        ContinuationInterceptor continuationInterceptor = (ContinuationInterceptor) coroutineContext.get(ContinuationInterceptor.Key);
	        <!--查找eventLoop 不存在就构建   new BlockingEventLoop(Thread.currentThread())  -->
	        if (continuationInterceptor == null) {
	        <!--这里的Loop其实就是BlockingEventLoop-->
	            eventLoop = ThreadLocalEventLoop.INSTANCE.getEventLoop$kotlinx_coroutines_core();
	            <!--构建coroutineContext2-->
	            coroutineContext2 = CoroutineContextKt.newCoroutineContext(GlobalScope.INSTANCE, coroutineContext.plus(eventLoop));
	        } else {
	            EventLoop eventLoop2 = null;
	            EventLoop eventLoop3 = continuationInterceptor instanceof EventLoop ? (EventLoop) continuationInterceptor : null;
	            if (eventLoop3 != null && eventLoop3.shouldBeProcessedFromContext()) {
	                eventLoop2 = eventLoop3;
	            }
	            eventLoop = eventLoop2 == null ? ThreadLocalEventLoop.INSTANCE.currentOrNull$kotlinx_coroutines_core() : eventLoop2;
	            coroutineContext2 = CoroutineContextKt.newCoroutineContext(GlobalScope.INSTANCE, coroutineContext);
	        }
	        <!--构建BlockingCoroutine-->
	        BlockingCoroutine blockingCoroutine = new BlockingCoroutine(coroutineContext2, currentThread, eventLoop);
	        <!--blockingCoroutine准备启动function2，将function2对象加入 function2就是个block的封装，利用CoroutineStart.DEFAULT启动->
	        blockingCoroutine.start(CoroutineStart.DEFAULT, blockingCoroutine, function2);
	        <!--等啊-->
	        return (T) blockingCoroutine.joinBlocking();
	    }
	}
	
blockingCoroutine.start(CoroutineStart.DEFAULT, blockingCoroutine, function2)调用的是CoroutineStart.DEFAULT的invoke函数，最终调用CancellableKt的startCoroutineCancellable进行处理
	
	public final class CancellableKt {
	    public static final <T> void startCoroutineCancellable(Function1<? super Continuation<? super T>, ? extends Object> function1, Continuation<? super T> continuation) {
	        try {
	            Continuation intercepted = IntrinsicsKt.intercepted(IntrinsicsKt.createCoroutineUnintercepted(function1, continuation));
	            Result.Companion companion = Result.Companion;
	            DispatchedContinuationKt.resumeCancellableWith$default(intercepted, Result.m4247constructorimpl(Unit.INSTANCE), null, 2, null);
	        } catch (Throwable th) {
	            dispatcherFailure(continuation, th);
	        }
	    }

 而在这里会调用之前Function的create构造BaseContinuationImpl
 
	     public static final <T> Continuation<Unit> createCoroutineUnintercepted(Function1<? super Continuation<? super T>, ? extends Object> function1, Continuation<? super T> continuation) {
	        Intrinsics.checkNotNullParameter(function1, "<this>");
	        Intrinsics.checkNotNullParameter(continuation, "completion");
	        Continuation<?> probeCoroutineCreated = DebugProbesKt.probeCoroutineCreated(continuation);
	        if (function1 instanceof BaseContinuationImpl) {
	            return ((BaseContinuationImpl) function1).create(probeCoroutineCreated);
	        }
	        CoroutineContext context = probeCoroutineCreated.getContext();
	        if (context == EmptyCoroutineContext.INSTANCE) {
	            return new IntrinsicsKt__IntrinsicsJvmKt$createCoroutineUnintercepted$$inlined$createCoroutineFromSuspendFunction$IntrinsicsKt__IntrinsicsJvmKt$1(probeCoroutineCreated, function1);
	        }
	        return new IntrinsicsKt__IntrinsicsJvmKt$createCoroutineUnintercepted$$inlined$createCoroutineFromSuspendFunction$IntrinsicsKt__IntrinsicsJvmKt$2(probeCoroutineCreated, context, function1);
	    }
    
   后续调用 DispatchedContinuationKt.resumeCancellableWith$default处理 ，continuation其实封装了之前的封装任务
   
     public static final <T> void resumeCancellableWith(Continuation<? super T> continuation, Object obj, Function1<? super Throwable, Unit> function1) {
        boolean z;
        UndispatchedCoroutine<?> undispatchedCoroutine;
        if (continuation instanceof DispatchedContinuation) {
            DispatchedContinuation dispatchedContinuation = (DispatchedContinuation) continuation;
            Object state = CompletionStateKt.toState(obj, function1);
            if (dispatchedContinuation.dispatcher.isDispatchNeeded(dispatchedContinuation.getContext())) {
                dispatchedContinuation._state = state;
                dispatchedContinuation.resumeMode = 1;
                dispatchedContinuation.dispatcher.dispatch(dispatchedContinuation.getContext(), dispatchedContinuation);
                return;
            }
            DebugKt.getASSERTIONS_ENABLED();
            EventLoop eventLoop$kotlinx_coroutines_core = ThreadLocalEventLoop.INSTANCE.getEventLoop$kotlinx_coroutines_core();
            if (eventLoop$kotlinx_coroutines_core.isUnconfinedLoopActive()) {
                dispatchedContinuation._state = state;
                dispatchedContinuation.resumeMode = 1;
                eventLoop$kotlinx_coroutines_core.dispatchUnconfined(dispatchedContinuation);
                return;
            }
            DispatchedContinuation dispatchedContinuation2 = dispatchedContinuation;
            eventLoop$kotlinx_coroutines_core.incrementUseCount(true);
            try {
                Job job = (Job) dispatchedContinuation.getContext().get(Job.Key);
                if (job == null || job.isActive()) {
                    z = false;
                } else {
                    CancellationException cancellationException = job.getCancellationException();
                    dispatchedContinuation.cancelCompletedResult$kotlinx_coroutines_core(state, cancellationException);
                    Result.Companion companion = Result.Companion;
                    dispatchedContinuation.resumeWith(Result.m4247constructorimpl(ResultKt.createFailure(cancellationException)));
                    z = true;
                }
                if (!z) {
                    Continuation<T> continuation2 = dispatchedContinuation.continuation;
                    Object obj2 = dispatchedContinuation.countOrElement;
                    CoroutineContext context = continuation2.getContext();
                    Object updateThreadContext = ThreadContextKt.updateThreadContext(context, obj2);
                    if (updateThreadContext != ThreadContextKt.NO_THREAD_ELEMENTS) {
                        undispatchedCoroutine = CoroutineContextKt.updateUndispatchedCompletion(continuation2, context, updateThreadContext);
                    } else {
                        UndispatchedCoroutine undispatchedCoroutine2 = null;
                        undispatchedCoroutine = null;
                    }
                    try {
                        dispatchedContinuation.continuation.resumeWith(obj);
                        Unit unit = Unit.INSTANCE;
                    } finally {
                        if (undispatchedCoroutine == null || undispatchedCoroutine.clearThreadContext()) {
                            ThreadContextKt.restoreThreadContext(context, updateThreadContext);
                        }
                    }
                }
                do {
                } while (eventLoop$kotlinx_coroutines_core.processUnconfinedEvent());
            } catch (Throwable th) {
                eventLoop$kotlinx_coroutines_core.decrementUseCount(true);
                throw th;
            }
            eventLoop$kotlinx_coroutines_core.decrementUseCount(true);
            return;
        }
        continuation.resumeWith(obj);
    }     
 
 如果不需要派发，可以直接执行continuation.resumeWith【invokeSuspend】，否则可能就要走dispatchedContinuation.dispatcher.dispatch(dispatchedContinuation.getContext(), dispatchedContinuation);进行派发，
 
     public final override fun resumeWith(result: Result<Any?>) {
        // This loop unrolls recursion in current.resumeWith(param) to make saner and shorter stack traces on resume
        var current = this
        var param = result
        while (true) {
            // Invoke "resume" debug probe on every resumed continuation, so that a debugging library infrastructure
            // can precisely track what part of suspended callstack was already resumed
            probeCoroutineResumed(current)
            with(current) {
                val completion = completion!! // fail fast when trying to resume continuation without completion
                val outcome: Result<Any?> =
                    try {
                        val outcome = invokeSuspend(param)
                        if (outcome === COROUTINE_SUSPENDED) return
                        Result.success(outcome)
                    } catch (exception: Throwable) {
                        Result.failure(exception)
                    }
                releaseIntercepted() // this state machine instance is terminating
                if (completion is BaseContinuationImpl) {
                    // unrolling recursion via loop
                    current = completion
                    param = outcome
                } else {
                    // top-level completion reached -- invoke and return
                    completion.resumeWith(outcome)
                    return
                }
            }
        }
    }

再看下当前调用堆栈  

 ![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b3c7d075470a431d99a36bb89e3f6fde~tplv-k3u1fbpfcp-watermark.image?)
     
    
main函数传递的context是EmptyCoroutineContext单利，Function就是新建的RunBlockingTestKt$main$1  Function2对象，

再看下调用堆栈  

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/544b5a3218e04a6194d9f46cb9bc1713~tplv-k3u1fbpfcp-watermark.image?)

coroutine.joinBlocking()调用之后会立刻执行EventLoopImplBase 的processNextEvent , 都在调用线程中执行【可能是main】，执行到delay的时候，会再次添加一个任务，这样processNextEvent中下次就会执行，甚至挂起，或者说阻塞，

## kotlin的delay原理 ：Delay只会挂起自己所处的协程

Delay 不会阻塞线程？个人感觉这个说法不完全正确，如果协程是在UI线程，那么UI线程其实也会阻塞的






	public final class RunBlockingTestKt {
	    public static final void main() {
	        Object unused = BuildersKt__BuildersKt.runBlocking$default(null, new RunBlockingTestKt$main$1(null), 1, null);
	    }
	}
 
    
## 将Block或者函数体抽象成对象



Kotlin协程不是线程，本质上是一个线程封装框架，但协程的实现还是离不开线程，协程最大的作用感觉是讲耗时任务的回调转变成了同步的调用，或者说：更加方便的写阻塞任务。

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
  通过对比其实可以看出：kotlin的协程在用法上，其实就是更好的Task封装，或者说少了一层，Java没法直接传递函数，只能用Runable封装一下。一个更有效的解释就是doAsync，类似一个Java跟kotlin之间的缓冲：
  
  
    doAsync {
        Log.e("TAG", " doAsync...   [当前线程为：${Thread.currentThread().name}]")
        uiThread {
            Log.e("TAG", " uiThread....   [当前线程为：${Thread.currentThread().name}]")
        }


### GlobalScope.launch 这种方式为什么有时候不会主线程不会阻塞等待完成

	fun main() {
	    GlobalScope.launch { // 在后台启动一个新的协程并继续
	        delay(1000L)
	        println("World!")
	    }
	    println("Hello,") // 主线程中的代码会立即执行
	}
	
main不会等待  GlobalScope.launch 的协程阻塞完成，为什么呢，因为他没有显式等待执行完毕条件 

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

对比之下你会发现，同runblock相比，缺少了coroutine.joinBlocking()，当然如若你需要等待，其实可以主动join。 





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
	    
	    <!--这个 block 函数定义为 CoroutineScope 的扩展函数，所以在代码块中可以直接访问 CoroutineScope 对象（也就是 this 对象）-->
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



###   Android的  lifecycleScope.launch  如果运行在main，好像是上来就运行


### CoroutineScope /CoroutineDispatcher [CoroutineContext] 


 
            CoroutineScope(Dispatchers.Main).launch(Dispatchers.Main) {
                Log.e("TAG", "1.执行CoroutineScope.... [当前线程为：${Thread.currentThread().name}]")
                delay(500)    
                Log.e("TAG", "2 .执行CoroutineScope.... [当前线程为：${Thread.currentThread().name}]")
            }

            lifecycleScope.launch {
                Log.e("TAG", "1.执行lifecycleScope.... [当前线程为：${Thread.currentThread().name}]")
                delay(500)     
                Thread.sleep(1000)
                Log.e("TAG", "2.执行lifecycleScope.... [当前线程为：${Thread.currentThread().name}]")
            }

            lifecycleScope.launch(Dispatchers.IO) {
                Log.e("TAG", "1.执行lifecycleScope Dispatchers.IO .... [当前线程为：${Thread.currentThread().name}]")
                delay(500)     
                Log.e("TAG", "2.执行lifecycleScope Dispatchers.IO.... [当前线程为：${Thread.currentThread().name}]")
            }
            
            Log.e("TAG", "3.BtnClick.... [当前线程为：${Thread.currentThread().name}]")

执行顺序  执行lifecycleScope.launch 中的非suspend ->后续UI线程-> CoroutineScope(Dispatchers.Main)或者其他区，When launch { ... } is used without parameters, it inherits the context (and thus dispatcher) from the CoroutineScope it is being launched from.

	public val Lifecycle.coroutineScope: LifecycleCoroutineScope
	    get() {
	        while (true) {
	            val existing = mInternalScopeRef.get() as LifecycleCoroutineScopeImpl?
	            if (existing != null) {
	                return existing
	            }
	            val newScope = LifecycleCoroutineScopeImpl(
	                this,
	                <!--plus被重载-->
	                SupervisorJob() + Dispatchers.Main.immediate
	            )
	            if (mInternalScopeRef.compareAndSet(null, newScope)) {
	                newScope.register()
	                return newScope
	            }
	        }
	    }
 

	
	internal object MainDispatcherLoader {
	
	    private val FAST_SERVICE_LOADER_ENABLED = systemProp(FAST_SERVICE_LOADER_PROPERTY_NAME, true)
	
	    @JvmField
	    val dispatcher: MainCoroutineDispatcher = loadMainDispatcher()
	
	    private fun loadMainDispatcher(): MainCoroutineDispatcher {
	        return try {
	            val factories = if (FAST_SERVICE_LOADER_ENABLED) {
	                FastServiceLoader.loadMainDispatcherFactory()
	            } else {
	                // We are explicitly using the
	                // `ServiceLoader.load(MyClass::class.java, MyClass::class.java.classLoader).iterator()`
	                // form of the ServiceLoader call to enable R8 optimization when compiled on Android.
	                ServiceLoader.load(
	                        MainDispatcherFactory::class.java,
	                        MainDispatcherFactory::class.java.classLoader
	                ).iterator().asSequence().toList()
	            }
	            @Suppress("ConstantConditionIf")
	            factories.maxByOrNull { it.loadPriority }?.tryCreateDispatcher(factories)
	                ?: createMissingDispatcher()
	        } catch (e: Throwable) {
	            // Service loader can throw an exception as well
	            createMissingDispatcher(e)
	        }
	    }
	}
	
### 协程任务+内部状态机[suspend点]+切换传递

回调与协程的阴性关系

	GlobalScope.launch(Dispatchers.Main) {
	    try {
	        //showUser 在 await 的 Continuation 的回调函数调用后执行
	        showUser(gitHubServiceApi.getUser("bennyhuo").await())
	    } catch (e: Exception) {
	        showError(e)
	    }
	}

	GlobalScope.launch(Dispatchers.Main) {
	    gitHubServiceApi.getUser("bennyhuo").await(object: Continuation<User>{
	            override fun resume(value: User) {
	                showUser(value)
	            }
	            override fun resumeWithException(exception: Throwable){
	                showError(exception)
	            }
	    })
	}

而在 await 当中，大致就是：

		//注意以下并不是真实的实现，仅供大家理解协程使用
	fun await(continuation: Continuation<User>): Any {
	    ... // 切到非 UI 线程中执行，等待结果返回
	    try {
	        val user = ...
	        handler.post{ continuation.resume(user) }
	    } catch(e: Exception) {
	        handler.post{ continuation.resumeWithException(e) }
	    }
	}
	
从执行机制上来讲，协程跟回调没有什么本质的区别，suspend 函数是 Kotlin 编译器对协程支持的唯一的黑魔法。





### CoroutineStart其实使用了invoke约定，可以直接()调用，等于invoke


    public fun <R> start(start: CoroutineStart, receiver: R, block: suspend R.() -> T) {
    	<!--CoroutineStart直接（）等于  invoke-->
        start(block, receiver, this)
    }

invoke 用的真是花哨，kotlin的东西太随意了

    @InternalCoroutinesApi
    public operator fun <R, T> invoke(block: suspend R.() -> T, receiver: R, completion: Continuation<T>): Unit =
        when (this) {
            DEFAULT -> block.startCoroutineCancellable(receiver, completion)
            ATOMIC -> block.startCoroutine(receiver, completion)
            UNDISPATCHED -> block.startCoroutineUndispatched(receiver, completion)
            LAZY -> Unit // will start lazily
        }
        
	        internal fun <R, T> (suspend (R) -> T).startCoroutineCancellable(
	    receiver: R, completion: Continuation<T>,
	    onCancellation: ((cause: Throwable) -> Unit)? = null
	) =
	    runSafely(completion) {
	        createCoroutineUnintercepted(receiver, completion).intercepted().resumeCancellableWith(Result.success(Unit), onCancellation)
	    }

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b3778b8371404c23a71a1de650450cf6~tplv-k3u1fbpfcp-watermark.image?)

	
	Suppress("NOTHING_TO_INLINE")
	inline fun resumeCancellableWith(
	    result: Result<T>,
	    noinline onCancellation: ((cause: Throwable) -> Unit)?
	) {
	    val state = result.toState(onCancellation)
	    ///dispatcher对应我们的前面 **4.2步骤** 分析得出的Dispatchers.Default+debug的对象
	    ///dispatcher.isDispatchNeeded(context)对象就是Dispatchers.Default的isDispatchNeeded方法，他没有实现过重写，所以还是用的基类CoroutineDispatcher中的isDispatchNeeded方法，默认是true
	    if (dispatcher.isDispatchNeeded(context)) {
	        _state = state
	        resumeMode = MODE_CANCELLABLE
	        ///这里的dispatcher，就是分发器DefaultScheduler对象
	        ///在 **4.2步骤** 中分析出了this 对象他就是我们的SuspendLambda对象，也就是我们的根示例
	        dispatcher.dispatch(context, this)
	    } else {
	        executeUnconfined(state, MODE_CANCELLABLE) {
	            if (!resumeCancelled(state)) {
	                resumeUndispatchedWith(result)
	            }
	        }
	    }
	}
	
	}
	 
	 
### 看看协程的转变


	@Metadata(
	   mv = {1, 5, 1},
	   k = 2,
	   d1 = {"\u0000\u0012\n\u0000\n\u0002\u0010\b\n\u0002\b\u0002\n\u0002\u0010\u0002\n\u0002\b\u0002\u001a\u0011\u0010\u0000\u001a\u00020\u0001H\u0086@ø\u0001\u0000¢\u0006\u0002\u0010\u0002\u001a\u0006\u0010\u0003\u001a\u00020\u0004\u001a\u0006\u0010\u0005\u001a\u00020\u0004\u0082\u0002\u0004\n\u0002\b\u0019¨\u0006\u0006"},
	   d2 = {"doSomethingUsefulOne", "", "(Lkotlin/coroutines/Continuation;)Ljava/lang/Object;", "kotlinCoroutines", "", "main", "app_debug"}
	)
	public final class CoroutinesTestKt {
	   public static final void main() {
	      kotlinCoroutines();
	      String var0 = "主线程在进行计算";
	      System.out.println(var0);
	   }
	
	   // $FF: synthetic method
	   public static void main(String[] var0) {
	      main();
	   }
	
	   public static final void kotlinCoroutines() {
	      BuildersKt.launch$default(CoroutineScopeKt.CoroutineScope((CoroutineContext)Dispatchers.getDefault()), (CoroutineContext)null, (CoroutineStart)null, (Function2)(new Function2((Continuation)null) {
	         int I$0;
	         int label;
	
	         @Nullable
	         public final Object invokeSuspend(@NotNull Object $result) {
	            Object var10000;
	            int one;
	            label17: {
	               Object var5 = IntrinsicsKt.getCOROUTINE_SUSPENDED();
	               switch (this.label) {
	                  case 0:
	                     ResultKt.throwOnFailure($result);
	                     this.label = 1;
	                     var10000 = CoroutinesTestKt.doSomethingUsefulOne(this);
	                     if (var10000 == var5) {
	                        return var5;
	                     }
	                     break;
	                  case 1:
	                     ResultKt.throwOnFailure($result);
	                     var10000 = $result;
	                     break;
	                  case 2:
	                     one = this.I$0;
	                     ResultKt.throwOnFailure($result);
	                     var10000 = $result;
	                     break label17;
	                  default:
	                     throw new IllegalStateException("call to 'resume' before 'invoke' with coroutine");
	               }
	
	               one = ((Number)var10000).intValue();
	               this.I$0 = one;
	               this.label = 2;
	               var10000 = CoroutinesTestKt.doSomethingUsefulOne(this);
	               if (var10000 == var5) {
	                  return var5;
	               }
	            }
	
	            int two = ((Number)var10000).intValue();
	            String var4 = "The answer is " + (one + two);
	            System.out.println(var4);
	            return Unit.INSTANCE;
	         }
	
	         @NotNull
	         public final Continuation create(@Nullable Object value, @NotNull Continuation completion) {
	            Intrinsics.checkNotNullParameter(completion, "completion");
	            Function2 var3 = new <anonymous constructor>(completion);
	            return var3;
	         }
	
	         public final Object invoke(Object var1, Object var2) {
	            return ((<undefinedtype>)this.create(var1, (Continuation)var2)).invokeSuspend(Unit.INSTANCE);
	         }
	      }), 3, (Object)null);
	   }
	
	   @Nullable
	   public static final Object doSomethingUsefulOne(@NotNull Continuation var0) {
	      Object $continuation;
	      label20: {
	         if (var0 instanceof <undefinedtype>) {
	            $continuation = (<undefinedtype>)var0;
	            if ((((<undefinedtype>)$continuation).label & Integer.MIN_VALUE) != 0) {
	               ((<undefinedtype>)$continuation).label -= Integer.MIN_VALUE;
	               break label20;
	            }
	         }
	
	         $continuation = new ContinuationImpl(var0) {
	            // $FF: synthetic field
	            Object result;
	            int label;
	
	            @Nullable
	            public final Object invokeSuspend(@NotNull Object $result) {
	               this.result = $result;
	               this.label |= Integer.MIN_VALUE;
	               return CoroutinesTestKt.doSomethingUsefulOne(this);
	            }
	         };
	      }
	
	      Object $result = ((<undefinedtype>)$continuation).result;
	      Object var3 = IntrinsicsKt.getCOROUTINE_SUSPENDED();
	      switch (((<undefinedtype>)$continuation).label) {
	         case 0:
	            ResultKt.throwOnFailure($result);
	            ((<undefinedtype>)$continuation).label = 1;
	            if (DelayKt.delay(1000L, (Continuation)$continuation) == var3) {
	               return var3;
	            }
	            break;
	         case 1:
	            ResultKt.throwOnFailure($result);
	            break;
	         default:
	            throw new IllegalStateException("call to 'resume' before 'invoke' with coroutine");
	      }
	
	      return Boxing.boxInt(13);
	   }
	}

BuildersKt.launch$default可以在把BuildersKt反编译成Java文件之后找到 

		public static Job launch$default(CoroutineScope var0, CoroutineContext var1, CoroutineStart var2, Function2 var3, int var4, Object var5) {
		      return BuildersKt__Builders_commonKt.launch$default(var0, var1, var2, var3, var4, var5);
		   }

jadx可以直观看到java代码
	 
		public final /* synthetic */ class BuildersKt__Builders_commonKt {
	    private static final int RESUMED;
	    private static final int SUSPENDED;
	    private static final int UNDECIDED;
	
	    public static /* synthetic */ Job launch$default(CoroutineScope coroutineScope, CoroutineContext coroutineContext, CoroutineStart coroutineStart, Function2 function2, int i, Object obj) {
	        if ((i & 1) != 0) {
	            coroutineContext = EmptyCoroutineContext.INSTANCE;
	        }
	        if ((i & 2) != 0) {
	            coroutineStart = CoroutineStart.DEFAULT;
	        }
	        return BuildersKt.launch(coroutineScope, coroutineContext, coroutineStart, function2);
	    }
	    
	        public static final Job launch(CoroutineScope $this$launch, CoroutineContext context, CoroutineStart start, Function2<? super CoroutineScope, ? super Continuation<? super Unit>, ? extends Object> function2) {
        return BuildersKt__Builders_commonKt.launch($this$launch, context, start, function2);
    }
    
    
	        public static final Job launch(CoroutineScope $this$launch, CoroutineContext context, CoroutineStart start, Function2<? super CoroutineScope, ? super Continuation<? super Unit>, ? extends Object> function2) {
        LazyStandaloneCoroutine coroutine;
        CoroutineContext newContext = CoroutineContextKt.newCoroutineContext($this$launch, context);
        if (start.isLazy()) {
            coroutine = new LazyStandaloneCoroutine(newContext, function2);
        } else {
            coroutine = new StandaloneCoroutine(newContext, true);
        }
        coroutine.start(start, coroutine, function2);
        return coroutine;
    }
    
其实最终还是走 coroutine.start(start, coroutine, function2);不过这里最重要的一点是Function2在变异工程中的自动化封装，有了这个对象，回调才被屏蔽干净。

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8db266fe57804c19b4d541ab7b78774d~tplv-k3u1fbpfcp-watermark.image?)
			
其实在jadx里可以直观感受到，kotlin的所有都是在java的框架里玩耍，只不过用了很多手段进行背后的封装。suspend 函数都会被抽象位类对象				 
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ed2a43448b844dfda3a0b3feb78e4db9~tplv-k3u1fbpfcp-watermark.image?)







	
	public static final java.lang.Object doSomethingUsefulOne(kotlin.coroutines.Continuation<? super java.lang.Integer> r5) {
	        /*
	            boolean r0 = r5 instanceof com.snail.androidarch.kt.CoroutinesTestKt$doSomethingUsefulOne$1
	            if (r0 == 0) goto L_0x0014
	            r0 = r5
	            com.snail.androidarch.kt.CoroutinesTestKt$doSomethingUsefulOne$1 r0 = (com.snail.androidarch.kt.CoroutinesTestKt$doSomethingUsefulOne$1) r0
	            int r1 = r0.label
	            r2 = -2147483648(0xffffffff80000000, float:-0.0)
	            r1 = r1 & r2
	            if (r1 == 0) goto L_0x0014
	            int r5 = r0.label
	            int r5 = r5 - r2
	            r0.label = r5
	            goto L_0x0019
	        L_0x0014:
	            com.snail.androidarch.kt.CoroutinesTestKt$doSomethingUsefulOne$1 r0 = new com.snail.androidarch.kt.CoroutinesTestKt$doSomethingUsefulOne$1
	            r0.<init>(r5)
	        L_0x0019:
	            r5 = r0
	            java.lang.Object r0 = r5.result
	            java.lang.Object r1 = kotlin.coroutines.intrinsics.IntrinsicsKt.getCOROUTINE_SUSPENDED()
	            int r2 = r5.label
	            switch(r2) {
	                case 0: goto L_0x0031;
	                case 1: goto L_0x002d;
	                default: goto L_0x0025;
	            }
	        L_0x0025:
	            java.lang.IllegalStateException r5 = new java.lang.IllegalStateException
	            java.lang.String r0 = "call to 'resume' before 'invoke' with coroutine"
	            r5.<init>(r0)
	            throw r5
	        L_0x002d:
	            kotlin.ResultKt.throwOnFailure(r0)
	            goto L_0x0044
	        L_0x0031:
	            kotlin.ResultKt.throwOnFailure(r0)
	            com.snail.androidarch.kt.LiveLiterals$CoroutinesTestKt r2 = com.snail.androidarch.kt.LiveLiterals$CoroutinesTestKt.INSTANCE
	            long r2 = r2.m4378Long$arg0$calldelay$fundoSomethingUsefulOne()
	            r4 = 1
	            r5.label = r4
	            java.lang.Object r2 = kotlinx.coroutines.DelayKt.delay(r2, r5)
	            if (r2 != r1) goto L_0x0044
	            return r1
	        L_0x0044:
	            com.snail.androidarch.kt.LiveLiterals$CoroutinesTestKt r1 = com.snail.androidarch.kt.LiveLiterals$CoroutinesTestKt.INSTANCE
	            int r1 = r1.m4376Int$fundoSomethingUsefulOne()
	            java.lang.Integer r1 = kotlin.coroutines.jvm.internal.Boxing.boxInt(r1)
	            return r1
	            switch-data {0->0x0031, 1->0x002d, }
	        */
	        throw new UnsupportedOperationException("Method not decompiled: com.snail.androidarch.kt.CoroutinesTestKt.doSomethingUsefulOne(kotlin.coroutines.Continuation):java.lang.Object");
	    }
	    
	public final class CoroutinesTestKt$doSomethingUsefulOne$1 extends ContinuationImpl {
	    int label;
	    /* synthetic */ Object result;
	
	    CoroutinesTestKt$doSomethingUsefulOne$1(Continuation<? super CoroutinesTestKt$doSomethingUsefulOne$1> continuation) {
	        super(continuation);
	    }
	
	    @Override // kotlin.coroutines.jvm.internal.BaseContinuationImpl
	    public final Object invokeSuspend(Object obj) {
	        this.result = obj;
	        this.label |= Integer.MIN_VALUE;
	        <!---->
	        return CoroutinesTestKt.doSomethingUsefulOne(this);
	    }
	}
	
suspend函数被抽象成静态函数 + ContinuationImpl对象，所有的ContinuationImpl对象的模板都一致，**通过invokeSuspend调用自己**，达到状态机执行的目的。状态机通过label+break跳转实现层层剥离，这里都是通过label的层次来搞的



### Dispatchers.Default的实现：看起来像是一个线程池，或者说是个执行器

    public actual val Default: CoroutineDispatcher = DefaultScheduler
    ↓
    internal object DefaultScheduler : SchedulerCoroutineDispatcher(
    ↓
    internal open class SchedulerCoroutineDispatcher(
    <!--线程池参数-->
	    private val corePoolSize: Int = CORE_POOL_SIZE,
	    private val maxPoolSize: Int = MAX_POOL_SIZE,
	    private val idleWorkerKeepAliveNs: Long = IDLE_WORKER_KEEP_ALIVE_NS,
	    private val schedulerName: String = "CoroutineScheduler",
	) : ExecutorCoroutineDispatcher(){
	
	    override val executor: Executor
        get() = coroutineScheduler
        
          private var coroutineScheduler = createScheduler()
          	<!--CoroutineScheduler多像一个线程池-->
              private fun createScheduler() =
      		  CoroutineScheduler(corePoolSize, maxPoolSize, idleWorkerKeepAliveNs, schedulerName)

	}
    ↓
	public abstract class ExecutorCoroutineDispatcher: CoroutineDispatcher(), Closeable {
	
	    public abstract val executor: Executor
 
### Dispatchers.Main的实现：它背后不是线程池

已经存在一个Loop线程，依靠上去就可以了，lifecycleScope都是直接在里面调用的


 
###  LifecycleOwner.lifecycleScope: LifecycleCoroutineScope的实现

LifecycleOwner.lifecycleScope是个扩展属性，只有在LifecycleOwner中才能用

    get() = lifecycle.coroutineScope
    
	public val Lifecycle.coroutineScope: LifecycleCoroutineScope
	    get() {
	        while (true) {
	            val existing = mInternalScopeRef.get() as LifecycleCoroutineScopeImpl?
	            if (existing != null) {
	                return existing
	            }
	            val newScope = LifecycleCoroutineScopeImpl(
	                this,
	                SupervisorJob() + Dispatchers.Main.immediate
	            )
	            if (mInternalScopeRef.compareAndSet(null, newScope)) {
	                newScope.register()
	                return newScope
	            }
	        }
	    }
	    
LifecycleCoroutineScopeImpl采用的CoroutineContext是SupervisorJob() + Dispatchers.Main.immediate 

	   public actual val Main: MainCoroutineDispatcher get() = MainDispatcherLoader.dispatcher
	   
	    @JvmField
    val dispatcher: MainCoroutineDispatcher = loadMainDispatcher()

    private fun loadMainDispatcher(): MainCoroutineDispatcher {
        return try {
            val factories = if (FAST_SERVICE_LOADER_ENABLED) {
                FastServiceLoader.loadMainDispatcherFactory()
            } else {
                  ServiceLoader.load(
                        MainDispatcherFactory::class.java,
                        MainDispatcherFactory::class.java.classLoader
                ).iterator().asSequence().toList()
            }
            @Suppress("ConstantConditionIf")
            factories.maxByOrNull { it.loadPriority }?.tryCreateDispatcher(factories)
                ?: createMissingDispatcher()
        } catch (e: Throwable) {
            // Service loader can throw an exception as well
            createMissingDispatcher(e)
        }
    }	   
    
Android平台
    
    internal class AndroidDispatcherFactory : MainDispatcherFactory {

    override fun createDispatcher(allFactories: List<MainDispatcherFactory>): MainCoroutineDispatcher {
        val mainLooper = Looper.getMainLooper() ?: throw IllegalStateException("The main looper is not available")
        return HandlerContext(mainLooper.asHandler(async = true))
    }

    override fun hintOnError(): String = "For tests Dispatchers.setMain from kotlinx-coroutines-test module can be used"

    override val loadPriority: Int
        get() = Int.MAX_VALUE / 2
	}


最终 内含Handler的HandlerContext构建成功

	internal class HandlerContext private constructor(
	    private val handler: Handler,
	    private val name: String?,
	    private val invokeImmediately: Boolean
	) : HandlerDispatcher(), Delay {
	    /**
	     * Creates [CoroutineDispatcher] for the given Android [handler].
	     *
	     * @param handler a handler.
	     * @param name an optional name for debugging.
	     */
	    constructor(
	        handler: Handler,
	        name: String? = null
	    ) : this(handler, name, false)
	
	    @Volatile
	    private var _immediate: HandlerContext? = if (invokeImmediately) this else null
	 