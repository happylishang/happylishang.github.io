---
layout: post
title: 从addView到View的显示过程
category: Android
image: 

---

他们写的我都没看懂，

在View显示中，我所遇到的主观问题：

*  主线程绘制View的说法
*  View显示时机，makeVisible 耗费主线程吗？
*  如何调用2D图形图Skia，不要太深入bitmap图形合成原理，只关心内存的分配与处理
*  共享内存的传递，与处理
*  Activity是显示View的唯一方式吗？
*  什么时候，从当前线程托付给其他线程与服务，比如当前线程View的bitmap绘制完毕，什么时候使用共享内存通吃其他surfaceFliner混排Window
*  Activity中View绘制所占用的内存什么时候释放，是不是Activity界面大块的越多，占用View的内存越多，只要Activity不销毁，View就会被强引用获取   
*  GraphicBuffer支持跨进程，共享内存是内核空间分配内存吗？
*  fd的传递（跨进程）

1、如何申请可以用来送显的内存，如何将其送往LCD？ 2、如何提供窗口系统？ 3、如何同步合成/显示多个图层？ 4、如何支持多屏？

* SurfaceFlinger是Android里面用于提供图层合成的服务，负责给应用层提供窗口，并按指定位置合成所有图层到屏幕。
* 视图的重绘流程，第一次绘制流程 


# View绘制所占用的内存如何共享

这个问题是我比较关心的问题，我们知道View在绘制后，APP占用的内存会上升，那么这个内存我们理所当然的认为是在APP的进程内分配的，但同时，SurfaceFlinger也需要这份内存进行图层高度合成，那么这两份内存是同一份吗？如果不是同一份，数据的传递是不是太大了，不会不会造成浪费。这里的内存就是匿名共享内存，是同一份，这个机制利用了Linux的tmpfs系统，具体的原理不想太深就，完全属于Linux IPC通信的东西，可以自己翻内核，对于理解Android只要清楚这两份内存是同一份，同一份内存是如何传递的呢，就是通过共享fd，文件操作符，tmpfs将共享内存抽象成文件，对于共享内存的操作，就如同对于文件的操作，可以通过map将数据映射到自己的进程空间，直接进行操作，当然要自己处理同步与互斥问题，ashmem_pin_region和ashmem_unpin_region就是同步用的（Android系统的运行时库提到了执行匿名共享内存的锁定和解锁操作的两个函数 ）。几个关注点


* 视图的绘制与更新 skia库
* fd的传递
* 渲染的点
* 图层的合成、合成的命令由谁发出
* 不同的图层，每次合成几个图层（悬浮Activity）
* ViewRootImpl、WMS与SurfaceFlinger分工
* surfaceView与窗口的关系
* WMS的作用
* Cient将UI绘制到内存，如何通知SurfaceFlinger混排Window（Signal），并且绘制到窗口的呢 WMS在SurfaceFlinger混排窗口中起到什么作用呢
 
 ![View绘制与共享内存.jpg](http://upload-images.jianshu.io/upload_images/1460468-103d49829291e1f7.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# 共享内存属于文件范畴

进程间需要共享的数据被放在一个叫做IPC共享内存区域的地方，所有需要访问该共享区域的进程都要把该共享区域映射到本进程的地址空间中去。系统V共享内存通过shmget获得或创建一个IPC共享内存区域，并返回相应的标识符。内核在保证shmget获得或创建一个共享内存区，初始化该共享内存区相应的shmid_kernel结构注同时，还将**在特殊文件系统shm中，创建并打开一个同名文件**，并在内存中建立起该文件的相应dentry及inode结构，**新打开的文件不属于任何一个进程（任何进程都可以访问该共享内存区）**。所有这一切都是系统调用shmget完成的。 
[Linux环境进程间通信（五）: 共享内存（下）](https://www.ibm.com/developerworks/cn/linux/l-ipc/part5/index2.html)      


# Activity是显示View的唯一方式吗？

答案肯定是否定的，Activity只是View显示的一种方式，但是不是唯一的。不用Activity照样显示，只不过Activity封装了一些生命周期之类的处理，让View的显示分成多个阶段，在不同的阶段，给开发者更多的操作空间，另外，省却了开发者主动添加View，并且方便了Window窗口管理，这里。


handlerresume

	 final void handleResumeActivity(IBinder token,
	            boolean clearHide, boolean isForward, boolean reallyResume, int seq, String reason) {
	        ActivityClientRecord r = mActivities.get(token);
	        if (!checkAndUpdateLifecycleSeq(seq, r, "resumeActivity")) {
	            return;
	        }
	        unscheduleGcIdler();
	        mSomeActivitiesChanged = true;

	        r = performResumeActivity(token, clearHide, reason);
	
	        if (r != null) {
	            final Activity a = r.activity;
	            final int forwardBit = isForward ?
	                    WindowManager.LayoutParams.SOFT_INPUT_IS_FORWARD_NAVIGATION : 0;
	            boolean willBeVisible = !a.mStartedActivity;
	            if (!willBeVisible) {
	                try {
	                    willBeVisible = ActivityManagerNative.getDefault().willActivityBeVisible(
	                            a.getActivityToken());
	                } catch (RemoteException e) {
	                    throw e.rethrowFromSystemServer();
	                }
	            }
	            if (r.window == null && !a.mFinished && willBeVisible) {
	                r.window = r.activity.getWindow();
	                View decor = r.window.getDecorView();
	                decor.setVisibility(View.INVISIBLE);
	                ViewManager wm = a.getWindowManager();
	                WindowManager.LayoutParams l = r.window.getAttributes();
	                a.mDecor = decor;
	                l.type = WindowManager.LayoutParams.TYPE_BASE_APPLICATION;
	                l.softInputMode |= forwardBit;
	                if (r.mPreserveWindow) {
	                    a.mWindowAdded = true;
	                    r.mPreserveWindow = false;
	                    ViewRootImpl impl = decor.getViewRootImpl();
	                    if (impl != null) {
	                        impl.notifyChildRebuilt();
	                    }
	                }
	
	                // 为什么不会添加window
	                if (a.mVisibleFromClient && !a.mWindowAdded) {
	                    a.mWindowAdded = true;
	                    wm.addView(decor, l);
	                }
                
WindowManager在onResume的时候向Window添加View，那为什么不会添加两次，因为a.mWindowAdded 添加后会被设置true。测量与布局。

                if (r.activity.mVisibleFromClient) {
                    r.activity.makeVisible();
                }
                
    void makeVisible() {
        if (!mWindowAdded) {
            ViewManager wm = getWindowManager();
            wm.addView(mDecor, getWindow().getAttributes());
            mWindowAdded = true;
        }
        mDecor.setVisibility(View.VISIBLE);
    }             
    
mDecor.setVisibility会回调onVisibilityChanged，每次前台切换后台，后台切换前台都能有响应的处理， 不过View的绘制时机到底是哪个？

是addWindow吗？很明显是不是，另外View的Bitmap的存储，这部分数据的绘制是什么时候，Window可见的时候，会绘制机选Window窗口，再进行混排，最后显示出来

![onVisibilityChanged.png](http://upload-images.jianshu.io/upload_images/1460468-0ddc969626724148.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# 主线程绘制View的说法

setContentView会新建View类，但是并不会涉及测量绘制，只有显示后才会，

# View显示时机，makeVisible 耗费主线程吗？



setContentView只是用来生成DecorView那一套，但是并未将窗口添加显示

SurfaceFlinger不是系统服务，是系统守护进程，当然也算是系统服务，但是很重要，   SkCanvas其实就是Cavas.java 在native的对象

> Session.java
 
    @Override
    public int addToDisplayWithoutInputChannel(IWindow window, int seq, WindowManager.LayoutParams attrs,
            int viewVisibility, int displayId, Rect outContentInsets, Rect outStableInsets) {
        return mService.addWindow(this, window, seq, attrs, viewVisibility, displayId,
            outContentInsets, outStableInsets, null /* outOutsets */, null);
    }
    
> windowmanagerservice

    public int addWindow(Session session, IWindow client, int seq,
            WindowManager.LayoutParams attrs, int viewVisibility, int displayId,
            Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
            InputChannel outInputChannel) {


            WindowState win = new WindowState(this, session, client, token,
                    attachedWindow, appOp[0], seq, attrs, viewVisibility, displayContent);
            ...
            win.attach();
		}

> WindowState

    void attach() {
        if (WindowManagerService.localLOGV) Slog.v(
            TAG, "Attaching " + this + " token=" + mToken
            + ", list=" + mToken.windows);
        mSession.windowAddedLocked();
    }
   
> Session.java
    
        void windowAddedLocked() {
        if (mSurfaceSession == null) {
            if (WindowManagerService.localLOGV) Slog.v(
                WindowManagerService.TAG, "First window added to " + this + ", creating SurfaceSession");
                // SurfaceSession新建
            mSurfaceSession = new SurfaceSession();
            if (WindowManagerService.SHOW_TRANSACTIONS) Slog.i(
                    WindowManagerService.TAG, "  NEW SURFACE SESSION " + mSurfaceSession);
            mService.mSessions.add(this);
            if (mLastReportedAnimatorScale != mService.getCurrentAnimatorScale()) {
                mService.dispatchNewAnimatorScaleLocked(this);
            }
        }
        mNumWindow++;
    }
    
> SurfaceSession
     
        public SurfaceSession() {
        mNativeClient = nativeCreate();
    }
 
>  android_view_SurfaceSession.cpp
   		
	 // SurfaceComposerClient 的 
	static jlong nativeCreate(JNIEnv* env, jclass clazz) {
	    SurfaceComposerClient* client = new SurfaceComposerClient();
	    client->incStrong((void*)nativeCreate);
	    return reinterpret_cast<jlong>(client);
	}

> SurfaceComposerClient.cpp

	SurfaceComposerClient::SurfaceComposerClient()
	    : mStatus(NO_INIT), mComposer(Composer::getInstance())
	{
	}
	// 单利的，所以只有第一次的时候采用
	void SurfaceComposerClient::onFirstRef() {
	    sp<ISurfaceComposer> sm(ComposerService::getComposerService());
	    if (sm != 0) {
	        sp<ISurfaceComposerClient> conn = sm->createConnection();
	        if (conn != 0) {
	            mClient = conn;
	            mStatus = NO_ERROR;
	        }
	    }
	}

SurfaceFlinger创建Client	

> SurfaceFlinger.java

	sp<ISurfaceComposerClient> SurfaceFlinger::createConnection()
	{
	    sp<ISurfaceComposerClient> bclient;
	    sp<Client> client(new Client(this));
	    status_t err = client->initCheck();
	    if (err == NO_ERROR) {
	        bclient = client;
	    }
	    return bclient;
	}

创建surface的代码

	sp<SurfaceControl> SurfaceComposerClient::createSurface(
	        const String8& name,
	        uint32_t w,
	        uint32_t h,
	        PixelFormat format,
	        uint32_t flags)
	{
	    sp<SurfaceControl> sur;
	    if (mStatus == NO_ERROR) {
	        sp<IBinder> handle;
	        sp<IGraphicBufferProducer> gbp;
	        status_t err = mClient->createSurface(name, w, h, format, flags,
	                &handle, &gbp);
	        if (err == NO_ERROR) {
	            sur = new SurfaceControl(this, handle, gbp);
	        }
	    }
	    return sur;
	}

SurfaceControl转化为surface

    void getSurface(Surface outSurface) {
        outSurface.copyFrom(mSurfaceControl);
    }

	
	static jlong nativeCreateFromSurfaceControl(JNIEnv* env, jclass clazz,
	        jlong surfaceControlNativeObj) {
	    /*
	     * This is used by the WindowManagerService just after constructing
	     * a Surface and is necessary for returning the Surface reference to
	     * the caller. At this point, we should only have a SurfaceControl.
	     */
	
	    sp<SurfaceControl> ctrl(reinterpret_cast<SurfaceControl *>(surfaceControlNativeObj));
	    sp<Surface> surface(ctrl->getSurface());
	    if (surface != NULL) {
	        surface->incStrong(&sRefBaseOwner);
	    }
	    return reinterpret_cast<jlong>(surface.get());
	}
	
	sp<Surface> SurfaceControl::getSurface() const
	{
	    Mutex::Autolock _l(mLock);
	    if (mSurfaceData == 0) {
	        // This surface is always consumed by SurfaceFlinger, so the
	        // producerControlledByApp value doesn't matter; using false.
	        mSurfaceData = new Surface(mGraphicBufferProducer, false);
	    }
	    return mSurfaceData;
	}


 ![Surface的一些类图](http://upload-images.jianshu.io/upload_images/1460468-6b433f387a6bae81.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


# 如何调用2D图形图Skia，不要太深入bitmap图形合成原理，只关心内存的分配与处理

# 共享内存的传递，与处理

ashmem 并像 binder 是 android 重新自己搞的一套东西，而是利用了 linux 的 tmpfs 文件系统。关于 tmpfs 我目前还不算很了解，可以先看下这里的2篇，有个基本的了解：

Linux tmpfs 
linux共享内存的设计

那么大致能够知道，tmpfs 是一种可以基于 ram 或是 swap 的高速文件系统，然后可以拿它来实现不同进程间的内存共享。

然后大致思路和流程是：

Proc A 通过 tmpfs 创建一块共享区域，得到这块区域的 fd（文件描述符）
Proc A 在 fd 上 mmap 一片内存区域到本进程用于共享数据
Proc A 通过某种方法把 fd 倒腾给 Proc B
Proc B 在接到的 fd 上同样 mmap 相同的区域到本进程
然后 A、B 在 mmap 到本进程中的内存中读、写，对方都能看到了
其实核心点就是创建一块共享区域，然后2个进程同时把这片区域 mmap 到本进程，然后读写就像本进程的内存一样。这里要解释下第3步，为什么要倒腾 fd，因为在 linux 中 fd 只是对本进程是唯一的，在 Proc A 中打开一个文件得到一个 fd，但是把这个打开的 fd 直接放到 Proc B 中，Proc B 是无法直接使用的。但是文件是唯一的，就是说一个文件（file）可以被打开多次，每打开一次就有一个 fd（文件描述符），所以对于同一个文件来说，需要某种转化，把 Proc A 中的 fd 转化成 Proc B 中的 fd。这样 Proc B 才能通过 fd mmap 同样的共享内存文件


		case BINDER_TYPE_FD: {
			int target_fd;
			struct file *file;
			<!--关键点1 可以根据fd在当前进程获取到file-->
			file = fget(fp->handle);
			<!--关键点2在目标进程中获取空闲fd-->
			target_fd = task_get_unused_fd_flags(target_proc, O_CLOEXEC);
			<!--关键点3将目标进程的空闲fd与file绑定-->
			task_fd_install(target_proc, target_fd, file);
			fp->handle = target_fd;
		} break;
		
通过以上三步，就完成了fd到目标进程的映射

# 为什么使用匿名共享内存

在Android系统中，匿名共享内存也是进程间通信方式的一种。相比于malloc和anonymous/named mmap等传统的内存分配机制，Ashmem的优势是通过内核驱动提供了辅助内核的内存回收算法机制(pin/unpin)。内存回收算法机制就是当你使用Ashmem分配了一块内存，但是其中某些部分却不会被使用时，那么就可以将这块内存unpin掉。unpin后，内核可以将它对应的物理页面回收，以作他用。你也不用担心进程无法对unpin掉的内存进行再次访问，因为回收后的内存还可以再次被获得(通过缺页handler)，因为unpin操作并不会改变已经 mmap的地址空间。



# tmpfs是一种文件系统，这种文件系统的特殊性在于，其有时候使用ram，有时候使用vm(虚拟内存，磁盘上的交换分区)

个人理解 ：**tmpfs是一种文件系统，不占用进程本身的用户空间与内存空间，那个空间需要使用改文件系统，只需要将文件映射到自己的空间即可操作，使用完，释放即可**

tmpfs是一种基于内存的文件系统，  tmpfs有时候使用rm(物理内存)，有时候使用swap(磁盘一块区域)。根据实际情况进行分配。 rm：物理内存。real memery的简称? 真实内存就是电脑主板上那块内存条，叫做真实内存不为过。swap：交换分区。是硬盘上一块区域


内核空间用1G虚拟地址，用户空间用3G虚拟地址
所以ioremap当然不能分出1G地址供你用(ioreamp的空间大小是有限制的)
一个物理地址，内核调用 ioremap得到一个1G内的虚拟地址，用来操作物理内存
             应用层调用 mmap 得到一个3G内的虚拟地址，用来操作物理内存
 
tmpfs 写的时候，缺页中断，申请内存[Linux 中 mmap() 函数的内存映射问题理解？](https://www.zhihu.com/question/48161206)

linux中默认大小是ram的一半，

tmpfs是一种文件系统，文件是不会占用用户空间，或者内核空间的数据的，如果说通过映射进来，虽然说占用了，但是还是复用一份文件，无论读取与否还是写与否，都是针对同一份文件，而tmpfs比较特殊，属于内存文件，一个内存文件也有node之类的属性，只不过它是存储在内存中，而不是磁盘中，它的申请与释放也一定更加的紧迫与严谨，方式内存被浪费。


# surface.unlockCanvasAndPost(canvas);真正绘制入口，但是如何触发绘制，这里是不是又牵扯到SurfaceFlinger呢？


如何触发SurfaceFlinger绘制

![](http://wiki.jikexueyuan.com/project/deep-android-v1/images/chapter8/image021.png)

SuraceFlinger是被动绘制，不是主动轮询的，有消息通知绘制才会绘制
![SF工作线程的流程](http://wiki.jikexueyuan.com/project/deep-android-v1/images/chapter8/image024.png)

Activity端在绘制完UI后，将把BackBuffer投递出去以显示。接着上面的流程，这个BackBuffer的编号是0。待Activity投递完后，才会调用signal函数触发SF消费，所以在此之前格局不会发生变化。

[参考文档](http://wiki.jikexueyuan.com/project/deep-android-v1/surface.html)

ViewRoot是Surfac系统甚至UI系统中一个非常关键的类，下面把网上一些关于ViewRoot的问题做个总结，希望这样能帮助读者对ViewRoot有更加清楚的认识。

·  ViewRoot和View类的关系是什么？

ViewRoot是View视图体系的根。每一个Window（注意是Window，比如PhoneWindow）有一个ViewRoot，它的作用是处理layout和View视图体系的绘制。那么视图体系又是什么呢？它包括Views和ViewGroups，也就是SDK中能看到的View类都属于视图体系。根据前面的分析可知，这些View是需要通过draw画出来的。而ViewRoot就是用来draw它们的，ViewRoot本身没有draw/onDraw函数。

·   ViewRoot和它所控制的View及其子View使用同一个Canvas吗？

这个问题的答案就很简单了，我们在ViewRoot的performTraversals中见过。ViewRoot提供Canvas给它所控制的View，所以它们使用同一个Canvas。但Canvas使用的内存却不是固定的，而是通过Surface的lockCanvas得到的。

·  View、Surface和Canvas之间的关系是怎样的？我认为，每一个view将和一个canvas，以及一个surface绑定到一起（这里的“我”表示提问人）。

这个问题的答案也很简单。一个Window将和一个Surface绑定在一起，绘制前ViewRoot会从Surface中lock出一个Canvas。

·  Canvas有一个bitmap，那么绘制UI时，数据是画在Canvas的这个bitmap中吗？

答案是肯定的，bitmap实际上包括了一块内存，绘制的数据最终都在这块内存上。 

·   同一个ViewRoot下，不同类型的View（不同类型指不同的UI单元，例如按钮、文本框等）使用同一个Surface吗？

是的，但是SurfaceView要除外。因为SurfaceView的绘制一般在单独的线程上，并且由应用层主动调用lockCanvas、draw和unlockCanvasAndPost来完成绘制流程。应用层相当于抛开了ViewRoot的控制，直接和屏幕打交道，这在camera、video方面用得最多



真正绘制的入口


	static void nativeUnlockCanvasAndPost(JNIEnv* env, jclass clazz,
	        jlong nativeObject, jobject canvasObj) {
	    sp<Surface> surface(reinterpret_cast<Surface *>(nativeObject));
	    if (!isSurfaceValid(surface)) {
	        return;
	    }
	
	    // detach the canvas from the surface
	    Canvas* nativeCanvas = GraphicsJNI::getNativeCanvas(env, canvasObj);
	    nativeCanvas->setBitmap(SkBitmap());
	
	    // unlock surface
	    status_t err = surface->unlockAndPost();
	    if (err < 0) {
	        doThrowIAE(env);
	    }
	}

	status_t Surface::unlockAndPost()
	{
	    if (mLockedBuffer == 0) {
	        ALOGE("Surface::unlockAndPost failed, no locked buffer");
	        return INVALID_OPERATION;
	    }
	
	    int fd = -1;
	    status_t err = mLockedBuffer->unlockAsync(&fd);
	    ALOGE_IF(err, "failed unlocking buffer (%p)", mLockedBuffer->handle);
	
	    err = queueBuffer(mLockedBuffer.get(), fd);
	    ALOGE_IF(err, "queueBuffer (handle=%p) failed (%s)",
	            mLockedBuffer->handle, strerror(-err));
	
	    mPostedBuffer = mLockedBuffer;
	    mLockedBuffer = 0;
	    return err;
	}

这里应该就跟WMS没啥关系了，SurfaceFling，要处理吗？


# Toast的token为null， 

如果自己写就算 token非空也不收影响，因为是系统窗口。

Application 的Token为 getSystemService，所以如果Dialog用Application的context就会崩溃

1）在 Window System 中，分为两部分的内容，一部分是运行在系统服务进程（WmS 所在进程）的 WmS 及相关类，另一部分是运行在应用进程的 WindowManagerImpl, WindowManagerGlobal，ViewRootImpl 等相关类。WmS 用 WindowState 来描述一个窗口，而应用进程用 ViewRootImpl，WindowManager.LayoutParms 来描述一个窗口的相关内容。

（2）对于 WmS 来讲，窗口对应一个 View 对象，而不是 Window 对象。添加一个窗口，就是通过 WindowManager 的 addView 方法。同样的，移除一个窗口，就是通过 removeView 方法。更新一个窗口的属性，通过 updateViewLayout 方法。

（3）Window 类描述是一类具有某种通用特性的窗口，其实现类是 PhoneWindow。Activity 对应的窗口，以及 Dialog 对应的窗口，会对应一个 PhoneWindow 对象。PhoneWindow 类把一些操作的统一处理了，例如长按，按”Back”键等。

Android Framework 把窗口分为三种类型，应用窗口，子窗口以及系统窗口。不同类型的窗口，在执行添加窗口操作时，对于 WindowManager.LayoutParams 中的参数 token 具有不同的要求。应用窗口，LayoutParams 中的 token，必须是某个有效的 Activity 的 mToken。而子窗口，LayoutParams 中的 token，必须是父窗口的 ViewRootImpl 中的 W 对象。系统窗口，有些系统窗口不需要 token，有些系统窗口的 token 必须满足一定的要求。

只能通过 Context.getSystemServer 来获取 WindowManager（即获取一个 WindowManagerImpl 的实例）。如果这个 context 是 Activity，则直接返回了 Activity 的 mWindowManager，其 WindowManagerImpl.mParentWindow 就是这个 Activity 本身对应的 PhoneWindow。如果这个 context 是 Application，或者 Service，则直接返回一个 WindowManagerImpl 的实例，而且 mParentWindow 为 null。

（6）在调用 WindowManagerImpl 的 addView 之前，如果没有给 token 赋值，则会走默认的 token 赋值逻辑。默认的 token 赋值逻辑是这样的，如果 mParentWindow 不为空，则会调用其 adjustLayoutParamsForSubWindow 方法。在 adjustLayoutParamsForSubWindow 方法中，如果当前要添加的窗口是，应用窗口，如果其 token 为空，则会把当前 PhoneWindow 的 mToken 赋值给 token。如果是子窗口，则会把当前 PhonwWindow 对应的 DecorView 的 mAttachInfo 中的 mWindowToken 赋值给 token。而 View 中的 AttachInfo mAttachIno 来自 ViewRootImpl 的 mAttachInfo。因此这个 token 本质就是父窗口的 ViewRootImpl 中的 W 类对象。
	
# 为什么Dialog的Token不能为空

Dialog的窗口类型同Activity类型是应用窗口，所以TOken不能为null，否则wms会出错，
popwindow也是个独立的窗口，有个windowstate但是，它必须 依附父窗口，这个父窗口不必是Actvity，但是token不能为null，这也是为了管理子window
Toast类系统窗口，可以为null，也可以不为null，系统窗口不走应用窗口的管理逻辑，所以为所谓Token是什么，跟随类型，

# detach from window 与Attach window的时机
	
	performTraversals的时候，第一次会dispatchAttachedToWindow
	
    private void performTraversals() {
  
          mWindowAttributesChangesFlag = 0;

        Rect frame = mWinFrame;
        if (mFirst) {
            mAttachInfo.mHasWindowFocus = false;
            mAttachInfo.mWindowVisibility = viewVisibility;
            mAttachInfo.mRecomputeGlobalAttributes = false;
            mLastConfiguration.setTo(host.getResources().getConfiguration());
            mLastSystemUiVisibility = mAttachInfo.mSystemUiVisibility;
            // Set the layout direction if it has not been set before (inherit is the default)
            if (mViewLayoutDirectionInitial == View.LAYOUT_DIRECTION_INHERIT) {
                host.setLayoutDirection(mLastConfiguration.getLayoutDirection());
            }
            host.dispatchAttachedToWindow(mAttachInfo, 0);
            mAttachInfo.mTreeObserver.dispatchOnWindowAttachedChange(true);
            dispatchApplyInsets(host);

        } else {
 
        }

        if (viewVisibilityChanged) {
            mAttachInfo.mWindowVisibility = viewVisibility;
            host.dispatchWindowVisibilityChanged(viewVisibility);
            host.dispatchVisibilityAggregated(viewVisibility == View.VISIBLE);
            if (viewVisibility != View.VISIBLE || mNewSurfaceNeeded) {
                endDragResizing();
      } 
               
 ViewRootImpl 在收到要删除窗口的命令后，会执行以下操作，详细见源码分析：
（1）判断是否可以立即删除窗口，否则会等下次 UI 操作时执行；
（2）确认需要删除窗口时，会执行 doDie 方法，通过 dispatchDetachedFromWindow 通知 View 树，窗口要被删除了；
（3）dispatchDetachedFromWindow 执行以下操作
1、通过 dispatchDetachedFromWindow，通知 View 树，窗口已经移除了，你们已经 detach from window 了。
2、把窗口对应的 HardRender, Surface 给释放了；
3、通过 mWindowSession，通知 WmS，窗口要移除了，WmS 会把跟这个窗口相关的 WindowState，以及 WindowToken 给移除，同时更新其它窗口的显示<br>
4、 通知 Choreographer,这个窗口不需要显示了，跟这个窗口相关的一些UI刷新操作，可以取消了。
（4）当根 View 收到 dispatchDetachedFromWindow 调用后，会遍历View树中的每一个 View，把这个通知传递下来。这样 View 的 mAttachInfo 会清除了，reset 为 null了。               
   
   
#  主动删除view，主动root.die
   
       private void removeViewLocked(int index, boolean immediate) {
        ViewRootImpl root = mRoots.get(index);
        View view = root.getView();

        if (view != null) {
            InputMethodManager imm = InputMethodManager.getInstance();
            if (imm != null) {
                imm.windowDismissed(mViews.get(index).getWindowToken());
            }
        }
        boolean deferred = root.die(immediate);
        if (view != null) {
            view.assignParent(null);
            if (deferred) {
                mDyingViews.add(view);
            }
        }
    }
    boolean die(boolean immediate) {
        // Make sure we do execute immediately if we are in the middle of a traversal or the damage
        // done by dispatchDetachedFromWindow will cause havoc on return.
        if (immediate && !mIsInTraversal) {
            doDie();
            return false;
        }

        if (!mIsDrawing) {
            destroyHardwareRenderer();
        } else {
            Log.e(mTag, "Attempting to destroy the window while drawing!\n" +
                    "  window=" + this + ", title=" + mWindowAttributes.getTitle());
        }
        mHandler.sendEmptyMessage(MSG_DIE);
        return true;
    }
     void doDie() {
        checkThread();
        if (LOCAL_LOGV) Log.v(mTag, "DIE in " + this + " of " + mSurface);
        synchronized (this) {
            if (mRemoved) {
                return;
            }
            mRemoved = true;
            if (mAdded) {
                dispatchDetachedFromWindow();
            }

            if (mAdded && !mFirst) {
                destroyHardwareRenderer();

                if (mView != null) {
                    int viewVisibility = mView.getVisibility();
                    boolean viewVisibilityChanged = mViewVisibility != viewVisibility;
                    if (mWindowAttributesChanged || viewVisibilityChanged) {
                        // If layout params have been changed, first give them
                        // to the window manager to make sure it has the correct
                        // animation info.
                        try {
                            if ((relayoutWindow(mWindowAttributes, viewVisibility, false)
                                    & WindowManagerGlobal.RELAYOUT_RES_FIRST_TIME) != 0) {
                                mWindowSession.finishDrawing(mWindow);
                            }
                        } catch (RemoteException e) {
                        }
                    }

                    mSurface.release();
                }
            }

            mAdded = false;
        }
        WindowManagerGlobal.getInstance().doRemoveView(this);
    }
 
 删除view   
 
#  Activity声明周期跟Window现实隐藏的关系

addwindow，
可见
不可见
Activity 回调 visible attctch detach

    private void handleStopActivity(IBinder token, boolean show, int configChanges, int seq) {
        ActivityClientRecord r = mActivities.get(token);
        if (!checkAndUpdateLifecycleSeq(seq, r, "stopActivity")) {
            return;
        }
        r.activity.mConfigChangeFlags |= configChanges;

        StopInfo info = new StopInfo();
        performStopActivityInner(r, info, show, true, "handleStopActivity");

        if (localLOGV) Slog.v(
            TAG, "Finishing stop of " + r + ": show=" + show
            + " win=" + r.window);
// 处理可见性
        updateVisibility(r, show);
        
        
        
            private void updateVisibility(ActivityClientRecord r, boolean show) {
        View v = r.activity.mDecor;
        if (v != null) {
            if (show) {
                if (!r.activity.mVisibleFromServer) {
                    r.activity.mVisibleFromServer = true;
                    mNumVisibleActivities++;
                    if (r.activity.mVisibleFromClient) {
                        r.activity.makeVisible();
                    }
                }
                if (r.newConfig != null) {
                    performConfigurationChangedForActivity(r, r.newConfig, REPORT_TO_ACTIVITY);
                    if (DEBUG_CONFIGURATION) Slog.v(TAG, "Updating activity vis "
                            + r.activityInfo.name + " with new config "
                            + r.activity.mCurrentConfig);
                    r.newConfig = null;
                }
            } else {

                // 更新DecorView的显示
                if (r.activity.mVisibleFromServer) {
                    r.activity.mVisibleFromServer = false;
                    mNumVisibleActivities--;
                    v.setVisibility(View.INVISIBLE);
                }
            }
        }
    }
    
消息驱动型UI更新界面
     
stop Activity对于WMS的影响，将WIndow设置为不可见，不用SurfaceFlinger混排了吗？     
     
    final void stopActivityLocked(ActivityRecord r) {
        if (DEBUG_SWITCH) Slog.d(TAG_SWITCH, "Stopping: " + r);
        if ((r.intent.getFlags()&Intent.FLAG_ACTIVITY_NO_HISTORY) != 0
                || (r.info.flags&ActivityInfo.FLAG_NO_HISTORY) != 0) {
            if (!r.finishing) {
                if (!mService.isSleepingLocked()) {
                    if (DEBUG_STATES) Slog.d(TAG_STATES, "no-history finish of " + r);
                    if (requestFinishActivityLocked(r.appToken, Activity.RESULT_CANCELED, null,
                            "stop-no-history", false)) {
                        // Activity was finished, no need to continue trying to schedule stop.
                        adjustFocusedActivityLocked(r, "stopActivityFinished");
                        r.resumeKeyDispatchingLocked();
                        return;
                    }
                } else {
                    if (DEBUG_STATES) Slog.d(TAG_STATES, "Not finishing noHistory " + r
                            + " on stop because we're just sleeping");
                }
            }
        }

        if (r.app != null && r.app.thread != null) {
            adjustFocusedActivityLocked(r, "stopActivity");
            r.resumeKeyDispatchingLocked();
            try {
                r.stopped = false;
                if (DEBUG_STATES) Slog.v(TAG_STATES,
                        "Moving to STOPPING: " + r + " (stop requested)");
                r.state = ActivityState.STOPPING;
                if (DEBUG_VISIBILITY) Slog.v(TAG_VISIBILITY,
                        "Stopping visible=" + r.visible + " for " + r);
                if (!r.visible) {
                    // stop的花，就需要将window设置为不可见，
                    // 注意这里的mWindowManager就是windowmangerservice对象
                    mWindowManager.setAppVisibility(r.appToken, false);
                }
                        
  WMS  控制Window可见性
           
     @Override
    public void setAppVisibility(IBinder token, boolean visible)
    

type   |  handle  | hint | flag | tr | blnd |   format    |     source crop (l,t,r,b)      |          frame         | name 
-----------+----------+------+------+----+------+-------------+--------------------------------+------------------------+------
       HWC | b66d6a50 | 0002 | 0000 | 00 | 0100 | RGBA_8888   |    0.0,    0.0, 1080.0, 1920.0 |    0,    0, 1080, 1920 | com.snail.labaffinity/com.snail.labaffinity.activity.MainActivity
       HWC | b66d6f50 | 0002 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,  384.0,  132.0 |  348, 1452,  732, 1584 | Toast
       HWC | b66d63c0 | 0002 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 1080.0,   72.0 |    0,    0, 1080,   72 | StatusBar
       HWC | b66d6690 | 0002 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 1080.0,  144.0 |    0, 1776, 1080, 1920 | NavigationBar
 FB TARGET | b6a51c10 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 1080.0, 1920.0 |    0,    0, 1080, 1920 | HWC_FRAMEBUFFER_TARGET
 
 
#  为什么要第二个Activity先Resume，上一个Activity才sotp
 
*  1、加快现实速度
*  2、有时候需要第二个先现实，再定第一个要不要被覆盖
    
 注意学习深入理解Android卷I
    
# invalide重新绘制原理    
    
    
	    
	/Users/netease/sourecode/base/native/libs/gui/BufferQueueProducer.cpp:
	  930  
	  931          if (frameAvailableListener != NULL) {
	  932:             frameAvailableListener->onFrameAvailable(item);
	  933          } else if (frameReplacedListener != NULL) {
	  934              frameReplacedListener->onFrameReplaced(item);


 
Layer.cpp

	void Layer::onFrameAvailable(const BufferItem& item) {
	    // Add this buffer from our internal queue tracker
	    { // Autolock scope
	        Mutex::Autolock lock(mQueueItemLock);
	
	        // Reset the frame number tracker when we receive the first buffer after
	        // a frame number reset
	        if (item.mFrameNumber == 1) {
	            mLastFrameNumberReceived = 0;
	        }
	
	        // Ensure that callbacks are handled in order
	        while (item.mFrameNumber != mLastFrameNumberReceived + 1) {
	            status_t result = mQueueItemCondition.waitRelative(mQueueItemLock,
	                    ms2ns(500));
	            if (result != NO_ERROR) {
	                ALOGE("[%s] Timed out waiting on callback", mName.string());
	            }
	        }
	
	        mQueueItems.push_back(item);
	        android_atomic_inc(&mQueuedFrames);
	
	        // Wake up any pending callbacks
	        mLastFrameNumberReceived = item.mFrameNumber;
	        mQueueItemCondition.broadcast();
	    }
	
	    mFlinger->signalLayerUpdate();
	}
	
# Client端本地surface.cpp构造

	static jlong nativeReadFromParcel(JNIEnv* env, jclass clazz,
	        jlong nativeObject, jobject parcelObj) {
	    Parcel* parcel = parcelForJavaObject(env, parcelObj);
	    if (parcel == NULL) {
	        doThrowNPE(env);
	        return 0;
	    }
	
	    android::view::Surface surfaceShim;
	
	    // Calling code in Surface.java has already read the name of the Surface
	    // from the Parcel
	    surfaceShim.readFromParcel(parcel, /*nameAlreadyRead*/true);
	
	    sp<Surface> self(reinterpret_cast<Surface *>(nativeObject));
	
	    // update the Surface only if the underlying IGraphicBufferProducer
	    // has changed.
	    if (self != nullptr
	            && (IInterface::asBinder(self->getIGraphicBufferProducer()) ==
	                    IInterface::asBinder(surfaceShim.graphicBufferProducer))) {
	        // same IGraphicBufferProducer, return ourselves
	        return jlong(self.get());
	    }
	
	    sp<Surface> sur;
	    if (surfaceShim.graphicBufferProducer != nullptr) {
	        // we have a new IGraphicBufferProducer, create a new Surface for it
	        sur = new Surface(surfaceShim.graphicBufferProducer, true);
	        // and keep a reference before passing to java
	        sur->incStrong(&sRefBaseOwner);
	    }
	
	    if (self != NULL) {
	        // and loose the java reference to ourselves
	        self->decStrong(&sRefBaseOwner);
	    }
	
	    return jlong(sur.get());
	}

# canvas bitmap跟surface关系，内存分配的实际，管理，用户空间的内存不断扩大的原因

# SufaceView都有独立的绘图表面

SurfaceView的拥有独立的Surface，或者可以使用两块，如果不往这块surface上绘制东西，底层是surface会被默认会绘制成黑色，SufaceView本来就是view

好像要mSurfaceHolder.unlockCanvasAndPost(canvas);//解锁画布，提交画好的图像，之后才能算有

type   |  handle  | hint | flag | tr | blnd |   format    |     source crop (l,t,r,b)      |          frame         | name 
-----------+----------+------+------+----+------+-------------+--------------------------------+------------------------+------
       HWC | b66d6820 | 0002 | 0000 | 00 | 0100 | RGB_565     |    0.0,    0.0, 1080.0, 1536.0 |    0,  240, 1080, 1776 | SurfaceView
       HWC | b4d425a0 | 0002 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 1080.0, 1920.0 |    0,    0, 1080, 1920 | com.snail.labaffinity/com.snail.labaffinity.activity.ColorThreadSurfaceActivity
       HWC | b66d6190 | 0002 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 1080.0,   72.0 |    0,    0, 1080,   72 | StatusBar
       HWC | b66d6870 | 0002 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 1080.0,  144.0 |    0, 1776, 1080, 1920 | NavigationBar
 FB TARGET | b6a4fb40 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 1080.0, 1920.0 |    0,    0, 1080, 1920 | HWC_FRAMEBUFFER_TARGET
 

# 内存分配跟释放是SurfaceFlinger管理？

Bitmap是Java层内存，绘图表面占用的内存跟bitmap不同
[ Android4.2.2 SurfaceFlinger之图形缓存区申请与分配dequeueBuffer](http://blog.csdn.net/gzzaigcnforever/article/details/21892067)
  
  
  Client -> creatSurface -> surfaceflinger ->createLayer->new layer- >onfirstRef > 
  
  
	  void Layer::onFirstRef() {
	    // Creates a custom BufferQueue for SurfaceFlingerConsumer to use
	    sp<IGraphicBufferProducer> producer;
	    sp<IGraphicBufferConsumer> consumer;
	    BufferQueue::createBufferQueue(&producer, &consumer);
	    mProducer = new MonitoredProducer(producer, mFlinger);
	    mSurfaceFlingerConsumer = new SurfaceFlingerConsumer(consumer, mTextureName,
	            this);
	    mSurfaceFlingerConsumer->setConsumerUsageBits(getEffectiveUsage(0));
	    mSurfaceFlingerConsumer->setContentsChangedListener(this);
	    mSurfaceFlingerConsumer->setName(mName);
	<!--是否最对限制两个缓存-->
	#ifndef TARGET_DISABLE_TRIPLE_BUFFERING
	    mProducer->setMaxDequeuedBufferCount(2);
	#endif
	
	    const sp<const DisplayDevice> hw(mFlinger->getDefaultDisplayDevice());
	    updateTransformHint(hw);
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
	
 [调用alloc分配了一块共享的内存缓冲区，alloc函数将返回共享区的fd和缓冲区的指针](http://www.voidcn.com/blog/kc58236582/article/p-6219365.html)
 
 
#  Surface.java如何同surfaceFlinger通信，绘制完成，如何人通知Surfacelinger绘制呢？
 
	 
	 status_t Surface::writeToParcel(Parcel* parcel, bool nameAlreadyWritten) const {
	    if (parcel == nullptr) return BAD_VALUE;
	
	    status_t res = OK;
	
	    if (!nameAlreadyWritten) {
	        res = parcel->writeString16(name);
	        if (res != OK) return res;
	
	        /* isSingleBuffered defaults to no */
	        res = parcel->writeInt32(0);
	        if (res != OK) return res;
	    }
	// 注意，这里是将 IGraphicBufferProducer::asBinder 写入到 了
	    res = parcel->writeStrongBinder(
	            IGraphicBufferProducer::asBinder(graphicBufferProducer));
	
	    return res;
	}

Surface可以理解为一张画布，那么Surface为何要和一个缓冲区队列相关呢？在播放动画时，美妙至少要播放24帧画面才能形成比较真实的动画效果。而这些数据是通过cpu解码得到的，准备他们需要时间。对于图像显示设备而言，刷新周期是固定的，我们必须要在它需要数据的时候把数据准备好。视频播放的每一帧也需要在指定的时间播放，因此解码器会提前准备好一批数据，这些数据保存在解码器内存的缓冲区中，当时间到达是，解码器会把内部缓冲区的图像复制到Surface中，但是显示设备并不是立刻就把数据取走的，因此Surface也需要缓冲区来临时保存数据。

# 分配内存的时机，是surface lock的时候，dequeueBuffer ，当然，可能已经分配过，unlockAndPost是绘制完成，请求SF进行图层混合。或者说，真正进行绘制的时候，才会分配共享内存

####   IGraphicBufferProducer是应用进程同SF通信的关键  IGraphicBufferProducer是新建Surface的时候，回传回来的

#### IwindowSession是应用同WMS通信的窗口

#### IWindow是WMS同APP通信窗口 


通过Surface里的远程通信类IGraphicBufferProducer对象向SF发送消息，请求分配内存，发送之后，内存分配，并回传fd，映射内存，之后，就可以使用内存进行绘图了。Surface Parcelable readFromParcel。
	
	static jint nativeReadFromParcel(JNIEnv* env, jclass clazz,
	        jint nativeObject, jobject parcelObj) {
	    Parcel* parcel = parcelForJavaObject(env, parcelObj);
	    if (parcel == NULL) {
	        doThrowNPE(env);
	        return 0;
	    }
	
	    sp<Surface> self(reinterpret_cast<Surface *>(nativeObject));
	    sp<IBinder> binder(parcel->readStrongBinder());
	
	    // update the Surface only if the underlying IGraphicBufferProducer
	    // has changed.
	    if (self != NULL
	            && (self->getIGraphicBufferProducer()->asBinder() == binder)) {
	        // same IGraphicBufferProducer, return ourselves
	        return int(self.get());
	    }
	
	    sp<Surface> sur;
	    sp<IGraphicBufferProducer> gbp(interface_cast<IGraphicBufferProducer>(binder));
	    if (gbp != NULL) {
	        // we have a new IGraphicBufferProducer, create a new Surface for it
	        sur = new Surface(gbp);
	        // and keep a reference before passing to java
	        sur->incStrong(&sRefBaseOwner);
	    }
	
	    if (self != NULL) {
	        // and loose the java reference to ourselves
	        self->decStrong(&sRefBaseOwner);
	    }
	
	    return int(sur.get());
	}



	status_t Surface::lock(
	        ANativeWindow_Buffer* outBuffer, ARect* inOutDirtyBounds)
	{
	    if (mLockedBuffer != 0) {
	        ALOGE("Surface::lock failed, already locked");
	        return INVALID_OPERATION;
	    }
	
	// connect
	    
	    if (!mConnectedToCpu) {
	        int err = Surface::connect(NATIVE_WINDOW_API_CPU);
	        if (err) {
	            return err;
	        }
	        setUsage(GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
	    }
	
	    ANativeWindowBuffer* out;
	    int fenceFd = -1;
	    status_t err = dequeueBuffer(&out, &fenceFd);

在Layer的onFirstRef函数中，调用了下面函数，创建了3个对象BufferQueueCore BufferQueueProducer BufferQueueConsumer。 每个Layer有自己的 BufferQueueCore、BufferQueueProducer、BufferQueueConsumer

	
	status_t BufferQueueProducer::dequeueBuffer(int *outSlot,
	        sp<android::Fence> *outFence, uint32_t width, uint32_t height,
	        PixelFormat format, uint32_t usage) {
	    ATRACE_CALL();
	    。。。
	            sp<GraphicBuffer> graphicBuffer(mCore->mAllocator->createGraphicBuffer(
                width, height, format, usage,
                {mConsumerName.string(), mConsumerName.size()}, &error));
                
                
     sp<GraphicBuffer> GraphicBufferAlloc::createGraphicBuffer(uint32_t width,
        uint32_t height, PixelFormat format, uint32_t usage,
        std::string requestorName, status_t* error) {
    sp<GraphicBuffer> graphicBuffer(new GraphicBuffer(
            width, height, format, usage, std::move(requestorName)));
    status_t err = graphicBuffer->initCheck();
    *error = err;
    if (err != 0 || graphicBuffer->handle == 0) {
        if (err == NO_MEMORY) {
            GraphicBuffer::dumpAllocationsToSystemLog();
        }
 
        return 0;
    }
    return graphicBuffer;
    }           
    
	    GraphicBuffer::GraphicBuffer(uint32_t inWidth, uint32_t inHeight,
	        PixelFormat inFormat, uint32_t inUsage)
	    : BASE(), mOwner(ownData), mBufferMapper(GraphicBufferMapper::get()),
	      mInitCheck(NO_ERROR), mId(getUniqueId())
	{
	    ......
	    mInitCheck = initSize(inWidth, inHeight, inFormat, inUsage);
	}

	status_t GraphicBuffer::initSize(uint32_t inWidth, uint32_t inHeight,
	        PixelFormat inFormat, uint32_t inUsage)
	{
	    GraphicBufferAllocator& allocator = GraphicBufferAllocator::get();
	    uint32_t outStride = 0;
	    status_t err = allocator.alloc(inWidth, inHeight, inFormat, inUsage,
	            &handle, &outStride);
	    if (err == NO_ERROR) {
	        width = static_cast<int>(inWidth);
	        height = static_cast<int>(inHeight);
	        format = inFormat;
	        usage = static_cast<int>(inUsage);
	        stride = static_cast<int>(outStride);
	    }
	    return err;
	}


	status_t GraphicBufferAllocator::alloc(uint32_t width, uint32_t height,
	        PixelFormat format, uint32_t usage, buffer_handle_t* handle,
	        uint32_t* stride)
	{
	    ......
	    err = mAllocDev->alloc(mAllocDev, static_cast<int>(width),
	            static_cast<int>(height), format, static_cast<int>(usage), handle,
	            &outStride);
	            
	            

在GraphicBufferAllocator的构造函数中装载了Gralloc模块，因此mAllocDev指向了Gralloc模块。这个会在后面的博客中分析

	GraphicBufferAllocator::GraphicBufferAllocator()
	    : mAllocDev(0)
	{
	    hw_module_t const* module;
	    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
	    ALOGE_IF(err, "FATAL: can't find the %s module", GRALLOC_HARDWARE_MODULE_ID);
	    if (err == 0) {
	        gralloc_open(module, &mAllocDev);
	    }
	}
	
这里调用alloc分配了一块共享的内存缓冲区，alloc函数将返回共享区的fd和缓冲区的指针。既然GraphicBuffer中的缓冲区是共享内存，我们知道使用共享内存需要传递共享内存的句柄fd。下面我们看看是如何传到客户进程的。

![](http://gityuan.com/images/surfaceFlinger/class_buffer_queue.jpg)

# Gralloc模块

分配共享内存


* 1、view添加wms到如何到显示？wms知道view吗？不知道
* 2、共享内存 = 
* 3、surface 的对应关系 一个surface 、layer 、lock 多个？
* 4、wms SurfaceFlinger的关系，为什么要经过wms，不经过可以吗？ 可以
* 5、不经过AMS可以吗 ，可以
* 6、就算是共享内存，什么时候分配，lock的时候分配，不是同一块吗？
* 7、通信surface绘制完成如何通知，AMS返回给客户端，WMS排序，input管理 surface对应内存，也拿着sf通信的proxy
* 8、WMS为何不拆开，转么处理输入，不太合理
* 9、计算大小WMS做，SurfaceFlinger，只是处理合成、分配内存、绘制、，其余的不考虑
* surface绘制如何到内存，Surface有内存地址，映射到Canvas，skiaCanvas
* surface -》surfacecontol-》client-》layer -》生产消费

## Surface  是客户端同SF进行通信的接口，没有WMS，依旧可以进行图形绘制，

SurfaceView使用的回调，其实就是要准备好Surface才可以，在

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
    
        @Override
    protected void onDetachedFromWindow() {
        if (mGlobalListenersAdded) {
            ViewTreeObserver observer = getViewTreeObserver();
            observer.removeOnScrollChangedListener(mScrollChangedListener);
            observer.removeOnPreDrawListener(mDrawListener);
            mGlobalListenersAdded = false;
        }

        mRequestedVisible = false;
        updateWindow(false, false);
        mHaveFrame = false;
        if (mWindow != null) {
            try {
                mSession.remove(mWindow);
            } catch (RemoteException ex) {
                // Not much we can do here...
            }
            mWindow = null;
        }
        mSession = null;
        mLayout.token = null;

        super.onDetachedFromWindow();
    }
    
     private void updateWindow(boolean force, boolean redrawNeeded) {
        if (!mHaveFrame) {
            return;
        }
        ViewRootImpl viewRoot = getViewRootImpl();
        if (viewRoot != null) {
            mTranslator = viewRoot.mTranslator;
        
                mLayout.height = getHeight();
                if (mTranslator != null) {
                    mTranslator.translateLayoutParamsInAppWindowToScreen(mLayout);
                }
                
                mLayout.format = mRequestedFormat;
                mLayout.flags |=WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
                              | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                              | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS
                              | WindowManager.LayoutParams.FLAG_SCALED
                              | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                              | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
                              ;
                if (!getContext().getResources().getCompatibilityInfo().supportsScreen()) {
                    mLayout.flags |= WindowManager.LayoutParams.FLAG_COMPATIBLE_WINDOW;
                }
                mLayout.privateFlags |= WindowManager.LayoutParams.PRIVATE_FLAG_NO_MOVE_ANIMATION;

                if (mWindow == null) {
                    Display display = getDisplay();
                    mWindow = new MyWindow(this);
                    mLayout.type = mWindowType;
                    mLayout.gravity = Gravity.START|Gravity.TOP;
                    mSession.addToDisplayWithoutInputChannel(mWindow, mWindow.mSeq, mLayout,

                int relayoutResult;

                mSurfaceLock.lock();
                try {
                    mUpdateWindowNeeded = false;
                    reportDrawNeeded = mReportDrawNeeded;
                    mReportDrawNeeded = false;
                    mDrawingStopped = !visible;
    
                    if (DEBUG) Log.i(TAG, "Cur surface: " + mSurface);

                    relayoutResult = mSession.relayout(
                        mWindow, mWindow.mSeq, mLayout, mWidth, mHeight,
                            visible ? VISIBLE : GONE,
                            WindowManagerGlobal.RELAYOUT_DEFER_SURFACE_DESTROY,
                            mWinFrame, mOverscanInsets, mContentInsets,
                            mVisibleInsets, mConfiguration, mNewSurface);
                    if ((relayoutResult & WindowManagerGlobal.RELAYOUT_RES_FIRST_TIME) != 0) {
                        mReportDrawNeeded = true;

 
    }


# View invalidate 更新原理

invalidate函数如果没有重新，没有调用requestLayout，就不会重新测量，不会重新布局
 
[Android View 深度分析requestLayout、invalidate与postInvalidate](http://www.jianshu.com/p/effe9b4333de)          ，
以TExtview的 setText而言，如果测量后，需要重新布局，那就要requestLayout，如果不需要，就算了，只需要draw

整个View树的绘图流程是在ViewRootImpl.Java类的performTraversals()函数展开的，该函数做的执行过程可简单概况为
 根据之前设置的状态，判断是否需要重新计算视图大小(measure)、是否重新需要安置视图的位置(layout)、以及是否需要重绘
 (draw)，其框架过程如下：

    void invalidate(boolean invalidateCache) {
        if (skipInvalidate()) {
            return;
        }
        if ((mPrivateFlags & (PFLAG_DRAWN | PFLAG_HAS_BOUNDS)) == (PFLAG_DRAWN | PFLAG_HAS_BOUNDS) ||
                (invalidateCache && (mPrivateFlags & PFLAG_DRAWING_CACHE_VALID) == PFLAG_DRAWING_CACHE_VALID) ||
                (mPrivateFlags & PFLAG_INVALIDATED) != PFLAG_INVALIDATED || isOpaque() != mLastIsOpaque) {
            mLastIsOpaque = isOpaque();
            mPrivateFlags &= ~PFLAG_DRAWN;
            mPrivateFlags |= PFLAG_DIRTY;
            if (invalidateCache) {
                mPrivateFlags |= PFLAG_INVALIDATED;
                mPrivateFlags &= ~PFLAG_DRAWING_CACHE_VALID;
            }
            final AttachInfo ai = mAttachInfo;
            final ViewParent p = mParent;
            //noinspection PointlessBooleanExpression,ConstantConditions
            if (!HardwareRenderer.RENDER_DIRTY_REGIONS) {
                if (p != null && ai != null && ai.mHardwareAccelerated) {
                    // fast-track for GL-enabled applications; just invalidate the whole hierarchy
                    // with a null dirty rect, which tells the ViewAncestor to redraw everything
                    p.invalidateChild(this, null);
                    return;
                }
            }
	
            if (p != null && ai != null) {
                final Rect r = ai.mTmpInvalRect;
                r.set(0, 0, mRight - mLeft, mBottom - mTop);
                // Don't call invalidate -- we don't want to internally scroll
                // our own bounds
                p.invalidateChild(this, r);
            }
        }
    }
    

ViewrootImpl本来就是ViewParent，在setView的时候，被assign给DecorView。

	public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
	        synchronized (this) {
	      ...
	        view.assignParent(this);

所以最终会调用ViewrootImpl的invalidate。最终

    void invalidate() {
        mDirty.set(0, 0, mWidth, mHeight);
        scheduleTraversals();
    }      

插入紧急异步消息，之前的消息都不执行，一直等到新消息
          
    void scheduleTraversals() {
        if (!mTraversalScheduled) {
            mTraversalScheduled = true;
            mTraversalBarrier = mHandler.getLooper().postSyncBarrier();
            mChoreographer.postCallback(
                    Choreographer.CALLBACK_TRAVERSAL, mTraversalRunnable, null);
            scheduleConsumeBatchedInput();
        }
    }
 
 重绘哪一部分？不能每次都重绘整个View吧？   
 
     void doTraversal() {
        if (mTraversalScheduled) {
            mTraversalScheduled = false;
            mHandler.getLooper().removeSyncBarrier(mTraversalBarrier);

            if (mProfile) {
                Debug.startMethodTracing("ViewAncestor");
            }

            Trace.traceBegin(Trace.TRACE_TAG_VIEW, "performTraversals");
            try {
                performTraversals();
            } finally {
                Trace.traceEnd(Trace.TRACE_TAG_VIEW);
            }

            if (mProfile) {
                Debug.stopMethodTracing();
                mProfile = false;
            }
        }
    }

# // 注意 注意 setView要先执行完，才会执行后面的消息，哪怕他是异步消息
   
   为何requestLayout在addwindow前面，仍然可以有效执行，因为是通过发消息来处理的，requestLayout虽然在前，但是实际的任务却在addwindow的后面，所以不会有问题。
   
    public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
        synchronized (this) {
        ...
        requestLayout();
                if ((mWindowAttributes.inputFeatures
                        & WindowManager.LayoutParams.INPUT_FEATURE_NO_INPUT_CHANNEL) == 0) {
                    mInputChannel = new InputChannel();
                }
                try {
                    mOrigWindowType = mWindowAttributes.type;
                    mAttachInfo.mRecomputeGlobalAttributes = true;
                    collectViewAttributes();
                    // 真正的显示逻辑
                    // 为何mWindow传过去，为了远端跟当前端通信
                    res = mWindowSession.addToDisplay(mWindow, mSeq, mWindowAttributes,
                            getHostVisibility(), mDisplay.getDisplayId(),
                            mAttachInfo.mContentInsets, mInputChannel);
                }
        ...
      }
      
      
# WMS 与PhoneWindow与Touch事件

SystemServer进程中新建InputManagerService

	HandlerThread wmHandlerThread = new HandlerThread("WindowManager");  
	wmHandlerThread.start();  
	Handler wmHandler = new Handler(wmHandlerThread.getLooper());    
	  
	    inputManager = new InputManagerService(context, wmHandler);             
	    wm = WindowManagerService.main(context, power, display, inputManager,  
	            wmHandler, factoryTest != SystemServer.FACTORY_TEST_LOW_LEVEL,  
	            !firstBoot, onlyCore);  
	    ServiceManager.addService(Context.WINDOW_SERVICE, wm);  
	    ServiceManager.addService(Context.INPUT_SERVICE, inputManager);  
	  
	    inputManager.setWindowManagerCallbacks(wm.getInputMonitor());  
    inputManager.start(); 
	
InputManagerService的启动

	static jint nativeInit(JNIEnv* env, jclass clazz,
	        jobject serviceObj, jobject contextObj, jobject messageQueueObj) {
	    sp<MessageQueue> messageQueue = android_os_MessageQueue_getMessageQueue(env, messageQueueObj);
	    if (messageQueue == NULL) {
	        jniThrowRuntimeException(env, "MessageQueue is not initialized.");
	        return 0;
	    }
	
	    NativeInputManager* im = new NativeInputManager(contextObj, serviceObj,
	            messageQueue->getLooper());
	    im->incStrong(0);
	    return reinterpret_cast<jint>(im);
	}
	
因为Java层的MessageQueue总是对应native层的NativeMessageQueue对象，所以首先先取得native层的messageQueue，并构造NativeInputManager对象：

	public class InputManagerService extends IInputManager.Stub	
	
	
	NativeInputManager::NativeInputManager(jobject contextObj,  
	        jobject serviceObj, const sp<Looper>& looper) :  
	        mLooper(looper) {  
	    JNIEnv* env = jniEnv();  
	  
	    mContextObj = env->NewGlobalRef(contextObj);  
	    mServiceObj = env->NewGlobalRef(serviceObj);  
	  
	    {  
	        AutoMutex _l(mLock);  
	        mLocked.systemUiVisibility = ASYSTEM_UI_VISIBILITY_STATUS_BAR_VISIBLE;  
	        mLocked.pointerSpeed = 0;  
	        mLocked.pointerGesturesEnabled = true;  
	        mLocked.showTouches = false;  
	    }  
	  
	    sp<EventHub> eventHub = new EventHub();  
	    mInputManager = new InputManager(eventHub, this, this);  
	} 
	
EventHub是监听的关键类，EventHub采用了管道，我们知道新版本的Looper采用了eventfd实现唤醒，而这里的EventHub还是采用管道
	
	EventHub::EventHub(void) :
	        mBuiltInKeyboardId(NO_BUILT_IN_KEYBOARD), mNextDeviceId(1), mControllerNumbers(),
	        mOpeningDevices(0), mClosingDevices(0),
	        mNeedToSendFinishedDeviceScan(false),
	        mNeedToReopenDevices(false), mNeedToScanDevices(true),
	        mPendingEventCount(0), mPendingEventIndex(0), mPendingINotify(false) {
	    acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKE_LOCK_ID);
	    mEpollFd = epoll_create(EPOLL_SIZE_HINT);
	    mINotifyFd = inotify_init();
	    int result = inotify_add_watch(mINotifyFd, DEVICE_PATH, IN_DELETE | IN_CREATE);
	    struct epoll_event eventItem;
	    memset(&eventItem, 0, sizeof(eventItem));
	    eventItem.events = EPOLLIN;
	    eventItem.data.u32 = EPOLL_ID_INOTIFY;
	    result = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mINotifyFd, &eventItem);
	    int wakeFds[2];
	    result = pipe(wakeFds);
	 	 mWakeReadPipeFd = wakeFds[0];
	    mWakeWritePipeFd = wakeFds[1];
	    result = fcntl(mWakeReadPipeFd, F_SETFL, O_NONBLOCK);
	    result = fcntl(mWakeWritePipeFd, F_SETFL, O_NONBLOCK);	    eventItem.data.u32 = EPOLL_ID_WAKE;
	    result = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mWakeReadPipeFd, &eventItem);
	}
	
	InputManager::InputManager(
        const sp<EventHubInterface>& eventHub,
        const sp<InputReaderPolicyInterface>& readerPolicy,
        const sp<InputDispatcherPolicyInterface>& dispatcherPolicy) {
    mDispatcher = new InputDispatcher(dispatcherPolicy);
    mReader = new InputReader(eventHub, readerPolicy, mDispatcher);
    initialize();
}

InputManagerService不是Android中传统的WMS AMS类Binder服务，InputManagerService更像是守护线程类服务，监听底层事件，将事件分发给上层需求。 epoll事件轮询输入事件

input有个管道，用来监听ANR ？？[Input系统—ANR原理分析](http://gityuan.com/2017/01/01/input-anr/) 利用Watching-Dog

Phonewindow Actvity Dialog PopWindow的触摸事件响应

Actvity Dialog有Phonwindow，里面有Decorview，DecorView在分发事件的时候，会首先利用Phonwindow的callBack，调用Activity或者Dialog的处理，而普通的是没有的，比如Toast类型的，触摸事件事件直接就会发送到View中去

可以有多个窗口接收触摸事件，比如Activity可以同Popwindow懂事接收触摸事件，

# 如何找到对应的窗口呢，或者说如何找到对应的InputChannal，通过管道发送消息

![整体框架图](http://gityuan.com/images/input/input_summary.jpg) 

[整体框架图](http://gityuan.com/2016/12/31/input-ipc/)

不同版本不一样，低版本用的是管道，高版本用的是本地socket，依托Linux的Android是很灵活的，所以有时候，理解其大概原理就行，因为具体的实现方式可能会不断优化。
WMS 在addWindow的时候，会利用           

	 mInputMonitor.setUpdateInputWindowsNeededLw();

    final InputMonitor mInputMonitor = new InputMonitor(this)
    
       public void updateInputWindowsLw(boolean force) {
        if (!force && !mUpdateInputWindowsNeeded) {
            return;
        }
        mUpdateInputWindowsNeeded = false;

        if (false) Slog.d(WindowManagerService.TAG, ">>>>>> ENTERED updateInputWindowsLw");

        // Populate the input window list with information about all of the windows that
        // could potentially receive input.
        // As an optimization, we could try to prune the list of windows but this turns
        // out to be difficult because only the native code knows for sure which window
        // currently has touch focus.
        final WindowStateAnimator universeBackground = mService.mAnimator.mUniverseBackground;
        final int aboveUniverseLayer = mService.mAnimator.mAboveUniverseLayer;
        boolean addedUniverse = false;

        // If there's a drag in flight, provide a pseudowindow to catch drag input
        final boolean inDrag = (mService.mDragState != null);
        if (inDrag) {
            if (WindowManagerService.DEBUG_DRAG) {
                Log.d(WindowManagerService.TAG, "Inserting drag window");
            }
            final InputWindowHandle dragWindowHandle = mService.mDragState.mDragWindowHandle;
            if (dragWindowHandle != null) {
                addInputWindowHandleLw(dragWindowHandle);
            } else {
                Slog.w(WindowManagerService.TAG, "Drag is in progress but there is no "
                        + "drag window handle.");
            }
        }

        final int NFW = mService.mFakeWindows.size();
        for (int i = 0; i < NFW; i++) {
            addInputWindowHandleLw(mService.mFakeWindows.get(i).mWindowHandle);
        }

        // Add all windows on the default display.
        final int numDisplays = mService.mDisplayContents.size();
        for (int displayNdx = 0; displayNdx < numDisplays; ++displayNdx) {
            WindowList windows = mService.mDisplayContents.valueAt(displayNdx).getWindowList();
            for (int winNdx = windows.size() - 1; winNdx >= 0; --winNdx) {
                final WindowState child = windows.get(winNdx);
                final InputChannel inputChannel = child.mInputChannel;
                final InputWindowHandle inputWindowHandle = child.mInputWindowHandle;
                if (inputChannel == null || inputWindowHandle == null || child.mRemoved) {
                    // Skip this window because it cannot possibly receive input.
                    continue;
                }

                final int flags = child.mAttrs.flags;
                final int privateFlags = child.mAttrs.privateFlags;
                final int type = child.mAttrs.type;

                final boolean hasFocus = (child == mInputFocus);
                final boolean isVisible = child.isVisibleLw();
                final boolean hasWallpaper = (child == mService.mWallpaperTarget)
                        && (type != WindowManager.LayoutParams.TYPE_KEYGUARD);
                final boolean onDefaultDisplay = (child.getDisplayId() == Display.DEFAULT_DISPLAY);

                // If there's a drag in progress and 'child' is a potential drop target,
                // make sure it's been told about the drag
                if (inDrag && isVisible && onDefaultDisplay) {
                    mService.mDragState.sendDragStartedIfNeededLw(child);
                }

                if (universeBackground != null && !addedUniverse
                        && child.mBaseLayer < aboveUniverseLayer && onDefaultDisplay) {
                    final WindowState u = universeBackground.mWin;
                    if (u.mInputChannel != null && u.mInputWindowHandle != null) {
                        addInputWindowHandleLw(u.mInputWindowHandle, u, u.mAttrs.flags,
                                u.mAttrs.privateFlags, u.mAttrs.type,
                                true, u == mInputFocus, false);
                    }
                    addedUniverse = true;
                }

                if (child.mWinAnimator != universeBackground) {
                    addInputWindowHandleLw(inputWindowHandle, child, flags, privateFlags, type,
                            isVisible, hasFocus, hasWallpaper);
                }
            }
        }
 
        mService.mInputManager.setInputWindows(mInputWindowHandles);
 
        clearInputWindowHandlesLw();

     }

    private void addInputWindowHandleLw(final InputWindowHandle windowHandle) {
        if (mInputWindowHandles == null) {
            mInputWindowHandles = new InputWindowHandle[16];
        }
        if (mInputWindowHandleCount >= mInputWindowHandles.length) {
            mInputWindowHandles = Arrays.copyOf(mInputWindowHandles,
                    mInputWindowHandleCount * 2);
        }
        mInputWindowHandles[mInputWindowHandleCount++] = windowHandle;
    }
    
   WMS  addWindow-》updateFocusedWindowLocked-》mInputMonitor.updateInputWindowsLw-》mInputManager.setInputWindows-》NativeInputManager::setInputWindows-》getDispatcher()->setInputWindows
   
也就说窗口变化的时候WMS会将需要获取Input事件的窗口告诉InputManager，之后InputDisPatch就能知道需要将事件发送给哪个窗口

void InputDispatcher::setInputWindows

![](http://img.blog.csdn.net/20141213164750258?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQvamluemh1b2p1bg==/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast)

VYNC主要处理的是动画，图形绘制、触摸事件这三种场景

	   public static final int CALLBACK_INPUT = 0;
	
	    /**
	     * Callback type: Animation callback.  Runs before traversals.
	     * @hide
	     */
	    public static final int CALLBACK_ANIMATION = 1;
	
	    /**
	     * Callback type: Traversal callback.  Handles layout and draw.  Runs last
	     * after all other asynchronous messages have been handled.
	     * @hide
	     */
	    public static final int CALLBACK_TRAVERSAL = 2;
 

# WMS 与PhoneWindow与动画  

动画更新原理，动画跟VSYNC的关系比较亲密。
 
*  View动画原理 http://www.cnblogs.com/kross/p/4087780.html] 一句话 ，基于VSYNC不断的重绘
*  属性动画  基于VSYNC不断的重绘
*  窗口动画  http://blog.csdn.net/luoshengyang/article/details/8611754

Activity全屏？ 全屏是Activity？ NO Activity跟Fragment一样，都是View的容器而已，最红呈现的都是View

[Android动画之原理篇（四）](http://gityuan.com/2015/09/06/android-anaimator-4/)

Android中动画，视图更新，Input事件都是通过VSYNC信号来控制的，都是在VSYNC到来之后，才会更新

Choreographer Choreographer 编导，加个塞子，将自己要处理的事件优先级提到最高，等到事件到来，唤醒


#    VSYNC事件分发原理

首先注册到底层硬件或者软件模拟模块，之后，收信号，不断地刷新，处理问题，UI绘制完成，也要等到信号到来，没完成，要等下一个信号

参考文档  http://blog.csdn.net/yangwen123/article/details/16985119
Android VSync信号产生过程源码分析http://blog.csdn.net/yangwen123/article/details/16969907

# Looper的addFd是为了，原来的唤醒的，后来的唤醒与执行，尤其是input的socket   mDataChannel->getFd()，其实VSYNC信号，用的也是DataChannel

收到 后，用DisplayEventReceiver，

不断的产生终端，
                  
# 	参考文档

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