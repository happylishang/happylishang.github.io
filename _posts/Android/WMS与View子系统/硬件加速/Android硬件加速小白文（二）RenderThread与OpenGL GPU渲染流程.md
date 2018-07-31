Android4.0之后，系统默认开启硬件加速来渲染View，之前，[理解Android硬件加速的小白文](http://www.jianshu.com/p/40f660e17a73)已经简单的讲述了硬件加速的简单模型，对于APP而言，硬件加速绘制可以看做三个阶段，
**OpenGL API的调用必须结合OpenGL Context（OpenGL上下文），它包含OpenGL状态变量及渲染相关的信息。 OpenGL 是个状态机，绘制的时候用户可以通过命令去设置一些状态，例如是否 开启深度测试是否开启混合等，改变状态会影响渲染流水线的操作。OpenGL 采 Client-Server 模型来进行编程，Client 提出渲染请求，Server 相应请求。**


* 第一阶段：APP依赖CPU构建OpenGL渲染需要的命令及数据
* 第二阶段：CPU将数据上传（共享或者拷贝）给GPU，通知并等待GPU渲染完成
* 第三阶段：GPU渲染完成，CPU通知SurfaceFlinger进行和成显示

第一个阶段，其实主要做的就是构建DrawOp树（封装OpenGL渲染命令），并预处理分组一些相似命令，以便提高GPU处理效率，这个阶段主要是CPU在工作，不过这个阶段前期运行在UI线程，后期部分运行在RenderThread（渲染线程），第二个阶段主要是CPU运行在渲染线程，CPU将数据同步（共享）给GPU，并通知GPU进行渲染，第三个阶段，其实是渲染完毕，APP通知SurfaceFlinger进行合成显示。为了方便理解，我是主观上将GPU看做一个linux系统中的设备，同这个设备进行交互都是通过该设备相应的驱动来完成，操作GPU，就如同操作一个普通设备（像蓝牙，摄像头等），因此，简单画下流程示意图：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-7b48185ca6849b13.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)。

由于之前已经简单分析过DrawOp树的构建，优化，本文主要是分析GPU如何完成OpenGL渲染，这个过程主要在Render线程，通过OpenGL API通知GPU处理渲染任务。

# Android OpenGL硬件加速类图

![image.png](https://upload-images.jianshu.io/upload_images/1460468-6c1252ee03d0ef62.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

每个显示的window对应一个ViewrootImpl对象，因此也会对应一个AttachInfo->ThreadRender对象->ThreadProxy(RootRenderNode)->CanvasContext.cpp(DrawFrameTask、EglManager（**单例复用**）、EglSurface)->->RenderThread(**单例复用**)，对于APP而言，一般只会维持一个OpenGL 渲染线程，当然，你也可以自己new一个独立的渲染线程。主动调用OpenGL API，我们先仔细看下OpenGL上下文的建立，本文基于Android6.0，ViewRootImpl在setView添加窗口的时候，会通过enableHardwareAcceleration开启硬件加速，创建OpenGL渲染环境，为下一步的显示做好准备，

	private void enableHardwareAcceleration(WindowManager.LayoutParams attrs) {
	        mAttachInfo.mHardwareAccelerated = false;
	        mAttachInfo.mHardwareAccelerationRequested = false;
				...
	        final boolean hardwareAccelerated =
	                (attrs.flags & WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED) != 0;
	
	        if (hardwareAccelerated) {
	            if (!HardwareRenderer.isAvailable()) {
	                return;
	            }
	 					...
	 					 if (!HardwareRenderer.sRendererDisabled
	                    || (HardwareRenderer.sSystemRendererDisabled && forceHwAccelerated)) {
	                if (mAttachInfo.mHardwareRenderer != null) {
	                    mAttachInfo.mHardwareRenderer.destroy();
	                }
	
	                final Rect insets = attrs.surfaceInsets;
	                final boolean hasSurfaceInsets = insets.left != 0 || insets.right != 0
	                        || insets.top != 0 || insets.bottom != 0;
	                final boolean translucent = attrs.format != PixelFormat.OPAQUE || hasSurfaceInsets;
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

主要就是通过HardwareRenderer.create(mContext, translucent)创建硬件加速环境,之后再需要draw绘制的时候，通过

        mAttachInfo.mHardwareRenderer.draw(mView, mAttachInfo, this);

进一步渲染。回过头，接着看APP如何初始化硬件加速环境：**直观上说，就是构建OpenGLContext、EglSurface、RenderThread(如果没启动的话)**。

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
		<!--初始化-->
        ProcessInitializer.sInstance.init(context, mNativeProxy);
        ...
    }
  
 之前分析过，通过递归mRootNode，可以找到View Tree所有的OpenGL绘制命令及数据，ThreadProxy则主要用来像RenderThread线程提交一些OpenGL相关任务，比如初始化，绘制、更新等，
	 
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

接着看RenderProxy的在创建之初会做什么,其实主要两件事，如果RenderThread未启动，则启动它，并且为当前窗口创建CanvasContext，

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

构造函数中mRenderThread会被赋值为OpenGL Render线程，它是一个单例，默认情况下，同一个进程只有一个RenderThread::getInstance()：

![renderThread](https://upload-images.jianshu.io/upload_images/1460468-265afedca9d749a1.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

简单看下这个线程的创建与启动：

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

RenderThread会维护一个MessageQuene，并通过loop的方式读取消息，执行，RenderThread在启动之前，为OpenGL创建EglManager、RenderState、VSync信号接收器等OpenGL渲染必须的工具组件，之后启动该线程进入loop：
	
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
	        if (nextWakeup == LLONG_MAX) {
	            timeoutMillis = -1;
	        } else {
	            nsecs_t timeoutNanos = nextWakeup - systemTime(SYSTEM_TIME_MONOTONIC);
	            timeoutMillis = nanoseconds_to_milliseconds(timeoutNanos);
	            if (timeoutMillis < 0) {
	                timeoutMillis = 0; }}
		        if (mPendingRegistrationFrameCallbacks.size() && !mFrameCallbackTaskPending) {
	            drainDisplayEventQueue();
	            mFrameCallbacks.insert(   mPendingRegistrationFrameCallbacks.begin(), mPendingRegistrationFrameCallbacks.end());
	            mPendingRegistrationFrameCallbacks.clear();
	            requestVsync();  }
	            
		        if (!mFrameCallbackTaskPending && !mVsyncRequested && mFrameCallbacks.size()) {
	            requestVsync();
	        } }
	
	    return false;}

初始化
	
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

OpenGL的渲染线程需要接受Vsync，**信号到来后，回调函数是RenderThread::displayEventReceiverCallback，最后调用doFrame绘制图形？？？？？？？**

	void RenderThread::initializeDisplayEventReceiver() {
	    LOG_ALWAYS_FATAL_IF(mDisplayEventReceiver, "Initializing a second DisplayEventReceiver?");
	    mDisplayEventReceiver = new DisplayEventReceiver();
	    status_t status = mDisplayEventReceiver->initCheck();
	    mLooper->addFd(mDisplayEventReceiver->getFd(), 0,
	            Looper::EVENT_INPUT, RenderThread::displayEventReceiverCallback, this);
	}

其次RenderThread需要new一个EglManager及RenderState，用于OpenGL渲染。

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
	
EglManager主要作用是管理OpenGL上下文，创建EglSurface等
	
	class EglManager {
	public:
	    // Returns true on success, false on failure
	    void initialize();
	
	    bool hasEglContext();
	
	    EGLSurface createSurface(EGLNativeWindowType window);
	    void destroySurface(EGLSurface surface);
	
	    void destroy();
	
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
	
	    const bool mAllowPreserveBuffer;
	    bool mCanSetPreserveBuffer;
	
	    EGLSurface mCurrentSurface;
	
	    sp<GraphicBuffer> mAtlasBuffer;
	    int64_t* mAtlasMap;
	    size_t mAtlasMapSize;
	};

RenderState可以看做是OpenGL状态机，真正负责OpenGL的渲染，
	
	RenderState::RenderState(renderthread::RenderThread& thread)
	        : mRenderThread(thread)
	        , mViewportWidth(0)
	        , mViewportHeight(0)
	        , mFramebuffer(0) {
	    mThreadId = pthread_self();
	}

postAndWait其实阻塞等待postAndWait的任务执行完毕，RenderThread的第一个任务，是创建CanvasContext，

	CREATE_BRIDGE4(createContext, RenderThread* thread, bool translucent,
	        RenderNode* rootRenderNode, IContextFactory* contextFactory) {
	    return new CanvasContext(*args->thread, args->translucent,
	            args->rootRenderNode, args->contextFactory);
	}


CanvasContext握有RenderThread、EglManager、RootRenderNode等，其实可以看做Android中OpenGL上下文，是上层渲染的入口

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

CanvasContext创建后，会跟随RenderProxy的initial进行初始化，不过需要注意的是initialize其实是在Render线程，
	
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

这里传入的ANativeWindow* window其实就是native的Surface，CanvasContext在初始化的时候，为当前窗口创建一个OpenGLRenderer用来执行OpenGL drawOp,同时还会通弄过setSurface为OpenGL创建EglSurface画布，


	void CanvasContext::setSurface(ANativeWindow* window) {
	    ATRACE_CALL();
	
	    mNativeWindow = window;
	
	    if (mEglSurface != EGL_NO_SURFACE) {
	        mEglManager.destroySurface(mEglSurface);
	        mEglSurface = EGL_NO_SURFACE;
	    }
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
	    } else {
	        mRenderThread.removeFrameCallback(this);
	    }
	}
	
 createSurface其实是比较标准的OpenGL函数，eglCreateWindowSurface
	
	EGLSurface EglManager::createSurface(EGLNativeWindowType window) {
	    initialize();
	    EGLSurface surface = eglCreateWindowSurface(mEglDisplay, mEglConfig, window, nullptr);
	 	    return surface;
	}
	
通过调用eglapi.cpp最终调用egl.cpp，native_window_api_connect
	
	EGLSurface eglCreateWindowSurface(  EGLDisplay dpy, EGLConfig config,
	                                    NativeWindowType window,
	                                    const EGLint *attrib_list)
	{
	    clearError();
	
	    egl_connection_t* cnx = NULL;
	    egl_display_ptr dp = validate_display_connection(dpy, cnx);
	    if (dp) {
	        EGLDisplay iDpy = dp->disp.dpy;
	
	        if (!window) {
	            return setError(EGL_BAD_NATIVE_WINDOW, EGL_NO_SURFACE);
	        }
	
	        int value = 0;
	        window->query(window, NATIVE_WINDOW_IS_VALID, &value);
	        if (!value) {
	            return setError(EGL_BAD_NATIVE_WINDOW, EGL_NO_SURFACE);
	        }
	
	        int result = native_window_api_connect(window, NATIVE_WINDOW_API_EGL);
	        if (result < 0) {
	            ALOGE("eglCreateWindowSurface: native_window_api_connect (win=%p) "
	                    "failed (%#x) (already connected to another API?)",
	                    window, result);
	            return setError(EGL_BAD_ALLOC, EGL_NO_SURFACE);
	        }
	
	        EGLint format;
	        getNativePixelFormat(iDpy, cnx, config, format);
	
	        // now select correct colorspace and dataspace based on user's attribute list
	        EGLint colorSpace;
	        android_dataspace dataSpace;
	        if (!getColorSpaceAttribute(dp, window, attrib_list, colorSpace, dataSpace)) {
	            ALOGE("error invalid colorspace: %d", colorSpace);
	            return setError(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
	        }
	
	        std::vector<EGLint> strippedAttribList;
	        if (stripColorSpaceAttribute(dp, attrib_list, format, strippedAttribList)) {
	            // Had to modify the attribute list due to use of color space.
	            // Use modified list from here on.
	            attrib_list = strippedAttribList.data();
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
	
	        // Eglsurface里面是有Surface的引用的，同时swap的时候，是能通知consumer的
	        EGLSurface surface = cnx->egl.eglCreateWindowSurface(
	                iDpy, config, window, attrib_list);
	        if (surface != EGL_NO_SURFACE) {
	            egl_surface_t* s =
	                    new egl_surface_t(dp.get(), config, window, surface, colorSpace, cnx);
	            return s;
	        }
	
	        // EGLSurface creation failed
	        native_window_set_buffers_format(window, 0);
	        native_window_api_disconnect(window, NATIVE_WINDOW_API_EGL);
	    }
	    return EGL_NO_SURFACE;
	}

最终调用的是egl.cpp的createWindowSurface，

	static EGLSurface createWindowSurface(EGLDisplay dpy, EGLConfig config,
	        NativeWindowType window, const EGLint* /*attrib_list*/)
	{
	    if (egl_display_t::is_valid(dpy) == EGL_FALSE)
	        return setError(EGL_BAD_DISPLAY, EGL_NO_SURFACE);
	    if (window == 0)
	        return setError(EGL_BAD_MATCH, EGL_NO_SURFACE);
	
	    EGLint surfaceType;
	    if (getConfigAttrib(dpy, config, EGL_SURFACE_TYPE, &surfaceType) == EGL_FALSE)
	        return EGL_FALSE;
	
	    if (!(surfaceType & EGL_WINDOW_BIT))
	        return setError(EGL_BAD_MATCH, EGL_NO_SURFACE);
	
	    if (static_cast<ANativeWindow*>(window)->common.magic !=
	            ANDROID_NATIVE_WINDOW_MAGIC) {
	        return setError(EGL_BAD_NATIVE_WINDOW, EGL_NO_SURFACE);
	    }
	        
	    EGLint configID;
	    if (getConfigAttrib(dpy, config, EGL_CONFIG_ID, &configID) == EGL_FALSE)
	        return EGL_FALSE;
	
	    int32_t depthFormat;
	    int32_t pixelFormat;
	    if (getConfigFormatInfo(configID, pixelFormat, depthFormat) != NO_ERROR) {
	        return setError(EGL_BAD_MATCH, EGL_NO_SURFACE);
	    }
	
	    // FIXME: we don't have access to the pixelFormat here just yet.
	    // (it's possible that the surface is not fully initialized)
	    // maybe this should be done after the page-flip
	    //if (EGLint(info.format) != pixelFormat)
	    //    return setError(EGL_BAD_MATCH, EGL_NO_SURFACE);
	
	    egl_surface_t* surface;
	    surface = new egl_window_surface_v2_t(dpy, config, depthFormat,
	            static_cast<ANativeWindow*>(window));
	
	    if (!surface->initCheck()) {
	        // there was a problem in the ctor, the error
	        // flag has been set.
	        delete surface;
	        surface = 0;
	    }
	    return surface;
	}
	
new 了一个egl_window_surface_v2_t，封装ANativeWindow，由于EGLSurface是一个Void* 类型指针，因此egl_window_surface_v2_t型指针可以直接赋值给它，到这里初始化环境结束，OpenGL需要的渲染环境已经搭建完毕，等到View需要显示或者更新的时候，就会调用VieWrootImpl的draw去更新。
	    
# OpenGL渲染三板斧

看一下渲染流程

        mAttachInfo.mHardwareRenderer.draw(mView, mAttachInfo, this);

    @Override
    void draw(View view, AttachInfo attachInfo, HardwareDrawCallbacks callbacks) {
       <!--构建DrawOp Tree-->        
       updateRootDisplayList(view, callbacks);
       <!--渲染-->
        int syncResult = nSyncAndDrawFrame(mNativeProxy, frameInfo, frameInfo.length);
        ...
    }


构建流程及优化流程[理解Android硬件加速的小白文](http://www.jianshu.com/p/40f660e17a73)已经简述过，不再分析，只看nSyncAndDrawFrame部分流程，

	static int android_view_ThreadedRenderer_syncAndDrawFrame(JNIEnv* env, jobject clazz,
	        jlong proxyPtr, jlongArray frameInfo, jint frameInfoSize) {
	    LOG_ALWAYS_FATAL_IF(frameInfoSize != UI_THREAD_FRAME_INFO_SIZE,
	            "Mismatched size expectations, given %d expected %d",
	            frameInfoSize, UI_THREAD_FRAME_INFO_SIZE);
	    RenderProxy* proxy = reinterpret_cast<RenderProxy*>(proxyPtr);
	    env->GetLongArrayRegion(frameInfo, 0, frameInfoSize, proxy->frameInfo());
	    return proxy->syncAndDrawFrame();
	}

其实就是调用RenderProxy的syncAndDrawFrame，主线程会将task插入到RenderThread，并且阻塞等待，直到RenderThread跟UI线程同步结束，才返回，之后RenderThread会开始调用GPU渲染


	int DrawFrameTask::drawFrame() {
	    LOG_ALWAYS_FATAL_IF(!mContext, "Cannot drawFrame with no CanvasContext!");
	
	    mSyncResult = kSync_OK;
	    mSyncQueued = systemTime(CLOCK_MONOTONIC);
	    postAndWait();
	
	    return mSyncResult;
	}
	
	<!---->
	void DrawFrameTask::postAndWait() {
	    AutoMutex _lock(mLock);
	    mRenderThread->queue(this);
	    mSignal.wait(mLock);
	}
	
	void DrawFrameTask::run() {
	    ATRACE_NAME("DrawFrame");
	
	    bool canUnblockUiThread;
	    bool canDrawThisFrame;
	    {
	        TreeInfo info(TreeInfo::MODE_FULL, mRenderThread->renderState());
	        canUnblockUiThread = syncFrameState(info);
	        canDrawThisFrame = info.out.canDrawThisFrame;
	    }
	
	    // Grab a copy of everything we need
	    CanvasContext* context = mContext;
	
	    // From this point on anything in "this" is *UNSAFE TO ACCESS*
	    if (canUnblockUiThread) {
	        unblockUiThread();
	    }
	
	    if (CC_LIKELY(canDrawThisFrame)) {
	        context->draw();
	    }
	
	    if (!canUnblockUiThread) {
	        unblockUiThread();
	    }
	}

这里的context的draw其实调用的是CanvasContext的draw，注意这里全部是在Render线程


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


# 参考文档

[](http://www.voidcn.com/article/p-njbssmva-bqc.html)
[原Android 5.1 SurfaceFlinger VSYNC详解](https://blog.csdn.net/newchenxf/article/details/49131167)                 
[Android中的GraphicBuffer同步机制-Fence](https://blog.csdn.net/jinzhuojun/article/details/39698317)                       
[android graphic(15)—fence](https://blog.csdn.net/lewif/article/details/50984212)          
[原android graphic(16)—fence(简化](https://blog.csdn.net/lewif/article/details/51007148)