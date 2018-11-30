[参考文档1](https://developer.android.com/studio/profile/inspect-gpu-rendering?hl=zh-cn)
[参考文档2](https://developer.android.com/topic/performance/rendering/profile-gpu)


## Bitmap  prepareToDraw


prepareToDraw

added in API level 4

public void prepareToDraw ()
Builds caches associated with the bitmap that are used for drawing it.

Starting in Build.VERSION_CODES.N, this call initiates an asynchronous upload to the GPU on RenderThread, if the Bitmap is not already uploaded. With Hardware Acceleration, Bitmaps must be uploaded to the GPU in order to be rendered. This is done by default the first time a Bitmap is drawn, but the process can take several milliseconds, depending on the size of the Bitmap. Each time a Bitmap is modified and drawn again, it must be re-uploaded.

Calling this method in advance can save time in the first frame it's used. For example, it is recommended to call this on an image decoding worker thread when a decoded Bitmap is about to be displayed. It is recommended to make any pre-draw modifications to the Bitmap before calling this method, so the cached, uploaded copy may be reused without re-uploading.

In Build.VERSION_CODES.KITKAT and below, for purgeable bitmaps, this call would attempt to ensure that the pixels have been decoded.


## issue

![image.png](https://upload-images.jianshu.io/upload_images/1460468-7ee8aec8ad5b6ab6.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)



# 关于触摸事件

![image.png](https://upload-images.jianshu.io/upload_images/1460468-429764b60708b10d.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

颜色好像对不上，绿色是测量布局：


直观上说，Vsync垂直同步信号是UI重绘的触发器

系统获取FPS的原理：手机屏幕显示的内容是通过Android系统的SurfaceFlinger类，把当前系统里所有进程需要显示的信息合成一帧，然后提交到屏幕上显示，FPS就是1秒内SurfaceFlinger提交到屏幕的帧数，


App停止操作后，FPS还是在一直变化，这种情况是否会影响到FPS的准确度？
有的时候FPS很低，APP看起来却很流畅，是因为当前界面在1秒内只需要10帧的显示需求，当然不会卡顿，此时FPS只要高于10就可以了，如果屏幕根本没有绘制需求，那FPS的值就是0。



**注： 尽管此工具名为 Profile GPU Rendering，但所有受监控的进程实际上发生在 CPU 中。 通过将命令提交到 GPU 触发渲染，GPU 异步渲染屏幕。 在某些情况下，GPU 会有太多工作要处理，在它可以提交新命令前，您的 CPU 必须等待。 在等待时，您将看到橙色条和红色条中出现峰值，且命令提交将被阻止，直到 GPU 命令队列腾出更多空间。**


![image.png](https://upload-images.jianshu.io/upload_images/1460468-6461878f98d427e0.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# FPS只针对特定场景才有意义

对于掉帧我们需要关注场景：动画、滚动、数据流（视频播放或者录制预览等）、只有这些场景才需要关注帧率，掉帧，

* 掉帧的概念
* 掉帧的原因
* 掉帧的处理方式


**注意，必须重绘，才会看到GPU渲染更新，还要注意这些流程图跟踪的全部是在CPU中，并不能真正100%反应帧率，毕竟不是同步绘制**

首先有个概念要清楚，GPU中OpenGL驱动分为软件实现跟硬件试下，软件实现的一般都是同步的，不存在GPU处理一说，Android源码自带的是软件实现的Agl，Android通过软件方法实现的一套OpenGL动态库

在使用 systrace 的过程中，请记住，每个事件都是由 CPU 上的活动触发的。


	static const std::array<BarSegment,7> Bar {{
	    { FrameInfoIndex::IntendedVsync, FrameInfoIndex::HandleInputStart, 0x00796B },
	    { FrameInfoIndex::HandleInputStart, FrameInfoIndex::PerformTraversalsStart, 0x388E3C },
	    { FrameInfoIndex::PerformTraversalsStart, FrameInfoIndex::DrawStart, 0x689F38},
	    { FrameInfoIndex::DrawStart, FrameInfoIndex::SyncStart, 0x2196F3},
	    { FrameInfoIndex::SyncStart, FrameInfoIndex::IssueDrawCommandsStart, 0x4FC3F7},
	    { FrameInfoIndex::IssueDrawCommandsStart, FrameInfoIndex::SwapBuffers, 0xF44336},
	    { FrameInfoIndex::SwapBuffers, FrameInfoIndex::FrameCompleted, 0xFF9800},
	}};

可以以看看每个阶段的数值

橙色部分表示的是处理时间,或者说是CPU告诉GPU渲染一帧的地方,这是一个阻塞调用,因为CPU会一直等待GPU发出接到命令的回复,如果柱状图很高,那就意味着你给GPU太多的工作,太多的负责视图需要OpenGL命令去绘制和处理.


# OpenGL Profiler源码

	void CanvasContext::draw() {
	    ...
	    profiler().draw(mCanvas);
	    
    
    FrameInfoVisualizer& profiler() { return mProfiler; }

条形图

	void FrameInfoVisualizer::draw(OpenGLRenderer* canvas) {
	    RETURN_IF_DISABLED();
	
	    if (mShowDirtyRegions) {
	        mFlashToggle = !mFlashToggle;
	        if (mFlashToggle) {
	            SkPaint paint;
	            paint.setColor(0x7fff0000);
	            canvas->drawRect(mDirtyRegion.fLeft, mDirtyRegion.fTop,
	                    mDirtyRegion.fRight, mDirtyRegion.fBottom, &paint);
	        }
	    }
	
	    // 绘制一条条
	    if (mType == ProfileType::Bars) {
	        // Patch up the current frame to pretend we ended here. CanvasContext
	        // will overwrite these values with the real ones after we return.
	        // This is a bit nicer looking than the vague green bar, as we have
	        // valid data for almost all the stages and a very good idea of what
	        // the issue stage will look like, too
	        FrameInfo& info = mFrameSource.back();
	        info.markSwapBuffers();
	        info.markFrameCompleted();
	
	        initializeRects(canvas->getViewportHeight(), canvas->getViewportWidth());
	        drawGraph(canvas);
	        drawThreshold(canvas);
	    }
	}

	<!--同步开始-->
	
	void CanvasContext::prepareTree(TreeInfo& info, int64_t* uiFrameInfo, int64_t syncQueued) {
	    mRenderThread.removeFrameCallback(this);
	
	    // If the previous frame was dropped we don't need to hold onto it, so
	    // just keep using the previous frame's structure instead
	    if (!wasSkipped(mCurrentFrameInfo)) {
	        mCurrentFrameInfo = &mFrames.next();
	    }
	    mCurrentFrameInfo->importUiThreadInfo(uiFrameInfo);
	    mCurrentFrameInfo->set(FrameInfoIndex::SyncQueued) = syncQueued;
	    // 这里表示同步上传？
	    mCurrentFrameInfo->markSyncStart();
	
	    info.damageAccumulator = &mDamageAccumulator;
	    info.renderer = mCanvas;
	    info.canvasContext = this;
	
	    mAnimationContext->startFrame(info.mode);
	    // node
	    mRootRenderNode->prepareTree(info);
	    mAnimationContext->runRemainingAnimations(info);
	
	// 什么意思
	    freePrefetechedLayers();
	
	    if (CC_UNLIKELY(!mNativeWindow.get())) {
	        mCurrentFrameInfo->addFlag(FrameInfoFlags::SkippedFrame);
	        info.out.canDrawThisFrame = false;
	        return;
	    }
	
	    int runningBehind = 0;
	    // TODO: This query is moderately expensive, investigate adding some sort
	    // of fast-path based off when we last called eglSwapBuffers() as well as
	    // last vsync time. Or something.
	    mNativeWindow->query(mNativeWindow.get(),
	            NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND, &runningBehind);
	    info.out.canDrawThisFrame = !runningBehind;
	
	    if (!info.out.canDrawThisFrame) {
	        mCurrentFrameInfo->addFlag(FrameInfoFlags::SkippedFrame);
	    }
	
	    if (info.out.hasAnimations || !info.out.canDrawThisFrame) {
	        if (!info.out.requiresUiRedraw) {
	            // If animationsNeedsRedraw is set don't bother posting for an RT anim
	            // as we will just end up fighting the UI thread.
	            mRenderThread.postFrameCallback(this);
	        }
	    }
	}



	// 创建Layer 以及其帧缓冲 ？？
	void RenderNode::prepareTreeImpl(TreeInfo& info, bool functorsNeedLayer) {
	    info.damageAccumulator->pushTransform(this);
	
	    if (info.mode == TreeInfo::MODE_FULL) {
	        // 同步到这里
	        pushStagingPropertiesChanges(info);
	    }
	    uint32_t animatorDirtyMask = 0;
	    if (CC_LIKELY(info.runAnimations)) {
	        animatorDirtyMask = mAnimatorManager.animate(info);
	    }
	
	    bool willHaveFunctor = false;
	    if (info.mode == TreeInfo::MODE_FULL && mStagingDisplayListData) {
	        willHaveFunctor = !mStagingDisplayListData->functors.isEmpty();
	    } else if (mDisplayListData) {
	        willHaveFunctor = !mDisplayListData->functors.isEmpty();
	    }
	    bool childFunctorsNeedLayer = mProperties.prepareForFunctorPresence(
	            willHaveFunctor, functorsNeedLayer);
	// layer
	    prepareLayer(info, animatorDirtyMask);
	    if (info.mode == TreeInfo::MODE_FULL) {
	        pushStagingDisplayListChanges(info);
	    }
	    prepareSubTree(info, childFunctorsNeedLayer, mDisplayListData);
	    // push
	    pushLayerUpdate(info);
	
	    info.damageAccumulator->popTransform();
	}


	
	void CanvasContext::draw() {
	    ...
	    <!--Issue的开始-->
	    mCurrentFrameInfo->markIssueDrawCommandsStart();
		...
	    <!--构建-->
	    profiler().draw(mCanvas);
	    <!--像GPU发送命令-->
		 mCanvas->drawRenderNode(mRootRenderNode.get(), outBounds);
	    // Even if we decided to cancel the frame, from the perspective of jank
	    // metrics the frame was swapped at this point
	    <!--命令发送完毕-->
	    mCurrentFrameInfo->markSwapBuffers();
	    ...
	    // TODO: Use a fence for real completion?
	    <!--这里只有用fence才能获取真正的耗时，不然还是无效的，看每个手机厂家的实现了-->
	    mCurrentFrameInfo->markFrameCompleted();
	    mJankTracker.addFrame(*mCurrentFrameInfo);
	    mRenderThread.jankTracker().addFrame(*mCurrentFrameInfo);
	}

Vsync信号到来后CanvasContext::prepareTree，县拷贝UI中的信息

	void CanvasContext::prepareTree(TreeInfo& info, int64_t* uiFrameInfo, int64_t syncQueued) {
	    mRenderThread.removeFrameCallback(this);
	
	    // If the previous frame was dropped we don't need to hold onto it, so
	    // just keep using the previous frame's structure instead
	    if (!wasSkipped(mCurrentFrameInfo)) {
	        mCurrentFrameInfo = &mFrames.next();
	    }
	    mCurrentFrameInfo->importUiThreadInfo(uiFrameInfo);
	    mCurrentFrameInfo->set(FrameInfoIndex::SyncQueued) = syncQueued;
	    mCurrentFrameInfo->markSyncStart();
	
预先构建


	void DrawFrameTask::run() {
	    ATRACE_NAME("DrawFrame");
	
	    bool canUnblockUiThread;
	    bool canDrawThisFrame;
	    {
	        // 这里用的是TreeInfo::MODE_FULL
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

Java层的信息来自下面，其实只有Java层的task在UI线程，其余的都在render线程，postandwait，同步完成后，UI线程就不会被阻塞了，后面的draw

>Chrophopher

绘制开始，测量 动画  inupt
	
	  void doFrame(long frameTimeNanos, int frame) {
	        final long startNanos;
	        synchronized (mLock) {
	            if (!mFrameScheduled) {
	                return; // no work to do
	            }
	
	            if (DEBUG_JANK && mDebugPrintNextFrameTimeDelta) {
	                mDebugPrintNextFrameTimeDelta = false;
	                Log.d(TAG, "Frame time delta: "
	                        + ((frameTimeNanos - mLastFrameTimeNanos) * 0.000001f) + " ms");
	            }
	
	            long intendedFrameTimeNanos = frameTimeNanos;
	            startNanos = System.nanoTime();
	            final long jitterNanos = startNanos - frameTimeNanos;
	            if (jitterNanos >= mFrameIntervalNanos) {
	                final long skippedFrames = jitterNanos / mFrameIntervalNanos;
	                if (skippedFrames >= SKIPPED_FRAME_WARNING_LIMIT) {
	                    Log.i(TAG, "Skipped " + skippedFrames + " frames!  "
	                            + "The application may be doing too much work on its main thread.");
	                }
	                final long lastFrameOffset = jitterNanos % mFrameIntervalNanos;
	                if (DEBUG_JANK) {
	                    Log.d(TAG, "Missed vsync by " + (jitterNanos * 0.000001f) + " ms "
	                            + "which is more than the frame interval of "
	                            + (mFrameIntervalNanos * 0.000001f) + " ms!  "
	                            + "Skipping " + skippedFrames + " frames and setting frame "
	                            + "time to " + (lastFrameOffset * 0.000001f) + " ms in the past.");
	                }
	                frameTimeNanos = startNanos - lastFrameOffset;
	            }
	
	            if (frameTimeNanos < mLastFrameTimeNanos) {
	                if (DEBUG_JANK) {
	                    Log.d(TAG, "Frame time appears to be going backwards.  May be due to a "
	                            + "previously skipped frame.  Waiting for next vsync.");
	                }
	                scheduleVsyncLocked();
	                return;
	            }
	
	            mFrameInfo.setVsync(intendedFrameTimeNanos, frameTimeNanos);
	            mFrameScheduled = false;
	            mLastFrameTimeNanos = frameTimeNanos;
	        }
	
	        try {
	            Trace.traceBegin(Trace.TRACE_TAG_VIEW, "Choreographer#doFrame");
	
	            mFrameInfo.markInputHandlingStart();
	            doCallbacks(Choreographer.CALLBACK_INPUT, frameTimeNanos);
	
	            mFrameInfo.markAnimationsStart();
	            doCallbacks(Choreographer.CALLBACK_ANIMATION, frameTimeNanos);
	
	            mFrameInfo.markPerformTraversalsStart();
	

ThreadRender.java  

    void draw(View view, AttachInfo attachInfo, HardwareDrawCallbacks callbacks) {
        attachInfo.mIgnoreDirtyState = true;

        final Choreographer choreographer = attachInfo.mViewRootImpl.mChoreographer;
        choreographer.mFrameInfo.markDrawStart();
 
 draw从这个开始
        	
	            
The following are a few things to note about the output:

* For each visible application, the tool displays a graph.
* Each vertical bar along the horizontal axis represents a frame, and the height of each vertical bar represents the amount of time the frame took to render (in milliseconds).
* The horizontal green line represents 16 milliseconds. To achieve 60 frames per second, the vertical bar for each frame needs to stay below this line. Any time a bar surpasses this line, there may be pauses in the animations.
* The tool highlights frames that exceed the 16 millisecond threshold by making the corresponding bar wider and less transparent.
* Each bar has colored components that map to a stage in the rendering pipeline. The number of components vary depending on the API level of the device.

![GPU呈现模式](https://upload-images.jianshu.io/upload_images/1460468-ff5f91880763bbb0.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

通过 $ adb shell dumpsys gfxinfo your_package可以得到当前GPU渲染使用的时间


    Draw	Prepare	Process	Execute
	10.30	0.67	5.21	3.51
	2.73	6.59	5.06	6.03
	4.38	1.92	5.81	5.32
	7.54	0.35	5.12	2.67
	4.18	0.50	5.58	1.97
	2.97	0.61	5.09	1.90
	3.23	0.51	5.22	1.78
	16.66	0.52	5.00	2.65
	13.79	0.52	5.10	6.74
	7.53	0.44	8.13	3.91
	23.74	0.30	6.03	2.19
	2.69	0.52	6.22	1.91
	7.01	0.62	5.24	12.73
	3.09	0.57	9.08	2.48
	3.22	0.62	6.11	3.90
	2.72	0.61	5.69	1.66
	3.61	2.10	12.52	2.56
	3.25	1.94	7.97	40.51
	5.57	0.60	5.66	16.36
	25.20	0.44	7.49	8.48
	14.42	1.94	8.82	3.94
	14.00	2.42	8.72	1.94
	13.25	1.84	17.10	2.06
	8.50	2.37	50.00	1.89
	12.26	0.48	6.67	2.14
	50.00	0.70	6.80	2.09
	3.28	0.54	5.86	3.67
	4.07	0.58	6.31	6.77
	3.48	0.46	6.06	2.02
	3.08	0.69	5.86	2.43
	5.25	1.03	5.95	5.12
	4.39	0.49	6.24	2.22
	3.24	0.72	6.22	1.76
	4.73	0.48	5.73	1.85
	3.32	0.65	5.94	1.84
	6.11	1.58	5.81	11.21
	50.00	2.45	18.20	2.94
	50.00	1.33	5.07	1.08
	27.31	0.25	6.38	1.11
	6.12	2.38	14.91	1.00
	1.54	0.38	4.08	0.71
	1.06	3.50	4.57	0.72
	0.98	2.25	4.87	2.54
	1.17	1.09	4.29	0.51
	0.96	0.39	4.44	0.49
	1.03	0.47	4.15	6.67
	1.18	0.43	4.25	0.53
	1.46	0.47	4.00	0.63
	7.01	0.43	4.12	0.63
	1.20	0.43	3.88	0.50
	0.99	0.45	3.95	0.68
	0.97	0.44	3.78	0.51
	1.69	0.49	4.32	0.65
	1.15	0.42	4.17	0.51
	1.04	0.42	4.18	0.79
	1.01	0.45	3.67	0.57
	1.05	0.49	3.91	0.59
	1.05	0.41	3.84	0.72
	2.15	0.43	4.04	0.67
	1.01	0.42	3.85	0.60
	1.03	0.42	3.89	1.14
	1.22	0.47	3.97	0.52
	1.03	0.44	11.03	0.63
	1.26	0.48	3.96	0.51
	1.09	0.42	3.77	0.48
	1.10	9.17	4.68	1.45
	1.19	0.25	4.26	0.54
	1.17	0.42	3.79	0.50
	1.26	0.43	4.24	0.53
	1.16	0.42	3.95	0.49
	1.01	0.42	3.73	0.46
	1.10	0.41	3.78	0.45
	1.15	0.99	10.85	0.63
	1.24	0.53	4.01	0.48
	50.00	16.28	50.00	0.72
	50.00	0.50	7.68	0.64
	50.00	1.79	17.77	0.56
	2.60	0.44	8.05	0.65
	3.86	1.18	8.05	0.59
	2.87	0.58	10.69	0.60
	3.55	0.63	9.46	1.65
	3.34	0.59	8.65	0.66
	3.53	0.64	11.66	0.89
	3.51	0.56	8.21	0.97
	3.09	0.64	16.08	0.89
	7.01	0.44	11.79	0.70
	2.94	0.68	8.72	3.90
	3.18	1.09	8.58	0.61
	3.19	0.72	8.49	0.62
	3.47	0.64	8.63	0.68
	3.05	0.77	7.92	0.60
	2.62	0.65	8.65	0.62
	2.81	1.17	8.34	0.64
	7.84	0.65	10.72	1.10
	3.54	0.55	9.71	0.66
	3.27	8.49	12.95	1.00
	4.66	0.49	9.35	1.05
	2.26	0.53	9.23	0.78
	2.34	0.80	8.87	0.60
	2.87	0.64	8.05	0.55
	2.47	0.69	8.33	0.51
	2.62	0.64	7.83	0.56
	3.21	5.52	7.86	0.64
	2.79	0.50	8.34	0.98
	3.17	1.02	8.35	0.57
	2.93	0.84	7.85	0.68
	2.80	0.75	7.93	0.58
	2.78	0.90	8.85	0.52
	3.09	0.76	7.54	0.49
	2.95	0.74	7.92	0.57
	2.72	0.71	8.00	0.53
	3.05	0.74	8.48	0.55
	2.72	0.76	8.22	0.57
	2.42	0.70	8.91	0.57
	2.57	0.76	8.20	0.59
	2.32	0.72	7.39	0.56
	2.35	0.71	8.59	0.55
	2.54	0.64	7.42	0.64
	2.70	0.56	7.59	0.52
	2.19	0.95	8.70	0.66

Stats since: 816160107662ns
Total frames rendered: 185077
Janky frames: 21764 (11.76%)
90th percentile: 18ms
95th percentile: 29ms
99th percentile: 53ms
Number Missed Vsync: 2746
Number High input latency: 165
Number Slow UI thread: 12367
Number Slow bitmap uploads: 1621
Number Slow issue draw commands: 7533

>在 Android M 中 gfxinfo（Profile data in ms） 的基础数值来源于 FrameInfo，详见源码：FrameInfoVisualizer。gfxinfo（Profile data in ms）只保存了 Surface 最近渲染的128帧的信息，因此，Jankiness count、Max accumulated frames、Frame rate 也仅仅是针对这 128 帧数据所计算出来的结果,它们的具体含义分别是：



* (1). Draw 对应于蓝色线：是消耗在构建java显示列表DisplayList的时间。说白了就是执行每一个View的onDraw方法,创建或者更新每一个View的DisplayList对象的时间。
* (2). Process 对应于红色线：是消耗在Android的2D渲染器执行显示列表的时间。你的视图层次越多，要执行的绘图命令就越多。
* (3). Execute 对应于橙色线：是消耗在排列每个发送过来的帧的顺序的时间.或者说是CPU告诉GPU渲染一帧的时间,这是一个阻塞调用,因为CPU会一直等待GPU发出接到命令的回复。其实可以简单理解为：红色线<span style="font-family: Arial, Helvetica, sans-serif;">Process时间＋GPU返回时</span><span style="font-family: Arial, Helvetica, sans-serif;">GPU</span><span style="font-family: Arial, Helvetica, sans-serif;">与CPU通信的时间</span>


GPU 呈现模式分析工具又可以获取什么信息呢：

* 可以查看android 手机每一帧的渲染情况，每一帧的总耗时，是否超过16ms，中间绿色横线表示16ms
* 指定计算区域的耗时情况，图片里每种颜色代表不同的数据计算阶段

渲染流程

* cpu测量，布局界面上变动的视图对象，然后绘制这些 view(onDraw方法) 生成界面一帧数据
* 然后 cpu 把这计算出的这一帧数据传递给 gpu，这一帧数据也叫纹理，具体的去看 OpenGL的内容
* gpu 根据cpu 传递过来的纹理数据，去具体的绘制出2D 图形来
* cpu 等待 pgu 通知绘制完成，cpu 才可以去干别的事，要不 cpu 会一直等着。。。这才算是完成了一帧的渲染

# swapbuffer的意义？

看名字，是交换Buffer，如果之前有个Buffer在用，现在就换一个，后面的替换前面的，双缓冲？ 3缓冲？queueBuffer，SF不一定能用，等GPU处理完SF才能用通知GPU执行，到底是哪个在通知呢 ？ APP 还是。。。

queue一个，dequeue一个，双缓冲，就两个，queue好后，queue的不一定会被用，如果dequeue失败，说明，还来不及用，如果成功，说明，就要用了，每个surface有两个，前后，slot是个缓存，


# FBO离屏渲染

申请个fbo，绘制材质fbo 直接用fbo，纹理，绑定图片纹理，纹理，绑定fbo，填充fbo，

# 参考文档

[Analyze with Profile GPU Rendering](https://developer.android.com/topic/performance/rendering/profile-gpu)    
[Android客户端性能工具2:FrameInfoVisualizer(gfxinfo和开发者选项gpu信息)分析](https://blog.csdn.net/woai110120130/article/details/79246547)    
[Android5.0中 hwui 中 RenderThread 工作流程](https://www.jianshu.com/p/bc1c1d2fadd1)    
[](http://www.voidcn.com/article/p-njbssmva-bqc.html)         
[原Android 5.1 SurfaceFlinger VSYNC详解](https://blog.csdn.net/newchenxf/article/details/49131167)                     
[Android中的GraphicBuffer同步机制-Fence](https://blog.csdn.net/jinzhuojun/article/details/39698317)                           
[android graphic(15)—fence](https://blog.csdn.net/lewif/article/details/50984212)              
[原android graphic(16)—fence(简化](https://blog.csdn.net/lewif/article/details/51007148)    
[了解 Systrace](https://source.android.com/devices/tech/debug/systrace)            
[Android帧率、卡顿详解及使用](https://blog.csdn.net/Jack_Chen3/article/details/76714030)