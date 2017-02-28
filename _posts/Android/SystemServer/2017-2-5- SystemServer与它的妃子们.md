---
layout: post
title: "SystemServer与它的妃子们"
description: "Android"
categories: [Android]

---

# SystemServer的启动

# SystemServer的概念与地位

# 妃子们的启动，通信与Handler的Looper

比如AMS，自己是个BBinder实体，但是为了异步处理请求，重新开启了一个Thread，并且绑定Looper，利用Handler消息机制处理。

# 地位

# 功能


    public static final void zygoteInit(int targetSdkVersion, String[] argv)
            throws ZygoteInit.MethodAndArgsCaller {
        if (DEBUG) Slog.d(TAG, "RuntimeInit: Starting application from zygote");

        redirectLogStreams();

        commonInit();
        nativeZygoteInit();

        applicationInit(targetSdkVersion, argv);
    }
    
    
    自动带Binder原理
    
ActivityThread主线程
    
  
Binder线程
    
	    virtual void onZygoteInit()
	{
	    //获取ProcessState对象【见小节2.2】
	    sp<ProcessState> proc = ProcessState::self();
	    //启动新binder线程 【见小节2.3】
	    proc->startThreadPool();
	}


# system_server的主线程并非binder线程？？？

这里要区分版本 4.3 源码里面可以看出，主线程就是binder线程 

但是6.0 来看，不是，主线程不是BInder线程 仅仅是个ActivityThread Looper线程

