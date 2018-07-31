# 纹理对象就会被附加上纹理图像

生成了纹理和相应的多级渐远纹理后，释放图像的内存并解绑纹理对象是一个很好的习惯。

SOIL_free_image_data(image);
glBindTexture(GL_TEXTURE_2D, 0);

纹理中有自己的备份

# HardWareLayer的概念，TextrueView如何获取HardwareLayer，它到底是什么？

 
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
     * 注意这里的layer通SurfaceFlinger的layer是一样的，只不过，这里是作为GLconsumer中转为纹理
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
	
	DeferredLayerUpdater* RenderProxy::createTextureLayer() {
	    SETUP_TASK(createTextureLayer);
	    args->context = mContext;
	    args->thread = &mRenderThread;
	    void* retval = postAndWait(task);
	    DeferredLayerUpdater* layer = reinterpret_cast<DeferredLayerUpdater*>(retval);
	    return layer;
	}

	CREATE_BRIDGE2(createTextureLayer, RenderThread* thread, CanvasContext* context) {
	    Layer* layer = args->context->createTextureLayer();
	    if (!layer) return nullptr;
	    return new DeferredLayerUpdater(*args->thread, layer);
	} 
	
	
 DeferredLayerUpdater是什么里面有什么？CanvasContext的createTextureLayer,调用CanvasContext::createTextureLayer创建一个TextureLayer

	Layer* CanvasContext::createTextureLayer() {
	    requireSurface();
	    return LayerRenderer::createTextureLayer(mRenderThread.renderState());
	}
	
	Layer* LayerRenderer::createTextureLayer(RenderState& renderState) {
	
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

调用OpenGL API创建一个纹理，注意：对于这里创建的Layer，每个只能对应一个纹理，只是创建，未绑定纹理图像，

	void Layer::generateTexture() {
	    if (!texture.id) {
	        glGenTextures(1, &texture.id);
	    }
	}

之后被，包装一层组成DeferredLayerUpdater ，

	DeferredLayerUpdater::DeferredLayerUpdater(renderthread::RenderThread& thread, Layer* layer)
	        : mSurfaceTexture(nullptr)
	        , mTransform(nullptr)
	        , mNeedsGLContextAttach(false)
	        , mUpdateTexImage(false)
	        , mLayer(layer)
	        , mCaches(Caches::getInstance())
	        , mRenderThread(thread) {
	    mWidth = mLayer->layer.getWidth();
	    mHeight = mLayer->layer.getHeight();
	    mBlend = mLayer->isBlend();
	    mColorFilter = SkSafeRef(mLayer->getColorFilter());
	    mAlpha = mLayer->getAlpha();
	    mMode = mLayer->getMode();
	}

再包装，构成HardwareLayer
   
    static HardwareLayer adoptTextureLayer(ThreadedRenderer renderer, long layer) {
        return new HardwareLayer(renderer, layer);
    }   
    
Layer构建完毕，还需要有数据来进行填充，数据存储在哪呢？那就是SurfaceTexture，首先新建一个SurfaceTexture，之后将SurfaceTexture设置给Layer，先看下SurfaceTexture的构造函数
  
      public SurfaceTexture(int texName, boolean singleBufferMode) {
        mCreatorLooper = Looper.myLooper();
        nativeInit(false, texName, singleBufferMode, new WeakReference<SurfaceTexture>(this));
    }

	
	static void SurfaceTexture_init(JNIEnv* env, jobject thiz, jboolean isDetached,
	        jint texName, jboolean singleBufferMode, jobject weakThiz)
	{
	    sp<IGraphicBufferProducer> producer;
	    sp<IGraphicBufferConsumer> consumer;
	    BufferQueue::createBufferQueue(&producer, &consumer);
	
	    if (singleBufferMode) {
	        consumer->disableAsyncBuffer();
	        consumer->setDefaultMaxBufferCount(1);
	    }
	
	    sp<GLConsumer> surfaceTexture;
	    if (isDetached) {
	        surfaceTexture = new GLConsumer(consumer, GL_TEXTURE_EXTERNAL_OES,
	                true, true);
	    } else {
	        surfaceTexture = new GLConsumer(consumer, texName,
	                GL_TEXTURE_EXTERNAL_OES, true, true);
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
 
默认情况下，consumerIsSurfaceFlinger=false，也就是普通APP也能自己申请内存，但是这部分内存如何给SF呢？先看下将SurfaceTexture设置到Layer，也可以理解成为Layer绑定数据源

	static void android_view_HardwareLayer_setSurfaceTexture(JNIEnv* env, jobject clazz,
	        jlong layerUpdaterPtr, jobject surface, jboolean isAlreadyAttached) {
	    DeferredLayerUpdater* layer = reinterpret_cast<DeferredLayerUpdater*>(layerUpdaterPtr);
	    sp<GLConsumer> surfaceTexture(SurfaceTexture_getSurfaceTexture(env, surface));
	    layer->setSurfaceTexture(surfaceTexture, !isAlreadyAttached);
	}

Layer通过setSurfaceTexture

    ANDROID_API void setSurfaceTexture(const sp<GLConsumer>& texture, bool needsAttach) {
        if (texture.get() != mSurfaceTexture.get()) {
            mNeedsGLContextAttach = needsAttach;
            mSurfaceTexture = texture;
            GLenum target = texture->getCurrentTextureTarget();

        }
    }
  
  mSurfaceTexture其实就是sp<GLConsumer>，到这里Layer取到了mSurfaceTexture，同时也拿到了这个相应target类型，下一步看更新，不过更新之前，需要知道SurfaceTexture中的数据是如何来的。
    
# SurfaceTexture是什么？

>引用的

SurfaceTexture 类是在 Android 3.0 中引入的。就像 SurfaceView 是 Surface 和 View 的结合一样，SurfaceTexture 是 Surface 和 GLES texture 的粗糙结合（有几个警告）。

当你创建了一个 SurfaceTexture，你就创建了你的应用作为消费者的 BufferQueue。当一个新的缓冲区由生产者入对时，你的应用将通过回调 (onFrameAvailable()) 被通知。你的应用调用 updateTexImage()，这将释放之前持有的缓冲区，并从队列中获取新的缓冲区，执行一些 EGL 调用以使缓冲区可作为一个外部 texture 由 GLES 使用。

# SurfaceTure的数据流动


SurfaceTexture最核心的是它可以被看做GLConsumer+ BufferQueueProducer，其他端可以通过IGraphicBufferProducer向SurfaceTexture申请内存buffer，填充好数据后，再提交给SurfaceTexture，buffer的数据源自远端，比如摄像头，或者视频流。也就是数据源，通知更新的入口，底层调用入口，使用SurfaceTexture的时候一般会为其设置一个OnFrameAvailableListener,以便数据到来获得通知，我们跟随这条路，看看流程，拿摄像头数据的显示为例子。Carmera有个函数

    public native final void setPreviewTexture(SurfaceTexture surfaceTexture) throws IOException;

调用之后，SurfaceTexture就能在摄像头数据发生更新的时候收到回调，同时数据也会被同步映射到SurfaceTexture这面（共享内存）：

	static void android_hardware_Camera_setPreviewTexture(JNIEnv *env,
	        jobject thiz, jobject jSurfaceTexture)
	{
	    sp<Camera> camera = get_native_camera(env, thiz, NULL);
	    if (camera == 0) return;
	
	    sp<IGraphicBufferProducer> producer = NULL;
	    if (jSurfaceTexture != NULL) {
	        producer = SurfaceTexture_getProducer(env, jSurfaceTexture);
	       ...
	    }
	    if (camera->setPreviewTarget(producer) != NO_ERROR) {
	        ...
	    }
	}
	
上面主要是将IGraphicBufferProducer传递给Camera，之后Camera就能向SurfaceTexture申请申请一块内存，用来存放摄像头数据：

    // pass the buffered IGraphicBufferProducer to the camera service
    status_t setPreviewTarget(const sp<IGraphicBufferProducer>& bufferProducer)
    {
        ALOGV("setPreviewTarget");
        Parcel data, reply;
        data.writeInterfaceToken(ICamera::getInterfaceDescriptor());
        sp<IBinder> b(IInterface::asBinder(bufferProducer));
        data.writeStrongBinder(b);
        remote()->transact(SET_PREVIEW_TARGET, data, &reply);
        return reply.readInt32();
    }

可以看到，其实是一个Binder跨进程请求，最终调用Camera::setPreviewTarget，将IGraphicBufferProducer传递给camera服务，

	// pass the buffered IGraphicBufferProducer to the camera service
	status_t Camera::setPreviewTarget(const sp<IGraphicBufferProducer>& bufferProducer)
	{
	    ALOGV("setPreviewTarget(%p)", bufferProducer.get());
	    sp <::android::hardware::ICamera> c = mCamera;
	    if (c == 0) return NO_INIT;
	    ALOGD_IF(bufferProducer == 0, "app passed NULL surface");
	    return c->setPreviewTarget(bufferProducer);
	}

这样当Camera摄像头捕获数据后，如果想要传输给SurfaceTexture，就向SurfaceTexture申请一块内存（匿名共享内存），将数据填充到这块内存，并通知SurfaceTexture，其实就是dequeue与enqueue操作。Camera服务有空再看，这里直接看书到来之后，如何通知SurfaceTexture及TextureView更新：



>SurfaceTexture.cpp

	void JNISurfaceTextureContext::onFrameAvailable(const BufferItem& /* item */)
	{
	    bool needsDetach = false;
	    JNIEnv* env = getJNIEnv(&needsDetach);
	    if (env != NULL) {
	        env->CallStaticVoidMethod(mClazz, fields.postEvent, mWeakThiz);
	    } else {
	    }
	    if (needsDetach) {
	        detachJNI();
	    }
	}

>SurfaceTexture.java

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

对于TextrueView，其实会回调

    private final SurfaceTexture.OnFrameAvailableListener mUpdateListener =
            new SurfaceTexture.OnFrameAvailableListener() {
        @Override
        public void onFrameAvailable(SurfaceTexture surfaceTexture) {
        <!--标记图层需要更新-->
            updateLayer();
            <!--重绘-->
            invalidate();
        }
    };

其实就是触发重绘，构建DrawOp Tree的时候，
    
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
 
	 void DisplayListCanvas::drawLayer(DeferredLayerUpdater* layerHandle, float x, float y) {
	    // We ref the DeferredLayerUpdater due to its thread-safe ref-counting
	    // semantics.
	    mDisplayListData->ref(layerHandle);
	    addDrawOp(new (alloc()) DrawLayerOp(layerHandle->backingLayer(), x, y));
	}
 
 重绘的时候，会接着调用draw

    @Override
    public final void draw(Canvas canvas) {
        // NOTE: Maintain this carefully (see View.java)
        mPrivateFlags = (mPrivateFlags & ~PFLAG_DIRTY_MASK) | PFLAG_DRAWN;

        applyUpdate();
        applyTransformMatrix();
    }

最后会先为Textview构建一个DrawLayerOp，之后再调用
   
	 private void applyUpdate() {
	        if (mLayer == null) {
	            return;
	        }
	
	        synchronized (mLock) {
	            if (mUpdateLayer) {
	                mUpdateLayer = false;
	            } else {
	                return;
	            }
	        }
	
	        mLayer.prepare(getWidth(), getHeight(), mOpaque);
	        mLayer.updateSurfaceTexture();
	
	        if (mListener != null) {
	            mListener.onSurfaceTextureUpdated(mSurface);
	        }
	    }

mLayer.updateSurfaceTexture()会将Camera传递过来数据绑定到OpenGL纹理，继续看

	static void android_view_HardwareLayer_updateSurfaceTexture(JNIEnv* env, jobject clazz,
	    jlong layerUpdaterPtr) {
	DeferredLayerUpdater* layer = reinterpret_cast<DeferredLayerUpdater*>(layerUpdaterPtr);
	layer->updateTexImage();
	}
	
	
	void DeferredLayerUpdater::doUpdateTexImage() {
	if (mSurfaceTexture->updateTexImage() == NO_ERROR) {
	    float transform[16];
	
		...
	    bool forceFilter = false;
	    <!--获取当前提交的Buffer-->
	    sp<GraphicBuffer> buffer = mSurfaceTexture->getCurrentBuffer();
	    if (buffer != nullptr) {
	        // force filtration if buffer size != layer size
	        forceFilter = mWidth != static_cast<int>(buffer->getWidth())
	                || mHeight != static_cast<int>(buffer->getHeight());
	    }
	
       
	    mSurfaceTexture->getTransformMatrix(transform);
	    <!--获取之前的纹理-->
	    GLenum renderTarget = mSurfaceTexture->getCurrentTextureTarget();
	    ...
	    <!--绑定并更新纹理贴图-->
	    LayerRenderer::updateTextureLayer(mLayer, mWidth, mHeight,
	            !mBlend, forceFilter, renderTarget, transform);
	}
	}

首先获取最近提交的GraphicBuffer，找到SurfaceTexture对应纹理标签，最后将最新的buffer绑定到纹理

	void LayerRenderer::updateTextureLayer(Layer* layer, uint32_t width, uint32_t height,
	        bool isOpaque, bool forceFilter, GLenum renderTarget, float* textureTransform) {
	    if (layer) {
	        layer->setBlend(!isOpaque);
	        layer->setForceFilter(forceFilter);
	        layer->setSize(width, height);
	        layer->layer.set(0.0f, 0.0f, width, height);
	        layer->region.set(width, height);
	        layer->regionRect.set(0.0f, 0.0f, width, height);
	        layer->getTexTransform().load(textureTransform);
	
	        if (renderTarget != layer->getRenderTarget()) {
	            layer->setRenderTarget(renderTarget);
	            layer->bindTexture();
	            layer->setFilter(GL_NEAREST, false, true);
	            layer->setWrap(GL_CLAMP_TO_EDGE, false, true);
	        }
	    }
	}

**最后兜兜转转会调用glBindTexture**

	void TextureState::bindTexture(GLuint texture) {
	    if (mBoundTextures[mTextureUnit] != texture) {
	        glBindTexture(GL_TEXTURE_2D, texture);
	        mBoundTextures[mTextureUnit] = texture;
	    }
	}
	
到这里，纹理处理完毕，之后绘制的时候，会将Layer对应的数据动态绑定到当前纹理，完成绘制。



	void OpenGLRenderer::drawLayer(Layer* layer, float x, float y) {
	    if (!layer) {
	        return;
	    }
	
	    mat4* transform = nullptr;
	    if (layer->isTextureLayer()) {
	        transform = &layer->getTransform();
	        if (!transform->isIdentity()) {
	            save(SkCanvas::kMatrix_SaveFlag);
	            concatMatrix(*transform);
	        }
	    }
	
	    bool clipRequired = false;
	    const bool rejected = mState.calculateQuickRejectForScissor(
	            x, y, x + layer->layer.getWidth(), y + layer->layer.getHeight(),
	            &clipRequired, nullptr, false);
	
	    if (rejected) {
	        if (transform && !transform->isIdentity()) {
	            restore();
	        }
	        return;
	    }
	
	    EVENT_LOGD("drawLayer," RECT_STRING ", clipRequired %d", x, y,
	            x + layer->layer.getWidth(), y + layer->layer.getHeight(), clipRequired);
	
	    updateLayer(layer, true);
	
	    mRenderState.scissor().setEnabled(mScissorOptimizationDisabled || clipRequired);
	    mCaches.textureState().activateTexture(0);
	
	    if (CC_LIKELY(!layer->region.isEmpty())) {
	        if (layer->region.isRect()) {
	            DRAW_DOUBLE_STENCIL_IF(!layer->hasDrawnSinceUpdate,
	                    composeLayerRect(layer, layer->regionRect));
	        } else if (layer->mesh) {
	            Glop glop;
	            GlopBuilder(mRenderState, mCaches, &glop)
	                    .setRoundRectClipState(currentSnapshot()->roundRectClipState)
	                    .setMeshTexturedIndexedQuads(layer->mesh, layer->meshElementCount)
	                    .setFillLayer(layer->getTexture(), layer->getColorFilter(), getLayerAlpha(layer), layer->getMode(), Blend::ModeOrderSwap::NoSwap)
	                    .setTransform(*currentSnapshot(),  TransformFlags::None)
	                    .setModelViewOffsetRectSnap(x, y, Rect(0, 0, layer->layer.getWidth(), layer->layer.getHeight()))
	                    .build();
	            DRAW_DOUBLE_STENCIL_IF(!layer->hasDrawnSinceUpdate, renderGlop(glop));
	#if DEBUG_LAYERS_AS_REGIONS
	            drawRegionRectsDebug(layer->region);
	#endif
	        }
	
	        if (layer->debugDrawUpdate) {
	            layer->debugDrawUpdate = false;
	
	            SkPaint paint;
	            paint.setColor(0x7f00ff00);
	            drawColorRect(x, y, x + layer->layer.getWidth(), y + layer->layer.getHeight(), &paint);
	        }
	    }
	    layer->hasDrawnSinceUpdate = true;
	
	    if (transform && !transform->isIdentity()) {
	        restore();
	    }
	
	    mDirty = true;
	}


其实就是利用SurfaceTexture中传过来的数据做纹理贴图，再进一步绘制到EglSurface对应的内存中去。


allocateTexture

 LayerRenderer::copyLayer


同步的时候，绑定上传内存


	Layer* LayerRenderer::createRenderLayer(RenderState& renderState, uint32_t width, uint32_t height) {
	    ATRACE_FORMAT("Allocate %ux%u HW Layer", width, height);
	    LAYER_RENDERER_LOGD("Requesting new render layer %dx%d", width, height);
	
	    Caches& caches = Caches::getInstance();
	    GLuint fbo = caches.fboCache.get();
	    if (!fbo) {
	        ALOGW("Could not obtain an FBO");
	        return nullptr;
	    }
	
	    caches.textureState().activateTexture(0);
	    Layer* layer = caches.layerCache.get(renderState, width, height);
	    if (!layer) {
	        ALOGW("Could not obtain a layer");
	        return nullptr;
	    }
	
	    // We first obtain a layer before comparing against the max texture size
	    // because layers are not allocated at the exact desired size. They are
	    // always created slighly larger to improve recycling
	    const uint32_t maxTextureSize = caches.maxTextureSize;
	    if (layer->getWidth() > maxTextureSize || layer->getHeight() > maxTextureSize) {
	        ALOGW("Layer exceeds max. dimensions supported by the GPU (%dx%d, max=%dx%d)",
	                width, height, maxTextureSize, maxTextureSize);
	
	        // Creating a new layer always increment its refcount by 1, this allows
	        // us to destroy the layer object if one was created for us
	        layer->decStrong(nullptr);
	
	        return nullptr;
	    }
	
	    layer->setFbo(fbo);
	    layer->layer.set(0.0f, 0.0f, width, height);
	    layer->texCoords.set(0.0f, height / float(layer->getHeight()),
	            width / float(layer->getWidth()), 0.0f);
	    layer->setAlpha(255, SkXfermode::kSrcOver_Mode);
	    layer->setColorFilter(nullptr);
	    layer->setDirty(true);
	    layer->region.clear();
	
	    GLuint previousFbo = renderState.getFramebuffer();
	
	    renderState.bindFramebuffer(layer->getFbo());
	    layer->bindTexture();
	
	    // Initialize the texture if needed
	    if (layer->isEmpty()) {
	        layer->setEmpty(false);
	        layer->allocateTexture();
	
	        // This should only happen if we run out of memory
	        if (CC_UNLIKELY(GLUtils::dumpGLErrors())) {
	            LOG_ALWAYS_FATAL("Could not allocate texture for layer (fbo=%d %dx%d)",
	                    fbo, width, height);
	            renderState.bindFramebuffer(previousFbo);
	            layer->decStrong(nullptr);
	            return nullptr;
	        }
	    }
	
	    // 帧缓冲
	    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
	            layer->getTextureId(), 0);
	
	    renderState.bindFramebuffer(previousFbo);
	
	    return layer;
	}
	
	
	



 
# 为什么会跳过前几帧？

The image stream may come from either camera preview or video decode. A Surface created from a SurfaceTexture can be used as an output destination for the android.hardware.camera2, MediaCodec, MediaPlayer, and Allocation APIs. When updateTexImage() is called, the contents of the texture object specified when the SurfaceTexture was created are updated to contain the most recent image from the image stream. This may cause some frames of the stream to be skipped.

A SurfaceTexture may also be used in place of a SurfaceHolder when specifying the output destination of the older Camera API. Doing so will cause all the frames from the image stream to be sent to the SurfaceTexture object rather than to the device's display.


# TexutView跟SurfaceTexure，是可以先创建纹理，在绑定上去，

    public SurfaceTexture(int texName) {
        this(texName, false);
    }

使用SurfaceTexture实现滤镜的关键，就是要自己创建有id的textture，之后再处理，是直接输出到帧缓冲区，还是怎么处理，要看

# glGenFramebuffers

获取帧缓冲区，直接渲染到屏幕，

# Frame Buffer Object（FBO）

Frame Buffer Object（FBO）即为帧缓冲对象，用于离屏渲染缓冲。相对于其它同类技术，如数据拷贝或交换缓冲区等，使用FBO技术会更高效并且更容易实现。而且FBO不受窗口大小限制。FBO可以包含许多颜色缓冲区，可以同时从一个片元着色器写入。FBO是一个容器，自身不能用于渲染，需要与一些可渲染的缓冲区绑定在一起，像纹理或者渲染缓冲区。 
Render Buffer Object（RBO）即为渲染缓冲对象，分为color buffer(颜色)、depth buffer(深度)、stencil buffer(模板)。 
在使用FBO做离屏渲染时，可以只绑定纹理，也可以只绑定Render Buffer，也可以都绑定或者绑定多个，视使用场景而定。如只是对一个图像做变色处理等，只绑定纹理即可。如果需要往一个图像上增加3D的模型和贴纸，则一定还要绑定depth Render Buffer。 

# GLsurfaceView方便在它创建了OpenGL上下文

猜测还是走SF那一套，不过EglSurface创建后，都是从这个对应的控件申请内存，也就说其实还是SF那一套

# OpenGL的绘图内存如何获取的

EglSurface的概念，是不是所有的内容都会流入EglSurface，它跟Surface绑定

            mEglSurface = mEgl.eglCreateWindowSurface(mEglDisplay, mEglConfig, mSurface, null);

EglSurface其实就是映射到Surface，当EglSurface bindTexre的时候，其实就是将数据传递到Surface，所以也是直接传递。 

# Texture是纹理，纹理是一个集合，采样用的，本身不算到绘制内存中去

绘制的内容是从纹理中采样得到的，但是纹理本身不是绘制，纹理是模板，但是模板不是画。OpenGL是个标准的框架，按照里面走就行。





#     参考文档

[Android BufferQueue简析](https://www.jianshu.com/p/edd7d264be73)           
[不错的demo GraphicsTestBed ](https://github.com/lb377463323/GraphicsTestBed)        
[小窗播放视频的原理和实现（上）](http://www.10tiao.com/html/223/201712/2651232830/1.html)         
[GLTextureViewActivity.java ](https://android.googlesource.com/platform/frameworks/base/+/master/tests/HwAccelerationTest/src/com/android/test/hwui/GLTextureViewActivity.java)