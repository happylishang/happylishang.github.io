---
layout: post
title: Android窗口管理分析（2）：WindowManagerService图层管理之窗口的添加
category: Android
image: http://upload-images.jianshu.io/upload_images/1460468-a16747b74ea5a486.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---

之前有分析说过，WindowManagerService只负责窗口管理，并不负责View的绘制跟图层混合，本文就来分析WMS到底是怎么管理窗口的。初接触Android时感觉：Activity似乎就是Google封装好的窗口，APP只要合理的启动新的Activity就打开了新窗口，这样理解没什么不对，Activity确实可以看做一种窗口及View的封装，不过从源码来看，Activity跟Window还是存在不同。本文主要从以下几点分析WMS窗口管理：

* 窗口的分类：Activity、Dialog、PopupWindow、Toast等对应窗口的区别
* 窗口的添加与删除
* 窗口的分组与窗口的Z顺序
* Window、IWindow 、WindowState、WindowToken、AppToken等之间的关系

## 窗口的分类简述

在Android系统中，PopupWindow、Dialog、Activity、Toast等都有窗口的概念，但又各有不同，Android将窗口大致分为三类：应用窗口、子窗口、系统窗口。其中，Activity与Dialog属于应用窗口、PopupWindow属于子窗口，必须依附到其他非子窗口才能存在，而Toast属于系统窗口，Dialog可能比较特殊，从表现上来说偏向于子窗口，必须依附Activity才能存在，但是从性质上来说，仍然是应用窗口，有自己的WindowToken，不同窗口之间的关系后面会更加详细的分析，这里有一个概念即可。

![窗口组织形式.jpg](http://upload-images.jianshu.io/upload_images/1460468-14737360edacc3b3.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

## 窗口的添加

Activity并不是展示View视图的唯一方式，分析窗口添加流程的话，Activity也并不是最好的例子，因为Activity还会牵扯到AMS的只是，这里我们不用Activiyt，而是用一个悬浮View的展示来分析窗口的添加，理解之后，再针对Activity做一个简单梳理：
		
    private void addTextViewWindow(Context context){

        TextView mview=new TextView(context);
		...<!--设置颜色 样式-->
		<!--关键点1-->
        WindowManager mWindowManager = (WindowManager) context.getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
        WindowManager.LayoutParams wmParams = new WindowManager.LayoutParams();
        <!--关键点2-->
        wmParams.type = WindowManager.LayoutParams.TYPE_TOAST;
        wmParams.format = PixelFormat.RGBA_8888;
        wmParams.width = 800;
        wmParams.height = 800;
        <!--关键点3-->
        mWindowManager.addView(mview, wmParams);
    }
    
这有三点比较关键，关键点1：获取WindowManagerService服务的代理对象，不过对于Application而言，获取到的其实是一个封装过的代理对象，一个WindowManagerImpl实例，Application 的getSystemService(）源码其实是在ContextImpl中：有兴趣的可以看看APP启动时Context的创建：

	    @Override
	    public Object getSystemService(String name) {
	        return SystemServiceRegistry.getSystemService(this, name);
	    }

SystemServiceRegistry类用静态字段及方法中封装了一些服务的代理，其中就包括WindowManagerService

	    public static Object getSystemService(ContextImpl ctx, String name) {
	        ServiceFetcher<?> fetcher = SYSTEM_SERVICE_FETCHERS.get(name);
	        return fetcher != null ? fetcher.getService(ctx) : null;
	    }
	    
	    static {
	    		 ...
	             registerService(Context.WINDOW_SERVICE, WindowManager.class,
	                new CachedServiceFetcher<WindowManager>() {
	            @Override
	            public WindowManager createService(ContextImpl ctx) {
	                return new WindowManagerImpl(ctx.getDisplay());
	            }});
	            ...
	    }
    
因此context.getApplicationContext().getSystemService()最终可以简化为new WindowManagerImpl(ctx.getDisplay())，下面看下WindowManagerImpl的构造方法，它有两个实现方法，对于Activity跟Application其实是有区别的，这点后面分析：

    public WindowManagerImpl(Display display) {
        this(display, null);
    }

    private WindowManagerImpl(Display display, Window parentWindow) {
        mDisplay = display;
        mParentWindow = parentWindow;
    }
    
对于Application采用的是一参构造方法，所以其mParentWindow=null，这点后面会有用，到这里，通过getService获取WMS代理的封装类，接着看第二点，WindowManager.LayoutParams，主要看一个type参数，这个参数决定了窗口的类型，这里我们定义成**一个Toast窗口，属于系统窗口，不需要处理父窗口、子窗口之类的事**，更容易分析，最后看关键点3，利用WindowManagerImpl的addView方法添加View到WMS，


     @Override
    public void addView(@NonNull View view, @NonNull ViewGroup.LayoutParams params) {
        applyDefaultToken(params);
        mGlobal.addView(view, params, mDisplay, mParentWindow);
    }

不过很明显WindowManagerImpl最后是委托mGlobal来进行这项操作，WindowManagerGlobal是一个单利，一个进程只有一个：

    private final WindowManagerGlobal mGlobal = WindowManagerGlobal.getInstance();

接着看WindowManagerGlobal的addView，对于添加系统窗口，这里将将代码精简一下，不关系子窗口等之类的逻辑

    public void addView(View view, ViewGroup.LayoutParams params,
            Display display, Window parentWindow) {

        final WindowManager.LayoutParams wparams = (WindowManager.LayoutParams) params;
			<!--关键点1-->
			root = new ViewRootImpl(view.getContext(), display);
			view.setLayoutParams(wparams);
			mViews.add(view);
			mRoots.add(root);
			mParams.add(wparams);
        }
       <!--关键点2-->
        try {
            root.setView(view, wparams, panelParentView);
        }           
         ...  }

先看关键点1，在向WMS添加View的时候，WindowManagerGlobal首先为View新建了一个ViewRootImpl，ViewRootImpl可以看做也是Window和View之间的通信的纽带，比如将View添加到WMS、处理WMS传入的触摸事件、通知WMS更新窗口大小等、同时ViewRootImpl也封装了View的绘制与更新方法等。看一下ViewRootImpl如何通过setView将视图添加到WMS的：
	   
	 public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
	        synchronized (this) {
	            if (mView == null) {
	                mView = view;
					  ...
						<!--关键点1 -->
	                // Schedule the first layout -before- adding to the window
	                // manager, to make sure we do the relayout before receiving
	                // any other events from the system.
	                requestLayout();
	                if ((mWindowAttributes.inputFeatures
	                        & WindowManager.LayoutParams.INPUT_FEATURE_NO_INPUT_CHANNEL) == 0) {
	                    mInputChannel = new InputChannel();
	                }
	                try {
						<!--关键点2 -->
	                    res = mWindowSession.addToDisplay(mWindow, mSeq, mWindowAttributes,
	                            getHostVisibility(), mDisplay.getDisplayId(),
	                            mAttachInfo.mContentInsets, mAttachInfo.mStableInsets,
	                            mAttachInfo.mOutsets, mInputChannel);
	                } catch (RemoteException e) {
	               ...

先看关键点1，这里是先为relayout占一个位置，其实是依靠Handler先发送一个Message，排在所有WMS发送过来的消息之前，先布局绘制一次，之后才会处理WMS传来的各种事件，比如触摸事件等，毕竟要首先将各个View的布局、位置处理好，才能准确的处理WMS传来的事件。接着看做关键点2，这里才是真正添加窗口的地方，虽然关键点1执行在前，但是用的是Handler发消息的方式来处理，其Runable一定是在关键点2之后执行，接着看关键点2，这里有个比较重要的对象mWindowSession与mWindow，两者都是在ViewRootImpl在实例化的时候创建的：

    public ViewRootImpl(Context context, Display display) {
        mContext = context;
        mWindowSession = WindowManagerGlobal.getWindowSession();
        mWindow = new W(this);
        
mWindowSession它是通过WindowManagerGlobal.getWindowSession获得的一个Binder服务代理，是App端向WMS发送消息的通道。相对的，mWindow是一个**W extends IWindow.Stub** Binder服务对象，其实可以看做是App端的窗口对象，主要作用是传递给WMS，并作为WMS向APP端发送消息的通道，在Android系统中存在大量的这种互为C\S的场景。接着看mWindowSession获取的具体操作是：首先通过getWindowManagerService 获取WMS的代理，之后通过WMS的代理在服务端open一个Session，并在APP端获取该Session的代理：

     public static IWindowSession getWindowSession() {
        synchronized (WindowManagerGlobal.class) {
            if (sWindowSession == null) {
                try {
                    InputMethodManager imm = InputMethodManager.getInstance();
                    <!--关键点1-->
                    IWindowManager windowManager = getWindowManagerService();
                    <!--关键点2-->
                    sWindowSession = windowManager.openSession(***）
                    ...
            return sWindowSession;
        }
    }
    
看关键点1 ：首先要记住sWindowSession是一个单例的对象，之后就可以将getWindowManagerService函数其实可以简化成下面一句代码，其实就是获得WindowManagerService的代理，之前的WindowManagerImpl都是一个壳子，或者说接口封装，并未真正的获得WMS的代理：

		IWindowManager.Stub.asInterface(ServiceManager.getService("window"))
    
再看关键点2：sWindowSession = windowManager.openSession，它通过binder驱动后，会通知WMS回调openSession，打开一个Session返回给APP端，而**Session extends IWindowSession.Stub** ，很明显也是一个Binder通信的Stub端，封装了每一个Session会话的操作。

    @Override
    public IWindowSession openSession(IWindowSessionCallback callback, IInputMethodClient client,
            IInputContext inputContext) {
        if (client == null) throw new IllegalArgumentException("null client");
        if (inputContext == null) throw new IllegalArgumentException("null inputContext");
        Session session = new Session(this, callback, client, inputContext);
        return session;
    }
    
到这里看到如何获取Session，下面就是利用Session来add一个窗口：其实是调用Session.java的addToDisplayWithoutInputChannel函数
                      
    @Override
    public int addToDisplay(IWindow window, int seq, WindowManager.LayoutParams attrs,
            int viewVisibility, int displayId, Rect outContentInsets, Rect outStableInsets,
            Rect outOutsets, InputChannel outInputChannel) {
        return mService.addWindow(this, window, seq, attrs, viewVisibility, displayId,
                outContentInsets, outStableInsets, outOutsets, outInputChannel);
    }
    
不过它又反过来去调用WMS的addWindow，**绕这么大一圈，并且APP端IWindowSession还是单例的，为什么不直接用WMS来处理呢？疑惑**，在WMS中addWindow又做了什么呢，就像名字写的，负责添加一个窗口，代码精简后如下：
	
	public int addWindow(Session session, IWindow client, int seq,
	            WindowManager.LayoutParams attrs, int viewVisibility, int displayId,
	            Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
	            InputChannel outInputChannel) {
	        ...
	        synchronized(mWindowMap) {
	        ...
	        <!--关键点1 不能重复添加-->
	            if (mWindowMap.containsKey(client.asBinder())) {
	                return WindowManagerGlobal.ADD_DUPLICATE_ADD;
	            }
	        <!--关键点2 对于子窗口类型的处理 1、必须有父窗口 2，父窗口不能是子窗口类型-->
	            if (type >= FIRST_SUB_WINDOW && type <= LAST_SUB_WINDOW) {
	                parentWindow = windowForClientLocked(null, attrs.token, false);
	                if (parentWindow == null) {
	                    return WindowManagerGlobal.ADD_BAD_SUBWINDOW_TOKEN;
	                }
	                if (parentWindow.mAttrs.type >= FIRST_SUB_WINDOW
	                        && parentWindow.mAttrs.type <= LAST_SUB_WINDOW) {
	                    return WindowManagerGlobal.ADD_BAD_SUBWINDOW_TOKEN;
	                }}
               ...
               boolean addToken = false;
				<!--关键点3 根据IWindow 获取WindowToken WindowToken是窗口分组的基础，每个窗口必定有一个分组-->
	            WindowToken token = mTokenMap.get(attrs.token);
	          <!--关键点4对于Toast类系统窗口，其attrs.token可以看做是null， 如果目前没有其他的类似系统窗口展示，token仍然获取不到，仍然要走新建流程-->
	            if (token == null) {
	            ...	   
	                token = new WindowToken(this, attrs.token, -1, false);
	                addToken = true;
	            } 
	            ...
				 <!--关键点5 新建WindowState，WindowState与窗口是一对一的关系，可以看做是WMS中与窗口的抽象实体-->
	            WindowState win = new WindowState(this, session, client, token,
	                    attachedWindow, appOp[0], seq, attrs, viewVisibility, displayContent);
	            ...
	            if (addToken) {
	                mTokenMap.put(attrs.token, token);
	            }
	            win.attach();
	            mWindowMap.put(client.asBinder(), win);
	            ...	
	           <!--关键点6-->
	          addWindowToListInOrderLocked(win, true);
	        return res;
	    }

这里有几个概念需要先了解下：

* IWindow：APP端窗口暴露给WMS的抽象实例，在ViewRootImpl中实例化，与ViewRootImpl一一对应，同时也是WMS向APP端发送消息的Binder通道。
* WindowState：WMS端窗口的令牌，与IWindow，或者说与窗口一一对应，是WMS管理窗口的重要依据。
* WindowToken：窗口的令牌，其实也可以看做窗口分组的依据，在WMS端，与分组对应的数据结构是WindowToken（窗口令牌），而与组内每个窗口对应的是WindowState对象，每块令牌（AppWindowToken、WindowToken）都对应一组窗口（WindowState），Activity与Dialog对应的是AppWindowToken，PopupWindow对应的是普通的WindowToken。
* AppToken：其实是ActivityRecord里面的IApplicationToken.Stub appToken 代理，也是ActivityClientRecord里面的token，可以看做Activity在其他服务（非AMS）的抽象


![WindowToken与WindowState关系.jpg](http://upload-images.jianshu.io/upload_images/1460468-8e9cfc3cc05ba860.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
	    
那么接着关键点1：一个窗口不能被添加两次，IWindow是一个Binder代理，在WMS端，一个窗口只会有一个IWindow代理，这是由Binder通信机制保证的，这个对象不能被添加两次，否则会报错。关键点2，如果是子窗口的话，父窗口必须已被添加，由于我们分析的是系统Toast窗口，可以先不用关心；关键点3，WindowManager.LayoutParams中有一个token字段，该字段标志着窗口的分组属性，比如Activity及其中的Dialog是复用用一个AppToken，Activity里的PopupWindow复用一个IWindow类型Token，其实就是Activity的ViewRootImpl里面创建的IWindow，而对于我们现在添加的Toast类系统窗口，并未设置其attrs.token，那即是null，其实所有的Toast类系统窗口的attrs.token都可以看做null，就算不是null，也会在WMS被强制设置为null。所以Toast类系统窗口必定复用一个WindowToken，也可以说所有的Toast类系统窗口都是位于同一分组，这也是因为该类型系统窗口太常用，而且为所有进程服务，直接用一个WindowToken管理更加快捷，毕竟快速新建与释放WindowToken也算是一种开销。假设到我们添加系统窗口的时候，没有任何系统窗口展示，是获取不到key=null的WindowToken的，要新建WindowToken，并且添加到全局的TokenMap中，而关键点5，其实就是新建窗口在WMS端的抽象实例：WindowState，它同窗口一一对应，详细记录了窗口的参数、Z顺序、状态等各种信息，新建只有会被放入全局的Map中，同时也会被附加到相应的WindowToken分组中去，到这里APP端向WMS注册窗口的流程就算走完了，不过只算完成了前半部分，WMS还需要向SurfaceFlinger申请Surface，才算完成真正的分配了窗口。在向SurfaceFlinger申请Surface之前，WMS端需要获得SF的代理，在WindowState对象创建后会利用 win.attach()函数为当前APP申请建立SurfaceFlinger的链接：

    void attach() {
        if (WindowManagerService.localLOGV) Slog.v(
        mSession.windowAddedLocked();
    }

    void windowAddedLocked() {
        if (mSurfaceSession == null) {
           // SurfaceSession新建
            mSurfaceSession = new SurfaceSession();
            mService.mSessions.add(this);
           ...
        }
        mNumWindow++;
    }
    
可以看到SurfaceSession对于Session来说是单利的，也就是与APP的Seesion一一对应，SurfaceSession所握着的SurfaceFlinger的代理其实就是SurfaceComposerClient，其实现如下：
     
	    public SurfaceSession() {
	        mNativeClient = nativeCreate();
	    }
	
		static jlong nativeCreate(JNIEnv* env, jclass clazz) {
		    SurfaceComposerClient* client = new SurfaceComposerClient();
		    client->incStrong((void*)nativeCreate);
		    return reinterpret_cast<jlong>(client);
		}
		
Session与APP进程是一一对应的，它会进一步为当前进程建立SurfaceSession会话，可以这么理解：Session是APP同WMS通信的通道，SurfaceSession是WMS为APP向SurfaceFlinger申请的通信通道，同样 SurfaceSession与APP也是一一对应的，既然是同SurfaceFlinger通信的信使，那么SurfaceSession就应该握着SurfaceFlinger的代理，其实就是SurfaceComposerClient里的ISurfaceComposerClient mClient对象，它是SurfaceFlinger为每个APP封装一个代理，也就是 **进程 <-> Session <-> SurfaceSession <-> SurfaceComposerClient <-> ISurfaceComposerClient(BpSurfaceComposerClient) **五者是一条线, 为什么不直接与SurfaceFlinger通信呢？大概为了SurfaceFlinger管理每个APP的Surface比较方便吧，这四个类的模型如下图：

![Session及SurfaceComposerClient类图](http://upload-images.jianshu.io/upload_images/1460468-459c0f1860b87606.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

至于ISurfaceComposerClient（BpSurfaceComposerClient） 究竟是怎么样一步步创建的，其实它是利用ComposerService这样一个单利对象为为每个APP在WMS端申请一个ISurfaceComposerClient对象，在WMS端表现为BpSurfaceComposerClient，在SurfaceFlinger端表现为BnSurfaceComposerClient，具体代码如下：

	SurfaceComposerClient::SurfaceComposerClient()
	    : mStatus(NO_INIT), mComposer(Composer::getInstance())
	{
	}
	// 单利的，所以只有第一次的时候采用
	void SurfaceComposerClient::onFirstRef() {
	    sp<ISurfaceComposer> sm(ComposerService::getComposerService());
	    if (sm != 0) {
	        sp<ISurfaceComposerClient> conn = sm->createConnection();
	        if (conn != 0) {
	            mClient = conn;
	            mStatus = NO_ERROR;
	        }
	    }
	}

	sp<ISurfaceComposerClient> SurfaceFlinger::createConnection()
	{
	    sp<ISurfaceComposerClient> bclient;
	    sp<Client> client(new Client(this));
	    status_t err = client->initCheck();
	    if (err == NO_ERROR) {
	        bclient = client;
	    }
	    return bclient;
	}

![SurfaceComposer类图](http://upload-images.jianshu.io/upload_images/1460468-d2345502904c7902.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
	
刚才说完成了前半部分，主要针对WMS的窗口管理，后半部分则是围绕Surface的分配来进行的，还记得之前ViewRootImpl在setView时候分了两步吗？虽然先调用requestLayout先执行，但是由于其内部利用Handler发送消息延迟执行的，所以可以看做requestLayout是在addWindow之后执行的，那么这里就看添加窗口之后，如何分配Surface的，requestLayout函数调用里面使用了Hanlder的一个小手段，那就是利用postSyncBarrier添加了一个Barrier（挡板），这个挡板的作用是阻塞普通的同步消息的执行，在挡板被撤销之前，只会执行异步消息，而requestLayout先添加了一个挡板Barrier，之后自己插入了一个异步任务mTraversalRunnable，其主要作用就是保证mTraversalRunnable在所有同步Message之前被执行，保证View绘制的最高优先级。具体实现如下：

    void scheduleTraversals() {
        if (!mTraversalScheduled) {
            mTraversalScheduled = true;

				<!--关键点1 添加塞子-->
            mTraversalBarrier = mHandler.getLooper().getQueue().postSyncBarrier();
            <!--关键点2 添加异步消息任务-->
            mChoreographer.postCallback(
                    Choreographer.CALLBACK_TRAVERSAL, mTraversalRunnable, null);
            ...

mTraversalRunnable任务的主要作用是：如果Surface未分配，则请求分配Surface，并测量、布局、绘图，其执行主体其实是performTraversals()函数，该函数包含了APP端View绘制大部分的逻辑, performTraversals函数很长，这里只简要看几个点,其实主要是关键点1：relayoutWindow：


    private void performTraversals() {
        		final View host = mView;
      			 ...
        if (mFirst || windowShouldResize || insetsChanged ||
                viewVisibilityChanged || params != null) {
                <!--关键点1 申请Surface或者重新设置参数-->
                relayoutResult = relayoutWindow(params, viewVisibility, insetsPending);
              <!--关键点2 测量-->
                    performMeasure(childWidthMeasureSpec, childHeightMeasureSpec);
                }        
              <!--关键点3 布局-->
                    performLayout(lp, desiredWindowWidth, desiredWindowHeight);
               <!--关键点4 更新window-->
                  try {
                    mWindowSession.setInsets(mWindow, insets.mTouchableInsets,
                            contentInsets, visibleInsets, touchableRegion);
                ...
              <!--关键点5 绘制-->
               performDraw();
               ...  
           }

relayoutWindow主要是通过mWindowSession.relayout向WMS申请或者更新Surface如下，这里只关心一个重要的参数mSurface，在Binder通信中mSurface是一个out类型的参数，也就是Surface内部的内容需要WMS端负责填充，并回传给APP端：

	   private int relayoutWindow(WindowManager.LayoutParams params, int viewVisibility,
	            boolean insetsPending) throws RemoteException {
	       ...
	        int relayoutResult = mWindowSession.relayout(
	                mWindow, mSeq, params, ...  mSurface);
	        ...
	        return relayoutResult;
	    }

看下到底relayout是如何想SurfaceFlinger申请Surface的。我们知道每个窗口都有一个WindowState与其对应，另外每个窗口也有自己的动画，比如入场/出厂动画，而WindowStateAnimator就是与WindowState的动画，为什么要提WindowStateAnimator，因为WindowStateAnimator是

	 public int relayoutWindow(Session session, IWindow client, int seq,... Surface outSurface) {
	         WindowState win = windowForClientLocked(session, client, false);
            WindowStateAnimator winAnimator = win.mWinAnimator;
	         <!--关键点1 -->
	           SurfaceControl surfaceControl = winAnimator.createSurfaceLocked();
	           if (surfaceControl != null) {
	         <!--关键点2 -->
                 outSurface.copyFrom(surfaceControl);
                    } else {
                        outSurface.release();
                    }

这里只看Surface创建代码，首先通过windowForClientLocked找到WindowState，利用WindowState的WindowStateAnimator成员创建一个SurfaceControl，SurfaceControl会调用native函数nativeCreate(session, name, w, h, format, flags)创建Surface，

	static jlong nativeCreate(JNIEnv* env, jclass clazz, jobject sessionObj,
	        jstring nameStr, jint w, jint h, jint format, jint flags) {
	    ScopedUtfChars name(env, nameStr);
		<!--关键点1-->
	    sp<SurfaceComposerClient> client(android_view_SurfaceSession_getClient(env, sessionObj));
	    <!--关键点2-->
	    sp<SurfaceControl> surface = client->createSurface(
	            String8(name.c_str()), w, h, format, flags);
	    surface->incStrong((void *)nativeCreate);
	    return reinterpret_cast<jlong>(surface.get());
	}
	
关键点1是取到SurfaceSession对象中SurfaceComposerClient对象，之后调用SurfaceComposerClient的createSurface方法进一步创建SurfaceControl，

	sp<SurfaceControl> SurfaceComposerClient::createSurface(
	        const String8& name,
	        uint32_t w,
	        uint32_t h,
	        PixelFormat format,
	        uint32_t flags)
	{
	    sp<SurfaceControl> sur;
	    if (mStatus == NO_ERROR) {
	        sp<IBinder> handle;
	        sp<IGraphicBufferProducer> gbp;
	        <!--关键点1 获取图层的关键信息handle, gbp-->
	        status_t err = mClient->createSurface(name, w, h, format, flags,
	                &handle, &gbp);
	         <!--关键点2 根据返回的图层关键信息 创建SurfaceControl对象-->
	        if (err == NO_ERROR) {
	            sur = new SurfaceControl(this, handle, gbp);
	        }
	    }
	    return sur;
	}

这里先看mClient->createSurface，SurfaceComposerClient的mClient其实是一个BpSurfaceComposerClient对象，它SurfaceFlinger端Client在WMS端的代理，因此创建Surface的代码还是在SurfaceFlinger服务端的Client对象中，这里有两个关键的变量sp<IBinder> handle与 sp<IGraphicBufferProducer> gbp，前者标志在SurfaceFlinger端的图层，后者用来创建GraphicBuffer，两者类型都是IBinder类型，同时也是需要SurfaceFlinger填充的对象，这两者是一个图层对应的最关键的信息：

	status_t Client::createSurface(
	        const String8& name,
	        uint32_t w, uint32_t h, PixelFormat format, uint32_t flags,
	        sp<IBinder>* handle,
	        sp<IGraphicBufferProducer>* gbp){
	    ...
	    <!--关键点2 这里并未直接创建 ，而是通过发送了一个MessageCreateLayer消息-->
	    sp<MessageBase> msg = new MessageCreateLayer(mFlinger.get(),
	            name, this, w, h, format, flags, handle, gbp);
	    mFlinger->postMessageSync(msg);
	    return static_cast<MessageCreateLayer*>( msg.get() )->getResult();
	}
	
Client 并不会直接新建图层，而是向SurfaceFlinger发送一个MessageCreateLayer消息，通知SurfaceFlinger服务去执行，其handler代码如下：

	 class MessageCreateLayer : public MessageBase {
	        SurfaceFlinger* flinger;
	        Client* client;
	  	        virtual bool handler() {
	            result = flinger->createLayer(name, client, w, h, format, flags,
	                    handle, gbp);
	            return true;
	        }
	    };
    
其实就是调用SurfaceFlinger的createLayer，创建一个图层，到这里才是真正的创建图层：

	
	status_t SurfaceFlinger::createLayer(
	        const String8& name,
	        const sp<Client>& client,
	        uint32_t w, uint32_t h, PixelFormat format, uint32_t flags,
	        sp<IBinder>* handle, sp<IGraphicBufferProducer>* gbp)
	{
	    if (int32_t(w|h) < 0) {
	        return BAD_VALUE;
	    }
	
	    status_t result = NO_ERROR;
	
	    sp<Layer> layer;
	  <!--关键点1 新建不同图层-->
	    switch (flags & ISurfaceComposerClient::eFXSurfaceMask) {
	        case ISurfaceComposerClient::eFXSurfaceNormal:
	            result = createNormalLayer(client,
	                    name, w, h, flags, format,
	                    handle, gbp, &layer);
	            break;
	        case ISurfaceComposerClient::eFXSurfaceDim:
	            result = createDimLayer(client,
	                    name, w, h, flags,
	                    handle, gbp, &layer);
	            break;
	        default:
	            result = BAD_VALUE;
	            break;
	    }
	
	    if (result != NO_ERROR) {
	        return result;
	    }
	   ...
	}
	
SurfaceFlinger会根据不同的窗口参数，创建不同类型的图层，这里只看一下createNormalLayer普通样式的图层，

	status_t SurfaceFlinger::createNormalLayer(const sp<Client>& client,
	        const String8& name, uint32_t w, uint32_t h, uint32_t flags, PixelFormat& format,
	        sp<IBinder>* handle, sp<IGraphicBufferProducer>* gbp, sp<Layer>* outLayer)
	{
	    // initialize the surfaces
	    switch (format) {
	    case PIXEL_FORMAT_TRANSPARENT:
	    case PIXEL_FORMAT_TRANSLUCENT:
	        format = PIXEL_FORMAT_RGBA_8888;
	        break;
	    case PIXEL_FORMAT_OPAQUE:
	        format = PIXEL_FORMAT_RGBX_8888;
	        break;
	    }
	    <!--关键点 1 -->
	    *outLayer = new Layer(this, client, name, w, h, flags);
	    status_t err = (*outLayer)->setBuffers(w, h, format, flags);
	    <!--关键点 2-->
	    if (err == NO_ERROR) {
	        *handle = (*outLayer)->getHandle();
	        *gbp = (*outLayer)->getProducer();
	    }
	  return err;
	}

可以看到 图层最终对应的是Layer，这里会新建一个Layer对象，Layer中包含着与这个图层对应的Handle及Producer对象，Handle可以看做是Surface的唯一性标识，不过好像没太大的作用，最多是一个标识，将来清理的时候有用。相比之下gbp = (*outLayer)->getProducer()比较重要，它实际是一个BufferQueueProducer对象，关系到共享内存的分配问题，后面会专门分析，这里到此打住，我们终于得到了一个图层对象，到这里之后，我们梳理一下，图层如何建立的：

* 首先APP端新建一个Surface图层的容器壳子，
* APP通过Binder通信将这个Surface的壳子传递给WMS，
* WMS为了填充Surface去向SurfaceFlinger申请真正的图层，
* SurfaceFlinger收到WMS请求为APP端的Surface分配真正图层
* 将图层相关的关键信息Handle及Producer传递给WMS

Layer建立之后，SurfaceFlinger会将图层标识信息Handle及Producer传递给WMS，WMS利用这两者创建一个SurfaceControl对象，之后再利用该对象创建Surface，具体代码如下：

    void getSurface(Surface outSurface) {
        outSurface.copyFrom(mSurfaceControl);
    }
	
    public void copyFrom(SurfaceControl other) {
    long surfaceControlPtr = other.mNativeObject;
    long newNativeObject = nativeCreateFromSurfaceControl(surfaceControlPtr);
    synchronized (mLock) {
        setNativeObjectLocked(newNativeObject);
    }
	}

可以看到Surface的拷贝函数其实就是直接修改Surface native对象指针值，native的Surface对象中包含mGraphicBufferProducer对象，很重要，会被传递给APP端。

	static jlong nativeCreateFromSurfaceControl(JNIEnv* env, jclass clazz,
	        jlong surfaceControlNativeObj) {
	
	    sp<SurfaceControl> ctrl(reinterpret_cast<SurfaceControl *>(surfaceControlNativeObj));
	    sp<Surface> surface(ctrl->getSurface());
	    if (surface != NULL) {
	        surface->incStrong(&sRefBaseOwner);
	    }
	    return reinterpret_cast<jlong>(surface.get());
	}
	
	sp<Surface> SurfaceControl::getSurface() const
	{
	    Mutex::Autolock _l(mLock);
	    if (mSurfaceData == 0) {
	        mSurfaceData = new Surface(mGraphicBufferProducer, false);
	    }
	    return mSurfaceData;
	}

到这里WMS端Surface创建及填充完毕，并且Surface其实与WMS的SurfaceControl一一对应，当APP端需要在图层级别进行操控的时候，其实还是要依靠SurfaceControl的，WMS的Surface创建完毕后，需要传递给APP端，之后APP端就获得直接同SurfaceFlinger通信的能力，比如绘图与UI更新，怎传递的呢？我们知道Surface实现了Parcel接口，因此可以传递序列化的数据，其实看一下Surface nativeReadFromParcel就知道到底是怎么传递的了，利用readStrongBinder获取IGraphicBufferProducer对象的句柄，之后转化为IGraphicBufferProducer代理其实就是BpGraphicBufferProducer，之后利用BpGraphicBufferProducer构建Surface，这样APP端Surface就被填充完毕，可以同SurfaceFlinger通信了：

	
	static jlong nativeReadFromParcel(JNIEnv* env, jclass clazz,
	        jlong nativeObject, jobject parcelObj) {
	    Parcel* parcel = parcelForJavaObject(env, parcelObj);
	    if (parcel == NULL) {
	        doThrowNPE(env);
	        return 0;
	    }
		 sp<Surface> self(reinterpret_cast<Surface *>(nativeObject));
	    sp<IBinder> binder(parcel->readStrongBinder());
	    if (self != NULL
	            && (IInterface::asBinder(self->getIGraphicBufferProducer()) == binder)) {
	        return jlong(self.get());
	    }
	    sp<Surface> sur;
	    sp<IGraphicBufferProducer> gbp(interface_cast<IGraphicBufferProducer>(binder));
	    if (gbp != NULL) {
	        sur = new Surface(gbp, true);
	        sur->incStrong(&sRefBaseOwner);
	    }
	
	    if (self != NULL) {
	        self->decStrong(&sRefBaseOwner);
	    }
	
	    return jlong(sur.get());
	}

到这里为止，APP<->WMS <->WMS 通信申请Surface的流程算走完了

![Surface对应关系.jpg](http://upload-images.jianshu.io/upload_images/1460468-a16747b74ea5a486.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


## 总结

窗口的添加流程简化如下，这里暂且忽略窗口的分组管理。

* APP去WMS登记窗口
* APP新建Surface壳子，请求WMS填充Surface
* WMS请求SurfaceFlinger分配窗口图层
* SurfaceFlinger分配Layer，将结果回传给WMS
* WMS将窗口信息填充到Surface传输到APP
* APP端获得填充信息，获取与SurfaceFlinger通信的能力

