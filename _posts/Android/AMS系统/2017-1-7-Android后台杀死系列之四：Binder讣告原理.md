---
layout: post
title: "Android后台杀死系列之四：Binder讣告原理"
category: Android

---
 
 
Binder通信是同步而不是异步的：但是在实际使用时，是设计成客户端同步而服务端异步。



我们知道Binder是一个类似于C/S架构的通信框架，作为客户端可能想知道服务端的状态，比如服务端如果挂了，客户端希望能提前知道，而不是等到再起请求服务端的时候才知道。Binder帮我们实现了一套“”死亡讣告”的功能，即：服务端挂了，Binder驱动会向客户端发送一份讣告，告诉客户端Binder服务挂了。那么这个究竟是如何实现的呢？
	
 
**在操作系统中，无论是正常退出还是异常退出，这个进程所申请的所有资源都会被回收，包括打开的一些设备文件，如Binder字符设备等。在释放的时候，就会调用相应的release函数
**
这里说的异常杀死，当然包括通过发送SIGKILL信号量进行的进程杀死。

# Binder死亡通知的发送

发送死亡通知：本地对象死亡会出发关闭/dev/binder设备，binder_release会被调用，binder驱动程序会在其中检查Binder本地对象是否死亡，该过程会调用binder_deferred_release 执行。如死亡会在binder_thread_read中检测到BINDER_WORK_DEAD_BINDER的工作项。就会发出死亡通知。

Server进程在启动时，会调用函数open来打开设备文件/dev/binder。

* 一方面，在正常情况下，它退出时会调用函数close来关闭设备文件/dev/binder，这时候就会触发函数binder_releasse被调用；
* 另一方面，如果Server进程异常退出，即它没有正常关闭设备文件/dev/binder，那么内核就会负责关闭它，这个时候也会触发函数binder_release被调用。

因此，Binder驱动程序就可以在函数binder_release中检查进程退出时，是否有Binder本地对象在里面运行。如果有，就说明它们是死亡了的Binder本地对象了。


### 参考文档

[Android Binder 分析——死亡通知（DeathRecipient）](http://light3moon.com/2015/01/28/Android%20Binder%20%E5%88%86%E6%9E%90%E2%80%94%E2%80%94%E6%AD%BB%E4%BA%A1%E9%80%9A%E7%9F%A5[DeathRecipient])