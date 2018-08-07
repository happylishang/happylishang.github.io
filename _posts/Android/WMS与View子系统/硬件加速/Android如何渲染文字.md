
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
