双缓冲是一条链路，不是某一个环节，是整个系统采用的一个机制，需要各个环节的支持，从APP到SurfaceFlinger、到图像显示都要参与协作。

VSYNC：刷新频率同步与不同步


### 双缓冲显示

这里存疑？

* 每次屏幕刷新，SurfaceFlinger都要重新合成，所以，无论何种时间，Surface必须要为SF保留一个用于显示的Buffer，
* 16ms根据内存内容刷新一次屏幕，点亮一屏幕led，等下一个vsync到来，再点亮一次，data与屏幕是分离的的
* 使用双缓冲是因为：一个存储区不适合同时写跟读，可能出问题

releaseBuffer猜想，SF，只有存在一个备份buffer的时候，才会releaseBuffer????


acquireFence会释放之前的Buffer，但是同时会抓住当前buffer

 mSurfaceFlingerConsumer->setReleaseFence(layer->getAndResetReleaseFence());
上面主要是针对Overlay的层，那对于GPU绘制的层呢？在收到INVALIDATE消息时，SurfaceFlinger会依次调用handleMessageInvalidate()->handlePageFlip()->Layer::latchBuffer()->SurfaceFlingerConsumer::updateTexImage() ，其中会调用该层对应Consumer的GLConsumer::updateAndReleaseLocked() 函数。该函数会释放老的GraphicBuffer，释放前会通过syncForReleaseLocked()函数插入releaseFence，代表如果触发时该GraphicBuffer消费者已经使用完毕。然后调用releaseBufferLocked()还给BufferQueue
 
#  这里才是双缓冲与三缓冲的关键
	 
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

 mCurrentTextureBuffer = nextTextureBuffer

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
	



注意，注意只有swapBuffer触发后，SF去acquireBuffer会用，使用的时候通过acquireFence的时候发现用不了，就不能release

才会让前一个Buffer被释放，如果不存在swapBuffer，那么前一个Buffer就会被SF一直占用着？？？


### Android图形系统中的双缓冲与三缓冲

### 双缓冲与三缓冲是可以配置的TARGET_DISABLE_TRIPLE_BUFFERING
	
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
		#warning "disabling triple buffering"
		    mSurfaceFlingerConsumer->setDefaultMaxBufferCount(2);
		#else
		    mSurfaceFlingerConsumer->setDefaultMaxBufferCount(3);
		#endif
		
		    const sp<const DisplayDevice> hw(mFlinger->getDefaultDisplayDevice());
		    updateTransformHint(hw);
		}
		
调用其实是BufferQueueCore的setDefaultMaxBufferCountLocked
		
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

虽然理论上能达到64，但是一般就设置2或者3，多了资源浪费。



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