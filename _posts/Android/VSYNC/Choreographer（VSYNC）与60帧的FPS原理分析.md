一般来说动画至少要24FPS，才能保证画面的流畅性，太低，肉眼就能明显感觉到卡顿。在手机上，这个值被调整到60FPS，增加丝滑度，这也是为什么有个（1000/60）16ms的指标，一般而言目前的Android系统最高FPS也就是60，这是因为Android采用了一个VSYNC来保证没16ms最多绘制一帧，简而言之：UI必须至少等待16ms的间隔才会绘制下一帧。先看一下UI数据改变与重绘流程。

### UI刷新流程示意

以Textview ，当我们通过setText改变TextView内容后，UI界面不会立刻改变，APP端会先向VSYNC服务请求，等到下一次VSYNC信号触发后，APP端的UI才真的开始刷新，基本流程如下

![image.png](https://upload-images.jianshu.io/upload_images/1460468-311b22120397333b.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

从我们的代码端来看如下：setText最终调用invalidate申请重绘，最后会通过ViewParent递归到ViewRootImpl的invalidate，请求VSYNC，在请求VSYNC的时候，会添加一个同步栅栏，防止UI线程中同步消息执行，这样做为了加快VSYNC的响应速度，如果不设置，VSYNC到来的时候，正在执行一个同步消息，那么UI更新的Task就会被延迟执行，这是Android的Looper跟MessageQueue决定的。

> APP端触发重绘，申请VSYNC流程示意

![image.png](https://upload-images.jianshu.io/upload_images/1460468-f76ea4cbb9a990ba.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

等到VSYNC到来后，会移除同步栅栏，并率先开始执行当前帧的处理，调用逻辑如下

> VSYNC回来流程示意

![image.png](https://upload-images.jianshu.io/upload_images/1460468-050895f38f6527e3.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

> doFrame执行UI绘制的示意图

![image.png](https://upload-images.jianshu.io/upload_images/1460468-4aab950bb9d74094.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

### UI刷新源码跟踪

同TextView类似，View内容改变一般都会调用invalidate触发视图重绘，这中间经历了什么呢？View会递归的调用父容器的invalidateChild，逐级回溯，最终走到ViewRootImpl的invalidate，如下：

> View.java
	
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
            
> ViewRootImpl.java

    void invalidate() {
        mDirty.set(0, 0, mWidth, mHeight);
        if (!mWillDrawSoon) {
            scheduleTraversals();
        }
    }


 ViewRootImpl会调用scheduleTraversals准备重绘，但是，重绘一般不会立即执行，而是往Choreographer的Choreographer.CALLBACK_TRAVERSAL队列中添加了一个mTraversalRunnable，同时申请VSYNC，这个mTraversalRunnable要一直等到申请的VSYNC到来后才会被执行，如下：
 
 > ViewRootImpl.java
  
     // 将UI绘制的mTraversalRunnable加入到下次垂直同步信号到来的等待callback中去
     // mTraversalScheduled用来保证本次Traversals未执行前，不会要求遍历两边，浪费16ms内，不需要绘制两次
    void scheduleTraversals() {
        if (!mTraversalScheduled) {
            mTraversalScheduled = true;
            // 防止同步栅栏，同步栅栏的意思就是拦截同步消息
            mTraversalBarrier = mHandler.getLooper().getQueue().postSyncBarrier();
            // postCallback的时候，顺便请求vnsc垂直同步信号scheduleVsyncLocked
            mChoreographer.postCallback(
                    Choreographer.CALLBACK_TRAVERSAL, mTraversalRunnable, null);
             <!--添加一个处理触摸事件的回调，防止中间有Touch事件过来-->
            if (!mUnbufferedInputDispatch) {
                scheduleConsumeBatchedInput();
            }
            notifyRendererOfFramePending();
            pokeDrawLockIfNeeded();
        }
    }

> Choreographer.java

    private void postCallbackDelayedInternal(int callbackType,
            Object action, Object token, long delayMillis) {
            
        synchronized (mLock) {
            final long now = SystemClock.uptimeMillis();
            final long dueTime = now + delayMillis;
            mCallbackQueues[callbackType].addCallbackLocked(dueTime, action, token);

            if (dueTime <= now) {
            <!--申请VSYNC同步信号-->
                scheduleFrameLocked(now);
            } 
        }
    }

scheduleTraversals利用mTraversalScheduled保证，在当前的mTraversalRunnable未被执行前，scheduleTraversals不会再被有效调用，也就是Choreographer.CALLBACK_TRAVERSAL理论上应该只有一个mTraversalRunnable的Task。mChoreographer.postCallback将mTraversalRunnable插入到CallBack之后，会接着调用scheduleFrameLocked请求Vsync同步信号
 
    // mFrameScheduled保证16ms内，只会申请一次垂直同步信号
    // scheduleFrameLocked可以被调用多次，但是mFrameScheduled保证下一个vsync到来之前，不会有新的请求发出
    // 多余的scheduleFrameLocked调用被无效化
    private void scheduleFrameLocked(long now) {
        if (!mFrameScheduled) {
            mFrameScheduled = true;
            if (USE_VSYNC) {
            
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
    
scheduleFrameLocked跟上一个scheduleTraversals类似，也采用了利用mFrameScheduled来保证：在当前申请的VSYNC到来之前，不会再去请求新的VSYNC，因为16ms内申请两个VSYNC没意义。再VSYNC到来之后，Choreographer利用Handler将FrameDisplayEventReceiver封装成一个**异步**Message，发送到UI线程的MessageQueue，

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
之所以封装成**异步Message**，是因为前面添加了一个同步栅栏，同步消息不会被执行。UI线程被唤起，取出该消息，最终调用doFrame进行UI刷新重绘

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
    
doFrame也采用了一个boolean遍历mFrameScheduled保证每次VSYNC中，只执行一次，可以看到，为了保证16ms只执行一次重绘，加了好多次层保障。doFrame里除了UI重绘，其实还处理了很多其他的事，比如检测VSYNC被延迟多久执行，掉了多少帧，处理Touch事件（一般是MOVE），处理动画，以及UI，当doFrame在处理Choreographer.CALLBACK_TRAVERSAL的回调时（mTraversalRunnable），才是真正的开始处理View重绘：
  
      final class TraversalRunnable implements Runnable {
        @Override
        public void run() {
            doTraversal();
        }
    }
    
 回到ViewRootImpl调用doTraversal进行View树遍历，
 
    // 这里是真正执行了，
    void doTraversal() {
        if (mTraversalScheduled) {
            mTraversalScheduled = false;
            <!--移除同步栅栏，只有重绘才设置了栅栏，说明重绘的优先级还是挺高的，所有的同步消息必须让步-->
            mHandler.getLooper().getQueue().removeSyncBarrier(mTraversalBarrier);
            performTraversals();
        }
    }
  
doTraversal会先将栅栏移除，然后处理performTraversals，进行测量、布局、绘制，提交当前帧给SurfaceFlinger进行图层合成显示。以上多个boolean变量保证了每16ms最多执行一次UI重绘，这也是目前Android存在60FPS上限的原因。
   
**注： VSYNC同步信号需要用户主动去请求才会收到，并且是单次有效。**



### 软件绘制

设置了软件的话，就是软件绘制 Canvas是普通Canvas

    @NonNull
    public RenderNode updateDisplayListIfDirty() {
    			<!--封装成硬件加速的drawBitmap-->
            try {
                if (layerType == LAYER_TYPE_SOFTWARE) {
                    buildDrawingCache(true);
                    Bitmap cache = getDrawingCache(true);
                    if (cache != null) {
                        canvas.drawBitmap(cache, 0, 0, mLayerPaint);
                    }
                } 
                
 构建普通Canvas
 
    private void buildDrawingCacheImpl(boolean autoScale) {       
           。。。
      Canvas canvas;
        if (attachInfo != null) {
            canvas = attachInfo.mCanvas;
            if (canvas == null) {
                canvas = new Canvas();
            }
            canvas.setBitmap(bitmap);
        } else {
            canvas = new Canvas(bitmap);
        }

       ...
        } else {
            draw(canvas);
        }

        canvas.restoreToCount(restoreCount);
        canvas.setBitmap(null);

        if (attachInfo != null) {
            // Restore the cached Canvas for our siblings
            attachInfo.mCanvas = canvas;
        }
    }
    
    
### UI局部重绘

某一个View重绘刷新，并不会导致所有View都进行一次measure、layout、draw，可能只是这个待刷新View链路需要调整，那么剩余的View就不需要浪费精力再来一遍，反应再APP侧就是：**不需要再次调用updateDisplayListIfDirty构建RenderNode渲染Op树**

	    public RenderNode updateDisplayListIfDirty() {
	        final RenderNode renderNode = mRenderNode;
			  ...
	        if ((mPrivateFlags & PFLAG_DRAWING_CACHE_VALID) == 0
	                || !renderNode.isValid()
	                || (mRecreateDisplayList)) {
	           <!--失效了，需要重绘-->
	        } else {
	        <!--依旧有效，无需重绘-->
	            mPrivateFlags |= PFLAG_DRAWN | PFLAG_DRAWING_CACHE_VALID;
	            mPrivateFlags &= ~PFLAG_DIRTY_MASK;
	        }
	        return renderNode;
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
        
输入DisplayEventReceiver   WindowInputEventReceiver    ConsumeBatchedInputRunnable 

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