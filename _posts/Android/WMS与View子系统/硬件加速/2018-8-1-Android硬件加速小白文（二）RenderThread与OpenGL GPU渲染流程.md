
---
layout: post
title: Android硬件加速-RenderThread与OpenGL GPU渲染
category: Android

---
      
Android4.0之后，系统默认开启硬件加速来渲染View，之前，[理解Android硬件加速的小白文](http://www.jianshu.com/p/40f660e17a73)简单的讲述了硬件加速的简单模型，不过主要针对前半阶段，并没怎么说是如何使用OpenGL、GPU处理数据的，OpenGL主要处理的任务有Surface的composition及图形图像的渲染，本篇文章简单说一下后半部分的模型，这部分对于理解View渲染也有不少帮助，也能更好的帮助理解GPU渲染玄学曲线。

不过这里有个概念要先弄清，OpenGL仅仅是提供标准的API及调用规则，在不同的硬件平台上有不同的实现，比如驱动等，这部分代码一般是不开源，本文主要基于Android libagl（6.0），它是Android中通过软件方法实现的一套OpenGL动态库，并结合Systrace真机上的调用栈，猜测libhgl的实现，对比两者区别（GPU厂商提供的硬件实现的OpenGL）。对于Android APP而言，基于GPU的硬件加速绘制可以分为如下几个阶段：

* 第一阶段：APP在UI线程依赖CPU构建OpenGL渲染需要的命令及数据
* 第二阶段：CPU将数据上传（共享或者拷贝）给GPU，PC上一般有显存一说，但是ARM这种嵌入式设备内存一般是GPU CPU共享内存
* 第三阶段：通知GPU渲染，一般而言，真机不会阻塞等待GPU渲染结束，效率低，CPU通知结束后就返回继续执行其他任务，当然，理论上也可以阻塞执行，glFinish就能满足这样的需求（**不同GPU厂商实现不同，Android源码自带的是软件实现的，只具有参考意义**）（Fence机制辅助GPU CPU同步）
* 第四阶段：swapBuffers，并通知SurfaceFlinger图层合成
* 第五阶段：SurfaceFlinger开始合成图层，如果之前提交的GPU渲染任务没结束，则等待GPU渲染完成，再合成（Fence机制），合成依然是依赖GPU完全，不过这就是下一个任务了

第一个阶段，其实主要做的就是构建DrawOp树（里面封装OpenGL渲染命令），同时，预处理分组一些相似命令，以便提高GPU处理效率，这个阶段主要是CPU在工作，不过这个阶段前期运行在UI线程，后期部分运行在RenderThread（渲染线程），第二个阶段主要运行在渲染线程，CPU将数据同步（共享）给GPU，之后，通知GPU进行渲染，不过这里需要注意的是，CPU一般不会阻塞等待GPU渲染完毕，而是通知结束后就返回，除非GPU非常繁忙，来不及响应CPU的请求，没有给CPU发送通知，CPU才会阻塞等待。CPU返回后，会直接将GraphicBuffer提交给SurfaceFlinger，告诉SurfaceFlinger进行合成，但是这个时候GPU可能并未完成图像的渲染，这个时候就牵扯到一个同步，Android中，这里用的是Fence机制，SurfaceFlinger合成前会查询这个Fence，如果GPU渲染没有结束，则等待GPU渲染结束，GPU结束后，会通知SurfaceFlinger进行合成，SF合成后，提交显示，如此完成图像的渲染显示，简单画下示意图：

![Android CPU GPU通信模型](https://upload-images.jianshu.io/upload_images/1460468-b4cf44398e5d221c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

之前已经简单分析过DrawOp树的构建，优化，本文主要是分析GPU如何完成OpenGL渲染，这个过程主要在Render线程，通过OpenGL API通知GPU处理渲染任务。

# Android OpenGL环境的初始化

一般在使用OpenGL的时候，首先需要获取OpenGL相应的配置，再为其构建渲染环境，比如必须创建OpenGL上下文(Context)，上下文可以看做是OpenGL的化身，没有上下文就没有OpenGL环境，同时还要构建一个用于绘图的画布GlSurface，在Android中抽象出来就是EglContext与EglSurface，如下：

        private void initGL() {
        
            mEgl = (EGL10) EGLContext.getEGL();
            <!--获取display显示目标-->
            mEglDisplay = mEgl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
             <!--构建配置-->
            mEglConfig = chooseEglConfig();
            ...<!--构建上下文-->
            mEglContext = createContext(mEgl, mEglDisplay, mEglConfig);
        	  ...<!--构建绘图Surface-->
            mEglSurface = mEgl.eglCreateWindowSurface(mEglDisplay, mEglConfig, mSurface, null);
            }
            
并且APP端可能会有多个窗口，但GPU同一时刻只会处理一个，到底渲染哪个呢？每一个绘制上下文对应于窗口，并且维护一套OpenGL状态机，多个窗口间彼此状态独立，不同上下文中，对应于各自资源，先看看Android系统中，APP端如何为每个窗口配置OpenGL环境的，在一个窗口被添加到窗口的时候会调用其ViewRootImpl对象的setView：

    public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
        synchronized (this) {
            		...
                    enableHardwareAcceleration(attrs);
                }
                
setView会调用enableHardwareAcceleration，配置OpenGL的硬件加速环境：

	private void enableHardwareAcceleration(WindowManager.LayoutParams attrs) {
	        mAttachInfo.mHardwareAccelerated = false;
	        mAttachInfo.mHardwareAccelerationRequested = false;
				...
	        final boolean hardwareAccelerated =
	                (attrs.flags & WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED) != 0;
	
	        if (hardwareAccelerated) {
	        <!--可以开启硬件加速 ，一般都是true-->
	            if (!HardwareRenderer.isAvailable()) {
	                return;
	            }
	 					...
	                <!--创建硬件加速环境-->
	                mAttachInfo.mHardwareRenderer = HardwareRenderer.create(mContext, translucent);
	                if (mAttachInfo.mHardwareRenderer != null) {
	                    mAttachInfo.mHardwareRenderer.setName(attrs.getTitle().toString());
	                    mAttachInfo.mHardwareAccelerated =
	                            mAttachInfo.mHardwareAccelerationRequested = true;
	                }
	            }
	        }
	    }


Android中每个显示的Window（Activity、Dialog、PopupWindow等）都对应一个ViewRootImpl对象，也会对应一个AttachInfo对象，之后通过

	HardwareRenderer.create(mContext, translucent);

创建的HardwareRenderer对象就被保存在ViewRootImpl的AttachInfo中，跟Window是一对一的关系，通过HardwareRenderer.create(mContext, translucent)创建硬件加速环境后，在需要draw绘制的时候，通过：

        mAttachInfo.mHardwareRenderer.draw(mView, mAttachInfo, this);

进一步渲染。回过头，接着看APP如何初始化硬件加速环境：**直观上说，主要是构建OpenGLContext、EglSurface、RenderThread(如果没启动的话)**。

    static HardwareRenderer create(Context context, boolean translucent) {
        HardwareRenderer renderer = null;
        if (DisplayListCanvas.isAvailable()) {
            renderer = new ThreadedRenderer(context, translucent);
        }
        return renderer;
    }
	    
    ThreadedRenderer(Context context, boolean translucent) {
        final TypedArray a = context.obtainStyledAttributes(null, R.styleable.Lighting, 0, 0);
        ...
		<!--创建rootnode-->
        long rootNodePtr = nCreateRootRenderNode();
        mRootNode = RenderNode.adopt(rootNodePtr);
       <!--创建native ThreadProxy-->
        mNativeProxy = nCreateProxy(translucent, rootNodePtr);
		<!--初始化AssetAtlas,本文不分析-->
        ProcessInitializer.sInstance.init(context, mNativeProxy);
        ...
    }
  
之前分析过，nCreateRootRenderNode 为ViewRootimpl创建一个root RenderNode，UI线程通过递归mRootNode，可以构建ViewTree所有的OpenGL绘制命令及数据，nCreateProxy会为当前widow创建一个ThreadProxy ，ThreadProxy则主要用来向RenderThread线程提交一些OpenGL相关任务，比如初始化，绘制、更新等：
	 
	 class ANDROID_API RenderProxy {
	public:
	    ANDROID_API RenderProxy(bool translucent, RenderNode* rootNode, IContextFactory* contextFactory);
	    ANDROID_API virtual ~RenderProxy();
		...
	    ANDROID_API bool initialize(const sp<ANativeWindow>& window);
	    ...
	    ANDROID_API int syncAndDrawFrame();
	    ...
	    ANDROID_API DeferredLayerUpdater* createTextureLayer();
	    ANDROID_API void buildLayer(RenderNode* node);
	    ANDROID_API bool copyLayerInto(DeferredLayerUpdater* layer, SkBitmap& bitmap);
	    ...
	    ANDROID_API void fence();
	    ...
	    void destroyContext();
	
	    void post(RenderTask* task);
	    void* postAndWait(MethodInvokeRenderTask* task);
		...
	};

RenderProxy的在创建之初会做什么？其实主要两件事，第一：如果RenderThread未启动，则启动它，第二：向RenderThread提交第一个Task--为当前窗口创建CanvasContext，CanvasContext有点EglContext的意味，所有的绘制命令都会通过CanvasContext进行中转：

	RenderProxy::RenderProxy(bool translucent, RenderNode* rootRenderNode, IContextFactory* contextFactory)
	        : mRenderThread(RenderThread::getInstance())
	        , mContext(nullptr) {
	     <!--创建CanvasContext-->
	    SETUP_TASK(createContext);
	    args->translucent = translucent;
	    args->rootRenderNode = rootRenderNode;
	    args->thread = &mRenderThread;
	    args->contextFactory = contextFactory;
	    mContext = (CanvasContext*) postAndWait(task);
	    <!--初始化DrawFrameTask-->
	    mDrawFrameTask.setContext(&mRenderThread, mContext);
	}

从其构造函数中可以看出，OpenGL Render线程是一个单例，同一个进程只有一个RenderThread，RenderProxy 通过mRenderThread引用该单例，将来需要提交任务的时候，直接通过该引用向RenderThread的Queue中插入消息，而RenderThread主要负责从Queue取出消息，并执行，比如将OpenGL命令issue提交给GPU，并通知GPU渲染。在Android Profile的CPU工具中可以清楚的看到该线程的存在（没有显示任务的进程是没有的：

![renderThread](https://upload-images.jianshu.io/upload_images/1460468-265afedca9d749a1.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

简单看下RenderThread()这个单例线程的创建与启动，

	RenderThread::RenderThread() : Thread(true), Singleton<RenderThread>()
	        , mNextWakeup(LLONG_MAX)
	        , mDisplayEventReceiver(nullptr)
	        , mVsyncRequested(false)
	        , mFrameCallbackTaskPending(false)
	        , mFrameCallbackTask(nullptr)
	        , mRenderState(nullptr)
	        , mEglManager(nullptr) {
	    Properties::load();
	    mFrameCallbackTask = new DispatchFrameCallbacks(this);
	    mLooper = new Looper(false);
	    run("RenderThread");
	}

RenderThread会维护一个MessageQuene，并通过loop的方式读取消息，执行，RenderThread在启动之前，会为OpenGL创建EglManager、RenderState、VSync信号接收器（这个主要为了动画）等OpenGL渲染需要工具组件，之后启动该线程进入loop：
	
	bool RenderThread::threadLoop() {
		
		<!--初始化-->
	    setpriority(PRIO_PROCESS, 0, PRIORITY_DISPLAY);
	    initThreadLocals();
	
	    int timeoutMillis = -1;
	    for (;;) {
	    <!--等待消息队列不为空-->
	        int result = mLooper->pollOnce(timeoutMillis);
	        nsecs_t nextWakeup;
	        // Process our queue, if we have anything
	        <!--获取消息并执行-->
	        while (RenderTask* task = nextTask(&nextWakeup)) {
	            task->run();
	        }
	        ...	
	    return false;}

初始化，主要是创建EglContext中必须的一些组件，到这里其实都是工具的创建，基本上还没构建OpenGL需要的任何实质性的东西
	
		 void RenderThread::initThreadLocals() {
	    sp<IBinder> dtoken(SurfaceComposerClient::getBuiltInDisplay(
	            ISurfaceComposer::eDisplayIdMain));
	    status_t status = SurfaceComposerClient::getDisplayInfo(dtoken, &mDisplayInfo);
	    nsecs_t frameIntervalNanos = static_cast<nsecs_t>(1000000000 / mDisplayInfo.fps);
	    mTimeLord.setFrameInterval(frameIntervalNanos);
	    <!--初始化vsync接收器-->
	    initializeDisplayEventReceiver();
	    <!--管家-->
	    mEglManager = new EglManager(*this);
	    <!--状态机-->
	    mRenderState = new RenderState(*this);
	    <!--debug分析工具-->
	    mJankTracker = new JankTracker(frameIntervalNanos);
	}

Android5.0之后，有些动画是可以完全在RenderThread完成的，这个时候render渲染线程需要接受Vsync，等信号到来后，回调RenderThread::displayEventReceiverCallback，计算当前动画状态，最后调用doFrame绘制当前动画帧（不详述），有时间可以看下Vsync机制

	void RenderThread::initializeDisplayEventReceiver() {
	    mDisplayEventReceiver = new DisplayEventReceiver();
	    status_t status = mDisplayEventReceiver->initCheck();
	    mLooper->addFd(mDisplayEventReceiver->getFd(), 0,
	            Looper::EVENT_INPUT, RenderThread::displayEventReceiverCallback, this);
	}

其次RenderThread需要new一个EglManager及RenderState，两者跟上面的DisplayEventReceiver都从属RenderThread，因此在一个进程中，也是单例的

	EglManager::EglManager(RenderThread& thread)
	        : mRenderThread(thread)
	        , mEglDisplay(EGL_NO_DISPLAY)
	        , mEglConfig(nullptr)
	        , mEglContext(EGL_NO_CONTEXT)
	        , mPBufferSurface(EGL_NO_SURFACE)
	        , mAllowPreserveBuffer(load_dirty_regions_property())
	        , mCurrentSurface(EGL_NO_SURFACE)
	        , mAtlasMap(nullptr)
	        , mAtlasMapSize(0) {
	    mCanSetPreserveBuffer = mAllowPreserveBuffer;
	}
	
EglManager主要作用是管理OpenGL上下文，比如创建EglSurface、指定当前操作的Surface、swapBuffers等，主要负责场景及节点的管理工作：
	
	class EglManager {
	public:
	    // Returns true on success, false on failure
	    void initialize();
	    EGLSurface createSurface(EGLNativeWindowType window);
	    void destroySurface(EGLSurface surface);
	
	    bool isCurrent(EGLSurface surface) { return mCurrentSurface == surface; }
	    // Returns true if the current surface changed, false if it was already current
	    bool makeCurrent(EGLSurface surface, EGLint* errOut = nullptr);
	    void beginFrame(EGLSurface surface, EGLint* width, EGLint* height);
	    bool swapBuffers(EGLSurface surface, const SkRect& dirty, EGLint width, EGLint height);
	
	    // Returns true iff the surface is now preserving buffers.
	    bool setPreserveBuffer(EGLSurface surface, bool preserve);
	    void setTextureAtlas(const sp<GraphicBuffer>& buffer, int64_t* map, size_t mapSize);
	    void fence();
	
	private:
	    friend class RenderThread;
	
	    EglManager(RenderThread& thread);
	    // EglContext is never destroyed, method is purposely not implemented
	    ~EglManager();
	    void createPBufferSurface();
	    void loadConfig();
	    void createContext();
	    void initAtlas();
	    RenderThread& mRenderThread;
	    EGLDisplay mEglDisplay;
	    EGLConfig mEglConfig;
	    EGLContext mEglContext;
	    EGLSurface mPBufferSurface;
	    ,,
	};

而RenderState可以看做是OpenGL状态机的具体呈现，真正负责OpenGL的渲染状态的维护及渲染命令的issue
	
	RenderState::RenderState(renderthread::RenderThread& thread)
	        : mRenderThread(thread)
	        , mViewportWidth(0)
	        , mViewportHeight(0)
	        , mFramebuffer(0) {
	    mThreadId = pthread_self();
	}
	
在RenderProxy创建之初，插入到的第一条消息就是SETUP_TASK(createContext)，构建CanvasContext ,它可以看做OpenGL的Context及Surface的封装，

	CREATE_BRIDGE4(createContext, RenderThread* thread, bool translucent,
	        RenderNode* rootRenderNode, IContextFactory* contextFactory) {
	    return new CanvasContext(*args->thread, args->translucent,
	            args->rootRenderNode, args->contextFactory);
	}


可以看到，CanvasContext同时握有RenderThread、EglManager、RootRenderNode等，它可以看做Android中OpenGL上下文，是上层渲染API的入口

	CanvasContext::CanvasContext(RenderThread& thread, bool translucent,
	        RenderNode* rootRenderNode, IContextFactory* contextFactory)
	        : mRenderThread(thread)
	        , mEglManager(thread.eglManager())
	        , mOpaque(!translucent)
	        , mAnimationContext(contextFactory->createAnimationContext(mRenderThread.timeLord()))
	        , mRootRenderNode(rootRenderNode)
	        , mJankTracker(thread.timeLord().frameIntervalNanos())
	        , mProfiler(mFrames) {
	    mRenderThread.renderState().registerCanvasContext(this);
	    mProfiler.setDensity(mRenderThread.mainDisplayInfo().density);
	}

其实到这里初始化完成了一般，另一半是在draw的时候，进行的也就是ThreadRender的initialize，毕竟，如果不需要绘制，是不需要初始化OpenGL环境的，省的浪费资源：

    private void performTraversals() {
       ...
          if (mAttachInfo.mHardwareRenderer != null) {
                            try {
                                hwInitialized = mAttachInfo.mHardwareRenderer.initialize(mSurface);

这里的mSurface其实是已经被WMS填充处理过的一个Surface，它在native层对应一个ANativeWindow（其实就是个native的Surface），随着RenderProxy的initial的初始化，EglContext跟EglSurface会被进一步创建，需要注意的是这里的initialize任务是在Render线程，OpenGL的相关操作都必须在Render线程：
	
	CREATE_BRIDGE2(initialize, CanvasContext* context, ANativeWindow* window) {
	    return (void*) args->context->initialize(args->window);
	}
	
	bool RenderProxy::initialize(const sp<ANativeWindow>& window) {
	    SETUP_TASK(initialize);
	    args->context = mContext;
	    args->window = window.get();
	    return (bool) postAndWait(task);
	}

	bool CanvasContext::initialize(ANativeWindow* window) {
	    setSurface(window);
	    if (mCanvas) return false;
	    mCanvas = new OpenGLRenderer(mRenderThread.renderState());
	    mCanvas->initProperties();
	    return true;
	}

这里传入的ANativeWindow* window其实就是native的Surface，CanvasContext在初始化的时候，会通过setSurface为OpenGL创建E关联Con小text、EglSurface画布，同时会为当前窗口创建一个OpenGLRenderer，OpenGLRenderer主要用来处理之前构建的DrawOp，输出对应的OpenGL命令。

	void CanvasContext::setSurface(ANativeWindow* window) {
	    mNativeWindow = window;
	    <!--创建EglSurface画布-->
	    if (window) {
	        mEglSurface = mEglManager.createSurface(window);
	    }
	    if (mEglSurface != EGL_NO_SURFACE) {
	        const bool preserveBuffer = (mSwapBehavior != kSwap_discardBuffer);
	        mBufferPreserved = mEglManager.setPreserveBuffer(mEglSurface, preserveBuffer);
	        mHaveNewSurface = true;
	        <!--绑定上下文-->
	        makeCurrent();
	    }}
	
	EGLSurface EglManager::createSurface(EGLNativeWindowType window) {
		<!--构建EglContext-->
	    initialize();
	    <!--创建EglSurface-->
	    EGLSurface surface = eglCreateWindowSurface(mEglDisplay, mEglConfig, window, nullptr);
	 	    return surface;
	}

	void EglManager::initialize() {
	    if (hasEglContext()) return;
	    
	    mEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	    loadConfig();
	    createContext();
	    createPBufferSurface();
	    makeCurrent(mPBufferSurface);
	    mRenderThread.renderState().onGLContextCreated();
	    initAtlas();
	}

	void EglManager::createContext() {
	    EGLint attribs[] = { EGL_CONTEXT_CLIENT_VERSION, GLES_VERSION, EGL_NONE };
	    mEglContext = eglCreateContext(mEglDisplay, mEglConfig, EGL_NO_CONTEXT, attribs);
	    LOG_ALWAYS_FATAL_IF(mEglContext == EGL_NO_CONTEXT,
	        "Failed to create context, error = %s", egl_error_str());
	}

EglManager::initialize()之后EglContext、Config全都有了，之后通过eglCreateWindowSurface创建EglSurface,这里先调用eglApi.cpp 的eglCreateWindowSurface
	
	
	EGLSurface eglCreateWindowSurface(  EGLDisplay dpy, EGLConfig config,
	                                    NativeWindowType window,
	                                    const EGLint *attrib_list) {
	        <!--配置-->
	        int result = native_window_api_connect(window, NATIVE_WINDOW_API_EGL);
	        <!--Android源码中，其实是调用egl.cpp的eglCreateWindowSurface，不过这一块软件模拟的跟真实硬件的应该差别不多-->	
	        // Eglsurface里面是有Surface的引用的，同时swap的时候，是能通知consumer的
	        EGLSurface surface = cnx->egl.eglCreateWindowSurface(
	                iDpy, config, window, attrib_list);
	        ...	}

>egl.cpp其实是软件模拟的GPU实现库，不过这里的eglCreateWindowSurface逻辑其实跟真实GPU平台的代码差别不大，因为只是抽象逻辑：

	static EGLSurface createWindowSurface(EGLDisplay dpy, EGLConfig config,
	        NativeWindowType window, const EGLint* /*attrib_list*/)
	{
	   ...
	    egl_surface_t* surface;
	    <!--其实返回的就是egl_window_surface_v2_t-->
	    surface = new egl_window_surface_v2_t(dpy, config, depthFormat,
	            static_cast<ANativeWindow*>(window));
	..	    return surface;
	}
	
从上面代码可以看出，其实就是new了一个egl_window_surface_v2_t，它内部封装了一个ANativeWindow，由于EGLSurface是一个Void* 类型指针，因此egl_window_surface_v2_t型指针可以直接赋值给它，到这里初始化环境结束，OpenGL需要的渲染环境已经搭建完毕，等到View需要显示或者更新的时候，就会接着调用VieWrootImpl的draw去更新，注意这里，一个Render线程，默认一个EglContext，但是可以有多个EglSurface，用eglMakeCurrent切换绑定即可。也就是一个Window对应一个ViewRootImpl->一个AttachInfo->ThreadRender对象->ThreadProxy(RootRenderNode)->CanvasContext.cpp(DrawFrameTask、EglManager（**单例复用**）、EglSurface)->->RenderThread(**单例复用**)，对于APP而言，一般只会维持一个OpenGL渲染线程，当然，你也可以自己new一个独立的渲染线程，主动调用OpenGL API。简答类图如下

![image.png](https://upload-images.jianshu.io/upload_images/1460468-6c1252ee03d0ef62.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

上面工作结束后，OpenGL渲染环境就已经准备好，或者说RenderThread这个渲染线程已经配置好了渲染环境，接下来，UI线程像渲染线程发送渲染任务就行了。

# Android OpenGL GPU 渲染

之前分析[理解Android硬件加速的小白文](http://www.jianshu.com/p/40f660e17a73)的时候，已经分析过，ViewRootImpl的draw是入口，会调用HardwareRender的draw，先构建DrawOp树，然后合并优化DrawOp，之后issue OpenGL命令到GPU，其中构建DrawOp的任务在UI线程，后面的任务都在Render线程

    @Override
    void draw(View view, AttachInfo attachInfo, HardwareDrawCallbacks callbacks) {
       <!--构建DrawOp Tree UI线程-->        
       updateRootDisplayList(view, callbacks);
       <!--渲染 提交任务到render线程-->
        int syncResult = nSyncAndDrawFrame(mNativeProxy, frameInfo, frameInfo.length);
        ...
    }
    
如上面代码所说updateRootDisplayList构建DrawOp树在UI线程，nSyncAndDrawFrame提交渲染任务到渲染线程，之前已经分析过构建流程，nSyncAndDrawFrame也简单分析了一些合并等操作，下面接着之前流程分析如何将OpenGL命令issue到GPU，这里有个同步问题，可能牵扯到UI线程的阻塞，先分析下同步

## SyncAndDrawFrame 同步

	static int android_view_ThreadedRenderer_syncAndDrawFrame(JNIEnv* env, jobject clazz,
	        jlong proxyPtr, jlongArray frameInfo, jint frameInfoSize) {
	    RenderProxy* proxy = reinterpret_cast<RenderProxy*>(proxyPtr);
	    env->GetLongArrayRegion(frameInfo, 0, frameInfoSize, proxy->frameInfo());
	    return proxy->syncAndDrawFrame();
	}

	int DrawFrameTask::drawFrame() {
	    mSyncResult = kSync_OK;
	    mSyncQueued = systemTime(CLOCK_MONOTONIC);
	    postAndWait();
	    return mSyncResult;
	}
	
	void DrawFrameTask::postAndWait() {
	    AutoMutex _lock(mLock);
	    mRenderThread->queue(this);
	    <!--阻塞等待，同步资源-->
	    mSignal.wait(mLock);
	}
	
	void DrawFrameTask::run() {
	    bool canUnblockUiThread;
	    bool canDrawThisFrame;
	    {
	        TreeInfo info(TreeInfo::MODE_FULL, mRenderThread->renderState());
	        <!--同步操作，其实就是同步Java跟native中的构建DrawOp Tree、图层、图像资源-->
	        canUnblockUiThread = syncFrameState(info);
	        canDrawThisFrame = info.out.canDrawThisFrame;
	    }
	    // Grab a copy of everything we need
	    CanvasContext* context = mContext;
	    <!--如果同步完成，则可以返回-->
	    if (canUnblockUiThread) {
	        unblockUiThread();
	    }
		<!--绘制，提交OpenGL命令道GPU-->
	    if (CC_LIKELY(canDrawThisFrame)) {
	        context->draw();
	    }
	   <!--看看是否之前因为同步问题阻塞了UI线程，如果阻塞了，需要唤醒-->
	    if (!canUnblockUiThread) {
	        unblockUiThread();
	    }
	}

其实就是调用RenderProxy的syncAndDrawFrame，将DrawFrameTask插入RenderThread，并且阻塞等待RenderThread跟UI线程同步，如果同步成功，则UI线程唤醒，否则UI线程阻塞等待直到Render线程完成OpenGL命令的issue完毕。同步结束后，之后RenderThread才会开始处理GPU渲染相关工作，先看下同步：


	bool DrawFrameTask::syncFrameState(TreeInfo& info) {
	    int64_t vsync = mFrameInfo[static_cast<int>(FrameInfoIndex::Vsync)];
	    mRenderThread->timeLord().vsyncReceived(vsync);
	    mContext->makeCurrent();
	    Caches::getInstance().textureCache.resetMarkInUse(mContext);
		
		<!--关键点1，TextureView类处理，主要牵扯纹理-->
	    for (size_t i = 0; i < mLayers.size(); i++) {
	        // 更新Layer 这里牵扯到图层数据的处理，可能还有拷贝，
	        mContext->processLayerUpdate(mLayers[i].get());
	    }
	    mLayers.clear();
	    <!--关键点2 同步DrawOp Tree -->
	    mContext->prepareTree(info, mFrameInfo, mSyncQueued);
		 ...
	    // If prepareTextures is false, we ran out of texture cache space
	    return info.prepareTextures;
	}

当Window中的TextureView（目前只考虑系统API，好像就这么一个View，自定义除外）有更新时，需要从TextureView的SurfaceTexture中读取图形缓冲区，并且封装绑定成Open GL纹理，供GPU绘制使用，这里不详述，将来有机会分析TexutureView的时候再分析。第二步，是将UI线程中构建的DrawOpTree等信息同步到Render Thread中，因为之前通过ViewRootImpl再Java层调用构建的DisplayListData还没被真正赋值到RenderNode的mDisplayListData（最终用到的对象），只是被setStagingDisplayList暂存，因为中间可能有那种多次meausre、layout的，还有可能发生改变，暂存逻辑如下：

	static void android_view_RenderNode_setDisplayListData(JNIEnv* env,
	        jobject clazz, jlong renderNodePtr, jlong newDataPtr) {
	    RenderNode* renderNode = reinterpret_cast<RenderNode*>(renderNodePtr);
	    DisplayListData* newData = reinterpret_cast<DisplayListData*>(newDataPtr);
	    renderNode->setStagingDisplayList(newData);
	}

	void RenderNode::setStagingDisplayList(DisplayListData* data) {
	    mNeedsDisplayListDataSync = true;
	    delete mStagingDisplayListData;
	    mStagingDisplayListData = data;
	}
	
View的DrawOpTree同步

	void CanvasContext::prepareTree(TreeInfo& info, int64_t* uiFrameInfo, int64_t syncQueued) {
	    mRenderThread.removeFrameCallback(this);

	    if (!wasSkipped(mCurrentFrameInfo)) {
	        mCurrentFrameInfo = &mFrames.next();
	    }

		<!--同步Java层测绘信息到native，OpenGL玄学曲线的来源-->
	    mCurrentFrameInfo->importUiThreadInfo(uiFrameInfo);
	    mCurrentFrameInfo->set(FrameInfoIndex::SyncQueued) = syncQueued;
	    <!--一个计时节点-->
	    mCurrentFrameInfo->markSyncStart();
		    info.damageAccumulator = &mDamageAccumulator;
	    info.renderer = mCanvas;
	    info.canvasContext = this;
	
	    mAnimationContext->startFrame(info.mode);
	    // mRootRenderNode递归遍历所有节点
	    mRootRenderNode->prepareTree(info);
	  ...
	  
通过递归遍历，mRootRenderNode可以检查所有的节点，

	void RenderNode::prepareTree(TreeInfo& info) {
	    bool functorsNeedLayer = Properties::debugOverdraw;
	    prepareTreeImpl(info, functorsNeedLayer);
	}

	void RenderNode::prepareTreeImpl(TreeInfo& info, bool functorsNeedLayer) {
	    info.damageAccumulator->pushTransform(this);
	
	    if (info.mode == TreeInfo::MODE_FULL) {
	        // 同步属性 
	        pushStagingPropertiesChanges(info);
	    }
	     
	    // layer
	    prepareLayer(info, animatorDirtyMask);
	    <!--同步DrawOpTree-->
	    if (info.mode == TreeInfo::MODE_FULL) {
	        pushStagingDisplayListChanges(info);
	    }
	    <!--递归处理子View-->
	    prepareSubTree(info, childFunctorsNeedLayer, mDisplayListData);
	    // push
	    pushLayerUpdate(info);
	    info.damageAccumulator->popTransform();
	}

到这里同步的时候，基本就是最终结果，只要把mStagingDisplayListData赋值到mDisplayListData即可，

	void RenderNode::pushStagingDisplayListChanges(TreeInfo& info) {
	    if (mNeedsDisplayListDataSync) {
	        mNeedsDisplayListDataSync = false;
	        ...
	        mDisplayListData = mStagingDisplayListData;
	        mStagingDisplayListData = nullptr;
	        if (mDisplayListData) {
	            for (size_t i = 0; i < mDisplayListData->functors.size(); i++) {
	                (*mDisplayListData->functors[i])(DrawGlInfo::kModeSync, nullptr);
	            }
	        }
	        damageSelf(info);
	    }
	}
	
之后通过递归遍历子View，便能够完成完成所有View的RenderNode的同步。要注意的是，虽然通过mRootRenderNode能递归所有的，但是每个RenderNood目前只负责自己的DisplayListData：

	void RenderNode::prepareSubTree(TreeInfo& info, bool functorsNeedLayer, DisplayListData* subtree) {
	    if (subtree) {
	        TextureCache& cache = Caches::getInstance().textureCache;
	        info.out.hasFunctors |= subtree->functors.size();
	        <!--吧RenderNode用到的bitmap封装成纹理-->
	        for (size_t i = 0; info.prepareTextures && i < subtree->bitmapResources.size(); i++) {
	            info.prepareTextures = cache.prefetchAndMarkInUse(
	                    info.canvasContext, subtree->bitmapResources[i]);
	        }
	        <!--递归子View-->
	        for (size_t i = 0; i < subtree->children().size(); i++) {
	            ...
	            childNode->prepareTreeImpl(info, childFunctorsNeedLayer);
	            info.damageAccumulator->popTransform();
	        }
	    }
	}

当DrawFrameTask::syncFrameState返回值(其实是TreeInfo的prepareTextures,主要是针对纹理的处理)为true时，表示同步完成，可以立刻唤醒UI线程，但是如果返回false，则就意UI中的数据没完全传输给GPU，这个情况下UI线程需要等待， 源码中有句注释 **If prepareTextures is false, we ran out of texture cache space**，其实就是说一个应用程序进程可以创建的Open GL纹理是有大小限制的，如果超出这个限制，纹理就会同步失败，看6.0代码，这个限制有Bitmap自身大小的限制，还有整体可用内存的限制，看代码中的限制

	Texture* TextureCache::getCachedTexture(const SkBitmap* bitmap, AtlasUsageType atlasUsageType) {
	    if (CC_LIKELY(mAssetAtlas != nullptr) && atlasUsageType == AtlasUsageType::Use) {
	        AssetAtlas::Entry* entry = mAssetAtlas->getEntry(bitmap);
	        if (CC_UNLIKELY(entry)) {
	            return entry->texture;
	        }
	    }
	
	    Texture* texture = mCache.get(bitmap->pixelRef()->getStableID());
	
	    // 没找到情况下
	    if (!texture) {
	        // 判断单个限制
	        if (!canMakeTextureFromBitmap(bitmap)) {
	            return nullptr;
	        }
	
	        const uint32_t size = bitmap->rowBytes() * bitmap->height();
	        //
	        bool canCache = size < mMaxSize;
	        // Don't even try to cache a bitmap that's bigger than the cache
	        // 剔除Lru算法中老的，不再用的，如果能够挪出空间，那就算成功，否则失败
	        while (canCache && mSize + size > mMaxSize) {
	            Texture* oldest = mCache.peekOldestValue();
	            if (oldest && !oldest->isInUse) {
	                mCache.removeOldest();
	            } else {
	                canCache = false;
	            }
	        }
	        // 如果能缓存，就新建一个Texture
	        if (canCache) {
	            texture = new Texture(Caches::getInstance());
	            texture->bitmapSize = size;
	            generateTexture(bitmap, texture, false);
	
	            mSize += size;
	            TEXTURE_LOGD("TextureCache::get: create texture(%p): name, size, mSize = %d, %d, %d",
	                     bitmap, texture->id, size, mSize);
	            if (mDebugEnabled) {
	                ALOGD("Texture created, size = %d", size);
	            }
	            mCache.put(bitmap->pixelRef()->getStableID(), texture);
	        }
	    } else if (!texture->isInUse && bitmap->getGenerationID() != texture->generation) {
	        // Texture was in the cache but is dirty, re-upload
	        // TODO: Re-adjust the cache size if the bitmap's dimensions have changed
	        generateTexture(bitmap, texture, true);
	    }
	
	    return texture;
	}

先看单个Bitmap限制：

	bool TextureCache::canMakeTextureFromBitmap(const SkBitmap* bitmap) {
	if (bitmap->width() > mMaxTextureSize || bitmap->height() > mMaxTextureSize) {
	    ALOGW("Bitmap too large to be uploaded into a texture (%dx%d, max=%dx%d)",
	            bitmap->width(), bitmap->height(), mMaxTextureSize, mMaxTextureSize);
	    return false;
	}
	return true;
	}

单个Bitmap大小限制基本上定义： 

	#define GL_MAX_TEXTURE_SIZE               0x0D33

如果bitmap的宽高超过这个值，可能就会同步失败，再看第二个原因：超过能够Cache纹理总和上限：

	#define DEFAULT_TEXTURE_CACHE_SIZE 24.0f 这里是24M

如果空间足够，则直接新建一个Texture，如果不够，则根据Lru算法 ，剔除老的不再使用的Textrue，剔除后的空间如果够，则新建Texture，否则按失败处理，这里虽然说得是GPU Cache，其实还是在同一个内存中，**归CPU管理的**，**不过由于对GPU不是太了解，不知道这个数值是不是跟GPU有关系**，纹理在需要新建的前提下：

	void TextureCache::generateTexture(const SkBitmap* bitmap, Texture* texture, bool regenerate) {
	    SkAutoLockPixels alp(*bitmap);
	    <!--glGenTextures新建纹理-->
	    if (!regenerate) {
	        glGenTextures(1, &texture->id);
	    }
	
	    texture->generation = bitmap->getGenerationID();
	    texture->width = bitmap->width();
	    texture->height = bitmap->height();
	    <!--绑定纹理-->
	    Caches::getInstance().textureState().bindTexture(texture->id);
	
	    switch (bitmap->colorType()) {
	    ...
	    case kN32_SkColorType:
	     // 32位 RGBA 或者BGREA resize第一次都是true，因为一开始宽高肯定不一致
	        uploadToTexture(resize, GL_RGBA, bitmap->rowBytesAsPixels(), bitmap->bytesPerPixel(),
	                texture->width, texture->height, GL_UNSIGNED_BYTE, bitmap->getPixels());
	    ...
	}

上面代码主要是新建纹理，然后为纹理绑定纹理图片资源，绑定资源代码如下：

	void TextureCache::uploadToTexture(bool resize, GLenum format, GLsizei stride, GLsizei bpp,
	        GLsizei width, GLsizei height, GLenum type, const GLvoid * data) {
	    glPixelStorei(GL_UNPACK_ALIGNMENT, bpp);
	    const bool useStride = stride != width
	            && Caches::getInstance().extensions().hasUnpackRowLength();
	   ...
		     if (resize) {
	            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, type, temp);
	        } else {
	            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, temp);
	        }
	
**关键就是调用glTexImage2D将纹理图片跟纹理绑定，其实按照OpenGL glTexImage2D会再次拷贝一次图片，Bitmap可以释放了，到这里就完成了纹理的上传这部分成功了，就算同步成功，UI线程可以不再阻塞**。那么为什么同步失败的时候，CPU需要等待呢？我是这么理解的，如果说正常缓存了，调用glTexImage2D完成了一次数据的转移与备份，那么UI线程就不需要维持这份Bitmap对应的数据了，但是如果失败，没有为GPU生成备份，那就要保留这份数据，直到调用glTexImage2D为其生成备份。那为什么不把缓存调整很大呢？可能是在内存跟性能之间做的一个平衡，如果很大，可能同一时刻为GPU缓存的Bitmap太大，但是这个时候，GPU并没有用的到，可能是GPU太忙，来不及处理，那么这部分内存其实是浪费掉的，而且，这个时候CPU明显比GPU快了很多，可以适当让CPU等等，有的解析说防止Bitmap被修改，说实话，我也没太明白，只是个人理解，**欢迎纠正**，不过这里就算缓存失败，在issue提交OpenGL命令的时候，还是会再次upload Bitmap的，这大概也是UI阻塞的原因，这个时段对应的耗时如下：

![OpenGL CPU跟GPU关系玄学曲线.jpg](https://upload-images.jianshu.io/upload_images/1460468-73ba03209982131b.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

## Render线程issue提交OpenGL渲染命令

同步完成后，就可以处理之前的DrawOpTree，装换成标准的OpenGL API，提交OpenGL渲染任务，继续看DrawFrameTask的后半部分，主要是调用CanvasContext的draw，递归之前的DrawOpTree

	void CanvasContext::draw() {
	   
	    EGLint width, height;
	    <!--开始绘制，绑定EglSurface， 申请EglSurface需要的内存-->
	    mEglManager.beginFrame(mEglSurface, &width, &height);
	    ...
	    Rect outBounds;
	    <!--递归调用OpenGLRender中的OpenGL API，绘制-->
	    mCanvas->drawRenderNode(mRootRenderNode.get(), outBounds);
	    bool drew = mCanvas->finish();
	    // Even if we decided to cancel the frame, from the perspective of jank
	    // metrics the frame was swapped at this point
	    mCurrentFrameInfo->markSwapBuffers();
	    <!--通知提交画布-->
	    if (drew) {
	        swapBuffers(dirty, width, height);
	    }
	   ...
	}

* 第一步，mEglManager.beginFrame，其实是标记当前上下文，并且申请绘制内存，因为一个进程中可能存在多个window，也就是多个EglSurface，那么我们首先需要标记处理哪个，也就是用哪块画布绘画。之前[理解Android硬件加速的小白文](http://www.jianshu.com/p/40f660e17a73)说过，硬件加速场景会提前在SurfaceFlinger申请内存坑位，但是并未真正申请内存，这块内存是在真正绘制的时候才去申请，这里申请内存是GPU直接操作的内存，也是将来用来提交给SurfaceFlinger用来合成用的Layer数据；
* 第二步是递归issue OpenGL命令，请GPU绘制；
* 第三步：通过swapBuffers将绘制好的数据提交给SF去合成（**其实GPU很可能并未完成渲染，但是可以提前释放Render线程，这里需要Fence机制保证同步**）。不同的GPU实现不同，厂商不会将这部分开源，本文结合Android源码（软件实现的OpenGL）跟真机Systrace猜测实现。

先看第一步，通过EglManager让Context绑定当前EglSurface，完成GPU绘制内存的申请

	void EglManager::beginFrame(EGLSurface surface, EGLint* width, EGLint* height) {

	    makeCurrent(surface);
	    ...
	    eglBeginFrame(mEglDisplay, surface);
	}
	
makeCurrent都会向BnGraphicproducer申请一块内存，对于非自己编写的Render线程，基本都是向SurfaceFlinger申请，

	EGLBoolean eglMakeCurrent(  EGLDisplay dpy, EGLSurface draw,
	                            EGLSurface read, EGLContext ctx)
	{
	    ogles_context_t* gl = (ogles_context_t*)ctx;
	    if (makeCurrent(gl) == 0) {
	        if (ctx) {
	            egl_context_t* c = egl_context_t::context(ctx);
	            egl_surface_t* d = (egl_surface_t*)draw;
	            egl_surface_t* r = (egl_surface_t*)read;
	            ...
	            if (d) {
	            <!--牵扯到申请内存-->
	                if (d->connect() == EGL_FALSE) {
	                    return EGL_FALSE;
	                }
	                d->ctx = ctx;
	                <!--绑定-->
	                d->bindDrawSurface(gl);
	            }
	           ...
	    return setError(EGL_BAD_ACCESS, EGL_FALSE);
	}
	
如果是第一次的话，则需要调用egl_surface_t connect，其实就是调用之前创建的egl_window_surface_v2_t的connect，触发申请绘制内存：


	EGLBoolean egl_window_surface_v2_t::connect() 
	{
	 	 // dequeue a buffer
	    int fenceFd = -1;
	    <!--调用nativeWindow的dequeueBuffer申请绘制内存,获取一个Fence-->
	    if (nativeWindow->dequeueBuffer(nativeWindow, &buffer,
	            &fenceFd) != NO_ERROR) {
	        return setError(EGL_BAD_ALLOC, EGL_FALSE);
	    }
	
	    // wait for the buffer  等待申请的内存可用
	    sp<Fence> fence(new Fence(fenceFd));
	 	...
	    return EGL_TRUE;
	}

上面的nativeWindow其实就是Surface:
	
	int Surface::dequeueBuffer(android_native_buffer_t** buffer, int* fenceFd) {
	        ...
	    FrameEventHistoryDelta frameTimestamps;
	    status_t result = mGraphicBufferProducer->dequeueBuffer(&buf, &fence, reqWidth, reqHeight,
	                                                            reqFormat, reqUsage, &mBufferAge,
	                                                            enableFrameTimestamps ? &frameTimestamps
	                                                                                  : nullptr);
	    ... 如果需要重新分配，则requestBuffer，请求分配
	    if ((result & IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION) || gbuf == nullptr) {
	        <!--请求分配-->
	        result = mGraphicBufferProducer->requestBuffer(buf, &gbuf);
	       }
	    ...

简单说就是先申请内存坑位，如果该坑位的内存需要重新分配，则再申请分配匿名共享内存，**这里分配的内存才是EglSurface(Surface)绘制所需内存（硬件加速）**，接下来就可以通知OpenGL渲染绘制了。上面流程牵扯到一个Fence机制，其实就是一种协助生产者消费者的机制，主要作用是处理GPU跟CPU的同步上，先不谈。先走完流程，CanvasContext的mCanvas其实是OpenGLRenderer，接着看OpenGLRenderer的drawRenderNode：
	
	void OpenGLRenderer::drawRenderNode(RenderNode* renderNode, Rect& dirty, int32_t replayFlags) {
	    // All the usual checks and setup operations (quickReject, setupDraw, etc.)
	    // will be performed by the display list itself
	    if (renderNode && renderNode->isRenderable()) {
	        // compute 3d ordering
	        <!--计算Z顺序-->
	        renderNode->computeOrdering();
	        <!--如果禁止合并Op直接绘制-->
	        if (CC_UNLIKELY(Properties::drawDeferDisabled)) {
	            startFrame();
	            ReplayStateStruct replayStruct(*this, dirty, replayFlags);
	            renderNode->replay(replayStruct, 0);
	            return;
	        }
           ...
	        DeferredDisplayList deferredList(mState.currentClipRect(), avoidOverdraw);
	        DeferStateStruct deferStruct(deferredList, *this, replayFlags);
	        <!--合并-->
	        renderNode->defer(deferStruct, 0);
			 <!--处理文理图层-->
	        flushLayers();
	        <!--设置视窗-->
	        startFrame();
	       <!--flush，生成并提交OpenGL命令-->
	        deferredList.flush(*this, dirty);
	    } ...
	    
计算Z order跟合并DrawOp之前简单说过，不分析，这里只看flushLayers跟最终的issue OpenGL 命令（deferredList.flush，其实也是遍历每个DrawOp，调用自己的draw函数），flushLayers主要是处理TextureView，为了简化，先不考虑，假设不存在此类试图，那么只看flush即可，
 
	void DeferredDisplayList::flush(OpenGLRenderer& renderer, Rect& dirty) {
	    ...
	    replayBatchList(mBatches, renderer, dirty);
		...
	}

	 static void replayBatchList(const Vector<Batch*>& batchList,
	        OpenGLRenderer& renderer, Rect& dirty) {
	    for (unsigned int i = 0; i < batchList.size(); i++) {
	        if (batchList[i]) {
	            batchList[i]->replay(renderer, dirty, i);
	        }
	    }
	}


![DrawOp合并](http://upload-images.jianshu.io/upload_images/1460468-ff584edcad217367.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

	  virtual void  DrawBatch::replay(OpenGLRenderer& renderer, Rect& dirty, int index) override {
	  	        for (unsigned int i = 0; i < mOps.size(); i++) {
	            DrawOp* op = mOps[i].op;
	            const DeferredDisplayState* state = mOps[i].state;
	            renderer.restoreDisplayState(*state);
	            op->applyDraw(renderer, dirty);     }  }
    
    
递归每个合并后的Batch，接着处理Batch中每个DrawOp，调用其replay，以DrawPointsOp画点为例：

	class DrawPointsOp : public DrawLinesOp {
	public:
	    DrawPointsOp(const float* points, int count, const SkPaint* paint)
	            : DrawLinesOp(points, count, paint) {}
	
	    virtual void applyDraw(OpenGLRenderer& renderer, Rect& dirty) override {
	        renderer.drawPoints(mPoints, mCount, mPaint);
	    }
	...

最终调用OpenGLrender的drawPoints

	void OpenGLRenderer::drawPoints(const float* points, int count, const SkPaint* paint) {
	    ...
		 count &= ~0x1; 
		<!--构建VertexBuffer-->
	    VertexBuffer buffer;
	    PathTessellator::tessellatePoints(points, count, paint, *currentTransform(), buffer);
	     ...	
	    int displayFlags = paint->isAntiAlias() ? 0 : kVertexBuffer_Offset;
	    <!--使用buffer paint绘制 -->
	    drawVertexBuffer(buffer, paint, displayFlags);
	    mDirty = true;
	}

	void OpenGLRenderer::drawVertexBuffer(float translateX, float translateY,
	        const VertexBuffer& vertexBuffer, const SkPaint* paint, int displayFlags) {
	    /...
	    Glop glop;
	    GlopBuilder(mRenderState, mCaches, &glop)
	            .setRoundRectClipState(currentSnapshot()->roundRectClipState)
	            .setMeshVertexBuffer(vertexBuffer, shadowInterp)
	            .setFillPaint(*paint, currentSnapshot()->alpha)
	             ...
	            .build();
	    renderGlop(glop);
	}
	
	void OpenGLRenderer::renderGlop(const Glop& glop, GlopRenderType type) {
    ...
    mRenderState.render(glop);
    ...
	    
Vertex是OpenGL的基础概念，drawVertexBuffer调用RenderState的render，向GPU提交绘制命令（不会立即绘制，GPU也是由缓冲区的，除非手动glFinish或者glFlush，才会即刻渲染），RenderState可以看做OpenGL状态机的抽象，render函数实现如下

	void RenderState::render(const Glop& glop) {
	    const Glop::Mesh& mesh = glop.mesh;
	    const Glop::Mesh::Vertices& vertices = mesh.vertices;
	    const Glop::Mesh::Indices& indices = mesh.indices;
	    const Glop::Fill& fill = glop.fill;
	    // ---------------------------------------------
	    // ---------- Program + uniform setup ----------
	    // ---------------------------------------------
	    mCaches->setProgram(fill.program);
	
	    if (fill.colorEnabled) {
	        fill.program->setColor(fill.color);
	    }
	
	    fill.program->set(glop.transform.ortho,
	            glop.transform.modelView,
	            glop.transform.meshTransform(),
	            glop.transform.transformFlags & TransformFlags::OffsetByFudgeFactor);
	
	    // Color filter uniforms
	    if (fill.filterMode == ProgramDescription::kColorBlend) {
	        const FloatColor& color = fill.filter.color;
	        glUniform4f(mCaches->program().getUniform("colorBlend"),
	                color.r, color.g, color.b, color.a);
	    }
	     ....
		 // ---------- Mesh setup ----------
	    // vertices
	    const bool force = meshState().bindMeshBufferInternal(vertices.bufferObject)
	            || (vertices.position != nullptr);
	    meshState().bindPositionVertexPointer(force, vertices.position, vertices.stride);
	
	    // indices
	    meshState().bindIndicesBufferInternal(indices.bufferObject);
	    ...
	    // ------------------------------------
	    // ---------- GL state setup ----------
	    // ------------------------------------
	    blend().setFactors(glop.blend.src, glop.blend.dst);
	    // ------------------------------------
	    // ---------- Actual drawing ----------
	    // ------------------------------------
	    if (indices.bufferObject == meshState().getQuadListIBO()) {
	        // Since the indexed quad list is of limited length, we loop over
	        // the glDrawXXX method while updating the vertex pointer
	        GLsizei elementsCount = mesh.elementCount;
	        const GLbyte* vertexData = static_cast<const GLbyte*>(vertices.position);
	        while (elementsCount > 0) {
	            ...
	            glDrawElements(mesh.primitiveMode, drawCount, GL_UNSIGNED_SHORT, nullptr);
	            elementsCount -= drawCount;
	            vertexData += (drawCount / 6) * 4 * vertices.stride;  } }  
	            ...
	}
	
可以看到，经过一步步的设置，变换，预处理，最后都是要转换成glXXX函数，生成相应的OpenGL命令发送给GPU，通知GPU绘制，这里有两种处理方式，第一种是CPU阻塞等待GPU绘制结束后返回，再将绘制内容提交给SurfaceFlinger进行合成，第二种是CPU直接返回，然后提交给SurfaceFlinger合成，等到SurfaceFlinger合成的时候，如果还未绘制完毕，则需要阻塞等待GPU绘制完毕，软件实现的采用的是第一种，硬件实现的一般是第二种。需要注意：**OpenGL绘制前各种准备包括传给GPU使用的内存都是CPU在APP的私有内存空间申请的，而GPU真正绘制到画布使用的提交给SurfaceFlinger的那块内存，是从匿名共享申请的内存，两者是不一样的**，这一部分的耗时，其实就是CPU 将命令同步给GPU的耗时，在OpenGL玄学曲线中是：

![构建OpenGL命令.jpg](https://upload-images.jianshu.io/upload_images/1460468-e2af8168e37558d3.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


## Render线程swapBuffers提交图形缓冲区（加Fence机制）

在Android里，GraphicBuffer的同步主要借助Fence同步机制，它最大的特点是能够处理GPU、CPU、HWC间的同步。因为，GPU处理一般是异步的，当我们调用OpenGL API返回后，OpenGL命令并不是即刻被GPU执行的，而是被缓存在本地的GL命令缓冲区中，等缓冲区满的时候，才会真正通知GPU执行，而CPU可能完全不知道执行时机，除非CPU主动使用glFinish()强制刷新，阻塞等待这些命令执行完，但是，毫无疑问，这会使得CPU、GPU并行处理效率降低，至少，渲染线程是被阻塞在那里的；相对而言异步处理的效率要高一些，CPU提交命令后就返回，不等待GPU处理完，这样渲染线程被解放处理下一条消息，不过这个时候图形未被处理完毕的前提的下就被提交给SurfaceFlinger图形合成，那么SurfaceFlinger需要知道什么时候这个GraphicBuffer被GPU处理填充完毕，这个时候就是Fence机制发挥作用的地方，关于Fence不过多分析，毕竟牵扯信息也挺多，只简单画了示意图：

![Fence示意图.jpg](https://upload-images.jianshu.io/upload_images/1460468-07cffa61dfbfffef.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
	
之前的命令被issue完毕后，CPU一般会发送最后一个命令给GPU，告诉GPU当前命令发送完毕，可以处理，GPU一般而言需要返回一个确认的指令，不过，这里并不代表执行完毕，仅仅是通知到而已，如果GPU比较忙，来不及回复通知，则CPU需要阻塞等待，CPU收到通知后，会唤起当前阻塞的Render线程，继续处理下一条消息，这个阶段是在swapBuffers中完成的，Google给的解释如下：

>Once Android finishes submitting all its display list to the GPU, the system issues one final command to tell the graphics driver that it's done with the current frame. At this point, the driver can finally present the updated image to the screen.
 
>It’s important to understand that the GPU executes work in parallel with the CPU. The Android system issues draw commands to the GPU, and then moves on to the next task. The GPU reads those draw commands from a queue and processes them.

>In situations where the CPU issues commands faster than the GPU consumes them, the communications queue between the processors can become full. When this occurs, the CPU blocks, and waits until there is space in the queue to place the next command. This full-queue state arises often during the Swap Buffers stage, because at that point, a whole frame’s worth of commands have been submitted

但看Android源码而言，软件实现的libagl可以看做同步的，不需要考虑Fence机制：

	EGLBoolean egl_window_surface_v2_t::swapBuffers()
	{
	  	...
	    // 其实就是queueBuffer，queueBuffer这里用的是-1
	    nativeWindow->queueBuffer(nativeWindow, buffer, -1);
	    buffer = 0;
	    // dequeue a new buffer
	    int fenceFd = -1;
	    // 这里是为了什么，还是阻塞等待，难道是为了等待GPU处理完成吗？  
	    // buffer换buffer
	    if (nativeWindow->dequeueBuffer(nativeWindow, &buffer, &fenceFd) == NO_ERROR) {
	        sp<Fence> fence(new Fence(fenceFd));
	        // fence->wait
	        if (fence->wait(Fence::TIMEOUT_NEVER)) {
	            nativeWindow->cancelBuffer(nativeWindow, buffer, );
	            return setError(EGL_BAD_ALLOC, EGL_FALSE);
	        }
			...

可以看到，源码中是先将Buffer提交给SurfaceFlinger，然后再申请一个Buffer用来处理下一次请求。并且这里queueBuffer传递的Fence是-1，也就在swapbuffer的时候，软件实现的OpenGL库是不需要Fence机制的（压根不需要考虑GPU、CPU同步）。**queueBuffer会触发Layer回调，并向SurfaceFlinger发送消息，请求SurfaceFlinger执行，这里是一个异步过程，所以也不会阻塞**，回调入口在Layer的onFrameAvailable

	void Layer::onFrameAvailable(const BufferItem& item) {
	    { 
	    ...queueBuffer后触发Layer的onFrameAvailable回调，
	    mFlinger->signalLayerUpdate();
	}

而dequeueBuffer在slot上限允许的前提下，也不会阻塞，按理说，不会怎么耗时，但是就模拟器而言，swapBuffers好像耗时比较严重(**其中的黄色部分就是swapBuffers耗时**)，这里不太理解，因为模拟器应该是同步的，应该不会牵扯缓冲区交换时也不会隐式将命令送去GPU执行，也不会阻塞等待，为什么耗时这么多呢，模拟器的（Genymotion 6.0），不知道是不是跟Genymotion有关系：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-75a85c4674621374.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

再看一下Genymotion 的Systrace：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-f6088f8ab2dc18ec.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到，Systace中的函数调用基本跟egl.cpp中基本一致，但是dequeue buffer为什么耗时这么久呢？有些不理解，希望有人能指点。而对于硬件则需要处理Fence，其egl_window_surface_v2_t::swapBuffers()应该会被重写，至少需要传递一个有效的Fence过去，

	    nativeWindow->queueBuffer(nativeWindow, buffer, fenceId（不应该再是-1）);


也就是说，queueBuffer的fenceid不能再是-1了，因为需要一个有效的Fence处理GPU CPU同步，再再看下真机的Systrace（nexus5 6.0）

![真机OpenGL渲染Systrace](https://upload-images.jianshu.io/upload_images/1460468-f00b845c598e6103.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到真机函数的调用跟模拟器差别很大，比如dequeue、enqueue，具体可能要看各家的实现了，再看8.0的nexus6p：

![nexus6p 8.0](https://upload-images.jianshu.io/upload_images/1460468-98cbede7afa36a80.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

一开始我以为，会在glFinish()或者glFlush的时候可能会阻塞，导致swapBuffers耗时增加，但是看源码说不通，因为好像也跟就不会在enqueue或者dequeue的时候直接触发，就算触发，也是异步的。一般，**issue任务给驱动后，如果采用是双缓冲，在缓冲区交换操作会隐式将命令送去执行**，这里猜想是不同厂商自己实现，但是看不到具体的代码，也不好确定，谁做rom的希望能指点下。 这段时间的耗时在GPU呈现曲线上如下，文档解释说是CPU等待GPU的时间，个人理解：是等待时间，但是不是等待GPU完成渲染的时间，仅仅是等待一个ACK类的信号，否则，就不存在CPU、GPU并行了：

![swapbuffer耗时.jpg](https://upload-images.jianshu.io/upload_images/1460468-b1f39419ace21d02.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

dequeueBuffer会阻塞导致耗时增加吗？应该也不会，关于swapbuffer这段时间的耗时有空再看了

# 总结

* UI线程构建OpenGL的DrawOpTree
* Render线程负责DrawOpTree合并优化、数据的同步
* Render线程负责将DrawOp转换成标准OpenGL命令，并isssue给GPU
* Render线程通过swapbuffer通知GPU（待研究），同时完成向SurfaceFlinger画布数据的提交
        
# 参考文档

[](http://www.voidcn.com/article/p-njbssmva-bqc.html)
[原Android 5.1 SurfaceFlinger VSYNC详解](https://blog.csdn.net/newchenxf/article/details/49131167)                 
[Android中的GraphicBuffer同步机制-Fence](https://blog.csdn.net/jinzhuojun/article/details/39698317)                       
[android graphic(15)—fence](https://blog.csdn.net/lewif/article/details/50984212)          
[原android graphic(16)—fence(简化](https://blog.csdn.net/lewif/article/details/51007148)          
[【OpenGL】glFinish()和glFlush()函数详解-[转]](http://www.cnblogs.com/vranger/p/3621121.html)            
[关于glFlush和glFinish以及SwapBuffer的用法小结](http://www.cppblog.com/topjackhjj/articles/87911.html)