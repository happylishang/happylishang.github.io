### View invalidate重绘流程

> APP端触发重绘，申请VSYNC流程示意

![image.png](https://upload-images.jianshu.io/upload_images/1460468-f76ea4cbb9a990ba.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

> VSYNC回来流程示意

![image.png](https://upload-images.jianshu.io/upload_images/1460468-050895f38f6527e3.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

> doFrame执行示意图

![image.png](https://upload-images.jianshu.io/upload_images/1460468-4aab950bb9d74094.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)



View内容改变时一般会调用invalidate去触发视图的重绘，从invalidate到UI视图被重绘中间经历了什么呢？假如View调用了invalidate函数，View会递归的调用父容器的invalidateChild，逐级回溯

	
	 void invalidateInternal(int l, int t, int r, int b, boolean invalidateCache,
	            boolean fullInvalidate) {
	            // Propagate the damage rectangle to the parent view.
	            final AttachInfo ai = mAttachInfo;
	            final ViewParent p = mParent;
	            if (p != null && ai != null && l < r && t < b) {
	                final Rect damage = ai.mTmpInvalRect;
	                damage.set(l, t, r, b);
	                p.invalidateChild(this, damage);
	            }
            

scheduleTraversals是重绘的入口

    void invalidate() {
        mDirty.set(0, 0, mWidth, mHeight);
        if (!mWillDrawSoon) {
            scheduleTraversals();
        }
    }
 
scheduleTraversals利用mTraversalScheduled保证，在当前的mTraversalRunnable未被执行前，不会再有新的mTraversalRunnable生效，也就是Choreographer.CALLBACK_TRAVERSAL只有一个mTraversalRunnable，
    
    // 这里是加入到下次垂直同步信号到来的等待callback中去，等待调用，然后遍历
    // mTraversalScheduled用来保证本次Traversals未执行前，不会要求遍历两边，浪费16ms内，不需要绘制两次
    void scheduleTraversals() {
        if (!mTraversalScheduled) {
            mTraversalScheduled = true;
            // 防止同步栅栏，同步栅栏的意思就是拦截同步消息
            mTraversalBarrier = mHandler.getLooper().getQueue().postSyncBarrier();
            // postCallback的时候，顺便请求vnsc垂直同步信号scheduleVsyncLocked
            mChoreographer.postCallback(
                    Choreographer.CALLBACK_TRAVERSAL, mTraversalRunnable, null);
             <!--为什么要添加一个处理触摸事件的回调呢-->
            if (!mUnbufferedInputDispatch) {
                scheduleConsumeBatchedInput();
            }
            notifyRendererOfFramePending();
            pokeDrawLockIfNeeded();
        }
    }

mChoreographer.postCallback在插入CallBack的时候，一般会调用scheduleFrameLocked请求Vsync同步信号
 
 
    // mFrameScheduled保证16ms内，只会申请一次垂直同步信号
    // scheduleFrameLocked可以被调用多次，但是mFrameScheduled保证下一个vsync到来之前，不会有新的请求发出
    // 多余的scheduleFrameLocked调用被无效化
    private void scheduleFrameLocked(long now) {
        if (!mFrameScheduled) {
            mFrameScheduled = true;
            if (USE_VSYNC) {
                if (DEBUG_FRAMES) {
                    Log.d(TAG, "Scheduling next frame on vsync.");
                }

                // If running on the Looper thread, then schedule the vsync immediately,
                // otherwise post a message to schedule the vsync from the UI thread
                // as soon as possible.
                if (isRunningOnLooperThreadLocked()) {
                    scheduleVsyncLocked();
                } else {
                    // 因为invalid已经有了同步栅栏，所以必须mFrameScheduled，消息才能被UI线程执行
                    Message msg = mHandler.obtainMessage(MSG_DO_SCHEDULE_VSYNC);
                    msg.setAsynchronous(true);
                    mHandler.sendMessageAtFrontOfQueue(msg);
                }
            }  
        }
    }
利用mFrameScheduled保证，在一个VSYNC到来之前，不会再去请求新的VSYNC，因为没用。VSYNC到来之后，利用Handler将FrameDisplayEventReceiver封装成一个异步Message，发送到MessageQueue，	
	
	  private final class FrameDisplayEventReceiver extends DisplayEventReceiver
	            implements Runnable {
	        private boolean mHavePendingVsync;
	        private long mTimestampNanos;
	        private int mFrame;
	
	        public FrameDisplayEventReceiver(Looper looper) {
	            super(looper);
	        }
	
	        @Override
	        public void onVsync(long timestampNanos, int builtInDisplayId, int frame) {
	           
	            long now = System.nanoTime();
	            if (timestampNanos > now) {
	            <!--正常情况，timestampNanos不应该大于now，一般是上传vsync的机制出了问题-->
	                timestampNanos = now;
	            }
	            <!--如果上一个vsync同步信号没执行，那就不应该相应下一个（可能是其他线程通过某种方式请求的）-->
		          if (mHavePendingVsync) {
	                Log.w(TAG, "Already have a pending vsync event.  There should only be "
	                        + "one at a time.");
	            } else {
	                mHavePendingVsync = true;
	            }
	            <!--timestampNanos其实是本次vsync产生的时间，从服务端发过来-->
	            mTimestampNanos = timestampNanos;
	            mFrame = frame;
	            Message msg = Message.obtain(mHandler, this);
	            <!--由于已经存在同步栅栏，所以VSYNC到来的Message需要作为异步消息发送过去-->
	            msg.setAsynchronous(true);
	            mHandler.sendMessageAtTime(msg, timestampNanos / TimeUtils.NANOS_PER_MS);
	        }
	
	        @Override
	        public void run() {
	            mHavePendingVsync = false;
	            <!--这里的mTimestampNanos其实就是本次Vynsc同步信号到来的时候，但是执行这个消息的时候，可能延迟了-->
	            doFrame(mTimestampNanos, mFrame);
	        }
	    }
	    
最终调用doFrame进行刷新

    void doFrame(long frameTimeNanos, int frame) {
        final long startNanos;
        synchronized (mLock) {
        <!--做了很多东西，都是为了保证一次16ms有一次垂直同步信号，有一次input 、刷新、重绘-->
            if (!mFrameScheduled) {
                return; // no work to do
            }
           long intendedFrameTimeNanos = frameTimeNanos;
            startNanos = System.nanoTime();
            final long jitterNanos = startNanos - frameTimeNanos;
            <!--检查是否因为延迟执行掉帧，每大于16ms，就多掉一帧-->
            if (jitterNanos >= mFrameIntervalNanos) {
                final long skippedFrames = jitterNanos / mFrameIntervalNanos;
                <!--跳帧，其实就是上一次请求刷新被延迟的时间，但是这里skippedFrames为0不代表没有掉帧-->
                if (skippedFrames >= SKIPPED_FRAME_WARNING_LIMIT) {
                <!--skippedFrames很大一定掉帧，但是为 0，去并非没掉帧-->
                    Log.i(TAG, "Skipped " + skippedFrames + " frames!  "
                            + "The application may be doing too much work on its main thread.");
                }
                final long lastFrameOffset = jitterNanos % mFrameIntervalNanos;
					<!--开始doFrame的真正有效时间戳-->
                frameTimeNanos = startNanos - lastFrameOffset;
            }

            if (frameTimeNanos < mLastFrameTimeNanos) {
                <!--这种情况一般是生成vsync的机制出现了问题，那就再申请一次-->
                scheduleVsyncLocked();
                return;
            }
			  <!--intendedFrameTimeNanos是本来要绘制的时间戳，frameTimeNanos是真正的，可以在渲染工具中标识延迟VSYNC多少-->
            mFrameInfo.setVsync(intendedFrameTimeNanos, frameTimeNanos);
            <!--移除mFrameScheduled判断，说明处理开始了，-->
            mFrameScheduled = false;
            <!--更新mLastFrameTimeNanos-->
            mLastFrameTimeNanos = frameTimeNanos;
        }

        try {
        	 <!--真正开始处理业务-->
            Trace.traceBegin(Trace.TRACE_TAG_VIEW, "Choreographer#doFrame");
			<!--处理打包的move事件-->
            mFrameInfo.markInputHandlingStart();
            doCallbacks(Choreographer.CALLBACK_INPUT, frameTimeNanos);
			<!--处理动画-->
            mFrameInfo.markAnimationsStart();
            doCallbacks(Choreographer.CALLBACK_ANIMATION, frameTimeNanos);
			<!--处理重绘-->
            mFrameInfo.markPerformTraversalsStart();
            doCallbacks(Choreographer.CALLBACK_TRAVERSAL, frameTimeNanos);
			<!--不知道干啥的-->
            doCallbacks(Choreographer.CALLBACK_COMMIT, frameTimeNanos);
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_VIEW);
        }
    }
    
doFrame利用mFrameScheduled保证，每次VSYNC中，只执行一次doFrame，为了16ms只执行一次重绘，加了好多次层保障，doFrame在处理Choreographer.CALLBACK_TRAVERSAL的回调时（mTraversalRunnable），会真正的开始View重绘：
  
      final class TraversalRunnable implements Runnable {
        @Override
        public void run() {
            doTraversal();
        }
    }
    
 调用doTraversal进行遍历，
  
    // 这里是真正执行了，
    void doTraversal() {
        if (mTraversalScheduled) {
            mTraversalScheduled = false;
            <!--移除同步栅栏，只有重绘才设置了栅栏，说明重绘的优先级还是挺高的，所有的同步消息必须让步-->
            mHandler.getLooper().getQueue().removeSyncBarrier(mTraversalBarrier);
            performTraversals();
        }
    }
  
doTraversal会先将栅栏移除，然后处理performTraversals，进行测量、布局、绘制，在这个期间，是可以再次触发invalidate，不过，那是下面一个信号要做的事情了，这样就完成一次重绘。

为什么在
触发-等待VSYNC到来-重绘，每次重绘触发的时候，顺带处理下touch事件，touch跟view重绘可能是两个独立的线路，但是Touch的优先级更高

    Runnable runnable = new Runnable() {
        @Override
        public void run() {
            first.setText("" + System.currentTimeMillis());
            handler.postDelayed(this, 1000 * 10);

        }
    };
    
 另一边，有触摸事件
  
  
    
**最主要的一点：VSYNC同步信号需要用户主动去请求才会接受到，并且是单次有效。**

### 局部重绘原理

应该是UI线程有局部重绘的概念，但是Surface还是全局刷新？？？只是某些DrawOp复用原来的

	 @NonNull
	    public RenderNode updateDisplayListIfDirty() {
	        final RenderNode renderNode = mRenderNode;
	        if (!canHaveDisplayList()) {
	            // can't populate RenderNode, don't try
	            return renderNode;
	        }
	
	        if ((mPrivateFlags & PFLAG_DRAWING_CACHE_VALID) == 0
	                || !renderNode.isValid()
	                || (mRecreateDisplayList)) {
	            // Don't need to recreate the display list, just need to tell our
	            // children to restore/recreate theirs
	            if (renderNode.isValid()
	                    && !mRecreateDisplayList) {
	                mPrivateFlags |= PFLAG_DRAWN | PFLAG_DRAWING_CACHE_VALID;
	                mPrivateFlags &= ~PFLAG_DIRTY_MASK;
	                dispatchGetDisplayList();
	                return renderNode; // no work needed
	            }
	            

### Touch事件原理

* Down事件 直接处理
* Move事件 对于大多数Move事件，结合绘制过程处理，当应用收到Vsync时，处理一批Move事件（Move事件之间的间隔通常小于16ms）
* Up事件 直接处理


有几个触发要区分清楚

* Input输入
* VSYNC输入
* INVALID消息输入
* Chorgrapher自己的几个MessageQueue

流程：

* 1 invalide需要重绘或者Input输入存在
* 2 去异步（oneway）请求VSYNC同步信号
* 3 VSYNC信号到来，重绘

也就是说垂直同步信号 是需要Client主动去请求的，否则VSYNC不会被通知到Client

垂直同步跟UI更新，跟消息处理、动画更新是两个完全不同的东西，前者属于引擎，后者属于业务

# 对此requestLayout跟invalid都不会重复调用布局测绘

    <ProgressBar
        android:id="@+id/progress_bar"
        android:layout_width="30dp"
        android:layout_height="30dp"
        android:layout_centerInParent="true"/>
        
输入DisplayEventReceiver       WindowInputEventReceiver    ConsumeBatchedInputRunnable 

![image.png](https://upload-images.jianshu.io/upload_images/1460468-e6173e52c5e28102.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![image.png](https://upload-images.jianshu.io/upload_images/1460468-59db43c5821639d6.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

https://digitalassetlinks.googleapis.com/v1/statements:list?
   source.web.site=https://you.163.com
   relation=delegate_permission/common.handle_all_urls


	
	status_t NativeDisplayEventReceiver::scheduleVsync() {
	    if (!mWaitingForVsync) {
	        ALOGV("receiver %p ~ Scheduling vsync.", this);
	
	        // Drain all pending events.
	        nsecs_t vsyncTimestamp;
	        int32_t vsyncDisplayId;
	        uint32_t vsyncCount;
	        processPendingEvents(&vsyncTimestamp, &vsyncDisplayId, &vsyncCount);
	
	        status_t status = mReceiver.requestNextVsync();
	        if (status) {
	            ALOGW("Failed to request next vsync, status=%d", status);
	            return status;
	        }
	
	        mWaitingForVsync = true;
	    }
	    return OK;
	}
	
不会同时请求两个vsync信号


#  参考文档

[Android应用处理MotionEvent的过程](https://www.jianshu.com/p/c2e26c6d4ac1)  