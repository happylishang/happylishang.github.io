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



