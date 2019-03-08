Canvas：默认是不支持硬件上加速 如果Activity禁止硬件加速得到的是ComptableCanvas 其实就是Canvas对应到Cpp就是 SkiaCanvas

    public Canvas() {
        if (!isHardwareAccelerated()) {
            // 0 means no native bitmap
            mNativeCanvasWrapper = initRaster(null);
            mFinalizer = new CanvasFinalizer(mNativeCanvasWrapper);
        } else {
            mFinalizer = null;
        }
    }


	// Native wrapper constructor used by Canvas(Bitmap)
	static jlong initRaster(JNIEnv* env, jobject, jobject jbitmap) {
	    SkBitmap bitmap;
	    if (jbitmap != NULL) {
	        GraphicsJNI::getSkBitmap(env, jbitmap, &bitmap);
	    }
	    return reinterpret_cast<jlong>(Canvas::create_canvas(bitmap));
	}

>  何为硬件加速：不是一帧，而是一个图层的绘制是CPU还是GPU来实现

![image.png](https://upload-images.jianshu.io/upload_images/1460468-5a1c9581538cc306.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

 图层的合成，也就是一帧图像的生成必然是通过GPU合成来实现的，必然是硬件加速的。
 
 SurfaceView中Canvas是不支持硬件加速的
 
            Canvas canvas = mSurfaceHolder.lockCanvas();//获取画布
                            这里是false
            canvas.isHardwareAccelerated()
                
而对于普通的自定义View

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        Log.v(this.getClass().getSimpleName(),""+canvas.isHardwareAccelerated());
    }
    
这里返回的是true，是支持硬件加速的，其实是两个图层，那么SurfaceView为什么要附着在一个已有窗口中呢？应该是为了管理，同时配合界面的生命周期。

SurfaceView用的时候有几种场景，第一种：利用SurfaceView获得Canvas之后，自己绘图，注意，这里绘图可以看做是软件绘制，没有硬件加速，何为硬件加速，就是绘制的逻辑是通过CPU还是GPU来实现的，SF图层合成一定是GPU，但是每一个图层的绘制不一定是GPU，

视频播放、拍照、录像都常会用到SurfaceView，SurfaceView其本质是什么，为什么说SurfaceView拥有独立的绘图表面，为什么能在子线程中绘制更新，并且SurfaceView是如何支持数据流的时时展示的呢？本文就简单分析下

* SurfaceView拥有独立的绘图表面是什么意思，如何实现的（独立的试图的内存）
* SurfaceView为何能在子线程更新UI
* 视频播放或者拍照预览的数据如何时时更新到屏幕上
* SurfaceView跟窗口管理的关系






# 拍照跟普通lockCanvas有什么不同

# SurfaceView跟窗口管理的关系

>SurfaceView.java 窗口类型

    int mWindowType = WindowManager.LayoutParams.TYPE_APPLICATION_MEDIA;

>WindowManager.java
    
      public static final int TYPE_APPLICATION_MEDIA = FIRST_SUB_WINDOW + 1;

可以看到，SurfaceView的窗口类型是TYPE_APPLICATION_MEDIA，本身就是为多媒体窗口，并且，也是一个子窗口，隶属其父窗口。
        
如何挖洞，从自己当前层开始，一直挖穿父窗口，而在自己层上面的不会被挖，会覆盖自己

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mParent.requestTransparentRegion(this);
        mSession = getWindowSession();
        mLayout.token = getWindowToken();
        mLayout.setTitle("SurfaceView");
        mViewVisibility = getVisibility() == VISIBLE;

        if (!mGlobalListenersAdded) {
            ViewTreeObserver observer = getViewTreeObserver();
            observer.addOnScrollChangedListener(mScrollChangedListener);
            observer.addOnPreDrawListener(mDrawListener);
            mGlobalListenersAdded = true;
        }
    }

注意：挖洞并不是SurfaceView独有的，只是一个应用

	public class SurfaceView extends View {
	    @Override
	    public boolean gatherTransparentRegion(Region region) {
	        if (mWindowType == WindowManager.LayoutParams.TYPE_APPLICATION_PANEL) {
	            return super.gatherTransparentRegion(region);
	        }
	        boolean opaque = true;
	        if ((mPrivateFlags & SKIP_DRAW) == 0) {
	            // this view draws, remove it from the transparent region
	            opaque = super.gatherTransparentRegion(region);
	        } else if (region != null) {
	            int w = getWidth();
	            int h = getHeight();
	            if (w>0 && h>0) {
	                getLocationInWindow(mLocation);
	                int l = mLocation[0];
	                int t = mLocation[1];
	                region.op(l, t, l+w, t+h, Region.Op.UNION);
	            }
	        }
	        if (PixelFormat.formatHasAlpha(mRequestedFormat)) {
	            opaque = false;
	        }
	        return opaque;
	    }
	    ……
	}

    
透明区域
    
    @Override
    public void requestTransparentRegion(View child) {
        // the test below should not fail unless someone is messing with us
        checkThread();
        if (mView == child) {
            mView.mPrivateFlags |= View.PFLAG_REQUEST_TRANSPARENT_REGIONS;
            // Need to make sure we re-evaluate the window attributes next
            // time around, to ensure the window has the correct format.
            mWindowAttributesChanged = true;
            mWindowAttributesChangesFlag = 0;
            requestLayout();
        }
    }
    
        @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        mWindowVisibility = visibility == VISIBLE;
        mRequestedVisible = mWindowVisibility && mViewVisibility;
        updateWindow(false, false);
    }
 
# SurfaceView可能有不同的数据来源

当一个SurfaceView的绘图表面的类型等于SURFACE_TYPE_NORMAL的时候，就表示该SurfaceView的绘图表面所使用的内存是一块普通的内存。一般来说，这块内存是由SurfaceFlinger服务来分配的，我们可以在应用程序内部自由地访问它，即可以在它上面填充任意的UI数据，然后交给SurfaceFlinger服务来合成，并且显示在屏幕上。在这种情况下，SurfaceFlinger服务使用一个Layer对象来描述该SurfaceView的绘图表面。

当一个SurfaceView的绘图表面的类型等于SURFACE_TYPE_PUSH_BUFFERS的时候，就表示该SurfaceView的绘图表面所使用的内存不是由SurfaceFlinger服务分配的，因而我们不能够在应用程序内部对它进行操作。例如，当一个SurfaceView是用来显示摄像头预览或者视频播放的时候，我们就会将它的绘图表面的类型设置为SURFACE_TYPE_PUSH_BUFFERS，这样摄像头服务或者视频播放服务就会为该SurfaceView绘图表面创建一块内存，并且将采集的预览图像数据或者视频帧数据源源不断地填充到该内存中去。注意，这块内存有可能是来自专用的硬件的，例如，它可能是来自视频卡的。在这种情况下，SurfaceFlinger服务使用一个LayerBuffer对象来描述该SurfaceView的绘图表面。**（不过最终还是收SF管理，分配）**

从上面的描述就得到一个重要的结论：绘图表面类型为SURFACE_TYPE_PUSH_BUFFERS的SurfaceView的UI是不能由应用程序来控制的，而是由专门的服务来控制的，例如，摄像头服务或者视频播放服务，同时，SurfaceFlinger服务会使用一种特殊的LayerBuffer来描述这种绘图表面。使用LayerBuffer来描述的绘图表面在进行渲染的时候，可以使用硬件加速，例如，使用copybit或者overlay来加快渲染速度，从而可以获得更流畅的摄像头预览或者视频播放。）
        

Session类的成员函数addWithoutInputChannel只是在WindowManagerService服务内部为指定的窗口增加一个WindowState对象，而Session类的成员函数add除了会在WindowManagerService服务内部为指定的窗口增加一个WindowState对象之外，还会为该窗口创建一个用来接收用户输入的通道，具体可以参考Android应用程序键盘（Keyboard）消息处理机制分析一文。

    
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
 

#     参考文档

[Android BufferQueue简析](https://www.jianshu.com/p/edd7d264be73)         
[Android视图SurfaceView的实现原理分析](https://blog.csdn.net/Luoshengyang/article/details/8661317)