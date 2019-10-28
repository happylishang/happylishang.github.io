其实对于Surface，最关键的就是一个IGraphicBufferProducer，他是一个生产者，GraphicBuffer，deque eque 

**BufferQueue是Android 中所有图形处理操作的核心。它的作用很简单：将生成图形数据缓冲区的一方（生产方）连接到接受数据以进行显示或进一步处理的一方（消耗方）。几乎所有在系统中移动图形数据缓冲区的内容都依赖于BufferQueue，比如显示、编码等。 以显示流程为例，生产者进程一般就是应用程序进程，消费者进程就是SurfaceFlinger进程，应用程序进程的surface对象和SurfaceFlinger进程的layer对象可以看做实际的生产者和消费者，主要类的关系如下所示：，应用程序申请surface时，会在SurfaceFlinger进程创建一个layer对象，接着会创建BufferQueueCore、BufferQueueProducer和BufferQueueConsumer对象，然后创建SurfaceFlingerConsumer和ProxyConsumerListener对象，而在应用程序进程这端会创建Surface对象和BpGraphicBufferProducer代理对象，应用程序进程通过Surface对象中的BpGraphicBufferProducer向SurfaceFlinger进程中的BufferQueueCore对象申请和提交GraphicBuffer，SurfaceFlinger进程中的BufferQueueCore对象通过ProxyConsumerListener、SurfaceFlingerConsumer、Layer一路通知到SurfaceFlinger有新的GraphicBuffer需要合成，SurfaceFlinger收到通知，通过Layer调用SurfaceFlingerConsumer的updateTexImage，将GraphicBuffer绘制成纹理，然后再合成输出。**

 

SurfaceTexture 类是在 Android 3.0 中推出的。就像 SurfaceView 是 Surface 和 View 的组合一样，SurfaceTexture 是 Surface 和 GLES 纹理的粗略组合（包含几个注意事项）。

当您创建 SurfaceTexture 时，会创建一个应用是其消耗方的 BufferQueue。如果生产方将新的缓冲区加入队列，您的应用便会通过回调 (onFrameAvailable()) 获得通知。应用调用 updateTexImage()（这会释放先前保留的缓冲区），从队列中获取新的缓冲区，然后发出一些 EGL 调用，让缓冲区可作为外部纹理供 GLES 使用。

外部纹理
外部纹理 (GL_TEXTURE_EXTERNAL_OES) 与 GLES (GL_TEXTURE_2D) 创建的纹理并不完全相同：您对渲染器的配置必须有所不同，而且有一些操作是不能对外部纹理执行的。关键是，您可以直接从 BufferQueue 接收到的数据中渲染纹理多边形。gralloc 支持各种格式，因此我们需要保证缓冲区中数据的格式是 GLES 可以识别的格式。为此，当 SurfaceTexture 创建 BufferQueue 时，它将消耗方用法标记设置为 GRALLOC_USAGE_HW_TEXTURE，确保由 gralloc 创建的缓冲区均可供 GLES 使用。

由于 SurfaceTexture 会与 EGL 上下文交互，因此您必须小心地从正确的会话中调用其方法（详见类文档）


TextureView
我们在 Android 4.0 中引入了 TextureView 类，它结合了 View 与 SurfaceTexture，是我们在此讨论的最复杂的 View 对象。

使用 GLES 呈现
我们已经知道，SurfaceTexture 是一个“GL 消费者”，它会占用图形数据的缓冲区，并将它们作为纹理进行提供。TextureView 会对 SurfaceTexture 进行封装，并接管对回调做出响应以及获取新缓冲区的责任。新缓冲区的就位会导致 TextureView 发出 View 失效请求。当被要求进行绘图时，TextureView 会使用最近收到的缓冲区的内容作为数据源，并根据 View 状态的指示，以相应的方式在相应的位置进行呈现。

您可以使用 GLES 在 TextureView 上呈现内容，就像在 SurfaceView 上一样。只需将 SurfaceTexture 传递到 EGL 窗口创建调用即可。不过，这样做会导致潜在问题。

在我们看到的大部分内容中，BufferQueue 是在不同进程之间传递缓冲区。当使用 GLES 呈现到 TextureView 时，生产者和消费者处于同一进程中，它们甚至可能会在单个线程上得到处理。假设我们以快速连续的方式从界面线程提交多个缓冲区。EGL 缓冲区交换调用需要使一个缓冲区从 BufferQueue 出列，而在有可用的缓冲区之前，它将处于暂停状态。只有当消费者获取一个缓冲区用于呈现时才会有可用的缓冲区，但是这一过程也会发生在界面线程上…因此我们陷入了困境。

解决方案是让 BufferQueue 确保始终有一个可用的缓冲区能够出列，以使缓冲区交换始终不会暂停。要保证能够实现这一点，一种方法是让 BufferQueue 在新缓冲区加入队列时舍弃之前加入队列的缓冲区的内容，并对最小缓冲区计数和最大获取缓冲区计数施加限制（如果您的队列有三个缓冲区，而所有这三个缓冲区均被消费者获取，那么就没有可以出列的缓冲区，缓冲区交换调用必然会暂停或失败。因此我们需要防止消费者一次获取两个以上的缓冲区）。丢弃缓冲区通常是不可取的，因此仅允许在特定情况下发生，例如生产者和消费者处于同一进程中时。

SurfaceView 还是 TextureView？
SurfaceView 和 TextureView 扮演的角色类似，但是拥有截然不同的实现。要作出最合适的选择，则需要了解它们各自的利弊。
因为 TextureView 是 View 层次结构的固有成员，所以其行为与其他所有 View 一样，可以与其他元素相互叠加。您可以执行任意转换，并通过简单的 API 调用将内容检索为位图。

影响 TextureView 的主要因素是合成步骤的表现。使用 SurfaceView 时，内容可以写到 SurfaceFlinger（理想情况下使用叠加层）合成的独立分层中。使用 TextureView 时，View 合成往往使用 GLES 执行，并且对其内容进行的更新也可能会导致其他 View 元素重绘（例如，如果它们位于 TextureView 上方）。View 呈现完成后，应用界面层必须由 SurfaceFlinger 与其他分层合成，以便您可以高效地将每个可见像素合成两次。对于全屏视频播放器，或任何其他相当于位于视频上方的界面元素的应用，SurfaceView 可以带来更好的效果。

如之前所述，受 DRM 保护的视频只能在叠加平面上呈现。支持受保护内容的视频播放器必须使用 SurfaceView 进行实现。

案例研究：Grafika 的视频播放 (TextureView)
Grafika 包括一对视频播放器，一个用 TextureView 实现，另一个用 SurfaceView 实现。对于这两个视频播放器来说，仅将帧从 MediaCodec 发送到 Surface 的视频解码部分是一样的。这两种实现之间最有趣的区别是呈现正确宽高比所需的步骤。

SurfaceView 需要 FrameLayout 的自定义实现; 而要重新调整 SurfaceTexture 的大小，只需使用 TextureView#setTransform() 配置转换矩阵即可。对于前者，您会通过 WindowManager 向 SurfaceFlinger 发送新的窗口位置和大小值；对于后者，您仅仅是在以不同的方式呈现它。

否则，两种实现均遵循相同的模式。创建Surface后，系统会启用播放。点击“播放”时，系统会启动视频解码线程，并将 Surface 作为输出目标。之后，应用代码不需要执行任何操作，SurfaceFlinger（适用于SurfaceView）或 TextureView 会处理合成和显示。

案例研究：Grafika 的双重解码
此操作组件演示了在 TextureView 中对 SurfaceTexture 的操控。

此操作组件的基本结构是一对显示两个并排播放的不同视频的 TextureView。为了模拟视频会议应用的需求，我们希望在操作组件因屏幕方向发生变化而暂停和恢复时，MediaCodec 解码器能保持活动状态。原因在于，如果不对 MediaCodec 解码器使用的 Surface 进行完全重新配置，就无法更改它，而这是成本相当高的操作；因此我们希望 Surface 保持活动状态。Surface 只是 SurfaceTexture 的 BufferQueue 中生产者界面的句柄，而 SurfaceTexture 由 TextureView 管理；因此我们还需要 SurfaceTexture 保持活动状态。那么我们如何处理 TextureView 被关闭的情况呢？

TextureView 提供的 setSurfaceTexture() 调用正好能够满足我们的需求。我们从 TextureView 获取对 SurfaceTexture 的引用，并将它们保存在静态字段中。当操作组件被关闭时，我们从 onSurfaceTextureDestroyed() 回调返回“false”，以防止 SurfaceTexture 被销毁。当操作组件重新启动时，我们将原来的 SurfaceTexture 填充到新的 TextureView 中。TextureView 类负责创建和破坏 EGL 上下文。

每个视频解码器都是从单独的线程驱动的。乍一看，我们似乎需要每个线程的本地 EGL 上下文；但请注意，具有解码输出的缓冲区实际上是从 mediaserver 发送给我们的 BufferQueue 消费者 (SurfaceTexture)。TextureView 会为我们处理呈现，并在界面线程上执行。

使用 SurfaceView 实现该操作组件可能较为困难。我们不能只创建一对 SurfaceView 并将输出引导至它们，因为 Surface 在屏幕方向改变期间会被销毁。此外，这样做会增加两个层，而由于可用叠加层的数量限制，我们不得不尽量将层数量减到最少。与上述方法不同，我们希望创建一对 SurfaceTexture，以从视频解码器接收输出，然后在应用中执行呈现，使用 GLES 将两个纹理间隙呈现到 SurfaceView 的 Surface。


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

如何通过SurfaceTexture实现数据的流动

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

# OpenGl绘制后，仍然需要入队swapBuffers，是通知绘制或者合成的关键，

TextureView会触发重绘（硬件加速），并通知SF合成，但是SurfaceView会直接通知SF合成，
	
	EGLBooleanegl_window_surface_v2_t::swapBuffers()
	 
	{
	 
	    //………….
	 
	    nativeWindow->queueBuffer(nativeWindow,buffer, -1);
	 
	 
	 
	    // dequeue a new buffer
	 
	    if (nativeWindow->dequeueBuffer（nativeWindow, &buffer, &fenceFd)== NO_ERROR) {
	 
	        sp<Fence> fence(new Fence(fenceFd));
	 
	        if(fence->wait(Fence::TIMEOUT_NEVER)) {
	 
	           nativeWindow->cancelBuffer(nativeWindow, buffer, fenceFd);
	 
	            return setError(EGL_BAD_ALLOC,EGL_FALSE);
	 
	        }
	 
	//。。。。。。

消费方会获得通知
	
	status_t BufferQueueProducer::queueBuffer(int slot,
	        const QueueBufferInput &input, QueueBufferOutput *output) {
	    ATRACE_CALL();
	    ATRACE_BUFFER_INDEX(slot);
	
	    int64_t timestamp;
	    bool isAutoTimestamp;
	    android_dataspace dataSpace;
	    Rect crop;
	    int scalingMode;
	    uint32_t transform;
	    uint32_t stickyTransform;
	    bool async;
	    sp<Fence> fence;
	    input.deflate(&timestamp, &isAutoTimestamp, &dataSpace, &crop, &scalingMode,
	            &transform, &async, &fence, &stickyTransform);
	    Region surfaceDamage = input.getSurfaceDamage();
	
	    if (fence == NULL) {
	        BQ_LOGE("queueBuffer: fence is NULL");
	        return BAD_VALUE;
	    }
	
	    switch (scalingMode) {
	        case NATIVE_WINDOW_SCALING_MODE_FREEZE:
	        case NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW:
	        case NATIVE_WINDOW_SCALING_MODE_SCALE_CROP:
	        case NATIVE_WINDOW_SCALING_MODE_NO_SCALE_CROP:
	            break;
	        default:
	            BQ_LOGE("queueBuffer: unknown scaling mode %d", scalingMode);
	            return BAD_VALUE;
	    }
	
		// queue之后就会通知就IConsumerListener
	    sp<IConsumerListener> frameAvailableListener;
	    sp<IConsumerListener> frameReplacedListener;
	    int callbackTicket = 0;
	    BufferItem item;
	    { // Autolock scope
	        Mutex::Autolock lock(mCore->mMutex);
	
	        if (mCore->mIsAbandoned) {
	            BQ_LOGE("queueBuffer: BufferQueue has been abandoned");
	            return NO_INIT;
	        }
	
	        const int maxBufferCount = mCore->getMaxBufferCountLocked(async);
	        if (async && mCore->mOverrideMaxBufferCount) {
	            // FIXME: Some drivers are manually setting the buffer count
	            // (which they shouldn't), so we do this extra test here to
	            // handle that case. This is TEMPORARY until we get this fixed.
	            if (mCore->mOverrideMaxBufferCount < maxBufferCount) {
	                BQ_LOGE("queueBuffer: async mode is invalid with "
	                        "buffer count override");
	                return BAD_VALUE;
	            }
	        }
	
	        if (slot < 0 || slot >= maxBufferCount) {
	            BQ_LOGE("queueBuffer: slot index %d out of range [0, %d)",
	                    slot, maxBufferCount);
	            return BAD_VALUE;
	        } else if (mSlots[slot].mBufferState != BufferSlot::DEQUEUED) {
	            BQ_LOGE("queueBuffer: slot %d is not owned by the producer "
	                    "(state = %d)", slot, mSlots[slot].mBufferState);
	            return BAD_VALUE;
	        } else if (!mSlots[slot].mRequestBufferCalled) {
	            BQ_LOGE("queueBuffer: slot %d was queued without requesting "
	                    "a buffer", slot);
	            return BAD_VALUE;
	        }
	
	        BQ_LOGV("queueBuffer: slot=%d/%" PRIu64 " time=%" PRIu64 " dataSpace=%d"
	                " crop=[%d,%d,%d,%d] transform=%#x scale=%s",
	                slot, mCore->mFrameCounter + 1, timestamp, dataSpace,
	                crop.left, crop.top, crop.right, crop.bottom, transform,
	                BufferItem::scalingModeName(static_cast<uint32_t>(scalingMode)));
	
	        const sp<GraphicBuffer>& graphicBuffer(mSlots[slot].mGraphicBuffer);
	        Rect bufferRect(graphicBuffer->getWidth(), graphicBuffer->getHeight());
	        Rect croppedRect;
	        crop.intersect(bufferRect, &croppedRect);
	        if (croppedRect != crop) {
	            BQ_LOGE("queueBuffer: crop rect is not contained within the "
	                    "buffer in slot %d", slot);
	            return BAD_VALUE;
	        }
	
	        // Override UNKNOWN dataspace with consumer default
	        if (dataSpace == HAL_DATASPACE_UNKNOWN) {
	            dataSpace = mCore->mDefaultBufferDataSpace;
	        }
	
	        mSlots[slot].mFence = fence;
	        mSlots[slot].mBufferState = BufferSlot::QUEUED;
	        ++mCore->mFrameCounter;
	        mSlots[slot].mFrameNumber = mCore->mFrameCounter;
	
	        item.mAcquireCalled = mSlots[slot].mAcquireCalled;
	        item.mGraphicBuffer = mSlots[slot].mGraphicBuffer;
	        item.mCrop = crop;
	        item.mTransform = transform &
	                ~static_cast<uint32_t>(NATIVE_WINDOW_TRANSFORM_INVERSE_DISPLAY);
	        item.mTransformToDisplayInverse =
	                (transform & NATIVE_WINDOW_TRANSFORM_INVERSE_DISPLAY) != 0;
	        item.mScalingMode = static_cast<uint32_t>(scalingMode);
	        item.mTimestamp = timestamp;
	        item.mIsAutoTimestamp = isAutoTimestamp;
	        item.mDataSpace = dataSpace;
	        item.mFrameNumber = mCore->mFrameCounter;
	        item.mSlot = slot;
	        item.mFence = fence;
	        item.mIsDroppable = mCore->mDequeueBufferCannotBlock || async;
	        item.mSurfaceDamage = surfaceDamage;
	
	        mStickyTransform = stickyTransform;
	
	        if (mCore->mQueue.empty()) {
	            // When the queue is empty, we can ignore mDequeueBufferCannotBlock
	            // and simply queue this buffer
	            mCore->mQueue.push_back(item);
	            frameAvailableListener = mCore->mConsumerListener;
	        } else {
	            // When the queue is not empty, we need to look at the front buffer
	            // state to see if we need to replace it
	            BufferQueueCore::Fifo::iterator front(mCore->mQueue.begin());
	            if (front->mIsDroppable) {
	                // If the front queued buffer is still being tracked, we first
	                // mark it as freed
	                if (mCore->stillTracking(front)) {
	                    mSlots[front->mSlot].mBufferState = BufferSlot::FREE;
	                    mCore->mFreeBuffers.push_front(front->mSlot);
	                }
	                // Overwrite the droppable buffer with the incoming one
	                *front = item;
	                frameReplacedListener = mCore->mConsumerListener;
	            } else {
	                mCore->mQueue.push_back(item);
	                frameAvailableListener = mCore->mConsumerListener;
	            }
	        }
	
	        mCore->mBufferHasBeenQueued = true;
	        mCore->mDequeueCondition.broadcast();
	
	        output->inflate(mCore->mDefaultWidth, mCore->mDefaultHeight,
	                mCore->mTransformHint,
	                static_cast<uint32_t>(mCore->mQueue.size()));
	
	        ATRACE_INT(mCore->mConsumerName.string(), mCore->mQueue.size());
	
	        // Take a ticket for the callback functions
	        callbackTicket = mNextCallbackTicket++;
	
	        mCore->validateConsistencyLocked();
	    } // Autolock scope
	
	    // Wait without lock held
	    if (mCore->mConnectedApi == NATIVE_WINDOW_API_EGL) {
	        // Waiting here allows for two full buffers to be queued but not a
	        // third. In the event that frames take varying time, this makes a
	        // small trade-off in favor of latency rather than throughput.
	        mLastQueueBufferFence->waitForever("Throttling EGL Production");
	        mLastQueueBufferFence = fence;
	    }
	
	    // Don't send the GraphicBuffer through the callback, and don't send
	    // the slot number, since the consumer shouldn't need it
	    item.mGraphicBuffer.clear();
	    item.mSlot = BufferItem::INVALID_BUFFER_SLOT;
	
	    // Call back without the main BufferQueue lock held, but with the callback
	    // lock held so we can ensure that callbacks occur in order
	    {
	        Mutex::Autolock lock(mCallbackMutex);
	        while (callbackTicket != mCurrentCallbackTicket) {
	            mCallbackCondition.wait(mCallbackMutex);
	        }
	
	        if (frameAvailableListener != NULL) {
	            // 调用回调，就是这里看到了吧，就是这么强大
	            frameAvailableListener->onFrameAvailable(item);
	        } else if (frameReplacedListener != NULL) {
	            frameReplacedListener->onFrameReplaced(item);
	        }
	
	        ++mCurrentCallbackTicket;
	        mCallbackCondition.broadcast();
	    }
	
	    return NO_ERROR;
	}

TextureView收到通知后会重绘，并且这个时候已经拿到了数据，OpenGL重绘即可，比SurfaceView多一步，这部分的更新是直接到SF吗？按理说，SF那段没对应的Layer

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

        // 更新Layer
        mLayer.prepare(getWidth(), getHeight(), mOpaque);
        mLayer.updateSurfaceTexture();

        if (mListener != null) {
            mListener.onSurfaceTextureUpdated(mSurface);
        }
    }
  
# 	consumer更新  
	  
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

如何绑定Texuture
  
	  status_t GLConsumer::bindTextureImageLocked() {
	    if (mEglDisplay == EGL_NO_DISPLAY) {
	        ALOGE("bindTextureImage: invalid display");
	        return INVALID_OPERATION;
	    }
	
	    GLenum error;
	    while ((error = glGetError()) != GL_NO_ERROR) {
	        GLC_LOGW("bindTextureImage: clearing GL error: %#04x", error);
	    }
	
	    glBindTexture(mTexTarget, mTexName);
	    if (mCurrentTexture == BufferQueue::INVALID_BUFFER_SLOT &&
	            mCurrentTextureImage == NULL) {
	        GLC_LOGE("bindTextureImage: no currently-bound texture");
	        return NO_INIT;
	    }
	
	    status_t err = mCurrentTextureImage->createIfNeeded(mEglDisplay,
	                                                        mCurrentCrop);
	    if (err != NO_ERROR) {
	        GLC_LOGW("bindTextureImage: can't create image on display=%p slot=%d",
	                mEglDisplay, mCurrentTexture);
	        return UNKNOWN_ERROR;
	    }
	    mCurrentTextureImage->bindToTextureTarget(mTexTarget);
	
	    // In the rare case that the display is terminated and then initialized
	    // again, we can't detect that the display changed (it didn't), but the
	    // image is invalid. In this case, repeat the exact same steps while
	    // forcing the creation of a new image.
	    if ((error = glGetError()) != GL_NO_ERROR) {
	        glBindTexture(mTexTarget, mTexName);
	        status_t result = mCurrentTextureImage->createIfNeeded(mEglDisplay,
	                                                               mCurrentCrop,
	                                                               true);
	        if (result != NO_ERROR) {
	            GLC_LOGW("bindTextureImage: can't create image on display=%p slot=%d",
	                    mEglDisplay, mCurrentTexture);
	            return UNKNOWN_ERROR;
	        }
	        mCurrentTextureImage->bindToTextureTarget(mTexTarget);
	        if ((error = glGetError()) != GL_NO_ERROR) {
	            GLC_LOGE("bindTextureImage: error binding external image: %#04x", error);
	            return UNKNOWN_ERROR;
	        }
	    }
	
	    // Wait for the new buffer to be ready.
	    return doGLFenceWaitLocked();
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