A TextureView can be used to display a content stream. Such a content stream can for instance be a video or an OpenGL scene. The content stream can come from the application's process as well as a remote process.

TextureView can only be used in a hardware accelerated window. When rendered in software, TextureView will draw nothing.

Unlike SurfaceView, TextureView does not create a separate window but behaves as a regular View. This key difference allows a TextureView to be moved, transformed, animated, etc. For instance, you can make a TextureView semi-translucent by calling myView.setAlpha(0.5f).


# SurfaceView不支持平移、缩放、透明度改变 - 这种说法并不严谨

我们常说的View的动画，透明度，指的是里面的内容，它是跟随View自身的属性发生改变的，但是对于SurfaceView而言，它是一个独立的窗口，它的平移改变只是窗口属性的改变，但是里面的内容并不会跟随改变，比如缩放效果，虽然SurfaceView缩小了，但是里面绘制的内容并不会缩小，而SurfaceView自身是可以改变大小的


因为SurfaceView的内容不在应用窗口上，所以不能使用变换（平移、缩放、旋转等） 


# TextureView的数据流

TextureView的数据从获取到显示中间经历了哪些呢，数据提供方肯定是多媒体设备，比如摄像头或者MediaPlayer等，这些数据是如何显示到TextureView上的呢？Android中图形内存的管理基本都是依赖BufferQueue进行处理，它采用了生产者消费者模式，数据提供方是生产者，这里拿拍照作为例子，拿摄像头就是生产者，它不断的捕获数据，并将数据传递给APP端的TextureView，TextureView再收到数据的时候，需要获取数据，并通知APP重绘，使用这部分数据，将其显示出来，TextureView没有独立的窗口，但是却有独立的图形内存分配，也拥有独立的Surface，只是这个Surface只是用来暂存，不是用来显示的，这些暂存的数据经过处理，被当做纹理最终被绘制到APP从SurfaceFlinger申请的那块内存中去，如此才完成了一次Camera数据的传递。

![Android TextureView数据流向图.jpg](https://upload-images.jianshu.io/upload_images/1460468-89d2215172a685b6.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# 为什么说SurfaceView的性能效率要比TextureView的高

SurfaceView相比TextureView而言更加独立，而且更新不需要UI线程的参与，是有数据提供方直接通知SurfaceFlinger进行处理，而且相对来说，还少了一次内存大分配跟数据的处理，减少了开销。

![Android SurfaceView数据流向图.jpg](https://upload-images.jianshu.io/upload_images/1460468-651f339379ec956a.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

* 视图的更新机制
* 绘制的时候，如何处理DrawOp的使用呢

# 用法

	public class LiveCameraActivity extends Activity implements TextureView.SurfaceTextureListener {
	      private Camera mCamera;
	      private TextureView mTextureView;
	
	      protected void onCreate(Bundle savedInstanceState) {
	          super.onCreate(savedInstanceState);
	
	          mTextureView = new TextureView(this);
	          mTextureView.setSurfaceTextureListener(this);
	          setContentView(mTextureView);
	      }
	
	      public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
	          mCamera = Camera.open();
	
	          try {
	              mCamera.setPreviewTexture(surface);
	              mCamera.startPreview();
	          } catch (IOException ioe) {
	          }
	      }
	
	      public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
	          // Ignored, Camera does all the work for us
	      }
	
	      public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
	          mCamera.stopPreview();
	          mCamera.release();
	          return true;
	      }
	
	      public void onSurfaceTextureUpdated(SurfaceTexture surface) {
	          // Invoked every time there's a new Camera preview frame
	      }
	  }
 
 

有个前提概念：Layer的Render跟普通View的Render不一样，前者LayerRenderer，后者OpenGLRender

**另一方面，对于前面提到的在Android 4.0引进的TextureView，它也不是通过Display List来绘制。由于它的底层实现直接就是一个Open GL纹理，因此就可以跳过Display List这一中间层，从而提高效率。这个Open GL纹理的绘制通过一个Layer Renderer来封装。Layer Renderer和Display List Renderer可以看作是同一级别的概念，它们都是通过Open GL命令来绘制UI元素的。只不过前者操作的是Open GL纹理，而后者操作的是Display List。
**

updateDeferred
	
	void RenderNode::pushLayerUpdate(TreeInfo& info) {
    ...
    if (dirty.intersect(0, 0, getWidth(), getHeight())) {
        dirty.roundOut(&dirty);
        mLayer->updateDeferred(this, dirty.fLeft, dirty.fTop, dirty.fRight, dirty.fBottom);
    }
    ...
    
 传递的node是自己
    
	void Layer::updateDeferred(RenderNode* renderNode, int left, int top, int right, int bottom) {
	    requireRenderer();
	    this->renderNode = renderNode;
	    const Rect r(left, top, right, bottom);
	    dirtyRect.unionWith(r);
	    deferredUpdateScheduled = true;
	}

替换render

	void Layer::requireRenderer() {
	    // 创建renderer
	    if (!renderer) {
	        renderer.reset(new LayerRenderer(renderState, this));
	        renderer->initProperties();
	    }
	}

这里的prepareDirty可就变样子了

	void LayerRenderer::prepareDirty(float left, float top, float right, float bottom,
	        bool opaque) {
	    LAYER_RENDERER_LOGD("Rendering into layer, fbo = %d", mLayer->getFbo());
	
	    // 绑定Fbo 关键
	    mRenderState.bindFramebuffer(mLayer->getFbo());
	
	    const float width = mLayer->layer.getWidth();
	    const float height = mLayer->layer.getHeight();
	
	    Rect dirty(left, top, right, bottom);
	    if (dirty.isEmpty() || (dirty.left <= 0 && dirty.top <= 0 &&
	            dirty.right >= width && dirty.bottom >= height)) {
	        mLayer->region.clear();
	        dirty.set(0.0f, 0.0f, width, height);
	    } else {
	        dirty.intersect(0.0f, 0.0f, width, height);
	        android::Rect r(dirty.left, dirty.top, dirty.right, dirty.bottom);
	        mLayer->region.subtractSelf(r);
	    }
	    mLayer->clipRect.set(dirty);
	
	    OpenGLRenderer::prepareDirty(dirty.left, dirty.top, dirty.right, dirty.bottom, opaque);
	}

flush之前关键绑定自己Layer的fbo，绑定后，渲染目标就成了FBO，之后flush跟普通的类似，之后为这个Layer构建TextureVertex

	bool LayerRenderer::finish() {
	    bool retval = OpenGLRenderer::finish();
	
	    generateMesh();

	    return retval;
	}


	void LayerRenderer::generateMesh() {
	    if (mLayer->region.isRect() || mLayer->region.isEmpty()) {
	        if (mLayer->mesh) {
	            delete[] mLayer->mesh;
	            mLayer->mesh = nullptr;
	            mLayer->meshElementCount = 0;
	        }
	       <!--设置成Rect-->
	        mLayer->setRegionAsRect();
	        return;
	    }
	
	    // avoid T-junctions as they cause artifacts in between the resultant
	    // geometry when complex transforms occur.
	    // TODO: generate the safeRegion only if necessary based on drawing transform (see
	    // OpenGLRenderer::composeLayerRegion())
	    Region safeRegion = Region::createTJunctionFreeRegion(mLayer->region);
	
	    size_t count;
	    const android::Rect* rects = safeRegion.getArray(&count);
	
	    GLsizei elementCount = count * 6;
	
	    if (mLayer->mesh && mLayer->meshElementCount < elementCount) {
	        delete[] mLayer->mesh;
	        mLayer->mesh = nullptr;
	    }
	
	    if (!mLayer->mesh) {
	        mLayer->mesh = new TextureVertex[count * 4];
	    }
	    mLayer->meshElementCount = elementCount;
	
	    const float texX = 1.0f / float(mLayer->getWidth());
	    const float texY = 1.0f / float(mLayer->getHeight());
	    const float height = mLayer->layer.getHeight();
	
	    TextureVertex* mesh = mLayer->mesh;
	
		<!--处理数据-->
	    for (size_t i = 0; i < count; i++) {
	        const android::Rect* r = &rects[i];
	
	        const float u1 = r->left * texX;
	        const float v1 = (height - r->top) * texY;
	        const float u2 = r->right * texX;
	        const float v2 = (height - r->bottom) * texY;
	
	        TextureVertex::set(mesh++, r->left, r->top, u1, v1);
	        TextureVertex::set(mesh++, r->right, r->top, u2, v1);
	        TextureVertex::set(mesh++, r->left, r->bottom, u1, v2);
	        TextureVertex::set(mesh++, r->right, r->bottom, u2, v2);
	    }
	}

之前issueOperations

	/
		template <class T>
		void RenderNode::issueOperations(OpenGLRenderer& renderer, T& handler) {
		    if (mDisplayListData->isEmpty()) {
		        DISPLAY_LIST_LOGD("%*sEmpty display list (%p, %s)", handler.level() * 2, "",
		                this, getName());
		        return;
		    }
		   // renderer != mLayer->renderer.get() 到底是哪个render
		    // 这里是LayRender还是OpenGLRender，如果是LayerRender，那么是第一次，否则是第二次，
		    const bool drawLayer = (mLayer && (&renderer != mLayer->renderer.get()));
		    // If we are updating the contents of mLayer, we don't want to apply any of
		    // the RenderNode's properties to this issueOperations pass. Those will all
		    // be applied when the layer is drawn, aka when this is true.
		    const bool useViewProperties = (!mLayer || drawLayer);
		    if (useViewProperties) {
		        const Outline& outline = properties().getOutline();
		        if (properties().getAlpha() <= 0 || (outline.getShouldClip() && outline.isEmpty())) {
		            DISPLAY_LIST_LOGD("%*sRejected display list (%p, %s)", handler.level() * 2, "",
		                    this, getName());
		            return;
		        }
		    }
		
		    handler.startMark(getName());
		
		#if DEBUG_DISPLAY_LIST
		    const Rect& clipRect = renderer.getLocalClipBounds();
		    DISPLAY_LIST_LOGD("%*sStart display list (%p, %s), localClipBounds: %.0f, %.0f, %.0f, %.0f",
		            handler.level() * 2, "", this, getName(),
		            clipRect.left, clipRect.top, clipRect.right, clipRect.bottom);
		#endif
		
		    LinearAllocator& alloc = handler.allocator();
		    int restoreTo = renderer.getSaveCount();
		    handler(new (alloc) SaveOp(SkCanvas::kMatrix_SaveFlag | SkCanvas::kClip_SaveFlag),
		            PROPERTY_SAVECOUNT, properties().getClipToBounds());
		
		    DISPLAY_LIST_LOGD("%*sSave %d %d", (handler.level() + 1) * 2, "",
		            SkCanvas::kMatrix_SaveFlag | SkCanvas::kClip_SaveFlag, restoreTo);
		
		    if (useViewProperties) {
		        setViewProperties<T>(renderer, handler);
		    }
		
		    bool quickRejected = properties().getClipToBounds()
		            && renderer.quickRejectConservative(0, 0, properties().getWidth(), properties().getHeight());
		    if (!quickRejected) {
		        Matrix4 initialTransform(*(renderer.currentTransform()));
		        renderer.setBaseTransform(initialTransform);
		
		
		// 如果一个Render Node设置了Layer，那么就意味着这个Render Node的所有绘制命令都是作为一个整体进行执行的。
		//也就是说，对于设置了Layer的Render Node，我们首先需要将它的Display List的所有绘制命令合成一个整体的绘制命令，
		//目的就是为了得到一个FBO，然后渲染这个FBO就可以得一个Render Node的UI。
		
		// 对于设置了Layer的Render Node来说，它的成员函数defer会被调用两次。
		// 第一次调用的时候，就是为了将它的Display List的所有绘制命令合成一个FBO。
		// 第二次调用的时候，就是为了将合成后的FBO渲染到应用程序窗口的UI上。
		
		//        这时候RenderNode类的成员函数defer属于第一次执行。
		        //那么RenderNode类的成员函数issueOperations是如何区分它是被第一次调用的成员函数defer调用，
		        //还是第二次调用的成员函数defer调用呢？主要是通过比较参数renderer描述的OpenGLRender对象和成员变量mLayer指向的一个Layer对象的成员变量renderer描述折一个OpenGLRender对象来区分。
		        //如果这两个OpenGLRenderer对象是同一个，就意味着是被第一次调用的成员函数defer调用；否则的话，
		        //就是被第二次调用的成员函数defer调用。
		
		//        当RenderNode类的成员函数issueOperations是被第二次调用的成员函数defer调用的时候，
		        //该Render Node的Display List的所有绘制命令已经被合成在一个FBO里面，
		        //并且这个FBO是由它所关联的Layer对象维护的，
		        //因此这时候只需要将该Layer对象封装成一个DrawLayerOp交给参数
		        //handler描述的一个DeferOperationHandler对象处理即可。
		
		//        我们再确认一下现在RenderNode类的成员函数issueOperations是被第一次调用的成员函数defer调用。
		        //它的参数renderer指向的一个OpenGLRenderer对象是从Layer类的成员函数defer传递进行的，
		        //而Layer类的成员函数defer传递进行的这个OpenGLRenderer对象就正好是与Render Node关联的Layer对象的成员变量renderer描述折一个OpenGLRender对象，因此它们就是相同的。从前面的分析可以知道，这个OpenGLRenderer对象的实际类型是LayerRenderer。
		
		//        后面我们会看到，当Render Node的成员函数issueOperations是被第二次调用的成员函数defer调用的时候，
		        //它的参数renderer指向的一个OpenGLRenderer对象的实际类型就是OpenGLRenderer，
		        //它与当前正在处理的Render Node关联的Layer对象的成员变量描述折一个OpenGLRender对象不可能是相同的，
		        //因为后者的实际类型是LayerRenderer。
		
		//        接下来我们就继续分析RenderNode类的成员函数issueOperations是被第一次调用的成员函数defer调用时的执行情况，这时候得到的本地变量drawLayer的值为false。
		
		        // drawLayer
		        if (drawLayer) {
		            // 第二次 FBO已经渲染完毕，构造一个新的即可
		            handler(new (alloc) DrawLayerOp(mLayer, 0, 0),
		                    renderer.getSaveCount() - 1, properties().getClipToBounds());
		        } else {
		            // 第一次
		            const int saveCountOffset = renderer.getSaveCount() - 1;
		            const int projectionReceiveIndex = mDisplayListData->projectionReceiveIndex;
		            for (size_t chunkIndex = 0; chunkIndex < mDisplayListData->getChunks().size(); chunkIndex++) {
		                const DisplayListData::Chunk& chunk = mDisplayListData->getChunks()[chunkIndex];
		
		                Vector<ZDrawRenderNodeOpPair> zTranslatedNodes;
		                buildZSortedChildList(chunk, zTranslatedNodes);
		
		                issueOperationsOf3dChildren(kNegativeZChildren,
		                        initialTransform, zTranslatedNodes, renderer, handler);
		
		
		                for (size_t opIndex = chunk.beginOpIndex; opIndex < chunk.endOpIndex; opIndex++) {
		                    DisplayListOp *op = mDisplayListData->displayListOps[opIndex];
		#if DEBUG_DISPLAY_LIST
		                    op->output(handler.level() + 1);
		#endif
		                    handler(op, saveCountOffset, properties().getClipToBounds());
		
		                    if (CC_UNLIKELY(!mProjectedNodes.isEmpty() && projectionReceiveIndex >= 0 &&
		                        opIndex == static_cast<size_t>(projectionReceiveIndex))) {
		                        issueOperationsOfProjectedChildren(renderer, handler);
		                    }
		                }
		
		                issueOperationsOf3dChildren(kPositiveZChildren,
		                        initialTransform, zTranslatedNodes, renderer, handler);
		            }
		        }
		    }
		
		    DISPLAY_LIST_LOGD("%*sRestoreToCount %d", (handler.level() + 1) * 2, "", restoreTo);
		    handler(new (alloc) RestoreToCountOp(restoreTo),
		            PROPERTY_SAVECOUNT, properties().getClipToBounds());
		
		    DISPLAY_LIST_LOGD("%*sDone (%p, %s)", handler.level() * 2, "", this, getName());
		    handler.endMark();
		}
	

最终会调用OpenGL的DrawLayerOp

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

假设不defer，
	
	void Layer::render(const OpenGLRenderer& rootRenderer) {
	    ATRACE_LAYER_WORK("Direct-Issue");
	
	    updateLightPosFromRenderer(rootRenderer);
	    renderer->setViewport(layer.getWidth(), layer.getHeight());
	    renderer->prepareDirty(dirtyRect.left, dirtyRect.top, dirtyRect.right, dirtyRect.bottom,
	            !isBlend());
	
	    renderer->drawRenderNode(renderNode.get(), dirtyRect, RenderNode::kReplayFlag_ClipChildren);
	
	    renderer->finish();
	
	    dirtyRect.setEmpty();
	
	    deferredUpdateScheduled = false;
	    renderNode = nullptr;
	}



可以看到，最后生成的是一个mesh，存好，Layer，Layer所有的 东西存储在Mesh中，将来DrawLayerOp的时候，一并处理完成，注意FBO只会关联，本身没有存储，将来再次使用DrawLayerOp绘制到FrameBuffer中的时候，会使用相应fbo中的texture



	GlopBuilder& GlopBuilder::setFillTextureLayer(Layer& layer, float alpha) {
	    TRIGGER_STAGE(kFillStage);
	    REQUIRE_STAGES(kMeshStage | kRoundRectClipStage);
	
	    mOutGlop->fill.texture = { &(layer.getTexture()),
	            layer.getRenderTarget(), GL_LINEAR, GL_CLAMP_TO_EDGE, &layer.getTexTransform() };
	    mOutGlop->fill.color = { alpha, alpha, alpha, alpha };
	
	    setFill(SK_ColorWHITE, alpha, layer.getMode(), Blend::ModeOrderSwap::NoSwap,
	            nullptr, layer.getColorFilter());
	
	    mDescription.modulate = mOutGlop->fill.color.a < 1.0f;
	    mDescription.hasTextureTransform = true;
	    return *this;
	}

到这里，Layer的使用就完成了，之前是构建Mesh，

	void OpenGLRenderer::drawTextureLayer(Layer* layer, const Rect& rect) {
	    const bool tryToSnap = !layer->getForceFilter()
	            && layer->getWidth() == (uint32_t) rect.getWidth()
	            && layer->getHeight() == (uint32_t) rect.getHeight();
	    Glop glop;
	    GlopBuilder(mRenderState, mCaches, &glop)
	            .setRoundRectClipState(currentSnapshot()->roundRectClipState)
	            .setMeshTexturedUvQuad(nullptr, Rect(0, 1, 1, 0)) // TODO: simplify with VBO
	            .setFillTextureLayer(*layer, getLayerAlpha(layer))
	            .setTransform(*currentSnapshot(), TransformFlags::None)
	            .setModelViewMapUnitToRectOptionalSnap(tryToSnap, rect)
	            .build();
	    renderGlop(glop);
	}



在OpenGL扩展中，GL_EXT_framebuffer_object提供了一种创建额外的不能显示的帧缓存对象的接口。为了和默认的“window系统生成”的帧缓存区别，这种帧缓冲成为应用程序帧缓存（application-createdframebuffer）。通过使用帧缓存对象（FBO），OpenGL可以将显示输出到引用程序帧缓存对象，而不是传统的“window系统生成”帧缓存。而且，它完全受OpenGL控制。

相似于window系统提供的帧缓存，一个FBO也包含一些存储颜色、深度和模板数据的区域。（注意：没有累积缓存）我们把FBO中这些逻辑缓存称之为“帧缓存关联图像”，它们是一些能够和一个帧缓存对象关联起来的二维数组像素。

有两种类型的“帧缓存关联图像”：纹理图像（texture images）和渲染缓存图像（renderbuffer images）。如果纹理对象的图像数据关联到帧缓存，OpenGL执行的是“渲染到纹理”（render to texture）操作。如果渲染缓存的图像数据关联到帧缓存，OpenGL执行的是离线渲染（offscreen rendering）。

**FBO本身并没有任何图像存储区，只有多个关联点。**FBO提供了一种高效的切换机制；将前面的帧缓存关联图像从FBO分离，然后把新的帧缓存关联图像关联到FBO。在帧缓存关联图像之间切换比在FBO之间切换要快得多。FBO提供了glFramebufferTexture2DEXT()来切换2D纹理对象和glFramebufferRenderbufferEXT()来切换渲染缓存对象。


一旦一个FBO被创建，在使用它之前必须绑定。

void glBindFramebufferEXT(GLenum target, GLuint id)

第一个参数target应该是GL_FRAMEBUFFER_EXT，第二个参数是FBO的ID号。一旦FBO被绑定，之后的所有的OpenGL操作都会对当前所绑定的FBO造成影响。ID号为0表示缺省帧缓存，即默认的window提供的帧缓存。因此，在glBindFramebufferEXT()中将ID号设置为0可以解绑定当前FBO。

FBO本身没有图像存储区。我们必须帧缓存关联图像（纹理或渲染对象）关联到FBO。这种机制允许FBO快速地切换（分离和关联）帧缓存关联图像。切换帧缓存关联图像比在FBO之间切换要快得多。而且，它节省了不必要的数据拷贝和内存消耗。比如，一个纹理可以被关联到多个FBO上，图像存储区可以被多个FBO共享。


# 纹理对象就会被附加上纹理图像

生成了纹理和相应的多级渐远纹理后，释放图像的内存并解绑纹理对象是一个很好的习惯。

SOIL_free_image_data(image);
glBindTexture(GL_TEXTURE_2D, 0);

纹理中有自己的备份

OpenGLRenderer::drawLayer跟flushLayer的区别，TetureView需要DrawLayerOp，

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
	
	
# 	内存的使用跟绑定EglImage

EGLImage代表一种由EGL客户API（如OpenGL，OpenVG）创建的共享资源类型。它的本意是共享2D图像数据，但是并没有明确限定共享数据的格式以及共享的目的，所以理论上来讲，应用程序以及相关的客户API可以基于任意的目的创建任意类型的共享数据。



	    // mCurrentTextureImage is the EglImage/buffer of the current texture. It's
    // possible that this buffer is not associated with any buffer slot, so we
    // must track it separately in order to support the getCurrentBuffer method.
    sp<EglImage> mCurrentTextureImage;


	   class EglImage : public LightRefBase<EglImage>  {
	    public:
	        EglImage(sp<GraphicBuffer> graphicBuffer);
	
	        // createIfNeeded creates an EGLImage if required (we haven't created
	        // one yet, or the EGLDisplay or crop-rect has changed).
	        status_t createIfNeeded(EGLDisplay display,
	                                const Rect& cropRect,
	                                bool forceCreate = false);
	
	        // This calls glEGLImageTargetTexture2DOES to bind the image to the
	        // texture in the specified texture target.
	        void bindToTextureTarget(uint32_t texTarget);
	
	        const sp<GraphicBuffer>& graphicBuffer() { return mGraphicBuffer; }
	        const native_handle* graphicBufferHandle() {
	            return mGraphicBuffer == NULL ? NULL : mGraphicBuffer->handle;
	        }
	
	    private:
	        // Only allow instantiation using ref counting.
	        friend class LightRefBase<EglImage>;
	        virtual ~EglImage();
	
	        // createImage creates a new EGLImage from a GraphicBuffer.
	        EGLImageKHR createImage(EGLDisplay dpy,
	                const sp<GraphicBuffer>& graphicBuffer, const Rect& crop);
	
	        // Disallow copying
	        EglImage(const EglImage& rhs);
	        void operator = (const EglImage& rhs);
	
	        // mGraphicBuffer is the buffer that was used to create this image.
	        sp<GraphicBuffer> mGraphicBuffer;
	
	        // mEglImage is the EGLImage created from mGraphicBuffer.
	        EGLImageKHR mEglImage;
	
	        // mEGLDisplay is the EGLDisplay that was used to create mEglImage.
	        EGLDisplay mEglDisplay;
	
	        // mCropRect is the crop rectangle passed to EGL when mEglImage
	        // was created.
	        Rect mCropRect;
	    };



	status_t GLConsumer::EglImage::createIfNeeded(EGLDisplay eglDisplay,
	                                              const Rect& cropRect,
	                                              bool forceCreation) {
	    // If there's an image and it's no longer valid, destroy it.
	    bool haveImage = mEglImage != EGL_NO_IMAGE_KHR;
	    bool displayInvalid = mEglDisplay != eglDisplay;
	    bool cropInvalid = hasEglAndroidImageCrop() && mCropRect != cropRect;
	    if (haveImage && (displayInvalid || cropInvalid || forceCreation)) {
	        if (!eglDestroyImageKHR(mEglDisplay, mEglImage)) {
	           ALOGE("createIfNeeded: eglDestroyImageKHR failed");
	        }
	        eglTerminate(mEglDisplay);
	        mEglImage = EGL_NO_IMAGE_KHR;
	        mEglDisplay = EGL_NO_DISPLAY;
	    }
	
	    // If there's no image, create one.
	    if (mEglImage == EGL_NO_IMAGE_KHR) {
	        mEglDisplay = eglDisplay;
	        mCropRect = cropRect;
	        mEglImage = createImage(mEglDisplay, mGraphicBuffer, mCropRect);
	    }
	
	    // Fail if we can't create a valid image.
	    if (mEglImage == EGL_NO_IMAGE_KHR) {
	        mEglDisplay = EGL_NO_DISPLAY;
	        mCropRect.makeInvalid();
	        const sp<GraphicBuffer>& buffer = mGraphicBuffer;
	        ALOGE("Failed to create image. size=%ux%u st=%u usage=%#" PRIx64 " fmt=%d",
	            buffer->getWidth(), buffer->getHeight(), buffer->getStride(),
	            buffer->getUsage(), buffer->getPixelFormat());
	        return UNKNOWN_ERROR;
	    }
	
	    return OK;
	}

	
	status_t GLConsumer::bindTextureImageLocked() {
	    if (mEglDisplay == EGL_NO_DISPLAY) {
	        ALOGE("bindTextureImage: invalid display");
	        return INVALID_OPERATION;
	    }
	
	    GLenum error;
	    while ((error = glGetError()) != GL_NO_ERROR) {
	        GLC_LOGW("bindTextureImage: clearing GL error: %#04x", error);
	    }
	  // 设定挡墙的Texture，这里就已经他妈的获取了，还
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
	    // 通知的时候，就已经绑定了
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
	        // 通知的时候绑定了，当时如何共享到当前teture id
	        mCurrentTextureImage->bindToTextureTarget(mTexTarget);
	        if ((error = glGetError()) != GL_NO_ERROR) {
	            GLC_LOGE("bindTextureImage: error binding external image: %#04x", error);
	            return UNKNOWN_ERROR;
	        }
	    }
	
	    // Wait for the new buffer to be ready.
	    return doGLFenceWaitLocked();
	}




	void GLConsumer::EglImage::bindToTextureTarget(uint32_t texTarget) {
	    glEGLImageTargetTexture2DOES(texTarget,
	            static_cast<GLeglImageOES>(mEglImage));
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


SurfaceTeture  用的是GL_TEXTURE_EXTERNAL_OES

# 8.0之后的渲染有了更多的选项

	CanvasContext* CanvasContext::create(RenderThread& thread,
	        bool translucent, RenderNode* rootRenderNode, IContextFactory* contextFactory) {
	
	    auto renderType = Properties::getRenderPipelineType();
	
	    switch (renderType) {
	        case RenderPipelineType::OpenGL:
	            return new CanvasContext(thread, translucent, rootRenderNode, contextFactory,
	                    std::make_unique<OpenGLPipeline>(thread));
	        case RenderPipelineType::SkiaGL:
	            return new CanvasContext(thread, translucent, rootRenderNode, contextFactory,
	                    std::make_unique<skiapipeline::SkiaOpenGLPipeline>(thread));
	        case RenderPipelineType::SkiaVulkan:
	            return new CanvasContext(thread, translucent, rootRenderNode, contextFactory,
	                                std::make_unique<skiapipeline::SkiaVulkanPipeline>(thread));
	        default:
	            LOG_ALWAYS_FATAL("canvas context type %d not supported", (int32_t) renderType);
	            break;
	    }
	    return nullptr;
	}

#     参考文档

[Android BufferQueue简析](https://www.jianshu.com/p/edd7d264be73)           
[不错的demo GraphicsTestBed ](https://github.com/lb377463323/GraphicsTestBed)        
[小窗播放视频的原理和实现（上）](http://www.10tiao.com/html/223/201712/2651232830/1.html)         
[GLTextureViewActivity.java ](https://android.googlesource.com/platform/frameworks/base/+/master/tests/HwAccelerationTest/src/com/android/test/hwui/GLTextureViewActivity.java)             
[OpenGL】OpenGL帧缓存对象(FBO：Frame Buffer Object)](https://blog.csdn.net/xiajun07061225/article/details/7283929)                
[原EGLImage与纹理](https://blog.csdn.net/fuyajun01/article/details/8940687)