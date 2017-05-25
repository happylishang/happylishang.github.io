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
* 


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

	
# 	参考文档
[ GUI系统之SurfaceFlinger(11)SurfaceComposerClient](http://blog.csdn.net/xuesen_lin/article/details/8954957)                 
[ Skia深入分析1——skia上下文](http://blog.csdn.net/jxt1234and2010/article/details/42572559)        
[ Android图形显示系统——概述](http://blog.csdn.net/jxt1234and2010/article/details/44164691)           
[Linux环境进程间通信（五）: 共享内存（下）](https://www.ibm.com/developerworks/cn/linux/l-ipc/part5/index2.html)      
[Android Binder 分析——匿名共享内存（Ashmem）
By Mingming](http://light3moon.com/2015/01/28/Android%20Binder%20%E5%88%86%E6%9E%90%E2%80%94%E2%80%94%E5%8C%BF%E5%90%8D%E5%85%B1%E4%BA%AB%E5%86%85%E5%AD%98[Ashmem]/)     
[Android 匿名共享内存驱动源码分析](http://blog.csdn.net/yangwen123/article/details/9318319)
[ Android窗口管理服务WindowManagerService的简要介绍和学习计划](http://blog.csdn.net/luoshengyang/article/details/8462738)         