---
layout: post
title: Android窗口管理分析（5）：硬件加速与软件加速的区别
category: Android
image: http://upload-images.jianshu.io/upload_images/1460468-103d49829291e1f7.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---

硬件加速，直观上说就是**依赖GPU实现图形绘制加速**，因此，软硬件加速的区别主要是指**图形的绘制究竟是GPU来处理还是CPU**，如果是GPU，就认为是硬件加速绘制，反之，软件绘制。在Android中也是如此，不过相对于普通的软件绘制，硬件加速还做了其他方面优化，不仅仅限定在绘制方面，绘制之前，在如何构建绘制区域上，硬件加速也做出了很大优化，因此硬件加速特性可以从下面两部分来分析：

* 1、前期策略：如何构建需要绘制的区域
* 2、后期绘制：**单独渲染线程，依赖GPU进行绘制**

无论是软件绘制还是硬件加速，绘制内存的分配都是类似的，都是需要请求SurfaceFlinger服务分配一块内存，只不过硬件加速有可能从FrameBuffer硬件缓冲区直接分配内存（具体有什么好处，还不太清楚，可能合成更快吧），两者的绘制都是在APP端，绘制完成之后同样需要通知SurfaceFlinger进行合成，在这个流程上没有任何区别，**真正的区别在于如何在APP端完成绘制**，本文就直观的了解下两者的区别，会涉及部分源码，但不求甚解。


# 软硬件加速的分歧点

关于View的绘制是软件加速实现的还是硬件加速实现的，一般在开发的时候并不可见，大概从Android 4.+开始，默认情况下都是支持跟开启了硬件加速的，也存在手机支持硬件加速，但是部分API不支持硬件加速的情况，如果使用了这些API，就需要主关闭硬件加速，或者在View层，或者在Activity层，比如Canvas的clipPath等。那图形绘制的时候，软硬件的分歧点究竟在哪呢？举个例子，有个View需要重绘，一般会调用View的invalidate，触发重绘，跟着这条线走，去查一下分歧点。

![视图重绘](http://upload-images.jianshu.io/upload_images/1460468-2cb862a7cd77c699.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

从上面的调用流程可以看出，视图重绘最后会进入ViewRootImpl的draw，这里有个判断点是软硬件加速的分歧点,简化后如下

>ViewRootImpl.java


    private void draw(boolean fullRedrawNeeded) {
        ...
        if (!dirty.isEmpty() || mIsAnimating || accessibilityFocusDirty) {
            <!--关键点1 是否开启硬件加速-->
            if (mAttachInfo.mHardwareRenderer != null && mAttachInfo.mHardwareRenderer.isEnabled()) {
                 ...
                dirty.setEmpty();
                mBlockResizeBuffer = false;
                <!--关键点2 硬件加速绘制-->
                mAttachInfo.mHardwareRenderer.draw(mView, mAttachInfo, this);
            } else {
              ...
               <!--关键点3 软件绘制-->
                if (!drawSoftware(surface, mAttachInfo, xOffset, yOffset, scalingRequired, dirty)) {
                    return;
                }
            ...
        
关键点1是启用硬件加速的条件，必须支持硬件并且开启了硬件加速才可以，满足，就利用HardwareRenderer.draw，否则drawSoftware（软件绘制）。简答看一下这个条件，默认情况下，该条件是成立的，因为4.+之后的手机一般都支持硬件加速，而且在添加窗口的时候，ViewRootImpl会enableHardwareAcceleration开启硬件加速，new HardwareRenderer，并初始化硬件加速环境。

    private void enableHardwareAcceleration(WindowManager.LayoutParams attrs) {
    
        <!--根据配置，获取硬件加速的开关-->
        // Try to enable hardware acceleration if requested
        final boolean hardwareAccelerated =
                (attrs.flags & WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED) != 0;
       if (hardwareAccelerated) {
            ...
                <!--新建硬件加速图形渲染器-->
                mAttachInfo.mHardwareRenderer = HardwareRenderer.create(mContext, translucent);
                if (mAttachInfo.mHardwareRenderer != null) {
                    mAttachInfo.mHardwareRenderer.setName(attrs.getTitle().toString());
                    mAttachInfo.mHardwareAccelerated =
                            mAttachInfo.mHardwareAccelerationRequested = true;
                }
            ...

其实到这里软件绘制跟硬件加速的分歧点已经找到了，就是ViewRootImpl在draw的时候，如果需要硬件加速就利用 HardwareRenderer进行draw，否则走软件绘制流程，drawSoftware其实很简单，利用Surface.lockCanvas，向SurfaceFlinger申请一块匿名共享内存[内存分配](http://www.jianshu.com/p/2fb8cc9e63cb)，同时获取一个普通的SkiaCanvas，用于调用Skia库，进行图形绘制，

	private boolean drawSoftware(Surface surface, AttachInfo attachInfo, int xoff, int yoff,
	            boolean scalingRequired, Rect dirty) {
	        final Canvas canvas;
	        try {
	            <!--关键点1 -->
	            canvas = mSurface.lockCanvas(dirty);
	            ..
	            <!--关键点2 绘制-->
	            	 mView.draw(canvas);
	             ..
	             关键点3 通知SurfaceFlinger进行图层合成
	                surface.unlockCanvasAndPost(canvas);
	            }   ...	        
	           return true;  }
    
上限drawSoftware工作完全由CPU来完成，不会牵扯到GPU的操作，下面重点看下HardwareRenderer所进行的硬件加速绘制。

# HardwareRenderer硬件加速绘制模型

开头说过，硬件加速绘制包括两个阶段：构建阶段+绘制阶段，所谓构建就是递归遍历所有视图，将需要的操作缓存下来，之后再交给单独的Render线程利用OpenGL渲染。在Android硬件加速框架中，View视图被抽象成RenderNode节点，View中的绘制都会被抽象成一个个DrawOp（DisplayListOp），比如View中drawLine，构建中就会被抽象成一个DrawLintOp，drawBitmap操作会被抽象成DrawBitmapOp，每个子View的绘制被抽象成DrawRenderNodeOp，每个DrawOp有对应的OpenGL绘制命令，同时内部也握着绘图所需要的数据。如下所示：

![绘图Op抽象](http://upload-images.jianshu.io/upload_images/1460468-d546b6e86e2a1f30.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

如此以来，每个View不仅仅握有自己DrawOp List，同时还拿着子View的绘制入口，如此递归，便能够统计到所有的绘制Op，很多分析都称为Display List，源码中也是这么来命名类的，不过这里其实更像是一个树，而不仅仅是List，示意如下：

![硬件加速.jpg](http://upload-images.jianshu.io/upload_images/1460468-1f3c83ffb4e74889.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

构建完成后，就可以将这个绘图Op树交给Render线程进行绘制，这里是同软件绘制很不同的地方，软件绘制时，View一般都在主线程中完成绘制，而硬件加速，除非特殊要求，一般都是在单独线程中完成绘制，如此以来就分担了主线程很多压力，提高了UI线程的响应速度。

![硬件加速模型.jpg](http://upload-images.jianshu.io/upload_images/1460468-22e4b5bba04b472b.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

知道整个模型后，就代码来简单了解下实现流程，先看下递归构建RenderNode树及DrawOp集。

# HardwareRenderer硬件加速:构建DrawOp集

HardwareRenderer是整个硬件加速绘制的入口，实现是一个ThreadedRenderer对象，从名字能看出，ThreadedRenderer应该跟一个Render线程息息相关，不过ThreadedRenderer是在UI线程中创建的，那么与UI线程也必定相关，其主要作用：

* 1、在UI线程中完成DrawOp集构建
* 2、负责跟渲染线程通信

可见ThreadedRenderer的作用是很重要的，简单看一下实现：

    ThreadedRenderer(Context context, boolean translucent) {
        ...
		<!--新建native node-->
        long rootNodePtr = nCreateRootRenderNode();
        mRootNode = RenderNode.adopt(rootNodePtr);
        mRootNode.setClipToBounds(false);
        <!--新建NativeProxy-->
        mNativeProxy = nCreateProxy(translucent, rootNodePtr);
        ProcessInitializer.sInstance.init(context, mNativeProxy);
        loadSystemProperties();
    }

从上面代码看出，ThreadedRenderer中有一个RootNode用来标识整个DrawOp树的根节点，有个这个根节点就可以访问所有的绘制Op，同时还有个RenderProxy对象，这个对象就是用来跟渲染线程进行通信的句柄，看一下其构造函数：

	RenderProxy::RenderProxy(bool translucent, RenderNode* rootRenderNode, IContextFactory* contextFactory)
	        : mRenderThread(RenderThread::getInstance())
	        , mContext(nullptr) {
	    SETUP_TASK(createContext);
	    args->translucent = translucent;
	    args->rootRenderNode = rootRenderNode;
	    args->thread = &mRenderThread;
	    args->contextFactory = contextFactory;
	    mContext = (CanvasContext*) postAndWait(task);
	    mDrawFrameTask.setContext(&mRenderThread, mContext);  
	   }
	       
从RenderThread::getInstance()可以看出，RenderThread是一个单例线程，也就是说，每个进程最多只有一个硬件渲染线程，这样就不会存在多线程并发访问冲突问题，到这里其实环境硬件渲染环境已经搭建好好了。下面就接着看ThreadedRenderer的draw函数，如何构建渲染Op树：

    @Override
    void draw(View view, AttachInfo attachInfo, HardwareDrawCallbacks callbacks) {
        attachInfo.mIgnoreDirtyState = true;

        final Choreographer choreographer = attachInfo.mViewRootImpl.mChoreographer;
        choreographer.mFrameInfo.markDrawStart();
        <!--关键点1：构建View的DrawOp树-->
        updateRootDisplayList(view, callbacks);

        <!--关键点2：通知RenderThread线程绘制-->
        int syncResult = nSyncAndDrawFrame(mNativeProxy, frameInfo, frameInfo.length);
        ...
    }

目前只关心关键点1 updateRootDisplayList，构建RootDisplayList，其实就是View的DrawOp树，

      
      

UI线程只能通过CanvasContext跟渲染线程通信。



## 构建DrawOp集优点 （减少重绘？那视图的迁移如何处理？）

我们实际上只是将对应的绘制命令以及参数保存在一个Display List中。接下来再通过Display List Renderer执行这个Display List的命令，这个过程称为Display List Replay。引进Display List的概念有什么好处呢？主要是两个好处。第一个好处是在下一帧绘制中，如果一个View的内容不需要更新，那么就不用重建它的Display List，也就是不需要调用它的onDraw（）成员函数。第二个好处是在下一帧中，如果一个View仅仅是一些简单的属性发生变化，例如位置和Alpha值发生变化，那么也无需要重建它的Display List，只需要在上一次建立的Display List中修改一下对应的属性就可以了，这也意味着不需要调用它的onDraw成员函数。这两个好处使用在绘制应用程序窗口的一帧时，省去很多应用程序代码的执行，也就是大大地节省了CPU的执行时间。

注意，只有使用硬件加速渲染的View，才会关联有Render Node，也就才会使用到Display List。我们知道，目前并不是所有的2D UI绘制命令都是GPU可以支持的。这一点具体可以参考官方说明文档：http://developer.android.com/guide/topics/graphics/hardware-accel.html。对于使用了GPU不支持的2D UI绘制命令的View，只能通过软件方式来渲染。具体的做法是将创建一个新的Canvas，这个Canvas的底层是一个Bitmap，也就是说，绘制都发生在这个Bitmap上。绘制完成之后，这个Bitmap再被记录在其Parent View的Display List中。而当Parent View的Display List的命令被执行时，记录在里面的Bitmap再通过Open GL命令来绘制。

另一方面，对于前面提到的在Android 4.0引进的TextureView，它也不是通过Display List来绘制。由于它的底层实现直接就是一个Open GL纹理，因此就可以跳过Display List这一中间层，从而提高效率。这个Open GL纹理的绘制通过一个Layer Renderer来封装。Layer Renderer和Display List Renderer可以看作是同一级别的概念，它们都是通过Open GL命令来绘制UI元素的。只不过前者操作的是Open GL纹理，而后者操作的是Display List。


# HardwareRenderer硬件加速:参照DrawOp集绘制UI到Graphic Buffer


我们知道，Android应用程序窗口的View是通过树形结构来组织的。这些View不管是通过硬件加速渲染还是软件渲染，或者是一个特殊的TextureView，在它们的成员函数onDraw被调用期间，它们都是将自己的UI绘制在Parent View的Display List中。其中，最顶层的Parent View是一个Root View，它关联的Root Node称为Root Render Node。也就是说，最终Root Render Node的Display List将会包含有一个窗口的所有绘制命令。在绘制窗口的下一帧时，Root Render Node的Display List都会通过一个Open GL Renderer真正地通过Open GL命令绘制在一个Graphic Buffer中。最后这个Graphic Buffer被交给SurfaceFlinger服务进行合成和显示
       
       
       
 
从名字是就能看出，ThreadedRenderer应该跟一个Render线程息息相关。

    ThreadedRenderer(Context context, boolean translucent) {
        ...
		<!--新建native node-->
        long rootNodePtr = nCreateRootRenderNode();
        mRootNode = RenderNode.adopt(rootNodePtr);
        mRootNode.setClipToBounds(false);
        <!--新建NativeProxy-->
        mNativeProxy = nCreateProxy(translucent, rootNodePtr);
        ProcessInitializer.sInstance.init(context, mNativeProxy);
        loadSystemProperties();
    }
 
 RenderProxy的构造函数会新建RootNode及NativeProxy对象，并将其初始化：
 
	 static jlong android_view_ThreadedRenderer_createRootRenderNode(JNIEnv* env, jobject clazz) {
	    RootRenderNode* node = new RootRenderNode(env);
	    node->incStrong(0);
	    node->setName("RootRenderNode");
	    return reinterpret_cast<jlong>(node);
	}

在native对应RootRenderNode，到底是做什么用的呢？现在还看不出来，后面分析，创建RootNode后会接着创建一个RenderProxy对象，而rootRenderNode是它的一个成员变量。

	 static jlong android_view_ThreadedRenderer_createProxy(JNIEnv* env, jobject clazz,
	        jboolean translucent, jlong rootRenderNodePtr) {
	    RootRenderNode* rootRenderNode = reinterpret_cast<RootRenderNode*>(rootRenderNodePtr);
	    ContextFactoryImpl factory(rootRenderNode);
	    return (jlong) new RenderProxy(translucent, rootRenderNode, &factory);
	}
	
前文说过ThreadedRenderer是一个跟线程有关的对象，那么新线程在哪呢？看一下

	RenderProxy::RenderProxy(bool translucent, RenderNode* rootRenderNode, IContextFactory* contextFactory)
	        : mRenderThread(RenderThread::getInstance())
	        , mContext(nullptr) {
	    SETUP_TASK(createContext);
	    args->translucent = translucent;
	    args->rootRenderNode = rootRenderNode;
	    args->thread = &mRenderThread;
	    args->contextFactory = contextFactory;
	    mContext = (CanvasContext*) postAndWait(task);
	    mDrawFrameTask.setContext(&mRenderThread, mContext);  
	   }

RenderProxy 的mRenderThread其实就是线程对象，并且从实现上来看，它是一个单利，也就是一个进程里面，只有一个RenderThread线程， mDrawFrameTask.setContext(&mRenderThread, mContext);  是很重重要的一句，让DrawFrameTask绑定了线程跟CanvasContext绘制上下文，只不过CanvasContext是在mRenderThread线程中创建的，因为CanvasContext不允许跨线程访问：

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

RenderThread确实采用Handler消息处理模型，只不过这里采用的全是native的实现，在创建后即可调用run将线程调度起来，

	bool RenderThread::threadLoop() {
	    setpriority(PRIO_PROCESS, 0, PRIORITY_DISPLAY);
	    initThreadLocals();
	
	    int timeoutMillis = -1;
	    for (;;) {
	        int result = mLooper->pollOnce(timeoutMillis);
	        nsecs_t nextWakeup;
	        // Process our queue, if we have anything
	        while (RenderTask* task = nextTask(&nextWakeup)) {
	            task->run();
	        }
	        if (nextWakeup == LLONG_MAX) {
	            timeoutMillis = -1;
	        } else {
	            nsecs_t timeoutNanos = nextWakeup - systemTime(SYSTEM_TIME_MONOTONIC);
	            timeoutMillis = nanoseconds_to_milliseconds(timeoutNanos);
	            if (timeoutMillis < 0) {
	                timeoutMillis = 0;
	            }
	        }
	
	        if (mPendingRegistrationFrameCallbacks.size() && !mFrameCallbackTaskPending) {
	            drainDisplayEventQueue();
	            mFrameCallbacks.insert(
	                    mPendingRegistrationFrameCallbacks.begin(), mPendingRegistrationFrameCallbacks.end());
	            mPendingRegistrationFrameCallbacks.clear();
	            requestVsync();
	        }
	
	        if (!mFrameCallbackTaskPending && !mVsyncRequested && mFrameCallbacks.size()) {
	            // TODO: Clean this up. This is working around an issue where a combination
	            // of bad timing and slow drawing can result in dropping a stale vsync
	            // on the floor (correct!) but fails to schedule to listen for the
	            // next vsync (oops), so none of the callbacks are run.
	            requestVsync();
	        }
	    }
	
	    return false;
	}

	void RenderThread::initThreadLocals() {  
	    initializeDisplayEventReceiver();  
	    mEglManager = new EglManager(*this);  
	    mRenderState = new RenderState();  
	}

  
下面接着看硬件加速的draw函数，其实就ThreadedRenderer的draw，这里会有很多新概念出现，比如DisplayList，RenderNoder等等，

    @Override
    void draw(View view, AttachInfo attachInfo, HardwareDrawCallbacks callbacks) {
        attachInfo.mIgnoreDirtyState = true;
       final Choreographer choreographer = attachInfo.mViewRootImpl.mChoreographer;
       choreographer.mFrameInfo.markDrawStart();
       <!--关键函数1 更新DisplayList-->
       updateRootDisplayList(view, callbacks);
        attachInfo.mIgnoreDirtyState = false;
        ....
        final long[] frameInfo = choreographer.mFrameInfo.mFrameInfo;
        <!--关键函数 绘制图形-->
        int syncResult = nSyncAndDrawFrame(mNativeProxy, frameInfo, frameInfo.length);
		  ...
        if ((syncResult & SYNC_INVALIDATE_REQUIRED) != 0) {
            attachInfo.mViewRootImpl.invalidate();
        }
    }


先构建，在渲染，

	 	
	 	 private void updateRootDisplayList(View view, HardwareDrawCallbacks callbacks) {

        updateViewTreeDisplayList(view);

        if (mRootNodeNeedsUpdate || !mRootNode.isValid()) {
            DisplayListCanvas canvas = mRootNode.start(mSurfaceWidth, mSurfaceHeight);
            try {
                final int saveCount = canvas.save();
                canvas.translate(mInsetLeft, mInsetTop);
                callbacks.onHardwarePreDraw(canvas);

                canvas.insertReorderBarrier();
                canvas.drawRenderNode(view.updateDisplayListIfDirty());
                canvas.insertInorderBarrier();

                callbacks.onHardwarePostDraw(canvas);
                canvas.restoreToCount(saveCount);
                mRootNodeNeedsUpdate = false;
            } finally {
                mRootNode.end(canvas);
            }
        }
        Trace.traceEnd(Trace.TRACE_TAG_VIEW);
    }


	 	/**
	     * Gets the RenderNode for the view, and updates its DisplayList (if needed and supported)
	     * @hide
	     */
	    @NonNull
	    public RenderNode updateDisplayListIfDirty() {
	        final RenderNode renderNode = mRenderNode;
	        if (!canHaveDisplayList()) {
	            // can't populate RenderNode, don't try
	            return renderNode;
	        }
	
	        if ((mPrivateFlags & PFLAG_DRAWING_CACHE_VALID) == 0
	                || !renderNode.isValid()
	                || (mRecreateDisplayList)) {
	            // Don't need to recreate the display list, just need to tell our
	            // children to restore/recreate theirs
	            if (renderNode.isValid()
	                    && !mRecreateDisplayList) {
	                mPrivateFlags |= PFLAG_DRAWN | PFLAG_DRAWING_CACHE_VALID;
	                mPrivateFlags &= ~PFLAG_DIRTY_MASK;
	                dispatchGetDisplayList();
	
	                return renderNode; // no work needed
	            }
	
	            // If we got here, we're recreating it. Mark it as such to ensure that
	            // we copy in child display lists into ours in drawChild()
	            mRecreateDisplayList = true;
	
	            int width = mRight - mLeft;
	            int height = mBottom - mTop;
	            int layerType = getLayerType();
	
	            final DisplayListCanvas canvas = renderNode.start(width, height);
	            canvas.setHighContrastText(mAttachInfo.mHighContrastText);
	
	            try {
	                final HardwareLayer layer = getHardwareLayer();
	                if (layer != null && layer.isValid()) {
	                    canvas.drawHardwareLayer(layer, 0, 0, mLayerPaint);
	                } else if (layerType == LAYER_TYPE_SOFTWARE) {
	                    buildDrawingCache(true);
	                    Bitmap cache = getDrawingCache(true);
	                    if (cache != null) {
	                        canvas.drawBitmap(cache, 0, 0, mLayerPaint);
	                    }
	                } else {
	                    computeScroll();
	
	                    canvas.translate(-mScrollX, -mScrollY);
	                    mPrivateFlags |= PFLAG_DRAWN | PFLAG_DRAWING_CACHE_VALID;
	                    mPrivateFlags &= ~PFLAG_DIRTY_MASK;
	
	                    // Fast path for layouts with no backgrounds
	                    if ((mPrivateFlags & PFLAG_SKIP_DRAW) == PFLAG_SKIP_DRAW) {
	                        dispatchDraw(canvas);
	                        if (mOverlay != null && !mOverlay.isEmpty()) {
	                            mOverlay.getOverlayView().draw(canvas);
	                        }
	                    } else {
	                        draw(canvas);
	                    }
	                }
	            } finally {
	                renderNode.end(canvas);
	                setDisplayListProperties(renderNode);
	            }
	        } else {
	            mPrivateFlags |= PFLAG_DRAWN | PFLAG_DRAWING_CACHE_VALID;
	            mPrivateFlags &= ~PFLAG_DIRTY_MASK;
	        }
	        return renderNode;
	    }
 
mRenderNode在View创建的时候，在Native层新建 ,DisplayListCanvas帮助绘制，
    
    
    @CallSuper
    public void draw(Canvas canvas) {
        final int privateFlags = mPrivateFlags;
        final boolean dirtyOpaque = (privateFlags & PFLAG_DIRTY_MASK) == PFLAG_DIRTY_OPAQUE &&
                (mAttachInfo == null || !mAttachInfo.mIgnoreDirtyState);
        mPrivateFlags = (privateFlags & ~PFLAG_DIRTY_MASK) | PFLAG_DRAWN;

        /*
         * Draw traversal performs several drawing steps which must be executed
         * in the appropriate order:
         *
         *      1. Draw the background
         *      2. If necessary, save the canvas' layers to prepare for fading
         *      3. Draw view's content
         *      4. Draw children
         *      5. If necessary, draw the fading edges and restore layers
         *      6. Draw decorations (scrollbars for instance)
         */

        // Step 1, draw the background, if needed
        int saveCount;

        if (!dirtyOpaque) {
        
        <!--看看canvas如何构建list 到底list在哪里-->
            drawBackground(canvas);
        }

        // skip step 2 & 5 if possible (common case)
        final int viewFlags = mViewFlags;
        boolean horizontalEdges = (viewFlags & FADING_EDGE_HORIZONTAL) != 0;
        boolean verticalEdges = (viewFlags & FADING_EDGE_VERTICAL) != 0;
        if (!verticalEdges && !horizontalEdges) {
            // Step 3, draw the content
            if (!dirtyOpaque) onDraw(canvas);

            // Step 4, draw the children
            dispatchDraw(canvas);

            // Overlay is part of the content and draws beneath Foreground
            if (mOverlay != null && !mOverlay.isEmpty()) {
                mOverlay.getOverlayView().dispatchDraw(canvas);
            }

            // Step 6, draw decorations (foreground, scrollbars)
            onDrawForeground(canvas);

            // we're done...
            return;
        }

        /*
         * Here we do the full fledged routine...
         * (this is an uncommon case where speed matters less,
         * this is why we repeat some of the tests that have been
         * done above)
         */

        boolean drawTop = false;
        boolean drawBottom = false;
        boolean drawLeft = false;
        boolean drawRight = false;

        float topFadeStrength = 0.0f;
        float bottomFadeStrength = 0.0f;
        float leftFadeStrength = 0.0f;
        float rightFadeStrength = 0.0f;

        // Step 2, save the canvas' layers
        int paddingLeft = mPaddingLeft;

        final boolean offsetRequired = isPaddingOffsetRequired();
        if (offsetRequired) {
            paddingLeft += getLeftPaddingOffset();
        }

        int left = mScrollX + paddingLeft;
        int right = left + mRight - mLeft - mPaddingRight - paddingLeft;
        int top = mScrollY + getFadeTop(offsetRequired);
        int bottom = top + getFadeHeight(offsetRequired);

        if (offsetRequired) {
            right += getRightPaddingOffset();
            bottom += getBottomPaddingOffset();
        }

        final ScrollabilityCache scrollabilityCache = mScrollCache;
        final float fadeHeight = scrollabilityCache.fadingEdgeLength;
        int length = (int) fadeHeight;

        // clip the fade length if top and bottom fades overlap
        // overlapping fades produce odd-looking artifacts
        if (verticalEdges && (top + length > bottom - length)) {
            length = (bottom - top) / 2;
        }

        // also clip horizontal fades if necessary
        if (horizontalEdges && (left + length > right - length)) {
            length = (right - left) / 2;
        }

        if (verticalEdges) {
            topFadeStrength = Math.max(0.0f, Math.min(1.0f, getTopFadingEdgeStrength()));
            drawTop = topFadeStrength * fadeHeight > 1.0f;
            bottomFadeStrength = Math.max(0.0f, Math.min(1.0f, getBottomFadingEdgeStrength()));
            drawBottom = bottomFadeStrength * fadeHeight > 1.0f;
        }

        if (horizontalEdges) {
            leftFadeStrength = Math.max(0.0f, Math.min(1.0f, getLeftFadingEdgeStrength()));
            drawLeft = leftFadeStrength * fadeHeight > 1.0f;
            rightFadeStrength = Math.max(0.0f, Math.min(1.0f, getRightFadingEdgeStrength()));
            drawRight = rightFadeStrength * fadeHeight > 1.0f;
        }

        saveCount = canvas.getSaveCount();

        int solidColor = getSolidColor();
        if (solidColor == 0) {
            final int flags = Canvas.HAS_ALPHA_LAYER_SAVE_FLAG;

            if (drawTop) {
                canvas.saveLayer(left, top, right, top + length, null, flags);
            }

            if (drawBottom) {
                canvas.saveLayer(left, bottom - length, right, bottom, null, flags);
            }

            if (drawLeft) {
                canvas.saveLayer(left, top, left + length, bottom, null, flags);
            }

            if (drawRight) {
                canvas.saveLayer(right - length, top, right, bottom, null, flags);
            }
        } else {
            scrollabilityCache.setFadeColor(solidColor);
        }

        // Step 3, draw the content
        if (!dirtyOpaque) onDraw(canvas);

        // Step 4, draw the children
        dispatchDraw(canvas);

        // Step 5, draw the fade effect and restore layers
        final Paint p = scrollabilityCache.paint;
        final Matrix matrix = scrollabilityCache.matrix;
        final Shader fade = scrollabilityCache.shader;

        if (drawTop) {
            matrix.setScale(1, fadeHeight * topFadeStrength);
            matrix.postTranslate(left, top);
            fade.setLocalMatrix(matrix);
            p.setShader(fade);
            canvas.drawRect(left, top, right, top + length, p);
        }

        if (drawBottom) {
            matrix.setScale(1, fadeHeight * bottomFadeStrength);
            matrix.postRotate(180);
            matrix.postTranslate(left, bottom);
            fade.setLocalMatrix(matrix);
            p.setShader(fade);
            canvas.drawRect(left, bottom - length, right, bottom, p);
        }

        if (drawLeft) {
            matrix.setScale(1, fadeHeight * leftFadeStrength);
            matrix.postRotate(-90);
            matrix.postTranslate(left, top);
            fade.setLocalMatrix(matrix);
            p.setShader(fade);
            canvas.drawRect(left, top, left + length, bottom, p);
        }

        if (drawRight) {
            matrix.setScale(1, fadeHeight * rightFadeStrength);
            matrix.postRotate(90);
            matrix.postTranslate(right, top);
            fade.setLocalMatrix(matrix);
            p.setShader(fade);
            canvas.drawRect(right - length, top, right, bottom, p);
        }

        canvas.restoreToCount(saveCount);

        // Overlay is part of the content and draws beneath Foreground
        if (mOverlay != null && !mOverlay.isEmpty()) {
            mOverlay.getOverlayView().dispatchDraw(canvas);
        }

        // Step 6, draw decorations (foreground, scrollbars)
        onDrawForeground(canvas);
    }

    /**
     * Draws the background onto the specified canvas.
     *
     * @param canvas Canvas on which to draw the background
     */
    private void drawBackground(Canvas canvas) {
        final Drawable background = mBackground;
        if (background == null) {
            return;
        }

        setBackgroundBounds();

        // Attempt to use a display list if requested.
        if (canvas.isHardwareAccelerated() && mAttachInfo != null
                && mAttachInfo.mHardwareRenderer != null) {
            mBackgroundRenderNode = getDrawableRenderNode(background, mBackgroundRenderNode);

            final RenderNode renderNode = mBackgroundRenderNode;
            if (renderNode != null && renderNode.isValid()) {
                setBackgroundRenderNodeProperties(renderNode);
                ((DisplayListCanvas) canvas).drawRenderNode(renderNode);
                return;
            }
        }

        final int scrollX = mScrollX;
        final int scrollY = mScrollY;
        if ((scrollX | scrollY) == 0) {
            background.draw(canvas);
        } else {
            canvas.translate(scrollX, scrollY);
            background.draw(canvas);
            canvas.translate(-scrollX, -scrollY);
        }
    }
    
    
可以看到，硬件加速最后调用的是ThreadedRenderer的nSyncAndDrawFrame函数，

	static int android_view_ThreadedRenderer_syncAndDrawFrame(JNIEnv* env, jobject clazz,
	        jlong proxyPtr, jlongArray frameInfo, jint frameInfoSize) {
	    RenderProxy* proxy = reinterpret_cast<RenderProxy*>(proxyPtr);
	    env->GetLongArrayRegion(frameInfo, 0, frameInfoSize, proxy->frameInfo());
	    return proxy->syncAndDrawFrame();
	}

它直接调用RenderProxy的syncAndDrawFrame函数，从名字看出，采用了代理模式，那么它的服务端在哪呢？


	int RenderProxy::syncAndDrawFrame() {
	    return mDrawFrameTask.drawFrame();
	}

	int DrawFrameTask::drawFrame() {
	    LOG_ALWAYS_FATAL_IF(!mContext, "Cannot drawFrame with no CanvasContext!");
	
	    mSyncResult = kSync_OK;
	    mSyncQueued = systemTime(CLOCK_MONOTONIC);
	    postAndWait();
	
	    return mSyncResult;
	}

postAndWait()有点像Hanlder机制，不过它同步原地等待，

	void DrawFrameTask::postAndWait() {
	    AutoMutex _lock(mLock);
	    mRenderThread->queue(this);
	    mSignal.wait(mLock);
	}

调用mRenderThread的函数，插入消息队列，并等待消息执行完毕，其实就是执行DrawFrameTask的run函数

	 void DrawFrameTask::run() {
	    ATRACE_NAME("DrawFrame");
	
	    bool canUnblockUiThread;
	    bool canDrawThisFrame;
	    {
	        TreeInfo info(TreeInfo::MODE_FULL, mRenderThread->renderState());
	        canUnblockUiThread = syncFrameState(info);
	        canDrawThisFrame = info.out.canDrawThisFrame;
	    }
	    CanvasContext* context = mContext;
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

 可以看到调用的是context->draw()，context是什么呢？上文说过，它是一个CanvasContext，在RenderThread线程中创建的，
 
	 CREATE_BRIDGE4(createContext, RenderThread* thread, bool translucent,
	        RenderNode* rootRenderNode, IContextFactory* contextFactory) {
	    return new CanvasContext(*args->thread, args->translucent,
	            args->rootRenderNode, args->contextFactory);
	}

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

 最后就是调用OpenGL进行绘制，
 
	 void CanvasContext::draw() {
	
	    SkRect dirty;
	    mDamageAccumulator.finish(&dirty);
	    mCurrentFrameInfo->markIssueDrawCommandsStart();
	
	    EGLint width, height;
	    mEglManager.beginFrame(mEglSurface, &width, &height);
	    if (width != mCanvas->getViewportWidth() || height != mCanvas->getViewportHeight()) {
	        mCanvas->setViewport(width, height);
	        dirty.setEmpty();
	    } else if (!mBufferPreserved || mHaveNewSurface) {
	        dirty.setEmpty();
	    } else {
	        if (!dirty.isEmpty() && !dirty.intersect(0, 0, width, height)) {
	            dirty.setEmpty();
	        }
	        profiler().unionDirty(&dirty);
	    }
		    if (!dirty.isEmpty()) {
	        mCanvas->prepareDirty(dirty.fLeft, dirty.fTop,
	                dirty.fRight, dirty.fBottom, mOpaque);
	    } else {
	        mCanvas->prepare(mOpaque);
	    }
	
	    Rect outBounds;
	    mCanvas->drawRenderNode(mRootRenderNode.get(), outBounds);
	    profiler().draw(mCanvas);
	    bool drew = mCanvas->finish();
	    mCurrentFrameInfo->markSwapBuffers();
	    if (drew) {
	        swapBuffers(dirty, width, height);
	    }
	
	}

mCanvas->drawRenderNode的实现,如何跟surface绑定，

	 private void performTraversals() {  
	 
	   hwInitialized = mAttachInfo.mHardwareRenderer.initialize(  
                                        mSurface);  
	   static jboolean android_view_ThreadedRenderer_initialize(JNIEnv* env, jobject clazz,  
	        jlong proxyPtr, jobject jsurface) {  
	    RenderProxy* proxy = reinterpret_cast<RenderProxy*>(proxyPtr);  
	    sp<ANativeWindow> window = android_view_Surface_getNativeWindow(env, jsurface);  
	    return proxy->initialize(window);  
	} 


参数proxyPtr描述的就是之前所创建的一个RenderProxy对象，而参数jsurface描述的是要绑定给Render Thread的Java层的Surface。前面提到，Java层的Surface在Native层对应的是一个ANativeWindow。我们可以通过函数android_view_Surface_getNativeWindow来获得一个Java层的Surface在Native层对应的ANativeWindow。

 绑定当前的窗口？？主要就是标识当前窗口，难道统一时刻只会操作一个窗口吗？

	void CanvasContext::setSurface(ANativeWindow* window) {  
	    mNativeWindow = window;  
	  
	    if (mEglSurface != EGL_NO_SURFACE) {  
	        mEglManager.destroySurface(mEglSurface);  
	        mEglSurface = EGL_NO_SURFACE;  
	    }  
	  
	    if (window) {  
	        mEglSurface = mEglManager.createSurface(window);  
	    }  
	  
	    if (mEglSurface != EGL_NO_SURFACE) {  
	        ......  
	        mHaveNewSurface = true;  
	        makeCurrent();  
	    }   
	  
	    ......  
	} 

每一个Open GL渲染上下文都需要关联有一个EGL Surface。这个EGL Surface描述的是一个绘图表面，它封装的实际上是一个ANativeWindow。有了这个EGL Surface之后，我们在执行Open GL命令的时候，才能确定这些命令是作用在哪个窗口上。
CanvasContext类的成员变量mEglManager实际上是指向前面我们分析RenderThread类的成员函数initThreadLocals时创建的一个EglManager对象。通过调用这个EglManager对象的成员函数createSurface就可以将参数window描述的ANativeWindow封装成一个EGL Surface。EGL Surface创建成功之后，就可以调用CanvasContext类的成员函数makeCurrent将它绑定到Render Thread的Open GL渲染上下文来，如下所示：

	void CanvasContext::makeCurrent() {  
	    // TODO: Figure out why this workaround is needed, see b/13913604  
	    // In the meantime this matches the behavior of GLRenderer, so it is not a regression  
	    mHaveNewSurface |= mEglManager.makeCurrent(mEglSurface);  
	}  

从这里就可以看到，将一个EGL Surface绑定到RenderThread的Open GL渲染上下文中是通过CanvasContext类的成员变量mEglManager指向的一个EglManager对象的成员函数makeCurrent来完成的。实际上就是通过EGL函数建立了从Open GL到底层OS图形系统的桥梁。这一点应该怎么理解呢？Open GL是一套与OS无关的规范，不过当它在一个具体的OS实现时，仍然是需要与OS的图形系统打交道的。例如，Open GL需要从底层的OS图形系统中获得图形缓冲区来保存渲染结果，并且也需要将渲染好的图形缓冲区交给底层的OS图形系统来显示到设备屏幕去。Open GL与底层的OS图形系统的这些交互通道都是通过EGL函数来建立的。


	EGLSurface EglManager::createSurface(EGLNativeWindowType window) {
	    initialize();
	    EGLSurface surface = eglCreateWindowSurface(mEglDisplay, mEglConfig, window, nullptr);
	    LOG_ALWAYS_FATAL_IF(surface == EGL_NO_SURFACE,
	            "Failed to create EGLSurface for window %p, eglErr = %s",
	            (void*) window, egl_error_str());
	    return surface;
	}

	   public EGLSurface eglCreateWindowSurface(EGLDisplay display, EGLConfig config, Object native_window, int[] attrib_list) {
	        Surface sur = null;
	        if (native_window instanceof SurfaceView) {
	            SurfaceView surfaceView = (SurfaceView)native_window;
	            sur = surfaceView.getHolder().getSurface();
	        } else if (native_window instanceof SurfaceHolder) {
	            SurfaceHolder holder = (SurfaceHolder)native_window;
	            sur = holder.getSurface();
	        } else if (native_window instanceof Surface) {
	            sur = (Surface) native_window;
	        }
	
	        long eglSurfaceId;
	        if (sur != null) {
	            eglSurfaceId = _eglCreateWindowSurface(display, config, sur, attrib_list);
	        } else if (native_window instanceof SurfaceTexture) {
	            eglSurfaceId = _eglCreateWindowSurfaceTexture(display, config,
	                    native_window, attrib_list);
	        } else {
	            throw new java.lang.UnsupportedOperationException(
	                "eglCreateWindowSurface() can only be called with an instance of " +
	                "Surface, SurfaceView, SurfaceHolder or SurfaceTexture at the moment.");
	        }
	
	        if (eglSurfaceId == 0) {
	            return EGL10.EGL_NO_SURFACE;
	        }
	        return new EGLSurfaceImpl( eglSurfaceId );
	    }

surfaceId


	bool EglManager::makeCurrent(EGLSurface surface, EGLint* errOut) {
	    if (isCurrent(surface)) return false;
	
	    if (surface == EGL_NO_SURFACE) {
	        // Ensure we always have a valid surface & context
	        surface = mPBufferSurface;
	    }
	    if (!eglMakeCurrent(mEglDisplay, surface, surface, mEglContext)) {
	    }
	    mCurrentSurface = surface;
	    return true;
	}



接着看绘制 ，更新RootDisplayList

	public class ThreadedRenderer extends HardwareRenderer {  
	    ......  
	  
    private void updateRootDisplayList(View view, HardwareDrawCallbacks callbacks) {
        Trace.traceBegin(Trace.TRACE_TAG_VIEW, "Record View#draw()");
        updateViewTreeDisplayList(view);

        if (mRootNodeNeedsUpdate || !mRootNode.isValid()) {
            DisplayListCanvas canvas = mRootNode.start(mSurfaceWidth, mSurfaceHeight);
            try {
                final int saveCount = canvas.save();
                canvas.translate(mInsetLeft, mInsetTop);
                callbacks.onHardwarePreDraw(canvas);

                canvas.insertReorderBarrier();
                canvas.drawRenderNode(view.updateDisplayListIfDirty());
                canvas.insertInorderBarrier();

                callbacks.onHardwarePostDraw(canvas);
                canvas.restoreToCount(saveCount);
                mRootNodeNeedsUpdate = false;
            } finally {
                mRootNode.end(canvas);
            }
        }
        Trace.traceEnd(Trace.TRACE_TAG_VIEW);
    }
	}


CanvasContext类的成员函数draw的实现如下所示：
 
	void CanvasContext::draw() {  
	    ......  
	  
	    SkRect dirty;  
	    mDamageAccumulator.finish(&dirty);  
	    ......  
	  
	    status_t status;  
	    if (!dirty.isEmpty()) {  
	        status = mCanvas->prepareDirty(dirty.fLeft, dirty.fTop,  
	                dirty.fRight, dirty.fBottom, mOpaque);  
	    } else {  
	        status = mCanvas->prepare(mOpaque);  
	    }  
	  
	    Rect outBounds;  
	    status |= mCanvas->drawRenderNode(mRootRenderNode.get(), outBounds);  
	 	    ......  
	  
	    mCanvas->finish();  
	 	    ......  
	  
	    if (status & DrawGlInfo::kStatusDrew) {  
	        swapBuffers();  
	    }  
	  
	    ......  
	}  
	
   这个函数定义在文件frameworks/base/libs/hwui/OpenGLRenderer.cpp中,CanvasContext类的成员函数draw的执行过程如下所示：
   
   1. 获得应用程序窗口要更新的脏区域之后，调用成员变量mCanvas指向的一个OpenGLRenderer对象的成员函数prepareDirty或者prepare执行一些初始化工作，取决于脏区域是不是空的。
   2, 调用成员变量mCanvas指向的一个OpenGLRenderer对象的成员函数drawRenderNode渲染成员变量mRootRenderNode描述的应用程序窗口的Root Render Node的Display List。
   3. 调用成员变量mCanvas指向的一个OpenGLRenderer对象的成员函数finish执行一些清理工作。在这一步中，如果开启了OverDraw，那么还会在应用程序窗口的上面绘制一些描述OverDraw的颜色块。
   4. 调用另外一个成员函数swapBuffers将前面已经绘制好的图形缓冲区提交给Surface Flinger合成和显示。
   在上述四个步骤中，最重要的是第1步和第2步，因此接下来我们就分别对它们进行分析。
   我们假设第1步得到的应用程序窗口要更新的脏区域不为空，因此这一步执行的就是OpenGLRenderer类的成员函数prepareDirty，它的实现如下所示：

 
# 有些API不支持硬件加速，需要同硬件加速混合来用


















# 软件加速


Surface 在lock canvas获取图形绘制缓存的时候，如果开启了硬件加速

	static int gralloc_alloc_framebuffer_locked(alloc_device_t* dev,
	        size_t size, int usage, buffer_handle_t* pHandle)
	{
	    private_module_t* m = reinterpret_cast<private_module_t*>(
	            dev->common.module);
	
	    // allocate the framebuffer
	    if (m->framebuffer == NULL) {
	        // initialize the framebuffer, the framebuffer is mapped once
	        // and forever.
	        int err = mapFrameBufferLocked(m);
	        if (err < 0) {
	            return err;
	        }
	    }
	
	    const uint32_t bufferMask = m->bufferMask;
	    const uint32_t numBuffers = m->numBuffers;
	    const size_t bufferSize = m->finfo.line_length * m->info.yres;
	    if (numBuffers == 1) {
	        // If we have only one buffer, we never use page-flipping. Instead,
	        // we return a regular buffer which will be memcpy'ed to the main
	        // screen when post is called.
	        int newUsage = (usage & ~GRALLOC_USAGE_HW_FB) | GRALLOC_USAGE_HW_2D;
	        return gralloc_alloc_buffer(dev, bufferSize, newUsage, pHandle);
	    }
	
	   <!--这里说明，一个fb对应的图形缓冲区最大是32个，-->
	    if (bufferMask >= ((1LU<<numBuffers)-1)) {
	        // We ran out of buffers.
	        return -ENOMEM;
	    }
	
	    // create a "fake" handles for it
	    intptr_t vaddr = intptr_t(m->framebuffer->base);
	    private_handle_t* hnd = new private_handle_t(dup(m->framebuffer->fd), size,
	            private_handle_t::PRIV_FLAGS_FRAMEBUFFER);
	
	    // find a free slot
	    for (uint32_t i=0 ; i<numBuffers ; i++) {
	        if ((bufferMask & (1LU<<i)) == 0) {
	            m->bufferMask |= (1LU<<i);
	            break;
	        }
	        vaddr += bufferSize;
	    }
	    
	    hnd->base = vaddr;
	    hnd->offset = vaddr - intptr_t(m->framebuffer->base);
	    *pHandle = hnd;
	
	    return 0;
	}


硬件的 map ，应该是直接用fd0设备


在Android系统中，所有的图形缓冲区都是由SurfaceFlinger服务分配的，而当一个图形缓冲区被分配的时候，它会同时被映射到请求分配的进程的地址空间去，即分配的过程同时也包含了注册的过程。但是对用户空间的其它的应用程序来说，它们所需要的图形缓冲区是在由SurfaceFlinger服务分配的，因此，当它们得到SurfaceFlinger服务分配的图形缓冲区之后，还需要将这块图形缓冲区映射到自己的地址空间来，以便可以使用这块图形缓冲区。这个映射的过程即为我们接下来要分析的图形缓冲区注册过程。


这句话对吗 ？？？？

**由于在系统帧缓冲区中分配的图形缓冲区只在SurfaceFlinger服务中使用，而SurfaceFlinger服务在初始化系统帧缓冲区的时候，已经将系统帧缓冲区映射到自己所在的进程中来了，因此，函数gralloc_map如果发现要注册的图形缓冲区是在系统帧缓冲区分配的时候，那么就不需要再执行映射图形缓冲区的操作了。**
       
       
假设此时系统帧缓冲区中尚有空闲的图形缓冲区的，接下来函数就会创建一个private_handle_t结构体hnd来描述这个即将要分配出去的图形缓冲区。注意，这个图形缓冲区的标志值等于PRIV_FLAGS_FRAMEBUFFER，即表示这是一块在系统帧缓冲区中分配的图形缓冲区。接下来的for循环从低位到高位检查变量bufferMask的值，并且找到第一个值等于0的位，这样就可以知道在系统帧缓冲区中，第几个图形缓冲区的是空闲的。注意，变量vadrr的值开始的时候指向系统帧缓冲区的基地址，在下面的for循环中，每循环一次它的值都会增加bufferSize。从这里就可以看出，每次从系统帧缓冲区中分配出去的图形缓冲区的大小都是刚好等于显示屏一屏内容大小的。
       
       
  
当private_handle_t结构体hnd所描述的图形缓冲区是在系统帧缓冲区中分配的时候，即这个图形缓冲区的标志值flags的PRIV_FLAGS_FRAMEBUFFER位等于1的时候，我们是不需要将图形缓冲区的内容拷贝到系统帧缓冲区去的，因为我们将内容写入到图形缓冲区的时候，已经相当于是将内容写入到了系统帧缓冲区中去了。虽然在这种情况下，我们不需要将图形缓冲区的内容拷贝到系统帧缓冲区去，但是我们需要告诉系统帧缓冲区设备将要渲染的图形缓冲区作为系统当前的输出图形缓冲区，这样才可以将要渲染的图形缓冲区的内容绘制到设备显示屏来。例如，假设系统帧缓冲区有2个图形缓冲区，当前是以第1个图形缓冲区作为输出图形缓冲区的，这时候如果我们需要渲染第2个图形缓冲区，那么就必须告诉系统帧绘冲区设备，将第2个图形缓冲区作为输出图形缓冲区。     
	
	
	int mapFrameBufferLocked(struct private_module_t* module)
	{
	    // already initialized...
	    if (module->framebuffer) {
	        return 0;
	    }
	        
	    char const * const device_template[] = {
	            "/dev/graphics/fb%u",
	            "/dev/fb%u",
	            0 };
	
	    int fd = -1;
	    int i=0;
	    char name[64];
	
	    while ((fd==-1) && device_template[i]) {
	        snprintf(name, 64, device_template[i], 0);
	        fd = open(name, O_RDWR, 0);
	        i++;
	    }
	    if (fd < 0)
	        return -errno;
	
	    struct fb_fix_screeninfo finfo;
	    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
	        return -errno;
	
	    struct fb_var_screeninfo info;
	    if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1)
	        return -errno;
	
	    info.reserved[0] = 0;
	    info.reserved[1] = 0;
	    info.reserved[2] = 0;
	    info.xoffset = 0;
	    info.yoffset = 0;
	    info.activate = FB_ACTIVATE_NOW;
	
	    /*
	     * Request NUM_BUFFERS screens (at lest 2 for page flipping)
	     */
	    info.yres_virtual = info.yres * NUM_BUFFERS;
	
	
	    uint32_t flags = PAGE_FLIP;
	    if (ioctl(fd, FBIOPUT_VSCREENINFO, &info) == -1) {
	        info.yres_virtual = info.yres;
	        flags &= ~PAGE_FLIP;
	        ALOGW("FBIOPUT_VSCREENINFO failed, page flipping not supported");
	    }
	
	    if (info.yres_virtual < info.yres * 2) {
	        // we need at least 2 for page-flipping
	        info.yres_virtual = info.yres;
	        flags &= ~PAGE_FLIP;
	        ALOGW("page flipping not supported (yres_virtual=%d, requested=%d)",
	                info.yres_virtual, info.yres*2);
	    }
	
	    if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1)
	        return -errno;
	
	    uint64_t  refreshQuotient =
	    (
	            uint64_t( info.upper_margin + info.lower_margin + info.yres )
	            * ( info.left_margin  + info.right_margin + info.xres )
	            * info.pixclock
	    );
	
	    /* Beware, info.pixclock might be 0 under emulation, so avoid a
	     * division-by-0 here (SIGFPE on ARM) */
	    int refreshRate = refreshQuotient > 0 ? (int)(1000000000000000LLU / refreshQuotient) : 0;
	
	    if (refreshRate == 0) {
	        // bleagh, bad info from the driver
	        refreshRate = 60*1000;  // 60 Hz
	    }
	
	    if (int(info.width) <= 0 || int(info.height) <= 0) {
	        // the driver doesn't return that information
	        // default to 160 dpi
	        info.width  = ((info.xres * 25.4f)/160.0f + 0.5f);
	        info.height = ((info.yres * 25.4f)/160.0f + 0.5f);
	    }
	
	    float xdpi = (info.xres * 25.4f) / info.width;
	    float ydpi = (info.yres * 25.4f) / info.height;
	    float fps  = refreshRate / 1000.0f;
	
	    ALOGI(   "using (fd=%d)\n"
	            "id           = %s\n"
	            "xres         = %d px\n"
	            "yres         = %d px\n"
	            "xres_virtual = %d px\n"
	            "yres_virtual = %d px\n"
	            "bpp          = %d\n"
	            "r            = %2u:%u\n"
	            "g            = %2u:%u\n"
	            "b            = %2u:%u\n",
	            fd,
	            finfo.id,
	            info.xres,
	            info.yres,
	            info.xres_virtual,
	            info.yres_virtual,
	            info.bits_per_pixel,
	            info.red.offset, info.red.length,
	            info.green.offset, info.green.length,
	            info.blue.offset, info.blue.length
	    );
	
	    ALOGI(   "width        = %d mm (%f dpi)\n"
	            "height       = %d mm (%f dpi)\n"
	            "refresh rate = %.2f Hz\n",
	            info.width,  xdpi,
	            info.height, ydpi,
	            fps
	    );
	
	
	    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
	        return -errno;
	
	    if (finfo.smem_len <= 0)
	        return -errno;
	
	
	    module->flags = flags;
	    module->info = info;
	    module->finfo = finfo;
	    module->xdpi = xdpi;
	    module->ydpi = ydpi;
	    module->fps = fps;
	
	    /*
	     * map the framebuffer
	     */
	
	    int err;
	    size_t fbSize = roundUpToPageSize(finfo.line_length * info.yres_virtual);
	    module->framebuffer = new private_handle_t(dup(fd), fbSize, 0);
	
	    module->numBuffers = info.yres_virtual / info.yres;
	    module->bufferMask = 0;
	
	    void* vaddr = mmap(0, fbSize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	    if (vaddr == MAP_FAILED) {
	        ALOGE("Error mapping the framebuffer (%s)", strerror(errno));
	        return -errno;
	    }
	    module->framebuffer->base = intptr_t(vaddr);
	    memset(vaddr, 0, fbSize);
	    return 0;
	}
它没有使用共性内存的哪一种，而是直接使用了fb设备的fd文件描述符



service bootanim /system/bin/bootanimation  
    user graphics  
    group graphics  
    disabled  
    oneshot  

应用程序bootanimation的用户和用户组名称分别被设置为graphics。注意， 用来启动应用程序bootanimation的服务是disable的，即init进程在启动的时候，不会主动将应用程序bootanimation启动起来。当SurfaceFlinger服务启动的时候，它会通过修改系统属性ctl.start的值来通知init进程启动应用程序bootanimation，以便可以显示第三个开机画面，而当System进程将系统中的关键服务都启动起来之后，ActivityManagerService服务就会通知SurfaceFlinger服务来修改系统属性ctl.stop的值，以便可以通知init进程停止执行应用程序bootanimation，即停止显示第三个开机画面。接下来我们就分别分析第三个开机画面的显示过程和停止过程。



# 图层合成一直使用的硬件加速来处理 

硬件加速是一种技术，OpenGl技术，不仅仅是分配一块内存就行了。


# 绘图从内存到底来自何处，是匿名共享内存分配的还是帧缓冲区？？？

目前手机基本没有独立显存，都是在内存中分配，是需要硬件支持 （有可能混合渲染）

# LCD屏幕显示原理 

[闲聊Framebuffer](http://happyseeker.github.io/kernel/2016/05/24/about-framebuffer.html)

理解硬件加速的关键是要理解FrameBuffer框架，也就是所谓的帧缓冲区，帧缓冲区到底是什么呢？为什么向帧缓冲区写数据就能直接显示呢？

LCD控制器可以通过编程支持不同LCD屏的要求，例如行和列像素数，数据总线宽度，接口时序和刷新频率等。LCD控制器的主要作用，是将定位在系统存储器中的显示缓冲区中的LCD图像数据传送到外部LCD驱动器。

Framebuffer，也叫帧缓冲，其内容对应于屏幕上的界面显示，可以将其简单理解为屏幕上显示内容对应的缓存，修改Framebuffer中的内容，即表示修改屏幕上的内容，所以，直接操作Framebuffer可以直接从显示器上观察到效果，Framebuffer就是一段存储空间，其可以位于显存，也可以位于内存，只要是在GPU能够访问的空间范围内(GPU的物理地址空间)，任意分配一段内存(或显存)，都可以作为Framebuffer使用，只需要在分配后将该内存区域信息，设置到显卡相关的寄存器中即可。这个其实跟DMA区域的概念是类似的，访问内存的是GPU，是GPU，

使用GTT

当使用GTT时，将图像显示到显示器上的大致逻辑是这样的：

在内存中分配一块缓存区域(作为Framebuffer)
将需要绘制的图形对应的数据拷贝到这块内存区域，当然，这个拷贝操作显然是由CPU负责的，即消耗的是CPU，GPU完全不参与。
GPU将Framebuffer中内容显示到显示器上(swapBuffer)。这个操作是由GPU负责的，消耗的是GPU，CPU基本不参与。这个过程显然有个数据搬移(可以理解为拷贝)的操作，毕竟搬移之前，数据还在内存中，是不可能直接显示到显示器上的。这个过程可以理解为一次DMA操作。
这个过程可以看出，如果使用GTT在做Framebuffer，存在“两次”数据拷贝操作：

1.将数据拷贝到Framebuffer中 2.从Framebuffer到显示器的数据搬移

虽然有两次数据拷贝操作，但CPU只负责其中一次，另一次由GPU负责。


# Linux FrameBuffer框架  使用了加速的内存从哪分配来

使用的仍然我们的内存，我们需要把一些东西放到内存，让GPU处理？？？


写到fb即可，fb是动态的，不是写死，可以动态映射，LCD的控制器可以将这部分内存设置到自己的寄存器，这样等到使用的GPU总线可以直接访问这块内存


CPU（Central Processing Unit，中央处理器）是计算机设备核心器件，用于执行程序代码，软件开发者对此都很熟悉；GPU（Graphics Processing Unit，图形处理器）主要用于处理图形运算，通常所说“显卡”的核心部件就是GPU。

和CPU不同的是，GPU就是为实现大量数学运算设计的。从结构图中可以看到，GPU的控制器比较简单，但包含了大量ALU。GPU中的ALU使用了并行设计，且具有较多浮点运算单元。

硬件加速的主要原理，就是通过底层软件代码，将CPU不擅长的图形计算转换成GPU专用指令，由GPU完成。扩展：很多计算机中的GPU有自己独立的显存；没有独立显存则使用共享内存的形式，从内存中划分一块区域作为显存。显存可以保存GPU指令等信息。

![帧缓冲区与视频缓冲区如何显示原理.jpg](http://upload-images.jianshu.io/upload_images/1460468-f8a24dc172586bb4.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


# Android基于FB的实现

然后会调用createBufferQueue函数创建一个producer和consumer。然后又创建了一个FramebufferSurface对象。这里我们看到在新建FramebufferSurface对象时把consumer参数传入了代表是一个消费者。
而在DisplayDevice的构造函数中（下面会讲到），会创建一个Surface对象传递给底层的OpenGL ES使用，而这个Surface是一个生产者。在OpenGl ES中合成好了图像之后会将图像数据写到Surface对象中，这将触发consumer对象的onFrameAvailable函数被调用。
这就是Surface数据好了就通知消费者来拿数据做显示用，在onFrameAvailable函数汇总，通过nextBuffer获得图像数据，然后调用HWComposer对象mHwc的fbPost函数输出。

# 为什么硬件加速好用更多的内存

GPU使用的仍然我们的内存，我们需要把一些东西放到内存，让GPU处理？？？

写到fb即可，fb是动态的，不是写死，可以动态映射，LCD的控制器可以将这部分内存设置到自己的寄存器，这样等到使用的GPU总线可以直接访问这块内存

总线

![帧缓冲区与视频缓冲区如何显示原理.jpg](http://upload-images.jianshu.io/upload_images/1460468-f8a24dc172586bb4.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# 哪怕硬件加速，也要先绘制再合成，哪怕软件加速，最后还是要合成


    在基于软件的绘制模型下，CPU主导绘图，视图按照两个步骤绘制：
1.      让View层次结构失效
2.      绘制View层次结构
    当应用程序需要更新它的部分UI时，都会调用内容发生改变的View对象的invalidate()方法。无效（invalidation）消息请求会在View对象层次结构中传递，以便计算出需要重绘的屏幕区域（脏区）。然后，Android系统会在View层次结构中绘制所有的跟脏区相交的区域。不幸的是，这种方法有两个缺点：
1.      绘制了不需要重绘的视图（与脏区域相交的区域）
2.      掩盖了一些应用的bug（由于会重绘与脏区域相交的区域）
    注意：在View对象的属性发生变化时，如背景色或TextView对象中的文本等，Android系统会自动的调用该View对象的invalidate()方法。
 
    在基于硬件加速的绘制模式下，GPU主导绘图，绘制按照三个步骤绘制：
1.      让View层次结构失效
2.      记录、更新显示列表
3.      绘制显示列表
这种模式下，Android系统依然会使用invalidate()方法和draw()方法来请求屏幕更新和展现View对象。但Android系统并不是立即执行绘制命令，而是首先把这些View的绘制函数作为绘制指令记录一个显示列表中，然后再读取显示列表中的绘制指令调用OpenGL相关函数完成实际绘制。另一个优化是，Android系统只需要针对由invalidate()方法调用所标记的View对象的脏区进行记录和更新显示列表。没有失效的View对象则能重放先前显示列表记录的绘制指令来进行简单的重绘工作。
使用显示列表的目的是，把视图的各种绘制函数翻译成绘制指令保存起来，对于没有发生改变的视图把原先保存的操作指令重新读取出来重放一次就可以了，提高了视图的显示速度。而对于需要重绘的View，则更新显示列表，以便下次重用，然后再调用OpenGL完成绘制。
硬件加速提高了Android系统显示和刷新的速度，但它也不是万能的，它有三个缺陷：
1.      兼容性（部分绘制函数不支持或不完全硬件加速，参见文章尾）
2.      内存消耗（OpenGL API调用就会占用8MB，而实际上会占用更多内存）
3.      电量消耗（GPU耗电）

系统侧

Android应用程序在图形缓冲区中绘制好View层次结构后，这个图形缓冲区会被交给SurfaceFlinger服务，而SurfaceFlinger服务再使用OpenGL图形库API来将这个图形缓冲区渲染到硬件帧缓冲区中。由于Android应用程序很少能涉及到Android系统底层，所以SurfaceFlinger服务的执行过程不做过多的介绍。



Android绘制流程（Android 6.0）
下面是安卓View完整的绘制流程图，主要通过阅读源码和调试得出，虚线箭头表示递归调用。

从ViewRootImpl.performTraversals到PhoneWindow.DecroView.drawChild是每次遍历View树的固定流程，首先根据标志位判断是否需要重新布局并执行布局；然后进行Canvas的创建等操作开始绘制。

如果硬件加速不支持或者被关闭，则使用软件绘制，生成的Canvas即Canvas.class的对象；
如果支持硬件加速，则生成的是DisplayListCanvas.class的对象；
两者的isHardwareAccelerated()方法返回的值分别为false、true，View根据这个值判断是否使用硬件加速。
View中的draw(canvas,parent,drawingTime) - draw(canvas) - onDraw - dispachDraw - drawChild这条递归路径（下文简称Draw路径），调用了Canvas.drawXxx()方法，在软件渲染时用于实际绘制；在硬件加速时，用于构建DisplayList。
View中的updateDisplayListIfDirty - dispatchGetDisplayList - recreateChildDisplayList这条递归路径（下文简称DisplayList路径），仅在硬件加速时会经过，用于在遍历View树绘制的过程中更新DisplayList属性，并快速跳过不需要重建DisplayList的View。

Android 6.0中，和DisplayList相关的API目前仍被标记为“@hide”不可访问，表示还不成熟，后续版本可能开放。
硬件加速情况下，draw流程执行结束后DisplayList构建完成，然后通过ThreadedRenderer.nSyncAndDrawFrame()利用GPU绘制DisplayList到屏幕上。

![引用绘制流程](http://tech.meituan.com/img/hardware-accelerate/render-func.png)  

**这里我们首先要明确什么是硬件加速渲染，其实就是通过GPU来进行渲染。GPU作为一个硬件，用户空间是不可以直接使用的，它是由GPU厂商按照Open GL规范实现的驱动间接进行使用的。也就是说，如果一个设备支持GPU硬件加速渲染，那么当Android应用程序调用Open GL接口来绘制UI时，Android应用程序的UI就是通过硬件加速技术进行渲染的。因此，在接下来的描述中，我们提及到GPU、硬件加速和Open GL时，它们表达的意思都是等价的。**

从图2可以看到，硬件加速渲染和软件渲染一样，在开始渲染之前，都是要先向SurfaceFlinger服务Dequeue一个Graphic Buffer。不过对硬件加速渲染来说，这个Graphic Buffer会被封装成一个ANativeWindow，并且传递给Open GL进行硬件加速渲染环境初始化。在Android系统中，ANativeWindow和Surface可以是认为等价的，只不过是ANativeWindow常用于Native层中，而Surface常用于Java层中。另外，我们还可以将ANativeWindow和Surface看作是像Skia和Open GL这样图形渲染库与操作系统底层的图形系统建立连接的一个桥梁。       Open GL获得了一个ANativeWindow，并且进行了硬件加速渲染环境初始化工作之后，Android应用程序就可以调用Open GL提供的API进行UI绘制了，绘制出来内容就保存在前面获得的Graphic Buffer中。当绘制完毕，Android应用程序再调用libegl库提供的一个eglSwapBuffer接口请求将绘制好的UI显示到屏幕中，其本质上与软件渲染过程是一样的，都是向SurfaceFlinger服务Queue一个Graphic Buffer，以便SurfaceFlinger服务可以对Graphic Buffer的内容进行合成，以及显示到屏幕上去。
              

# GLSurfaceView跟SurfaceView的区别

# 哪个过程是CPU哪个是GPU ？绘制是GPU
       
       
 
![硬件加速.jpg](http://upload-images.jianshu.io/upload_images/1460468-1f3c83ffb4e74889.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

虽说是List，但是感觉更像是一个树。

# 概念上的不同  OpenGL、3D、2D、Skia、硬件加速中的软件绘制、全部软件绘制

每个View抽象成一个RenderNode，每个RenderNode都包含了一些绘制命令列表，而ViewGroup本身会将子View的RenderNode抽象成绘制命令，放到自己的绘制命令列表中来，这样ViewGroup在绘制的时候，就会递归调用子View的的绘制命令列表。

### 构建列表，哪些需要构建

* 一个是自己的绘制命令，比如onDraw中调用的drawLine drawBitmap等，
* 另一部分是子View及背景的绘制，一般是抽象成RenderNode命令，

软件绘制的抽象成BitMapOp

DisplayListCanvas的目的就是构建及暂存供，在第一步完成后，就没有意义，recycle了通过start end 搞一个封闭，nSetDisplayListData将回执命令及数据添加到RenderNode中去.

	void RenderNode::setStagingDisplayList(DisplayListData* data) {
	    mNeedsDisplayListDataSync = true;
	    delete mStagingDisplayListData;
	    mStagingDisplayListData = data;
	}

这个包含了所有绘制操作及数据，比如DrawBitmapOp里面包含了bitmap数据。

    GraphicsJNI::getSkBitmap(env, jbitmap, &bitmap);
    
          
# 参考文档

[闲聊Framebuffer](http://happyseeker.github.io/kernel/2016/05/24/about-framebuffer.html)       
[Android SurfaceFlinger 学习之路(五)----VSync 工作原理](http://windrunnerlihuan.com/2017/05/25/Android-SurfaceFlinger-%E5%AD%A6%E4%B9%A0%E4%B9%8B%E8%B7%AF-%E4%BA%94-VSync-%E5%B7%A5%E4%BD%9C%E5%8E%9F%E7%90%86/)      
[美团技术团队 Android硬件加速原理与实现简介](https://tech.meituan.com/hardware-accelerate.html)        
[Android系统的开机画面显示过程分析](http://blog.csdn.net/luoshengyang/article/details/7691321)         
[Android帧缓冲区（Frame Buffer）硬件抽象层（HAL）模块Gralloc的实现原理分析](http://blog.csdn.net/luoshengyang/article/details/7747932)               
[视频讲解 Android应用程序UI硬件加速渲染技术 罗胜阳](http://www.infoq.com/cn/presentations/android-application-ui-hardware-accelerated-rendering-technology)     