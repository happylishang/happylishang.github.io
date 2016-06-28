---
layout: default
title: "Binder入门与深入"
description: "Java"
categories: [android,Binder]
tags: [Binder]

---

分析原理的步骤 

* 实现初衷
* 实现方式
* 使用方式

### 7、Java层最Binder的支持（Client与Server分析）

#### 7.1 Java层的使用方式，看下层实现的支持。为什么aidl，就够了？

####     ActivityThread::scheduleBindService()

binderService 之后会将onBinder返回的Binder注册到binder驱动，但是这里并不一定要注册到ServiceManager，因为你不通过getService获取service，其实只要获取binder，就可以，AMS也可以返回，还独立，不然会乱。这里也说明。一个Binder Thread 可以对英国各Binder实体。


    

###### 首先为什么需要aidl？

下面是不需要aidl 的binder的IPC通讯过程，

![](http://img.blog.csdn.net/20130703113256593)

表面上结构很简单，但是有个困难就是，客户端和服务端进行通讯，你得先将你的通讯请求转换成序列化的数据，然后调用transact（）函数发送给服务端，而且还得制定一个小协议，参数谁先谁后，服务端和客户端都必须一致，否则就会出错。这样的过程有没有觉的很麻烦，如果有上百个接口，那可就要疯掉了。可不可以就像调用自家函数那样呢？而不需要麻烦的将参数值转化成序列化数据呢？由此AIDL诞生了。

######  AIDL定义Java层AIDL（Android Interface Definition Language，其实就是基于Binder框架的一种实现语言。在进行编译的时候，就已经对Binder的实现进行一系列的封装，生成的IxxxxService以及内部IxxxxService.Proxy类都是对Binder封装的一种体现。AIDL的最终效果就是让 IPC的通讯就像调用函数那样简单。自动的帮你完成了参数序列化发送以及解析返回数据的那一系列麻烦。而你所需要做的就是写上一个接口文件，然后利用aidl工具转化一下得到另一个java文件，这个文件在服务和客户端程序各放一份。服务程序继承IxxxxService.Stub 然后将函数接口里面的逻辑代码实现一下。这里可以看到明显的Server与Client架构
	//server 端	public interface ICatService extends android.os.IInterface {
	    /** Local-side IPC implementation stub class. */
	    public static abstract class Stub extends android.os.Binder implements org.crazyit.service.ICatService {
	        private static final java.lang.String DESCRIPTOR = "org.crazyit.service.ICatService";
	
	        /** Construct the stub at attach it to the interface. */
	        public Stub() {
	            this.attachInterface(this, DESCRIPTOR);
	        }

	//client 端    private static class Proxy implements org.crazyit.service.ICatService {
            private android.os.IBinder mRemote;
    
    final class BinderProxy implements IBinder {
    public native boolean pingBinder();
    public native boolean isBinderAlive();
    
    public IInterface queryLocalInterface(String descriptor) {
        return null;
    }
通信后，
		IxxxxService.Stub.asInterface(IBinder obj);
这个函数是干啥用呢？首先当bindService之后，客户端会得到一个Binder引用，是Binder 哟，不是IxxxxService.Proxy实例，想要使用就必须基于Binder实例化出一个IxxxxService.Proxy。如果服务端和客户端都是在同一个进程呢，还需要利用IPC吗？这样就不需要了，直接将IxxxxService当做普通的对象调用就成了。Google 的同志们他们利用IxxxxService.Stub.asInterface函数对这两种不同的情况进行了统一，也就是不管你是在同一进程还是不同进程，那么在拿到Binder引用后，调用IxxxxService.Stub.asInterface(IBinder obj) 即可得到一个IxxxxService 实例，然后你只管调用IxxxxService里的函数就成了。
        /**
         * Cast an IBinder object into an org.crazyit.service.ICatService
         * interface, generating a proxy if needed.
         */
        public static org.crazyit.service.ICatService asInterface(android.os.IBinder obj) {
            if ((obj == null)) {
                return null;
            }
            android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
            if (((iin != null) && (iin instanceof org.crazyit.service.ICatService))) {
                return ((org.crazyit.service.ICatService) iin);
            }
            return new org.crazyit.service.ICatService.Stub.Proxy(obj);
        }
        AIDL的最终效果就是让 IPC的通讯就像调用函数那样简单。自动的帮你完成了参数序列化发送以及解析返回数据的那一系列麻烦。而你所需要做的就是写上一个接口文件，然后利用aidl工具转化一下得到另一个java文件，这个文件在服务和客户端程序各放一份。服务程序继承IxxxxService.Stub 然后将函数接口里面的逻辑代码实现一下。

              ##### 7.2 Android Java层App天然支持Binder通信的原理Java层Server的实现，首先你要清楚Android Java层程序在建立之初，就已经实现了onTransact与Loop，也就是说，Java层默认已经打通了Binder通路，我们要做的只是基于这条通路实现业务逻辑，那么是怎么通的呢？当然，你自己利用JNI实现一套也可以，只是有必要吗？放着现成的不用。Android的应用程序包括Java应用及本地应用，Java应用运行在davik虚拟机中，由zygote进程来创建启动，而本地服务应用在Android系统启动时，通过配置init.rc文件来由Init进程启动。无论是Android的Java应用还是本地服务应用程序，都支持Binder进程间通信机制， 在zygote启动Android应用程序时，会调用zygoteInit函数来初始化应用程序运行环境，比如虚拟机堆栈大小，Binder线程的注册等。
		public static final void zygoteInit(int targetSdkVersion, String[] argv)				throws ZygoteInit.MethodAndArgsCaller {			redirectLogStreams();			commonInit();			//启动Binder线程池以支持Binder通信			nativeZygoteInit();			applicationInit(targetSdkVersion, argv);		}
		nativeZygoteInit函数用于创建线程池，该函数是一个本地函数，其对应的JNI函数为frameworks\base\core\jni\AndroidRuntime.cpp 

		static void com_android_internal_os_RuntimeInit_nativeZygoteInit(JNIEnv* env, jobject clazz)  			{  			    gCurRuntime->onZygoteInit();  			}  
变量gCurRuntime的类型是AndroidRuntime，AndroidRuntime类的onZygoteInit()函数是一个虚函数，在AndroidRuntime的子类AppRuntime中被实现frameworks\base\cmds\app_process\App_main.cpp 	virtual void onZygoteInit()  	{ 	    sp<ProcessState> proc = ProcessState::self();  	    ALOGV("App process: starting thread pool.\n");  	    proc->startThreadPool();  	}  函数首先得到ProcessState对象，然后调用它的startThreadPool()函数来启动线程池。
	void ProcessState::startThreadPool()  {  	    AutoMutex _l(mLock);  	    if (!mThreadPoolStarted) {  	        mThreadPoolStarted = true;  	        spawnPooledThread(true);  	    }  	}
	

##### 7.3 binderService背景与原理

为何要借助Service的壳子呢？是方便管理还是怎么回事，为何要bindService，Service只是个容器，其中基于aidl实现的IXXService才是真正的服务，Service中主要就是初始化，提供绑定操作，已经解绑，结束一些服务的操作。

###### bindService()用法


* 本地Server


* 远程Server


###### bindService()背景与初衷

绑定服务，启动服务，是动态服务的一种，不能所有的App的服务都要注册SVM中并运行吧？ 
把Service类加载到内存中来，然后调用它的onCreate函数。



###### bindService()原理

首先看一下binderService的源码：

    @Override
    public boolean bindService(Intent service, ServiceConnection conn,
            int flags) {
        IServiceConnection sd;
        if (mPackageInfo != null) {
            sd = mPackageInfo.getServiceDispatcher(conn, getOuterContext(),
                    mMainThread.getHandler(), flags);
        } else {
            throw new RuntimeException("Not supported in system context");
        }
        try {
            int res = ActivityManagerNative.getDefault().bindService(
                mMainThread.getApplicationThread(), getActivityToken(),
                service, service.resolveTypeIfNeeded(getContentResolver()),
                sd, flags);
            if (res < 0) {
                throw new SecurityException(
                        "Not allowed to bind to service " + service);
            }
            return res != 0;
        } catch (RemoteException e) {
            return false;
        }
    }
    
bindServic并不能保证onServiceConnected及时执行，也就是可能还没有连接成功，这里牵扯到双向回调的问题。两者在大方向上可以看做是异步的：

    private ServiceConnection regConn=new ServiceConnection() {
                
                @Override
                public void onServiceDisconnected(ComponentName name) {
                        iservice=null;
                }
                
                @Override
                public void onServiceConnected(ComponentName name, IBinder service)
                   {
                        iservice=IService.Stub.asInterface(service);
                        try {
                                result=iservice.appRegist(100, "app1");
                        } catch (RemoteException e) {
                                e.printStackTrace();
                        }finally{
                                unbindService(this);
                        }
                }
        };
      }

如果向下面的用法，result是无法得到正确结果的

	Intent intent=new Intent("com.demo.aidl.START_SERVICE");
	                                bindService(intent, regConn, BIND_AUTO_CREATE);
	                                
	                                //输出注册结果
	                                System.out.println(result);
	                                
当然，Java层使用Service也不是非得binderService，比如使用系统Service的时候，就不用这么操作，我们只是用自己实现的java层Service的时候，习惯这么做，其实这是一个穿插的问题，本地的带着本端的Binder实体去访问Server，Server处理完毕后，根据Server端生成的Client的代理去访问客户端，这个时候，可以把客户端看成Server，其实是一个自带返回属性的请求，访问的同时，将后门留给了Server端。这么做的原因是什么？为什么一定要绑定？因为没有在ServiceManager中注册，所以不能查询的到吗？

IServiceConnection.Stub 其实也是一条BInder体系，InnerConnection对象是一个Binder对象，一会是要传递给ActivityManagerService的，ActivityManagerServic后续就是要通过这个Binder对象和ServiceConnection通信的。
        private static class InnerConnection extends IServiceConnection.Stub {  
            final WeakReference<LoadedApk.ServiceDispatcher> mDispatcher;  
            ......  
  
            InnerConnection(LoadedApk.ServiceDispatcher sd) {  
                mDispatcher = new WeakReference<LoadedApk.ServiceDispatcher>(sd);  
            }  
  
            ......  
        }  
来看看bind函数
 
     public int bindService(IApplicationThread caller, IBinder token,  
            Intent service, String resolvedType, IServiceConnection connection,  
            int flags
        
                
        data.writeStrongBinder(connection.asBinder());  

接着通过retrieveServiceLocked函数，得到一个ServiceRecord，这个ServiceReocrd描述的是一个Service对象，这里就是CounterService了，这是根据传进来的参数service的内容获得的。回忆一下在MainActivity.onCreate函数绑定服务的语句：

	Intent bindIntent = new Intent(MainActivity.this, CounterService.class);  
	bindService(bindIntent, serviceConnection, Context.BIND_AUTO_CREATE);  
	
这里的参数service，就是上面的bindIntent了，它里面设置了CounterService类的信息（CounterService.class），因此，这里可以通过它来把CounterService的信息取出来，并且保存在ServiceRecord对象s中。
接下来，就是把传进来的参数connection封装成一个ConnectionRecord对象。注意，这里的参数connection是一个Binder对象，它的类型是LoadedApk.ServiceDispatcher.InnerConnection，是在Step 4中创建的，后续ActivityManagerService就是要通过它来告诉MainActivity，CounterService已经启动起来了，因此，这里要把这个ConnectionRecord变量c保存下来，它保在在好几个地方，都是为了后面要用时方便地取回来的，这里就不仔细去研究了，只要知道ActivityManagerService要使用它时就可以方便地把它取出来就可以了. 我们先沿着app.thread.scheduleCreateService这个路径分析下去，然后再回过头来分析requestServiceBindingsLocked的调用过程。这里的app.thread是一个Binder对象的远程接口，类型为ApplicationThreadProxy。每一个Android应用程序进程里面都有一个ActivtyThread对象和一个ApplicationThread对象，其中是ApplicationThread对象是ActivityThread对象的一个成员变量，是ActivityThread与ActivityManagerService之间用来执行进程间通信的，

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
    
    
    * 1. MainActivity调用bindService函数通知ActivityManagerService，它要启动Service这个服务，ActivityManagerService创建Servicerecord，并且ApplicationThreadProxy回调，在MainActivity所在的进程内部把CounterService启动起来，并且调用它的onCreate函数；
 
* 2. ActivityManagerService把Service启动起来后，继续调用onBind函数，让CounterService返回一个Binder对象给它，以便AMS传递给Activity

* 3. ActivityManagerService把从Service处得到这个Binder对象传给MainActivity，即把这个Binder对象作为参数传递给MainActivity内部定义的ServiceConnection对象的onServiceConnected函数；这里是通过IserviceConnection binder实现。

* 4. MainActivity内部定义的ServiceConnection对象的onServiceConnected函数在得到这个Binder对象后，就通过它的getService成同函数获得CounterService接口，封装跟拆解	
* Java层的mRemote本身都是BinderProxy
	   
		   public native boolean transact(int code, Parcel data, Parcel reply,
		            int flags) throws RemoteException; 
	            

##### 7.4 ActivityManagerService没有继承Service如何处理的服务呢

    ActivityManagerService服务是由SystemServer启动的服务，加载main函数，
    	[-->SystemServer.java::ServerThread的run函数]

	
	public class SystemServer    
	{    
	    ......    
	  
	    native public static void init1(String[] args);    
	  
	    ......    
	  
	    public static void main(String[] args) {    
	        ......    
	  
	        init1(args);    
	  
	        ......    
	    }   
	  
	    public static final void init2() {    
	        Slog.i(TAG, "Entered the Android system server!");    
	        Thread thr = new ServerThread();    
	        thr.setName("android.server.ServerThread");    
	        thr.start();    
	    }
	    
	    class ServerThread extends Thread {
 
		    @Override
		    public void run() {
		   
	 
		        try {

		            context = ActivityManagerService.main(factoryTest);
	 
		            pm = PackageManagerService.main(context,
		                    factoryTest != SystemServer.FACTORY_TEST_OFF);
		
		            ActivityManagerService.setSystemProcess();
		            
	
  并且，即使是在Java层基于Binder通信也并不一定要继承Service类，而且，启动AMS的时候，还没有Service跟Activity的概念呢，他们的框架还没有搭建起来呢。而且ActivityManagerService是有系统服务SystemServer启动的，启动方式也不同，并且AMS是长留Servic，不是动态服务，因此不用上层的那些AIDL封装也是可以的。


##### 7.5 C++与Java Binder的转换

Java层客户端的Binder代理都是BinderProxy，而且他们都是在native层生成的，因此，在上层看不到BinderProxy实例化。

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

####     ActivityThread::scheduleBindService()函數其实是注册的服务的地方 其实是注册到AMS中，binderService是跟AMS交互而非ServiceManager


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
			    
####     参考文档

[android4.4组件分析--service组件-bindService源码分析](http://blog.csdn.net/xiashaohua/article/details/40424767)