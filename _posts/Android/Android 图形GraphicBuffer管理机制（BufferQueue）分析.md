GraphicBuffer跟ANativeWindowBuffer的关系，

	class GraphicBuffer
	    : public ANativeObjectBase< ANativeWindowBuffer, GraphicBuffer, RefBase >,
	      public Flattenable<GraphicBuffer>
	{
	
	template <typename NATIVE_TYPE, typename TYPE, typename REF>
	class ANativeObjectBase : public NATIVE_TYPE, public REF
	{

GraphicBuffer 是ANativeWindowBuffer的子类，GraphicBuffer就是ANativeWindowBuffer一种具体实现,把ANativeWindowBuffer的common成员的两个函数指针incRef decRef指向了GraphicBuffer的另一个基类RefBase的incStrong和decStrong,而ANativeWindowBuffer无非就是把buffer_handle_t包了一层.
	
	typedef struct ANativeWindowBuffer
	{
	#ifdef __cplusplus
	    ANativeWindowBuffer() {
	        common.magic = ANDROID_NATIVE_BUFFER_MAGIC;
	        common.version = sizeof(ANativeWindowBuffer);
	        memset(common.reserved, 0, sizeof(common.reserved));
	    }
	
	    // Implement the methods that sp<ANativeWindowBuffer> expects so that it
	    // can be used to automatically refcount ANativeWindowBuffer's.
	    void incStrong(const void* /*id*/) const {
	        common.incRef(const_cast<android_native_base_t*>(&common));
	    }
	    void decStrong(const void* /*id*/) const {
	        common.decRef(const_cast<android_native_base_t*>(&common));
	    }
	#endif
	
	    struct android_native_base_t common;
	
	    int width;
	    int height;
	    int stride;
	    int format;
	    int usage;
	
	    void* reserved[2];
		<!--buffer_handle_t 是一个内存handle的指针-->
	    buffer_handle_t handle;
	
	    void* reserved_proc[8];
	} ANativeWindowBuffer_t;
	

	typedef const native_handle_t* buffer_handle_t;
	
	typedef struct native_handle
	{
	    int version;        /* sizeof(native_handle_t) */
	    int numFds;         /* number of file-descriptors at &data[0] */
	    int numInts;        /* number of ints at &data[numFds] */
	    int data[0];        /* numFds + numInts ints */
	} native_handle_t;


	typedef struct android_native_base_t
	{
	    /* a magic value defined by the actual EGL native type */
	    int magic;
	
	    /* the sizeof() of the actual EGL native type */
	    int version;
	
	    void* reserved[4];
	
	    /* reference-counting interface */
	    void (*incRef)(struct android_native_base_t* base);
	    void (*decRef)(struct android_native_base_t* base);
	} android_native_base_t;

native_handle/native_handle_t只是定义了一个描述buffer的结构体原型,这个原型是和平台无关的,方便buffer在各个进程之间传递,注意成员data是一个大小为0的数组,这意味着data指向紧挨着numInts后面的一个地址.我们可以把native_handle_t看成是一个纯虚的基类
	    
	   
	
	#ifdef __cplusplus
	//在c++编译环境下private_handle_t继承于native_handle
	struct private_handle_t : public native_handle {
	#else
	//在c编译环境下,private_handle_t的第一个成员是native_handle类型,其实和c++的继承是一个意思,
	//总之就是一个指向private_handle_t的指针同样也可以表示一个指向native_handle的指针.
	struct private_handle_t {
	    struct native_handle nativeHandle;
	#endif
	    // file-descriptors
	    int     fd; 
	    // ints
	    int     magic;
	    int     flags;
	    int     size;
	    int     offset;
		// 因为native_handle的data成员是一个大小为0的数组,所以data[0]其实就是指向了fd,data[1]指向magic,以此类推.
		// 上面提到我们可以把native_handle看成是一个纯虚的基类,那么在private_handle_t这个派生类中,numFds=1 numInts=4.
		...
	   
gralloc分配的buffer都可以用一个private_handle_t来描述,同时也可以用一个native_handle来描述.在不同的平台的实现上,private_handle_t可能会有不同的定义,所以private_handle_t在各个模块之间传递的时候很不方便,而如果用native_handle的身份来传递,就可以消除平台的差异性.在HardwareComposer中,由SurfaceFlinger传给hwc的handle即是native_handle类型,而hwc作为平台相关的模块,他需要知道native_handle中各个字段的具体含义,所以hwc往往会将native_handle指针转化为private_handle_t指针来使用.	   
   
>android / platform / hardware / libhardware / marshmallow-release / . / modules / gralloc / gralloc_priv.h
	   
	   
	#ifdef __cplusplus
	struct private_handle_t : public native_handle {
	#else
	struct private_handle_t {
	    struct native_handle nativeHandle;
	#endif
	    enum {
	        PRIV_FLAGS_FRAMEBUFFER = 0x00000001
	    };
	    // file-descriptors
	    int     fd;
	    // ints
	    int     magic;
	    int     flags;
	    int     size;
	    int     offset;
	    // FIXME: the attributes below should be out-of-line
	    uint64_t base __attribute__((aligned(8)));
	    int     pid;
	#ifdef __cplusplus
	    static inline int sNumInts() {
	        return (((sizeof(private_handle_t) - sizeof(native_handle_t))/sizeof(int)) - sNumFds);
	    }
	    static const int sNumFds = 1;
	    static const int sMagic = 0x3141592;
	    private_handle_t(int fd, int size, int flags) :
	        fd(fd), magic(sMagic), flags(flags), size(size), offset(0),
	        base(0), pid(getpid())
	    {
	        version = sizeof(native_handle);
	        numInts = sNumInts();
	        numFds = sNumFds;
	    }
	    ~private_handle_t() {
	        magic = 0;
	    }
	    static int validate(const native_handle* h) {
	        const private_handle_t* hnd = (const private_handle_t*)h;
	        if (!h || h->version != sizeof(native_handle) ||
	                h->numInts != sNumInts() || h->numFds != sNumFds ||
	                hnd->magic != sMagic)
	        {
	            ALOGE("invalid gralloc handle (at %p)", h);
	            return -EINVAL;
	        }
	        return 0;
	    }
	#endif
	};
	   
	   
* sturct native_handle
* native_handle_t
* struct private_handle_t

这三个类型可以看作是同一个东西,而buffer_handle_t则是指向他们的指针.


    
其实对于Surface，最关键的就是一个IGraphicBufferProducer，他是一个生产者，GraphicBuffer，deque eque 

首先看一下Surface跟ANativeWindow的关系

	class Surface
	    : public ANativeObjectBase<ANativeWindow, Surface, RefBase>
	{

	template <typename NATIVE_TYPE, typename TYPE, typename REF>
	class ANativeObjectBase : public NATIVE_TYPE, public REF
	{

结合C++的泛型知识，可以看到，Surface其实是ANativeWindow的子类，使用中Surface可以直接转换为ANativeWindow，

	struct ANativeWindow
	{
	   .....
	   
	   //上层定义好函数指针，传给opengl后，opengl在必要的时候会调用相应的函数
	   int     (*dequeueBuffer )(struct ANativeWindow* window ,
	                struct ANativeWindowBuffer ** buffer, int* fenceFd );
	   
	   int     (*queueBuffer )(struct ANativeWindow* window ,
	                struct ANativeWindowBuffer * buffer, int fenceFd );
	
	   int     (*cancelBuffer )(struct ANativeWindow* window ,
	                struct ANativeWindowBuffer * buffer, int fenceFd );
	}



**native_handle private_handle_t ANativeWindowBuffer GraphicBuffer这四个struct/class所描述的是一块memory**
**ANativeWindow 和Surface所描述的是一系列上述memeory的组合和对buffer的操作方法**有的struct/class在比较低的level使用,和平台相关,而另外一些在比较高的level使用,和平台无关,还有一些介于低/高level之间,用以消除平台相关性,让android可以方便的运行在不同的平台上


我们目前需要注意的是ANativeWindow的函数指针成员所指向的函数都需要一个struct ANativeWindowBuffer* buffer的参数。ANativeWindowBuffer和ANativeWindow还是没有给android_native_base_t的incRef和decRef指针赋值,ANativeWindowBuffer和ANativeWindow两个还是可以理解为抽象类!


Surface和GraphicBuffer都继承自模版类ANativeObjectBase,他使用的三个模版是ANativeWindow, Surface, RefBase,关于incRef和decRef两个函数指针的指向问题和上面GraphicBuffer是完全相同的, 这里就不赘述了.我们需要注意的是Surface有一个BufferSlot类型的成员数组mSlots,BufferSlot是GraphicBuffer的包装,所以我们可以理解为每个Surface中都有一个大小为NUM_BUFFER_SLOTS的GraphicBuffer数组.
    
    
	class egl_surface_t : public egl_object_t {
	protected:
	    ~egl_surface_t();
	public:
	    typedef egl_object_t::LocalRef<egl_surface_t, EGLSurface> Ref;
	    egl_surface_t(egl_display_t* dpy, EGLConfig config,
	            EGLNativeWindowType win, EGLSurface surface,
	            egl_connection_t const* cnx);
	    EGLSurface surface;
	    EGLConfig config;
	    sp<ANativeWindow> win;
	    egl_connection_t const* cnx;
	};


	typedef void *EGLConfig;
	typedef void *EGLContext;
	typedef void *EGLDisplay;
	typedef void *EGLSurface;
	typedef void *EGLClientBuffer;

EGLSurface是一个void类型指针，所以随便赋值

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

egl_window_surface_v2_t是一个egl_surface_t

	struct egl_window_surface_v2_t : public egl_surface_t
	{

支持函数
	
	 virtual     bool        initCheck() const { return true; } // TODO: report failure if ctor fails
	    virtual     EGLBoolean  swapBuffers();
	    virtual     EGLBoolean  bindDrawSurface(ogles_context_t* gl);
	    virtual     EGLBoolean  bindReadSurface(ogles_context_t* gl);
	    virtual     EGLBoolean  connect();
	    virtual     void        disconnect();
	    virtual     EGLint      getWidth() const    { return width;  }
	    virtual     EGLint      getHeight() const   { return height; }
	    virtual     EGLint      getHorizontalResolution() const;
	    virtual     EGLint      getVerticalResolution() const;
	    virtual     EGLint      getRefreshRate() const;
	    virtual     EGLint      getSwapBehavior() const;
	    virtual     EGLBoolean  setSwapRectangle(EGLint l, EGLint t, EGLint w, EGLint h);
	   
   内部变量看出，握有ANativeWindowBuffer previousBuffer+ buffer
         
	private:
	    status_t lock(ANativeWindowBuffer* buf, int usage, void** vaddr);
	    status_t unlock(ANativeWindowBuffer* buf);
	    ANativeWindow*   nativeWindow;
	    ANativeWindowBuffer*   buffer;
	    ANativeWindowBuffer*   previousBuffer;
	    gralloc_module_t const*    module;
	    int width;
	    int height;
	    void* bits;
	    GGLFormat const* pixelFormatTable;
	    

**BufferQueue是Android 中所有图形处理操作的核心。它的作用很简单：将生成图形数据缓冲区的一方（生产方）连接到接受数据以进行显示或进一步处理的一方（消耗方）。几乎所有在系统中移动图形数据缓冲区的内容都依赖于BufferQueue，比如显示、编码等。 以显示流程为例，生产者进程一般就是应用程序进程，消费者进程就是SurfaceFlinger进程，应用程序进程的surface对象和SurfaceFlinger进程的layer对象可以看做实际的生产者和消费者，主要类的关系如下所示：，应用程序申请surface时，会在SurfaceFlinger进程创建一个layer对象，接着会创建BufferQueueCore、BufferQueueProducer和BufferQueueConsumer对象，然后创建SurfaceFlingerConsumer和ProxyConsumerListener对象，而在应用程序进程这端会创建Surface对象和BpGraphicBufferProducer代理对象，应用程序进程通过Surface对象中的BpGraphicBufferProducer向SurfaceFlinger进程中的BufferQueueCore对象申请和提交GraphicBuffer，SurfaceFlinger进程中的BufferQueueCore对象通过ProxyConsumerListener、SurfaceFlingerConsumer、Layer一路通知到SurfaceFlinger有新的GraphicBuffer需要合成，SurfaceFlinger收到通知，通过Layer调用SurfaceFlingerConsumer的updateTexImage，将GraphicBuffer绘制成纹理，然后再合成输出。**

 GraphicBuffer的管理，Android也设计了一套机制：BufferQueue，作为SurfaceFlinger管理和消费surface的中介
 
 FREE->DEQUEUED->QUEUED->ACQUIRED->FREE这个过程


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
	
egl_pixmap_surface_t存储保存在系统内存中的位图
egl_pbuffer_surface_t存储保存在显存中的帧，以上两种位图属于不可显示的Surface。

在Android系统上EGLNativeWindowType就是ANativeWindow* 指针，


	#elif defined(__ANDROID__) || defined(ANDROID)
	
	struct ANativeWindow;
	struct egl_native_pixmap_t;
	
	typedef struct ANativeWindow*           EGLNativeWindowType;
	typedef struct egl_native_pixmap_t*     EGLNativePixmapType;
	typedef void*                           EGLNativeDisplayType;
	
	#elif defined(__unix__)


native_handle/native_handle_t是private_handle_t的抽象表示方法,消除平台相关性,方便private_handle_t所表示的memory信息在android各个层次之间传递.而buffer_handle_t是指向他们的指针.

ANativeWindowBuffer将buffer_handle_t进行了包装,ANativeWindow和ANativeWindowBuffer都继承于android_native_base_t,定义了common.incRef和common.decRef两个函数指针,但是并没有为函数指针赋值,所以ANativeWindow和ANativeWindowBuffer仍然是抽象类.

GraphicBuffer和Surface通过继承模版类ANativeObjectBase并指定其中一个模版是RefBase,为incRef和decRef两个指针分别赋值为RefBase的incStrong和decStrong,这样

GraphicBuffer继承了ANativeWindowBuffer,Surface继承了ANativeWindow,并且两者都具有的和RefBase同样的incStong decStrong成员函数.

Surface的成员BufferSlot mSlots[NUM_BUFFER_SLOTS];可以看作是sp<GraphicBuffer>类型的数组,也就是说每个Surface中都包含有NUM_BUFFER_SLOTS个sp<GraphicBuffer>.


EglManager开始绘制beginFrame

	void EglManager::beginFrame(EGLSurface surface, EGLint* width, EGLint* height) {
	    LOG_ALWAYS_FATAL_IF(surface == EGL_NO_SURFACE,
	            "Tried to beginFrame on EGL_NO_SURFACE!");
	    makeCurrent(surface);
	    if (width) {
	        eglQuerySurface(mEglDisplay, surface, EGL_WIDTH, width);
	    }
	    if (height) {
	        eglQuerySurface(mEglDisplay, surface, EGL_HEIGHT, height);
	    }
	    eglBeginFrame(mEglDisplay, surface);
	}

swapBuffers提交

	bool EglManager::swapBuffers(EGLSurface surface, const SkRect& dirty,
	        EGLint width, EGLint height) {
	
	#if WAIT_FOR_GPU_COMPLETION
	    {
	        ATRACE_NAME("Finishing GPU work");
	        fence();
	    }
	#endif
	
	#ifdef EGL_KHR_swap_buffers_with_damage
	    if (CC_LIKELY(Properties::swapBuffersWithDamage)) {
	        SkIRect idirty;
	        dirty.roundOut(&idirty);
	        /*
	         * EGL_KHR_swap_buffers_with_damage spec states:
	         *
	         * The rectangles are specified relative to the bottom-left of the surface
	         * and the x and y components of each rectangle specify the bottom-left
	         * position of that rectangle.
	         *
	         * HWUI does everything with 0,0 being top-left, so need to map
	         * the rect
	         */
	        EGLint y = height - (idirty.y() + idirty.height());
	        // layout: {x, y, width, height}
	        EGLint rects[4] = { idirty.x(), y, idirty.width(), idirty.height() };
	        EGLint numrects = dirty.isEmpty() ? 0 : 1;
	        eglSwapBuffersWithDamageKHR(mEglDisplay, surface, rects, numrects);
	    } else {
	        eglSwapBuffers(mEglDisplay, surface);
	    }
	#else
	    eglSwapBuffers(mEglDisplay, surface);
	#endif
	
	    EGLint err = eglGetError();
	    if (CC_LIKELY(err == EGL_SUCCESS)) {
	        return true;
	    }
	    if (err == EGL_BAD_SURFACE) {
	        // For some reason our surface was destroyed out from under us
	        // This really shouldn't happen, but if it does we can recover easily
	        // by just not trying to use the surface anymore
	        ALOGW("swapBuffers encountered EGL_BAD_SURFACE on %p, halting rendering...", surface);
	        return false;
	    }
	    LOG_ALWAYS_FATAL("Encountered EGL error %d %s during rendering",
	            err, egl_error_str(err));
	    // Impossible to hit this, but the compiler doesn't know that
	    return false;
	}



OpenGLRender.cpp如何绘制

	void OpenGLRenderer::drawColorRect(float left, float top, float right, float bottom,
	        const SkPaint* paint, bool ignoreTransform) {
	    const int transformFlags = ignoreTransform
	            ? TransformFlags::MeshIgnoresCanvasTransform : TransformFlags::None;
	    Glop glop;
	    GlopBuilder(mRenderState, mCaches, &glop)
	            .setRoundRectClipState(currentSnapshot()->roundRectClipState)
	            .setMeshUnitQuad()
	            .setFillPaint(*paint, currentSnapshot()->alpha)
	            .setTransform(*currentSnapshot(), transformFlags)
	            .setModelViewMapUnitToRect(Rect(left, top, right, bottom))
	            .build();
	    renderGlop(glop);
	}

Glop


	struct Glop {
	    struct Mesh {
	        GLuint primitiveMode; // GL_TRIANGLES and GL_TRIANGLE_STRIP supported
	
	        // buffer object and void* are mutually exclusive.
	        // Only GL_UNSIGNED_SHORT supported.
	        struct Indices {
	            GLuint bufferObject;
	            const void* indices;
	        } indices;
	
	        // buffer object and void*s are mutually exclusive.
	        // TODO: enforce mutual exclusion with restricted setters and/or unions
	        struct Vertices {
	            GLuint bufferObject;
	            int attribFlags;
	            const void* position;
	            const void* texCoord;
	            const void* color;
	            GLsizei stride;
	        } vertices;
	
	        int elementCount;
	        TextureVertex mappedVertices[4];
	    } mesh;
	
	    struct Fill {
	        Program* program;
	
	        struct TextureData {
	            Texture* texture;
	            GLenum target;
	            GLenum filter;
	            GLenum clamp;
	            Matrix4* textureTransform;
	        } texture;
	
	        bool colorEnabled;
	        FloatColor color;
	
	        ProgramDescription::ColorFilterMode filterMode;
	        union Filter {
	            struct Matrix {
	                float matrix[16];
	                float vector[4];
	            } matrix;
	            FloatColor color;
	        } filter;
	
	        SkiaShaderData skiaShaderData;
	    } fill;
	
	    struct Transform {
	        // Orthographic projection matrix for current FBO
	        // TODO: move out of Glop, since this is static per FBO
	        Matrix4 ortho;
	
	        // modelView transform, accounting for delta between mesh transform and content of the mesh
	        // often represents x/y offsets within command, or scaling for mesh unit size
	        Matrix4 modelView;
	
	        // Canvas transform of Glop - not necessarily applied to geometry (see flags)
	        Matrix4 canvas;
	        int transformFlags;
	
	       const Matrix4& meshTransform() const {
	           return (transformFlags & TransformFlags::MeshIgnoresCanvasTransform)
	                   ? Matrix4::identity() : canvas;
	       }
	    } transform;
	
	    const RoundRectClipState* roundRectClipState;
	
	    /**
	     * Blending to be used by this draw - both GL_NONE if blending is disabled.
	     *
	     * Defined by fill step, but can be force-enabled by presence of kAlpha_Attrib
	     */
	    struct Blend {
	        GLenum src;
	        GLenum dst;
	    } blend;
	
	    /**
	     * Bounds of the drawing command in layer space. Only mapped into layer
	     * space once GlopBuilder::build() is called.
	     */
	    Rect bounds;
	
	    /**
	     * Additional render state to enumerate:
	     * - scissor + (bits for whether each of LTRB needed?)
	     * - stencil mode (draw into, mask, count, etc)
	     */
	};
		
	void OpenGLRenderer::renderGlop(const Glop& glop, GlopRenderType type) {
	    // TODO: It would be best if we could do this before quickRejectSetupScissor()
	    //       changes the scissor test state
	    if (type != GlopRenderType::LayerClear) {
	        // Regular draws need to clear the dirty area on the layer before they start drawing on top
	        // of it. If this draw *is* a layer clear, it skips the clear step (since it would
	        // infinitely recurse)
	        clearLayerRegions();
	    }
	
	    if (mState.getDirtyClip()) {
	        if (mRenderState.scissor().isEnabled()) {
	            setScissorFromClip();
	        }
	
	        setStencilFromClip();
	    }
	    mRenderState.render(glop);
	    if (type == GlopRenderType::Standard && !mRenderState.stencil().isWriteEnabled()) {
	        // TODO: specify more clearly when a draw should dirty the layer.
	        // is writing to the stencil the only time we should ignore this?
	        dirtyLayer(glop.bounds.left, glop.bounds.top, glop.bounds.right, glop.bounds.bottom);
	        mDirty = true;
	    }
	}



总的来说，可以看到一个View上的东西要绘制出来，要经过多步的转化。

![](https://img-blog.csdn.net/20170108232134138?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQvamluemh1b2p1bg==/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast)

这样做有几个好处：第一、对绘制操作进行batch/merge可以减少GL的draw call，从而减少渲染状态切换，提高了性能。第二、因为将View层次结构要绘制的东西转化为DisplayList这种“中间语言”的形式，当需要绘制时才转化为GL命令。因此在View中内容没有更改或只有部分属性更改时只要修改中间表示（即RenderNode和RenderProperties）即可，从而避免很多重复劳动。第三、由于DisplayList中包含了要绘制的所有信息，一些属性动画可以由渲染线程全权处理，无需主线程介入，主线程卡住也不会让界面卡住。另一方面，也可以看到一些潜力可挖。比如当前可以合并的操作类型有限。另外主线程和渲染线程间的很多调用还是同步的，并行度或许可以进一步提高。另外Vulkan的引入也可以帮助进一步榨干GPU的能力。


RenderState OpenGl状态机？？
	
	class RenderState {
	    PREVENT_COPY_AND_ASSIGN(RenderState);
	public:
	    void onGLContextCreated();
	    void onGLContextDestroyed();
	
	    void setViewport(GLsizei width, GLsizei height);
	    void getViewport(GLsizei* outWidth, GLsizei* outHeight);
	
	    void bindFramebuffer(GLuint fbo);
	    GLint getFramebuffer() { return mFramebuffer; }
	
	    void invokeFunctor(Functor* functor, DrawGlInfo::Mode mode, DrawGlInfo* info);
	
	    void debugOverdraw(bool enable, bool clear);
	
	    void registerLayer(Layer* layer) {
	        mActiveLayers.insert(layer);
	    }
	    void unregisterLayer(Layer* layer) {
	        mActiveLayers.erase(layer);
	    }
	
	    void registerCanvasContext(renderthread::CanvasContext* context) {
	        mRegisteredContexts.insert(context);
	    }
	
	    void unregisterCanvasContext(renderthread::CanvasContext* context) {
	        mRegisteredContexts.erase(context);
	    }
	
	    void requireGLContext();
	
	    // TODO: This system is a little clunky feeling, this could use some
	    // more thinking...
	    void postDecStrong(VirtualLightRefBase* object);
	
	    void render(const Glop& glop);
	
	    AssetAtlas& assetAtlas() { return mAssetAtlas; }
	    Blend& blend() { return *mBlend; }
	    MeshState& meshState() { return *mMeshState; }
	    Scissor& scissor() { return *mScissor; }
	    Stencil& stencil() { return *mStencil; }
	
	    void dump();
	private:
	    friend class renderthread::RenderThread;
	    friend class Caches;
	
	    void interruptForFunctorInvoke();
	    void resumeFromFunctorInvoke();
	    void assertOnGLThread();
	
	    RenderState(renderthread::RenderThread& thread);
	    ~RenderState();
	
	
	    renderthread::RenderThread& mRenderThread;
	    Caches* mCaches = nullptr;
	
	    Blend* mBlend = nullptr;
	    MeshState* mMeshState = nullptr;
	    Scissor* mScissor = nullptr;
	    Stencil* mStencil = nullptr;
	
	    AssetAtlas mAssetAtlas;
	    std::set<Layer*> mActiveLayers;
	    std::set<renderthread::CanvasContext*> mRegisteredContexts;
	
	    GLsizei mViewportWidth;
	    GLsizei mViewportHeight;
	    GLuint mFramebuffer;
	
	    pthread_t mThreadId;
	};
	
	} /* namespace uirenderer */
	} /* namespace android */
	
	#endif /* RENDERSTATE_H */

RenderState

	void RenderThread::initThreadLocals() {
	    initializeDisplayEventReceiver();
	    mEglManager = new EglManager(*this);
	    mRenderState = new RenderState();
	}

看一下真正的OpenGL，又是如何交给GPU呢？


	
	void RenderState::render(const Glop& glop) {
	    const Glop::Mesh& mesh = glop.mesh;
	    const Glop::Mesh::Vertices& vertices = mesh.vertices;
	    const Glop::Mesh::Indices& indices = mesh.indices;
	    const Glop::Fill& fill = glop.fill;
	
	    // ---------------------------------------------
	    // ---------- Program + uniform setup ----------
	    // ---------------------------------------------
	    mCaches->setProgram(fill.program);
	
	    if (fill.colorEnabled) {
	        fill.program->setColor(fill.color);
	    }
	
	    fill.program->set(glop.transform.ortho,
	            glop.transform.modelView,
	            glop.transform.meshTransform(),
	            glop.transform.transformFlags & TransformFlags::OffsetByFudgeFactor);
	
	    // Color filter uniforms
	    if (fill.filterMode == ProgramDescription::kColorBlend) {
	        const FloatColor& color = fill.filter.color;
	        glUniform4f(mCaches->program().getUniform("colorBlend"),
	                color.r, color.g, color.b, color.a);
	    } else if (fill.filterMode == ProgramDescription::kColorMatrix) {
	        glUniformMatrix4fv(mCaches->program().getUniform("colorMatrix"), 1, GL_FALSE,
	                fill.filter.matrix.matrix);
	        glUniform4fv(mCaches->program().getUniform("colorMatrixVector"), 1,
	                fill.filter.matrix.vector);
	    }
	
	    // Round rect clipping uniforms
	    if (glop.roundRectClipState) {
	        // TODO: avoid query, and cache values (or RRCS ptr) in program
	        const RoundRectClipState* state = glop.roundRectClipState;
	        const Rect& innerRect = state->innerRect;
	        glUniform4f(fill.program->getUniform("roundRectInnerRectLTRB"),
	                innerRect.left, innerRect.top,
	                innerRect.right, innerRect.bottom);
	        glUniformMatrix4fv(fill.program->getUniform("roundRectInvTransform"),
	                1, GL_FALSE, &state->matrix.data[0]);
	
	        // add half pixel to round out integer rect space to cover pixel centers
	        float roundedOutRadius = state->radius + 0.5f;
	        glUniform1f(fill.program->getUniform("roundRectRadius"),
	                roundedOutRadius);
	    }
	
	    // --------------------------------
	    // ---------- Mesh setup ----------
	    // --------------------------------
	    // vertices
	    const bool force = meshState().bindMeshBufferInternal(vertices.bufferObject)
	            || (vertices.position != nullptr);
	    meshState().bindPositionVertexPointer(force, vertices.position, vertices.stride);
	
	    // indices
	    meshState().bindIndicesBufferInternal(indices.bufferObject);
	
	    if (vertices.attribFlags & VertexAttribFlags::TextureCoord) {
	        const Glop::Fill::TextureData& texture = fill.texture;
	        // texture always takes slot 0, shader samplers increment from there
	        mCaches->textureState().activateTexture(0);
	
	        if (texture.clamp != GL_INVALID_ENUM) {
	            texture.texture->setWrap(texture.clamp, true, false, texture.target);
	        }
	        if (texture.filter != GL_INVALID_ENUM) {
	            texture.texture->setFilter(texture.filter, true, false, texture.target);
	        }
	
	        mCaches->textureState().bindTexture(texture.target, texture.texture->id);
	        meshState().enableTexCoordsVertexArray();
	        meshState().bindTexCoordsVertexPointer(force, vertices.texCoord, vertices.stride);
	
	        if (texture.textureTransform) {
	            glUniformMatrix4fv(fill.program->getUniform("mainTextureTransform"), 1,
	                    GL_FALSE, &texture.textureTransform->data[0]);
	        }
	    } else {
	        meshState().disableTexCoordsVertexArray();
	    }
	    int colorLocation = -1;
	    if (vertices.attribFlags & VertexAttribFlags::Color) {
	        colorLocation = fill.program->getAttrib("colors");
	        glEnableVertexAttribArray(colorLocation);
	        glVertexAttribPointer(colorLocation, 4, GL_FLOAT, GL_FALSE, vertices.stride, vertices.color);
	    }
	    int alphaLocation = -1;
	    if (vertices.attribFlags & VertexAttribFlags::Alpha) {
	        // NOTE: alpha vertex position is computed assuming no VBO
	        const void* alphaCoords = ((const GLbyte*) vertices.position) + kVertexAlphaOffset;
	        alphaLocation = fill.program->getAttrib("vtxAlpha");
	        glEnableVertexAttribArray(alphaLocation);
	        glVertexAttribPointer(alphaLocation, 1, GL_FLOAT, GL_FALSE, vertices.stride, alphaCoords);
	    }
	    // Shader uniforms
	    SkiaShader::apply(*mCaches, fill.skiaShaderData);
	
	    // ------------------------------------
	    // ---------- GL state setup ----------
	    // ------------------------------------
	    blend().setFactors(glop.blend.src, glop.blend.dst);
	
	    // ------------------------------------
	    // ---------- Actual drawing ----------
	    // ------------------------------------
	    if (indices.bufferObject == meshState().getQuadListIBO()) {
	        // Since the indexed quad list is of limited length, we loop over
	        // the glDrawXXX method while updating the vertex pointer
	        GLsizei elementsCount = mesh.elementCount;
	        const GLbyte* vertexData = static_cast<const GLbyte*>(vertices.position);
	        while (elementsCount > 0) {
	            GLsizei drawCount = MathUtils::min(elementsCount, (GLsizei) kMaxNumberOfQuads * 6);
	
	            // rebind pointers without forcing, since initial bind handled above
	            meshState().bindPositionVertexPointer(false, vertexData, vertices.stride);
	            if (vertices.attribFlags & VertexAttribFlags::TextureCoord) {
	                meshState().bindTexCoordsVertexPointer(false,
	                        vertexData + kMeshTextureOffset, vertices.stride);
	            }
	
	            glDrawElements(mesh.primitiveMode, drawCount, GL_UNSIGNED_SHORT, nullptr);
	            elementsCount -= drawCount;
	            vertexData += (drawCount / 6) * 4 * vertices.stride;
	        }
	    } else if (indices.bufferObject || indices.indices) {
	        glDrawElements(mesh.primitiveMode, mesh.elementCount, GL_UNSIGNED_SHORT, indices.indices);
	    } else {
	        glDrawArrays(mesh.primitiveMode, 0, mesh.elementCount);
	    }
	
	    // -----------------------------------
	    // ---------- Mesh teardown ----------
	    // -----------------------------------
	    if (vertices.attribFlags & VertexAttribFlags::Alpha) {
	        glDisableVertexAttribArray(alphaLocation);
	    }
	    if (vertices.attribFlags & VertexAttribFlags::Color) {
	        glDisableVertexAttribArray(colorLocation);
	    }
	}

  glDrawElements
  
  
	  
	void drawIndexedPrimitivesTriangles(ogles_context_t* c,
	        GLsizei count, const GLvoid *indices)
	{
	    if (ggl_unlikely(count < 3))
	        return;
	
	    count -= 3;
	    if (ggl_likely(c->arrays.indicesType == GL_UNSIGNED_SHORT)) {
	        // This case is probably our most common case...
	        uint16_t const * p = (uint16_t const *)indices;
	        do {
	            vertex_t* const v0 = fetch_vertex(c, *p++);
	            vertex_t* const v1 = fetch_vertex(c, *p++);
	            vertex_t* const v2 = fetch_vertex(c, *p++);
	            const uint32_t cc = v0->flags & v1->flags & v2->flags;
	            if (ggl_likely(!(cc & vertex_t::CLIP_ALL)))
	                c->prims.renderTriangle(c, v0, v1, v2);
	            v0->locked = 0;
	            v1->locked = 0;
	            v2->locked = 0;
	            count -= 3;
	        } while (count >= 0);
	    } else {
	        uint8_t const * p = (uint8_t const *)indices;
	        do {
	            vertex_t* const v0 = fetch_vertex(c, *p++);
	            vertex_t* const v1 = fetch_vertex(c, *p++);
	            vertex_t* const v2 = fetch_vertex(c, *p++);
	            const uint32_t cc = v0->flags & v1->flags & v2->flags;
	            if (ggl_likely(!(cc & vertex_t::CLIP_ALL)))
	                c->prims.renderTriangle(c, v0, v1, v2);
	            v0->locked = 0;
	            v1->locked = 0;
	            v2->locked = 0;
	            count -= 3;
	        } while (count >= 0);
	    }
	}


内存消耗从何而来



	/* void glGenTextures ( GLsizei n, GLuint *textures ) */
	static void
	android_glGenTextures__I_3II
	  (JNIEnv *_env, jobject _this, jint n, jintArray textures_ref, jint offset) {
	    jint _exception = 0;
	    const char * _exceptionType = NULL;
	    const char * _exceptionMessage = NULL;
	    GLuint *textures_base = (GLuint *) 0;
	    jint _remaining;
	    GLuint *textures = (GLuint *) 0;
	
	    if (!textures_ref) {
	        _exception = 1;
	        _exceptionType = "java/lang/IllegalArgumentException";
	        _exceptionMessage = "textures == null";
	        goto exit;
	    }
	    if (offset < 0) {
	        _exception = 1;
	        _exceptionType = "java/lang/IllegalArgumentException";
	        _exceptionMessage = "offset < 0";
	        goto exit;
	    }
	    _remaining = _env->GetArrayLength(textures_ref) - offset;
	    if (_remaining < n) {
	        _exception = 1;
	        _exceptionType = "java/lang/IllegalArgumentException";
	        _exceptionMessage = "length - offset < n < needed";
	        goto exit;
	    }
	    textures_base = (GLuint *)
	        _env->GetIntArrayElements(textures_ref, (jboolean *)0);
	    textures = textures_base + offset;
	
	    glGenTextures(
	        (GLsizei)n,
	        (GLuint *)textures
	    );
	
	exit:
	    if (textures_base) {
	        _env->ReleaseIntArrayElements(textures_ref, (jint*)textures_base,
	            _exception ? JNI_ABORT: 0);
	    }
	    if (_exception) {
	        jniThrowException(_env, _exceptionType, _exceptionMessage);
	    }
	}


	void glGenTextures(GLsizei n, GLuint *textures) {
	    glGenCommon(n, textures);
	}


	void glGenCommon(GLsizei n, GLuint *buffers) {
	    static GLuint nextId = 0;
	    int i;
	    for(i = 0; i < n; i++) {
	        buffers[i] = ++nextId;
	    }
	}

# util_texImage2D 纹理为何要拷贝一份呢glTexImage2D自身有拷贝机制吗

	static jint util_texImage2D(JNIEnv *env, jclass clazz,
	        jint target, jint level, jint internalformat,
	        jobject jbitmap, jint type, jint border)
	{
	    SkBitmap bitmap;
	    GraphicsJNI::getSkBitmap(env, jbitmap, &bitmap);
	    SkColorType colorType = bitmap.colorType();
	    if (internalformat < 0) {
	        internalformat = getInternalFormat(colorType);
	    }
	    if (type < 0) {
	        type = getType(colorType);
	    }
	    int err = checkFormat(colorType, internalformat, type);
	    if (err)
	        return err;
	    bitmap.lockPixels();
	    const int w = bitmap.width();
	    const int h = bitmap.height();
	    const void* p = bitmap.getPixels();
	    if (internalformat == GL_PALETTE8_RGBA8_OES) {
	        if (sizeof(SkPMColor) != sizeof(uint32_t)) {
	            err = -1;
	            goto error;
	        }
	        const size_t size = bitmap.getSize();
	        const size_t palette_size = 256*sizeof(SkPMColor);
	        const size_t imageSize = size + palette_size;
	        void* const data = malloc(imageSize);
	        if (data) {
	            void* const pixels = (char*)data + palette_size;
	            SkColorTable* ctable = bitmap.getColorTable();
	            memcpy(data, ctable->readColors(), ctable->count() * sizeof(SkPMColor));
	            memcpy(pixels, p, size);
	            glCompressedTexImage2D(target, level, internalformat, w, h, border, imageSize, data);
	            free(data);
	        } else {
	            err = -1;
	        }
	    } else {
	        glTexImage2D(target, level, internalformat, w, h, border, internalformat, type, p);
	    }
	error:
	    bitmap.unlockPixels();
	    return err;
	}


	
	
	
	void glTexImage2D(
	        GLenum target, GLint level, GLint internalformat,
	        GLsizei width, GLsizei height, GLint border,
	        GLenum format, GLenum type, const GLvoid *pixels)
	{
	    ogles_context_t* c = ogles_context_t::get();
	    if (target != GL_TEXTURE_2D) {
	        ogles_error(c, GL_INVALID_ENUM);
	        return;
	    }
	    if (width<0 || height<0 || border!=0 || level < 0) {
	        ogles_error(c, GL_INVALID_VALUE);
	        return;
	    }
	    if (format != (GLenum)internalformat) {
	        ogles_error(c, GL_INVALID_OPERATION);
	        return;
	    }
	    if (validFormatType(c, format, type)) {
	        return;
	    }
	
	    int32_t size = 0;
	    GGLSurface* surface = 0;
	    <!--重新分配-->
	    int error = createTextureSurface(c, &surface, &size,
	            level, format, type, width, height);
	    if (error) {
	        ogles_error(c, error);
	        return;
	    }
	
	    if (pixels) {
	        const int32_t formatIdx = convertGLPixelFormat(format, type);
	        const GGLFormat& pixelFormat(c->rasterizer.formats[formatIdx]);
	        const int32_t align = c->textures.unpackAlignment-1;
	        const int32_t bpr = ((width * pixelFormat.size) + align) & ~align;
	        const size_t size = bpr * height;
	        const int32_t stride = bpr / pixelFormat.size;
	
	        GGLSurface userSurface;
	        userSurface.version = sizeof(userSurface);
	        userSurface.width  = width;
	        userSurface.height = height;
	        userSurface.stride = stride;
	        userSurface.format = formatIdx;
	        userSurface.compressedFormat = 0;
	        userSurface.data = (GLubyte*)pixels;
	<!--拷贝，不用CPU线程中的数据，只有自己线程的数据，可能是为了安全-->
	        int err = copyPixels(c, *surface, 0, 0, userSurface, 0, 0, width, height);
	        if (err) {
	            ogles_error(c, err);
	            return;
	        }
	        generateMipmap(c, level);
	    }
	}
	
重新分配，并拷贝内存
	
	int createTextureSurface(ogles_context_t* c,
	        GGLSurface** outSurface, int32_t* outSize, GLint level,
	        GLenum format, GLenum type, GLsizei width, GLsizei height,
	        GLenum compressedFormat = 0)
	{
	    // find out which texture is bound to the current unit
	    const int active = c->textures.active;
	    const GLuint name = c->textures.tmu[active].name;
	
	    // convert the pixelformat to one we can handle
	    const int32_t formatIdx = convertGLPixelFormat(format, type);
	    if (formatIdx == 0) { // we don't know what to do with this
	        return GL_INVALID_OPERATION;
	    }
	
	    // figure out the size we need as well as the stride
	    const GGLFormat& pixelFormat(c->rasterizer.formats[formatIdx]);
	    const int32_t align = c->textures.unpackAlignment-1;
	    const int32_t bpr = ((width * pixelFormat.size) + align) & ~align;
	    const size_t size = bpr * height;
	    const int32_t stride = bpr / pixelFormat.size;
	
	    if (level > 0) {
	        const int active = c->textures.active;
	        EGLTextureObject* tex = c->textures.tmu[active].texture;
	        status_t err = tex->reallocate(level,
	                width, height, stride, formatIdx, compressedFormat, bpr);
	        if (err != NO_ERROR)
	            return GL_OUT_OF_MEMORY;
	        GGLSurface& surface = tex->editMip(level);
	        *outSurface = &surface;
	        *outSize = size;
	        return 0;
	    }
	
	    sp<EGLTextureObject> tex = getAndBindActiveTextureObject(c);
	    status_t err = tex->reallocate(level,
	            width, height, stride, formatIdx, compressedFormat, bpr);
	    if (err != NO_ERROR)
	        return GL_OUT_OF_MEMORY;
	
	    tex->internalformat = format;
	    *outSurface = &tex->surface;
	    *outSize = size;
	    return 0;
	}
	

	
	status_t EGLTextureObject::reallocate(
	        GLint level, int w, int h, int s,
	        int format, int compressedFormat, int bpr)
	{
	    const size_t size = h * bpr;
	    if (level == 0)
	    {
	        if (size!=mSize || !surface.data) {
	            if (mSize && surface.data) {
	                free(surface.data);
	            }
	            surface.data = (GGLubyte*)malloc(size);
	            if (!surface.data) {
	                mSize = 0;
	                mIsComplete = false;
	                return NO_MEMORY;
	            }
	            mSize = size;
	        }
	        surface.version = sizeof(GGLSurface);
	        surface.width  = w;
	        surface.height = h;
	        surface.stride = s;
	        surface.format = format;
	        surface.compressedFormat = compressedFormat;
	        if (mMipmaps)
	            freeMipmaps();
	        mIsComplete = true;
	    }
	    else
	    {
	        if (!mMipmaps) {
	            if (allocateMipmaps() != NO_ERROR)
	                return NO_MEMORY;
	        }
	
	        ALOGW_IF(level-1 >= mNumExtraLod,
	                "specifying mipmap level %d, but # of level is %d",
	                level, mNumExtraLod+1);
	
	        GGLSurface& mipmap = editMip(level);
	        if (mipmap.data)
	            free(mipmap.data);
	
	        mipmap.data = (GGLubyte*)malloc(size);
	        if (!mipmap.data) {
	            memset(&mipmap, 0, sizeof(GGLSurface));
	            mIsComplete = false;
	            return NO_MEMORY;
	        }
	
	        mipmap.version = sizeof(GGLSurface);
	        mipmap.width  = w;
	        mipmap.height = h;
	        mipmap.stride = s;
	        mipmap.format = format;
	        mipmap.compressedFormat = compressedFormat;
	
	        // check if the texture is complete
	        mIsComplete = true;
	        const GGLSurface* prev = &surface;
	        for (int i=0 ; i<mNumExtraLod ; i++) {
	            const GGLSurface* curr = mMipmaps + i;
	            if (curr->format != surface.format) {
	                mIsComplete = false;
	                break;
	            }
	
	            uint32_t w = (prev->width  >> 1) ? : 1;
	            uint32_t h = (prev->height >> 1) ? : 1;
	            if (w != curr->width || h != curr->height) {
	                mIsComplete = false;
	                break;
	            }
	            prev = curr;
	        }
	    }
	    return NO_ERROR;
	}

为什么采用硬件加速后内存占用增加，因为OpenGL会在自己的渲染上下文保留一份备份，这样的才能保证渲染线程的安全。所以内存占用升高，这些内存并不一定是从共享内存分配的，可能直接native分配的，OpenGL加载的什么。

glTexImage2D always makes a copy of the data.

不可见的时候Graphics占用的内存就会降低，


Graphics：图形缓冲区队列向屏幕显示像素（包括 GL 表面、GL 纹理等等）所使用的内存。 （请注意，这是与 CPU 共享的内存，不是 GPU 专用内存。）可能提交多个内存，没来接使用

#     参考文档

[Android BufferQueue简析](https://www.jianshu.com/p/edd7d264be73)           
[不错的demo GraphicsTestBed ](https://github.com/lb377463323/GraphicsTestBed)        
[小窗播放视频的原理和实现（上）](http://www.10tiao.com/html/223/201712/2651232830/1.html)         
[GLTextureViewActivity.java ](https://android.googlesource.com/platform/frameworks/base/+/master/tests/HwAccelerationTest/src/com/android/test/hwui/GLTextureViewActivity.java)              
[Android SurfaceFlinger 学习之路(七)----创建图形缓冲区GraphicBuffer](http://windrunnerlihuan.com/2017/06/22/Android-SurfaceFlinger-%E5%AD%A6%E4%B9%A0%E4%B9%8B%E8%B7%AF-%E4%B8%83-%E5%88%9B%E5%BB%BA%E5%9B%BE%E5%BD%A2%E7%BC%93%E5%86%B2%E5%8C%BAGraphicBuffer/)
[](https://www.jianshu.com/p/ccd5da85cf9e)         
[Android OpenGL ES与EGL](https://blog.csdn.net/MARTINGANG/article/details/8142120)