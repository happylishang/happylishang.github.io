
### UI刷新流程示意

以Textview setText刷新为例，基本流程如下

![image.png](https://upload-images.jianshu.io/upload_images/1460468-311b22120397333b.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

设置了软件的话，就是软件绘制 Canvas是普通Canvas

    @NonNull
    public RenderNode updateDisplayListIfDirty() {
    			<!--封装成硬件加速的drawBitmap-->
            try {
                if (layerType == LAYER_TYPE_SOFTWARE) {
                    buildDrawingCache(true);
                    Bitmap cache = getDrawingCache(true);
                    if (cache != null) {
                        canvas.drawBitmap(cache, 0, 0, mLayerPaint);
                    }
                } 
                
 构建普通Canvas
 
    private void buildDrawingCacheImpl(boolean autoScale) {       
           。。。
      Canvas canvas;
        if (attachInfo != null) {
            canvas = attachInfo.mCanvas;
            if (canvas == null) {
                canvas = new Canvas();
            }
            canvas.setBitmap(bitmap);
        } else {
            canvas = new Canvas(bitmap);
        }

       ...
        } else {
            draw(canvas);
        }

        canvas.restoreToCount(restoreCount);
        canvas.setBitmap(null);

        if (attachInfo != null) {
            // Restore the cached Canvas for our siblings
            attachInfo.mCanvas = canvas;
        }
    }
    
    
### UI局部重绘

某一个View重绘刷新，并不会导致所有View都进行一次measure、layout、draw，可能只是这个待刷新View链路需要调整，那么剩余的View就不需要浪费精力再来一遍，反应再APP侧就是：**不需要再次调用updateDisplayListIfDirty构建RenderNode渲染Op树**

	    public RenderNode updateDisplayListIfDirty() {
	        final RenderNode renderNode = mRenderNode;
			  ...
	        if ((mPrivateFlags & PFLAG_DRAWING_CACHE_VALID) == 0
	                || !renderNode.isValid()
	                || (mRecreateDisplayList)) {
	           <!--失效了，需要重绘-->
	        } else {
	        <!--依旧有效，无需重绘-->
	            mPrivateFlags |= PFLAG_DRAWN | PFLAG_DRAWING_CACHE_VALID;
	            mPrivateFlags &= ~PFLAG_DIRTY_MASK;
	        }
	        return renderNode;
	    }

    