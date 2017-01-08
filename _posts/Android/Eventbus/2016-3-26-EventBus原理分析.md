---
layout: post
title: "EventBus原理解析"
description: "Java"
category: EventBus
tags: [Binder]

---
 
#### 单利模式


    /** Convenience singleton for apps using a process-wide EventBus instance. */
    public static EventBus getDefault() {
        if (defaultInstance == null) {
            synchronized (EventBus.class) {
                if (defaultInstance == null) {
                    defaultInstance = new EventBus();
                }
            }
        }
        return defaultInstance;
    }
    
多线程共享一个Eventbus单利，所以任何线程都可以直接操作Subscriber列表，对与不同的需求实现不同的任务，配合Hander可以完成各个界面刷新及后台更新等任务。



    private final HandlerPoster mainThreadPoster;
    private final BackgroundPoster backgroundPoster;
    private final AsyncPoster asyncPoster;
    private final SubscriberMethodFinder subscriberMethodFinder;
    private final ExecutorService executorService;
   
 
	     private final static ExecutorService DEFAULT_EXECUTOR_SERVICE = Executors.newCachedThreadPool();
	       
	       
	  public static ExecutorService newCachedThreadPool()
  
创建一个可根据需要创建新线程的线程池，但是在以前构造的线程可用时将重用它们。对于执行很多短期异步任务的程序而言，这些线程池通常可提高程序性能。
 
       public void enqueue(Subscription subscription, Object event) {
        PendingPost pendingPost = PendingPost.obtainPendingPost(subscription, event);
        queue.enqueue(pendingPost);
        eventBus.getExecutorService().execute(this);
    }
    
    
    class AsyncPoster implements Runnable {

    private final PendingPostQueue queue;
    private final EventBus eventBus;

    AsyncPoster(EventBus eventBus) {
        this.eventBus = eventBus;
        queue = new PendingPostQueue();
    }

    public void enqueue(Subscription subscription, Object event) {
        PendingPost pendingPost = PendingPost.obtainPendingPost(subscription, event);
        queue.enqueue(pendingPost);
        eventBus.getExecutorService().execute(this);
    }
    
    
    

        case PostThread:
        invokeSubscriber(subscription, event);
        break;
    case MainThread:
        if (isMainThread) {
            invokeSubscriber(subscription, event);
        } else {
            mainThreadPoster.enqueue(subscription, event);
        }
        break;
    case BackgroundThread:
        if (isMainThread) {
            backgroundPoster.enqueue(subscription, event);
        } else {
            invokeSubscriber(subscription, event);
        }
        break;
    case Async:
        asyncPoster.enqueue(subscription, event);
                
上面四种只有BackgroundThread跟Async需要进入队列，其他的都不需要，要么直接在当前线程运行，要么发送到UI线程。

####   不允许重复注册
  
        // Must be called in synchronized block
    private void subscribe(Object subscriber, SubscriberMethod subscriberMethod, boolean sticky, int priority) {
        Class<?> eventType = subscriberMethod.eventType;
        CopyOnWriteArrayList<Subscription> subscriptions = subscriptionsByEventType.get(eventType);
        Subscription newSubscription = new Subscription(subscriber, subscriberMethod, priority);
        if (subscriptions == null) {
            subscriptions = new CopyOnWriteArrayList<Subscription>();
            subscriptionsByEventType.put(eventType, subscriptions);
        } else {
            if (subscriptions.contains(newSubscription)) {
                throw new EventBusException("Subscriber " + subscriber.getClass() + " already registered to event "
                        + eventType);
            }
        }
        
#### 反射机制，维护不同Event事件的处理列表。

    void invokeSubscriber(Subscription subscription, Object event) {
        try {
            invokeSubscriberBefore(subscription.subscriberMethod.method, subscription.subscriber, event);
            subscription.subscriberMethod.method.invoke(subscription.subscriber, event);
            invokeSubscriberAfter(subscription.subscriberMethod.method, subscription.subscriber, event);
        } catch (InvocationTargetException e) {
            handleSubscriberException(subscription, event, e.getCause());
        } catch (IllegalAccessException e) {
            throw new IllegalStateException("Unexpected exception", e);
        }
    }
    
    
#### EventBus  注册/注销代码理解

        <!--添加到相应的EventBus订阅列表-->
        int size = subscriptions.size();
        for (int i = 0; i <= size; i++) {
            if (i == size || newSubscription.priority > subscriptions.get(i).priority) {
                subscriptions.add(i, newSubscription);
                break;
            }
        }
       
       <!--为了方便Unregister 缓存一下，，不用再次遍历类的方法数列表-->

        List<Class<?>> subscribedEvents = typesBySubscriber.get(subscriber);
        if (subscribedEvents == null) {
            subscribedEvents = new ArrayList<Class<?>>();
            typesBySubscriber.put(subscriber, subscribedEvents);
        }
        subscribedEvents.add(eventType);


注销

    /** Unregisters the given subscriber from all event classes. */
    public synchronized void unregister(Object subscriber) {
        List<Class<?>> subscribedTypes = typesBySubscriber.get(subscriber);
        if (subscribedTypes != null) {
            for (Class<?> eventType : subscribedTypes) {
                unubscribeByEventType(subscriber, eventType);
            }
            typesBySubscriber.remove(subscriber);
        } else {
            Log.w(TAG, "Subscriber to unregister was not registered before: " + subscriber.getClass());
        }
    }

#### 使用注意事项 混淆注意 版本2.4.1

	#EventBus
	-keep class de.greenrobot.**{*;}
	-keepclassmembers class ** {
	    public void onEvent*(**);
	    void onEvent*(**);
	}

#### 参考文档

[Android解耦库EventBus的使用和源码分析](http://blog.csdn.net/yuanzeyao/article/details/38174537)