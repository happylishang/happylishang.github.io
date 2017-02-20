---
layout: default
title: "binderService原理"
description: "Java"
categories: [android,Binder]
tags: [Binder]

---

# Service是什么 （ 很多东西，能简单几句话描述清楚，就说明你懂了）

是一个可以在后台执行长时间运行操作而不使用用户界面的应用组件。服务可由其他应用组件启动，而且即使用户切换到其他应用，服务仍将在后台继续运行。 此外，组件可以绑定到服务，以与之进行交互，甚至是执行进程间通信 (IPC)。例如，服务可以处理网络事务、播放音乐，执行文件 I/O 或与内容提供程序交互，而所有这一切均可在后台进行。（做个音乐播放器）

Service 作为四大组件之一，提供了不需要前台页面情况下，在后台继续执行任务的能力。Service 一般有两种使用方式，分别是通过 startService 和 bindService，前者适合执行一次性的任务，而后者则具备一定交互的能力，可以用作处理相对复杂的后台逻辑。

没界面，生命周期跟界面无关，Service运行在后台，开线程处理一些耗时任务，完成后，Service再销毁。

##### binderService源码分析

bindService与onServiceConnected执行是异步的，首先看一下binderService的源码，

		@Override
		public boolean bindService(Intent service, ServiceConnection conn,
	 
	 			....
		        int res = ActivityManagerNative.getDefault().bindService(
		            mMainThread.getApplicationThread(), getActivityToken(),
		            service, service.resolveTypeIfNeeded(getContentResolver()),
		            sd, flags);
		        if (res < 0) {
		            throw new SecurityException(
		                    "Not allowed to bind to service " + service);
		        }
		    ...
		}
		

IServiceConnection.Stub 其实也是一条Binder体系，InnerConnection对象是一个Binder对象，一会是要传递给ActivityManagerService的，ActivityManagerServic后续就是要通过这个Binder对象和ServiceConnection通信的。

	    private static class InnerConnection extends IServiceConnection.Stub {  
	        final WeakReference<LoadedApk.ServiceDispatcher> mDispatcher;  
	        ......  
	  
	        InnerConnection(LoadedApk.ServiceDispatcher sd) {  
	            mDispatcher = new WeakReference<LoadedApk.ServiceDispatcher>(sd);  
	        }  
	  
	        ......  
	    }   来看看bind函数
	 
	 public int bindService(IApplicationThread caller, IBinder token,  
	        Intent service, String resolvedType, IServiceConnection connection,  
	        int flags
	    
	            
	    data.writeStrongBinder(connection.asBinder());  
    
接着通过retrieveServiceLocked函数，得到一个ServiceRecord，这个ServiceReocrd描述的是一个Service对象，这里就是CounterService了，这是根据传进来的参数service的内容获得的。回忆一下在MainActivity.onCreate函数绑定服务的语句：

	Intent bindIntent = new Intent(MainActivity.this, CounterService.class);  
	bindService(bindIntent, serviceConnection, Context.BIND_AUTO_CREATE);  

这里的参数service，就是上面的bindIntent了，它里面设置了Service类的信息，因此，这里可以通过它来把Service的信息取出来，并且保存在ServiceRecord对象s中。 接下来，就是把传进来的参数connection封装成一个ConnectionRecord对象。注意，这里的参数connection是一个Binder对象，它的类型是LoadedApk.ServiceDispatcher.InnerConnection，是在Step 4中创建的，后续ActivityManagerService就是要通过它来告诉MainActivity，CounterService已经启动起来了，因此，这里要把这个ConnectionRecord变量c保存下来，它保在在好几个地方，都是为了后面要用时方便地取回来的，这里就不仔细去研究了，只要知道ActivityManagerService要使用它时就可以方便地把它取出来就可以了. 我们先沿着app.thread.scheduleCreateService这个路径分析下去，然后再回过头来分析requestServiceBindingsLocked的调用过程。这里的app.thread是一个Binder对象的远程接口，类型为ApplicationThreadProxy。每一个Android应用程序进程里面都有一个ActivtyThread对象和一个ApplicationThread对象，其中是ApplicationThread对象是ActivityThread对象的一个成员变量，是ActivityThread与ActivityManagerService之间用来执行进程间通信的，
    
       IBinder binder = s.onBind(data.intent);  
       ActivityManagerNative.getDefault().publishService(  
                            data.token, data.intent, binder);  
                            


IServiceConnection如何完成在AMS端口的转换

	sp<IBinder> Parcel::readStrongBinder() const  
		{  
		    sp<IBinder> val;  
		    unflatten_binder(ProcessState::self(), *this, &val);  
		    return val;  
		}  
		没什么，再向下看，不是什么东东都可以向下看的，否则别人会骂的。
		
		[cpp] view plain copy 在CODE上查看代码片派生到我的代码片
		status_t unflatten_binder(const sp<ProcessState>& proc,  
		    const Parcel& in, sp<IBinder>* out)  
		{  
		    const flat_binder_object* flat = in.readObject(false);  
		      
		    if (flat) {  
		        switch (flat->type) {  
		            case BINDER_TYPE_BINDER:  
		                *out = static_cast<IBinder*>(flat->cookie);  
		                return finish_unflatten_binder(NULL, *flat, in);  
		            case BINDER_TYPE_HANDLE: //因为我们是Client,当然会调用这个  
		                *out = proc->getStrongProxyForHandle(flat->handle);  
		                return finish_unflatten_binder(  
		                    static_cast<BpBinder*>(out->get()), *flat, in);  
		        }          
		    }  
		    return BAD_TYPE;  
		} 
	
这个返回的就是一个BpBinder,其handle为传入的handle.现在已经看到reply.readStrongBinder()的返回值为一个BpBinder,即interface_cast(reply.readStrongBinder());的参数为一个BpBinder.

	  case BIND_SERVICE_TRANSACTION: {
	        data.enforceInterface(IActivityManager.descriptor);
	        IBinder b = data.readStrongBinder();
	        IApplicationThread app = ApplicationThreadNative.asInterface(b);
	        IBinder token = data.readStrongBinder();
	        Intent service = Intent.CREATOR.createFromParcel(data);
	        String resolvedType = data.readString();
	                    
	        //这个转换可以吧b转换成代理
	        b = data.readStrongBinder();
	        int fl = data.readInt();
	        IServiceConnection conn = IServiceConnection.Stub.asInterface(b);
	        int res = bindService(app, token, service, resolvedType, conn, fl);
	        reply.writeNoException();
	        reply.writeInt(res);
	        return true;
	    }
	    

接着通过retrieveServiceLocked函数，得到一个ServiceRecord，这个ServiceReocrd描述的是一个Service对象，这里就是CounterService了，这是根据传进来的参数service的内容获得的。回忆一下在MainActivity.onCreate函数绑定服务的语句：

	Intent bindIntent = new Intent(MainActivity.this, CounterService.class);  
	bindService(bindIntent, serviceConnection, Context.BIND_AUTO_CREATE);  
	
这里的参数service，就是上面的bindIntent了，它里面设置了CounterService类的信息（CounterService.class），因此，这里可以通过它来把CounterService的信息取出来，并且保存在ServiceRecord对象s中。接下来，就是把传进来的参数connection封装成一个ConnectionRecord对象。注意，这里的参数connection是一个Binder对象，它的类型是LoadedApk.ServiceDispatcher.InnerConnection，是在Step 4中创建的，后续ActivityManagerService就是要通过它来告诉MainActivity，CounterService已经启动起来了，因此，这里要把这个ConnectionRecord变量c保存下来，它保在在好几个地方，都是为了后面要用时方便地取回来的，这里就不仔细去研究了，只要知道ActivityManagerService要使用它时就可以方便地把它取出来就可以了. 我们先沿着app.thread.scheduleCreateService这个路径分析下去，然后再回过头来分析requestServiceBindingsLocked的调用过程。这里的app.thread是一个Binder对象的远程接口，类型为ApplicationThreadProxy。每一个Android应用程序进程里面都有一个ActivtyThread对象和一个ApplicationThread对象，其中是ApplicationThread对象是ActivityThread对象的一个成员变量，是ActivityThread与ActivityManagerService之间用来执行进程间通信的，

	   IBinder binder = s.onBind(data.intent);  
	   ActivityManagerNative.getDefault().publishService(  
	                            data.token, data.intent, binder);  
                            
接下来

	class ActivityManagerProxy implements IActivityManager  
	{  
	    ......  
	  
	    public void publishService(IBinder token,  
	    Intent intent, IBinder service) throws RemoteException {  
	        Parcel data = Parcel.obtain();  
	        Parcel reply = Parcel.obtain();  
	        data.writeInterfaceToken(IActivityManager.descriptor);  
	        data.writeStrongBinder(token);  
	        intent.writeToParcel(data, 0);  
	        data.writeStrongBinder(service);  
	        mRemote.transact(PUBLISH_SERVICE_TRANSACTION, data, reply, 0);  
	        reply.readException();  
	        data.recycle();  
	        reply.recycle();  
	    }  
	  
	    ......  
	} 
	
IServiceConnection如何完成在AMS端口的转换

		sp<IBinder> Parcel::readStrongBinder() const  
		{  
		    sp<IBinder> val;  
		    unflatten_binder(ProcessState::self(), *this, &val);  
		    return val;  
		}  
		
		[cpp] view plain copy 在CODE上查看代码片派生到我的代码片
		status_t unflatten_binder(const sp<ProcessState>& proc,  
		    const Parcel& in, sp<IBinder>* out)  
		{  
		    const flat_binder_object* flat = in.readObject(false);  
		      
		    if (flat) {  
		        switch (flat->type) {  
		            case BINDER_TYPE_BINDER:  
		                *out = static_cast<IBinder*>(flat->cookie);  
		                return finish_unflatten_binder(NULL, *flat, in);  
		            case BINDER_TYPE_HANDLE: //因为我们是Client,当然会调用这个  
		                *out = proc->getStrongProxyForHandle(flat->handle);  
		                return finish_unflatten_binder(  
		                    static_cast<BpBinder*>(out->get()), *flat, in);  
		        }          
		    }  
		    return BAD_TYPE;  
		}  


这个返回的就是一个BpBinder,其handle为传入的handle.现在已经看到reply.readStrongBinder()的返回值为一个BpBinder,即interface_cast<IMediaPlayer>(reply.readStrongBinder());的参数为一个BpBinder.


        case BIND_SERVICE_TRANSACTION: {
            data.enforceInterface(IActivityManager.descriptor);
            IBinder b = data.readStrongBinder();
            IApplicationThread app = ApplicationThreadNative.asInterface(b);
            IBinder token = data.readStrongBinder();
            Intent service = Intent.CREATOR.createFromParcel(data);
            String resolvedType = data.readString();
                        
            //这个转换可以吧b转换成代理
            b = data.readStrongBinder();
            int fl = data.readInt();
            IServiceConnection conn = IServiceConnection.Stub.asInterface(b);
            int res = bindService(app, token, service, resolvedType, conn, fl);
            reply.writeNoException();
            reply.writeInt(res);
            return true;
        }
    
    
  
	            
#####     ActivityThread::scheduleBindService()函數其实是注册的服务的地方 其实是注册到AMS中，binderService是跟AMS交互而非ServiceManager


    private void handleBindService(BindServiceData data) {
        Service s = mServices.get(data.token);
        if (DEBUG_SERVICE)
            Slog.v(TAG, "handleBindService s=" + s + " rebind=" + data.rebind);
        if (s != null) {
            try {
                data.intent.setExtrasClassLoader(s.getClassLoader());
                try {
                    if (!data.rebind) {
                        IBinder binder = s.onBind(data.intent);
                        ActivityManagerNative.getDefault().publishService(
                                data.token, data.intent, binder);
                    } else {
                        s.onRebind(data.intent);
                        ActivityManagerNative.getDefault().serviceDoneExecuting(
                                data.token, 0, 0, 0);
                    }
                    ensureJitEnabled();
                } catch (RemoteException ex) {
                }
            } catch (Exception e) {
                if (!mInstrumentation.onException(s, e)) {
                    throw new RuntimeException(
                            "Unable to bind to service " + s
                            + " with " + data.intent + ": " + e.toString(), e);
                }
            }
        }
    }
    
	   public void publishService(IBinder token,
	            Intent intent, IBinder service) throws RemoteException {
	        Parcel data = Parcel.obtain();
	        Parcel reply = Parcel.obtain();
	        data.writeInterfaceToken(IActivityManager.descriptor);
	        data.writeStrongBinder(token);
	        intent.writeToParcel(data, 0);
	        data.writeStrongBinder(service);
	        mRemote.transact(PUBLISH_SERVICE_TRANSACTION, data, reply, 0);
	        reply.readException();
	        data.recycle();
	        reply.recycle();
	    }
	    
#### binder代理与代理之间的转发，代理跟存根之间的转发

		case BINDER_TYPE_HANDLE:
		case BINDER_TYPE_WEAK_HANDLE: {
			struct binder_ref *ref = binder_get_ref(proc, fp->handle);
			if (ref == NULL) {
				binder_user_error("binder: %d:%d got "
					"transaction with invalid "
					"handle, %ld\n", proc->pid,
					thread->pid, fp->handle);
				return_error = BR_FAILED_REPLY;
				goto err_binder_get_ref_failed;
			}
			if (ref->node->proc == target_proc) {
				if (fp->type == BINDER_TYPE_HANDLE)
					fp->type = BINDER_TYPE_BINDER;
				else
					fp->type = BINDER_TYPE_WEAK_BINDER;
				fp->binder = ref->node->ptr;
				fp->cookie = ref->node->cookie;
				binder_inc_node(ref->node, fp->type == BINDER_TYPE_BINDER, 0, NULL);
				if (binder_debug_mask & BINDER_DEBUG_TRANSACTION)
					printk(KERN_INFO "        ref %d desc %d -> node %d u%p\n",
					       ref->debug_id, ref->desc, ref->node->debug_id, ref->node->ptr);
			} else {
				struct binder_ref *new_ref;
				new_ref = binder_get_ref_for_node(target_proc, ref->node);
				if (new_ref == NULL) {
					return_error = BR_FAILED_REPLY;
					goto err_binder_get_ref_for_node_failed;
				}
				fp->handle = new_ref->desc;
				binder_inc_ref(new_ref, fp->type == BINDER_TYPE_HANDLE, NULL);
				if (binder_debug_mask & BINDER_DEBUG_TRANSACTION)
					printk(KERN_INFO "        ref %d desc %d -> ref %d desc %d (node %d)\n",
					       ref->debug_id, ref->desc, new_ref->debug_id, new_ref->desc, ref->node->debug_id);
			}
		} break;
		
Java层的通信是经过封装。in与to 就是个例子 	

##### C++与Java Binder的转换

#  注册的android_util_Binder.cpp入口

int_register_android_os_BinderProxy入口[参考文档](http://gityuan.com/2015/11/21/binder-framework/)


unflatten_binder 创建BpBinder 并复制到BinderProxy的字段中

==> Parcel.cpp

status_t unflatten_binder(const sp<ProcessState>& proc,
    const Parcel& in, sp<IBinder>* out)
{
    const flat_binder_object* flat = in.readObject(false);
    if (flat) {
        switch (flat->type) {
            case BINDER_TYPE_BINDER:
                *out = reinterpret_cast<IBinder*>(flat->cookie);
                return finish_unflatten_binder(NULL, *flat, in);
            case BINDER_TYPE_HANDLE:
                //进入该分支【见4.6】
                *out = proc->getStrongProxyForHandle(flat->handle);
                //创建BpBinder对象
                return finish_unflatten_binder(
                    static_cast<BpBinder*>(out->get()), *flat, in);
        }
    }
    return BAD_TYPE;
}



Java层客户端的Binder代理都是BinderProxy，而且他们都是在native层生成的，因此，在上层看不到BinderProxy实例化。BinderProxy位于Binder.java中，

	
	final class BinderProxy implements IBinder {
	    public native boolean pingBinder();
	    public native boolean isBinderAlive();
	    
其创建位于Native代码/frameworks/base/core/jni/android_util_Binder.cpp中  

	const char* const kBinderProxyPathName = "android/os/BinderProxy";
	
	clazz = env->FindClass(kBinderProxyPathName);
	
	gBinderProxyOffsets.mClass = (jclass) env->NewGlobalRef(clazz);
	
	jobject javaObjectForIBinder(JNIEnv* env, const sp<IBinder>& val)
	{
	    if (val == NULL) return NULL;
	
	    if (val->checkSubclass(&gBinderOffsets)) {
	        // One of our own!
	        jobject object = static_cast<JavaBBinder*>(val.get())->object();
	        //printf("objectForBinder %p: it's our own %p!\n", val.get(), object);
	        return object;
	    }
	
	    // For the rest of the function we will hold this lock, to serialize
	    // looking/creation of Java proxies for native Binder proxies.
	    AutoMutex _l(mProxyLock);
	
	    // Someone else's...  do we know about it?
	    jobject object = (jobject)val->findObject(&gBinderProxyOffsets);
	    if (object != NULL) {
	        jobject res = env->CallObjectMethod(object, gWeakReferenceOffsets.mGet);
	        if (res != NULL) {
	            LOGV("objectForBinder %p: found existing %p!\n", val.get(), res);
	            return res;
	        }
	        LOGV("Proxy object %p of IBinder %p no longer in working set!!!", object, val.get());
	        android_atomic_dec(&gNumProxyRefs);
	        val->detachObject(&gBinderProxyOffsets);
	        env->DeleteGlobalRef(object);
	    }
	
	    object = env->NewObject(gBinderProxyOffsets.mClass, gBinderProxyOffsets.mConstructor);
	    if (object != NULL) {
	        LOGV("objectForBinder %p: created new %p!\n", val.get(), object);
	        // The proxy holds a reference to the native object.
	        env->SetIntField(object, gBinderProxyOffsets.mObject, (int)val.get());
	        val->incStrong(object);
	
	        // The native object needs to hold a weak reference back to the
	        // proxy, so we can retrieve the same proxy if it is still active.
	        jobject refObject = env->NewGlobalRef(
	                env->GetObjectField(object, gBinderProxyOffsets.mSelf));
	        val->attachObject(&gBinderProxyOffsets, refObject,
	                jnienv_to_javavm(env), proxy_cleanup);
	
	        // Note that a new object reference has been created.
	        android_atomic_inc(&gNumProxyRefs);
	        incRefsCreated(env);
	    }
	
	    return object;
	}

接下去是进入AMS的bindService，再调用ActiveServices.java 的bindServiceLocked，它会把IServiceConnection实例存放到ConnectionRecord里面，并执行bringUpServiceLocked，

    int bindServiceLocked(IApplicationThread caller, IBinder token,
            Intent service, String resolvedType,
            IServiceConnection connection, int flags, int userId) {
 
            ConnectionRecord c = new ConnectionRecord(b, activity,
                    connection, flags, clientLabel, clientIntent);
 
            IBinder binder = connection.asBinder();

				...
                if (bringUpServiceLocked(s, service.getFlags(), callerFg, false) != null){
                    return 0;      
	}
 
 
bringUpServiceLocked会调用realStartServiceLocked，调用scheduleCreateService，完成service的创建和Oncreate()的执行，然后执行requestServiceBindingsLocked，这个是bind服务相关处理，最后是sendServiceArgsLocked，这个是Start服务的处理。

    private final void realStartServiceLocked(ServiceRecord r,
            ProcessRecord app, boolean execInFg) throws RemoteException {
			<!--下面是Service的创建即启动流程-->
            app.thread.scheduleCreateService(r, r.serviceInfo,                   
             mAm.compatibilityInfoForPackageLocked(r.serviceInfo.applicationInfo),  app.repProcState);
        	 requestServiceBindingsLocked(r, execInFg);
      		 sendServiceArgsLocked(r, execInFg, true);
 
            }

继续往下看requestServiceBindingsLocked再调用ActivityThread的方法scheduleBindService，在ActivityThread.java 中，它发出一个BIND_SERVICE事件，被handleBindService处理，

    private void handleBindService(BindServiceData data) {
                    if (!data.rebind) {
                    <!--如果是第一次绑定-->
                        IBinder binder = s.onBind(data.intent);
                        ActivityManagerNative.getDefault().publishService(
                                data.token, data.intent, binder);
                    } else {
                    
                        s.onRebind(data.intent);
                        ActivityManagerNative.getDefault().serviceDoneExecuting(
                                data.token, 0, 0, 0);
                    }

这里先调用Service服务的onBind方法，因为服务是重载的，所以会执行具体服务类的方法，并返回服务里的binder实例，被转换后返回到AMS中，AMS继续调用publishService方法，进而调用ActiveServices.java的publishServiceLocked，

    void publishServiceLocked(ServiceRecord r, Intent intent, IBinder service) {
                    for (int conni=r.connections.size()-1; conni>=0; conni--) {
                        ArrayList<ConnectionRecord> clist = r.connections.valueAt(conni);
                        for (int i=0; i<clist.size(); i++) {
                            ConnectionRecord c = clist.get(i);
                          try {
                                c.conn.connected(r.name, service);
                             }
 
                serviceDoneExecutingLocked(r, mDestroyingServices.contains(r), false);
                
这里主要调用到c.conn.connected，c就是ConnectionRecord，其成员conn是一个IServiceConnection类型实例，connected则是其实现类的方法，这里其实也是一套基于binder通信proxy与Stub，IServiceConnection是采用aidl定义的一个接口，位置在core/java/Android/app/IServiceConnection.aidl，aidl定义如下，只有一个接口方法connected：

		oneway interface IServiceConnection {
		    void connected(in ComponentName name, IBinder service);
		}
 
其服务端的实现在LoadedApk.java，InnerConnection类是在ServiceDispatcher的内部类，并在ServiceDispatcher的构造函数里面实例化的，其方法connected也是调用的ServiceDispatcher的方法connected，

        private static class InnerConnection extendsIServiceConnection.Stub {
            final WeakReference<LoadedApk.ServiceDispatcher> mDispatcher;
 
            InnerConnection(LoadedApk.ServiceDispatcher sd) {
                mDispatcher = new WeakReference<LoadedApk.ServiceDispatcher>(sd);
            }
 
            public void connected(ComponentName name, IBinder service) throws RemoteException {
                LoadedApk.ServiceDispatcher sd = mDispatcher.get();
                if (sd != null) {
                    sd.connected(name, service);
                }
            }
        }
 
        ServiceDispatcher(ServiceConnection conn,
                Context context, Handler activityThread, int flags) {
            mIServiceConnection = new InnerConnection(this);
            mConnection = conn;
            mContext = context;
            mActivityThread = activityThread;
            mLocation = new ServiceConnectionLeaked(null);
            mLocation.fillInStackTrace();
            mFlags = flags;
        }
 
这里就再回到我们前面的ContextImpl里面bindServiceCommon方法里面，这里进行ServiceConnection转化为IServiceConnection时，调用了mPackageInfo.getServiceDispatcher，mPackageInfo就是一个LoadedApk实例，

    /*package*/ LoadedApk mPackageInfo;
 
    private boolean bindServiceCommon(Intent service, ServiceConnection conn, int flags,
            UserHandle user) {
        IServiceConnection sd;
 
            sd = mPackageInfo.getServiceDispatcher(conn, getOuterContext(),
                    mMainThread.getHandler(), flags);
}
 
所以，getServiceDispatcher会创建一个ServiceDispatcher实例，并将ServiceDispatcher实例和ServiceConnection实例形成KV对，并在ServiceDispatcher的构造函数里将ServiceConnection实例c赋值给ServiceConnection的成员变量mConnection，

    public final IServiceConnection getServiceDispatcher(ServiceConnection c,
            Context context, Handler handler, int flags) {
        synchronized (mServices) {
            LoadedApk.ServiceDispatcher sd = null;
            ArrayMap<ServiceConnection, LoadedApk.ServiceDispatcher> map = mServices.get(context);
            if (map != null) {
                sd = map.get(c);
            }
            if (sd == null) {
                sd = new ServiceDispatcher(c, context, handler, flags);
                if (map == null) {
                    map = new ArrayMap<ServiceConnection, LoadedApk.ServiceDispatcher>();
                    mServices.put(context, map);
                }
                map.put(c, sd);
         }
 
在执行ServiceDispatcher的connected方法时，就会调用到ServiceConnection的onServiceConnected，完成绑定ServiceConnection的触发。

        public void doConnected(ComponentName name, IBinder service) {

            if (old != null) {
                mConnection.onServiceDisconnected(name);
            }
            // If there is a new service, it is now connected.
            if (service != null) {
                mConnection.onServiceConnected(name, service);
            }
            }
 

##### AIDL总结

binderService其实是通过AMS进行中转，如果Service没启动，就启动Service，之后进行Publish将新进程的Bidner的代理转发给各个端口，谁需要发给谁，具体流程如下图：

<img src="http://happylishang.github.io/images/android/binder/binderService.png" height=250 width=600/>

* 1、Activity调用bindService函数通知ActivityManagerService，要启动Service这个服务
* 2、ActivityManagerService创建Servicerecord，并且ApplicationThreadProxy回调，在MainActivity所在的进程内部把Service启动起来，并且调用它的onCreate函数； 
* 3、ActivityManagerService把Service启动起来后，继续调用onBind函数，让Service返回一个Binder对象给它，以便AMS传递给Activity
* 4、ActivityManagerService把从Service处得到这个Binder对象传给Activity，即把这个Binder对象作为参数传递给Activity内部定义的ServiceConnection对象的onServiceConnected函数；这里是通过IserviceConnection binder实现。
* 5、Activity内部定义的ServiceConnection对象的onServiceConnected函数在得到这个Binder对象后，就通过它的getService成同函数获得CounterService接口，封装跟拆解       
* 6、Java层的mRemote本身都是BinderProxy


# 参考文档：

[android4.4组件分析--service组件-bindService源码分析](http://blog.csdn.net/xiashaohua/article/details/40424767)         
[从源码出发深入理解 Android Service](http://www.woaitqs.cc/android/2016/09/20/android-service-usage.html)             
[Android系统进程间通信Binder机制在应用程序框架层的Java接口源代码分析](http://blog.csdn.net/luoshengyang/article/details/6642463)