Android开发中经常遇到Context，弹出个对话框、获取资源、打开新的Activity等都需要依赖Context，Context到底是什么，能做什么呢？本文就来简单的跟踪下，先看下Context的定义

>Interface to global information about an application environment. This is an abstract class whose implementation is provided by the Android system. It allows access to application-specific resources and classes, as well as up-calls for application-level operations such as launching activities, broadcasting and receiving intents, etc.

简化点：Context是这样的一个抽象接口，承载应用程序的想关信息，同时也可以用来访问应用声明的资源及相应类、启动Activity、广播等。Context直译为"上下文", 对于面向对象编程来说，可以看做成一个场景抽象，就像普通的OOP思想一样，包含两部分，一：包含一个场景的全部信息（场景的名字、资源等）：

	abstract String	getPackageResourcePath()  //包资源路径
	abstract Resources	getResources()         //使用的资源

二：包含这个场景所能做的事情

	abstract ComponentName	startService(Intent service) //启动服务
	abstract boolean	stopService(Intent service)         //停止服务

Context就是一个运行场景的抽象，通过它可以知道当前运行场景的各种信息，并且执行该场景所能做的事情。

# 不同的Context场景及新建时机

既然是场景，那必定分为不同类型，比如悲剧场景、喜剧场景，就喜剧场景而言，就必然有喜剧演员，以及这个场景如何让观众发笑的方法，对应到Android中来，Activity是一个场景、Service是一个场景，前者代表一个前台显示场景，后者代表一个后台运行的场景，对于前台显示的场景，需要关心窗口的添加删除、View的绘制等，而对于后台运行的场景显然是不需要关心这些的，不过，两者的都需要访问资源，因此这两个场景都必须具备获取资源的能力.看一下相应的类图：

![Context场景.png](http://upload-images.jianshu.io/upload_images/1460468-33cb5a363d47d174.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

Context是一个抽闲类，ContextImpl和ContextWrapper是它的两个实现子类，不过ContextWrapper只是一个包装，真正的实现类只有ContextImpl一个，ContextWrapper中有一个指向Context的引用，所有Context函数调用的归宿都是ContextImpl，只有它有具体的实现
	
	public class ContextWrapper extends Context {
	    Context mBase;
	
	    public ContextWrapper(Context base) {
	        mBase = base;
	    }
	    @Override
	    public void sendBroadcast(Intent intent) {
	        mBase.sendBroadcast(intent);
	    }
	    @Override
	    public void sendBroadcast(Intent intent, String receiverPermission) {
	        mBase.sendBroadcast(intent, receiverPermission);
	    }

可以看到ContextWrapper都是引用其mBase的对应函数，mBase可以通过attachBaseContext来绑定，ContextThemeWrapper类实现了与显示主题相关的功能，只有Activity需要主题，而Service及Application都跟界面显示无关，因此不需要主题，Activity、Application、Service在初始化都会直接创建ContextImpl对象，并通过attachBaseContext绑定给自己。

	private Activity performLaunchActivity(ActivityClientRecord r, Intent customIntent) {
	    ...
	    java.lang.ClassLoader cl = r.packageInfo.getClassLoader();
	    <!--创建Activity对象-->
	    Activity activity = mInstrumentation.newActivity(cl, component.getClassName(), r.intent);
	    <!--创建Application对象-->
	    Application app = r.packageInfo.makeApplication(false, mInstrumentation);
	    if (activity != null) {
	        <!--创建ContextImpl对象-->
	        Context appContext = createBaseContextForActivity(r, activity);
	        <!--绑定-->
	        activity.attach(appContext, this, getInstrumentation(), r.token,r.ident, app,...);
	        ...
	}

	 private Context createBaseContextForActivity(ActivityClientRecord r,
	            final Activity activity) {
	        <!--为Activity创建ContextImpl-->
	        ContextImpl appContext = new ContextImpl();
	        appContext.init(r.packageInfo, r.token, this);
	        appContext.setOuterContext(activity);
	        Context baseContext = appContext;
	        ...
	        return baseContext;
	    }
    
        final void attach(Context context, ActivityThread aThread,
            Instrumentation instr, IBinder token, int ident, Application application,...) {
            <!--绑定-->
        attachBaseContext(context);
        ...
        }
        
可以看到在新建Actiivty的同时会为其创建ContextImpl，并绑定到Activity内部，对于Service，步骤类似。

    private void handleCreateService(CreateServiceData data) {
        ...
        try {
        	  <!--创建ContextImpl-->
            ContextImpl context = new ContextImpl();
            context.init(packageInfo, null, this);
            Application app = packageInfo.makeApplication(false, mInstrumentation);
            context.setOuterContext(service);
            <!--绑定-->
            service.attach(context, this, data.info.name, data.token, app,ActivityManagerNative.getDefault());
     	..
     }               

Application代表的不能算一个具体场景，它代表整个应用，并非单个界面或者后台场景，它只能再应用整体层面应用，比如创建一个全局的Toast，启动新的Activity、Service，获取资源等，所有Context的getApplicationContext返回的都是Application对象，并且每个进程只有一个：

    public Application makeApplication(boolean forceDefaultAppClass,
            Instrumentation instrumentation) {
         <!--单利-->
        if (mApplication != null) { return mApplication; }
        Application app = null;
        String appClass = mApplicationInfo.className;
        if (forceDefaultAppClass || (appClass == null)) {
            appClass = "android.app.Application"; }
        try {
            java.lang.ClassLoader cl = getClassLoader();
            <!--创建ContextImpl-->
            ContextImpl appContext = new ContextImpl();
            appContext.init(this, null, mActivityThread);
            <!--创建Application对象,并绑定-->
            app = mActivityThread.mInstrumentation.newApplication(
                    cl, appClass, appContext);
            appContext.setOuterContext(app);
        } catch (Exception e) {
        }
        ...
        return app;
    }

# 不同的Context场景对应的属性及能做的事

获取资源是每个Context场景都需要的，不同的资源定义在不同的位置，比如字符串常定义在res/values/strings.xml中

	string，Color，dimen 常量资源
	Drawable 图
	layout 布局 
	style 样式  
	
简单看一下资源获取流程，基类Context中定义了这么一个抽象函数

    public abstract Resources getResources();

具体实现在CotextImpl， 

	Resources mResources;
    @Override
    public Resources getResources() {
        return mResources;
    }

mResources赋值是在init初始化函数中，

    final void init(LoadedApk packageInfo, IBinder activityToken, ActivityThread mainThread,
            Resources container, String basePackageName, UserHandle user) {
        mPackageInfo = packageInfo;
        mBasePackageName = basePackageName != null ? basePackageName : packageInfo.mPackageName;
        mResources = mPackageInfo.getResources(mainThread);
     ...
     }

	  Resources getTopLevelResources(String resDir,...);
	        Resources r;
	        synchronized (mPackages) {
	            WeakReference<Resources> wr = mActiveResources.get(key);
	            r = wr != null ? wr.get() : null;
	        }
			 ...	       
	        r = new Resources(assets, dm, config, compInfo);
	        ...
	            mActiveResources.put(key, new WeakReference<Resources>(r));
	            return r;
	        }
	    }
	    
	    
创建Resources需要一个AssetManager对象。在开发应用程序时，使用Resources.getAssets()获取的就是这里创建的AssetManager对象。AssetManager其实并不只是访问res/assets目录下的资源，而是可以访问res目录下的所有资源。

AssetManager在初始化的时候会被赋予两个路径，一个是应用程序资源路径 /data/app/xxx.apk，一个是Framework资源路径/system/framework/framework-res.apk(系统资源会被打包到此apk中）。所以应用程序使用本地Resources既可访问应用程序资源，又可访问系统资源。

AssetManager中很多获取资源的关键方法都是native实现，当使用getXXX(int id)访问资源时，如果id小于0x1000 0000时表示访问系统资源，如果id都大于0x7000 0000则表示应用资源。aapt在对系统资源进行编译时，所有资源id都被编译为小于0x1000 0000。

当创建好Resources后就把该对象放到mActivieResources中以便以后继续使用


# 启动Activity或者服务
 
# 弹出对话框


	    
# 参考文档

[理解Android Context](http://gityuan.com/2017/04/09/android_context/)       