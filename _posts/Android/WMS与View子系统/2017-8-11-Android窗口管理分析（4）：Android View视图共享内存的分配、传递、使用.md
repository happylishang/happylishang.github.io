---
layout: post
title: Android窗口管理分析（4）：Android View绘制图层内存的分配、传递、使用
category: Android
image: http://upload-images.jianshu.io/upload_images/1460468-103d49829291e1f7.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---

前文[Android匿名共享内存（Ashmem）原理](http://www.jianshu.com/p/d9bc9c668ba6)分析了匿名共享内存，它最主要的作用就是View视图绘制，Android视图是按照一帧一帧显示到屏幕的，而每一帧都会占用一定的存储空间，通过Ashmem机制APP与SurfaceFlinger共享绘图数据，提高图形处理性能，本文就看Android是怎么利用Ashmem分配及绘制的：


## View视图内存的分配

前文[Window添加流程](http://www.jianshu.com/p/40776c123adb)中描述了：在添加窗口的时候，WMS会为APP分配一个WindowState，以标识当前窗口并用于窗口管理，同时向SurfaceFlinger端请求分配Layer抽象图层，在SurfaceFlinger分配Layer的时候创建了两个比较关键的Binder对象，用于填充WMS端Surface，一个是sp<IBinder> handle：是每个窗口标识的句柄，将来WMS同SurfaceFlinger通信的时候方便找到对应的图层。另一个是sp<IGraphicBufferProducer> gbp ：共享内存分配的关键对象，同时兼具Binder通信的功能，用来传递**指令**及**共享内存的句柄**，注意，这里只是抽象创建了对象，并未真正分配每一帧的内存，内存的分配要等到真正绘制的时候才会申请，首先看一下分配流程：

* 分配的时机：什么时候分配
* 分配的手段：如何分配
* 传递的方式：如何跨进程传递

Surface被抽象成一块画布，只要拥有Surface就可以绘图，其根本原理就是Surface握有可以绘图的一块内存，这块内存是APP端在需要的时候，通过sp<IGraphicBufferProducer> gbp向SurfaceFlinger申请的，那么首先看一下APP端如何获得sp<IGraphicBufferProducer> gbp这个服务代理的，之后再看如何利用它申请内存，在WMS利用向SurfaceFlinger申请填充Surface的时候，会请求SurfaceFlinger分配这把剑，并将其句柄交给自己

	sp<SurfaceControl> SurfaceComposerClient::createSurface(
	        const String8& name,  uint32_t w, uint32_t h, PixelFormat format, uint32_t flags){
	      sp<SurfaceControl> sur;
	      ...
	      if (mStatus == NO_ERROR) {
	        sp<IBinder> handle;
	        sp<IGraphicBufferProducer> gbp;
	        <!--关键点1 获取图层的关键信息handle, gbp-->
	        status_t err = mClient->createSurface(name, w, h, format, flags,
	                &handle, &gbp);
	         <!--关键点2 根据返回的图层关键信息 创建SurfaceControl对象-->
	        if (err == NO_ERROR) {
	            sur = new SurfaceControl(this, handle, gbp);
	        }
	    }
	    return sur;
	}

看关键点1，这里其实就是建立了一个sp<IGraphicBufferProducer> gbp容器，并请求SurfaceFlinger分配填充内容，SurfaceFlinger收到请求后会为WMS建立与APP端对应的Layer，同时为其分配sp<IGraphicBufferProducer> gbp，并填充到Surface中返回给APP，

	status_t SurfaceFlinger::createNormalLayer(const sp<Client>& client,
	        const String8& name, uint32_t w, uint32_t h, uint32_t flags, PixelFormat& format,
	        sp<IBinder>* handle, sp<IGraphicBufferProducer>* gbp, sp<Layer>* outLayer){
	    ...
	    <!--关键点 1 -->
	    *outLayer = new Layer(this, client, name, w, h, flags);
	    status_t err = (*outLayer)->setBuffers(w, h, format, flags);
	    <!--关键点 2-->
	    if (err == NO_ERROR) {
	        *handle = (*outLayer)->getHandle();
	        *gbp = (*outLayer)->getProducer();
	    }
	  return err;
	}

	void Layer::onFirstRef() {
	    sp<IGraphicBufferProducer> producer;
	    sp<IGraphicBufferConsumer> consumer;
	    <!--创建producer与consumer-->
	    BufferQueue::createBufferQueue(&producer, &consumer);
	    mProducer = new MonitoredProducer(producer, mFlinger);
	    mSurfaceFlingerConsumer = new SurfaceFlingerConsumer(consumer, mTextureName,
	            this);
	   ...
	}

	void BufferQueue::createBufferQueue(sp<IGraphicBufferProducer>* outProducer,
	        sp<IGraphicBufferConsumer>* outConsumer,
	        const sp<IGraphicBufferAlloc>& allocator) {
	    sp<BufferQueueCore> core(new BufferQueueCore(allocator));
	    sp<IGraphicBufferProducer> producer(new BufferQueueProducer(core));
	    sp<IGraphicBufferConsumer> consumer(new BufferQueueConsumer(core));
	    *outProducer = producer;
	    *outConsumer = consumer;
	}

从上面两个函数可以很清楚的看到Producer/Consumer的模型原样，也就说每个图层Layer都有自己的producer/ consumer,sp<IGraphicBufferProducer> gbp对应的其实是BufferQueueProducer，而BufferQueueProducer是一个Binder通信对象，在服务端是:

	class BufferQueueProducer : public BnGraphicBufferProducer,
	                            private IBinder::DeathRecipient {}

在APP端是

	class BpGraphicBufferProducer : public BpInterface<IGraphicBufferProducer>{}

IGraphicBufferProducer Binder实体在SurfaceFlinger中创建后，打包到Surface对象，并通过binder通信传递给APP端，APP段通过反序列化将其恢复出来，如下：

	status_t Surface::readFromParcel(const Parcel* parcel, bool nameAlreadyRead) {
	    if (parcel == nullptr) return BAD_VALUE;
	
	    status_t res = OK;
	    if (!nameAlreadyRead) {
	        name = readMaybeEmptyString16(parcel);
	        // Discard this for now
	        int isSingleBuffered;
	        res = parcel->readInt32(&isSingleBuffered);
	        if (res != OK) {
	            return res;
	        }
	    }
	    sp<IBinder> binder;
	    res = parcel->readStrongBinder(&binder);
	    if (res != OK) return res;
	   <!--interface_cast会将其转换成BpGraphicBufferProducer-->
	    graphicBufferProducer = interface_cast<IGraphicBufferProducer>(binder);
	    return OK;
	}

自此，APP端就获得了申请内存的句柄BpGraphicBufferProducer，它真正发挥作用是在第一次绘图时，看一下ViewRootImpl中的draw

	   private boolean drawSoftware(Surface surface, AttachInfo attachInfo, int xoff, int yoff,
	            boolean scalingRequired, Rect dirty) {
		        final Canvas canvas;
	        try {
	            final int left = dirty.left;
	            final int top = dirty.top;
	            final int right = dirty.right;
	            final int bottom = dirty.bottom;
	            <!--关键点1 获取绘图内存-->
	            canvas = mSurface.lockCanvas(dirty);
	        try {
	           try {
	               <!--关键点2 绘图-->
	               mView.draw(canvas);
	            }              
	        } finally {
	            try {
	            <!--关键点 3 绘图结束 ，通知surfacefling混排，更新显示界面-->
	              surface.unlockCanvasAndPost(canvas);
	            } catch (IllegalArgumentException e) {}

    
先看关键点1，内存的分配时机其实就在这里，直接进入到native层

	static jlong nativeLockCanvas(JNIEnv* env, jclass clazz,
	        jlong nativeObject, jobject canvasObj, jobject dirtyRectObj) {
	    sp<Surface> surface(reinterpret_cast<Surface *>(nativeObject));
	    ...
	    status_t err = surface->lock(&outBuffer, dirtyRectPtr);
	    ...
	    sp<Surface> lockedSurface(surface);
	    lockedSurface->incStrong(&sRefBaseOwner);
	    return (jlong) lockedSurface.get();
	}

surface.cpp的lock会进一步调用dequeueBuffer函数来请求分配内存：

	int Surface::dequeueBuffer(android_native_buffer_t** buffer, int* fenceFd) {
	    ...
	    int buf = -1;
	    sp<Fence> fence;
	    nsecs_t now = systemTime();
	    <!--申请buffer,并获得标识符-->
	    status_t result = mGraphicBufferProducer->dequeueBuffer(&buf, &fence,
	            reqWidth, reqHeight, reqFormat, reqUsage);
	    ...
	    if ((result & IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION) || gbuf == 0) {
	    <!--申请的内存是在surfaceflinger进程中，Surface通过调用requestBuffer将图形缓冲区映射到Surface所在进程-->        
	        result = mGraphicBufferProducer->requestBuffer(buf, &gbuf);
	   ...
	}

最终会调用BpGraphicBufferProducer的dequeueBuffer向服务端请求分配内存，这里用到了匿名共享内存的知识，在Linux中一切都是文件，共享内存也看成一个文件。分配成功之后，需要跨进程传递tmpfs临时文件的描述符fd。先看下申请的逻辑：
	
		class BpGraphicBufferProducer : public BpInterface<IGraphicBufferProducer>{
	    virtual status_t dequeueBuffer(int *buf, sp<Fence>* fence, bool async,
	            uint32_t w, uint32_t h, uint32_t format, uint32_t usage) {
	        Parcel data, reply;
	        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
	        data.writeInt32(async);
	        data.writeInt32(w);
	        data.writeInt32(h);
	        data.writeInt32(format);
	        data.writeInt32(usage);
	        //通过BpBinder将要什么的buffer的相关参数保存到data，发送给BBinder
	        status_t result = remote()->transact(DEQUEUE_BUFFER, data, &reply);
	        if (result != NO_ERROR) {
	            return result;
	        }
	        //BBinder给BpBinder返回了一个int，并不是缓冲区的内存
	        *buf = reply.readInt32();
	        bool nonNull = reply.readInt32();
	        if (nonNull) {
	            *fence = new Fence();
	            reply.read(**fence);
	        }
	        result = reply.readInt32();
	        return result;
	    }
	}

在client侧，也就是BpGraphicBufferProducer侧，通过DEQUEUE_BUFFER后核心只返回了一个*buf = reply.readInt32();其实是数组mSlots的下标，在BufferQueue中有个和mSlots对应的数组，也是32个，一一对应，

	status_t BnGraphicBufferProducer::onTransact(
	    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
	{
	      case DEQUEUE_BUFFER: {
	            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
	            bool async      = data.readInt32();
	            uint32_t w      = data.readInt32();
	            uint32_t h      = data.readInt32();
	            uint32_t format = data.readInt32();
	            uint32_t usage  = data.readInt32();
	            int buf;
	            sp<Fence> fence;
	            //调用BufferQueue的dequeueBuffer
	            //也返回一个int的buf
	            int result = dequeueBuffer(&buf, &fence, async, w, h, format, usage);
	            //将buf和fence写入parcel，通过binder传给client
	            reply->writeInt32(buf);
	            reply->writeInt32(fence != NULL);
	            if (fence != NULL) {
	                reply->write(*fence);
	            }
	            reply->writeInt32(result);
	            return NO_ERROR;
	}

可以看到BnGraphicBufferProducer端获取到长宽及格式，之后利用BufferQueueProducer的dequeueBuffer来申请内存，内存可能已经申请，也可能未申请，未申请，则直接申请新内存，每个surface可以对应32块内存：
		
	status_t BufferQueueProducer::dequeueBuffer(int *outSlot,
	        sp<android::Fence> *outFence, uint32_t width, uint32_t height,
	        PixelFormat format, uint32_t usage) {
	    ...
	        sp<GraphicBuffer> graphicBuffer(mCore->mAllocator->createGraphicBuffer(
	                width, height, format, usage,
	                {mConsumerName.string(), mConsumerName.size()}, &error));

mCore其实就是上面的BufferQueueCore，mCore->mAllocator = new GraphicBufferAlloc()，最终会利用GraphicBufferAlloc对象分配共享内存:

	sp<GraphicBuffer> GraphicBufferAlloc::createGraphicBuffer(uint32_t width,
	        uint32_t height, PixelFormat format, uint32_t usage,
	        std::string requestorName, status_t* error) {
	        
	    <!--直接new新建-->
	    sp<GraphicBuffer> graphicBuffer(new GraphicBuffer(
	            width, height, format, usage, std::move(requestorName)));
	    status_t err = graphicBuffer->initCheck();
	    return graphicBuffer;
	}

从上面看到，直接new GraphicBuffer新建图像内存，
	
	GraphicBuffer::GraphicBuffer(uint32_t inWidth, uint32_t inHeight,
	        PixelFormat inFormat, uint32_t inUsage, std::string requestorName)
	    : BASE(), mOwner(ownData), mBufferMapper(GraphicBufferMapper::get()),
	      mInitCheck(NO_ERROR), mId(getUniqueId()), mGenerationNumber(0){
		...
	    handle = NULL;
	    mInitCheck = initSize(inWidth, inHeight, inFormat, inUsage,
	            std::move(requestorName));
	}
	
	status_t GraphicBuffer::initSize(uint32_t inWidth, uint32_t inHeight,
	        PixelFormat inFormat, uint32_t inUsage, std::string requestorName)
	{
	    GraphicBufferAllocator& allocator = GraphicBufferAllocator::get();
	    uint32_t outStride = 0;
	    <!--请求分配内存-->
	    status_t err = allocator.allocate(inWidth, inHeight, inFormat, inUsage,
	            &handle, &outStride, mId, std::move(requestorName));
	    if (err == NO_ERROR) {
	        width = static_cast<int>(inWidth);
	        height = static_cast<int>(inHeight);
	        format = inFormat;
	        usage = static_cast<int>(inUsage);
	        stride = static_cast<int>(outStride);
	    }
	    return err;
	}

	 status_t GraphicBufferAllocator::allocate(uint32_t width, uint32_t height,
	        PixelFormat format, uint32_t usage, buffer_handle_t* handle,
	        uint32_t* stride, uint64_t graphicBufferId, std::string requestorName)
	{
		 ...
	    auto descriptor = mDevice->createDescriptor();
	    auto error = descriptor->setDimensions(width, height);
	    error = descriptor->setFormat(static_cast<android_pixel_format_t>(format));
	    error = descriptor->setProducerUsage(
	            static_cast<gralloc1_producer_usage_t>(usage));
	    error = descriptor->setConsumerUsage(
	            static_cast<gralloc1_consumer_usage_t>(usage));
	    <!--这里的device就是抽象的硬件设备-->
	    error = mDevice->allocate(descriptor, graphicBufferId, handle);
	    error = mDevice->getStride(*handle, stride);
	    ...
	    return NO_ERROR;
	}

上面代码的mDevice就是利用hw_get_module及gralloc1_open获取到的硬件抽象层device，hw_get_module装载HAL模块，会加载相应的.so文件gralloc.default.so，它实现位于 hardware/libhardware/modules/gralloc.cpp中，最后将device映射的函数操作加载进来。这里我们关心的是allocate函数，先分析普通图形缓冲区的分配，它最终会调用gralloc_alloc_buffer()利用匿名共享内存进行分配，之前的文章[Android匿名共享内存（Ashmem）原理](http://www.jianshu.com/p/d9bc9c668ba6)分析了Android是如何通过匿名共享内存进行通信的，这里就直接用了：
	
	static int gralloc_alloc_buffer(alloc_device_t* dev,
	        size_t size, int usage, buffer_handle_t* pHandle)
	{
	    int err = 0;
	    int fd = -1;
	    size = roundUpToPageSize(size);
	    // 创建共享内存，并且设定名字跟size
	    fd = ashmem_create_region("gralloc-buffer", size);
	    if (err == 0) {
	        private_handle_t* hnd = new private_handle_t(fd, size, 0);
	        gralloc_module_t* module = reinterpret_cast<gralloc_module_t*>(
	                dev->common.module);
	         // 执行mmap，将内存映射到自己的进程
	        err = mapBuffer(module, hnd);
	        if (err == 0) {
	            *pHandle = hnd;
	        }
	    }
	
	    return err;
	}

mapBuffer会进一步调用ashmem的驱动，在tmpfs新建文件，同时开辟虚拟内存，

	int mapBuffer(gralloc_module_t const* module,
		        private_handle_t* hnd)
		{
		    void* vaddr; 
		    // vaddr有个毛用？
		    return gralloc_map(module, hnd, &vaddr);
		}
	
	static int gralloc_map(gralloc_module_t const* module,
	        buffer_handle_t handle,
	        void** vaddr)
	{
	    private_handle_t* hnd = (private_handle_t*)handle;
	    if (!(hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER)) {
	        size_t size = hnd->size;
	        void* mappedAddress = mmap(0, size,
	                PROT_READ|PROT_WRITE, MAP_SHARED, hnd->fd, 0);
	        if (mappedAddress == MAP_FAILED) {
	            return -errno;
	        }
	        hnd->base = intptr_t(mappedAddress) + hnd->offset;
	    }
	    *vaddr = (void*)hnd->base;
	    return 0;
	}

# View绘制内存的传递

分配之后，会继续利用BpGraphicBufferProducer的requestBuffer，申请将共享内存给映射到当前进程：

    virtual status_t requestBuffer(int bufferIdx, sp<GraphicBuffer>* buf) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeInt32(bufferIdx);
        status_t result =remote()->transact(REQUEST_BUFFER, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        bool nonNull = reply.readInt32();
        if (nonNull) {
            *buf = new GraphicBuffer();
            reply.read(**buf);
        }
        result = reply.readInt32();
        return result;
    }
    
private_handle_t对象用来抽象图形缓冲区，其中存储着与共享内存对应tmpfs文件的fd，GraphicBuffer对象会通过序列化，将这个fd会利用Binder通信传递给App进程，APP端获取到fd之后，便可以同mmap将共享内存映射到自己的进程空间，进而进行图形绘制。等到APP端对GraphicBuffer的反序列化的时候，会将共享内存mmap到当前进程空间：

	status_t Parcel::read(Flattenable& val) const  
	{  
	    // size  
	    const size_t len = this->readInt32();  
	    const size_t fd_count = this->readInt32();  
	    // payload  
	    void const* buf = this->readInplace(PAD_SIZE(len));  
	    if (buf == NULL)  
	        return BAD_VALUE;  
	    int* fds = NULL;  
	    if (fd_count) {  
	        fds = new int[fd_count];  
	    }  
	    status_t err = NO_ERROR;  
	    for (size_t i=0 ; i<fd_count && err==NO_ERROR ; i++) {  
	        fds[i] = dup(this->readFileDescriptor());  
	        if (fds[i] < 0) err = BAD_VALUE;  
	    }  
	    if (err == NO_ERROR) {  
	        err = val.unflatten(buf, len, fds, fd_count);  
	    }  
	    if (fd_count) {  
	        delete [] fds;  
	    }  
	    return err;  
	}  
	   
进而调用GraphicBuffer::unflatten：
		
	status_t GraphicBuffer::unflatten(void const* buffer, size_t size,
	        int fds[], size_t count)
	{
	   ...
	    mOwner = ownHandle;
		<!--将共享内存映射当前内存空间-->
	    if (handle != 0) {
	        status_t err = mBufferMapper.registerBuffer(handle);
	    }
	    return NO_ERROR;
	}

mBufferMapper.registerBuffer函数对应gralloc_register_buffer
	
	struct private_module_t HAL_MODULE_INFO_SYM = {
	    .base = {
	        .common = {
	            .tag = HARDWARE_MODULE_TAG,
	            .version_major = 1,
	            .version_minor = 0,
	            .id = GRALLOC_HARDWARE_MODULE_ID,
	            .name = "Graphics Memory Allocator Module",
	            .author = "The Android Open Source Project",
	            .methods = &gralloc_module_methods
	        },
	        .registerBuffer = gralloc_register_buffer,
	        .unregisterBuffer = gralloc_unregister_buffer,
	        .lock = gralloc_lock,
	        .unlock = gralloc_unlock,
	    },
	    .framebuffer = 0,
	    .flags = 0,
	    .numBuffers = 0,
	    .bufferMask = 0,
	    .lock = PTHREAD_MUTEX_INITIALIZER,
	    .currentBuffer = 0,
	};

最后会调用gralloc_register_buffer，通过mmap真正将tmpfs文件映射到进程空间：

	static int gralloc_register_buffer(gralloc_module_t const* module,
	                                   buffer_handle_t handle)
	{
	    ...
	    if (cb->ashmemSize > 0 && cb->mappedPid != getpid()) {
	        void *vaddr;
	        <!--mmap-->
	        int err = map_buffer(cb, &vaddr);
	        cb->mappedPid = getpid();
	    }
	
	    return 0;
	}

终于我们用到tmpfs中文件对应的描述符fd0->cb->fd

	static int map_buffer(cb_handle_t *cb, void **vaddr)
	{
	    if (cb->fd < 0 || cb->ashmemSize <= 0) {
	        return -EINVAL;
	    }
	
	    void *addr = mmap(0, cb->ashmemSize, PROT_READ | PROT_WRITE,
	                      MAP_SHARED, cb->fd, 0);
	    cb->ashmemBase = intptr_t(addr);
	    cb->ashmemBasePid = getpid();
	    *vaddr = addr;
	    return 0;
	}

到这里内存传递成功，App端就可以应用这块内存进行图形绘制了。

# View绘制内存的使用

关于内存的使用，我们回到之前的Surface lock函数，内存经过反序列化，拿到内存地址后，会封装一个ANativeWindow_Buffer返回给上层调用：

	status_t Surface::lock(
	        ANativeWindow_Buffer* outBuffer, ARect* inOutDirtyBounds)
	{
	     ...
	        void* vaddr;
	        <!--lock获取地址-->
	        status_t res = backBuffer->lock(
	                GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN,
	                newDirtyRegion.bounds(), &vaddr);
	
	        if (res != 0) {
	            err = INVALID_OPERATION;
	        } else {
	            mLockedBuffer = backBuffer;
	            outBuffer->width  = backBuffer->width;
	            outBuffer->height = backBuffer->height;
	            outBuffer->stride = backBuffer->stride;
	            outBuffer->format = backBuffer->format;

					<!--关键点 设置虚拟内存的地址-->
	            outBuffer->bits   = vaddr;
	        }
	    }
	    return err;
	}

ANativeWindow_Buffer的数据结构如下，其中bits字段与虚拟内存地址对应，
	
	typedef struct ANativeWindow_Buffer {
	    // The number of pixels that are show horizontally.
	    int32_t width;
	
	    // The number of pixels that are shown vertically.
	    int32_t height;
	
	    // The number of *pixels* that a line in the buffer takes in
	    // memory.  This may be >= width.
	    int32_t stride;
	
	    // The format of the buffer.  One of WINDOW_FORMAT_*
	    int32_t format;
	
	    // The actual bits.
	    void* bits;
	    
	    // Do not touch.
	    uint32_t reserved[6];
	} ANativeWindow_Buffer;
	
如何使用，看下Canvas的draw

	static void nativeLockCanvas(JNIEnv* env, jclass clazz,
	        jint nativeObject, jobject canvasObj, jobject dirtyRectObj) {
	    sp<Surface> surface(reinterpret_cast<Surface *>(nativeObject));
	    ...
	    status_t err = surface->lock(&outBuffer, &dirtyBounds);
	    ...
	    <!--SkBitmap-->
	    SkBitmap bitmap;
	    ssize_t bpr = outBuffer.stride * bytesPerPixel(outBuffer.format);
	    <!--为SkBitmap填充配置-->
	    bitmap.setConfig(convertPixelFormat(outBuffer.format), outBuffer.width, outBuffer.height, bpr);
	    <!--为SkBitmap填充格式-->
	    if (outBuffer.format == PIXEL_FORMAT_RGBX_8888) {
	        bitmap.setIsOpaque(true);
	    }
	    <!--为SkBitmap填充内存-->
	    if (outBuffer.width > 0 && outBuffer.height > 0) {
	        bitmap.setPixels(outBuffer.bits);
	    } else {
	        // be safe with an empty bitmap.
	        bitmap.setPixels(NULL);
	    }
	
		<!--创建native SkCanvas-->
	    SkCanvas* nativeCanvas = SkNEW_ARGS(SkCanvas, (bitmap));
	    swapCanvasPtr(env, canvasObj, nativeCanvas);
	   ...
	}

对于2D绘图，会用skia库会填充Bitmap对应的共享内存，如此即可完成绘制，本文不深入Skia库，有兴趣自行分析。绘制完成后，通过unlock直接通知SurfaceFlinger服务进行图层合成。


# Android View局部重绘的原理

拿TextView来说，如果内容发生了改变，就会触发重绘，加入当前视图中还包含其他View，这个时候，可能只会触发TextView及其父层级View的重绘，其他View不重绘，为什么呢？这个时候传递给SurfaceFlinger的UI数据如何保证完整呢？其实在lockCanvas的时候，默认是又一次数据拷贝的，也就是将之前绘制的UI数据拷贝到最新的申请内存中去，而新的重绘是从拷贝之后开始的，也就是在原来视图的基础上进行脏区域重绘：
	
	status_t Surface::lock(
	        ANativeWindow_Buffer* outBuffer, ARect* inOutDirtyBounds)
	{
     <!--申请内存-->
	    status_t err = dequeueBuffer(&out, &fenceFd);
	    ALOGE_IF(err, "dequeueBuffer failed (%s)", strerror(-err));
	    if (err == NO_ERROR) {
	    <!--如果需要就尽心拷贝-->
	        sp<GraphicBuffer> backBuffer(GraphicBuffer::getSelf(out));
	        const Rect bounds(backBuffer->width, backBuffer->height);
		        ...
	        const sp<GraphicBuffer>& frontBuffer(mPostedBuffer);
	        const bool canCopyBack = (frontBuffer != 0 &&
	                backBuffer->width  == frontBuffer->width &&
	                backBuffer->height == frontBuffer->height &&
	                backBuffer->format == frontBuffer->format);
	
	        // 是否能够拷贝到当前backBuffer中来？必须两个样式一样，才能拷贝，如果不一样不用
		        if (canCopyBack) {
	            // copy the area that is invalid and not repainted this round
	            const Region copyback(mDirtyRegion.subtract(newDirtyRegion));
	            if (!copyback.isEmpty()) {
	                // 拷贝
	                copyBlt(backBuffer, frontBuffer, copyback, &fenceFd);
	            }
	        } else {
	            // 如果不能拷贝，那就整块绘制，终于找到了入口 入江口 入口啊
	            newDirtyRegion.set(bounds);
	            mDirtyRegion.clear();
	            Mutex::Autolock lock(mMutex);
	            for (size_t i=0 ; i<NUM_BUFFER_SLOTS ; i++) {
	                mSlots[i].dirtyRegion.clear();
	            }
	        }
      ....
	}

对于通过lockCanvas获取的内存，要么被上次绘制的UI数据填充，要么整体重绘，如果被上次填充，那么这次就只需要绘制脏区域相关的视图，这就是Android局部重绘的原理。
 
# 总结   

Android View的绘制建立匿名共享内存的基础上，APP端与SurfaceFlinger通过共享内存的方式避免了View视图数据的拷贝，提高了系统同的视图处理能力。
  
# 	参考文档

[参考文档 分配](http://www.voidcn.com/article/p-yxkmayqw-ev.html)       
[Android图形缓冲区分配过程源码分析](http://blog.csdn.net/yangwen123/article/details/12231687)
[Android 图形系统之gralloc](https://www.wolfcstech.com/2017/09/21/android_graphics_gralloc/   )          
[ Android6.0 SurfaceControl分析（二）SurfaceControl和SurfaceFlinger通信](http://blog.csdn.net/kc58236582/article/details/65445141)       
[ GUI系统之SurfaceFlinger(11)SurfaceComposerClient](http://blog.csdn.net/xuesen_lin/article/details/8954957)                 
[ Skia深入分析1——skia上下文](http://blog.csdn.net/jxt1234and2010/article/details/42572559)        
[ Android图形显示系统——概述](http://blog.csdn.net/jxt1234and2010/article/details/44164691)           
[Linux环境进程间通信（五）: 共享内存（下）](https://www.ibm.com/developerworks/cn/linux/l-ipc/part5/index2.html)      
[Android Binder 分析——匿名共享内存（Ashmem）
By Mingming](http://light3moon.com/2015/01/28/Android%20Binder%20%E5%88%86%E6%9E%90%E2%80%94%E2%80%94%E5%8C%BF%E5%90%8D%E5%85%B1%E4%BA%AB%E5%86%85%E5%AD%98[Ashmem]/)     
[Android 匿名共享内存驱动源码分析](http://blog.csdn.net/yangwen123/article/details/9318319)       
[ Android窗口管理服务WindowManagerService的简要介绍和学习计划](http://blog.csdn.net/luoshengyang/article/details/8462738)                      
[Android4.2.2 SurfaceFlinger之图形渲染queueBuffer实现和VSYNC的存在感](http://blog.csdn.net/gzzaigcnforever/article/details/22046141)          
[Android6.0 显示系统GraphicBuffer分配内存](http://www.voidcn.com/blog/kc58236582/article/p-6238474.html)   
[InputManagerService分析一：IMS的启动与事件传递](http://blog.csdn.net/lilian0118/article/details/28617185)        
[Android 5.0(Lollipop)事件输入系统(Input System)](http://blog.csdn.net/jinzhuojun/article/details/41909159)    
[浅析 Android 的窗口](https://dev.qq.com/topic/5923ef85bdc9739041a4a798)      
[ 【Linux】进程间通信（IPC）之共享内存详解与测试用例](http://blog.csdn.net/a1414345/article/details/69389647)     
[Android6.0 显示系统（三） 管理图像缓冲区](http://blog.csdn.net/kc58236582/article/details/52681363)       
