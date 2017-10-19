---
layout: post
title: Android窗口管理分析（4）：View绘制图层内存的分配
category: Android
image: http://upload-images.jianshu.io/upload_images/1460468-103d49829291e1f7.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---

阅读本文之前，不妨先思考一个问题，APP端View视图的数据是如何传递SurfaceFlinger服务的呢？Android系统中，View绘制的数据最终是按照一帧一帧显示到屏幕的，而每一帧都会占用一定的存储空间，在APP端执行draw的时候，数据很明显是要绘制到APP的进程空间，但是视图窗口要经过SurfaceFlinger图层混排才会生成最终的帧，而SurfaceFlinger又运行在独立的服务进程，那么View视图的数据是如何在两个进程间传递的呢，普通的Binder通信肯定不行，因为Binder不太适合这种数据量比较大的通信，那么View数据的通信采用的是什么IPC手段呢？答案就是共享内存，更精确的说是匿名共享内存。共享内存Linux自带的一种IPC机制，Android直接使用了该模型，在绘制图形的时候，APP进程同SurfaceFlinger共用一块内存，两者采用生产者/消费者模型进行同步，APP端绘制完毕，通知SurfaceFlinger端合成，再输出到硬件进行显示，当然，个中细节会更复杂写，但是流程大概如此，本文不会太过深究各种技术，重点在于描述View绘制共享内存的分配跟管理。

 ![View绘制与共享内存.jpg](http://upload-images.jianshu.io/upload_images/1460468-103d49829291e1f7.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

如此以来，就不需要再次进行数据传递，只要合理的处理同步机制，便可以高效的传递数据，之后的分析按三步走：分配、使用、释放。

## View绘制内存的分配

前文[Window添加流程](http://www.jianshu.com/p/40776c123adb)中描述了：在添加窗口的时候，WMS会为APP分配一个WindowState，以标识当前窗口并用于窗口管理，同时向SurfaceFlinger端请求分配Layer抽象图层，在SurfaceFlinger分配Layer的时候创建了两个比较关键的Binder对象，用于填充WMS端Surface，一个是sp<IBinder> handle：是每个窗口标识的句柄，将来WMS同SurfaceFlinger通信的时候方便找到对应的图层。另一个是sp<IGraphicBufferProducer> gbp ：共享内存分配的关键对象，同时兼具Binder通信的功能，用来传递**指令**及**共享内存的句柄**，注意，这里只是抽象创建了对象，并未真正分配每一帧的内存，内存的分配要等到真正绘制的时候才会申请，首先看一下分配流程：

* 分配的时机：什么时候分配
* 分配的手段：如何分配
* 传递的方式：共享内存的地址如何跨进程传递

Surface被抽象成一块画布，只要拥有Surface就可以绘图，其根本原理就是Surface握有可以绘图的一块内存，这块内存是APP端在需要的时候，通过sp<IGraphicBufferProducer> gbp向SurfaceFlinger申请的，那么首先看一下APP端如何获得sp<IGraphicBufferProducer> gbp这个服务代理的，之后再看如何如何利用它申请内存，在WMS利用向SurfaceFlinger申请填充Surface的时候，会请求SurfaceFlinger分配这把剑，并将其句柄交给自己

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

IGraphicBufferProducer Binder实体在SurfaceFlinger中创建后，打包到Surface对象，并通过binder通信传递给APP端，之后利用Surface恢复出来，如下：

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

自此，APP端就获得了申请内存的能努力，BpGraphicBufferProducer真正发挥作用是在第一次绘图时，看一下ViewRootImpl中的draw

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

最终会调用BpGraphicBufferProducer的dequeueBuffer向服务端请求分配内存，这里用到了匿名共享内存的知识，在Linux中一切都是文件，可以将共享内存看成一个文件。分配成功之后，由于不是在当前进程申请的内存，需要同时映射到surface所在的进程(匿名共享内存)，其实就是获取当前进程可用的文件描述符，而binder驱动也支持fd的传递与转化，只需要跨进程传递文件描述符fd即可（为什么不传递文件名与路径呢？是不是因为是匿名共享内存tmpfs导致的？）。这里对应的Binder实体是BufferQueueProducer，先看下申请的逻辑：
	
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

在client侧，也就是BpGraphicBufferProducer侧，通过DEQUEUE_BUFFER后核心只返回了一个*buf = reply.readInt32();也就是数组mSlots的下标。看来，BufferQueue中应该也有个和mSlots对应的数组，也是32个，一一对应，继续分析server侧，即Bn侧:

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

可以看到BnGraphicBufferProducer端获取到长宽及格式，之后利用BufferQueueProducer的dequeueBuffer来申请内存，内存可能已经申请，也可能未申请，未申请，则直接申请新内存，每个surface可以对应32块内存
		
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
	    width  =
	    height =
	    stride =
	    format =
	    usage  = 0;
	    handle = NULL;
	    mInitCheck = initSize(inWidth, inHeight, inFormat, inUsage,
	            std::move(requestorName));
	}
	
	status_t GraphicBuffer::initSize(uint32_t inWidth, uint32_t inHeight,
	        PixelFormat inFormat, uint32_t inUsage, std::string requestorName)
	{
	    GraphicBufferAllocator& allocator = GraphicBufferAllocator::get();
	    uint32_t outStride = 0;
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

[参考文档 分配](http://www.voidcn.com/article/p-yxkmayqw-ev.html)       

如何从硬件抽象层获取分配函数的

	GraphicBufferAllocator::GraphicBufferAllocator()
	  : mLoader(std::make_unique<Gralloc1::Loader>()),
	    mDevice(mLoader->getDevice()) {}

std::make_unique是C++高版本引入的构造智能指针更优秀的方法：
    
	Loader::Loader()
	  : mDevice(nullptr)
	{
	    hw_module_t const* module;
	    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
	    uint8_t majorVersion = (module->module_api_version >> 8) & 0xFF;
	    uint8_t minorVersion = module->module_api_version & 0xFF;
	    gralloc1_device_t* device = nullptr;
	    if (majorVersion == 1) {
	        gralloc1_open(module, &device);
	    } else {
	        if (!mAdapter) {
	            mAdapter = std::make_unique<Gralloc1On0Adapter>(module);
	        }
	        device = mAdapter->getDevice();
	    }
	    mDevice = std::make_unique<Gralloc1::Device>(device);
	}

这里的device就是利用hw_get_module及gralloc1_open获取到的硬件抽象层device，hw_get_module装载HAL模块，



## 使用

## 释放
                    
# 	参考文档
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
