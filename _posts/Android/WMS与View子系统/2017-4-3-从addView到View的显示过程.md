---
layout: post
title: 从addView到View的显示过程
category: Android
image: 

---

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
	
