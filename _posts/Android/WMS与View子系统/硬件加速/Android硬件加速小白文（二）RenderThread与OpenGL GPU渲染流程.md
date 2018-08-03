      
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
    
如上面代码所说updateRootDisplayList构建DrawOp树在UI线程，nSyncAndDrawFrame提交渲染任务到渲染线程，之前已经分析过构建流程，nSyncAndDrawFrame也简单分析了一些合并等操作，下面接着之前流程分析如何将OpenGL命令issue到GPU，这里有个同步问题，可能牵扯到UI线程的阻塞：

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

其实就是调用RenderProxy的syncAndDrawFrame，将DrawFrameTask插入RenderThread，并且阻塞等待RenderThread跟UI线程同步，如果同步成功，则UI线程唤醒，否则UI线程阻塞等待直到Render线程完成OpenGL命令的issue完毕。同步结束后，之后RenderThread会开始处理GPU渲染相关工作，先看下同步：


	bool DrawFrameTask::syncFrameState(TreeInfo& info) {
	    ATRACE_CALL();
	    int64_t vsync = mFrameInfo[static_cast<int>(FrameInfoIndex::Vsync)];
	    mRenderThread->timeLord().vsyncReceived(vsync);
	    mContext->makeCurrent();
	    Caches::getInstance().textureCache.resetMarkInUse(mContext);
	
	    for (size_t i = 0; i < mLayers.size(); i++) {
	        // 更新Layer 这里牵扯到图层数据的再拷贝吧
	        mContext->processLayerUpdate(mLayers[i].get());
	    }
	    mLayers.clear();
	    // 处理Tree
	    mContext->prepareTree(info, mFrameInfo, mSyncQueued);
	
	    // This is after the prepareTree so that any pending operations
	    // (RenderNode tree state, prefetched layers, etc...) will be flushed.
	    if (CC_UNLIKELY(!mContext->hasSurface())) {
	        mSyncResult |= kSync_LostSurfaceRewardIfFound;
	    }
	
	    if (info.out.hasAnimations) {
	        if (info.out.requiresUiRedraw) {
	            mSyncResult |= kSync_UIRedrawRequired;
	        }
	    }
	    // If prepareTextures is false, we ran out of texture cache space
	    return info.prepareTextures;
	}

内存必须足够才会同步成功

	void CanvasContext::draw() {
	   
	    EGLint width, height;
	    <!--开始绘制，绑定EglSurface， 申请EglSurface需要的内存-->
	    mEglManager.beginFrame(mEglSurface, &width, &height);
	    ...
	    Rect outBounds;
	    <!--递归调用OpenGL，绘制-->
	    mCanvas->drawRenderNode(mRootRenderNode.get(), outBounds);
	    bool drew = mCanvas->finish();
	    // Even if we decided to cancel the frame, from the perspective of jank
	    // metrics the frame was swapped at this point
	    mCurrentFrameInfo->markSwapBuffers();
	    <!--通知提交画布-->
	    if (drew) {
	        swapBuffers(dirty, width, height);
	    }
	
	    // TODO: Use a fence for real completion?
	    mCurrentFrameInfo->markFrameCompleted();
	    mJankTracker.addFrame(*mCurrentFrameInfo);
	    mRenderThread.jankTracker().addFrame(*mCurrentFrameInfo);
	}

第一步，mEglManager.beginFrame，其实是标记当前上下文，并且申请绘制内存，之前说硬件加速会提前在SurfaceFlinger申请内存坑位，但是并未真正申请内存，这里是针对申请内存进行绘制，也是GPU直接操作的内存，将来用来提交给SurfaceFlinger，第二步是请GPU绘制，第三部是将绘制好的数据提交给SF去合成，

	void EglManager::beginFrame(EGLSurface surface, EGLint* width, EGLint* height) {

	    makeCurrent(surface);
	    ...
	    eglBeginFrame(mEglDisplay, surface);
	}
	
每次makeCurrent都会向BnGraphicproducer申请一块内存，这里我们简单的看做是向SurfaceFlinger申请，因为它的BufferQueues是由SurfaceFlinger管理，


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
	                if (d->connect() == EGL_FALSE) {
	                    return EGL_FALSE;
	                }
	                d->ctx = ctx;
	                d->bindDrawSurface(gl);
	            }
	           ...
	    return setError(EGL_BAD_ACCESS, EGL_FALSE);
	}
	
如果之前没有makeCurrent，这里是第一次的话，需要egl_surface_t connect，其实就是调用之前创建的egl_window_surface_v2_t的connect，


	EGLBoolean egl_window_surface_v2_t::connect() 
	{
	    // we're intending to do software rendering
	    native_window_set_usage(nativeWindow, 
	            GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
	
	    // dequeue a buffer
	    int fenceFd = -1;
	    <!--调用nativeWindow的dequeueBuffer申请绘制内存-->
	    if (nativeWindow->dequeueBuffer(nativeWindow, &buffer,
	            &fenceFd) != NO_ERROR) {
	        return setError(EGL_BAD_ALLOC, EGL_FALSE);
	    }
	
	    // wait for the buffer
	    sp<Fence> fence(new Fence(fenceFd));
	    
	    <!--分配深度缓存，占用也不小-->
	   // allocate a corresponding depth-buffer
	    width = buffer->width;
	    height = buffer->height;
	    if (depth.format) {
	        depth.width   = width;
	        depth.height  = height;
	        depth.stride  = depth.width; // use the width here
	        uint64_t allocSize = static_cast<uint64_t>(depth.stride) *
	                static_cast<uint64_t>(depth.height) * 2;
	        if (depth.stride < 0 || depth.height > INT_MAX ||
	                allocSize > UINT32_MAX) {
	            return setError(EGL_BAD_ALLOC, EGL_FALSE);
	        }
	        depth.data    = (GGLubyte*)malloc(allocSize);
	        if (depth.data == 0) {
	            return setError(EGL_BAD_ALLOC, EGL_FALSE);
	        }
	    }
			...
	    return EGL_TRUE;
	}

上面的nativeWindow其实就是Surface，这里才是真的要内存了

	
	int Surface::dequeueBuffer(android_native_buffer_t** buffer, int* fenceFd) {
	    ATRACE_CALL();
	    ALOGV("Surface::dequeueBuffer");
	
	    uint32_t reqWidth;
	    uint32_t reqHeight;
	    PixelFormat reqFormat;
	    uint64_t reqUsage;
	    bool enableFrameTimestamps;
	
	    {
	        Mutex::Autolock lock(mMutex);
	        if (mReportRemovedBuffers) {
	            mRemovedBuffers.clear();
	        }
	
	        reqWidth = mReqWidth ? mReqWidth : mUserWidth;
	        reqHeight = mReqHeight ? mReqHeight : mUserHeight;
	
	        reqFormat = mReqFormat;
	        reqUsage = mReqUsage;
	
	        ...
	    FrameEventHistoryDelta frameTimestamps;
	    status_t result = mGraphicBufferProducer->dequeueBuffer(&buf, &fence, reqWidth, reqHeight,
	                                                            reqFormat, reqUsage, &mBufferAge,
	                                                            enableFrameTimestamps ? &frameTimestamps
	                                                                                  : nullptr);
	    ... 如果需要重新分配，则requestBuffer，请求分配
	    if ((result & IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION) || gbuf == nullptr) {
	        if (mReportRemovedBuffers && (gbuf != nullptr)) {
	            mRemovedBuffers.push_back(gbuf);
	        }
	        <!--请求分配-->
	        result = mGraphicBufferProducer->requestBuffer(buf, &gbuf);
	       }
	
	    ...

这里才是为EglSurface(Surface)分配的内存（硬件加速）。Eglmamager的beginFrame结束后，内存也有了，EglContext也在待命，接下来就可以通知OpenGL渲染绘制了。

# 	OpenGL渲染

接着看 OpenGLRenderer

	mCanvas->drawRenderNode(mRootRenderNode.get(), outBounds);，

CanvasContext的mCanvas其实是OpenGLRenderer，
	
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
	
	        // Don't avoid overdraw when visualizing, since that makes it harder to
	        // debug where it's coming from, and when the problem occurs.
	        bool avoidOverdraw = !Properties::debugOverdraw;
	        DeferredDisplayList deferredList(mState.currentClipRect(), avoidOverdraw);
	        DeferStateStruct deferStruct(deferredList, *this, replayFlags);
	        <!--合并-->
	        renderNode->defer(deferStruct, 0);
			 <!--处理文理图层-->
	        flushLayers();
	        <!--设置视窗-->
	        startFrame();
	       <!--flush-->
	        deferredList.flush(*this, dirty);
	    } else {
	        // Even if there is no drawing command(Ex: invisible),
	        // it still needs startFrame to clear buffer and start tiling.
	        startFrame();
	    }
	}

	void OpenGLRenderer::startFrame() {
	    if (mFrameStarted) return;
	    mFrameStarted = true;
	
	    mState.setDirtyClip(true);
	
	    discardFramebuffer(mTilingClip.left, mTilingClip.top, mTilingClip.right, mTilingClip.bottom);
	    <!--似乎只有一个视窗设置比较有用-->
	    mRenderState.setViewport(mState.getWidth(), mState.getHeight());
	
	    // Functors break the tiling extension in pretty spectacular ways
	    // This ensures we don't use tiling when a functor is going to be
	    // invoked during the frame
	    mSuppressTiling = mCaches.hasRegisteredFunctors()
	            || mFirstFrameAfterResize;
	    mFirstFrameAfterResize = false;
	
	    startTilingCurrentClip(true);
	
	    debugOverdraw(true, true);
	
	    clear(mTilingClip.left, mTilingClip.top,
	            mTilingClip.right, mTilingClip.bottom, mOpaque);
	}
	
 deferredList其实是合并后的DrawOp，优化GPU渲染流程， deferredList.flush是真的绘制渲染，这里其实也是便利每个DrawOp，调用自己的
 
 
	void DeferredDisplayList::flush(OpenGLRenderer& renderer, Rect& dirty) {
	    ...
	    // NOTE: depth of the save stack at this point, before playback, should be reflected in
	    // FLUSH_SAVE_STACK_DEPTH, so that save/restores match up correctly
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

随便拿个DrawOp分析，依画点为例



	class DrawPointsOp : public DrawLinesOp {
	public:
	    DrawPointsOp(const float* points, int count, const SkPaint* paint)
	            : DrawLinesOp(points, count, paint) {}
	
	    virtual void applyDraw(OpenGLRenderer& renderer, Rect& dirty) override {
	        renderer.drawPoints(mPoints, mCount, mPaint);
	    }
	
	    virtual void output(int level, uint32_t logFlags) const override {
	        OP_LOG("Draw Points count %d", mCount);
	    }
	
	    virtual const char* name() override { return "DrawPoints"; }
	};

最终调用OpenGLrender的drawPoints


	void OpenGLRenderer::drawPoints(const float* points, int count, const SkPaint* paint) {
	    if (mState.currentlyIgnored() || count < 2) return;
	
	    count &= ~0x1; // round down to nearest two
		<!--构建VertexBuffer-->
	    VertexBuffer buffer;
	    PathTessellator::tessellatePoints(points, count, paint, *currentTransform(), buffer);
	
	    const Rect& bounds = buffer.getBounds();
	    if (quickRejectSetupScissor(bounds.left, bounds.top, bounds.right, bounds.bottom)) {
	        return;
	    }
	
	    int displayFlags = paint->isAntiAlias() ? 0 : kVertexBuffer_Offset;
	    <!--使用buffer paint绘制 -->
	    drawVertexBuffer(buffer, paint, displayFlags);
	
	    mDirty = true;
	}

看到会调用


	void OpenGLRenderer::drawVertexBuffer(float translateX, float translateY,
	        const VertexBuffer& vertexBuffer, const SkPaint* paint, int displayFlags) {
	    // not missing call to quickReject/dirtyLayer, always done at a higher level
	    if (!vertexBuffer.getVertexCount()) {
	        // no vertices to draw
	        return;
	    }
	
	    bool shadowInterp = displayFlags & kVertexBuffer_ShadowInterp;
	    const int transformFlags = TransformFlags::OffsetByFudgeFactor;
	    Glop glop;
	    GlopBuilder(mRenderState, mCaches, &glop)
	            .setRoundRectClipState(currentSnapshot()->roundRectClipState)
	            .setMeshVertexBuffer(vertexBuffer, shadowInterp)
	            .setFillPaint(*paint, currentSnapshot()->alpha)
	            .setTransform(*currentSnapshot(), transformFlags)
	            .setModelViewOffsetRect(translateX, translateY, vertexBuffer.getBounds())
	            .build();
	    renderGlop(glop);
	}
	
drawVertexBuffer其实就跟OpenGL API很贴近了，Vertex也是OpenGL最基础的概念，数据组装好之后，调用RenderState的render，通知GPU状态机绘制，
	
	void OpenGLRenderer::renderGlop(const Glop& glop, GlopRenderType type) {
	    // TODO: It would be best if we could do this before quickRejectSetupScissor()
	    //       changes the scissor test state
	    if (type != GlopRenderType::LayerClear) {
	        // Regular draws need to clear the dirty area on the layer before they start drawing on top
	        // of it. If this draw *is* a layer clear, it skips the clear step (since it would
	        // infinitely recurse)
	        clearLayerRegions();
	    }
	
	    if (mState.getDirtyClip()) {
	        if (mRenderState.scissor().isEnabled()) {
	            setScissorFromClip();
	        }
	
	        setStencilFromClip();
	    }
	    mRenderState.render(glop);
	    if (type == GlopRenderType::Standard && !mRenderState.stencil().isWriteEnabled()) {
	        // TODO: specify more clearly when a draw should dirty the layer.
	        // is writing to the stencil the only time we should ignore this?
	        dirtyLayer(glop.bounds.left, glop.bounds.top, glop.bounds.right, glop.bounds.bottom);
	        mDirty = true;
	    }
	}

RenderState可以看做OpenGL状态机的抽象，如下


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
	    } else if (fill.filterMode == ProgramDescription::kColorMatrix) {
	        glUniformMatrix4fv(mCaches->program().getUniform("colorMatrix"), 1, GL_FALSE,
	                fill.filter.matrix.matrix);
	        glUniform4fv(mCaches->program().getUniform("colorMatrixVector"), 1,
	                fill.filter.matrix.vector);
	    }
	
	    // Round rect clipping uniforms
	    if (glop.roundRectClipState) {
	        // TODO: avoid query, and cache values (or RRCS ptr) in program
	        const RoundRectClipState* state = glop.roundRectClipState;
	        const Rect& innerRect = state->innerRect;
	        glUniform4f(fill.program->getUniform("roundRectInnerRectLTRB"),
	                innerRect.left, innerRect.top,
	                innerRect.right, innerRect.bottom);
	        glUniformMatrix4fv(fill.program->getUniform("roundRectInvTransform"),
	                1, GL_FALSE, &state->matrix.data[0]);
	
	        // add half pixel to round out integer rect space to cover pixel centers
	        float roundedOutRadius = state->radius + 0.5f;
	        glUniform1f(fill.program->getUniform("roundRectRadius"),
	                roundedOutRadius);
	    }
	
	    // --------------------------------
	    // ---------- Mesh setup ----------
	    // --------------------------------
	    // vertices
	    const bool force = meshState().bindMeshBufferInternal(vertices.bufferObject)
	            || (vertices.position != nullptr);
	    meshState().bindPositionVertexPointer(force, vertices.position, vertices.stride);
	
	    // indices
	    meshState().bindIndicesBufferInternal(indices.bufferObject);
	
	    if (vertices.attribFlags & VertexAttribFlags::TextureCoord) {
	        const Glop::Fill::TextureData& texture = fill.texture;
	        // texture always takes slot 0, shader samplers increment from there
	        mCaches->textureState().activateTexture(0);
	
	        if (texture.clamp != GL_INVALID_ENUM) {
	            texture.texture->setWrap(texture.clamp, true, false, texture.target);
	        }
	        if (texture.filter != GL_INVALID_ENUM) {
	            texture.texture->setFilter(texture.filter, true, false, texture.target);
	        }
	
	        mCaches->textureState().bindTexture(texture.target, texture.texture->id);
	        meshState().enableTexCoordsVertexArray();
	        meshState().bindTexCoordsVertexPointer(force, vertices.texCoord, vertices.stride);
	
	        if (texture.textureTransform) {
	            glUniformMatrix4fv(fill.program->getUniform("mainTextureTransform"), 1,
	                    GL_FALSE, &texture.textureTransform->data[0]);
	        }
	    } else {
	        meshState().disableTexCoordsVertexArray();
	    }
	    int colorLocation = -1;
	    if (vertices.attribFlags & VertexAttribFlags::Color) {
	        colorLocation = fill.program->getAttrib("colors");
	        glEnableVertexAttribArray(colorLocation);
	        glVertexAttribPointer(colorLocation, 4, GL_FLOAT, GL_FALSE, vertices.stride, vertices.color);
	    }
	    int alphaLocation = -1;
	    if (vertices.attribFlags & VertexAttribFlags::Alpha) {
	        // NOTE: alpha vertex position is computed assuming no VBO
	        const void* alphaCoords = ((const GLbyte*) vertices.position) + kVertexAlphaOffset;
	        alphaLocation = fill.program->getAttrib("vtxAlpha");
	        glEnableVertexAttribArray(alphaLocation);
	        glVertexAttribPointer(alphaLocation, 1, GL_FLOAT, GL_FALSE, vertices.stride, alphaCoords);
	    }
	    // Shader uniforms
	    SkiaShader::apply(*mCaches, fill.skiaShaderData);
	
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
	            GLsizei drawCount = MathUtils::min(elementsCount, (GLsizei) kMaxNumberOfQuads * 6);
	
	            // rebind pointers without forcing, since initial bind handled above
	            meshState().bindPositionVertexPointer(false, vertexData, vertices.stride);
	            if (vertices.attribFlags & VertexAttribFlags::TextureCoord) {
	                meshState().bindTexCoordsVertexPointer(false,
	                        vertexData + kMeshTextureOffset, vertices.stride);
	            }
	
	            glDrawElements(mesh.primitiveMode, drawCount, GL_UNSIGNED_SHORT, nullptr);
	            elementsCount -= drawCount;
	            vertexData += (drawCount / 6) * 4 * vertices.stride;
	        }
	    } else if (indices.bufferObject || indices.indices) {
	        glDrawElements(mesh.primitiveMode, mesh.elementCount, GL_UNSIGNED_SHORT, indices.indices);
	    } else {
	        glDrawArrays(mesh.primitiveMode, 0, mesh.elementCount);
	    }
	
	    // -----------------------------------
	    // ---------- Mesh teardown ----------
	    // -----------------------------------
	    if (vertices.attribFlags & VertexAttribFlags::Alpha) {
	        glDisableVertexAttribArray(alphaLocation);
	    }
	    if (vertices.attribFlags & VertexAttribFlags::Color) {
	        glDisableVertexAttribArray(colorLocation);
	    }
	}
	
可以看到，经过一步步的设置，变换，预处理，最后调用glDrawElements，通知GPU绘制，等到GPU处理完，函数返回。到这里OpenGL 如何通过GPU来绘制就结束了，还有些问题。
	

* 第一：OpenGL使用的内存跟CPU使用的内存一样吗？

ARM内存共享，但是GPU经常会再拷贝一份数据，可能是为了安全性。

* 第二：CPU预处理的内存比如材质、文理的加载，同Surface（或者EglSurface）使用的内存是同一份吗 

不是，前者是CPU为GPU准备数据用的内存，后者是GPU绘制的内存（提交给SurfaceFlinger），两者不是同一份

* 第三：CPU如何等待GPU处理完成，不会阻塞UI线程吗

不会，等待的线程是render线程，不是UI线程

**OpenGL 绘制用的内存基本都是CPU在APP的内存空间申请的，而真正绘制到画布，提交给SurfaceFlinger的那块内存，是EglSurface拥有的内存，是从匿名共享申请的内存，两者不是同一份。
**



# Android OpenGL栅格化

Android中drawText drawRec drawBitmap 等，能划分几种？首先APP端这些内存是CPU GPU共享，应该不用再来一份，CPU将绘制命令交给GPU，开始绘制 CPU GPU通信

Resterization栅格化是绘制那些Button，Shape，Path，String，Bitmap等组件最基础的操作。它把那些组件拆分到不同的像素上进行显示。这是一个很费时的操作，GPU的引入就是为了加快栅格化的操作。
 
![APP 不可见的时候，占用内存](https://upload-images.jianshu.io/upload_images/1460468-85662ebc583cf6e0.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

Graphics：图形缓冲区队列向屏幕显示像素（包括 GL表面、GL纹理等等）所使用的内存。 （请注意，这是与 CPU 共享的内存，不是 GPU 专用内存。）可能提交多个内存，没来接使用

GL表面内存是从共享内存弄得，但是GL纹理却不是，仅仅是在本APP的有，

OpenGL需要备份的数据，哪些视图 之类的可能APP主线程已经处理回收，但是OpenGL备份了，这些数据也需没来的绘制，似乎Android处理内存泄漏会导致OOM，其他导致OOM的原因基本都不会存在，

Android中每个DrawOp都有自己的数据，有自己的点，变化，对应的纹理也会被拷贝加载到GPU上下文，


看看Android中如何绘制Text

	
	void OpenGLRenderer::drawText(const char* text, int bytesCount, int count, float x, float y,
	        const float* positions, const SkPaint* paint, float totalAdvance, const Rect& bounds,
	        DrawOpMode drawOpMode) {
	
	    if (drawOpMode == DrawOpMode::kImmediate) {
	        // The checks for corner-case ignorable text and quick rejection is only done for immediate
	        // drawing as ops from DeferredDisplayList are already filtered for these
	        if (text == nullptr || count == 0 || mState.currentlyIgnored() || canSkipText(paint) ||
	                quickRejectSetupScissor(bounds)) {
	            return;
	        }
	    }
	
	    const float oldX = x;
	    const float oldY = y;
	
	    const mat4& transform = *currentTransform();
	    const bool pureTranslate = transform.isPureTranslate();
	
	    if (CC_LIKELY(pureTranslate)) {
	        x = floorf(x + transform.getTranslateX() + 0.5f);
	        y = floorf(y + transform.getTranslateY() + 0.5f);
	    }
	
	    int alpha;
	    SkXfermode::Mode mode;
	    getAlphaAndMode(paint, &alpha, &mode);
	
	    FontRenderer& fontRenderer = mCaches.fontRenderer->getFontRenderer(paint);
	
	    if (CC_UNLIKELY(hasTextShadow(paint))) {
	        fontRenderer.setFont(paint, SkMatrix::I());
	        drawTextShadow(paint, text, bytesCount, count, positions, fontRenderer,
	                alpha, oldX, oldY);
	    }
	
	    const bool hasActiveLayer = hasLayer();
	
	    // We only pass a partial transform to the font renderer. That partial
	    // matrix defines how glyphs are rasterized. Typically we want glyphs
	    // to be rasterized at their final size on screen, which means the partial
	    // matrix needs to take the scale factor into account.
	    // When a partial matrix is used to transform glyphs during rasterization,
	    // the mesh is generated with the inverse transform (in the case of scale,
	    // the mesh is generated at 1.0 / scale for instance.) This allows us to
	    // apply the full transform matrix at draw time in the vertex shader.
	    // Applying the full matrix in the shader is the easiest way to handle
	    // rotation and perspective and allows us to always generated quads in the
	    // font renderer which greatly simplifies the code, clipping in particular.
	    SkMatrix fontTransform;
	    bool linearFilter = findBestFontTransform(transform, &fontTransform)
	            || fabs(y - (int) y) > 0.0f
	            || fabs(x - (int) x) > 0.0f;
	    fontRenderer.setFont(paint, fontTransform);
	    fontRenderer.setTextureFiltering(linearFilter);
	
	    // TODO: Implement better clipping for scaled/rotated text
	    const Rect* clip = !pureTranslate ? nullptr : &mState.currentClipRect();
	    Rect layerBounds(FLT_MAX / 2.0f, FLT_MAX / 2.0f, FLT_MIN / 2.0f, FLT_MIN / 2.0f);
	
	    bool status;
	    TextDrawFunctor functor(this, x, y, pureTranslate, alpha, mode, paint);
	
	    // don't call issuedrawcommand, do it at end of batch
	    bool forceFinish = (drawOpMode != DrawOpMode::kDefer);
	    if (CC_UNLIKELY(paint->getTextAlign() != SkPaint::kLeft_Align)) {
	        SkPaint paintCopy(*paint);
	        paintCopy.setTextAlign(SkPaint::kLeft_Align);
	        status = fontRenderer.renderPosText(&paintCopy, clip, text, 0, bytesCount, count, x, y,
	                positions, hasActiveLayer ? &layerBounds : nullptr, &functor, forceFinish);
	    } else {
	        status = fontRenderer.renderPosText(paint, clip, text, 0, bytesCount, count, x, y,
	                positions, hasActiveLayer ? &layerBounds : nullptr, &functor, forceFinish);
	    }
	
	    if ((status || drawOpMode != DrawOpMode::kImmediate) && hasActiveLayer) {
	        if (!pureTranslate) {
	            transform.mapRect(layerBounds);
	        }
	        dirtyLayerUnchecked(layerBounds, getRegion());
	    }
	
	    drawTextDecorations(totalAdvance, oldX, oldY, paint);
	
	    mDirty = true;
	}


	
	bool FontRenderer::renderPosText(const SkPaint* paint, const Rect* clip, const char *text,
	        uint32_t startIndex, uint32_t len, int numGlyphs, int x, int y,
	        const float* positions, Rect* bounds, TextDrawFunctor* functor, bool forceFinish) {
	    if (!mCurrentFont) {
	        ALOGE("No font set");
	        return false;
	    }
	
	    initRender(clip, bounds, functor);
	    mCurrentFont->render(paint, text, startIndex, len, numGlyphs, x, y, positions);
	
	    if (forceFinish) {
	        finishRender();
	    }
	
	    return mDrawn;
	}
	
	
		void FontRenderer::setFont(const SkPaint* paint, const SkMatrix& matrix) {
	    mCurrentFont = Font::create(this, paint, matrix);
	}
	
	
		
	void Font::render(const SkPaint* paint, const char *text, uint32_t start, uint32_t len,
	        int numGlyphs, const SkPath* path, float hOffset, float vOffset) {
	    if (numGlyphs == 0 || text == nullptr || len == 0) {
	        return;
	    }
	
	    text += start;
	
	    int glyphsCount = 0;
	    SkFixed prevRsbDelta = 0;
	
	    float penX = 0.0f;
	
	    SkPoint position;
	    SkVector tangent;
	
	    SkPathMeasure measure(*path, false);
	    float pathLength = SkScalarToFloat(measure.getLength());
	
	    if (paint->getTextAlign() != SkPaint::kLeft_Align) {
	        float textWidth = SkScalarToFloat(paint->measureText(text, len));
	        float pathOffset = pathLength;
	        if (paint->getTextAlign() == SkPaint::kCenter_Align) {
	            textWidth *= 0.5f;
	            pathOffset *= 0.5f;
	        }
	        penX += pathOffset - textWidth;
	    }
	
	    while (glyphsCount < numGlyphs && penX < pathLength) {
	        glyph_t glyph = GET_GLYPH(text);
	
	        if (IS_END_OF_STRING(glyph)) {
	            break;
	        }
	
	        CachedGlyphInfo* cachedGlyph = getCachedGlyph(paint, glyph);
	        penX += SkFixedToFloat(AUTO_KERN(prevRsbDelta, cachedGlyph->mLsbDelta));
	        prevRsbDelta = cachedGlyph->mRsbDelta;
	
	        if (cachedGlyph->mIsValid && cachedGlyph->mCacheTexture) {
	            drawCachedGlyph(cachedGlyph, penX, hOffset, vOffset, measure, &position, &tangent);
	        }
	
	        penX += SkFixedToFloat(cachedGlyph->mAdvanceX);
	
	        glyphsCount++;
	    }
	}
	
Android 采用了FreeType字体光栅化库。它可以用来将字符栅格化并映射成位图以及提供其他字体相关业务的支持。
	
	
>Graphics: Memory used for graphics buffer queues to display pixels to the screen, including GL surfaces, GL textures, and so on. (Note that this is memory shared with the CPU, not dedicated GPU memory.)


这里需要注意的是GL surfaces所对应的内存中，并不会存在textures的内存，textures是CPU申请是，之后交给GPU，注意这里的dequeBuffer，GL surfaces可以申请多块内存，但是同一时刻，好像只会提交一块，对于硬件加速，什么时候，申请这块内存呢？什么时候创建GLSurface，在创建CanvasContext的时候，就会创建GLSurface，这里面CanvasContext的render其实就是OpenGLRenderer

	bool CanvasContext::initialize(ANativeWindow* window) {
	    setSurface(window);
	    if (mCanvas) return false;
	    mCanvas = new OpenGLRenderer(mRenderThread.renderState());
	    mCanvas->initProperties();
	    return true;
	}
		
		
	void CanvasContext::setSurface(ANativeWindow* window) {
	    ATRACE_CALL();
	
	    mNativeWindow = window;
	
	    if (mEglSurface != EGL_NO_SURFACE) {
	        mEglManager.destroySurface(mEglSurface);
	        mEglSurface = EGL_NO_SURFACE;
	    }
	
	    if (window) {
	    <!--创建EglSurface-->
	        mEglSurface = mEglManager.createSurface(window);
	    }
	
	    if (mEglSurface != EGL_NO_SURFACE) {
	        const bool preserveBuffer = (mSwapBehavior != kSwap_discardBuffer);
	        mBufferPreserved = mEglManager.setPreserveBuffer(mEglSurface, preserveBuffer);
	        mHaveNewSurface = true;

		<!--opengl的makecurrent逻辑-->
	        makeCurrent();
	    } else {
	        mRenderThread.removeFrameCallback(this);
	    }
	}
	
		void CanvasContext::makeCurrent() {
	    // TODO: Figure out why this workaround is needed, see b/13913604
	    // In the meantime this matches the behavior of GLRenderer, so it is not a regression
	    EGLint error = 0;
	    mHaveNewSurface |= mEglManager.makeCurrent(mEglSurface, &error);
	    if (error) {
	        setSurface(nullptr);
	    }
	}

CanvasContext又是在什么时候创建的呢？RenderProxy创建的时候，会创建

	RenderProxy::RenderProxy(bool translucent, RenderNode* rootRenderNode, IContextFactory* contextFactory)
	        : mRenderThread(RenderThread::getInstance())
	        , mContext(nullptr) {
	    SETUP_TASK(createContext);
	    args->translucent = translucent;
	    args->rootRenderNode = rootRenderNode;
	    args->thread = &mRenderThread;
	    args->contextFactory = contextFactory;
	    // CanvasContext创建CanvasContext
	    mContext = (CanvasContext*) postAndWait(task);
	    mDrawFrameTask.setContext(&mRenderThread, mContext);
	}
	
RenderThread::getInstance()其实标识，renderThread是一个单例，只有一个render线程, ThreadedRenderer握有RenderProxy的native指针

    ThreadedRenderer(Context context, boolean translucent) {
        final TypedArray a = context.obtainStyledAttributes(null, R.styleable.Lighting, 0, 0);
        mLightY = a.getDimension(R.styleable.Lighting_lightY, 0);
        mLightZ = a.getDimension(R.styleable.Lighting_lightZ, 0);
        mLightRadius = a.getDimension(R.styleable.Lighting_lightRadius, 0);
        mAmbientShadowAlpha =
                (int) (255 * a.getFloat(R.styleable.Lighting_ambientShadowAlpha, 0) + 0.5f);
        mSpotShadowAlpha = (int) (255 * a.getFloat(R.styleable.Lighting_spotShadowAlpha, 0) + 0.5f);
        a.recycle();

        long rootNodePtr = nCreateRootRenderNode();
        mRootNode = RenderNode.adopt(rootNodePtr);
        mRootNode.setClipToBounds(false);
        mNativeProxy = nCreateProxy(translucent, rootNodePtr);

        ProcessInitializer.sInstance.init(context, mNativeProxy);

        loadSystemProperties();
    }	
		
而ThreadedRenderer其实就是View中AttachInfo.mHardwareRenderer

	public class ThreadedRenderer extends HardwareRenderer {
	    private static final String LOGTAG = "ThreadedRenderer";
	    ...
	    	    
    public ViewRootImpl(Context context, Display display) {
        mContext = context;
        mWindowSession = WindowManagerGlobal.getWindowSession();
       ...
        mAttachInfo = new View.AttachInfo(mWindowSession, mWindow, display, this, mHandler, this);
       ...
       
    private void enableHardwareAcceleration(WindowManager.LayoutParams attrs) {
        mAttachInfo.mHardwareAccelerated = false;
        mAttachInfo.mHardwareAccelerationRequested = false;
 
       final boolean hardwareAccelerated =
                (attrs.flags & WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED) != 0;

        if (hardwareAccelerated) {
            if (!HardwareRenderer.isAvailable()) {
                return;
            }
                mAttachInfo.mHardwareRenderer = HardwareRenderer.create(mContext, translucent);
               ...
        }
 
 接着创建ThreadedRenderer
    
        static HardwareRenderer create(Context context, boolean translucent) {
        HardwareRenderer renderer = null;
        if (DisplayListCanvas.isAvailable()) {
            renderer = new ThreadedRenderer(context, translucent);
        }
        return renderer;
    }
    
为了方便，我们画个图    


一般，eglmanager.cpp的函数会调用eglApi.cpp会进一步调用egl.cpp的函数

>eglApi.cpp

	// 这里创建了EglSurface，并且做了些设置
		
	EGLSurface eglCreateWindowSurface(  EGLDisplay dpy, EGLConfig config,
	                                    NativeWindowType window,
	                                    const EGLint *attrib_list)
	{
	    clearError();
	
	    egl_connection_t* cnx = NULL;
	    egl_display_ptr dp = validate_display_connection(dpy, cnx);
	    if (dp) {
	        EGLDisplay iDpy = dp->disp.dpy;
	
	        int result = native_window_api_connect(window, NATIVE_WINDOW_API_EGL);
	        if (result != OK) {
	            return setError(EGL_BAD_ALLOC, EGL_NO_SURFACE);
	        }
	
	        // Set the native window's buffers format to match what this config requests.
	        // Whether to use sRGB gamma is not part of the EGLconfig, but is part
	        // of our native format. So if sRGB gamma is requested, we have to
	        // modify the EGLconfig's format before setting the native window's
	        // format.
	
	        // by default, just pick RGBA_8888
	        EGLint format = HAL_PIXEL_FORMAT_RGBA_8888;
	        android_dataspace dataSpace = HAL_DATASPACE_UNKNOWN;
	
	        EGLint a = 0;
	        cnx->egl.eglGetConfigAttrib(iDpy, config, EGL_ALPHA_SIZE, &a);
	        if (a > 0) {
	            // alpha-channel requested, there's really only one suitable format
	            format = HAL_PIXEL_FORMAT_RGBA_8888;
	        } else {
	            EGLint r, g, b;
	            r = g = b = 0;
	            cnx->egl.eglGetConfigAttrib(iDpy, config, EGL_RED_SIZE,   &r);
	            cnx->egl.eglGetConfigAttrib(iDpy, config, EGL_GREEN_SIZE, &g);
	            cnx->egl.eglGetConfigAttrib(iDpy, config, EGL_BLUE_SIZE,  &b);
	            EGLint colorDepth = r + g + b;
	            if (colorDepth <= 16) {
	                format = HAL_PIXEL_FORMAT_RGB_565;
	            } else {
	                format = HAL_PIXEL_FORMAT_RGBX_8888;
	            }
	        }
	
	        // now select a corresponding sRGB format if needed
	        if (attrib_list && dp->haveExtension("EGL_KHR_gl_colorspace")) {
	            for (const EGLint* attr = attrib_list; *attr != EGL_NONE; attr += 2) {
	                if (*attr == EGL_GL_COLORSPACE_KHR) {
	                    if (ENABLE_EGL_KHR_GL_COLORSPACE) {
	                        dataSpace = modifyBufferDataspace(dataSpace, *(attr+1));
	                    } else {
	                        // Normally we'd pass through unhandled attributes to
	                        // the driver. But in case the driver implements this
	                        // extension but we're disabling it, we want to prevent
	                        // it getting through -- support will be broken without
	                        // our help.
	                        ALOGE("sRGB window surfaces not supported");
	                        return setError(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
	                    }
	                }
	            }
	        }
	
	        if (format != 0) {
	            int err = native_window_set_buffers_format(window, format);
	            if (err != 0) {
	                ALOGE("error setting native window pixel format: %s (%d)",
	                        strerror(-err), err);
	                native_window_api_disconnect(window, NATIVE_WINDOW_API_EGL);
	                return setError(EGL_BAD_NATIVE_WINDOW, EGL_NO_SURFACE);
	            }
	        }
	
	        if (dataSpace != 0) {
	            int err = native_window_set_buffers_data_space(window, dataSpace);
	            if (err != 0) {
	                ALOGE("error setting native window pixel dataSpace: %s (%d)",
	                        strerror(-err), err);
	                native_window_api_disconnect(window, NATIVE_WINDOW_API_EGL);
	                return setError(EGL_BAD_NATIVE_WINDOW, EGL_NO_SURFACE);
	            }
	        }
	
	        // the EGL spec requires that a new EGLSurface default to swap interval
	        // 1, so explicitly set that on the window here.
	        ANativeWindow* anw = reinterpret_cast<ANativeWindow*>(window);
	        anw->setSwapInterval(anw, 1);
	
	        EGLSurface surface = cnx->egl.eglCreateWindowSurface(
	                iDpy, config, window, attrib_list);
	        if (surface != EGL_NO_SURFACE) {
	            // egl_surface_t
	            egl_surface_t* s = new egl_surface_t(dp.get(), config, window,
	                    surface, cnx);
	            return s;
	        }
	
	        // EGLSurface creation failed
	        native_window_set_buffers_format(window, 0);
	        native_window_api_disconnect(window, NATIVE_WINDOW_API_EGL);
	    }
	    return EGL_NO_SURFACE;
	}
	

真正需要内存的时候是渲染的时候，否则凑是cpu在本地内存处理，而不需要共享内存，这个时候就是EglManager的beginFrame，

	void EglManager::beginFrame(EGLSurface surface, EGLint* width, EGLint* height) {
	    LOG_ALWAYS_FATAL_IF(surface == EGL_NO_SURFACE,
	            "Tried to beginFrame on EGL_NO_SURFACE!");
	    makeCurrent(surface);
	    if (width) {
	        eglQuerySurface(mEglDisplay, surface, EGL_WIDTH, width);
	    }
	    if (height) {
	        eglQuerySurface(mEglDisplay, surface, EGL_HEIGHT, height);
	    }
	    eglBeginFrame(mEglDisplay, surface);
	}



	bool EglManager::makeCurrent(EGLSurface surface, EGLint* errOut) {
	    if (isCurrent(surface)) return false;
	
	    if (surface == EGL_NO_SURFACE) {
	        // Ensure we always have a valid surface & context
	        surface = mPBufferSurface;
	    }
	    if (!eglMakeCurrent(mEglDisplay, surface, surface, mEglContext)) {
	        if (errOut) {
	            *errOut = eglGetError();
	            ALOGW("Failed to make current on surface %p, error=%s",
	                    (void*)surface, egl_error_str(*errOut));
	        } else {
	            LOG_ALWAYS_FATAL("Failed to make current on surface %p, error=%s",
	                    (void*)surface, egl_error_str());
	        }
	    }
	    mCurrentSurface = surface;
	    return true;
	}



	EGLBoolean eglMakeCurrent(  EGLDisplay dpy, EGLSurface draw,
	                            EGLSurface read, EGLContext ctx)
	{
	    clearError();
	
	    egl_display_ptr dp = validate_display(dpy);
	    if (!dp) return setError(EGL_BAD_DISPLAY, EGL_FALSE);
	
	    // If ctx is not EGL_NO_CONTEXT, read is not EGL_NO_SURFACE, or draw is not
	    // EGL_NO_SURFACE, then an EGL_NOT_INITIALIZED error is generated if dpy is
	    // a valid but uninitialized display.
	    if ( (ctx != EGL_NO_CONTEXT) || (read != EGL_NO_SURFACE) ||
	         (draw != EGL_NO_SURFACE) ) {
	        if (!dp->isReady()) return setError(EGL_NOT_INITIALIZED, EGL_FALSE);
	    }
	
	    // get a reference to the object passed in
	    ContextRef _c(dp.get(), ctx);
	    SurfaceRef _d(dp.get(), draw);
	    SurfaceRef _r(dp.get(), read);
	
	    // validate the context (if not EGL_NO_CONTEXT)
	    if ((ctx != EGL_NO_CONTEXT) && !_c.get()) {
	        // EGL_NO_CONTEXT is valid
	        return setError(EGL_BAD_CONTEXT, EGL_FALSE);
	    }
	
	    // these are the underlying implementation's object
	    EGLContext impl_ctx  = EGL_NO_CONTEXT;
	    EGLSurface impl_draw = EGL_NO_SURFACE;
	    EGLSurface impl_read = EGL_NO_SURFACE;
	
	    // these are our objects structs passed in
	    egl_context_t       * c = NULL;
	    egl_surface_t const * d = NULL;
	    egl_surface_t const * r = NULL;
	
	    // these are the current objects structs
	    egl_context_t * cur_c = get_context(getContext());
	
	    if (ctx != EGL_NO_CONTEXT) {
	        c = get_context(ctx);
	        impl_ctx = c->context;
	    } else {
	        // no context given, use the implementation of the current context
	        if (draw != EGL_NO_SURFACE || read != EGL_NO_SURFACE) {
	            // calling eglMakeCurrent( ..., !=0, !=0, EGL_NO_CONTEXT);
	            return setError(EGL_BAD_MATCH, EGL_FALSE);
	        }
	        if (cur_c == NULL) {
	            // no current context
	            // not an error, there is just no current context.
	            return EGL_TRUE;
	        }
	    }
	
	    // retrieve the underlying implementation's draw EGLSurface
	    if (draw != EGL_NO_SURFACE) {
	        if (!_d.get()) return setError(EGL_BAD_SURFACE, EGL_FALSE);
	        d = get_surface(draw);
	        impl_draw = d->surface;
	    }
	
	    // retrieve the underlying implementation's read EGLSurface
	    if (read != EGL_NO_SURFACE) {
	        if (!_r.get()) return setError(EGL_BAD_SURFACE, EGL_FALSE);
	        r = get_surface(read);
	        impl_read = r->surface;
	    }
	
	
	    EGLBoolean result = dp->makeCurrent(c, cur_c,
	            draw, read, ctx,
	            impl_draw, impl_read, impl_ctx);
	
	    if (result == EGL_TRUE) {
	        if (c) {
	            setGLHooksThreadSpecific(c->cnx->hooks[c->version]);
	            egl_tls_t::setContext(ctx);
	#if EGL_TRACE
	            if (getEGLDebugLevel() > 0)
	                GLTrace_eglMakeCurrent(c->version, c->cnx->hooks[c->version], ctx);
	#endif
	            _c.acquire();
	            _r.acquire();
	            _d.acquire();
	        } else {
	            setGLHooksThreadSpecific(&gHooksNoContext);
	            egl_tls_t::setContext(EGL_NO_CONTEXT);
	        }
	    } else {
	        // this will ALOGE the error
	        egl_connection_t* const cnx = &gEGLImpl;
	        result = setError(cnx->egl.eglGetError(), EGL_FALSE);
	    }
	    return result;
	}

会一步步调用到egl.cpp的eglmakeCurrent


	EGLBoolean eglMakeCurrent(  EGLDisplay dpy, EGLSurface draw,
	                            EGLSurface read, EGLContext ctx)
	{
	    ...
	      if (current_ctx) {
                egl_context_t* c = egl_context_t::context(current_ctx);
                egl_surface_t* d = (egl_surface_t*)c->draw;
                egl_surface_t* r = (egl_surface_t*)c->read;
	                if (d) {
	                if (d->connect() == EGL_FALSE) {
	                    return EGL_FALSE;
	                }
	                d->ctx = ctx;
	                d->bindDrawSurface(gl);
	            }
	            
	            
EGLSurface 其实就是egl_context_t的draw 那么EGLSurface之前说了，是一个egl_surface_t，它的connect其实就是egl_window_surface_v2_t的connect，


	
	EGLBoolean egl_window_surface_v2_t::connect() 
	{
	    // we're intending to do software rendering
	    native_window_set_usage(nativeWindow, 
	            GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
	
	    // dequeue a buffer
	    int fenceFd = -1;
	    if (nativeWindow->dequeueBuffer(nativeWindow, &buffer,
	            &fenceFd) != NO_ERROR) {
	        return setError(EGL_BAD_ALLOC, EGL_FALSE);
	    }
	
	    // wait for the buffer
	    sp<Fence> fence(new Fence(fenceFd));
	    if (fence->wait(Fence::TIMEOUT_NEVER) != NO_ERROR) {
	        nativeWindow->cancelBuffer(nativeWindow, buffer, fenceFd);
	        return setError(EGL_BAD_ALLOC, EGL_FALSE);
	    }
	
	    // allocate a corresponding depth-buffer
	    
	    <!--分配相应的深度缓存，这里其实是很耗内存的，中间内存-->
	    width = buffer->width;
	    height = buffer->height;
	    if (depth.format) {
	        depth.width   = width;
	        depth.height  = height;
	        depth.stride  = depth.width; // use the width here
	        uint64_t allocSize = static_cast<uint64_t>(depth.stride) *
	                static_cast<uint64_t>(depth.height) * 2;
	        if (depth.stride < 0 || depth.height > INT_MAX ||
	                allocSize > UINT32_MAX) {
	            return setError(EGL_BAD_ALLOC, EGL_FALSE);
	        }
	        depth.data  = (GGLubyte*)malloc(allocSize);
	        if (depth.data == 0) {
	            return setError(EGL_BAD_ALLOC, EGL_FALSE);
	        }
	    }
	
	    // keep a reference on the buffer
	    buffer->common.incRef(&buffer->common);
	
	    // pin the buffer down
	    if (lock(buffer, GRALLOC_USAGE_SW_READ_OFTEN | 
	            GRALLOC_USAGE_SW_WRITE_OFTEN, &bits) != NO_ERROR) {
	        ALOGE("connect() failed to lock buffer %p (%ux%u)",
	                buffer, buffer->width, buffer->height);
	        return setError(EGL_BAD_ACCESS, EGL_FALSE);
	        // FIXME: we should make sure we're not accessing the buffer anymore
	    }
	    return EGL_TRUE;
	}
	
在这里看到了nativeWindow->dequeueBuffer，其实就是调用surface.cpp的dequeuebuffer，egl_window_surface_v2_t中有一个buffer，ANativeWindowBuffer*   buffer;就是为了指向这块缓存的。还有个    ANativeWindowBuffer*   previousBuffer;为了指向之前的，eglmakeCurrent并不是一次性的，而是会经常调用，所以可能申请多块内存，这里缓存了之前的一个。内存操作的函数最终都调用egl_window_surface_v2_t的函数。到这里看到了如何分配。

那么如何使用的呢？看一个绘制

都会调用renderState的renderGlop
	
	void OpenGLRenderer::renderGlop(const Glop& glop, GlopRenderType type) {
	    // TODO: It would be best if we could do this before quickRejectSetupScissor()
	    //       changes the scissor test state
	    if (type != GlopRenderType::LayerClear) {
	        // Regular draws need to clear the dirty area on the layer before they start drawing on top
	        // of it. If this draw *is* a layer clear, it skips the clear step (since it would
	        // infinitely recurse)
	        clearLayerRegions();
	    }
	
	    if (mState.getDirtyClip()) {
	        if (mRenderState.scissor().isEnabled()) {
	            setScissorFromClip();
	        }
	
	        setStencilFromClip();
	    }
	    mRenderState.render(glop);
	    if (type == GlopRenderType::Standard && !mRenderState.stencil().isWriteEnabled()) {
	        // TODO: specify more clearly when a draw should dirty the layer.
	        // is writing to the stencil the only time we should ignore this?
	        dirtyLayer(glop.bounds.left, glop.bounds.top, glop.bounds.right, glop.bounds.bottom);
	        mDirty = true;
	    }
	}



	
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
	    } else if (fill.filterMode == ProgramDescription::kColorMatrix) {
	        glUniformMatrix4fv(mCaches->program().getUniform("colorMatrix"), 1, GL_FALSE,
	                fill.filter.matrix.matrix);
	        glUniform4fv(mCaches->program().getUniform("colorMatrixVector"), 1,
	                fill.filter.matrix.vector);
	    }
	
	    // Round rect clipping uniforms
	    if (glop.roundRectClipState) {
	        // TODO: avoid query, and cache values (or RRCS ptr) in program
	        const RoundRectClipState* state = glop.roundRectClipState;
	        const Rect& innerRect = state->innerRect;
	        glUniform4f(fill.program->getUniform("roundRectInnerRectLTRB"),
	                innerRect.left, innerRect.top,
	                innerRect.right, innerRect.bottom);
	        glUniformMatrix4fv(fill.program->getUniform("roundRectInvTransform"),
	                1, GL_FALSE, &state->matrix.data[0]);
	
	        // add half pixel to round out integer rect space to cover pixel centers
	        float roundedOutRadius = state->radius + 0.5f;
	        glUniform1f(fill.program->getUniform("roundRectRadius"),
	                roundedOutRadius);
	    }
	
	    // --------------------------------
	    // ---------- Mesh setup ----------
	    // --------------------------------
	    // vertices
	    const bool force = meshState().bindMeshBufferInternal(vertices.bufferObject)
	            || (vertices.position != nullptr);
	    meshState().bindPositionVertexPointer(force, vertices.position, vertices.stride);
	
	    // indices
	    meshState().bindIndicesBufferInternal(indices.bufferObject);
	
	    if (vertices.attribFlags & VertexAttribFlags::TextureCoord) {
	        const Glop::Fill::TextureData& texture = fill.texture;
	        // texture always takes slot 0, shader samplers increment from there
	        mCaches->textureState().activateTexture(0);
	
	        if (texture.clamp != GL_INVALID_ENUM) {
	            texture.texture->setWrap(texture.clamp, true, false, texture.target);
	        }
	        if (texture.filter != GL_INVALID_ENUM) {
	            texture.texture->setFilter(texture.filter, true, false, texture.target);
	        }
	
	        mCaches->textureState().bindTexture(texture.target, texture.texture->id);
	        meshState().enableTexCoordsVertexArray();
	        meshState().bindTexCoordsVertexPointer(force, vertices.texCoord, vertices.stride);
	
	        if (texture.textureTransform) {
	            glUniformMatrix4fv(fill.program->getUniform("mainTextureTransform"), 1,
	                    GL_FALSE, &texture.textureTransform->data[0]);
	        }
	    } else {
	        meshState().disableTexCoordsVertexArray();
	    }
	    int colorLocation = -1;
	    if (vertices.attribFlags & VertexAttribFlags::Color) {
	        colorLocation = fill.program->getAttrib("colors");
	        glEnableVertexAttribArray(colorLocation);
	        glVertexAttribPointer(colorLocation, 4, GL_FLOAT, GL_FALSE, vertices.stride, vertices.color);
	    }
	    int alphaLocation = -1;
	    if (vertices.attribFlags & VertexAttribFlags::Alpha) {
	        // NOTE: alpha vertex position is computed assuming no VBO
	        const void* alphaCoords = ((const GLbyte*) vertices.position) + kVertexAlphaOffset;
	        alphaLocation = fill.program->getAttrib("vtxAlpha");
	        glEnableVertexAttribArray(alphaLocation);
	        glVertexAttribPointer(alphaLocation, 1, GL_FLOAT, GL_FALSE, vertices.stride, alphaCoords);
	    }
	    // Shader uniforms
	    SkiaShader::apply(*mCaches, fill.skiaShaderData);
	
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
	            GLsizei drawCount = MathUtils::min(elementsCount, (GLsizei) kMaxNumberOfQuads * 6);
	
	            // rebind pointers without forcing, since initial bind handled above
	            meshState().bindPositionVertexPointer(false, vertexData, vertices.stride);
	            if (vertices.attribFlags & VertexAttribFlags::TextureCoord) {
	                meshState().bindTexCoordsVertexPointer(false,
	                        vertexData + kMeshTextureOffset, vertices.stride);
	            }
	
	            glDrawElements(mesh.primitiveMode, drawCount, GL_UNSIGNED_SHORT, nullptr);
	            elementsCount -= drawCount;
	            vertexData += (drawCount / 6) * 4 * vertices.stride;
	        }
	    } else if (indices.bufferObject || indices.indices) {
	        glDrawElements(mesh.primitiveMode, mesh.elementCount, GL_UNSIGNED_SHORT, indices.indices);
	    } else {
	        glDrawArrays(mesh.primitiveMode, 0, mesh.elementCount);
	    }
	
	    // -----------------------------------
	    // ---------- Mesh teardown ----------
	    // -----------------------------------
	    if (vertices.attribFlags & VertexAttribFlags::Alpha) {
	        glDisableVertexAttribArray(alphaLocation);
	    }
	    if (vertices.attribFlags & VertexAttribFlags::Color) {
	        glDisableVertexAttribArray(colorLocation);
	    }
	}
	
最后调用glDrawArrays或者glDrawElements绘制，这里就是往EGLsurface对应的内存进行绘制


	
	static void drawIndexedPrimitivesTriangleFanOrStrip(ogles_context_t* c,
	        GLsizei count, const GLvoid *indices, int winding)
	{
	    // winding == 2 : fan
	    // winding == 1 : strip
	
	    if (ggl_unlikely(count < 3))
	        return;
	
	    vertex_t * const v = c->vc.vBuffer;
	    vertex_t* v0 = v;
	    vertex_t* v1 = v+1;
	    vertex_t* v2;
	
	    const int type = (c->arrays.indicesType == GL_UNSIGNED_BYTE);
	    c->arrays.compileElement(c, v0, read_index(type, indices));
	    c->arrays.compileElement(c, v1, read_index(type, indices));
	    count -= 2;
	
	    // note: GCC 4.1.1 here makes a prety interesting optimization
	    // where it duplicates the loop below based on c->arrays.indicesType
	
	    do {
	        v2 = fetch_vertex(c, read_index(type, indices));
	        const uint32_t cc = v0->flags & v1->flags & v2->flags;
	        if (ggl_likely(!(cc & vertex_t::CLIP_ALL)))
	            c->prims.renderTriangle(c, v0, v1, v2);
	        vertex_t* & consumed = ((winding^=1) ? v1 : v0);
	        consumed->locked = 0;
	        consumed = v2;
	        count--;
	    } while (count);
	    v0->locked = v1->locked = 0;
	    v2->locked = 0;
	}

配置GPU，放置好内存，通知执行命令，之前很多GPU设置的命令，比如绑定材质，加载材质图形，等都是配置，配置后，通知执行CPU等待，另外，之前也为GPU设置好了输出位置，那么就是之前的EglSurface对应的内存，
	
第一步：CPU从文件系统里读出原始数据，分离出图形数据，然后放在系统内存中，这个时候GPU在发呆。
第二步：CPU准备把图形数据交给GPU，这时系统总线上开始忙了，数据将从系统内存拷贝到GPU的显存里。
第三步：CPU要求GPU开始数据处理，现在换CPU发呆了，而GPU开始忙碌工作。当然CPU还是会定期询问一下GPU忙得怎么样了。
第四步：GPU开始用自己的工作间（GPU核心电路）处理数据，处理后的数据还是放在显存里面，CPU还在继续发呆。
第五步：图形数据处理完成后，GPU告诉CPU，我忙完了，准备输出或者已经输出。于是CPU开始接手，读出下一段数据，并告诉GPU可以歇会了，然后返回第一步。

注意，在Andorid中，只有最终绘制的内存才会被OpenGL绘制到EglSurface对应的内存中，中间比如纹理材质加载，都是加载到本地独有的内存，不是写入到SF那面要处理的内存中。

CPU要等待OpenGL处理完，处理完之后，CPU通知SF去合成

	
	EGLBoolean egl_window_surface_v2_t::swapBuffers()
	{
	    if (!buffer) {
	        return setError(EGL_BAD_ACCESS, EGL_FALSE);
	    }
	    
	    /*
	     * Handle eglSetSwapRectangleANDROID()
	     * We copyback from the front buffer 
	     */
	    if (!dirtyRegion.isEmpty()) {
	        dirtyRegion.andSelf(Rect(buffer->width, buffer->height));
	        if (previousBuffer) {
	            // This was const Region copyBack, but that causes an
	            // internal compile error on simulator builds
	            /*const*/ Region copyBack(Region::subtract(oldDirtyRegion, dirtyRegion));
	            if (!copyBack.isEmpty()) {
	                void* prevBits;
	                if (lock(previousBuffer, 
	                        GRALLOC_USAGE_SW_READ_OFTEN, &prevBits) == NO_ERROR) {
	                    // copy from previousBuffer to buffer
	                    copyBlt(buffer, bits, previousBuffer, prevBits, copyBack);
	                    unlock(previousBuffer);
	                }
	            }
	        }
	        oldDirtyRegion = dirtyRegion;
	    }
	
	    if (previousBuffer) {
	        previousBuffer->common.decRef(&previousBuffer->common); 
	        previousBuffer = 0;
	    }
	    
	    unlock(buffer);
	    previousBuffer = buffer;
	    nativeWindow->queueBuffer(nativeWindow, buffer, -1);
	    buffer = 0;
	
	    // dequeue a new buffer
	    int fenceFd = -1;
	    if (nativeWindow->dequeueBuffer(nativeWindow, &buffer, &fenceFd) == NO_ERROR) {
	        sp<Fence> fence(new Fence(fenceFd));
	        if (fence->wait(Fence::TIMEOUT_NEVER)) {
	            nativeWindow->cancelBuffer(nativeWindow, buffer, fenceFd);
	            return setError(EGL_BAD_ALLOC, EGL_FALSE);
	        }
	
	        // reallocate the depth-buffer if needed
	        if ((width != buffer->width) || (height != buffer->height)) {
	            // TODO: we probably should reset the swap rect here
	            // if the window size has changed
	            width = buffer->width;
	            height = buffer->height;
	            if (depth.data) {
	                free(depth.data);
	                depth.width   = width;
	                depth.height  = height;
	                depth.stride  = buffer->stride;
	                uint64_t allocSize = static_cast<uint64_t>(depth.stride) *
	                        static_cast<uint64_t>(depth.height) * 2;
	                if (depth.stride < 0 || depth.height > INT_MAX ||
	                        allocSize > UINT32_MAX) {
	                    setError(EGL_BAD_ALLOC, EGL_FALSE);
	                    return EGL_FALSE;
	                }
	                depth.data    = (GGLubyte*)malloc(allocSize);
	                if (depth.data == 0) {
	                    setError(EGL_BAD_ALLOC, EGL_FALSE);
	                    return EGL_FALSE;
	                }
	            }
	        }
	
	        // keep a reference on the buffer
	        buffer->common.incRef(&buffer->common);
	
	        // finally pin the buffer down
	        if (lock(buffer, GRALLOC_USAGE_SW_READ_OFTEN |
	                GRALLOC_USAGE_SW_WRITE_OFTEN, &bits) != NO_ERROR) {
	            return setError(EGL_BAD_ACCESS, EGL_FALSE);
	            // FIXME: we should make sure we're not accessing the buffer anymore
	        }
	    } else {
	        return setError(EGL_BAD_CURRENT_SURFACE, EGL_FALSE);
	    }
	
	    return EGL_TRUE;
	}
	
不过，这里要注意，虽然是CPU等待，但是并非UI线程中等待，而是在渲染线程中等待，UI线程不受影响。

GPU实际上是一组图形函数的集合，而这些函数由硬件实现

CPU会将交给GPU的命令进行编程，很明显，这个挺像函数执行的模型，提供执行函数指令+数据，让GPU执行，


![CPU与GPU通信](https://upload-images.jianshu.io/upload_images/1460468-e5d82359c2e55b1c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# CPU GPU 同步机制

	bool DrawFrameTask::syncFrameState(TreeInfo& info) {
	    ATRACE_CALL();
	    int64_t vsync = mFrameInfo[static_cast<int>(FrameInfoIndex::Vsync)];
	    mRenderThread->timeLord().vsyncReceived(vsync);
	    mContext->makeCurrent();
	    Caches::getInstance().textureCache.resetMarkInUse(mContext);
	
	    for (size_t i = 0; i < mLayers.size(); i++) {
	        mContext->processLayerUpdate(mLayers[i].get());
	    }
	    mLayers.clear();
	    mContext->prepareTree(info, mFrameInfo, mSyncQueued);
	
	    // This is after the prepareTree so that any pending operations
	    // (RenderNode tree state, prefetched layers, etc...) will be flushed.
	    if (CC_UNLIKELY(!mContext->hasSurface())) {
	        mSyncResult |= kSync_LostSurfaceRewardIfFound;
	    }
	
	    if (info.out.hasAnimations) {
	        if (info.out.requiresUiRedraw) {
	            mSyncResult |= kSync_UIRedrawRequired;
	        }
	    }
	    // If prepareTextures is false, we ran out of texture cache space
	    return info.prepareTextures;
	}

 syncFrameState 的作用就是同步 frame 信息，将 Java 层维护的 frame 信息同步到 RenderThread中。
 
> Main Thread 和Render Thread 都各自维护了一份应用程序窗口视图信息。各自维护了一份应用程序窗口视图信息的目的，就是为了可以互不干扰，进而实现最大程度的并行。其中，Render Thread维护的应用程序窗口视图信息是来自于 Main Thread 的。因此，当Main Thread 维护的应用程序窗口信息发生了变化时，就需要同步到 Render Thread 去。
 

# 丢帧

CPU处理完毕，交给GPU，GPU还没弄完，CPU的又来了，当时上一个呢？

# 等待GPU完工的时机


	#ifndef EGL_KHR_swap_buffers_with_damage
	#define EGL_KHR_swap_buffers_with_damage 1

	#define WAIT_FOR_GPU_COMPLETION 0
	bool Properties::swapBuffersWithDamage = true;

	bool EglManager::swapBuffers(EGLSurface surface, const SkRect& dirty,
	        EGLint width, EGLint height) {
	
	#if WAIT_FOR_GPU_COMPLETION 不过源码中并不等待GPU完工，可能怕影响性能，渲染线程也是要处理任务的
	    {
	        // 等待GPU完工
	        ATRACE_NAME("Finishing GPU work");
	        fence();
	    }
	#endif
	
	#ifdef EGL_KHR_swap_buffers_with_damage
	    if (CC_LIKELY(Properties::swapBuffersWithDamage)) {
	        SkIRect idirty;
	        dirty.roundOut(&idirty);
	        /*
	         * EGL_KHR_swap_buffers_with_damage spec states:
	         *
	         * The rectangles are specified relative to the bottom-left of the surface
	         * and the x and y components of each rectangle specify the bottom-left
	         * position of that rectangle.
	         *
	         * HWUI does everything with 0,0 being top-left, so need to map
	         * the rect
	         */
	        EGLint y = height - (idirty.y() + idirty.height());
	        // layout: {x, y, width, height}
	        EGLint rects[4] = { idirty.x(), y, idirty.width(), idirty.height() };
	        EGLint numrects = dirty.isEmpty() ? 0 : 1;
	        eglSwapBuffersWithDamageKHR(mEglDisplay, surface, rects, numrects);
	    } else {
	        eglSwapBuffers(mEglDisplay, surface);
	    }
	...


fence机制，其实没直接启动，因为WAIT_FOR_GPU_COMPLETION=0，

	void EglManager::fence() {
	    EGLSyncKHR fence = eglCreateSyncKHR(mEglDisplay, EGL_SYNC_FENCE_KHR, NULL);
	    eglClientWaitSyncKHR(mEglDisplay, fence,
	            EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER_KHR);
	    eglDestroySyncKHR(mEglDisplay, fence);
	}

eglSwapBuffersWithDamageKHR走到eglApi调用

	EGLBoolean eglSwapBuffersWithDamageKHR(EGLDisplay dpy, EGLSurface draw,
	        EGLint *rects, EGLint n_rects)
	{
	    ATRACE_CALL();
	    clearError();
	
	    const egl_display_ptr dp = validate_display(dpy);
	    if (!dp) return EGL_FALSE;
	
	    SurfaceRef _s(dp.get(), draw);
	    if (!_s.get())
	        return setError(EGL_BAD_SURFACE, (EGLBoolean)EGL_FALSE);
	
	    egl_surface_t const * const s = get_surface(draw);
	
	    if (CC_UNLIKELY(dp->traceGpuCompletion)) {
	        EGLSyncKHR sync = eglCreateSyncKHR(dpy, EGL_SYNC_FENCE_KHR, NULL);
	        if (sync != EGL_NO_SYNC_KHR) {
	            FrameCompletionThread::queueSync(sync);
	        }
	    }
	
	    if (CC_UNLIKELY(dp->finishOnSwap)) {
	        uint32_t pixel;
	        egl_context_t * const c = get_context( egl_tls_t::getContext() );
	        if (c) {
	            // glReadPixels() ensures that the frame is complete
	            s->cnx->hooks[c->version]->gl.glReadPixels(0,0,1,1,
	                    GL_RGBA,GL_UNSIGNED_BYTE,&pixel);
	        }
	    }
	
	    if (n_rects == 0) {
	        return s->cnx->egl.eglSwapBuffers(dp->disp.dpy, s->surface);
	    }
	
	    std::vector<android_native_rect_t> androidRects((size_t)n_rects);
	    for (int r = 0; r < n_rects; ++r) {
	        int offset = r * 4;
	        int x = rects[offset];
	        int y = rects[offset + 1];
	        int width = rects[offset + 2];
	        int height = rects[offset + 3];
	        android_native_rect_t androidRect;
	        androidRect.left = x;
	        androidRect.top = y + height;
	        androidRect.right = x + width;
	        androidRect.bottom = y;
	        androidRects.push_back(androidRect);
	    }
	    native_window_set_surface_damage(s->getNativeWindow(), androidRects.data(), androidRects.size());
	
	    if (s->cnx->egl.eglSwapBuffersWithDamageKHR) {
	        return s->cnx->egl.eglSwapBuffersWithDamageKHR(dp->disp.dpy, s->surface,
	                rects, n_rects);
	    } else {
	        return s->cnx->egl.eglSwapBuffers(dp->disp.dpy, s->surface);
	    }
	}

>Fence是一种同步机制，在Android里主要用于图形系统中GraphicBuffer的同步。那它和已有同步机制相比有什么特点呢？它主要被用来处理跨硬件的情况，尤其是CPU，GPU和HWC之间的同步，另外它还可以用于多个时间点之间的同步。GPU编程和纯CPU编程一个很大的不同是它是异步的，也就是说当我们调用GL command返回时这条命令并不一定完成了，只是把这个命令放在本地的command buffer里。具体什么时候这条GL command被真正执行完毕CPU是不知道的，除非CPU使用glFinish()等待这些命令执行完，另外一种方法就是基于同步对象的Fence机制。下面举个生产者把GraphicBuffer交给消费者的例子。如生产者是App中的renderer，消费者是SurfaceFlinger。GraphicBuffer的队列放在缓冲队列BufferQueue中。BufferQueue对App端的接口为IGraphicBufferProducer，实现类为Surface，对SurfaceFlinger端的接口为IGraphicBufferConsumer，实现类为SurfaceFlingerConsumer。BufferQueue中对每个GraphiBuffer都有BufferState标记着它的状态
 
>这个状态一定程度上说明了该GraphicBuffer的归属，但只指示了CPU里的状态，而GraphicBuffer的真正使用者是GPU。也就是说，当生产者把一个GraphicBuffer放入BufferQueue时，只是在CPU层面完成了归属的转移。但GPU说不定还在用，如果还在用的话消费者是不能拿去合成的。这时候GraphicBuffer和生产消费者的关系就比较暧昧了，消费者对GraphicBuffer具有拥有权，但无使用权，它需要等一个信号，告诉它GPU用完了，消费者才真正拥有使用权


>
![](https://img-blog.csdn.net/20140930173508266)

Most recent Android devices support the “sync framework”. This allows the system to do some nifty thing when combined with hardware components that can manipulate graphics data asynchronously. For example, a producer can submit a series of OpenGL ES drawing commands and then enqueue the output buffer before rendering completes. The buffer is accompanied by a fence that signals when the contents are ready. A second fence accompanies the buffer when it is returned to the free list, so that the consumer can release the buffer while the contents are still in use. This approach improves latency and throughput as the buffers move through the system.

上面这段话结合BufferQueue的生产者和消费者模式更容易理解，描述了fence如何提升graphic的显示性能。生产者利用opengl绘图，不用等绘图完成，直接queue buffer，在queue buffer的同时，需要传递给BufferQueue一个fence，而消费者acquire这个buffer后同时也会获取到这个fence，这个fence在GPU绘图完成后signal。这就是所谓的“acquireFence”，用于生产者通知消费者生产已完成。

当消费者对acquire到的buffer做完自己要做的事情后（例如把buffer交给surfaceflinger去合成），就要把buffer release到BufferQueue的free list，由于该buffer的内容可能正在被surfaceflinger使用，所以release时也需要传递一个fence，用来指示该buffer的内容是否依然在被使用，接下来生产者在继续dequeue buffer时，如果dequeue到了这个buffer，在使用前先要等待该fence signal。这就是所谓的“releaseFence”，后者用于消费者通知生产者消费已完成。

一般来说，fence对象(new Fence)在一个BufferQueue对应的生产者和消费者之间通过binder传递，不会在不同的BufferQueue中传递(但是对利用overlay合成的layer，其所对应的acquire fence，会被传递到HWComposer中，因为overlay直接会由hal层的hwcomposer去合成，其使用的graphic buffer是上层surface中render的buffer，如果上层surface使用opengl合成，那么在hwcomposer对overlay合成前先要保证render完成(画图完成)，即在hwcomposer中等待这个fence触发，所以fence需要首先被传递到hal层，但是这个fence的传递不是通过BufferQueue的binder传递，而是利用具体函数去实现，后续有分析)。



Android OpenGl的fence机制是为了CPU不再等待GPU，Fence机制要看每个GPU厂商自己的实现，大概流程就是CPU将GPU渲染交给GPU，同时还有个围栏，GPU渲染结束后，拆除围栏，再GPU渲染结束之前，围栏一直存在，当SF获取Layer中的数据进行合成的时候如果围栏没有被拆除，SF是不能使用GraphicBuffer的，这样能够最大程度的做到CPU GPU独立，同时CPU也能多塞几块交给GPU处理，多层缓冲。

# Android双缓冲 三缓冲说的到底什么

# GPU过度绘制原理

过度绘制的图形是在GPU渲染之前就计算好的

	void OpenGLRenderer::renderOverdraw() {
	    if (Properties::debugOverdraw && getTargetFbo() == 0) {
	        const Rect* clip = &mTilingClip;
	
	        mRenderState.scissor().setEnabled(true);
	        mRenderState.scissor().set(clip->left,
	                mState.firstSnapshot()->getViewportHeight() - clip->bottom,
	                clip->right - clip->left,
	                clip->bottom - clip->top);
	
	        // 1x overdraw
	        mRenderState.stencil().enableDebugTest(2);
	        drawColor(mCaches.getOverdrawColor(1), SkXfermode::kSrcOver_Mode);
	
	        // 2x overdraw
	        mRenderState.stencil().enableDebugTest(3);
	        drawColor(mCaches.getOverdrawColor(2), SkXfermode::kSrcOver_Mode);
	
	        // 3x overdraw
	        mRenderState.stencil().enableDebugTest(4);
	        drawColor(mCaches.getOverdrawColor(3), SkXfermode::kSrcOver_Mode);
	
	        // 4x overdraw and higher
	        mRenderState.stencil().enableDebugTest(4, true);
	        drawColor(mCaches.getOverdrawColor(4), SkXfermode::kSrcOver_Mode);
	
	        mRenderState.stencil().disable();
	    }
	}

# Caches如何获取过度绘制区域的呢？

	uint32_t Caches::getOverdrawColor(uint32_t amount) const {
	    static uint32_t sOverdrawColors[2][4] = {
	            { 0x2f0000ff, 0x2f00ff00, 0x3fff0000, 0x7fff0000 },
	            { 0x2f0000ff, 0x4fffff00, 0x5fff8ad8, 0x7fff0000 }
	    };
	    if (amount < 1) amount = 1;
	    if (amount > 4) amount = 4;
	
	    int overdrawColorIndex = static_cast<int>(Properties::overdrawColorSet);
	    return sOverdrawColors[overdrawColorIndex][amount - 1];
	}


# libagl是一个软件模拟的GPU库，这里需要注意

Android源码中OpenGL由其自带软件库libagl实现（基于软件算法），而真实的场景一般是由各个不同平台的硬件libhgl实现，libhgl需要OpenGL驱动程序，不同平台间实现由很大不同，拿egl_window_surface_v2_t::swapBuffers而言，软件实现的可以看做同步实现的，不需要考虑Fence机制，而对于硬件

	EGLBoolean egl_window_surface_v2_t::swapBuffers()
	{
	    if (!buffer) {
	        return setError(EGL_BAD_ACCESS, EGL_FALSE);
	    }
	    
	    /*
	     * Handle eglSetSwapRectangleANDROID()
	     * We copyback from the front buffer 
	     */
	    if (!dirtyRegion.isEmpty()) {
	        dirtyRegion.andSelf(Rect(buffer->width, buffer->height));
	        if (previousBuffer) {
	            // This was const Region copyBack, but that causes an
	            // internal compile error on simulator builds
	            // 之类
	            /*const*/ Region copyBack(Region::subtract(oldDirtyRegion, dirtyRegion));
	            // 存在可以服用的区域？？
	            if (!copyBack.isEmpty()) {
	                void* prevBits;
	                if (lock(previousBuffer, 
	                        GRALLOC_USAGE_SW_READ_OFTEN, &prevBits) == NO_ERROR) {
	                    // copy from previousBuffer to buffer 将可以拷贝的区域拷贝过去
	                    // 这样在处理区域的时候，是不是GPU就不用处理全部区域了？？？？
	                    copyBlt(buffer, bits, previousBuffer, prevBits, copyBack);
	                    unlock(previousBuffer);
	                }
	            }
	        }
	        oldDirtyRegion = dirtyRegion;
	    }
	 
	    if (previousBuffer) {
	        previousBuffer->common.decRef(&previousBuffer->common); 
	        previousBuffer = 0;
	    }
	    
	    // 第一次  previousBuffer=0,说明没有previousBuffer，第二次就有了
	    unlock(buffer);
	    previousBuffer = buffer;
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
	
	        // reallocate the depth-buffer if needed
	        if ((width != buffer->width) || (height != buffer->height)) {
	            // TODO: we probably should reset the swap rect here
	            // if the window size has changed
	            width = buffer->width;
	            height = buffer->height;
	            if (depth.data) {
	                free(depth.data);
	                depth.width   = width;
	                depth.height  = height;
	                depth.stride  = buffer->stride;
	                uint64_t allocSize = static_cast<uint64_t>(depth.stride) *
	                        static_cast<uint64_t>(depth.height) * 2;
	                if (depth.stride < 0 || depth.height > INT_MAX ||
	                        allocSize > UINT32_MAX) {
	                    setError(EGL_BAD_ALLOC, EGL_FALSE);
	                    return EGL_FALSE;
	                }
	                depth.data    = (GGLubyte*)malloc(allocSize);
	                if (depth.data == 0) {
	                    setError(EGL_BAD_ALLOC, EGL_FALSE);
	                    return EGL_FALSE;
	                }
	            }
	        }
	
	        // keep a reference on the buffer
	        buffer->common.incRef(&buffer->common);
	
	        // finally pin the buffer down
	        if (lock(buffer, GRALLOC_USAGE_SW_READ_OFTEN |
	                GRALLOC_USAGE_SW_WRITE_OFTEN, &bits) != NO_ERROR) {
	            ALOGE("eglSwapBuffers() failed to lock buffer %p (%ux%u)",
	                    buffer, buffer->width, buffer->height);
	            return setError(EGL_BAD_ACCESS, EGL_FALSE);
	            // FIXME: we should make sure we're not accessing the buffer anymore
	        }
	    } else {
	        return setError(EGL_BAD_CURRENT_SURFACE, EGL_FALSE);
	    }
	
	    return EGL_TRUE;
	}


注意，这里没有使用Fence，而是直接传递-1，真机的话，就不是了，模拟器用的应该是模拟的GPU，跟上面流程一致，可惜看不到GPU相关的库，没有厂商放出来，真机一般有Fence机制，保证GPU CPU并行，而且实现代码经常不一样：看一下Systrace，先看下模拟器的（Genymotion 6.0）

![image.png](https://upload-images.jianshu.io/upload_images/1460468-f6088f8ab2dc18ec.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到，Systace中的函数调用跟egl.cpp中基本一致，可以认为模拟器中，Render线程中，CPU是等待GPU执行完再结束的，所以一般是看着有些卡的，也就是swaper buffer部分很耗时。dequeue buffer是为了什么呢？为什么这么卡顿，基本耗时都在这里？

![image.png](https://upload-images.jianshu.io/upload_images/1460468-75a85c4674621374.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

再再看下真机的（nexus5 6.0）

![真机OpenGL渲染Systrace](https://upload-images.jianshu.io/upload_images/1460468-f00b845c598e6103.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到有egl.cpp中的函数再高通平台，用的是高通平台的代码，很明显能看到dequeue跟queue的顺序不同，由于真机有Fence机制，一般是先dequeue，

再看8.0的nexus6p，新改变

![nexus6p 8.0](https://upload-images.jianshu.io/upload_images/1460468-98cbede7afa36a80.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

glFinish()将缓冲区的指令立即送往硬件执行，但是要一直等到硬件执行完这些指令之后才返回。

glFlush()清空缓冲区，将指令送往硬件立即执行，但是它是将命令传送完毕之后立即返回，不会等待指令执行完毕。如果直接绘制到前缓冲，那么OpenGL的绘制将不会有任何延迟。设想有一个复杂的场景，有很多物体需要绘制。当调用glFlush时，物体会一个一个地出现在屏幕上。但是，如果使用双缓冲，这个函数将不会有什么影响，因为直到交换缓冲区的时候变化才显现出来。

一般，使用glFlush的目的是确保在调用之后，CPU没有OpenGL相关的事情需要做-命令会送到硬件执行。调用glFinish的目的是确保当返回之后，没有相关工作留下需要继续做。如果调用glFinish，通常会带来性能上的损失。因为它会是的GPU和CPU之间的并行性丧失。一般，我们提交给驱动的任务被分组，然后被送到硬件上（在缓冲区交换的时候）。如果调用glFinish，就强制驱动将命令送到GPU。然后CPU等待直到被传送的命令全部执行完毕。这样在GPU工作的整个期间内，CPU没有工作（至少在这个线程上）。而在CPU工作时（通常是在对命令分组），GPU没有工作。因此造成性能上的下降。
交换缓冲 ？？？


**如果你使用的是双缓冲，那么可能这两个函数都不需要用到。缓冲区交换操作会隐式将命令送去执行。** 时机，时机，时机，时机是什么时候？

* 缓冲区交换操作会隐式将命令送去执行，这个实现看不同的GPU 处理机制，应该是不同厂商自己实现，Android开源的源码中是软件实现的OpenGL，基本都是同步的，不牵扯GPU渲染，所以，不存在CPU、GPU同步一说




这个是在dequeue的时候吗？还是queue的时候？

会不会是updateAndReleaseLocked中调用了glFlush？？？

	status_t GLConsumer::syncForReleaseLocked(EGLDisplay dpy) {
	    GLC_LOGV("syncForReleaseLocked");
	
	    if (mCurrentTexture != BufferQueue::INVALID_BUFFER_SLOT) {
	        if (SyncFeatures::getInstance().useNativeFenceSync()) {
	            EGLSyncKHR sync = eglCreateSyncKHR(dpy,
	                    EGL_SYNC_NATIVE_FENCE_ANDROID, NULL);
	            if (sync == EGL_NO_SYNC_KHR) {
	                GLC_LOGE("syncForReleaseLocked: error creating EGL fence: %#x",
	                        eglGetError());
	                return UNKNOWN_ERROR;
	            }
	            glFlush();
	            


	status_t GLConsumer::updateAndReleaseLocked(const BufferItem& item,
	        PendingRelease* pendingRelease)
	{
	    ...
	    // Do whatever sync ops we need to do before releasing the old slot.
	    if (slot != mCurrentTexture) {
	        err = syncForReleaseLocked(mEglDisplay);
	        if (err != NO_ERROR) {
	        

也就是      glFlush();的时机交给了GLConsumer？？ 这样可能就解释通了，但是这个是同步的吗？

glFinish和glFlush都是强制将命令缓冲区的内容提交给硬件执行。

>glFinish does not return until the effects of all previously called GL commands are complete. Such effects include all changes to GL state, all changes to connection state, and all changes to the frame buffer contents.

>Different GL implementations buffer commands in several different locations, including network buffers and the graphics accelerator itself. glFlush empties all of these buffers, causing all issued commands to be executed as quickly as they are accepted by the actual rendering engine. Though this execution may not be completed in any particular time period, it does complete in finite time.

>Because any GL program might be executed over a network, or on an accelerator that buffers commands, all programs should call glFlush whenever they count on having all of their previously issued commands completed. For example, call glFlush before waiting for user input that depends on the generated image.

>Notes
>glFlush can return at any time. It does not wait until the execution of all previously issued GL commands is complete.
>


关键GPU是同步的，不需要Fence

这里只能是认为在swapbuffer的时候，手机厂家对GPU发出最后的命令，通知去渲染，但是这个不是说阻塞等待，而是类似glFlush，通知后就返回，除非GPU很忙，来不及恢复通知，那这个时候就要等待，结束后，提交Buffer，这个时候buffer不一定能用，但是SF aquireBuffer后，在用的时候，会检查Fence机制，只有GPU处理完，才会进行合成工作。这个要看具体平台了，一般GPU应该来的及响应下，CPU不至于提交太快，
	
	：     



        
# 参考文档

[](http://www.voidcn.com/article/p-njbssmva-bqc.html)
[原Android 5.1 SurfaceFlinger VSYNC详解](https://blog.csdn.net/newchenxf/article/details/49131167)                 
[Android中的GraphicBuffer同步机制-Fence](https://blog.csdn.net/jinzhuojun/article/details/39698317)                       
[android graphic(15)—fence](https://blog.csdn.net/lewif/article/details/50984212)          
[原android graphic(16)—fence(简化](https://blog.csdn.net/lewif/article/details/51007148)          
[【OpenGL】glFinish()和glFlush()函数详解-[转]](http://www.cnblogs.com/vranger/p/3621121.html)            
[关于glFlush和glFinish以及SwapBuffer的用法小结](http://www.cppblog.com/topjackhjj/articles/87911.html)