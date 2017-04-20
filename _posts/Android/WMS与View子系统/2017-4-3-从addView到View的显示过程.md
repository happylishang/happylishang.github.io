---
layout: post
title: 从addView到View的显示过程
category: Android
image: 

---


SurfaceFlinger不是系统服务，是系统守护进程，当然也算是系统服务，但是很重要，

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
	        ALOGE_IF(err, "SurfaceComposerClient::createSurface error %s", strerror(-err));
	        if (err == NO_ERROR) {
	            sur = new SurfaceControl(this, handle, gbp);
	        }
	    }
	    return sur;
	}


# 如何调用2D图形图Skia，不要太深入bitmap图形合成原理，只关心内存的分配与处理

# 共享内存的传递，与处理




	
# 	参考文档
[ GUI系统之SurfaceFlinger(11)SurfaceComposerClient](http://blog.csdn.net/xuesen_lin/article/details/8954957)       
[Android Project Butter分析](http://blog.csdn.net/innost/article/details/8272867)