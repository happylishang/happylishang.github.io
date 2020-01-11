### 1、
* 每次都要重新合成，所以，无论何种时间段，SF必定会占用Surface的一个Buffer，

* 16ms根据内存内容刷新一次屏幕，点亮一屏幕led，等下一个vsync到来，再点亮一次，data与屏幕是分离的的

* 使用双缓冲是因为：一个存储区不适合同时写跟读，可能出问题

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