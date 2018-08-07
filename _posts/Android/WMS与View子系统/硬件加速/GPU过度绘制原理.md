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

