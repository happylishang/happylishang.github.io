FPS(Frames Per Second)每秒传输帧数，在图像领域中，FPS越高动画就会越流畅。在APP开发中，FPS也只是针对特定的场景有意义，比如List滚动，动画，视频播放等，而对于一些常用的点击、静止则没有太大意义，脱离场景谈FPS没有意义，而流畅度其实也对应这几个场景。APP的性能监测工具也通常会将FPS作为一个重要的指标，本文就结合几款流行的性能监测工具分析下FPS监测原理。涉及腾讯GT、微信Matrix、360的ArgusAPM。


## 先看下微信matrix：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-0a91505303d75580.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

matrix帧率检测关键代码如下：其实并非完全的帧率检测，而是动态模拟Vsync信号到来，检测每个16ms是否能够完成UI任务

	public final class FrameBeat implements IFrameBeat, Choreographer.FrameCallback, ApplicationLifeObserver.IObserver {
				....
	/**
	     * when the device's Vsync is coming,it will be called.
	     *
	     * @param frameTimeNanos The time in nanoseconds when the frame started being rendered.
	     */
	    @Override
	    public void doFrame(long frameTimeNanos) {
	        if (isPause) {
	            return;
	        }
	       <!--mLastFrameNanos校准-->
	        if (frameTimeNanos < mLastFrameNanos || mLastFrameNanos <= 0) { 
	            mLastFrameNanos = frameTimeNanos;
	            if (null != mChoreographer) {
	                mChoreographer.postFrameCallback(this);
	            }
	            return;
	        }
	
	        if (null != mFrameListeners) {
		       <!--mLastFrameNanos跟当前frameTimeNanos时间传给Lister，做检测-->
	            for (IFrameBeatListener listener : mFrameListeners) {
	                listener.doFrame(mLastFrameNanos, frameTimeNanos);
	            }
	
	            if (null != mChoreographer) {
	                mChoreographer.postFrameCallback(this);
	            }
			<!--更新mLastFrameNanos开启下一轮检测-->
	            mLastFrameNanos = frameTimeNanos;
	        }
	
	    }
   
*   起始点mLastFrameNanos = frameTimeNanos，第一次检测无效：
*   第二次开始时候mLastFrameNanos = frameTimeNanos（上一个），但是在最后更新mLastFrameNanos的时候，后面的很多消息还没执行
*   等到新的VSYNC到来的时候frameTimeNanos，是下一个VSYNC执行的入口。VSYNC申请后，到来

## Choreographer.java的原始监听入口，原始跳帧检测

	    @UnsupportedAppUsage
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
	            // 是否存在跳帧
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
	
	            if (mFPSDivisor > 1) {
	                long timeSinceVsync = frameTimeNanos - mLastFrameTimeNanos;
	                if (timeSinceVsync < (mFrameIntervalNanos * mFPSDivisor) && timeSinceVsync > 0) {
	                    scheduleVsyncLocked();
	                    return;
	                }
	            }
	
	            mFrameInfo.setVsync(intendedFrameTimeNanos, frameTimeNanos);
	            mFrameScheduled = false;
	            mLastFrameTimeNanos = frameTimeNanos;
	        }
	
	        try {
	            Trace.traceBegin(Trace.TRACE_TAG_VIEW, "Choreographer#doFrame");
	            AnimationUtils.lockAnimationClock(frameTimeNanos / TimeUtils.NANOS_PER_MS);
	
	            mFrameInfo.markInputHandlingStart();
	            doCallbacks(Choreographer.CALLBACK_INPUT, frameTimeNanos);
	
	            mFrameInfo.markAnimationsStart();
	            doCallbacks(Choreographer.CALLBACK_ANIMATION, frameTimeNanos);
	
	            mFrameInfo.markPerformTraversalsStart();
	            doCallbacks(Choreographer.CALLBACK_TRAVERSAL, frameTimeNanos);
	
	            doCallbacks(Choreographer.CALLBACK_COMMIT, frameTimeNanos);
	        } finally {
	            AnimationUtils.unlockAnimationClock();
	            Trace.traceEnd(Trace.TRACE_TAG_VIEW);
	        }

	    }
	
	    
    
## 再来看下360 ArgusAPM

## 腾讯GT




可以看出几款性能监测工具的原理大同小异，插入的检测时机都是Choregrapher的doFrame，都是不断的请求vsync信号，做监测，如果某个draw耗时，就可以定位点，其实就是不断的检测两个vsync的消耗是否超时，如果超时说明一个16ms无法完成任务，那就会出现掉帧的可能。