Android 消息机制学习
作者：许璐 2016-04-29 09:45
Android消息机制大家都不陌生，想必大家也都看过Handler、Looper的源码（看过可以直接看末尾重点，一款监控APP卡顿情况的控件），下面就整合一下这方面的资料，加深对这方面的印象。

用法

private Handler mHandler = new Handler() {
    @Override
    public void handleMessage(Message msg) {
        switch (msg.what) {
            case MESSAGE_TEXT_VIEW:
                mTextView.setText("UI成功更新");
            default:
                super.handleMessage(msg);
        }
    }
};


new Thread(new Runnable() {
        @Override
        public void run() {
            try {
                Thread.sleep(3000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
              Message message = new Message();   
            message.what = MESSAGE_TEXT_VIEW;     
            mHandler.sendMessage(message); 
        }
    }).start();
Handler 机制架构



从上图可以看到，是围绕 Handler、Message、MessageQueue 和 Looper 进行的。先介绍相关的概念

从开发角度看， Handler 是 Android 消息系统机制的上层接口，这使得在开发过程中只需要和 Handler 交互即可。另外， Handler 并不是专门用来更新 UI 的，只是经常被开发者用来更新 UI 而已，但是不能忽略它的其他功能，例如进行耗时的 I/O 操作等。

疑问：为什么子线程不能更新 UI？

这是因为 ViewRootImpl 对 UI 操作进行了验证

void checkThread() {
   if (mThread != Thread.currentThread()) {//Thread.currentThread()是UI主线程
       throw new CalledFromWrongThreadException(
               "Only the original thread that created a view hierarchy can touch its views.");
   }
另外， Android 的 UI 空间不是线程安全的，如果在多线程中并发访问可能会导致 UI 控件处于不可预期的状态。

疑问：为什么不对 UI 控件的访问加上锁机制呢？

这是因为加锁，会导致 UI 访问的逻辑变复杂；其次，锁机制会降低 UI 访问的效率。

这也是为啥会存在 Hanlder 的原因。

MessageQueue：消息队列，顾名思义，它的内部存储了一组消息，以队列的形式对外提供插入和删除的工作。但是其内部是采用单链表来存储消息列表。

Looper：循环（消息循环），以无限循环的形式去查找是否有新消息，有则处理，无则等待。

Handler 源码分析及其原理

Handler 的构造方法

Handler 的构造方法有很多，核心的构造方法如下

/**
 * Use the {@link Looper} for the current thread with the specified callback interface
 * and set whether the handler should be asynchronous.
 *
 * Handlers are synchronous by default unless this constructor is used to make
 * one that is strictly asynchronous.
 *
 * Asynchronous messages represent interrupts or events that do not require global ordering
 * with respect to synchronous messages.  Asynchronous messages are not subject to
 * the synchronization barriers introduced by {@link MessageQueue#enqueueSyncBarrier(long)}.
 *
 * @param callback The callback interface in which to handle messages, or null.
 * @param async If true, the handler calls {@link Message#setAsynchronous(boolean)} for
 * each {@link Message} that is sent to it or {@link Runnable} that is posted to it.
 *
 * @hide
 */
public Handler(Callback callback, boolean async) {
    if (FIND_POTENTIAL_LEAKS) {//默认是false，若为true，则会检测当前handler是否是静态类
        final Class<? extends Handler> klass = getClass();
        if ((klass.isAnonymousClass() || klass.isMemberClass() || klass.isLocalClass()) &&
                (klass.getModifiers() & Modifier.STATIC) == 0) {
            Log.w(TAG, "The following Handler class should be static or leaks might occur: " +
                klass.getCanonicalName());
        }
    }

    mLooper = Looper.myLooper();//获得了 Looper 对象
    if (mLooper == null) {//如果是工作线程，就为空
        throw new RuntimeException(
            "Can't create handler inside thread that has not called Looper.prepare()");//不能在未调用 Looper.prepare() 的线程创建 handler
    }
    mQueue = mLooper.mQueue;//mLooper对应的消息队列
    mCallback = callback;
    mAsynchronous = async;
}
一个构造方法，Android 消息机制的三个重要角色全部出现了，分别是 Handler 、Looper 以及 MessageQueue。

mLooper = Looper.myLooper();//获得了 Looper 对象
下面看看 Looper.myLooper() 方法是嘛

/**
 * Return the Looper object associated with the current thread.  Returns
 * null if the calling thread is not associated with a Looper.
 */
public static @Nullable Looper myLooper() {
    return sThreadLocal.get();
}
sThreadLocal 是个嘛

 // sThreadLocal.get() will return null unless you've called prepare().
static final ThreadLocal<Looper> sThreadLocal = new ThreadLocal<Looper>();
好温馨的提示，定义在 Looper 中，是一个 static final 类型的 ThreadLocal 对象（在 Java 中，一般情况下，通过 ThreadLocal.set() 到线程中的对象是该线程自己使用的对象，其他线程是不需要访问的，也访问不到的，各个线程中访问的是不同的对象。）至于 ThreadLocal 是个嘛，参考这里

大概说一下， ThreadLocal 是一个线程内部的数据存储类，通过它可以在指定的线程中存储数据，数据存储以后，只有在指定线程中可以获得存储的数据，对于其他线程来说则无法获取到数据。

对于 Handler 来说，它需要获取当前线程的 Looper，很显然，Looper 的作用域就是线程并且不同线程具有不同的 Looper，这个时候通过 ThreadLocal 就可以轻松实现 Looper 在线程中的存取。

根据提示，看看 prepare() 方法

/** Initialize the current thread as a looper.
  * This gives you a chance to create handlers that then reference
  * this looper, before actually starting the loop. Be sure to call
  * {@link #loop()} after calling this method, and end it by calling
  * {@link #quit()}.
  */
public static void prepare() {
    prepare(true);
}

private static void prepare(boolean quitAllowed) {
    if (sThreadLocal.get() != null) {//一个线程只会有一个 Looper
        throw new RuntimeException("Only one Looper may be created per thread");
    }
    sThreadLocal.set(new Looper(quitAllowed));
}
这段代码首先判断 sThreadLocal 中是否已经存在 Looper 了，如果还没有则创建一个新的 Looper 设置进去。下面看看 Looper 的构造方法

private Looper(boolean quitAllowed) {
    mQueue = new MessageQueue(quitAllowed);
    mThread = Thread.currentThread();
}
构造方法可以看出，创建了一个 MessageQueue，传入参数值为 true （子线程默认是true，why？后面有讲到）；创建了一个当前 thread 的实例引用。很明显，one looper only one MessageQueue

到此，就有一个疑问了：在 UI Thread 中创建 Handler 时没有调用 Looper.prepare()，但是却能正常运行（但是，我们注意到，sThreadLocal.get() will return null unless you've called prepare()），Why？

既然能正常运行，那么肯定是调用了 prepare 方法，但是，在哪里调用了呢，这就要看主线程 ActivityThread 。首次启动 Activity 时通过 Process.start 创建应用层程序的主线程，创建成功后进入到主线程 ActivityThread 的 main 方法中开始执行， main 方法有：

    Looper.prepareMainLooper();

    ActivityThread thread = new ActivityThread();
    thread.attach(false);

    if (sMainThreadHandler == null) {
        sMainThreadHandler = thread.getHandler();
    }

    if (false) {
        Looper.myLooper().setMessageLogging(new
                LogPrinter(Log.DEBUG, "ActivityThread"));
    }

    Looper.loop();
很明显咯，秘密就在 prepareMainLooper() 里面（即使后面加了个 MainLooper，但也是个 prepare）

/**
 * Initialize the current thread as a looper, marking it as an
 * application's main looper. The main looper for your application
 * is created by the Android environment, so you should never need
 * to call this function yourself.  See also: {@link #prepare()}
 */
public static void prepareMainLooper() {
    prepare(false);//可以看出，UI thread传入的是false
    synchronized (Looper.class) {
        if (sMainLooper != null) {
            throw new IllegalStateException("The main Looper has already been prepared.");
        }
        sMainLooper = myLooper();
    }
}
UI 线程中会始终存在一个 Looper 对象（ sMainLooper 保存在 Looper 类中， UI 线程通过getMainLooper 方法获取 UI 线程的 Looper 对象），从而不需要再手动去调用 Looper.prepare() 方法了。如下 Looper 类提供的 get 方法：

/**
 * Returns the application's main looper, which lives in the main thread of the application.
 */
public static Looper getMainLooper() {
    synchronized (Looper.class) {
        return sMainLooper;
    }
}
到这里，上面疑问的答案就显而易见了。同时，如果在子线程实例化 Handler，就必须要先调用Looper.prepare() 方法才可以。

到此先初步总结下上面关于 Handler 实例化的一些关键信息，具体如下：

在主线程中可以直接创建 Handler 对象，而在子线程中需要先调用 Looper.prepare() 才能创建 Handler 对象，否则运行抛出 ”Can’t create handler inside thread that has not called Looper.prepare()” 异常信息。

每个线程中最多只能有一个 Looper 对象，否则抛出异常。

可以通过 Looper.myLooper() 获取当前线程的 Looper 实例，通过 Looper.getMainLooper() 获取主（UI）线程的 Looper 实例。

一个 Looper 只能对应了一个M essageQueue 。

一个线程中只有一个 Looper 实例，一个 MessageQueue 实例，可以有多个 Handler 实例。

Handler 对象也创建好了，接下来就该发送消息了 mHandler.sendMessage(message);

/**
 * Pushes a message onto the end of the message queue after all pending messages
 * before the current time. It will be received in {@link #handleMessage},
 * in the thread attached to this handler.
 *  
 * @return Returns true if the message was successfully placed in to the 
 *         message queue.  Returns false on failure, usually because the
 *         looper processing the message queue is exiting.
 */
public final boolean sendMessage(Message msg)
{
    return sendMessageDelayed(msg, 0);
}
嗯，继续往下看咯

/**
 * Enqueue a message into the message queue after all pending messages
 * before (current time + delayMillis). You will receive it in
 * {@link #handleMessage}, in the thread attached to this handler.
 *  
 * @return Returns true if the message was successfully placed in to the 
 *         message queue.  Returns false on failure, usually because the
 *         looper processing the message queue is exiting.  Note that a
 *         result of true does not mean the message will be processed -- if
 *         the looper is quit before the delivery time of the message
 *         occurs then the message will be dropped.
 */
public final boolean sendMessageDelayed(Message msg, long delayMillis)
{
    if (delayMillis < 0) {
        delayMillis = 0;
    }
    return sendMessageAtTime(msg, SystemClock.uptimeMillis() + delayMillis);
}
最终走到了 sendMessageAtTime 这个方法

/**
 * Enqueue a message into the message queue after all pending messages
 * before the absolute time (in milliseconds) <var>uptimeMillis</var>.
 * <b>The time-base is {@link android.os.SystemClock#uptimeMillis}.</b>
 * Time spent in deep sleep will add an additional delay to execution.
 * You will receive it in {@link #handleMessage}, in the thread attached
 * to this handler.
 * 
 * @param uptimeMillis The absolute time at which the message should be
 *         delivered, using the
 *         {@link android.os.SystemClock#uptimeMillis} time-base.
 *         
 * @return Returns true if the message was successfully placed in to the 
 *         message queue.  Returns false on failure, usually because the
 *         looper processing the message queue is exiting.  Note that a
 *         result of true does not mean the message will be processed -- if
 *         the looper is quit before the delivery time of the message
 *         occurs then the message will be dropped.
 */
public boolean sendMessageAtTime(Message msg, long uptimeMillis) {
    MessageQueue queue = mQueue;//mQueue是在Handler实例化时构造函数中实例化的
    if (queue == null) {
        RuntimeException e = new RuntimeException(
                this + " sendMessageAtTime() called with no mQueue");
        Log.w("Looper", e.getMessage(), e);
        return false;
    }
    return enqueueMessage(queue, msg, uptimeMillis);
}
sendMessageAtTime() 方法接收两个参数，其中 msg 参数就是我们发送的 Message 对象，而uptimeMillis 参数则表示发送消息的时间，它的值等于自系统开机到当前时间的毫秒数再加上延迟时间，如果调用的不是 sendMessageDelayed() 方法，延迟时间就为0。

而 mQueue 是在 Handler 实例化时构造函数中实例化的，在 Handler 的构造函数中可以看见 mQueue = mLooper.mQueue ;而 Looper 的 mQueue 对象上面分析过了，是在 Looper 的构造函数中创建的一个MessageQueue。

最终的 MessageQueue 的 enqueueMessage() 方法是个嘛，下面看看

private boolean enqueueMessage(MessageQueue queue, Message msg, long uptimeMillis) {
    msg.target = this;
    if (mAsynchronous) {
        msg.setAsynchronous(true);
    }
    return queue.enqueueMessage(msg, uptimeMillis);
}
这个方法首先将我们要发送的消息 Message 的 target 属性设置为当前 Handler 对象（进行关联）；接着将 msg 与 uptimeMillis 这两个参数都传递到 MessageQueue （消息队列）的 enqueueMessage() 方法中，如下

boolean enqueueMessage(Message msg, long when) {
    if (msg.target == null) {//上面的target已经是handler对象，not null
        throw new IllegalArgumentException("Message must have a target.");
    }
    if (msg.isInUse()) {
        throw new IllegalStateException(msg + " This message is already in use.");
    }

    synchronized (this) {
        if (mQuitting) {
            IllegalStateException e = new IllegalStateException(
                    msg.target + " sending message to a Handler on a dead thread");
            Log.w(TAG, e.getMessage(), e);
            msg.recycle();
            return false;
        }

        msg.markInUse();//设置当前msg的状态
        msg.when = when;
        Message p = mMessages;
        boolean needWake;
        if (p == null || when == 0 || when < p.when) {//检测当前头指针是否为空（队列为空）或者没有设置when 或者设置的when比头指针的when要前
            // New head, wake up the event queue if blocked.
            msg.next = p;
            mMessages = msg;
            needWake = mBlocked;
        } else {
            // Inserted within the middle of the queue.  Usually we don't have to wake
            // up the event queue unless there is a barrier at the head of the queue
            // and the message is the earliest asynchronous message in the queue.
            //几种情况要唤醒线程处理消息：1）队列是堵塞的 2)barrier，头部结点无target 3）当前msg是异步的
            needWake = mBlocked && p.target == null && msg.isAsynchronous();
            Message prev;
            for (;;) {
                prev = p;
                p = p.next;
                if (p == null || when < p.when) {
                    break;
                }
                if (needWake && p.isAsynchronous()) {
                    needWake = false;
                }
            }
            msg.next = p; // invariant: p == prev.next 将当前msg插入第一个比其when值大的结点前。
            prev.next = msg;
        }

        // We can assume mPtr != 0 because mQuitting is false.
        if (needWake) {
            nativeWake(mPtr);
        }
    }
    return true;
}
MessageQueue 消息队列对于消息排队是通过类似 C 语言的链表来存储这些有序的消息的。其中的 mMessages 对象表示当前待处理的消息；消息插入队列的实质就是将所有的消息按时间（ uptimeMillis 参数，也就是 when ）进行排序。具体的操作方法就根据时间的顺序调用 msg.next ，从而为每一个消息指定它的下一个消息是什么。

当然如果你是通过 sendMessageAtFrontOfQueue() 方法来发送消息的，它也会调用 enqueueMessage() 来让消息入队，只不过时间为0，这时会把 mMessages 赋值为新入队的这条消息，然后将这条消息的 next 指定为刚才的 mMessages ，这样也就完成了添加消息到队列头部的操作。

到此，消息也通过 handler 发送了，并且存到了 MessageQueue 中，那么，系统怎么处理 message 呢？

我们知道 MessageQueue 的对象在 Looper 构造函数中实例化的；一个 Looper 对应一个 MessageQueue，所以说 Handler 发送消息是通过 Handler 构造函数里拿到的 Looper 对象的成员 MessageQueue 的enqueueMessage 方法将消息插入队列，也就是说出队列一定也与 Handler 和 Looper 和 MessageQueue有关系。

既然会涉及到出队，那么肯定就有出队的方法，那么找来找去，就在 loop() 方法里面（为啥 UI Thread 没有调用 loop，loop 也会执行呢？一看就没有好好的看刚才贴的代码，明明已经调用，却说没有调用

Looper.prepareMainLooper(); 
...
Looper.loop();   
下回看代码用点心 ）

/**
 * Run the message queue in this thread. Be sure to call
 * {@link #quit()} to end the loop.
 */
public static void loop() {
    final Looper me = myLooper();
    if (me == null) {
        throw new RuntimeException("No Looper; Looper.prepare() wasn't called on this thread.");
    }
    final MessageQueue queue = me.mQueue;

    // Make sure the identity of this thread is that of the local process,
    // and keep track of what that identity token actually is.
    Binder.clearCallingIdentity();
    final long ident = Binder.clearCallingIdentity();

    for (;;) {
        Message msg = queue.next(); // might block
        if (msg == null) {
            // No message indicates that the message queue is quitting.
            return;
        }

        // This must be in a local variable, in case a UI event sets the logger
        Printer logging = me.mLogging;
        if (logging != null) {
            logging.println(">>>>> Dispatching to " + msg.target + " " +
                    msg.callback + ": " + msg.what);
        }

        msg.target.dispatchMessage(msg);

        if (logging != null) {
            logging.println("<<<<< Finished to " + msg.target + " " + msg.callback);
        }

        // Make sure that during the course of dispatching the
        // identity of the thread wasn't corrupted.
        final long newIdent = Binder.clearCallingIdentity();
        if (ident != newIdent) {
            Log.wtf(TAG, "Thread identity changed from 0x"
                    + Long.toHexString(ident) + " to 0x"
                    + Long.toHexString(newIdent) + " while dispatching to "
                    + msg.target.getClass().getName() + " "
                    + msg.callback + " what=" + msg.what);
        }

        msg.recycleUnchecked();
    }
}
可以看到 for (;;) {} 就是一个死循环，然后不断的调用 next 方法（出队的方法）

Message next() {
    // Return here if the message loop has already quit and been disposed.
    // This can happen if the application tries to restart a looper after quit
    // which is not supported.
    final long ptr = mPtr;
    if (ptr == 0) {
        return null;
    }

    int pendingIdleHandlerCount = -1; // -1 only during first iteration
    int nextPollTimeoutMillis = 0;
    for (;;) {
        if (nextPollTimeoutMillis != 0) {
            Binder.flushPendingCommands();
        }

        nativePollOnce(ptr, nextPollTimeoutMillis);

        synchronized (this) {
            // Try to retrieve the next message.  Return if found.
            final long now = SystemClock.uptimeMillis();
            Message prevMsg = null;
            Message msg = mMessages;
            if (msg != null && msg.target == null) {
                // Stalled by a barrier.  Find the next asynchronous message in the queue.
                do {
                    prevMsg = msg;
                    msg = msg.next;
                } while (msg != null && !msg.isAsynchronous());
            }
            if (msg != null) {
                if (now < msg.when) {
                    // Next message is not ready.  Set a timeout to wake up when it is ready.
                    nextPollTimeoutMillis = (int) Math.min(msg.when - now, Integer.MAX_VALUE);
                } else {
                    // Got a message.
                    mBlocked = false;
                    if (prevMsg != null) {
                        prevMsg.next = msg.next;
                    } else {
                        mMessages = msg.next;
                    }
                    msg.next = null;
                    if (DEBUG) Log.v(TAG, "Returning message: " + msg);
                    msg.markInUse();
                    return msg;
                }
            } else {
                // No more messages.
                nextPollTimeoutMillis = -1;
            }

            // Process the quit message now that all pending messages have been handled.
            if (mQuitting) {
                dispose();
                return null;
            }

            // If first time idle, then get the number of idlers to run.
            // Idle handles only run if the queue is empty or if the first message
            // in the queue (possibly a barrier) is due to be handled in the future.
            if (pendingIdleHandlerCount < 0
                    && (mMessages == null || now < mMessages.when)) {
                pendingIdleHandlerCount = mIdleHandlers.size();
            }
            if (pendingIdleHandlerCount <= 0) {
                // No idle handlers to run.  Loop and wait some more.
                mBlocked = true;
                continue;
            }

            if (mPendingIdleHandlers == null) {
                mPendingIdleHandlers = new IdleHandler[Math.max(pendingIdleHandlerCount, 4)];
            }
            mPendingIdleHandlers = mIdleHandlers.toArray(mPendingIdleHandlers);
        }

        // Run the idle handlers.
        // We only ever reach this code block during the first iteration.
        for (int i = 0; i < pendingIdleHandlerCount; i++) {
            final IdleHandler idler = mPendingIdleHandlers[i];
            mPendingIdleHandlers[i] = null; // release the reference to the handler

            boolean keep = false;
            try {
                keep = idler.queueIdle();
            } catch (Throwable t) {
                Log.wtf(TAG, "IdleHandler threw exception", t);
            }

            if (!keep) {
                synchronized (this) {
                    mIdleHandlers.remove(idler);
                }
            }
        }

        // Reset the idle handler count to 0 so we do not run them again.
        pendingIdleHandlerCount = 0;

        // While calling an idle handler, a new message could have been delivered
        // so go back and look again for a pending message without waiting.
        nextPollTimeoutMillis = 0;
    }
}
同样，可以看到 for (;;) {}，一个死循环

它的简单逻辑就是如果当前 MessageQueue 中存在 mMessages (即待处理消息)，就将这个消息出队，然后让下一条消息成为 mMessages，否则就进入一个阻塞状态，一直等到有新的消息入队。

这里有个疑问，相信网友的回答应该能回答这个问题了。

另外，可以参考这里，里面有对 epoll_wait 的介绍。

继续 loop 方法，每当有一个消息出队就将它传递到 msg.target 的 dispatchMessage() 方法中。其中这个 msg.target 其实就是当前 Handler 对象

/**
 * Handle system messages here.
 */
public void dispatchMessage(Message msg) {
    if (msg.callback != null) {
        handleCallback(msg);
    } else {
        if (mCallback != null) {
            if (mCallback.handleMessage(msg)) {
                return;
            }
        }
        handleMessage(msg);
    }
}
可以看见 dispatchMessage 方法中的逻辑比较简单，具体就是检查 Message 的 callback 是否为空，不为空，就通过 handleCallback() 方法处理消息。 Message 的 callback 是一个 Runnable 对象，就是handler 的 post 方法所传递的 Runnable 参数）不为空，handleCallback() 方法，如下

private static void handleCallback(Message message) {
    message.callback.run();
}
否则，就检查 mCallback 是否为空，不为空就调用 Callback 的 handleMessage() 方法处理消息。mCallback 是一个接口，如下

/**
 * Callback interface you can use when instantiating a Handler to avoid
 * having to implement your own subclass of Handler.
 *
 * @param msg A {@link android.os.Message Message} object
 * @return True if no further handling is desired
 */
public interface Callback {
    public boolean handleMessage(Message msg);
}
通过注释可知，可以采用如下方式创建 Handler 对象： Handler handler = new Handler（callback）。对应的构造方法如下

/**
 * Constructor associates this handler with the {@link Looper} for the
 * current thread and takes a callback interface in which you can handle
 * messages.
 *
 * If this thread does not have a looper, this handler won't be able to receive messages
 * so an exception is thrown.
 *
 * @param callback The callback interface in which to handle messages, or null.
 */
public Handler(Callback callback) {
    this(callback, false);
}
Callback 的意义如同注释一般：可以用来创一个 Handler 的实例但不需要派生出 Handler 的子类。

因为在日常开发过程中，创建 Handler 最常见的方式就是派生一个 Handler 的子类并重写其handleMessage 方法来处理具体的消息，而Callback给我们提供了另外一种使用 Handler 的方式，当我们不想派生子类时，就可以通过 Callback 实现。

最后，调用 Handler 的 handleMessage 方法来处理消息。

为什么handleMessage() 方法中可以获取到之前发送的消息，这就是原因。

因此，一个最标准的异步消息处理线程的写法应该是这样：

class LooperThread extends Thread {
  public Handler mHandler;

  public void run() {
      Looper.prepare();

      mHandler = new Handler() {
          public void handleMessage(Message msg) {
              // process incoming messages here
          }
      };

      Looper.loop();
  }
}

现在再看 handler 的架构图，是不是就更清晰了。

当我们在子线程调用 loop.prepare() 和 loop() 方法后，最好调用 loop.quit() 方法退出，终止消息循环，否则这个子线程就会一直处于等待状态。那么 quit 方法如下

/**
 * Quits the looper.
 * <p>
 * Causes the {@link #loop} method to terminate without processing any
 * more messages in the message queue.
 * </p><p>
 * Any attempt to post messages to the queue after the looper is asked to quit will fail.
 * For example, the {@link Handler#sendMessage(Message)} method will return false.
 * </p><p class="note">
 * Using this method may be unsafe because some messages may not be delivered
 * before the looper terminates.  Consider using {@link #quitSafely} instead to ensure
 * that all pending work is completed in an orderly manner.
 * </p>
 *
 * @see #quitSafely
 */
public void quit() {
    mQueue.quit(false);
}
再找

void quit(boolean safe) {
    if (!mQuitAllowed) {
        throw new IllegalStateException("Main thread not allowed to quit.");
    }

    synchronized (this) {
        if (mQuitting) {
            return;
        }
        mQuitting = true;

        if (safe) {
            removeAllFutureMessagesLocked();
        } else {
            removeAllMessagesLocked();
        }

        // We can assume mPtr != 0 because mQuitting was previously false.
        nativeWake(mPtr);
    }
}
我们知道，在子线程中调用 preare 时

public static void prepare() {
    prepare(true);
}
默认的是 true，也是就是说，子线程是可以退出的，而在 UI Thread 中

public static void prepareMainLooper() {
    prepare(false);//可以看出，UI thread传入的是false
    synchronized (Looper.class) {
        if (sMainLooper != null) {
            throw new IllegalStateException("The main Looper has already been prepared.");
        }
        sMainLooper = myLooper();
    }
}
传的 false，就是提示 UI Thread 是不可以退出的

回到 quit 方法继续看，可以发现实质就是对 mQuitting 标记置位，这个 mQuitting 标记在MessageQueue 的阻塞等待 next 方法中用做了判断条件，所以可以通过 quit 方法退出整个当前线程的loop 循环。

到此整个 Android 的一次完整异步消息机制分析使用流程结束。

前面涉及到的几个主要的类 Handler、Looper、MessageQueue 和 Message 的关系如下所述：

Handler 负责将 Looper 绑定到线程，初始化 Looper 和提供对外 API。
Looper 负责消息循环和操作 MessageQueue 对象。
MessageQueue 实现了一个堵塞队列。
Message 是一次业务中所有参数的载体。
重点

如果您看到了这里，那么今天分析 Handler、Loop 和 MessageQueue，主要是为了引出下面的这个东西 BlockCanary, 一个 Android 平台的一个非侵入式的性能监控组件，项目中已经打算性能优化专项中引入并解决相关性能问题，为了了解其原理，故整理了一下整个 Handler 的原理。该控件的相关说明在这里。

如果应用滑动卡顿，可以使用该控件进行监控（作者实现这个控件的原理，大家看过就会了解。很佩服作者在消息机制中发现了这么一个方式能够监控性能，同样是看过源码分析，差距还是很明显的，脑子不够开窍哇）

参考资料

Android开发艺术探索[M]. 电子工业出版社, 2015.372-390

android在线程中创建handler应注意什么 #44

Android异步消息处理机制完全解析，带你从源码的角度彻底理解

Android异步消息处理机制详解及源码分析