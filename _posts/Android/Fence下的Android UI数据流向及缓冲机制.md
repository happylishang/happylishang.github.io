

## 源码中的三缓冲数据流向 
 
 
**updateAndReleaseLocked会释放之前的Buffer，但是同时会抓住当前buffer**

mSurfaceFlingerConsumer->setReleaseFence(layer->getAndResetReleaseFence());
上面主要是针对Overlay的层，那对于GPU绘制的层呢？在收到INVALIDATE消息时，SurfaceFlinger会依次调用handleMessageInvalidate()->handlePageFlip()->Layer::latchBuffer()->SurfaceFlingerConsumer::updateTexImage() ，其中会调用该层对应Consumer的GLConsumer::updateAndReleaseLocked() 函数。该函数会释放老的GraphicBuffer，释放前会通过syncForReleaseLocked()函数插入releaseFence，代表如果触发时该GraphicBuffer消费者已经使用完毕。然后调用releaseBufferLocked()还给BufferQueue
 
 
 
#  Android图形系统中的双缓冲与三缓冲配置

三缓冲并非是必须的，Android系统中是可以配置的，有个TARGET_DISABLE_TRIPLE_BUFFERING标记，如果不想支持三缓冲，设置该标记即可。这部分的代码在Layer.cpp中，每个Surface都对应一个Window，在SurfaceFlinger端对应一个Layer，每个Layer都拥有一个BufferQueue，用来存储UI渲染需要的数据。

### TARGET_DISABLE_TRIPLE_BUFFERING设置三缓冲开关
	
> 	Layer.cpp

		void Layer::onFirstRef() {
		    // Creates a custom BufferQueue for SurfaceFlingerConsumer to use
		    sp<IGraphicBufferProducer> producer;
		    sp<IGraphicBufferConsumer> consumer;
		    BufferQueue::createBufferQueue(&producer, &consumer);
		      <!--创建producer与consumer-->
		    mProducer = new MonitoredProducer(producer, mFlinger);
		    mSurfaceFlingerConsumer = new SurfaceFlingerConsumer(consumer, mTextureName);
		    mSurfaceFlingerConsumer->setConsumerUsageBits(getEffectiveUsage(0));
		    mSurfaceFlingerConsumer->setContentsChangedListener(this);
		    mSurfaceFlingerConsumer->setName(mName);
		
		<!--三缓冲还是双缓冲-->
		#ifdef TARGET_DISABLE_TRIPLE_BUFFERING
		<!--不支持三缓冲，只用双缓冲-->
		#warning "disabling triple buffering"
		    mSurfaceFlingerConsumer->setDefaultMaxBufferCount(2);
		#else
		<!--支持三缓冲-->
		    mSurfaceFlingerConsumer->setDefaultMaxBufferCount(3);
		#endif
		    const sp<const DisplayDevice> hw(mFlinger->getDefaultDisplayDevice());
		    updateTransformHint(hw);
		}
		

SurfaceFlingerConsumer的setDefaultMaxBufferCount可以认为是设支持的最大缓冲帧数，最终调用的是BufferQueueCore的setDefaultMaxBufferCountLocked
	
	
			
		status_t BufferQueueConsumer::setDefaultMaxBufferCount(int bufferCount) {
		    ATRACE_CALL();
		    Mutex::Autolock lock(mCore->mMutex);
		    return mCore->setDefaultMaxBufferCountLocked(bufferCount);
		}
	
		 // setDefaultMaxBufferCountLocked sets the maximum number of buffer slots
	    // that will be used if the producer does not override the buffer slot
	    // count. The count must be between 2 and NUM_BUFFER_SLOTS, inclusive. The
	    // initial default is 2.
	    
	    <!--   enum { NUM_BUFFER_SLOTS = 64 };-->
	    
	    status_t BufferQueueCore::setDefaultMaxBufferCountLocked(int count) {
	    const int minBufferCount = mUseAsyncBuffer ? 2 : 1;
	    if (count < minBufferCount || count > BufferQueueDefs::NUM_BUFFER_SLOTS) {
	        return BAD_VALUE;
	    }
	
	    BQ_LOGV("setDefaultMaxBufferCount: setting count to %d", count);
	    mDefaultMaxBufferCount = count;
	    mDequeueCondition.broadcast();
	
	    return NO_ERROR;
	}

每个BufferQueue的上限是NUM_BUFFER_SLOTS=64个，虽然理论上能达到64，但是一般就设置2或者3，这64个位置通常是分成**3个一份，对应一种窗口状态，比如窗口Size改变了，那就换立刻换一套。**，设置这个mDefaultMaxBufferCount的意义是什么呢？在哪里用了？

    // allocated for a slot when requestBuffer is called with that slot's index.
    BufferQueueDefs::SlotsType mSlots;

    // mQueue is a FIFO of queued buffers used in synchronous mode.
    Fifo mQueue;

    // mFreeSlots contains all of the slots which are FREE and do not currently
    // have a buffer attached
    std::set<int> mFreeSlots;

    // mFreeBuffers contains all of the slots which are FREE and currently have
    // a buffer attached
    std::list<int> mFreeBuffers;
    
 
 分配BufferSlot       
    
	void BufferQueueProducer::allocateBuffers(bool async, uint32_t width,
	        uint32_t height, PixelFormat format, uint32_t usage) {
	    while (true) {
	       		...
		 
	            int currentBufferCount = 0;
	            for (int slot = 0; slot < BufferQueueDefs::NUM_BUFFER_SLOTS; ++slot) {
	                if (mSlots[slot].mGraphicBuffer != NULL) {
	                <!--计算用了几个buffer了-->
	                    ++currentBufferCount;
	                } else {
	                    if (mSlots[slot].mBufferState != BufferSlot::FREE) {
	                        continue;
	                    }
	                    freeSlots.push_back(slot);
	                }
	            }

	            int maxBufferCount = mCore->getMaxBufferCountLocked(async);
	            BQ_LOGV("allocateBuffers: allocating from %d buffers up to %d buffers",
	                    currentBufferCount, maxBufferCount);
	               <!--如果超过限制，不分配，直接return-->
	              if (maxBufferCount <= currentBufferCount)
                return;
	            ...
	          }
	          

### buffer状态 
	 
	
	 // All slots are initially FREE.
    
    enum BufferState {
        // FREE indicates that the buffer is available to be dequeued
        // by the producer.  The buffer may be in use by the consumer for
        // a finite time, so the buffer must not be modified until the
        // associated fence is signaled.
        //
        // The slot is "owned" by BufferQueue.  It transitions to DEQUEUED
        // when dequeueBuffer is called.
       
        <!--Free不代表没数据，BufferQueue拥有，但是没使用权，但是可以DEQUEUED，有可能正在被consumer拿着Fence，等着用呢，但是用完了就可以被DEQUEUED的一方用，可以先让他DEQUEUED-->
        FREE = 0,  

        // DEQUEUED indicates that the buffer has been dequeued by the
        // producer, but has not yet been queued or canceled.  The
        // producer may modify the buffer's contents as soon as the
        // associated ready fence is signaled.
        //
        // The slot is "owned" by the producer.  It can transition to
        // QUEUED (via queueBuffer) or back to FREE (via cancelBuffer).
        
        <!--DEQUEUED不代表能用，拥有但不代表能用，必须等到Fence释放-->
        DEQUEUED = 1,

        // QUEUED indicates that the buffer has been filled by the
        // producer and queued for use by the consumer.  The buffer
        // contents may continue to be modified for a finite time, so
        // the contents must not be accessed until the associated fence
        // is signaled.
        //
        // The slot is "owned" by BufferQueue.  It can transition to
        // ACQUIRED (via acquireBuffer) or to FREE (if another buffer is
        // queued in asynchronous mode).
        
        <!--QUEUED之后也不代表能直接用，BufferQueue拥有但是没使用权，还有可能被GPU处理-->
        QUEUED = 2,

        // ACQUIRED indicates that the buffer has been acquired by the
        // consumer.  As with QUEUED, the contents must not be accessed
        // by the consumer until the fence is signaled.
        //
        // The slot is "owned" by the consumer.  It transitions to FREE
        // when releaseBuffer is called.
        <!--ACQUIRED被SF拥有，但是不代表立即拥有使用权，必须获取Fence才行-->
        ACQUIRED = 3
    };

可以看到整个Buffer的流转都牵扯到Fence，

    // mFence is a fence which will signal when work initiated by the
    // previous owner of the buffer is finished. 
    
    //  When the buffer is FREE,the fence indicates when the consumer has finished reading
    // from the buffer, or when the producer has finished writing if it
    // called cancelBuffer after queueing some writes. 
    
    When the buffer is QUEUED, it indicates when the producer has finished filling the  buffer. 
    
    When the buffer is DEQUEUED or ACQUIRED, the fence has been passed to the consumer or producer along with ownership of the  buffer, and mFence is set to NO_FENCE.
    

    sp<Fence> mFence;

前一个Fence owner初始化的任务被后一个owner完成的时候，Fence会发信号。


生产者利用opengl绘图，不用等绘图完成，直接queue buffer，在queue buffer的同时，需要传递给BufferQueue一个fence，而消费者acquire这个buffer后同时也会获取到这个fence，这个fence在GPU绘图完成后signal。这就是所谓的“acquireFence”，用于生产者通知消费者生产已完成。

当消费者对acquire到的buffer做完自己要做的事情后（例如把buffer交给surfaceflinger去合成），就要把buffer release到BufferQueue的free list，由于该buffer的内容可能正在被surfaceflinger使用，所以release时也需要传递一个fence，用来指示该buffer的内容是否依然在被使用，接下来生产者在继续dequeue buffer时，如果dequeue到了这个buffer，在使用前先要等待该fence signal。这就是所谓的“releaseFence”，后者用于消费者通知生产者消费已完成。
 

         
	          
# 采用双缓冲与三缓冲的关键
	 
	status_t BufferLayerConsumer::updateTexImage(BufferRejecter* rejecter, nsecs_t expectedPresentTime,
	                                             bool* autoRefresh, bool* queuedBuffer,
	                                             uint64_t maxFrameNumber) {
	    ATRACE_CALL();
	    BLC_LOGV("updateTexImage");
	    Mutex::Autolock lock(mMutex);
	
	    if (mAbandoned) {
	        return NO_INIT;
	    }
	
	    BufferItem item;
	
	    // Acquire the next buffer.
	    // In asynchronous mode the list is guaranteed to be one buffer
	    // deep, while in synchronous mode we use the oldest buffer.
 
	    status_t err = acquireBufferLocked(&item, expectedPresentTime, maxFrameNumber);
	    if (err != NO_ERROR) {
	        if (err == BufferQueue::NO_BUFFER_AVAILABLE) {
	            err = NO_ERROR;
	        } else if (err == BufferQueue::PRESENT_LATER) {
	            // return the error, without logging
	        } else {
	            BLC_LOGE("updateTexImage: acquire failed: %s (%d)", strerror(-err), err);
	        }
	        return err;
	    }
	
	    if (autoRefresh) {
	        *autoRefresh = item.mAutoRefresh;
	    }
	
	    if (queuedBuffer) {
	        *queuedBuffer = item.mQueuedBuffer;
	    }
	
	    // We call the rejecter here, in case the caller has a reason to
	    // not accept this buffer.  This is used by SurfaceFlinger to
	    // reject buffers which have the wrong size
	    int slot = item.mSlot;
	    if (rejecter && rejecter->reject(mSlots[slot].mGraphicBuffer, item)) {
	        releaseBufferLocked(slot, mSlots[slot].mGraphicBuffer);
	        return BUFFER_REJECTED;
	    }
	
	    // Release the previous buffer.
	    <!--注意release的是前一个buffer-->
	    err = updateAndReleaseLocked(item, &mPendingRelease);
	    if (err != NO_ERROR) {
	        return err;
	    }
	
	    if (!mRE.useNativeFenceSync()) {
	        // Bind the new buffer to the GL texture.
	        //
	        // Older devices require the "implicit" synchronization provided
	        // by glEGLImageTargetTexture2DOES, which this method calls.  Newer
	        // devices will either call this in Layer::onDraw, or (if it's not
	        // a GL-composited layer) not at all.
	        err = bindTextureImageLocked();
	    }
	
	    return err;
	}

 mCurrentTextureBuffer = nextTextureBuffer 更新机制

	status_t BufferLayerConsumer::updateAndReleaseLocked(const BufferItem& item,
	                                                     PendingRelease* pendingRelease) {
	    status_t err = NO_ERROR;
	
	    int slot = item.mSlot;
	
	    BLC_LOGV("updateAndRelease: (slot=%d buf=%p) -> (slot=%d buf=%p)", mCurrentTexture,
	             (mCurrentTextureBuffer != nullptr && mCurrentTextureBuffer->graphicBuffer() != nullptr)
	                     ? mCurrentTextureBuffer->graphicBuffer()->handle
	                     : 0,
	             slot, mSlots[slot].mGraphicBuffer->handle);
	
	    // Hang onto the pointer so that it isn't freed in the call to
	    // releaseBufferLocked() if we're in shared buffer mode and both buffers are
	    // the same.
	
	    std::shared_ptr<Image> nextTextureBuffer;
	    {
	        std::lock_guard<std::mutex> lock(mImagesMutex);
	        nextTextureBuffer = mImages[slot];
	    }
	
	    // release old buffer
	    if (mCurrentTexture != BufferQueue::INVALID_BUFFER_SLOT) {
	        if (pendingRelease == nullptr) {
	            status_t status =
	                    releaseBufferLocked(mCurrentTexture, mCurrentTextureBuffer->graphicBuffer());
	            if (status < NO_ERROR) {
	                BLC_LOGE("updateAndRelease: failed to release buffer: %s (%d)", strerror(-status),
	                         status);
	                err = status;
	                // keep going, with error raised [?]
	            }
	        } else {
	            pendingRelease->currentTexture = mCurrentTexture;
	            pendingRelease->graphicBuffer = mCurrentTextureBuffer->graphicBuffer();
	            pendingRelease->isPending = true;
	        }
	    }
	
	    // Update the BufferLayerConsumer state.
	    mCurrentTexture = slot;
	    mCurrentTextureBuffer = nextTextureBuffer;
	    mCurrentCrop = item.mCrop;
	    mCurrentTransform = item.mTransform;
	    mCurrentScalingMode = item.mScalingMode;
	    mCurrentTimestamp = item.mTimestamp;
	    mCurrentDataSpace = static_cast<ui::Dataspace>(item.mDataSpace);
	    mCurrentHdrMetadata = item.mHdrMetadata;
	    mCurrentFence = item.mFence;
	    mCurrentFenceTime = item.mFenceTime;
	    mCurrentFrameNumber = item.mFrameNumber;
	    mCurrentTransformToDisplayInverse = item.mTransformToDisplayInverse;
	    mCurrentSurfaceDamage = item.mSurfaceDamage;
	    mCurrentApi = item.mApi;
	
	    computeCurrentTransformMatrixLocked();
	
	    return err;
	}
	


	status_t GLConsumer::bindTextureImageLocked() {
	    if (mEglDisplay == EGL_NO_DISPLAY) {
	        ALOGE("bindTextureImage: invalid display");
	        return INVALID_OPERATION;
	    }
	
	    GLenum error;
	    while ((error = glGetError()) != GL_NO_ERROR) {
	        GLC_LOGW("bindTextureImage: clearing GL error: %#04x", error);
	    }
	
	    glBindTexture(mTexTarget, mTexName);
	    if (mCurrentTexture == BufferQueue::INVALID_BUFFER_SLOT &&
	            mCurrentTextureImage == nullptr) {
	        GLC_LOGE("bindTextureImage: no currently-bound texture");
	        return NO_INIT;
	    }
	
	    status_t err = mCurrentTextureImage->createIfNeeded(mEglDisplay);
	    if (err != NO_ERROR) {
	        GLC_LOGW("bindTextureImage: can't create image on display=%p slot=%d",
	                mEglDisplay, mCurrentTexture);
	        return UNKNOWN_ERROR;
	    }
	    mCurrentTextureImage->bindToTextureTarget(mTexTarget);
	
	    // In the rare case that the display is terminated and then initialized
	    // again, we can't detect that the display changed (it didn't), but the
	    // image is invalid. In this case, repeat the exact same steps while
	    // forcing the creation of a new image.
	    if ((error = glGetError()) != GL_NO_ERROR) {
	        glBindTexture(mTexTarget, mTexName);
	        status_t result = mCurrentTextureImage->createIfNeeded(mEglDisplay, true);
	        if (result != NO_ERROR) {
	            GLC_LOGW("bindTextureImage: can't create image on display=%p slot=%d",
	                    mEglDisplay, mCurrentTexture);
	            return UNKNOWN_ERROR;
	        }
	        mCurrentTextureImage->bindToTextureTarget(mTexTarget);
	        if ((error = glGetError()) != GL_NO_ERROR) {
	            GLC_LOGE("bindTextureImage: error binding external image: %#04x", error);
	            return UNKNOWN_ERROR;
	        }
	    }
	
	    // Wait for the new buffer to be ready.
	    return doGLFenceWaitLocked();
	}
	
	



注意，注意只有swapBuffer触发后，SF去acquireBuffer会用，使用的时候通过acquireFence的时候发现用不了，就不能release

才会让前一个Buffer被释放，如果不存在swapBuffer，那么前一个Buffer就会被SF一直占用着？？？
