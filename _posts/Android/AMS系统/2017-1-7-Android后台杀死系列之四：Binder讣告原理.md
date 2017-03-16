---
layout: post
title: "Android后台杀死系列之四：Binder讣告原理"
category: Android

---
 
Binder是一个类似于C/S架构的通信框架，有时候客户端可能想知道服务端的状态，比如服务端如果挂了，客户端希望能及时的被通知到，而不是等到再起请求服务端的时候才知道。Binder实现了一套”死亡讣告”的功能，即：服务端挂了，或者正常退出，Binder驱动会向客户端发送一份讣告，告诉客户端Binder服务挂了。那么这个“讣告”究竟是如何实现的呢？其作用又是什么呢？其实“讣告”真正的入口只有一个：**在释放binder设备的时候发送讣告**，**在操作系统中，无论进程是正常退出还是异常退出，进程所申请的所有资源都会被回收，包括打开的一些设备文件，如Binder字符设备等。在释放的时候，就会调用相应的release函数 **。Binder“讣告”有点采用了类似观察者模式，因此，首先需要将Observer注册到目标对象中，其实就是将Client注册到Binder驱动。

# 注册入口

# Binder死亡通知的发送

发送死亡通知：本地对象死亡会出发关闭/dev/binder设备，binder_release会被调用，binder驱动程序会在其中检查Binder本地对象是否死亡，该过程会调用binder_deferred_release 执行。如死亡会在binder_thread_read中检测到BINDER_WORK_DEAD_BINDER的工作项。就会发出死亡通知。

Server进程在启动时，会调用函数open来打开设备文件/dev/binder。

* 一方面，在正常情况下，它退出时会调用函数close来关闭设备文件/dev/binder，这时候就会触发函数binder_releasse被调用；
* 另一方面，如果Server进程异常退出，即它没有正常关闭设备文件/dev/binder，那么内核就会负责关闭它，这个时候也会触发函数binder_release被调用。

因此，Binder驱动程序就可以在函数binder_release中检查进程退出时，是否有Binder本地对象在里面运行。如果有，就说明它们是死亡了的Binder本地对象了。

在bindService的时候，是系统框架帮我们封装好了回调，但是native服务一般都是需要自己写的，IBinder.DeathRecipient

        public void doConnected(ComponentName name, IBinder service) {
            ServiceDispatcher.ConnectionInfo old;
            ServiceDispatcher.ConnectionInfo info;

            synchronized (this) {     
                if (service != null) {

                    mDied = false;
                    info = new ConnectionInfo();
                    info.binder = service;
                    info.deathMonitor = new DeathMonitor(name, service);
                    try {
                        service.linkToDeath(info.deathMonitor, 0);
                        mActiveConnections.put(name, info);
                    } 
                } 
        }

看关键点点1 可以看出，当Client bindService结束后，会通过BinderProxy的linkToDeath注册死亡回调，进而去调用Native函数：


	status_t BpBinder::linkToDeath(
	    const sp<DeathRecipient>& recipient, void* cookie, uint32_t flags){
	    <!--关键点1-->              
	                IPCThreadState* self = IPCThreadState::self();
	                self->requestDeathNotification(mHandle, this);
	                self->flushCommands();

	}

看关键点1，其实是调用IPCThreadState的requestDeathNotification(mHandle, this)，之后发送BC_REQUEST_DEATH_NOTIFICATION请求到内核驱动：

	status_t IPCThreadState::requestDeathNotification(int32_t handle, BpBinder* proxy)
	{
	    mOut.writeInt32(BC_REQUEST_DEATH_NOTIFICATION);
	    mOut.writeInt32((int32_t)handle);
	    mOut.writeInt32((int32_t)proxy);
	    return NO_ERROR;
	}

有一点很关键：**在binder驱动中，binder_node节点会记录所有binder_ref**，当binder_node所在的进程挂掉后，驱动就能根据这个全局binder_ref列表找到所有Client的binder_ref，并对于设置了死亡回调的Client发送“讣告”。

![binder讣告原理.jpg](http://upload-images.jianshu.io/upload_images/1460468-d01abc307b4e32d7.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


### 参考文档

[Android Binder 分析——死亡通知（DeathRecipient）](http://light3moon.com/2015/01/28/Android%20Binder%20%E5%88%86%E6%9E%90%E2%80%94%E2%80%94%E6%AD%BB%E4%BA%A1%E9%80%9A%E7%9F%A5[DeathRecipient])

