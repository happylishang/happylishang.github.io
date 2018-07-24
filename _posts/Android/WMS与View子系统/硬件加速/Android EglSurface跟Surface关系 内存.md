

其实对于Surface，最关键的就是一个IGraphicBufferProducer，他是一个生产者，GraphicBuffer，deque eque 
  GraphicBuffer的管理，Android也设计了一套机制：BufferQueue，作为SurfaceFlinger管理和消费surface的中介
 
 FREE->DEQUEUED->QUEUED->ACQUIRED->FREE这个过程


EGLSurface 

EGL14.java

	public static EGLSurface eglCreateWindowSurface(EGLDisplay dpy,EGLConfig config,    Object win,    int[] attrib_list,    int offset){
	    Surface sur = null;
	    if (win instanceof SurfaceView) {
	        SurfaceView surfaceView = (SurfaceView)win;
	        sur = surfaceView.getHolder().getSurface();
	    } else if (win instanceof SurfaceHolder) {
	        SurfaceHolder holder = (SurfaceHolder)win;
	        sur = holder.getSurface();
	    } else if (win instanceof Surface) {
	        sur = (Surface) win;
	    }
	
	    EGLSurface surface;
	    if (sur != null) {
	        surface = _eglCreateWindowSurface(dpy, config, sur, attrib_list, offset);
	    } else if (win instanceof SurfaceTexture) {
	        surface = _eglCreateWindowSurfaceTexture(dpy, config,
	                win, attrib_list, offset);
	    } 
	
	    return surface;
	}

native
	
	/* EGLSurface eglCreateWindowSurface ( EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list ) */
	static jobject
	android_eglCreateWindowSurface
	  (JNIEnv *_env, jobject _this, jobject dpy, jobject config, jobject win, jintArray attrib_list_ref, jint offset) {
	    jint _exception = 0;
	    const char * _exceptionType = "";
	    const char * _exceptionMessage = "";
	    // 创建EGLSurface
	    EGLSurface _returnValue = (EGLSurface) 0;
	    // display
	    EGLDisplay dpy_native = (EGLDisplay) fromEGLHandle(_env, egldisplayGetHandleID, dpy);
	    // config
	    EGLConfig config_native = (EGLConfig) fromEGLHandle(_env, eglconfigGetHandleID, config);
	    int attrib_list_sentinel = 0;
	    EGLint *attrib_list_base = (EGLint *) 0;
	    jint _remaining;
	    EGLint *attrib_list = (EGLint *) 0;
	    // ANativeWindow
	    android::sp<ANativeWindow> window;
	
	    if (!attrib_list_ref) {
	        _exception = 1;
	        _exceptionType = "java/lang/IllegalArgumentException";
	        _exceptionMessage = "attrib_list == null";
	        goto exit;
	    }
	    if (offset < 0) {
	        _exception = 1;
	        _exceptionType = "java/lang/IllegalArgumentException";
	        _exceptionMessage = "offset < 0";
	        goto exit;
	    }
	    if (win == NULL) {
	not_valid_surface:
	        _exception = 1;
	        _exceptionType = "java/lang/IllegalArgumentException";
	        _exceptionMessage = "Make sure the SurfaceView or associated SurfaceHolder has a valid Surface";
	        goto exit;
	    }
	
	    window = android::android_view_Surface_getNativeWindow(_env, win);
	
	    if (window == NULL)
	        goto not_valid_surface;
	
	    _remaining = _env->GetArrayLength(attrib_list_ref) - offset;
	    attrib_list_base = (EGLint *)
	        _env->GetIntArrayElements(attrib_list_ref, (jboolean *)0);
	    attrib_list = attrib_list_base + offset;
	    attrib_list_sentinel = 0;
	    for (int i = _remaining - 1; i >= 0; i--)  {
	        if (*((EGLint*)(attrib_list + i)) == EGL_NONE){
	            attrib_list_sentinel = 1;
	            break;
	        }
	    }
	    if (attrib_list_sentinel == 0) {
	        _exception = 1;
	        _exceptionType = "java/lang/IllegalArgumentException";
	        _exceptionMessage = "attrib_list must contain EGL_NONE!";
	        goto exit;
	    }
	
	    // eglCreateWindowSurface
	    _returnValue = eglCreateWindowSurface(
	        (EGLDisplay)dpy_native,
	        (EGLConfig)config_native,
	        (EGLNativeWindowType)window.get(),
	        (EGLint *)attrib_list
	    );
	
	exit:
	    if (attrib_list_base) {
	        _env->ReleaseIntArrayElements(attrib_list_ref, attrib_list_base,
	            JNI_ABORT);
	    }
	    if (_exception) {
	        jniThrowException(_env, _exceptionType, _exceptionMessage);
	    }
	    return toEGLHandle(_env, eglsurfaceClass, eglsurfaceConstructor, _returnValue);
	}

>egl.cpp
	
	static EGLSurface createWindowSurface(EGLDisplay dpy, EGLConfig config,
	        NativeWindowType window, const EGLint* /*attrib_list*/)
	{
	    if (egl_display_t::is_valid(dpy) == EGL_FALSE)
	        return setError(EGL_BAD_DISPLAY, EGL_NO_SURFACE);
	    if (window == 0)
	        return setError(EGL_BAD_MATCH, EGL_NO_SURFACE);
	
	    EGLint surfaceType;
	    if (getConfigAttrib(dpy, config, EGL_SURFACE_TYPE, &surfaceType) == EGL_FALSE)
	        return EGL_FALSE;
	
	    if (!(surfaceType & EGL_WINDOW_BIT))
	        return setError(EGL_BAD_MATCH, EGL_NO_SURFACE);
	
	    if (static_cast<ANativeWindow*>(window)->common.magic !=
	            ANDROID_NATIVE_WINDOW_MAGIC) {
	        return setError(EGL_BAD_NATIVE_WINDOW, EGL_NO_SURFACE);
	    }
	        
	    EGLint configID;
	    if (getConfigAttrib(dpy, config, EGL_CONFIG_ID, &configID) == EGL_FALSE)
	        return EGL_FALSE;
	
	    int32_t depthFormat;
	    int32_t pixelFormat;
	    if (getConfigFormatInfo(configID, pixelFormat, depthFormat) != NO_ERROR) {
	        return setError(EGL_BAD_MATCH, EGL_NO_SURFACE);
	    }
	
	    // FIXME: we don't have access to the pixelFormat here just yet.
	    // (it's possible that the surface is not fully initialized)
	    // maybe this should be done after the page-flip
	    //if (EGLint(info.format) != pixelFormat)
	    //    return setError(EGL_BAD_MATCH, EGL_NO_SURFACE);
	
	    egl_surface_t* surface;
	    surface = new egl_window_surface_v2_t(dpy, config, depthFormat,
	            static_cast<ANativeWindow*>(window));
	
	    if (!surface->initCheck()) {
	        // there was a problem in the ctor, the error
	        // flag has been set.
	        delete surface;
	        surface = 0;
	    }
	    return surface;
	}

egl_window_surface_v2_t	

	egl_window_surface_v2_t::egl_window_surface_v2_t(EGLDisplay dpy,
	        EGLConfig config,
	        int32_t depthFormat,
	        ANativeWindow* window)
	    : egl_surface_t(dpy, config, depthFormat), 
	    nativeWindow(window), buffer(0), previousBuffer(0), module(0),
	    bits(NULL)
	{
	    hw_module_t const* pModule;
	    hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &pModule);
	    module = reinterpret_cast<gralloc_module_t const*>(pModule);
	
	    pixelFormatTable = gglGetPixelFormatTable();
	    
	    // keep a reference on the window
	    nativeWindow->common.incRef(&nativeWindow->common);
	    nativeWindow->query(nativeWindow, NATIVE_WINDOW_WIDTH, &width);
	    nativeWindow->query(nativeWindow, NATIVE_WINDOW_HEIGHT, &height);
	}
	
	


#     参考文档
[原Android中native_handle private_handle_t ANativeWindowBuffer ANativeWindow GraphicBuffer Surface的关系](https://blog.csdn.net/ear5cm/article/details/45458683)           
[Android SurfaceView中的Surface，openGL es中 EGLDisplay,EGLConfig,EGLContext,EGLSurface](https://blog.csdn.net/jamesshaoya/article/details/53310856)            
[Android BufferQueue简析](https://www.jianshu.com/p/edd7d264be73)           
[不错的demo GraphicsTestBed ](https://github.com/lb377463323/GraphicsTestBed)        
[小窗播放视频的原理和实现（上）](http://www.10tiao.com/html/223/201712/2651232830/1.html)         
[GLTextureViewActivity.java ](https://android.googlesource.com/platform/frameworks/base/+/master/tests/HwAccelerationTest/src/com/android/test/hwui/GLTextureViewActivity.java)              
[Android SurfaceFlinger 学习之路(七)----创建图形缓冲区GraphicBuffer](http://windrunnerlihuan.com/2017/06/22/Android-SurfaceFlinger-%E5%AD%A6%E4%B9%A0%E4%B9%8B%E8%B7%AF-%E4%B8%83-%E5%88%9B%E5%BB%BA%E5%9B%BE%E5%BD%A2%E7%BC%93%E5%86%B2%E5%8C%BAGraphicBuffer/)