# SurfaceTexture是什么？

>引用的

SurfaceTexture 类是在 Android 3.0 中引入的。就像 SurfaceView 是 Surface 和 View 的结合一样，SurfaceTexture 是 Surface 和 GLES texture 的粗糙结合（有几个警告）。

当你创建了一个 SurfaceTexture，你就创建了你的应用作为消费者的 BufferQueue。当一个新的缓冲区由生产者入对时，你的应用将通过回调 (onFrameAvailable()) 被通知。你的应用调用 updateTexImage()，这将释放之前持有的缓冲区，并从队列中获取新的缓冲区，执行一些 EGL 调用以使缓冲区可作为一个外部 texture 由 GLES 使用。

# SurfaceTexture怎么用

SurfaceTexture 最核心的是它有一个HardwareLayer，这个HardwareLayer到底是什么，它主要是握有buffer，这个buffer的数据源自远端，比如摄像头，或者视频流。也就是数据源，通知更新的入口，底层调用入口：

    /**
     * This method is invoked from native code only.
     */
    // native找到该方法，通知更新数据
    @SuppressWarnings({"UnusedDeclaration"})
    private static void postEventFromNative(WeakReference<SurfaceTexture> weakSelf) {
        SurfaceTexture st = weakSelf.get();
        if (st != null) {
            Handler handler = st.mOnFrameAvailableHandler;
            if (handler != null) {
                handler.sendEmptyMessage(0);
            }
        }
    }

	void JNISurfaceTextureContext::onFrameAvailable(const BufferItem& /* item */)
	{
	    bool needsDetach = false;
	    JNIEnv* env = getJNIEnv(&needsDetach);
	    if (env != NULL) {
	        env->CallStaticVoidMethod(mClazz, fields.postEvent, mWeakThiz);
	    } else {
	        ALOGW("onFrameAvailable event will not posted");
	    }
	    if (needsDetach) {
	        detachJNI();
	    }
	}

如何通过SurfaceTure实现数据的流动

	static void android_hardware_Camera_setPreviewTexture(JNIEnv *env,
	        jobject thiz, jobject jSurfaceTexture)
	{
	    ALOGV("setPreviewTexture");
	    sp<Camera> camera = get_native_camera(env, thiz, NULL);
	    if (camera == 0) return;
	
	    sp<IGraphicBufferProducer> producer = NULL;
	    if (jSurfaceTexture != NULL) {
	        producer = SurfaceTexture_getProducer(env, jSurfaceTexture);
	        if (producer == NULL) {
	            jniThrowException(env, "java/lang/IllegalArgumentException",
	                    "SurfaceTexture already released in setPreviewTexture");
	            return;
	        }
	
	    }
	
	    if (camera->setPreviewTarget(producer) != NO_ERROR) {
	        jniThrowException(env, "java/io/IOException",
	                "setPreviewTexture failed");
	    }
	}
	
难道内从是从SurfaceTexture获取，传递给Camera共享来使用的吗？

	#define ANDROID_GRAPHICS_SURFACETEXTURE_JNI_ID "mSurfaceTexture"
	#define ANDROID_GRAPHICS_PRODUCER_JNI_ID "mProducer"
	#define ANDROID_GRAPHICS_FRAMEAVAILABLELISTENER_JNI_ID \
	                                         "mFrameAvailableListener"
	
	static void SurfaceTexture_classInit(JNIEnv* env, jclass clazz)
	{
		<!--1 -->
	    fields.surfaceTexture = env->GetFieldID(clazz,
	            ANDROID_GRAPHICS_SURFACETEXTURE_JNI_ID, "J");
		<!--2 -->
	    fields.producer = env->GetFieldID(clazz,
	            ANDROID_GRAPHICS_PRODUCER_JNI_ID, "J");
		<!--3 -->
	    fields.frameAvailableListener = env->GetFieldID(clazz,
	            ANDROID_GRAPHICS_FRAMEAVAILABLELISTENER_JNI_ID, "J");
		<!--4 -->
	    fields.postEvent = env->GetStaticMethodID(clazz, "postEventFromNative",
	            "(Ljava/lang/ref/WeakReference;)V");

	}
	
SurfaceTexture在初始化的时候，就已经获取producer

	
	static void SurfaceTexture_init(JNIEnv* env, jobject thiz, jboolean isDetached,
	        jint texName, jboolean singleBufferMode, jobject weakThiz)
	{
		<!--producer-->
	    sp<IGraphicBufferProducer> producer;
	    <!--consumer-->
	    sp<IGraphicBufferConsumer> consumer;
	    <!--初始化 创建 -->
	    BufferQueue::createBufferQueue(&producer, &consumer);
	
	    if (singleBufferMode) {
	        consumer->setMaxBufferCount(1);
	    }
	
	    sp<GLConsumer> surfaceTexture;
	    if (isDetached) {
	        surfaceTexture = new GLConsumer(consumer, GL_TEXTURE_EXTERNAL_OES,
	                true, !singleBufferMode);
	    } else {
	        surfaceTexture = new GLConsumer(consumer, texName,
	                GL_TEXTURE_EXTERNAL_OES, true, !singleBufferMode);
	    }
	
	    if (surfaceTexture == 0) {
	        jniThrowException(env, OutOfResourcesException,
	                "Unable to create native SurfaceTexture");
	        return;
	    }
	    surfaceTexture->setName(String8::format("SurfaceTexture-%d-%d-%d",
	            (isDetached ? 0 : texName),
	            getpid(),
	            createProcessUniqueId()));
	
	    // If the current context is protected, inform the producer.
	    consumer->setConsumerIsProtected(isProtectedContext());
	
	    SurfaceTexture_setSurfaceTexture(env, thiz, surfaceTexture);
	    SurfaceTexture_setProducer(env, thiz, producer);
	
	    jclass clazz = env->GetObjectClass(thiz);
	    if (clazz == NULL) {
	        jniThrowRuntimeException(env,
	                "Can't find android/graphics/SurfaceTexture");
	        return;
	    }
	
	    sp<JNISurfaceTextureContext> ctx(new JNISurfaceTextureContext(env, weakThiz,
	            clazz));
	    surfaceTexture->setFrameAvailableListener(ctx);
	    SurfaceTexture_setFrameAvailableListener(env, thiz, ctx);
	}
	
	
初始化
	
	void BufferQueue::createBufferQueue(sp<IGraphicBufferProducer>* outProducer,
	        sp<IGraphicBufferConsumer>* outConsumer,
	        bool consumerIsSurfaceFlinger) {
		<!--core-->
	    sp<BufferQueueCore> core(new BufferQueueCore());
		<!--producer-->
	    sp<IGraphicBufferProducer> producer(new BufferQueueProducer(core, consumerIsSurfaceFlinger));
		<!--consumer-->
	    sp<IGraphicBufferConsumer> consumer(new BufferQueueConsumer(core));
	    
	    *outProducer = producer;
	    *outConsumer = consumer;
	}

    static void createBufferQueue(sp<IGraphicBufferProducer>* outProducer,
            sp<IGraphicBufferConsumer>* outConsumer,
            bool consumerIsSurfaceFlinger = false);
 
默认情况下，consumerIsSurfaceFlinger=false，也就是普通APP也能自己申请内存，但是这部分内存如何给SF呢？
 

# 集合TextrueView看数据流向图(SurfaceTexture如何知道新的数据帧到来，如何通知 又是如何显示的）

TextrueView自己就有一个SurfaceTexture，并且自己实现了监听与刷新

    private final SurfaceTexture.OnFrameAvailableListener mUpdateListener =
            new SurfaceTexture.OnFrameAvailableListener() {
        @Override
        public void onFrameAvailable(SurfaceTexture surfaceTexture) {
            updateLayer();
            invalidate();
        }
    };
    
等到SurfaceTexture的数据有更新，需要重绘只，就取出数据，重绘

    @Override
    public final void draw(Canvas canvas) {
        // NOTE: Maintain this carefully (see View#draw)
        mPrivateFlags = (mPrivateFlags & ~PFLAG_DIRTY_MASK) | PFLAG_DRAWN;

        /* Simplify drawing to guarantee the layer is the only thing drawn - so e.g. no background,
        scrolling, or fading edges. This guarantees all drawing is in the layer, so drawing
        properties (alpha, layer paint) affect all of the content of a TextureView. */
		 <!--关键点1 ，必须支持硬件加速，才能用-->
        if (canvas.isHardwareAccelerated()) {
            DisplayListCanvas displayListCanvas = (DisplayListCanvas) canvas;

            HardwareLayer layer = getHardwareLayer();
            if (layer != null) {
                applyUpdate();
                applyTransformMatrix();
                mLayer.setLayerPaint(mLayerPaint); // ensure layer paint is up to date
                displayListCanvas.drawHardwareLayer(layer);
            }
        }
    }
    
从上面的draw可以看出，必须支持硬件加速，才能用TextrueView，  DisplayListCanvas的drawHardwareLayer有什么不同吗？**其实跟普通的View硬件加速绘制没什么区别，就是构建一个DrawOp，并放到待绘制List**，等待渲染。

	   void drawHardwareLayer(HardwareLayer layer) {
	        nDrawLayer(mNativeCanvasWrapper, layer.getLayerHandle());
	    }
 
	 static void android_view_DisplayListCanvas_drawLayer(jlong canvasPtr, jlong layerPtr) {
	    Canvas* canvas = reinterpret_cast<Canvas*>(canvasPtr);
	    DeferredLayerUpdater* layer = reinterpret_cast<DeferredLayerUpdater*>(layerPtr);
	    canvas->drawLayer(layer);
	}

	Canvas* Canvas::create_recording_canvas(int width, int height, uirenderer::RenderNode* renderNode) {
	    if (uirenderer::Properties::isSkiaEnabled()) {
	        return new uirenderer::skiapipeline::SkiaRecordingCanvas(renderNode, width, height);
	    }
	    return new uirenderer::RecordingCanvas(width, height);
	}

	void RecordingCanvas::drawLayer(DeferredLayerUpdater* layerHandle) {
	    // We ref the DeferredLayerUpdater due to its thread-safe ref-counting semantics.
	    mDisplayList->ref(layerHandle);
	
	    LOG_ALWAYS_FATAL_IF(layerHandle->getBackingLayerApi() != Layer::Api::OpenGL);
	    // Note that the backing layer has *not* yet been updated, so don't trust
	    // its width, height, transform, etc...!
	    addOp(alloc().create_trivial<TextureLayerOp>(
	            Rect(layerHandle->getWidth(), layerHandle->getHeight()),
	            *(mState.currentSnapshot()->transform),
	            getRecordedClip(), layerHandle));
	}

	void FrameBuilder::deferTextureLayerOp(const TextureLayerOp& op) {
	    GlLayer* layer = static_cast<GlLayer*>(op.layerHandle->backingLayer());
	    if (CC_UNLIKELY(!layer || !layer->isRenderable())) return;
	
	    const TextureLayerOp* textureLayerOp = &op;
	    // Now safe to access transform (which was potentially unready at record time)
	    if (!layer->getTransform().isIdentity()) {
	        Matrix4 combinedMatrix(op.localMatrix);
	        combinedMatrix.multiply(layer->getTransform());
	        textureLayerOp = mAllocator.create<TextureLayerOp>(op, combinedMatrix);
	    }
	    BakedOpState* bakedState = tryBakeOpState(*textureLayerOp);
	
	    if (!bakedState) return; // quick rejected
	    currentLayer().deferUnmergeableOp(mAllocator, bakedState, OpBatchType::TextureLayer);
	}


 创建TextureLayerOp，并且，将layerHandle传递进去，这里的layerHandle其实是一个DeferredLayerUpdater指针
 
	 struct TextureLayerOp : RecordedOp {
	    TextureLayerOp(BASE_PARAMS_PAINTLESS, DeferredLayerUpdater* layer)
	            : SUPER_PAINTLESS(TextureLayerOp)
	            , layerHandle(layer) {}
	
	    // Copy an existing TextureLayerOp, replacing the underlying matrix
	    TextureLayerOp(const TextureLayerOp& op, const Matrix4& replacementMatrix)
	            : RecordedOp(RecordedOpId::TextureLayerOp, op.unmappedBounds, replacementMatrix,
	                    op.localClip, op.paint)
	            , layerHandle(op.layerHandle) {
	
	    }
	    DeferredLayerUpdater* layerHandle;
	};

 
 这里有个HardWareLayer的概念，TextrueView如何获取HardwareLayer，它到底是什么？
 
    HardwareLayer getHardwareLayer() {
        if (mLayer == null) {
            if (mAttachInfo == null || mAttachInfo.mThreadedRenderer == null) {
                return null;
            }

			 <!--直接通过ThreadedRenderer构建一个Layer,主线程 mThreadedRenderer--> 
            mLayer = mAttachInfo.mThreadedRenderer.createTextureLayer();
            boolean createNewSurface = (mSurface == null);
            <!--SurfaceTexture的构建-->
            if (createNewSurface) {
                // Create a new SurfaceTexture for the layer.
                mSurface = new SurfaceTexture(false);
                nCreateNativeWindow(mSurface);
            }
            <!--为Layer设置数据源-->
            mLayer.setSurfaceTexture(mSurface);
            mSurface.setDefaultBufferSize(getWidth(), getHeight());
            <!--设置数据源更新回调-->
            mSurface.setOnFrameAvailableListener(mUpdateListener, mAttachInfo.mHandler);				<!--SurfaceTexture可用的回调，通知上层，可以用了，数据可以传输过来了-->
            if (mListener != null && createNewSurface) {
                mListener.onSurfaceTextureAvailable(mSurface, getWidth(), getHeight());
            }
            <!--设置回执paint-->
            mLayer.setLayerPaint(mLayerPaint);
        }

        if (mUpdateSurface) {
            // Someone has requested that we use a specific SurfaceTexture, so
            // tell mLayer about it and set the SurfaceTexture to use the
            // current view size.
            mUpdateSurface = false;

            // Since we are updating the layer, force an update to ensure its
            // parameters are correct (width, height, transform, etc.)
            updateLayer();
            mMatrixChanged = true;
            mLayer.setSurfaceTexture(mSurface);
            mSurface.setDefaultBufferSize(getWidth(), getHeight());
        }

        return mLayer;
    }
       
 怎么创建Layer，Layer到底是什么
       
     /**
     * Creates a new hardware layer. A hardware layer built by calling this
     * method will be treated as a texture layer, instead of as a render target.
     *
     * @return A hardware layer
     */
    HardwareLayer createTextureLayer() {
        long layer = nCreateTextureLayer(mNativeProxy);
        return HardwareLayer.adoptTextureLayer(this, layer);
    }
    
 实质是一个DeferredLayerUpdater
    
	 static jlong android_view_ThreadedRenderer_createTextureLayer(JNIEnv* env, jobject clazz,
	        jlong proxyPtr) {
	    RenderProxy* proxy = reinterpret_cast<RenderProxy*>(proxyPtr);
	    DeferredLayerUpdater* layer = proxy->createTextureLayer();
	    return reinterpret_cast<jlong>(layer);
	}
 
 DeferredLayerUpdater是什么里面有什么？CanvasContext的createTextureLayer
	 
	 CREATE_BRIDGE1(createTextureLayer, CanvasContext* context) {
	    return args->context->createTextureLayer();
	}

调用OpenGLPipeline对应API创建一个TextureLayer

	 DeferredLayerUpdater* OpenGLPipeline::createTextureLayer() {
	    mEglManager.initialize();
	    return new DeferredLayerUpdater(mRenderThread.renderState(), createLayer, Layer::Api::OpenGL);
	}

 之后被，包装一层组成HardwareLayer
   
    static HardwareLayer adoptTextureLayer(ThreadedRenderer renderer, long layer) {
        return new HardwareLayer(renderer, layer);
    }   
    
    
封装完毕，看看后面更新怎么用的displayListCanvas.drawHardwareLayer(layer)，这个怎么调用呢？

	static void android_view_DisplayListCanvas_drawLayer(jlong canvasPtr, jlong layerPtr) {
	    Canvas* canvas = reinterpret_cast<Canvas*>(canvasPtr);
	    DeferredLayerUpdater* layer = reinterpret_cast<DeferredLayerUpdater*>(layerPtr);
	    canvas->drawLayer(layer);
	}

创建Layer的Function

	static Layer* createLayer(RenderState& renderState, uint32_t layerWidth, uint32_t layerHeight,
	        SkColorFilter* colorFilter, int alpha, SkBlendMode mode, bool blend) {
	    GlLayer* layer = new GlLayer(renderState, layerWidth, layerHeight, colorFilter, alpha,
	            mode, blend);
	    Caches::getInstance().textureState().activateTexture(0);
	    layer->generateTexture();
	    return layer;
	}

	void GlLayer::generateTexture() {
	    if (!texture.mId) {
	        glGenTextures(1, &texture.mId);
	    }
	}

SurfaceTexture的数据，怎么直接传递给给了App的buffer呢,


# 为什么TetureView比SurfaceView占用内存

# SurfaceView的硬件加速跟软件绘制

视频播放应该是数据直接填充到SurfaceView的那块内存

# Surface的内存分配与数据流 还是只看6.0

Surface都是归SF管理，所有的分配最后都会走到SF，一个Surface有一个BufferQueue，一个Queue有多个slot，    

	BufferQueueDefs::SlotsType mSlots;

producer跟consumer都会映射这个slots，一个surface有一块内存，这块内存有很多歌slot 32 或者64 

    
不过SurfaceView传说的前后双缓冲是怎么回事？    不同的版本不同，看6.0跟8.0差别很大，只看6.0

>surface.cpp中的slots

    // mSlots stores the buffers that have been allocated for each buffer slot.
    // It is initialized to null pointers, and gets filled in with the result of
    // IGraphicBufferProducer::requestBuffer when the client dequeues a buffer from a
    // slot that has not yet been used. The buffer allocated to a slot will also
    // be replaced if the requested buffer usage or geometry differs from that
    // of the buffer allocated to a slot.
    
    BufferSlot mSlots[NUM_BUFFER_SLOTS];

>BufferQueueCore.cpp中的slots

    // mSlots is an array of buffer slots that must be mirrored on the producer
    // side. This allows buffer ownership to be transferred between the producer
    // and consumer without sending a GraphicBuffer over Binder. The entire
    // array is initialized to NULL at construction time, and buffers are
    // allocated for a slot when requestBuffer is called with that slot's index.

    BufferQueueDefs::SlotsType mSlots;
    
        namespace BufferQueueDefs {
        // BufferQueue will keep track of at most this value of buffers.
        // Attempts at runtime to increase the number of buffers past this
        // will fail.
        enum { NUM_BUFFER_SLOTS = 64 };
        typedef BufferSlot SlotsType[NUM_BUFFER_SLOTS];
    } // namespace BufferQueueDefs
    
    
![31530101142_.pic.jpg](https://upload-images.jianshu.io/upload_images/1460468-617b3362ee32a84a.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

一个Surface对应内存块最多64块内存，如何管理，如何申请一个slot


	
	status_t BufferQueueProducer::dequeueBuffer(int *outSlot,
	        sp<android::Fence> *outFence, bool async,
	        uint32_t width, uint32_t height, PixelFormat format, uint32_t usage) {
 
	    
	    status_t returnFlags = NO_ERROR;
	    EGLDisplay eglDisplay = EGL_NO_DISPLAY;
	    EGLSyncKHR eglFence = EGL_NO_SYNC_KHR;
	    bool attachedByConsumer = false;
	
	    { 
	    
	    	// 保护
	        Mutex::Autolock lock(mCore->mMutex);
	        mCore->waitWhileAllocatingLocked();
		        if (format == 0) {
	            format = mCore->mDefaultBufferFormat;
	        }
	        // Enable the usage bits the consumer requested
	        usage |= mCore->mConsumerUsageBits;
	        
			<!--是否需要使用默认尺寸-->
	        const bool useDefaultSize = !width && !height;
	        if (useDefaultSize) {
	            width = mCore->mDefaultWidth;
	            height = mCore->mDefaultHeight;
	        }
			<!--找SLOT-->
	        int found = BufferItem::INVALID_BUFFER_SLOT;
	        while (found == BufferItem::INVALID_BUFFER_SLOT) {
	            status_t status = waitForFreeSlotThenRelock("dequeueBuffer", async,
	                    &found, &returnFlags);
	            if (status != NO_ERROR) {
	                return status;
	            }
	
	            // This should not happen
	            if (found == BufferQueueCore::INVALID_BUFFER_SLOT) {
	                BQ_LOGE("dequeueBuffer: no available buffer slots");
	                return -EBUSY;
	            }
	
	            const sp<GraphicBuffer>& buffer(mSlots[found].mGraphicBuffer);
	
	            // If we are not allowed to allocate new buffers,
	            // waitForFreeSlotThenRelock must have returned a slot containing a
	            // buffer. If this buffer would require reallocation to meet the
	            // requested attributes, we free it and attempt to get another one.
	            if (!mCore->mAllowAllocation) {
	                if (buffer->needsReallocation(width, height, format, usage)) {
	                    mCore->freeBufferLocked(found);
	                    found = BufferItem::INVALID_BUFFER_SLOT;
	                    continue;
	                }
	            }
	        }
	
	        *outSlot = found;
	        ATRACE_BUFFER_INDEX(found);
	
	        attachedByConsumer = mSlots[found].mAttachedByConsumer;
	
	        mSlots[found].mBufferState = BufferSlot::DEQUEUED;
	
	        const sp<GraphicBuffer>& buffer(mSlots[found].mGraphicBuffer);
	        if ((buffer == NULL) ||
	                buffer->needsReallocation(width, height, format, usage))
	        {
	            mSlots[found].mAcquireCalled = false;
	            mSlots[found].mGraphicBuffer = NULL;
	            mSlots[found].mRequestBufferCalled = false;
	            mSlots[found].mEglDisplay = EGL_NO_DISPLAY;
	            mSlots[found].mEglFence = EGL_NO_SYNC_KHR;
	            mSlots[found].mFence = Fence::NO_FENCE;
	            mCore->mBufferAge = 0;
	
	            returnFlags |= BUFFER_NEEDS_REALLOCATION;
	        } else {
	            // We add 1 because that will be the frame number when this buffer
	            // is queued
	            mCore->mBufferAge =
	                    mCore->mFrameCounter + 1 - mSlots[found].mFrameNumber;
	        }
	
	        BQ_LOGV("dequeueBuffer: setting buffer age to %" PRIu64,
	                mCore->mBufferAge);
	
	        if (CC_UNLIKELY(mSlots[found].mFence == NULL)) {
	            BQ_LOGE("dequeueBuffer: about to return a NULL fence - "
	                    "slot=%d w=%d h=%d format=%u",
	                    found, buffer->width, buffer->height, buffer->format);
	        }
	
	        eglDisplay = mSlots[found].mEglDisplay;
	        eglFence = mSlots[found].mEglFence;
	        *outFence = mSlots[found].mFence;
	        mSlots[found].mEglFence = EGL_NO_SYNC_KHR;
	        mSlots[found].mFence = Fence::NO_FENCE;
	
	        mCore->validateConsistencyLocked();
	    } // Autolock scope
	
	    if (returnFlags & BUFFER_NEEDS_REALLOCATION) {
	        status_t error;
	        BQ_LOGV("dequeueBuffer: allocating a new buffer for slot %d", *outSlot);
	        sp<GraphicBuffer> graphicBuffer(mCore->mAllocator->createGraphicBuffer(
	                width, height, format, usage, &error));
	        if (graphicBuffer == NULL) {
	            BQ_LOGE("dequeueBuffer: createGraphicBuffer failed");
	            return error;
	        }
	
	        { // Autolock scope
	            Mutex::Autolock lock(mCore->mMutex);
	
	            if (mCore->mIsAbandoned) {
	                BQ_LOGE("dequeueBuffer: BufferQueue has been abandoned");
	                return NO_INIT;
	            }
	
	            graphicBuffer->setGenerationNumber(mCore->mGenerationNumber);
	            mSlots[*outSlot].mGraphicBuffer = graphicBuffer;
	        } // Autolock scope
	    }
	
	    if (attachedByConsumer) {
	        returnFlags |= BUFFER_NEEDS_REALLOCATION;
	    }
	
	    if (eglFence != EGL_NO_SYNC_KHR) {
	        EGLint result = eglClientWaitSyncKHR(eglDisplay, eglFence, 0,
	                1000000000);
	        // If something goes wrong, log the error, but return the buffer without
	        // synchronizing access to it. It's too late at this point to abort the
	        // dequeue operation.
	        if (result == EGL_FALSE) {
	            BQ_LOGE("dequeueBuffer: error %#x waiting for fence",
	                    eglGetError());
	        } else if (result == EGL_TIMEOUT_EXPIRED_KHR) {
	            BQ_LOGE("dequeueBuffer: timeout waiting for fence");
	        }
	        eglDestroySyncKHR(eglDisplay, eglFence);
	    }
	
	    BQ_LOGV("dequeueBuffer: returning slot=%d/%" PRIu64 " buf=%p flags=%#x",
	            *outSlot,
	            mSlots[*outSlot].mFrameNumber,
	            mSlots[*outSlot].mGraphicBuffer->handle, returnFlags);
	
	    return returnFlags;
	}



	
	status_t BufferQueueProducer::waitForFreeSlotThenRelock(const char* caller,
	        bool async, int* found, status_t* returnFlags) const {
	    bool tryAgain = true;
	    while (tryAgain) {
	        if (mCore->mIsAbandoned) {
	            BQ_LOGE("%s: BufferQueue has been abandoned", caller);
	            return NO_INIT;
	        }
	
	        const int maxBufferCount = mCore->getMaxBufferCountLocked(async);
	        if (async && mCore->mOverrideMaxBufferCount) {
	            // FIXME: Some drivers are manually setting the buffer count
	            // (which they shouldn't), so we do this extra test here to
	            // handle that case. This is TEMPORARY until we get this fixed.
	            if (mCore->mOverrideMaxBufferCount < maxBufferCount) {
	                BQ_LOGE("%s: async mode is invalid with buffer count override",
	                        caller);
	                return BAD_VALUE;
	            }
	        }
	
	        // Free up any buffers that are in slots beyond the max buffer count
	        for (int s = maxBufferCount; s < BufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
	            assert(mSlots[s].mBufferState == BufferSlot::FREE);
	            if (mSlots[s].mGraphicBuffer != NULL) {
	                mCore->freeBufferLocked(s);
	                *returnFlags |= RELEASE_ALL_BUFFERS;
	            }
	        }
	
	        int dequeuedCount = 0;
	        int acquiredCount = 0;
	        for (int s = 0; s < maxBufferCount; ++s) {
	            switch (mSlots[s].mBufferState) {
	                case BufferSlot::DEQUEUED:
	                    ++dequeuedCount;
	                    break;
	                case BufferSlot::ACQUIRED:
	                    ++acquiredCount;
	                    break;
	                default:
	                    break;
	            }
	        }
	
	        // Producers are not allowed to dequeue more than one buffer if they
	        // did not set a buffer count
	        if (!mCore->mOverrideMaxBufferCount && dequeuedCount) {
	            BQ_LOGE("%s: can't dequeue multiple buffers without setting the "
	                    "buffer count", caller);
	            return INVALID_OPERATION;
	        }
	
	        // See whether a buffer has been queued since the last
	        // setBufferCount so we know whether to perform the min undequeued
	        // buffers check below
	        if (mCore->mBufferHasBeenQueued) {
	            // Make sure the producer is not trying to dequeue more buffers
	            // than allowed
	            const int newUndequeuedCount =
	                maxBufferCount - (dequeuedCount + 1);
	            const int minUndequeuedCount =
	                mCore->getMinUndequeuedBufferCountLocked(async);
	            if (newUndequeuedCount < minUndequeuedCount) {
	                BQ_LOGE("%s: min undequeued buffer count (%d) exceeded "
	                        "(dequeued=%d undequeued=%d)",
	                        caller, minUndequeuedCount,
	                        dequeuedCount, newUndequeuedCount);
	                return INVALID_OPERATION;
	            }
	        }
	
	        *found = BufferQueueCore::INVALID_BUFFER_SLOT;
	
	        // If we disconnect and reconnect quickly, we can be in a state where
	        // our slots are empty but we have many buffers in the queue. This can
	        // cause us to run out of memory if we outrun the consumer. Wait here if
	        // it looks like we have too many buffers queued up.
	        bool tooManyBuffers = mCore->mQueue.size()
	                            > static_cast<size_t>(maxBufferCount);
	        if (tooManyBuffers) {
	            BQ_LOGV("%s: queue size is %zu, waiting", caller,
	                    mCore->mQueue.size());
	        } else {
	            if (!mCore->mFreeBuffers.empty()) {
	                auto slot = mCore->mFreeBuffers.begin();
	                *found = *slot;
	                mCore->mFreeBuffers.erase(slot);
	            } else if (mCore->mAllowAllocation && !mCore->mFreeSlots.empty()) {
	                auto slot = mCore->mFreeSlots.begin();
	                // Only return free slots up to the max buffer count
	                if (*slot < maxBufferCount) {
	                    *found = *slot;
	                    mCore->mFreeSlots.erase(slot);
	                }
	            }
	        }
	
	        // If no buffer is found, or if the queue has too many buffers
	        // outstanding, wait for a buffer to be acquired or released, or for the
	        // max buffer count to change.
	        tryAgain = (*found == BufferQueueCore::INVALID_BUFFER_SLOT) ||
	                   tooManyBuffers;
	        if (tryAgain) {
	            // Return an error if we're in non-blocking mode (producer and
	            // consumer are controlled by the application).
	            // However, the consumer is allowed to briefly acquire an extra
	            // buffer (which could cause us to have to wait here), which is
	            // okay, since it is only used to implement an atomic acquire +
	            // release (e.g., in GLConsumer::updateTexImage())
	            if (mCore->mDequeueBufferCannotBlock &&
	                    (acquiredCount <= mCore->mMaxAcquiredBufferCount)) {
	                return WOULD_BLOCK;
	            }
	            mCore->mDequeueCondition.wait(mCore->mMutex);
	        }
	    } // while (tryAgain)
	
	    return NO_ERROR;
	}

# BufferSlot跟mGraphicBuffer的关系

    BufferSlot()
    : mEglDisplay(EGL_NO_DISPLAY),
      mBufferState(BufferSlot::FREE),
      mRequestBufferCalled(false),
      mFrameNumber(0),
      mEglFence(EGL_NO_SYNC_KHR),
      mAcquireCalled(false),
      mNeedsCleanupOnRelease(false),
      mAttachedByConsumer(false) {
    }

    // mGraphicBuffer points to the buffer allocated for this slot or is NULL
    // if no buffer has been allocated.
    sp<GraphicBuffer> mGraphicBuffer;
    
    Graphics是哪块内存，算是本APP所处理的内存吗？但是它是native的内存吧，并且，好像不算到当前App中，不会导致OOM，除非系统内存不足，

    

# 为什么TetureView比SurfaceView占用内存

拿两个播放视频来对比下：CPU跟内存使用

>CPU对比

![cpu使用对比.png](https://upload-images.jianshu.io/upload_images/1460468-8f398182e3e1cddb.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

>内存使用对比

![内存使用对比.png](https://upload-images.jianshu.io/upload_images/1460468-adb477885b1c6814.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

TextureView播放视频同样需要Surface，在SurfaceTextureAvailable的时候，需要用SurfaceTexture创建Surface，之后再使用这个Surface：


    @Override
    public void onSurfaceTextureAvailable(SurfaceTexture surfaceTexture, int width, int height) {
        if (mSurfaceTexture == null) {
            mSurfaceTexture = surfaceTexture;
            mSurface = new Surface(surfaceTexture);
            //   这里是设置数据的输出流吗？
            mMediaPlayer.setSurface(mSurface);
            if (mTargetState == PlayState.PLAYING) {
                start();
            }
        } else {
            mTextureView.setSurfaceTexture(mSurfaceTexture);
        }
    }

那么究竟如何新建的呢new Surface(surfaceTexture)

    public Surface(SurfaceTexture surfaceTexture) {
        if (surfaceTexture == null) {
            throw new IllegalArgumentException("surfaceTexture must not be null");
        }
        mIsSingleBuffered = surfaceTexture.isSingleBuffered();
        synchronized (mLock) {
            mName = surfaceTexture.toString();
            setNativeObjectLocked(nativeCreateFromSurfaceTexture(surfaceTexture));
        }
    }
    
会调用native

	static jlong nativeCreateFromSurfaceTexture(JNIEnv* env, jclass clazz,
	        jobject surfaceTextureObj) {
	     
	     <!--获取SurfaceTexture中已经创建的GraphicBufferProducer-->
	    sp<IGraphicBufferProducer> producer(SurfaceTexture_getProducer(env, surfaceTextureObj));
 		 
	   <!--根据producer直接创建Surface，其实Surface只是为了表示数据从哪来，由谁填充，其实数据是由MediaPlayer填充的，只是这里的Surface不是归属SurfaceFlinger管理，SurfaceFlinger感知不到-->
	   <!--关键点2 -->
	    sp<Surface> surface(new Surface(producer, true));
	    surface->incStrong(&sRefBaseOwner);
	    return jlong(surface.get());
	}

SurfaceView跟TexutureView在使用Surface的时候，SurfaceView的Surface的Consumer是SurfaceFlinger（BnGraphicBufferProducer是在SF中创建的），但是TexutureView中SurfaceView的consumer却是TexutureView（BnGraphicBufferProducer是在APP中创建的），所以数据必须再由TexutureView处理后，给SF才可以，这也是TextureView效率低的原因。 

# SurfaceView的硬件加速跟软件绘制

视频播放应该是数据直接填充到SurfaceView的那块内存

# Surface的内存分配与数据流

Surface都是归SF管理，所有的分配最后都会走到SF，一个Surface有一个BufferQueue，一个Queue有多个slot，    

	BufferQueueDefs::SlotsType mSlots;

producer跟consumer都会映射这个slots，一个surface有一块内存，这块内存有很多歌slot 32 或者64 

    // mSlots is an array of buffer slots that must be mirrored on the producer
    // side. This allows buffer ownership to be transferred between the producer
    // and consumer without sending a GraphicBuffer over Binder. The entire
    // array is initialized to NULL at construction time, and buffers are
    // allocated for a slot when requestBuffer is called with that slot's index.
    BufferQueueDefs::SlotsType mSlots;
    
不过SurfaceView传说的前后双缓冲是怎么回事？    

# SurfaceView如何支持视频播放，到底有几块缓存back front？

    // must be used from the lock/unlock thread
    
    // 之类的GraphicBuffer很明显不止一块
    sp<GraphicBuffer>           mLockedBuffer;
    sp<GraphicBuffer>           mPostedBuffer;
    
 同一时刻，有几块内存生效呢？ 



#     参考文档

[Android BufferQueue简析](https://www.jianshu.com/p/edd7d264be73)