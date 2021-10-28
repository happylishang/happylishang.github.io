
handleResumeActivity在Resume只有，会添加窗口，这个地方会兜底处理Decorview    
    
	@Override
	    public void handleResumeActivity(IBinder token, boolean finalStateRequest, boolean isForward,
	            String reason) {
	 				...
	        final ActivityClientRecord r = performResumeActivity(token, finalStateRequest, reason);
	         		...
	          	r.window = r.activity.getWindow();
            		View decor = r.window.getDecorView();
	                if (!a.mWindowAdded) {
	                    a.mWindowAdded = true;
	                    wm.addView(decor, l);
	                } else {
	                   
	    @Override
	    public final View getDecorView() {
	        if (mDecor == null || mForceDecorInstall) {
	            installDecor();
	        }
	        return mDecor;
	    }
	    
	    
Activity启动时候一般会调用onCreate回调setContentView，会触发View的inflate，即使不自己set，上面说的 wm.addView前也会线性创建Decorview

    @Override
    public void setContentView(int resId) {
        ensureSubDecor();
        ViewGroup contentParent = mSubDecor.findViewById(android.R.id.content);
        contentParent.removeAllViews();
        LayoutInflater.from(mContext).inflate(resId, contentParent);
        mAppCompatWindowCallback.getWrapped().onContentChanged();
    }
	    
	    
![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/25c4129b2cf448b78aaf7ff021c1392c~tplv-k3u1fbpfcp-watermark.image?)


之后会被addWidow可见，那么绘制是什么时候触发的呢？addView，最后回调WindowManagerGlobal的addView，进而创建ViewRootImpl，利用ViewRootImpl进一步添加Window

	   public void addView(View view, ViewGroup.LayoutParams params,
	            Display display, Window parentWindow) {
	          <!--关键-->
	      	      root = new ViewRootImpl(view.getContext(), display);
	
	            view.setLayoutParams(wparams);
  
	                root.setView(view, wparams, panelParentView);
	     
	        }
	    }

而ViewRootImpl接管流程之后，后续所以View相关的操作都将在ViewRootImpl处理而最终由requestLayout触发测量、布局、绘制的动作

    public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
        synchronized (this) {
            if (mView == null) {

		  		/	/ Schedule the first layout -before- adding to the window
		       	// manager, to make sure we do the relayout before receiving
		                // any other events from the system.
		                requestLayout();
		                
                         <!--添加窗口-->      
                    mOrigWindowType = mWindowAttributes.type;
                    mAttachInfo.mRecomputeGlobalAttributes = true;
                    collectViewAttributes();
                    res = mWindowSession.addToDisplay(mWindow, mSeq, mWindowAttributes,
                            getHostVisibility(), mDisplay.getDisplayId(), mWinFrame,
                            mAttachInfo.mContentInsets, mAttachInfo.mStableInsets,
                            mAttachInfo.mOutsets, mAttachInfo.mDisplayCutout, mInputChannel);
                 
            }
        }
    }

而requestLayout最后调用scheduleTraversals插入异步消息，并doScheduleCallback等待VSYNC信号到来。

    void scheduleTraversals() {
        if (!mTraversalScheduled) {
            mTraversalScheduled = true;
            mTraversalBarrier = mHandler.getLooper().getQueue().postSyncBarrier();
            mChoreographer.postCallback(
                    Choreographer.CALLBACK_TRAVERSAL, mTraversalRunnable, null);
 
    }
  
 之后便会触发绘制逻辑
    
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f5e66d79ca4b4621addec689f02940c9~tplv-k3u1fbpfcp-watermark.image?)


所以整体Message的流程是：Create->Start->Resume->performTraversals->其他消息，所以只要performTraversals之后插入一条消息，其实就可以认为能拿到第一帧时机，onResume之后Post一个即可，为什么呢？因为performTraversals已经占位了。一些onAttachedToWindow与OnWindowFocusChange的回调时机如下图:

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ba357571e08640808e3193fd009c07ba~tplv-k3u1fbpfcp-watermark.image?)

 
 可以看到，其实Resume之后插入一个消息即可，当然OnWindowFocusChange也可以，差别不大。