Viewroot 会调用ThreadRender.java的draw		
		
    @Override
    void draw(View view, AttachInfo attachInfo, HardwareDrawCallbacks callbacks) {
        attachInfo.mIgnoreDirtyState = true;

        final Choreographer choreographer = attachInfo.mViewRootImpl.mChoreographer;
        choreographer.mFrameInfo.markDrawStart();

        updateRootDisplayList(view, callbacks);

        attachInfo.mIgnoreDirtyState = false;

        // register animating rendernodes which started animating prior to renderer
        // creation, which is typical for animators started prior to first draw
        if (attachInfo.mPendingAnimatingRenderNodes != null) {
            final int count = attachInfo.mPendingAnimatingRenderNodes.size();
            for (int i = 0; i < count; i++) {
                registerAnimatingRenderNode(
                        attachInfo.mPendingAnimatingRenderNodes.get(i));
            }
            attachInfo.mPendingAnimatingRenderNodes.clear();
            // We don't need this anymore as subsequent calls to
            // ViewRootImpl#attachRenderNodeAnimator will go directly to us.
            attachInfo.mPendingAnimatingRenderNodes = null;
        }

        final long[] frameInfo = choreographer.mFrameInfo.mFrameInfo;
        int syncResult = nSyncAndDrawFrame(mNativeProxy, frameInfo, frameInfo.length);
        if ((syncResult & SYNC_LOST_SURFACE_REWARD_IF_FOUND) != 0) {
            setEnabled(false);
            attachInfo.mViewRootImpl.mSurface.release();
            // Invalidate since we failed to draw. This should fetch a Surface
            // if it is still needed or do nothing if we are no longer drawing
            attachInfo.mViewRootImpl.invalidate();
        }
        if ((syncResult & SYNC_INVALIDATE_REQUIRED) != 0) {
            attachInfo.mViewRootImpl.invalidate();
        }
    }
    
nSyncAndDrawFrame会进一步调用

	
	static int android_view_ThreadedRenderer_syncAndDrawFrame(JNIEnv* env, jobject clazz,
	        jlong proxyPtr, jlongArray frameInfo, jint frameInfoSize) {
	    LOG_ALWAYS_FATAL_IF(frameInfoSize != UI_THREAD_FRAME_INFO_SIZE,
	            "Mismatched size expectations, given %d expected %d",
	            frameInfoSize, UI_THREAD_FRAME_INFO_SIZE);
	    RenderProxy* proxy = reinterpret_cast<RenderProxy*>(proxyPtr);
	    env->GetLongArrayRegion(frameInfo, 0, frameInfoSize, proxy->frameInfo());
	    return proxy->syncAndDrawFrame();
	}

    int RenderProxy::syncAndDrawFrame() {
    return mDrawFrameTask.drawFrame();
		}

 插入一个Task

	int DrawFrameTask::drawFrame() {
	    LOG_ALWAYS_FATAL_IF(!mContext, "Cannot drawFrame with no CanvasContext!");
	
	    mSyncResult = kSync_OK;
	    mSyncQueued = systemTime(CLOCK_MONOTONIC);
	    postAndWait();
	
	    return mSyncResult;
	}
	
	void DrawFrameTask::postAndWait() {
	    AutoMutex _lock(mLock);
	    mRenderThread->queue(this);
	    mSignal.wait(mLock);
	}



	void DrawFrameTask::run() {
	
	    bool canUnblockUiThread;
	    bool canDrawThisFrame;
	    	...
	    if (CC_LIKELY(canDrawThisFrame)) {
	        context->draw();
	    }
	     ...
	}‘
	
绘制

	void CanvasContext::draw() {
	
		 SkRect dirty;
	    mDamageAccumulator.finish(&dirty);
	
	    mCurrentFrameInfo->markIssueDrawCommandsStart();
	    EGLint width, height;
	    <!--关键点1 beginFrame -->
	    mEglManager.beginFrame(mEglSurface, &width, &height);
	    <!--脏区处理-->
	    ...
	    Rect outBounds;
	    // 递归绘制
	    mCanvas->drawRenderNode(mRootRenderNode.get(), outBounds);
	     <!--性能工具，不需关系-->
	    profiler().draw(mCanvas);
	
	    bool drew = mCanvas->finish();
	
	    // Even if we decided to cancel the frame, from the perspective of jank
	    // metrics the frame was swapped at this point
	    mCurrentFrameInfo->markSwapBuffers();
	
	    if (drew) {
	    <!---->
	        swapBuffers(dirty, width, height);
	    }
	
	    // TODO: Use a fence for real completion?
	    mCurrentFrameInfo->markFrameCompleted();
	    mJankTracker.addFrame(*mCurrentFrameInfo);
	    mRenderThread.jankTracker().addFrame(*mCurrentFrameInfo);
	}
 
 这里的mCanvas = new OpenGLRenderer(mRenderThread.renderState());
 
	 bool CanvasContext::initialize(ANativeWindow* window) {
	    setSurface(window);
	    if (mCanvas) return false;
	    mCanvas = new OpenGLRenderer(mRenderThread.renderState());
	    mCanvas->initProperties();
	    return true;
	}


	void OpenGLRenderer::drawRenderNode(RenderNode* renderNode, Rect& dirty, int32_t replayFlags) {
	    // All the usual checks and setup operations (quickReject, setupDraw, etc.)
	    // will be performed by the display list itself
	    if (renderNode && renderNode->isRenderable()) {
	        // compute 3d ordering
	        renderNode->computeOrdering();
	        if (CC_UNLIKELY(Properties::drawDeferDisabled)) {
	            startFrame();
	            ReplayStateStruct replayStruct(*this, dirty, replayFlags);
	            // drawRenderNode
	            renderNode->replay(replayStruct, 0);
	            return;
	        }
	
	        // Don't avoid overdraw when visualizing, since that makes it harder to
	        // debug where it's coming from, and when the problem occurs.
	        bool avoidOverdraw = !Properties::debugOverdraw;
	        DeferredDisplayList deferredList(mState.currentClipRect(), avoidOverdraw);
	        DeferStateStruct deferStruct(deferredList, *this, replayFlags);
	        renderNode->defer(deferStruct, 0);
	
	        flushLayers();
	        startFrame();
	
			 // 真正的绘制 ？？
	        deferredList.flush(*this, dirty);
	    } else {
	        // Even if there is no drawing command(Ex: invisible),
	        // it still needs startFrame to clear buffer and start tiling.
	        startFrame();
	    }
	}

<!--这里是不是harderwareLayer-->	
	
	void OpenGLRenderer::flushLayers() {
	    int count = mLayerUpdates.size();
	    if (count > 0) {
	        startMark("Apply Layer Updates");
	
	        // Note: it is very important to update the layers in order
	        for (int i = 0; i < count; i++) {
	            mLayerUpdates.itemAt(i)->flush();
	        }
	
	        mLayerUpdates.clear();
	        mRenderState.bindFramebuffer(getTargetFbo());
	
	        endMark();
	    }
	
	
	    
	void OpenGLRenderer::startFrame() {
	    if (mFrameStarted) return;
	    mFrameStarted = true;
	
	    mState.setDirtyClip(true);
	
	    discardFramebuffer(mTilingClip.left, mTilingClip.top, mTilingClip.right, mTilingClip.bottom);
	
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
		    
		    
	void DeferredDisplayList::flush(OpenGLRenderer& renderer, Rect& dirty) {
	    ATRACE_NAME("flush drawing commands");
	    Caches::getInstance().fontRenderer->endPrecaching();
	
	    if (isEmpty()) return; // nothing to flush
	    renderer.restoreToCount(1);
	
	    DEFER_LOGD("--flushing");
	    renderer.eventMark("Flush");
	
	    // save and restore so that reordering doesn't affect final state
	    renderer.save(SkCanvas::kMatrix_SaveFlag | SkCanvas::kClip_SaveFlag);
	
	    if (CC_LIKELY(mAvoidOverdraw)) {
	        for (unsigned int i = 1; i < mBatches.size(); i++) {
	            if (mBatches[i] && mBatches[i]->coversBounds(mBounds)) {
	                discardDrawingBatches(i - 1);
	            }
	        }
	    }
	    // NOTE: depth of the save stack at this point, before playback, should be reflected in
	    // FLUSH_SAVE_STACK_DEPTH, so that save/restores match up correctly
	    replayBatchList(mBatches, renderer, dirty);
	
	    renderer.restoreToCount(1);
	
	    clear();
		}
	
		    
之前设定的
    
	void CanvasContext::setSurface(ANativeWindow* window) {
	    ATRACE_CALL();

	    mNativeWindow = window;
	
	    if (mEglSurface != EGL_NO_SURFACE) {
	        mEglManager.destroySurface(mEglSurface);
	        mEglSurface = EGL_NO_SURFACE;
	    }
	
	    if (window) {
	        mEglSurface = mEglManager.createSurface(window);
	    }
	
	    if (mEglSurface != EGL_NO_SURFACE) {
	        const bool preserveBuffer = (mSwapBehavior != kSwap_discardBuffer);
	        mBufferPreserved = mEglManager.setPreserveBuffer(mEglSurface, preserveBuffer);
	        mHaveNewSurface = true;
	        makeCurrent();
	    } else {
	        mRenderThread.removeFrameCallback(this);
	    }
	}

创建OpenGL EGLSurface绘图表面，其实就是一个与ANativeWindow、或者说Surface对应的对象 ANativeWindow就是一个surface，EGLSurface就是对这两个封装，可以deque enque，

	EGLSurface EglManager::createSurface(EGLNativeWindowType window) {
	    initialize();
	    EGLSurface surface = eglCreateWindowSurface(mEglDisplay, mEglConfig, window, nullptr);
	    return surface;
	}



swapBuffers	
	
	void CanvasContext::swapBuffers(const SkRect& dirty, EGLint width, EGLint height) {
    if (CC_UNLIKELY(!mEglManager.swapBuffers(mEglSurface, dirty, width, height))) {
        setSurface(nullptr);
    }
    mHaveNewSurface = false;
	}
	
EGL如何处理


	
	bool EglManager::swapBuffers(EGLSurface surface, const SkRect& dirty,
	        EGLint width, EGLint height) {
	
	#if WAIT_FOR_GPU_COMPLETION
	    {
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
	#else
	    eglSwapBuffers(mEglDisplay, surface);
	#endif
	
	    EGLint err = eglGetError();
	    if (CC_LIKELY(err == EGL_SUCCESS)) {
	        return true;
	    }
	    if (err == EGL_BAD_SURFACE) {
	        // For some reason our surface was destroyed out from under us
	        // This really shouldn't happen, but if it does we can recover easily
	        // by just not trying to use the surface anymore
	        ALOGW("swapBuffers encountered EGL_BAD_SURFACE on %p, halting rendering...", surface);
	        return false;
	    }
	    LOG_ALWAYS_FATAL("Encountered EGL error %d %s during rendering",
	            err, egl_error_str(err));
	    // Impossible to hit this, but the compiler doesn't know that
	    return false;
	}


DisplayListData



设置了Layer的View，每个Layer要分配一个FBO，先渲染到FBO，再由OpenGL统一渲染到SF分配的内存，


ThreadProxy的Canvas就是CanvasContext


	// 创建的Canvas
	CREATE_BRIDGE4(createContext, RenderThread* thread, bool translucent,
	        RenderNode* rootRenderNode, IContextFactory* contextFactory) {
	    return new CanvasContext(*args->thread, args->translucent,
	            args->rootRenderNode, args->contextFactory);
	}


	Layer* CanvasContext::createTextureLayer() {
	    requireSurface();
	    return LayerRenderer::createTextureLayer(mRenderThread.renderState());
	}
	
创建createTextureLayer	
	
	Layer* LayerRenderer::createTextureLayer(RenderState& renderState) {
	    LAYER_RENDERER_LOGD("Creating new texture layer");
	
	    Layer* layer = new Layer(Layer::kType_Texture, renderState, 0, 0);
	    layer->setCacheable(false);
	    layer->layer.set(0.0f, 0.0f, 0.0f, 0.0f);
	    layer->texCoords.set(0.0f, 1.0f, 1.0f, 0.0f);
	    layer->region.clear();
	    layer->setRenderTarget(GL_NONE); // see ::updateTextureLayer()
	
	    Caches::getInstance().textureState().activateTexture(0);
	    layer->generateTexture();
	
	    return layer;
	}

	void Layer::generateTexture() {
	    if (!texture.id) {
	        glGenTextures(1, &texture.id);
	    }
	}

可以看到最终还是要OpengGL API 创建。LayerRenderer类的静态成员函数createTextureLayer首先是创建一个Layer对象，然后再调用该Layer对象的成员函数generateTexture生成一个Open GL纹理。从这里就可以看到，TextureView是通过Open GL纹理来实现的。

这一步执行完成之后，就可以得到一个Texture Layer了。回到TextureView类的成员函数getHardwareLayer中，接下来是判断成员变量mUpdateSurface的值是否等于false。如果等于false，就需要创建一个SurfaceTexture，并且这个SurfaceTexture会被设置到前面创建的Texture Layer中去。这样以后Texture Layer就可以通过该SurfaceTexture来获得Open GL纹理的内容。

当TextureView获得了新内容更新通知之后，一方面是调用TextureView类的成员函数updateLayer来标记Open GL纹理需要更新，另一方面是调用从父类继承下来的成员函数invalidate通知Main Thread需要重绘TextureView的UI。

TextureView类的成员函数updateLayer的实现如下所示：

	public class TextureView extends View {  
	    ......  
	  
	    private void updateLayer() {  
	        synchronized (mLock) {  
	            mUpdateLayer = true;  
	        }  
	    }  
	  
	    ......  
	} 

	static void android_view_HardwareLayer_updateSurfaceTexture(JNIEnv* env, jobject clazz,  
	        jlong layerUpdaterPtr) {  
	    DeferredLayerUpdater* layer = reinterpret_cast<DeferredLayerUpdater*>(layerUpdaterPtr);  
	    layer->updateTexImage();  
	}  
	
	
		class DeferredLayerUpdater : public VirtualLightRefBase {  
	public:  
	    ......  
	  
	    ANDROID_API void updateTexImage() {  
	        mUpdateTexImage = true;  
	    }  
	  
	    ......  
	};  
		
从这里可以看到，DeferredLayerUpdater类的成员函数updateTexImage并没有真正去更新当前正在处理的TextureView的Open GL纹理，而只是将DeferredLayerUpdater类的成员变量mUpdateTexImage设置为true，用来表示当前正在处理的TextureView的Open GL纹理需要进行更新。之所以要这样做，是因为纹理的更新要在Render Thread进行，而现在是在Main Thread执行。等到后面应用程序窗口的Display List被渲染时，TextureView的Open GL纹理才会被真正的更新。

DisplayListRenderer类的成员函数drawLayer的实现如下所示：

status_t DisplayListRenderer::drawLayer(Layer* layer, float x, float y) {  
    layer = refLayer(layer);  
    addDrawOp(new (alloc()) DrawLayerOp(layer, x, y));  
    return DrawGlInfo::kStatusDone;  
}  
这个函数定义在文件frameworks/base/libs/hwui/DisplayListRenderer.cpp中。

DisplayListRenderer类的成员函数drawLayer首先是调用另外一个成员函数refLayer将参数layer描述的一个Layer对象保存内部维护的一个Display List Data的一个Layer列表中，并且增加该Layer对象的引用计数，以便接下来将该Layer对象封装成一个Draw Layer Op，并且调用我们前面已经分析过的成员函数addDrawOp该Draw Layer Op记录在Display List Data中。

DisplayListRenderer类的成员函数refLayer的实现如下所示：

class ANDROID_API DisplayListRenderer: public StatefulBaseRenderer {  
    ......  
  
    inline Layer* refLayer(Layer* layer) {  
        mDisplayListData->layers.add(layer);  
        mCaches.resourceCache.incrementRefcount(layer);  
        return layer;  
    }  
  
    ......  
}; 
这个函数定义在文件frameworks/base/libs/hwui/DisplayListRenderer.h中。

从这里就可以看到，DisplayListRenderer内部维护的一个Display List Data有一个成员变量layers，它指向的是一个列表，用来保存那些以Layer形式绘制的视图。同时从这里也可以看到，增加Layer对象的引用计数是通过调用成员变量mCaches指向的一个Caches对象的成员变量resourceCache描述的一个ResourceCache对象的成员函数incrementRefcount完成的。

这一步执行完成之后，TextureView的Display List就构建完毕，这个过程实际上就是将一个DrawLayerOp记录在TextureView的Display List中，而该DrawLayerOp封装了一个Layer对象，该Layer对象通过Open Gl纹理描述了TextureView的UI。

需要做的处理就是从与TextureView关联的SurfaceTexture中读出下一个可用的图形缓冲区，并且将该图形缓冲区封装成一个Open GL纹理。这是通过调用DrawFrameTask类的成员变量mContext指向的一个CanvasContext对象的成员函数processLayerUpdate来实现的。

CanvasContext类的成员函数processLayerUpdate的实现如下所示：

void CanvasContext::processLayerUpdate(DeferredLayerUpdater* layerUpdater) {  
    bool success = layerUpdater->apply();  
    ......  
}  
这个函数定义在文件frameworks/base/libs/hwui/renderthread/CanvasContext.cpp中。

CanvasContext类的成员函数processLayerUpdate主要是调用参数layerUpdater描述的一个DeferredLayerUpdater对象的成员函数apply读出下一个可用的图形缓冲区，并且将该图形缓冲区封装成一个Open GL纹理，以便后面可以对它进行渲染。

DeferredLayerUpdater类的成员函数apply的实现如下所示：

bool DeferredLayerUpdater::apply() {  
    bool success = true;  
    ......  
  
    if (mSurfaceTexture.get()) {  
        ......  
        if (mUpdateTexImage) {  
            mUpdateTexImage = false;  
            doUpdateTexImage();  
        }  
        ......  
    }  
    return success;  
}  
这个函数定义在文件frameworks/base/libs/hwui/DeferredLayerUpdater.cpp中。

DeferredLayerUpdater类的成员变量mSurfaceTexture指向的一个是GLConsumer对象。这个GLConsumer对象用来描述与当前正在处理的DeferredLayerUpdater对象关联的TextureView对象所使用的一个SurfaceTexture对象的读端。也就是说，通过这个GLConsumer对象可以将关联的TextureView对象的下一个可用的图形缓冲区读取出来。

从前面Android应用程序UI硬件加速渲染的Display List构建过程分析一文可以知道，当一个TextureView有可用的图形缓冲区时，与它关联的DeferredLayerUpdater对象的成员变量mUpdateTexImage值会被设置为true。这时候如果当前正在处理的DeferredLayerUpdater对象的成员变量mSurfaceTexture指向了一个GLConsumer对象，那么现在就是时候去读取可用的图形缓冲区了。这是通过调用DeferredLayerUpdater类的成员函数doUpdateTexImage来实现的。

DeferredLayerUpdater类的成员函数doUpdateTexImage的实现如下所示：

void DeferredLayerUpdater::doUpdateTexImage() {  
    if (mSurfaceTexture->updateTexImage() == NO_ERROR) {  
        ......  
  
        GLenum renderTarget = mSurfaceTexture->getCurrentTextureTarget();  
  
        LayerRenderer::updateTextureLayer(mLayer, mWidth, mHeight,  
                !mBlend, forceFilter, renderTarget, transform);  
    }  
}  
这个函数定义在文件frameworks/base/libs/hwui/DeferredLayerUpdater.cpp中。

DeferredLayerUpdater类的成员函数doUpdateTexImage调用成员变量mSurfaceTexture指向的一个GLConsumer对象的成员函数updateTexImage读出可用的图形缓冲区，并且将该图形缓冲区封装成一个Open GL纹理。这个Open GL纹理可以通过调用上述的GLConsumer对象的成员函数getCurrentTextureTarget获得了。

接下来DeferredLayerUpdater类的成员函数doUpdateTexImage调用LayerRenderer类的静态成员函数updateTextureLayer将获得的Open GL纹理关联给成员变量mLayer描述的一个Layer对象。

LayerRenderer类的静态成员函数updateTextureLayer的实现如下所示：

void LayerRenderer::updateTextureLayer(Layer* layer, uint32_t width, uint32_t height,  
        bool isOpaque, bool forceFilter, GLenum renderTarget, float* textureTransform) {  
    if (layer) {  
        ......  
  
        if (renderTarget != layer->getRenderTarget()) {  
            layer->setRenderTarget(renderTarget);  
            ......  
        }  
    }  
} 
这个函数定义在文件frameworks/base/libs/hwui/LayerRenderer.cpp中。

LayerRenderer类的静态成员函数updateTextureLayer主要就是将参数renderTarget描述的Open GL纹理设置给参数layer描述的Layer对象。这是通过调用Layer类的成员函数setRenderTarget实现的。一个Layer对象关联了Open GL纹理之后，以后就可以进行渲染了。

这一步执行完成之后，如果应用程序窗口存在需要更新的TextureView，那么这些TextureView就更新完毕，也就是这些TextureView下一个可用的图形缓冲区已经被读出，并且封装成了Open GL纹理。回到前面分析的DrawFrameTask类的成员函数syncFrameState中，接下来要做的事情是将Main Thread维护的Display List等信息同步到Render Thread中。这是通过调用DrawFrameTask类的成员变量mContext指向的一个CanvasContext对象的成员函数prepareTree实现的。

CanvasContext对象的成员函数prepareTree执行完毕之后，会通过参数info描述的一个TreeInfo对象返回一些同步结果：
1. 当这个TreeInfo对象的成员变量out指向的一个Out对象的成员变量hasAnimations等于true时，表示应用程序窗口存在未完成的动画。如果这些未完成的动画至少存在一个是非异步动画时，上述Out对象的成员变量requiresUiRedraw的值就会被设置为true。这时候DrawFrameTask类的成员变量mSyncResult的kSync_UIRedrawRequired位就会被设置为1。所谓非异步动画，就是那些在执行过程可以停止的动画。这个停止执行的逻辑是由Main Thread执行的，例如，Main Thread可以响应用户输入停止执行一个非异步动画。从前面分析可以知道，DrawFrameTask类的成员变量mSyncResult的值最后将会返回给Java层的ThreadedRenderer类的成员函数draw。ThreadedRenderer类的成员函数draw一旦发现该值的kSync_UIRedrawRequired位被设置为1，那么就会向Main Thread的消息队列发送一个INVALIDATE消息，以便在处理这个INVALIDATE消息的时候，可以响应停止执行非异步动画的请求。
2. 当这个TreeInfo对象的成员变量prepareTextures的值等于true时，表示应用程序窗口的Display List引用到的Bitmap均已作为Open GL纹理上传到了GPU。这意味着应用程序窗口的Display List引用到的Bitmap已全部同步完成。在这种情况下，Render Thread在渲染下一帧之前，就可以唤醒Main Thread。另一方面，如果上述TreeInfo对象的成员变量prepareTextures的值等于false，就意味着应用程序窗口的Display List引用到的某些Bitmap不能成功地作为Open GL纹理上传到GPU，这时候Render Thread在渲染下一帧之后，才可以唤醒Main Thread，防止这些未能作为Open GL纹理上传到GPU的Bitmap一边被Render Thread渲染，一边又被Main Thread修改。那么什么时候应用程序窗口的Display List引用到的Bitmap会不能成功地作为Open GL纹理上传到GPU呢？一个应用程序进程可以创建的Open GL纹理是有大小限制的，如果超出这个限制，那么就会导至某些Bitmap不能作为Open GL纹理上传到GPU。

看看啊如何更新layer


	
	
	// 更新Layer
	void DeferredLayerUpdater::doUpdateTexImage() {
	    if (mSurfaceTexture->updateTexImage() == NO_ERROR) {
	        float transform[16];
	
	        int64_t frameNumber = mSurfaceTexture->getFrameNumber();
	        // If the GLConsumer queue is in synchronous mode, need to discard all
	        // but latest frame, using the frame number to tell when we no longer
	        // have newer frames to target. Since we can't tell which mode it is in,
	        // do this unconditionally.
	
	
	        int dropCounter = 0;
	        while (mSurfaceTexture->updateTexImage() == NO_ERROR) {
	            int64_t newFrameNumber = mSurfaceTexture->getFrameNumber();
	            // 
	            if (newFrameNumber == frameNumber) break;
	            frameNumber = newFrameNumber;
	            dropCounter++;
	        }
	
	        bool forceFilter = false;
	        // 获取
	        sp<GraphicBuffer> buffer = mSurfaceTexture->getCurrentBuffer();
	        if (buffer != nullptr) {
	            // force filtration if buffer size != layer size
	            forceFilter = mWidth != static_cast<int>(buffer->getWidth())
	                    || mHeight != static_cast<int>(buffer->getHeight());
	        }
	
	        #if DEBUG_RENDERER
	        if (dropCounter > 0) {
	            RENDERER_LOGD("Dropped %d frames on texture layer update", dropCounter);
	        }
	        #endif
	        mSurfaceTexture->getTransformMatrix(transform);
	        GLenum renderTarget = mSurfaceTexture->getCurrentTextureTarget();
	
	        LOG_ALWAYS_FATAL_IF(renderTarget != GL_TEXTURE_2D && renderTarget != GL_TEXTURE_EXTERNAL_OES,
	                "doUpdateTexImage target %x, 2d %x, EXT %x",
	                renderTarget, GL_TEXTURE_2D, GL_TEXTURE_EXTERNAL_OES);
	        LayerRenderer::updateTextureLayer(mLayer, mWidth, mHeight,
	                !mBlend, forceFilter, renderTarget, transform);
	    }
	}
	
GLConsumer如何获取当前buffer，最新的吗？		
		
		sp<GraphicBuffer> GLConsumer::getCurrentBuffer() const {
    Mutex::Autolock lock(mMutex);
    return (mCurrentTextureImage == NULL) ?
            NULL : mCurrentTextureImage->graphicBuffer();
}



status_t GLConsumer::updateTexImage() {
    ATRACE_CALL();
    GLC_LOGV("updateTexImage");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        GLC_LOGE("updateTexImage: GLConsumer is abandoned!");
        return NO_INIT;
    }

    // Make sure the EGL state is the same as in previous calls.
    status_t err = checkAndUpdateEglStateLocked();
    if (err != NO_ERROR) {
        return err;
    }

    BufferItem item;

    // Acquire the next buffer.
    // In asynchronous mode the list is guaranteed to be one buffer
    // deep, while in synchronous mode we use the oldest buffer.
    err = acquireBufferLocked(&item, 0);
    if (err != NO_ERROR) {
        if (err == BufferQueue::NO_BUFFER_AVAILABLE) {
            // We always bind the texture even if we don't update its contents.
            GLC_LOGV("updateTexImage: no buffers were available");
            //glBindTexture 渲染的 Texture
            glBindTexture(mTexTarget, mTexName);
            err = NO_ERROR;
        } else {
            GLC_LOGE("updateTexImage: acquire failed: %s (%d)",
                strerror(-err), err);
        }
        return err;
    }

    // Release the previous buffer.
    err = updateAndReleaseLocked(item);
    if (err != NO_ERROR) {
        // We always bind the texture.
        glBindTexture(mTexTarget, mTexName);
        return err;
    }

    // Bind the new buffer to the GL texture, and wait until it's ready.
    return bindTextureImageLocked();
}



status_t GLConsumer::updateAndReleaseLocked(const BufferItem& item)
{
    status_t err = NO_ERROR;

    int buf = item.mBuf;

    if (!mAttached) {
        GLC_LOGE("updateAndRelease: GLConsumer is not attached to an OpenGL "
                "ES context");
        releaseBufferLocked(buf, mSlots[buf].mGraphicBuffer,
                mEglDisplay, EGL_NO_SYNC_KHR);
        return INVALID_OPERATION;
    }

    // Confirm state.
    err = checkAndUpdateEglStateLocked();
    if (err != NO_ERROR) {
        releaseBufferLocked(buf, mSlots[buf].mGraphicBuffer,
                mEglDisplay, EGL_NO_SYNC_KHR);
        return err;
    }

    // Ensure we have a valid EglImageKHR for the slot, creating an EglImage
    // if nessessary, for the gralloc buffer currently in the slot in
    // ConsumerBase.
    // We may have to do this even when item.mGraphicBuffer == NULL (which
    // means the buffer was previously acquired).
    err = mEglSlots[buf].mEglImage->createIfNeeded(mEglDisplay, item.mCrop);
    if (err != NO_ERROR) {
        GLC_LOGW("updateAndRelease: unable to createImage on display=%p slot=%d",
                mEglDisplay, buf);
        releaseBufferLocked(buf, mSlots[buf].mGraphicBuffer,
                mEglDisplay, EGL_NO_SYNC_KHR);
        return UNKNOWN_ERROR;
    }

    // Do whatever sync ops we need to do before releasing the old slot.
    err = syncForReleaseLocked(mEglDisplay);
    if (err != NO_ERROR) {
        // Release the buffer we just acquired.  It's not safe to
        // release the old buffer, so instead we just drop the new frame.
        // As we are still under lock since acquireBuffer, it is safe to
        // release by slot.
        releaseBufferLocked(buf, mSlots[buf].mGraphicBuffer,
                mEglDisplay, EGL_NO_SYNC_KHR);
        return err;
    }

    GLC_LOGV("updateAndRelease: (slot=%d buf=%p) -> (slot=%d buf=%p)",
            mCurrentTexture, mCurrentTextureImage != NULL ?
                    mCurrentTextureImage->graphicBufferHandle() : 0,
            buf, mSlots[buf].mGraphicBuffer->handle);

    // release old buffer
    if (mCurrentTexture != BufferQueue::INVALID_BUFFER_SLOT) {
        status_t status = releaseBufferLocked(
                mCurrentTexture, mCurrentTextureImage->graphicBuffer(),
                mEglDisplay, mEglSlots[mCurrentTexture].mEglFence);
        if (status < NO_ERROR) {
            GLC_LOGE("updateAndRelease: failed to release buffer: %s (%d)",
                   strerror(-status), status);
            err = status;
            // keep going, with error raised [?]
        }
    }

    // Update the GLConsumer state.
    mCurrentTexture = buf;
    mCurrentTextureImage = mEglSlots[buf].mEglImage;
    mCurrentCrop = item.mCrop;
    mCurrentTransform = item.mTransform;
    mCurrentScalingMode = item.mScalingMode;
    mCurrentTimestamp = item.mTimestamp;
    mCurrentFence = item.mFence;
    mCurrentFrameNumber = item.mFrameNumber;

    computeCurrentTransformMatrixLocked();

    return err;
}


EglSlot


GLConsumer::EglImage::EglImage(sp<GraphicBuffer> graphicBuffer) :
    mGraphicBuffer(graphicBuffer),
    mEglImage(EGL_NO_IMAGE_KHR),
    mEglDisplay(EGL_NO_DISPLAY) {
}


EGLImageKHR GLConsumer::EglImage::createImage(EGLDisplay dpy,
        const sp<GraphicBuffer>& graphicBuffer, const Rect& crop) {
    EGLClientBuffer cbuf =
            static_cast<EGLClientBuffer>(graphicBuffer->getNativeBuffer());
    EGLint attrs[] = {
        EGL_IMAGE_PRESERVED_KHR,        EGL_TRUE,
        EGL_IMAGE_CROP_LEFT_ANDROID,    crop.left,
        EGL_IMAGE_CROP_TOP_ANDROID,     crop.top,
        EGL_IMAGE_CROP_RIGHT_ANDROID,   crop.right,
        EGL_IMAGE_CROP_BOTTOM_ANDROID,  crop.bottom,
        EGL_NONE,
    };
    if (!crop.isValid()) {
        // No crop rect to set, so terminate the attrib array before the crop.
        attrs[2] = EGL_NONE;
    } else if (!isEglImageCroppable(crop)) {
        // The crop rect is not at the origin, so we can't set the crop on the
        // EGLImage because that's not allowed by the EGL_ANDROID_image_crop
        // extension.  In the future we can add a layered extension that
        // removes this restriction if there is hardware that can support it.
        attrs[2] = EGL_NONE;
    }
    eglInitialize(dpy, 0, 0);
    EGLImageKHR image = eglCreateImageKHR(dpy, EGL_NO_CONTEXT,
            EGL_NATIVE_BUFFER_ANDROID, cbuf, attrs);
    if (image == EGL_NO_IMAGE_KHR) {
        EGLint error = eglGetError();
        ALOGE("error creating EGLImage: %#x", error);
        eglTerminate(dpy);
    }
    return image;
}


最终返回  native_buffer
	
	
	EGLImageKHR eglCreateImageKHR(EGLDisplay dpy, EGLContext ctx, EGLenum target,
	        EGLClientBuffer buffer, const EGLint* /*attrib_list*/)
	{
	    if (egl_display_t::is_valid(dpy) == EGL_FALSE) {
	        return setError(EGL_BAD_DISPLAY, EGL_NO_IMAGE_KHR);
	    }
	    if (ctx != EGL_NO_CONTEXT) {
	        return setError(EGL_BAD_CONTEXT, EGL_NO_IMAGE_KHR);
	    }
	    if (target != EGL_NATIVE_BUFFER_ANDROID) {
	        return setError(EGL_BAD_PARAMETER, EGL_NO_IMAGE_KHR);
	    }
	
	    ANativeWindowBuffer* native_buffer = (ANativeWindowBuffer*)buffer;
	
	    if (native_buffer->common.magic != ANDROID_NATIVE_BUFFER_MAGIC)
	        return setError(EGL_BAD_PARAMETER, EGL_NO_IMAGE_KHR);
	
	    if (native_buffer->common.version != sizeof(ANativeWindowBuffer))
	        return setError(EGL_BAD_PARAMETER, EGL_NO_IMAGE_KHR);
	
	    switch (native_buffer->format) {
	        case HAL_PIXEL_FORMAT_RGBA_8888:
	        case HAL_PIXEL_FORMAT_RGBX_8888:
	        case HAL_PIXEL_FORMAT_RGB_888:
	        case HAL_PIXEL_FORMAT_RGB_565:
	        case HAL_PIXEL_FORMAT_BGRA_8888:
	            break;
	        default:
	            return setError(EGL_BAD_PARAMETER, EGL_NO_IMAGE_KHR);
	    }
	
	    native_buffer->common.incRef(&native_buffer->common);
	    return (EGLImageKHR)native_buffer;
	}

可以说，是直接返回了一块内存，并没有怎么封装的引用

typedef void *EGLImageKHR; 

全你麻痹 typedef void *EGLImageKHR;

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