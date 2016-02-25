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

###### 首先为什么需要aidl？

下面是不需要aidl 的binder的IPC通讯过程，表面上结构很简单，但是有个困难就是，客户端和服务端进行通讯，你得先将你的通讯请求转换成序列化的数据，然后调用transact（）函数发送给服务端，而且还得制定一个小协议，参数谁先谁后，服务端和客户端都必须一致，否则就会出错。这样的过程有没有觉的很麻烦，如果有上百个接口，那可就要疯掉了。可不可以就像调用自家函数那样呢？而不需要麻烦的将参数值转化成序列化数据呢？由此AIDL诞生了。

######  aidl定义Java层AIDL（Android Interface Definition Language，其实就是基于Binder框架的一种实现语言。在进行编译的时候，就已经对Binder的实现进行一系列的封装，生成的IxxxxService以及内部IxxxxService.Proxy类都是对Binder封装的一种体现。AIDL的最终效果就是让 IPC的通讯就像调用函数那样简单。自动的帮你完成了参数序列化发送以及解析返回数据的那一系列麻烦。而你所需要做的就是写上一个接口文件，然后利用aidl工具转化一下得到另一个java文件，这个文件在服务和客户端程序各放一份。服务程序继承IxxxxService.Stub 然后将函数接口里面的逻辑代码实现一下。
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
        AIDL的最终效果就是让 IPC的通讯就像调用函数那样简单。自动的帮你完成了参数序列化发送以及解析返回数据的那一系列麻烦。而你所需要做的就是写上一个接口文件，然后利用aidl工具转化一下得到另一个java文件，这个文件在服务和客户端程序各放一份。服务程序继承IxxxxService.Stub 然后将函数接口里面的逻辑代码实现一下。##### 7.2 Android Java层App天然支持Binder通信的原理Java层Server的实现，首先你要清楚Android Java层程序在建立之初，就已经实现了onTransact与Loop，也就是说，Java层默认已经打通了Binder通路，我们要做的只是基于这条通路实现业务逻辑，那么是怎么通的呢？当然，你自己利用JNI实现一套也可以，只是有必要吗？放着现成的不用。Android的应用程序包括Java应用及本地应用，Java应用运行在davik虚拟机中，由zygote进程来创建启动，而本地服务应用在Android系统启动时，通过配置init.rc文件来由Init进程启动。无论是Android的Java应用还是本地服务应用程序，都支持Binder进程间通信机制， 在zygote启动Android应用程序时，会调用zygoteInit函数来初始化应用程序运行环境，比如虚拟机堆栈大小，Binder线程的注册等。
		public static final void zygoteInit(int targetSdkVersion, String[] argv)				throws ZygoteInit.MethodAndArgsCaller {			redirectLogStreams();			commonInit();			//启动Binder线程池以支持Binder通信			nativeZygoteInit();			applicationInit(targetSdkVersion, argv);		}
		nativeZygoteInit函数用于创建线程池，该函数是一个本地函数，其对应的JNI函数为frameworks\base\core\jni\AndroidRuntime.cpp 

		static void com_android_internal_os_RuntimeInit_nativeZygoteInit(JNIEnv* env, jobject clazz)  			{  			    gCurRuntime->onZygoteInit();  			}  
变量gCurRuntime的类型是AndroidRuntime，AndroidRuntime类的onZygoteInit()函数是一个虚函数，在AndroidRuntime的子类AppRuntime中被实现frameworks\base\cmds\app_process\App_main.cpp 	virtual void onZygoteInit()  	{ 	    sp<ProcessState> proc = ProcessState::self();  	    ALOGV("App process: starting thread pool.\n");  	    proc->startThreadPool();  	}  函数首先得到ProcessState对象，然后调用它的startThreadPool()函数来启动线程池。
	void ProcessState::startThreadPool()  {  	    AutoMutex _l(mLock);  	    if (!mThreadPoolStarted) {  	        mThreadPoolStarted = true;  	        spawnPooledThread(true);  	    }  	}
	

##### 7.3 binderService背景与原理

###### bindService()用法


* 本地Server


* 远程Server


###### bindService()背景与初衷

绑定服务，启动服务，是动态服务的一种，不能所有的App的服务都要注册SVM中并运行吧？

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

##### 7.4 本地Service的实现方式JNI

##### 7.5 ActivityManagerService没有继承Service如何处理的服务呢

即使是在Java层，基于Binder通信，并不一定要继承Service类，而且启动的时候，还没有Service跟Activity的概念呢，他们 的框架还没有搭建起来呢。而且ActivityManagerService是有系统服务启动的，启动方式也不同，并且AMS是长留Servic，不是动态服务。

不用上层的那些封装。