---
layout: post
title: "HHanlder与Looper原理分析"
description: "android"
category: android
tags: [android]

---

直观的想一下，Hanlder与Loope组成的消息队列如何用，肯定是一个死循环，不断的从队列上读取消息，并执行，再读取，当然，这个期间是可以通过同步机制插入消息，唤醒等待的线程之类的操作。

# 缓存机制Message

[参考](http://blog.csdn.net/tear2210/article/details/49741647)

# 同步

# idle，最后执行

# MessageQueue.java与NativeMessageQueue.cpp两个队列的问题

为什么保留两个队列

在nativeInit中，new了一个Native层的MessageQueue的对象，并将其地址保存在了Java层MessageQueue的成员mPtr中，Android中有好多这样的实现，一个类在Java层与Native层都有实现，通过JNI的GetFieldID与SetIntField把Native层的类的实例地址保存到Java层类的实例的mPtr成员中，比如Parcel。


# Handler的基本用法

        Handler hanlder=new Handler();
        hanlder.post(new Runnable() {
            @Override
            public void run() {
                //TODO 
            }
        });


# Hanlder实现原理

    public Handler(Callback callback, boolean async) {
        if (FIND_POTENTIAL_LEAKS) {
            final Class<? extends Handler> klass = getClass();
            if ((klass.isAnonymousClass() || klass.isMemberClass() || klass.isLocalClass()) &&
                    (klass.getModifiers() & Modifier.STATIC) == 0) {
                Log.w(TAG, "The following Handler class should be static or leaks might occur: " +
                    klass.getCanonicalName());
            }
        }
		<!--关键点1-->
        mLooper = Looper.myLooper();
        if (mLooper == null) {
            throw new RuntimeException(
                "Can't create handler inside thread that has not called Looper.prepare()");
        }
        mQueue = mLooper.mQueue;
        mCallback = callback;
        mAsynchronous = async;
    }
    
新建Hanlder的时候 Looper.myLooper()不能为空，而Looper在线程中创建时机：
    
        private static void prepare(boolean quitAllowed) {
        if (sThreadLocal.get() != null) {
            throw new RuntimeException("Only one Looper may be created per thread");
        }
        sThreadLocal.set(new Looper(quitAllowed));
    }
    
在新建Looper时候，也新建了本线程唯一的消息队列：
    
        private Looper(boolean quitAllowed) {
        mQueue = new MessageQueue(quitAllowed);
        mThread = Thread.currentThread();
    }
    
 到这里我们知道，Hanlder里面会有个Looper成员变量，而Looper内存会创建一个MessageQueue消息队列，Hanlder会忘这个队列上发送消息， 
    
        public final boolean post(Runnable r)
    {
       return  sendMessageDelayed(getPostMessage(r), 0);
    }
    
 先获取一个消息
 
 
    private static Message getPostMessage(Runnable r) {
        Message m = Message.obtain();
        m.callback = r;
        return m;
    }
   
获取是一个静态方法，这里牵扯到复用，

    // 静态方法，同步
    public static Message obtain() {
        synchronized (sPoolSync) {
            if (sPool != null) {
                Message m = sPool;
                sPool = m.next;
                m.next = null;
                m.flags = 0; // clear in-use flag
                sPoolSize--;
                return m;
            }
        }
        return new Message();
    }
    
 之后发送
 
     public boolean sendMessageAtTime(Message msg, long uptimeMillis) {
        MessageQueue queue = mQueue;
        if (queue == null) {
            RuntimeException e = new RuntimeException(
                    this + " sendMessageAtTime() called with no mQueue");
            Log.w("Looper", e.getMessage(), e);
            return false;
        }
        return enqueueMessage(queue, msg, uptimeMillis);
    }   
   
        private boolean enqueueMessage(MessageQueue queue, Message msg, long uptimeMillis) {
        <!--关键点1-->
        msg.target = this;
        if (mAsynchronous) {
            msg.setAsynchronous(true);
        }
        return queue.enqueueMessage(msg, uptimeMillis);
    }


	boolean enqueueMessage(Message msg, long when) {
	        if (msg.target == null) {
	            throw new IllegalArgumentException("Message must have a target.");
	        }
	        if (msg.isInUse()) {
	            throw new IllegalStateException(msg + " This message is already in use.");
	        }

		// 同步，需要同步

        synchronized (this) {
            if (mQuitting) {
                IllegalStateException e = new IllegalStateException(
                        msg.target + " sending message to a Handler on a dead thread");
                Log.w(TAG, e.getMessage(), e);
                msg.recycle();
                return false;
            }

            msg.markInUse();
            msg.when = when;
            Message p = mMessages;
            boolean needWake;
            if (p == null || when == 0 || when < p.when) {
                // New head, wake up the event queue if blocked.
                msg.next = p;
                mMessages = msg;
                needWake = mBlocked;
            } else {
                // Inserted within the middle of the queue.  Usually we don't have to wake
                // up the event queue unless there is a barrier at the head of the queue
                // and the message is the earliest asynchronous message in the queue.
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
                msg.next = p; // invariant: p == prev.next
                prev.next = msg;
            }

            // We can assume mPtr != 0 because mQuitting is false.
            if (needWake) {
                nativeWake(mPtr);
            }
        }
        return true;
    }
    
 很明显需要同步,Java层只是一个简单的入栈过程，至于是否需要唤醒，
    
    
所以一个普通线程，必须调用Looper.prepare()才能新建Hanlder对象，而Hanler对象真正能用还需要线程编程Loop线程：

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
<!--关键点1-->
        for (;;) {
        <!--关键点2-->
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
    
  看关键点1 ，这里其实就是将线程化身成Looper线程，不断的执行消息  ，关键点2 ，就是去除下一个要执行的消息，当然，函数有可能阻塞到这里，看下MessageQueue的函数
  
  
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
	    
# epoll_wait(int epfd, epoll_event events, int max events, int timeout) 

timeout==0直接返回，上层已经兼容性的考虑了延时的问题，已经考虑了有延时，并且确定了下一个消息的最大延时

关键是什么？睡眠在一个队列上？不是真正的睡眠，是在一个Fd上，并且有睡眠延时，唤醒，可以从其他线程唤醒，Syn关键字，保证了互斥访问，


# 即可执行与延时消息的处理

# 参考文档
 
[Android消息机制1-Handler(Java层)](http://gityuan.com/2015/12/26/handler-message-framework/)          
[Android消息处理机制(Handler、Looper、MessageQueue与Message)](http://www.cnblogs.com/angeldevil/p/3340644.html)           