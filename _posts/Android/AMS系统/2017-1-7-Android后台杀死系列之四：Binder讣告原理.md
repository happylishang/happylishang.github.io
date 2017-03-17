---
layout: post
title: "Android后台杀死系列之四：Binder讣告原理"
category: Android

---
 
Binder是一个类似于C/S架构的通信框架，有时候客户端可能想知道服务端的状态，比如服务端如果挂了，客户端希望能及时的被通知到，而不是等到再起请求服务端的时候才知道。Binder实现了一套”死亡讣告”的功能，即：服务端挂了，或者正常退出，Binder驱动会向客户端发送一份讣告，告诉客户端Binder服务挂了。那么这个“讣告”究竟是如何实现的呢？其作用又是什么呢？其实“讣告”真正的入口只有一个：**在释放binder设备的时候发送讣告**，**在操作系统中，无论进程是正常退出还是异常退出，进程所申请的所有资源都会被回收，包括打开的一些设备文件，如Binder字符设备等。在释放的时候，就会调用相应的release函数 **。Binder“讣告”有点采用了类似观察者模式，因此，首先需要将Observer注册到目标对象中，其实就是将Client注册到Binder驱动。

# 注册入口

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

之后会进入内核

	int
	binder_thread_write(struct binder_proc *proc, struct binder_thread *thread,
			    void __user *buffer, int size, signed long *consumed)
	{
	...
	case BC_REQUEST_DEATH_NOTIFICATION:
			case BC_CLEAR_DEATH_NOTIFICATION: {
				...
				ref = binder_get_ref(proc, target);
				if (cmd == BC_REQUEST_DEATH_NOTIFICATION) {
					...关键点1
					death = kzalloc(sizeof(*death), GFP_KERNEL);
					binder_stats.obj_created[BINDER_STAT_DEATH]++;
					INIT_LIST_HEAD(&death->work.entry);
					death->cookie = cookie;
					ref->death = death;
					if (ref->node->proc == NULL) {
						ref->death->work.type = BINDER_WORK_DEAD_BINDER;
						if (thread->looper & (BINDER_LOOPER_STATE_REGISTERED | BINDER_LOOPER_STATE_ENTERED)) {
							list_add_tail(&ref->death->work.entry, &thread->todo);
						} else {
							list_add_tail(&ref->death->work.entry, &proc->todo);
							wake_up_interruptible(&proc->wait);
						}
					}
				} 
	 }

看关键点1 ，其实就是为Client新建binder_ref_death对象，并赋值给binder_ref。**在binder驱动中，binder_node节点会记录所有binder_ref**，当binder_node所在的进程挂掉后，驱动就能根据这个全局binder_ref列表找到所有Client的binder_ref，并对于设置了死亡回调的Client发送“讣告”，这是因为在binder_get_ref_for_node向Client插入binder_ref的时候，也会插入binder_node的binder_ref列表。

	static struct binder_ref *
	binder_get_ref_for_node(struct binder_proc *proc, struct binder_node *node)
	{
		struct rb_node *n;
		struct rb_node **p = &proc->refs_by_node.rb_node;
		struct rb_node *parent = NULL;
		struct binder_ref *ref, *new_ref;
	
		if (node) {
			hlist_add_head(&new_ref->node_entry, &node->refs);
			}
		return new_ref;
	}
			
![binder讣告原理.jpg](http://upload-images.jianshu.io/upload_images/1460468-d01abc307b4e32d7.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

如此，就死亡回调就被注册到binder内核驱动。之后，等到进程结束释放binder，就会触发死亡回调。

![死亡讣告的注册.png](http://upload-images.jianshu.io/upload_images/1460468-36506aff731ad964.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# 死亡通知的发送

	static void binder_deferred_release(struct binder_proc *proc)
		{         ....
			if (ref->death) {
					death++;
					if (list_empty(&ref->death->work.entry)) {
						ref->death->work.type = BINDER_WORK_DEAD_BINDER;
						list_add_tail(&ref->death->work.entry, &ref->proc->todo);
						// 插入到binder_ref请求进程的binder线程等待队列？？？？？ 天然支持binder通信吗？
						// 什么时候，需要死亡回调，自己也是binder服务？
						wake_up_interruptible(&ref->proc->wait);
					} 				
			...
	 }
				
似乎只有那些相互为Binder服务的进程才需要，也就是说，Client也是服务

	static int
	binder_thread_read(struct binder_proc *proc, struct binder_thread *thread,
		void  __user *buffer, int size, signed long *consumed, int non_block)
	{
		case BINDER_WORK_DEAD_BINDER:
				case BINDER_WORK_DEAD_BINDER_AND_CLEAR:
				case BINDER_WORK_CLEAR_DEATH_NOTIFICATION: {
					struct binder_ref_death *death = container_of(w, struct binder_ref_death, work);
					uint32_t cmd;
					if (w->type == BINDER_WORK_CLEAR_DEATH_NOTIFICATION)
						cmd = BR_CLEAR_DEATH_NOTIFICATION_DONE;
					else
						cmd = BR_DEAD_BINDER;
					if (put_user(cmd, (uint32_t __user *)ptr))
						return -EFAULT;
					ptr += sizeof(uint32_t);
					if (put_user(death->cookie, (void * __user *)ptr))
						return -EFAULT;
					ptr += sizeof(void *);
		
					if (w->type == BINDER_WORK_CLEAR_DEATH_NOTIFICATION) {
						list_del(&w->entry);
						kfree(death);
						binder_stats.obj_deleted[BINDER_STAT_DEATH]++;
					} else
						list_move(&w->entry, &proc->delivered_death);
					if (cmd == BR_DEAD_BINDER)
						goto done; /* DEAD_BINDER notifications can cause transactions */
				} break;
				}

可以看到，就是插入到Client进程的主等待队列，如果Client存在Binder线程，就会执行，当然，如果不存在，则在下次请求服务的时候会发现binder_node进程已死，可以预先处理了。
	
	status_t IPCThreadState::executeCommand(int32_t cmd)
	{
	    // 死亡讣告
	    case BR_DEAD_BINDER:
	        {
	            BpBinder *proxy = (BpBinder*)mIn.readInt32();
	            <!--关键点1 -->
	            proxy->sendObituary();
	            mOut.writeInt32(BC_DEAD_BINDER_DONE);
	            mOut.writeInt32((int32_t)proxy);
	        } break;

看关键点1，Obituary直译过来就是讣告，就是利用BpBinder发送讣告的意思，通知结束后，再发送给Binder驱动。

	void BpBinder::sendObituary()
	{
	    ALOGV("Sending obituary for proxy %p handle %d, mObitsSent=%s\n",
	        this, mHandle, mObitsSent ? "true" : "false");
	    mAlive = 0;
	    if (mObitsSent) return;
	    mLock.lock();
	    Vector<Obituary>* obits = mObituaries;
	    if(obits != NULL) {
	    <!--关键点1-->
	        IPCThreadState* self = IPCThreadState::self();
	        self->clearDeathNotification(mHandle, this);
	        self->flushCommands();
	        mObituaries = NULL;
	    }
	    mObitsSent = 1;
	    mLock.unlock();
	    if (obits != NULL) {
	        const size_t N = obits->size();
	        for (size_t i=0; i<N; i++) {
	            reportOneDeath(obits->itemAt(i));
	        }
	        delete obits;
	    }
	}

看关键点1，这里跟注册相对应，将自己从观察者列表中清除，之后再上报

	void BpBinder::reportOneDeath(const Obituary& obit)
	{
	    sp<DeathRecipient> recipient = obit.recipient.promote();
	    ALOGV("Reporting death to recipient: %p\n", recipient.get());
	    if (recipient == NULL) return;
	
	    recipient->binderDied(this);
	}
	
进而调用上层DeathRecipient的回调，做一些清理之类的逻辑。

![死亡讣告的发送.png](http://upload-images.jianshu.io/upload_images/1460468-ea8da541d7339d22.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
	        						
### 参考文档

[Android Binder 分析——死亡通知（DeathRecipient）](http://light3moon.com/2015/01/28/Android%20Binder%20%E5%88%86%E6%9E%90%E2%80%94%E2%80%94%E6%AD%BB%E4%BA%A1%E9%80%9A%E7%9F%A5[DeathRecipient])

