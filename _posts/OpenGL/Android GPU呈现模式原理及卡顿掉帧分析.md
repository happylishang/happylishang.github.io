前言：**MOVE一般要导致UI的更新，不然MOVE事件有毛用**

APP开发中，卡顿绝对优化的大头，Google为了帮助开发者更好的定位问题，提供了不少工具，如Systrace、GPU呈现模式分析工具、Android Studio自带的CPU Profiler等，主要是辅助定位哪段代码、哪块逻辑比较耗时，影响UI渲染，导致了卡顿。拿Profile GPU Rendering工具而言，它用一种很直观的方式呈现可能超时的节点，该工具及其原理也是本文的重点：

![gettingstarted_image003.png](https://upload-images.jianshu.io/upload_images/1460468-57ebb2dda8157015.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

CPU Profiler也会提供相似的图表，本文主要围绕着GPU呈现模式分析工具展开，简析各个阶段耗时统计的原理，同时总结下在使用及分析过程中也遇到的一些问题，可能算工具自身的BUG，这给分析带来了不少困惑。比如如下几点：

* GPU呈现模式分析工具跟Google官方文档上似乎对应不起来（各个颜色代表的阶段）
* CPU Profiler的函数调用似乎有些调用被合并了，并非独立的调用栈（影响分析哪块耗时）
* Skip Frame掉帧可能跟我们预想的不同，而且掉帧的统计也可能不准（主要是Vsync的延时部分，有些耗时操作导致卡顿了，但是可能没有统计出掉帧）

# GPU呈现模式分析工具简介

Profile GPU Rendering工具的使用很简单，就是直观上看一帧的耗时有多长，绿线是16ms的阈值，超过了，可能会导致掉帧，这个跟VSYNC垂直同步信号有关系，当然，这个图表并不是绝对严谨的（后文会说原因）。每个颜色的方块代表不同的处理阶段，先看下官方文档给的映射表：


![image.png](https://upload-images.jianshu.io/upload_images/1460468-6461878f98d427e0.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

想要完全理解各个阶段，要对硬件加速及GPU渲染有一定的了解，不过，有一点，必须先记心里：**虽名为 Profile GPU Rendering，但图标中所有阶段都发生在CPU中，不是GPU** 。最终CPU将命令提交到 GPU 后触发GPU异步渲染屏幕，之后CPU会处理下一帧，而GPU并行处理渲染，两者硬件上算是并行。 不过，有些时候，GPU可能过于繁忙，不能跟上CPU的步伐，这个时候，CPU必须等待，也就是最终的swapbuffer部分，主要是最后的红色及黄色部分（**同步上传的部分不会有问题，个人认为是因为在Android GPU与CPU是共享内存区域的**），在等待时，将看到橙色条和红色条中出现峰值，且命令提交将被阻止，直到 GPU 命令队列腾出更多空间。

在使用Profile GPU Rendering工具时，我面临第一个问题是：**官方文档的使用指导好像不太对**。
 
# Profile GPU Rendering工具颜色问题

真正使用该工具的时候，条形图的颜色跟文档好像对不上，为了测试，这里先用一个小段代码模拟场景，鉴别出各个阶段，最后再分析源码。从下往上，先忽略VSYNC部分，先看输入事件，在一个自定义布局中，为触摸事件添加延时，并触发重绘。

	    @Override
	    public boolean dispatchTouchEvent(MotionEvent ev) {
	        try {
	            Thread.sleep(20);
	        } catch (InterruptedException e) {
	            e.printStackTrace();
	        }
	        mTextView.setText("" + System.currentTimeMillis());
	        requestLayout();
	        super.dispatchTouchEvent(ev);
	        return true;
	    }


这个时候看到的超时部分主要是输入事件引起的，进而确定下输入事件的颜色：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-29dcaf12b3a2759b.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 

输入事件加个20ms延后，上图红色方块部分正好映射到输入事件耗时，这里就能看到，输入事件的颜色跟官方文档的颜色对不上，如下图


![image.png](https://upload-images.jianshu.io/upload_images/1460468-0da84239c597b22d.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


同样，测量布局的耗时也跟文档对不上。为布局测量加个耗时，即可验证：

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        try {
            Thread.sleep(20);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

![image.png](https://upload-images.jianshu.io/upload_images/1460468-d6db3c11c2bc3204.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到，上图中测量布局的耗时跟官方文档的颜色也对不上。除此之外，**似乎**多出了第三部分耗时，这部分其实是VSYNC同步耗时，这部分耗时怎么来的，真的存在耗时吗？官方解释似乎是连个连续帧之间的耗时，但是后面分析会发现，可能这个解释同源码对应不起来。

> Miscellaneous

> In addition to the time it takes the rendering system to perform its work, there’s an additional set of work that occurs on the main thread and has nothing to do with rendering. Time that this work consumes is reported as misc time. Misc time generally represents work that might be occurring on the UI thread between two consecutive frames of rendering.

其次，为什么几乎每个条形图都有一个**测量布局耗时**跟**输入事件耗时**呢？为什么是一一对应，而不是有多个？测量布局是在Touch事件之后立即执行呢，还是等待下一个VSYNC信号到来再执行呢？这部主要牵扯到的内容：VSYNC垂直同步信号、ViewRootImpl、Choreographer、Touch事件处理机制，后面会逐步说明，先来看一下以上三个事件的耗时是怎么统计的。


# Miscellaneous VSYNC延时

Profile GPU Rendering工具统计的入口在Choreographer类中，时机是VSYNC信号Message被执行，注意这里是**信号消息被执行，而不是信号到来**，因为信号到来并不意味着立即被执行，因为VSYNC信号的申请是异步的，信号申请后线程继续执行当前消息，SurfaceFlinger在下一次分发VSYNC的时候直接往APP UI线程的MessageQueue插入一条VSYNC到来的消息，而消息被插入后，并不会立即被执行，而是要等待之前的消息执行完毕后才会执行，而**VSYNC延时其实就是VSYNC消息到来到被执行之间的延时**。

	 void doFrame(long frameTimeNanos, int frame) {
	        final long startNanos;
	        synchronized (mLock) {
	            if (!mFrameScheduled) {
	         ...
	            long intendedFrameTimeNanos = frameTimeNanos;
	      
	          <!--关键点1  设置vsync开始，并记录起始时间 -->
	            mFrameInfo.setVsync(intendedFrameTimeNanos, frameTimeNanos);
	            mFrameScheduled = false;
	            mLastFrameTimeNanos = frameTimeNanos;
	           }
		        try {
	       	 // 开始处理输入事件，并记录起始时间
	            mFrameInfo.markInputHandlingStart();
	            doCallbacks(Choreographer.CALLBACK_INPUT, frameTimeNanos);
	    		 // 开始处理动画，并记录起始时间 
	            mFrameInfo.markAnimationsStart();
	            doCallbacks(Choreographer.CALLBACK_ANIMATION, frameTimeNanos);
	     		 // 开始处理测量布局，并记录起始时间
	            mFrameInfo.markPerformTraversalsStart();
	            doCallbacks(Choreographer.CALLBACK_TRAVERSAL, frameTimeNanos);
	        } finally {
	        }

这里的	VSYNC延时其实是 mFrameInfo.markInputHandlingStart - frameTimeNanos，而frameTimeNanos是VSYNC信号到达的时间戳，如下

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
            ...
            <!--存下时间戳，并往UI的MessageQueue发送一个消息-->
            mTimestampNanos = timestampNanos;
            mFrame = frame;
            Message msg = Message.obtain(mHandler, this);
            msg.setAsynchronous(true);
            mHandler.sendMessageAtTime(msg, timestampNanos / TimeUtils.NANOS_PER_MS);
        }

        @Override
        public void run() {
           <!--将之前的时间戳作为参数传递给doFrame-->
            mHavePendingVsync = false;
            doFrame(mTimestampNanos, mFrame);
        }
    }
    
 onVsync是VSYNC信号到达的时候在Native层回调Java层的方法，其实是MessegeQueue的native消息队列那一套，并且VSYNC要一个执行完，下一个才会生效，否则下一个VSYNC只能在队列中等待，所以之前说的？？？第三部分延时就是VSYNC延时，但是这部分不应该被算到渲染中去，另外根据写法，VSYNC延时可能也有很大出入。看doFrame中有一部分是统计掉帧的，个人理解也许这部分统计并不是特别靠谱，下面看下掉帧的部分。

# Skiped Frame同Vsync的耗时 

有些APM检测工具通过将Choreographer的SKIPPED_FRAME_WARNING_LIMIT设置为1，来达到掉帧检测的目的，即如下设置：

        try {
            Field field = Choreographer.class.getDeclaredField("SKIPPED_FRAME_WARNING_LIMIT");
            field.setAccessible(true);
            field.set(Choreographer.class, 0);
        } catch (Throwable e) {
            e.printStackTrace();
        }
        
如果出现卡顿，在log日志中就能看到如下信息

![image.png](https://upload-images.jianshu.io/upload_images/1460468-bc381411e8299cc6.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

其实这里并不是太严谨，看源码中原理如下：

    void doFrame(long frameTimeNanos, int frame) {
        final long startNanos;
        synchronized (mLock) {
            if (!mFrameScheduled) {
                return; // no work to do
            }
            
            long intendedFrameTimeNanos = frameTimeNanos;
            <!--skip frame关键点-->
            startNanos = System.nanoTime();
            final long jitterNanos = startNanos - frameTimeNanos;
            if (jitterNanos >= mFrameIntervalNanos) {
                final long skippedFrames = jitterNanos / mFrameIntervalNanos;
                if (skippedFrames >= SKIPPED_FRAME_WARNING_LIMIT) {
                    Log.i(TAG, "Skipped " + skippedFrames + " frames!  "
                            + "The application may be doing too much work on its main thread.");
                }
          ...
        }

可以看到跳帧检测的关键点就是Vsync信号被延时，但是Vsync信号被延时真的能反应跳帧吗？Vsync信号到了后，并不一定会被立刻执行，因为UI线程可能被阻塞再某个地方，比如在Touch事件中，触发了重绘，但是异步申请VSYNC后继续执行了一个耗时操作，那么这个时候，必然会导致Vsync信号被延时执行，那么跳帧日志就会被打印，如下

	    @Override
	    public boolean dispatchTouchEvent(MotionEvent ev) {
	        super.dispatchTouchEvent(ev);
	        scrollTo(0,new Random().nextInt(15));
	        try {
	            Thread.sleep(40);
	        } catch (InterruptedException e) {
	            e.printStackTrace();
	        }
	        return true;
	    }
	    
![image.png](https://upload-images.jianshu.io/upload_images/1460468-5e249d3b7e80a829.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到，颜色2的部分就是Vsync信信号延时，这个时候会有掉帧日志。但是如果将触发UI重绘的消息放到延时操作后面呢？毫无疑问，卡顿依然有，但是这时会发生一个有趣的现象，跳帧没了，系统认为没有帧丢失，代码如下：

	    @Override
	    public boolean dispatchTouchEvent(MotionEvent ev) {
	        super.dispatchTouchEvent(ev);
	        try {
	            Thread.sleep(40);
	        } catch (InterruptedException e) {
	            e.printStackTrace();
	        }
	        scrollTo(0,new Random().nextInt(15));
	        return true;
	    }

 ![image.png](https://upload-images.jianshu.io/upload_images/1460468-2418fd574dbba5e5.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 
 可以看到，图中几乎没有Vsync信信号延时，这时为什么？因为下一个VSYNC信号的申请是由scrollTo触发，触发后并没有什么延时操作，知道VSYNC信号到来后，立即执行doFrame，这个之间的延时很少，系统就认为没有掉帧，但是其实卡顿依旧。因为整体来看，一段时间内的帧率是相同的。
   
   
![image.png](https://upload-images.jianshu.io/upload_images/1460468-00d823d551cde305.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)  
     
以上就是scrollTo在延时前后的区别，无论哪种，都其实都是掉帧了，而且掉帧都是一样的，但是日志统计的跳帧却出现了问题，每一帧真正的耗时可能并不是我们看到的样子，个人觉得这可能算是工具的一个BUG，不能很精确的反应卡顿问题，依靠真个做FPS侦测，应该也都有问题，**比如滚动时候，处理耗时操作，之后更新UI，这种情况下，通过这种方式是检测不出跳帧的。**，当然不排除有其他更好的方案，下面看一下Input时间耗时。


# 输入事件耗时分析

输入事件其实就是：InputManagerService捕获用户输入，通过Socket将事件传递给APP端（往UI线程的消息队列里插入消息），不过对于不同的触摸事件有不同的处理机制，对于Down、UP事件，APP端需要直接处理，对于Move事件，要结合重绘事件处理，其实就是要等到下一次VSYNC到来，分批处理。可以认为**只有MOVE事件才被GPU柱状图统计到里面，UP、DOWN事件被立即执行，不会等待VSYNC跟UI重绘一起执行。**

	 void doFrame(long frameTimeNanos, int frame) {
	        final long startNanos;
	        synchronized (mLock) {
	            if (!mFrameScheduled) { 
	            ...
	          // 设置vsync开始，并记录起始时间
	          <!--关键点1-->
	            mFrameInfo.setVsync(intendedFrameTimeNanos, frameTimeNanos);
	            mFrameScheduled = false;
	            mLastFrameTimeNanos = frameTimeNanos;
	           }
		        try {
	       	 // 开始处理输入事件，并记录起始时间
	            mFrameInfo.markInputHandlingStart();
	            doCallbacks(Choreographer.CALLBACK_INPUT, frameTimeNanos);
	    		 // 开始处理动画，并记录起始时间 
	            mFrameInfo.markAnimationsStart();
	            doCallbacks(Choreographer.CALLBACK_ANIMATION, frameTimeNanos);
	     		 // 开始处理测量布局，并记录起始时间
	            mFrameInfo.markPerformTraversalsStart();
	            doCallbacks(Choreographer.CALLBACK_TRAVERSAL, frameTimeNanos);
	            doCallbacks(Choreographer.CALLBACK_COMMIT, frameTimeNanos);
	        } finally {
	        }

 
 	 
	 
	/**
     * Request unbuffered dispatch of the given stream of MotionEvents to this View.
     *
     * Until this View receives a corresponding {@link MotionEvent#ACTION_UP}, ask that the input
     * system not batch {@link MotionEvent}s but instead deliver them as soon as they're
     * available. This method should only be called for touch events.
     *
     * <p class="note">This api is not intended for most applications. Buffered dispatch
     * provides many of benefits, and just requesting unbuffered dispatch on most MotionEvent
     * streams will not improve your input latency. Side effects include: increased latency,
     * jittery scrolls and inability to take advantage of system resampling. Talk to your input
     * professional to see if {@link #requestUnbufferedDispatch(MotionEvent)} is right for
     * you.</p>
     */

	   public final void requestUnbufferedDispatch(MotionEvent event) {
	        final int action = event.getAction();
	        if (mAttachInfo == null
	                || action != MotionEvent.ACTION_DOWN && action != MotionEvent.ACTION_MOVE
	                || !event.isTouchEvent()) {
	            return;
	        }
	        mAttachInfo.mUnbufferedDispatchRequested = true;
	    }
	    
 mUnbufferedDispatchRequested应该是为了不批量处理用的，正常情况下是需要批量处理的，这了一般只限定MOVE事件
 
 
 
 



# 消费分批次，无论是输入事件还是测量绘制，都要分批次

其实主要是MOVE事件  消费分批次，无论是输入事件还是测量绘制，都要分批次，上一批没完成，下一批就算到了也没用，算丢弃吧


 

用一个自定义布局作为测试，先看看输入事件

为何Down UP事件不会GPU Profiler中input延时有影响，如上就是原因，因为这两个事件不会算到里面

	    @Override
	    public boolean dispatchTouchEvent(MotionEvent ev) {
	        try {
	            Thread.sleep(20);
	        } catch (InterruptedException e) {
	            e.printStackTrace();
	        }
	        mTextView.setText("" + System.currentTimeMillis());
	        requestLayout();
	        super.dispatchTouchEvent(ev);
	        return true;
	    }
	    







































输入事件之就是测量布局，因为我们这里没有动效耗时，先忽略动效。不过这里有个不太好理解的底下深色部分那部分是什么呢？为什么还会动态变化呢？按照官方文档解释是：**其他时间/VSync 延迟**，官方解释如下：


> Miscellaneous

> In addition to the time it takes the rendering system to perform its work, there’s an additional set of work that occurs on the main thread and has nothing to do with rendering. Time that this work consumes is reported as misc time. Misc time generally represents work that might be occurring on the UI thread between two consecutive frames of rendering.
 


字面上看，是两个连续帧之间等待的耗时，这么说有点不好理解，个人理解是：**Vsync信号到来到下一次doFrame的开始时间，这个时间其实并不能很好的反应出绘制，也仅仅是是连续的时候有些参考价值。**，换句话说invalide放在耗时操作前后带来影响有很大差别，虽然全局上看没啥问题，但是GPU Profiler呈现的表有很大区别，个人认为这里应该算是他的bug吧，不过这个时间官方也说了 has nothing to do with rendering，

	 

> adb shell dumpsys gfxinfo  com.snail.labaffinity  framestats

	---PROFILEDATA---
	Flags,IntendedVsync,Vsync,OldestInputEvent,NewestInputEvent,HandleInputStart,AnimationStart,PerformTraversalsStart,DrawStart,SyncQueued,SyncStart,IssueDrawCommandsStart,SwapBuffers,FrameCompleted,
	0,1001692707421551,1001692707421551,1001692644983000,1001692703788000,1001692707792467,1001692760678614,1001692760684447,1001692761844395,1001692762753405,1001692762858613,1001692763011582,1001692767238561,1001692768593613,
	0,1001692774585923,1001692774585923,1001692712150000,1001692770943000,1001692774929811,1001692827529603,1001692827535176,1001692828162936,1001692828998092,1001692829048405,1001692829241321,1001692833842155,1001692834748822,
	0,1001692841750295,1001692841750295,1001692779289000,1001692838071000,1001692842103405,1001692894429863,1001692894436165,1001692895064759,1001692895912051,1001692895959290,1001692896120697,1001692899783509,1001692900774134,
	0,1001692908914667,1001692908914667,1001692846455000,1001692905242000,1001692909246113,1001692961335176,1001692961341061,1001692961990645,1001692962887832,1001692962933301,1001692963088770,1001692967199134,1001692967809134,

	0,1006270418867 ,1006270452200 ,9223372036854775,0,1006270467341,1006270467363,1006270467366554,1006270491639992,1006270497920460,1006270498202023,1006270498375564,1006270504900460,1006270505562075,	

FrameInfo 里面也定义了某些状态


    // The intended vsync time, unadjusted by jitter
    private static final int INTENDED_VSYNC = 1;

    // Jitter-adjusted vsync time, this is what was used as input into the
    // animation & drawing system
    private static final int VSYNC = 2;

    // The time of the oldest input event
    private static final int OLDEST_INPUT_EVENT = 3;

    // The time of the newest input event
    private static final int NEWEST_INPUT_EVENT = 4;

    // When input event handling started
    private static final int HANDLE_INPUT_START = 5;

    // When animation evaluations started
    private static final int ANIMATION_START = 6;

    // When ViewRootImpl#performTraversals() started
    private static final int PERFORM_TRAVERSALS_START = 7;

    // When View:draw() started
    private static final int DRAW_START = 8;



![image.png](https://upload-images.jianshu.io/upload_images/1460468-0c1e0ae042b03876.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

这个栈是怎么回事？为什么Input会通测量布局一一对应？不是一次批量处理完，再处理下一个？

![image.png](https://upload-images.jianshu.io/upload_images/1460468-efb39dfd42ad34f5.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

为什么延时加到输入事件上，就是另一番表现？ **这里比较怀疑是他娘的Android Profiler CPU统计的bug，虽然是一套逻辑，但是不是同一组函数栈，合并了。证据就是doFrame的调用次数跟CPU Profiler 的压根对应不起来，所以之类是有问题的**。 

![image.png](https://upload-images.jianshu.io/upload_images/1460468-8eed61b5aa599e76.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![image.png](https://upload-images.jianshu.io/upload_images/1460468-9dca8476e0eec275.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


也就是说，垂直同步信号机制下，同一个16ms内，或者说在下一个垂直同步信号到来之前，最多只能处理一个MOVE的pathc、最多只有一个绘制请求、一次动画更新，

    void scheduleTraversals() {
        // 重复多次调用invalid requestLayout只会标记一次，等到下一次Vsync信号到，只会执行执行一次
        if (!mTraversalScheduled) {
            mTraversalScheduled = true;
            mTraversalBarrier = mHandler.getLooper().getQueue().postSyncBarrier();
            mChoreographer.postCallback(
                    Choreographer.CALLBACK_TRAVERSAL, mTraversalRunnable, null);
            <!--关键点1预判下，更新UI的时候，通常伴随MOVE事件-->
            // mUnbufferedInputDispatch =false 一般都是false 所以会执行scheduleConsumeBatchedInput
            if (!mUnbufferedInputDispatch) {
                scheduleConsumeBatchedInput();
            }
            notifyRendererOfFramePending();
            pokeDrawLockIfNeeded();
        }
    }

**scheduleTraversals本身也会调用scheduleConsumeBatchedInput来预备应对下一批MOVE事件，可能是为了提前预判吧**，不过这个时候仍然是只有一个Vsync

    private void scheduleFrameLocked(long now) {
        if (!mFrameScheduled) {
        <!--保证单一-->
            mFrameScheduled = true;
            if (USE_VSYNC) 
                // as soon as possible.
                if (isRunningOnLooperThreadLocked()) {
                    scheduleVsyncLocked();
                    ...
          }


# 请求Vsync是个异步过程 很明显不会阻塞等待垂直同步信号到来

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

    /*
     * requestNextVsync() schedules the next vsync event. It has no effect if the vsync rate is > 0.
     */
    virtual void requestNextVsync() = 0; // Asynchronous


在运行行期间，是可以动态增加CallBack的，比如相应MOVE事件的时候，触发重绘，这个重绘会在当前MOVE时间处理完毕后立即执行，而不会等待到下一次Vsync信号的到来。

![image.png](https://upload-images.jianshu.io/upload_images/1460468-c4f3f9d6eb99e0fc.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![image.png](https://upload-images.jianshu.io/upload_images/1460468-19bc89f8d6cc97b9.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


# 一般MOVE事件伴随Scroll，比如List，更新机制里面还少不了动效

    public void scrollTo(int x, int y) {
        if (mScrollX != x || mScrollY != y) {
            int oldX = mScrollX;
            int oldY = mScrollY;
            mScrollX = x;
            mScrollY = y;
            invalidateParentCaches();
            onScrollChanged(mScrollX, mScrollY, oldX, oldY);
            if (!awakenScrollBars()) {
                postInvalidateOnAnimation();
            }
        }
    }
        public void postInvalidateOnAnimation() {
        // We try only with the AttachInfo because there's no point in invalidating
        // if we are not attached to our window
        final AttachInfo attachInfo = mAttachInfo;
        if (attachInfo != null) {
            attachInfo.mViewRootImpl.dispatchInvalidateOnAnimation(this);
        }
    }
    
    final class InvalidateOnAnimationRunnable implements Runnable {
    private boolean mPosted;
    private final ArrayList<View> mViews = new ArrayList<View>();
    private final ArrayList<AttachInfo.InvalidateInfo> mViewRects =
            new ArrayList<AttachInfo.InvalidateInfo>();
    private View[] mTempViews;
    private AttachInfo.InvalidateInfo[] mTempViewRects;

    public void addView(View view) {
        synchronized (this) {
            mViews.add(view);
            postIfNeededLocked();
        }
    }
    
    private void postIfNeededLocked() {
        if (!mPosted) {
            mChoreographer.postCallback(Choreographer.CALLBACK_ANIMATION, this, null);
            mPosted = true;
        }
    }


## Bitmap  prepareToDraw


prepareToDraw

added in API level 4

public void prepareToDraw ()
Builds caches associated with the bitmap that are used for drawing it.

Starting in Build.VERSION_CODES.N, this call initiates an asynchronous upload to the GPU on RenderThread, if the Bitmap is not already uploaded. With Hardware Acceleration, Bitmaps must be uploaded to the GPU in order to be rendered. This is done by default the first time a Bitmap is drawn, but the process can take several milliseconds, depending on the size of the Bitmap. Each time a Bitmap is modified and drawn again, it must be re-uploaded.

Calling this method in advance can save time in the first frame it's used. For example, it is recommended to call this on an image decoding worker thread when a decoded Bitmap is about to be displayed. It is recommended to make any pre-draw modifications to the Bitmap before calling this method, so the cached, uploaded copy may be reused without re-uploading.

In Build.VERSION_CODES.KITKAT and below, for purgeable bitmaps, this call would attempt to ensure that the pixels have been decoded.


## issue

![image.png](https://upload-images.jianshu.io/upload_images/1460468-7ee8aec8ad5b6ab6.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)



# 关于触摸事件

![image.png](https://upload-images.jianshu.io/upload_images/1460468-429764b60708b10d.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

颜色好像对不上，绿色是测量布局：


直观上说，Vsync垂直同步信号是UI重绘的触发器

系统获取FPS的原理：手机屏幕显示的内容是通过Android系统的SurfaceFlinger类，把当前系统里所有进程需要显示的信息合成一帧，然后提交到屏幕上显示，FPS就是1秒内SurfaceFlinger提交到屏幕的帧数，


App停止操作后，FPS还是在一直变化，这种情况是否会影响到FPS的准确度？
有的时候FPS很低，APP看起来却很流畅，是因为当前界面在1秒内只需要10帧的显示需求，当然不会卡顿，此时FPS只要高于10就可以了，如果屏幕根本没有绘制需求，那FPS的值就是0。


 


![image.png](https://upload-images.jianshu.io/upload_images/1460468-6461878f98d427e0.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# FPS只针对特定场景才有意义

对于掉帧我们需要关注场景：动画、滚动、数据流（视频播放或者录制预览等）、只有这些场景才需要关注帧率，掉帧，

* 掉帧的概念
* 掉帧的原因
* 掉帧的处理方式


**注意，必须重绘，才会看到GPU渲染更新，还要注意这些流程图跟踪的全部是在CPU中，并不能真正100%反应帧率，毕竟不是同步绘制**

首先有个概念要清楚，GPU中OpenGL驱动分为软件实现跟硬件试下，软件实现的一般都是同步的，不存在GPU处理一说，Android源码自带的是软件实现的Agl，Android通过软件方法实现的一套OpenGL动态库

在使用 systrace 的过程中，请记住，每个事件都是由 CPU 上的活动触发的。


	static const std::array<BarSegment,7> Bar {{
	    { FrameInfoIndex::IntendedVsync, FrameInfoIndex::HandleInputStart, 0x00796B },
	    { FrameInfoIndex::HandleInputStart, FrameInfoIndex::PerformTraversalsStart, 0x388E3C },
	    { FrameInfoIndex::PerformTraversalsStart, FrameInfoIndex::DrawStart, 0x689F38},
	    { FrameInfoIndex::DrawStart, FrameInfoIndex::SyncStart, 0x2196F3},
	    { FrameInfoIndex::SyncStart, FrameInfoIndex::IssueDrawCommandsStart, 0x4FC3F7},
	    { FrameInfoIndex::IssueDrawCommandsStart, FrameInfoIndex::SwapBuffers, 0xF44336},
	    { FrameInfoIndex::SwapBuffers, FrameInfoIndex::FrameCompleted, 0xFF9800},
	}};

可以以看看每个阶段的数值

橙色部分表示的是处理时间,或者说是CPU告诉GPU渲染一帧的地方,这是一个阻塞调用,因为CPU会一直等待GPU发出接到命令的回复,如果柱状图很高,那就意味着你给GPU太多的工作,太多的负责视图需要OpenGL命令去绘制和处理.


# OpenGL Profiler源码

	void CanvasContext::draw() {
	    ...
	    profiler().draw(mCanvas);
	    
    
    FrameInfoVisualizer& profiler() { return mProfiler; }

条形图

	void FrameInfoVisualizer::draw(OpenGLRenderer* canvas) {
	    RETURN_IF_DISABLED();
	
	    if (mShowDirtyRegions) {
	        mFlashToggle = !mFlashToggle;
	        if (mFlashToggle) {
	            SkPaint paint;
	            paint.setColor(0x7fff0000);
	            canvas->drawRect(mDirtyRegion.fLeft, mDirtyRegion.fTop,
	                    mDirtyRegion.fRight, mDirtyRegion.fBottom, &paint);
	        }
	    }
	
	    // 绘制一条条
	    if (mType == ProfileType::Bars) {
	        // Patch up the current frame to pretend we ended here. CanvasContext
	        // will overwrite these values with the real ones after we return.
	        // This is a bit nicer looking than the vague green bar, as we have
	        // valid data for almost all the stages and a very good idea of what
	        // the issue stage will look like, too
	        FrameInfo& info = mFrameSource.back();
	        info.markSwapBuffers();
	        info.markFrameCompleted();
	
	        initializeRects(canvas->getViewportHeight(), canvas->getViewportWidth());
	        drawGraph(canvas);
	        drawThreshold(canvas);
	    }
	}

	<!--同步开始-->
	
	void CanvasContext::prepareTree(TreeInfo& info, int64_t* uiFrameInfo, int64_t syncQueued) {
	    mRenderThread.removeFrameCallback(this);
	
	    // If the previous frame was dropped we don't need to hold onto it, so
	    // just keep using the previous frame's structure instead
	    if (!wasSkipped(mCurrentFrameInfo)) {
	        mCurrentFrameInfo = &mFrames.next();
	    }
	    mCurrentFrameInfo->importUiThreadInfo(uiFrameInfo);
	    mCurrentFrameInfo->set(FrameInfoIndex::SyncQueued) = syncQueued;
	    // 这里表示同步上传？
	    mCurrentFrameInfo->markSyncStart();
	
	    info.damageAccumulator = &mDamageAccumulator;
	    info.renderer = mCanvas;
	    info.canvasContext = this;
	
	    mAnimationContext->startFrame(info.mode);
	    // node
	    mRootRenderNode->prepareTree(info);
	    mAnimationContext->runRemainingAnimations(info);
	
	// 什么意思
	    freePrefetechedLayers();
	
	    if (CC_UNLIKELY(!mNativeWindow.get())) {
	        mCurrentFrameInfo->addFlag(FrameInfoFlags::SkippedFrame);
	        info.out.canDrawThisFrame = false;
	        return;
	    }
	
	    int runningBehind = 0;
	    // TODO: This query is moderately expensive, investigate adding some sort
	    // of fast-path based off when we last called eglSwapBuffers() as well as
	    // last vsync time. Or something.
	    mNativeWindow->query(mNativeWindow.get(),
	            NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND, &runningBehind);
	    info.out.canDrawThisFrame = !runningBehind;
	
	    if (!info.out.canDrawThisFrame) {
	        mCurrentFrameInfo->addFlag(FrameInfoFlags::SkippedFrame);
	    }
	
	    if (info.out.hasAnimations || !info.out.canDrawThisFrame) {
	        if (!info.out.requiresUiRedraw) {
	            // If animationsNeedsRedraw is set don't bother posting for an RT anim
	            // as we will just end up fighting the UI thread.
	            mRenderThread.postFrameCallback(this);
	        }
	    }
	}



	// 创建Layer 以及其帧缓冲 ？？
	void RenderNode::prepareTreeImpl(TreeInfo& info, bool functorsNeedLayer) {
	    info.damageAccumulator->pushTransform(this);
	
	    if (info.mode == TreeInfo::MODE_FULL) {
	        // 同步到这里
	        pushStagingPropertiesChanges(info);
	    }
	    uint32_t animatorDirtyMask = 0;
	    if (CC_LIKELY(info.runAnimations)) {
	        animatorDirtyMask = mAnimatorManager.animate(info);
	    }
	
	    bool willHaveFunctor = false;
	    if (info.mode == TreeInfo::MODE_FULL && mStagingDisplayListData) {
	        willHaveFunctor = !mStagingDisplayListData->functors.isEmpty();
	    } else if (mDisplayListData) {
	        willHaveFunctor = !mDisplayListData->functors.isEmpty();
	    }
	    bool childFunctorsNeedLayer = mProperties.prepareForFunctorPresence(
	            willHaveFunctor, functorsNeedLayer);
	// layer
	    prepareLayer(info, animatorDirtyMask);
	    if (info.mode == TreeInfo::MODE_FULL) {
	        pushStagingDisplayListChanges(info);
	    }
	    prepareSubTree(info, childFunctorsNeedLayer, mDisplayListData);
	    // push
	    pushLayerUpdate(info);
	
	    info.damageAccumulator->popTransform();
	}


	
	void CanvasContext::draw() {
	    ...
	    <!--Issue的开始-->
	    mCurrentFrameInfo->markIssueDrawCommandsStart();
		...
	    <!--构建-->
	    profiler().draw(mCanvas);
	    <!--像GPU发送命令-->
		 mCanvas->drawRenderNode(mRootRenderNode.get(), outBounds);
	    // Even if we decided to cancel the frame, from the perspective of jank
	    // metrics the frame was swapped at this point
	    <!--命令发送完毕-->
	    mCurrentFrameInfo->markSwapBuffers();
	    ...
	    // TODO: Use a fence for real completion?
	    <!--这里只有用fence才能获取真正的耗时，不然还是无效的，看每个手机厂家的实现了-->
	    mCurrentFrameInfo->markFrameCompleted();
	    mJankTracker.addFrame(*mCurrentFrameInfo);
	    mRenderThread.jankTracker().addFrame(*mCurrentFrameInfo);
	}

Vsync信号到来后CanvasContext::prepareTree，县拷贝UI中的信息

	void CanvasContext::prepareTree(TreeInfo& info, int64_t* uiFrameInfo, int64_t syncQueued) {
	    mRenderThread.removeFrameCallback(this);
	
	    // If the previous frame was dropped we don't need to hold onto it, so
	    // just keep using the previous frame's structure instead
	    if (!wasSkipped(mCurrentFrameInfo)) {
	        mCurrentFrameInfo = &mFrames.next();
	    }
	    mCurrentFrameInfo->importUiThreadInfo(uiFrameInfo);
	    mCurrentFrameInfo->set(FrameInfoIndex::SyncQueued) = syncQueued;
	    mCurrentFrameInfo->markSyncStart();
	
预先构建


	void DrawFrameTask::run() {
	    ATRACE_NAME("DrawFrame");
	
	    bool canUnblockUiThread;
	    bool canDrawThisFrame;
	    {
	        // 这里用的是TreeInfo::MODE_FULL
	        TreeInfo info(TreeInfo::MODE_FULL, mRenderThread->renderState());
	        canUnblockUiThread = syncFrameState(info);
	        canDrawThisFrame = info.out.canDrawThisFrame;
	    }
	
	    // Grab a copy of everything we need
	    CanvasContext* context = mContext;
	
	    // From this point on anything in "this" is *UNSAFE TO ACCESS*
	    if (canUnblockUiThread) {
	        unblockUiThread();
	    }
	
	    if (CC_LIKELY(canDrawThisFrame)) {
	        context->draw();
	    }
	
	    if (!canUnblockUiThread) {
	        unblockUiThread();
	    }
	}
	
	bool DrawFrameTask::syncFrameState(TreeInfo& info) {
	    ATRACE_CALL();
	    int64_t vsync = mFrameInfo[static_cast<int>(FrameInfoIndex::Vsync)];
	    mRenderThread->timeLord().vsyncReceived(vsync);
	    mContext->makeCurrent();
	    Caches::getInstance().textureCache.resetMarkInUse(mContext);
	
	    for (size_t i = 0; i < mLayers.size(); i++) {
	        // 更新Layer 这里牵扯到图层数据的再拷贝吧
	        mContext->processLayerUpdate(mLayers[i].get());
	    }
	    mLayers.clear();
	    // 处理Tree
	    mContext->prepareTree(info, mFrameInfo, mSyncQueued);
	
	    // This is after the prepareTree so that any pending operations
	    // (RenderNode tree state, prefetched layers, etc...) will be flushed.
	    if (CC_UNLIKELY(!mContext->hasSurface())) {
	        mSyncResult |= kSync_LostSurfaceRewardIfFound;
	    }
	
	    if (info.out.hasAnimations) {
	        if (info.out.requiresUiRedraw) {
	            mSyncResult |= kSync_UIRedrawRequired;
	        }
	    }
	    // If prepareTextures is false, we ran out of texture cache space
	    return info.prepareTextures;
	}

Java层的信息来自下面，其实只有Java层的task在UI线程，其余的都在render线程，postandwait，同步完成后，UI线程就不会被阻塞了，后面的draw

>Chrophopher

绘制开始，测量 动画  inupt
	
	  void doFrame(long frameTimeNanos, int frame) {
	        final long startNanos;
	        synchronized (mLock) {
	            if (!mFrameScheduled) {
	                return; // no work to do
	            }
	
	            if (DEBUG_JANK && mDebugPrintNextFrameTimeDelta) {
	                mDebugPrintNextFrameTimeDelta = false;
	                Log.d(TAG, "Frame time delta: "
	                        + ((frameTimeNanos - mLastFrameTimeNanos) * 0.000001f) + " ms");
	            }
	
	            long intendedFrameTimeNanos = frameTimeNanos;
	            startNanos = System.nanoTime();
	            final long jitterNanos = startNanos - frameTimeNanos;
	            if (jitterNanos >= mFrameIntervalNanos) {
	                final long skippedFrames = jitterNanos / mFrameIntervalNanos;
	                if (skippedFrames >= SKIPPED_FRAME_WARNING_LIMIT) {
	                    Log.i(TAG, "Skipped " + skippedFrames + " frames!  "
	                            + "The application may be doing too much work on its main thread.");
	                }
	                final long lastFrameOffset = jitterNanos % mFrameIntervalNanos;
	                if (DEBUG_JANK) {
	                    Log.d(TAG, "Missed vsync by " + (jitterNanos * 0.000001f) + " ms "
	                            + "which is more than the frame interval of "
	                            + (mFrameIntervalNanos * 0.000001f) + " ms!  "
	                            + "Skipping " + skippedFrames + " frames and setting frame "
	                            + "time to " + (lastFrameOffset * 0.000001f) + " ms in the past.");
	                }
	                frameTimeNanos = startNanos - lastFrameOffset;
	            }
	
	            if (frameTimeNanos < mLastFrameTimeNanos) {
	                if (DEBUG_JANK) {
	                    Log.d(TAG, "Frame time appears to be going backwards.  May be due to a "
	                            + "previously skipped frame.  Waiting for next vsync.");
	                }
	                scheduleVsyncLocked();
	                return;
	            }
	
	            mFrameInfo.setVsync(intendedFrameTimeNanos, frameTimeNanos);
	            mFrameScheduled = false;
	            mLastFrameTimeNanos = frameTimeNanos;
	        }
	
	        try {
	            Trace.traceBegin(Trace.TRACE_TAG_VIEW, "Choreographer#doFrame");
	            <!--输入事件-->
	            mFrameInfo.markInputHandlingStart();
	            doCallbacks(Choreographer.CALLBACK_INPUT, frameTimeNanos);
	            <!--动画-->
	            mFrameInfo.markAnimationsStart();
	            doCallbacks(Choreographer.CALLBACK_ANIMATION, frameTimeNanos);
	            <!---->
	            mFrameInfo.markPerformTraversalsStart();
	

ThreadRender.java  

    void draw(View view, AttachInfo attachInfo, HardwareDrawCallbacks callbacks) {
        attachInfo.mIgnoreDirtyState = true;

        final Choreographer choreographer = attachInfo.mViewRootImpl.mChoreographer;
        choreographer.mFrameInfo.markDrawStart();
 
 draw从这个开始
        	
	            
The following are a few things to note about the output:

* For each visible application, the tool displays a graph.
* Each vertical bar along the horizontal axis represents a frame, and the height of each vertical bar represents the amount of time the frame took to render (in milliseconds).
* The horizontal green line represents 16 milliseconds. To achieve 60 frames per second, the vertical bar for each frame needs to stay below this line. Any time a bar surpasses this line, there may be pauses in the animations.
* The tool highlights frames that exceed the 16 millisecond threshold by making the corresponding bar wider and less transparent.
* Each bar has colored components that map to a stage in the rendering pipeline. The number of components vary depending on the API level of the device.

![GPU呈现模式](https://upload-images.jianshu.io/upload_images/1460468-ff5f91880763bbb0.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

通过 $ adb shell dumpsys gfxinfo your_package可以得到当前GPU渲染使用的时间


	    Draw	Prepare	Process	Execute
		10.30	0.67	5.21	3.51
		2.73	6.59	5.06	6.03
		4.38	1.92	5.81	5.32
		7.54	0.35	5.12	2.67
		4.18	0.50	5.58	1.97
		2.97	0.61	5.09	1.90
		3.23	0.51	5.22	1.78
		16.66	0.52	5.00	2.65
		....
	
	Stats since: 816160107662ns
	Total frames rendered: 185077
	Janky frames: 21764 (11.76%)
	90th percentile: 18ms
	95th percentile: 29ms
	99th percentile: 53ms
	Number Missed Vsync: 2746
	Number High input latency: 165
	Number Slow UI thread: 12367
	Number Slow bitmap uploads: 1621
	Number Slow issue draw commands: 7533

>在 Android M 中 gfxinfo（Profile data in ms） 的基础数值来源于 FrameInfo，详见源码：FrameInfoVisualizer。gfxinfo（Profile data in ms）只保存了 Surface 最近渲染的128帧的信息，因此，Jankiness count、Max accumulated frames、Frame rate 也仅仅是针对这 128 帧数据所计算出来的结果,它们的具体含义分别是：



* (1). Draw 对应于蓝色线：是消耗在构建java显示列表DisplayList的时间。说白了就是执行每一个View的onDraw方法,创建或者更新每一个View的DisplayList对象的时间。
* (2). Process 对应于红色线：是消耗在Android的2D渲染器执行显示列表的时间。你的视图层次越多，要执行的绘图命令就越多。
* (3). Execute 对应于橙色线：是消耗在排列每个发送过来的帧的顺序的时间.或者说是CPU告诉GPU渲染一帧的时间,这是一个阻塞调用,因为CPU会一直等待GPU发出接到命令的回复。其实可以简单理解为：红色线<span style="font-family: Arial, Helvetica, sans-serif;">Process时间＋GPU返回时</span><span style="font-family: Arial, Helvetica, sans-serif;">GPU</span><span style="font-family: Arial, Helvetica, sans-serif;">与CPU通信的时间</span>


GPU 呈现模式分析工具又可以获取什么信息呢：

* 可以查看android 手机每一帧的渲染情况，每一帧的总耗时，是否超过16ms，中间绿色横线表示16ms
* 指定计算区域的耗时情况，图片里每种颜色代表不同的数据计算阶段

渲染流程

* cpu测量，布局界面上变动的视图对象，然后绘制这些 view(onDraw方法) 生成界面一帧数据
* 然后 cpu 把这计算出的这一帧数据传递给 gpu，这一帧数据也叫纹理，具体的去看 OpenGL的内容
* gpu 根据cpu 传递过来的纹理数据，去具体的绘制出2D 图形来
* cpu 等待 pgu 通知绘制完成，cpu 才可以去干别的事，要不 cpu 会一直等着。。。这才算是完成了一帧的渲染

# swapbuffer的意义？

看名字，是交换Buffer，如果之前有个Buffer在用，现在就换一个，后面的替换前面的，双缓冲？ 3缓冲？queueBuffer，SF不一定能用，等GPU处理完SF才能用通知GPU执行，到底是哪个在通知呢 ？ APP 还是。。。

queue一个，dequeue一个，双缓冲，就两个，queue好后，queue的不一定会被用，如果dequeue失败，说明，还来不及用，如果成功，说明，就要用了，每个surface有两个，前后，slot是个缓存，


# FBO离屏渲染

申请个fbo，绘制材质fbo 直接用fbo，纹理，绑定图片纹理，纹理，绑定fbo，填充fbo，

# 参考文档

[Analyze with Profile GPU Rendering](https://developer.android.com/topic/performance/rendering/profile-gpu)    
[Android客户端性能工具2:FrameInfoVisualizer(gfxinfo和开发者选项gpu信息)分析](https://blog.csdn.net/woai110120130/article/details/79246547)    
[Android5.0中 hwui 中 RenderThread 工作流程](https://www.jianshu.com/p/bc1c1d2fadd1)    
[](http://www.voidcn.com/article/p-njbssmva-bqc.html)         
[原Android 5.1 SurfaceFlinger VSYNC详解](https://blog.csdn.net/newchenxf/article/details/49131167)                     
[Android中的GraphicBuffer同步机制-Fence](https://blog.csdn.net/jinzhuojun/article/details/39698317)                           
[android graphic(15)—fence](https://blog.csdn.net/lewif/article/details/50984212)              
[原android graphic(16)—fence(简化](https://blog.csdn.net/lewif/article/details/51007148)    
[了解 Systrace](https://source.android.com/devices/tech/debug/systrace)            
[Android帧率、卡顿详解及使用](https://blog.csdn.net/Jack_Chen3/article/details/76714030)       
[检查 GPU 渲染速度和绘制过度](https://developer.android.com/studio/profile/inspect-gpu-rendering?hl=zh-cn)         
[Analyze with Profile GPU Rendering](https://developer.android.com/topic/performance/rendering/profile-gpu)     
 [检查 GPU 渲染速度和绘制过度](https://developer.android.com/studio/profile/inspect-gpu-rendering?hl=zh-cn)         
[Analyze with Profile GPU Rendering](https://developer.android.com/topic/performance/rendering/profile-gpu)