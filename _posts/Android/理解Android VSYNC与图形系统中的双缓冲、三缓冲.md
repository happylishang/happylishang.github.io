先接触两个图形概念： 帧率（Frame Rate，单位FPS）--GPU显卡生成帧的速率，也可以认为是数据处理的速度）， 屏幕刷新频率 （Refresh Rate单位赫兹/HZ）：是指硬件设备刷新屏幕的频率。屏幕刷新率一般是固定的，比如60Hz的每16ms就刷一次屏幕，可以类比一下黑白电视的电子扫描枪，每16ms电子枪从上到下从左到右一行一行逐渐把图片绘制出来，如果GPU显卡性能非常强悍，帧率可以非常高，甚至会高于屏幕刷新频率。

[本文参考视频 Google IO](https://www.youtube.com/watch?v=Q8m9sHdyXnE)
# 单缓存画面撕裂与（垂直同步+双缓冲）

什么是画面撕裂？如下：用两帧的部分数据合成一帧。

![画面撕裂](https://www.androidpolice.com/wp-content/uploads/2012/07/0006_Layer-2.png)

The display (LCD, AMOLED, whatever) gets each frame from the graphics chip, and starts drawing it line by line. Ideally, you want the display to get a new frame from the graphics chip after it is finished drawing the previous frame. Tearing occurs when the graphics chip loads a new frame in the middle of the LCD draw, so you get half of one frame and half of another.

如果只有一块缓存，在没有加锁的情况下，容易出现。即：在屏幕更新的时候，如果显卡输出帧率很高，在A帧的数据上半部分刚更新完时，B帧就到了，如果没采取同步锁机制，可以认为**帧到了就可用**，在继续刷新下半部分时，由于只有一块存储，A被B覆盖，绘制用的数据就是B帧，此时就会出现上半部分是A下半部分是B，这就是屏幕撕裂，**个人觉得描述成显卡瞬时帧率过高也许更好**。同正常帧绘制相比，正常的帧给时间才就能完整绘制一帧，但撕裂的帧没有机会补全。

![image.png](https://upload-images.jianshu.io/upload_images/1460468-d8a7b252191b7ad8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

相比较画面撕裂场景如下：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-4424c66d36b291f2.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

不过按照Android官方指导的说法，屏幕撕裂还有另外一种解释，那就是显示器用了半成品的帧，不过我是不太理解他说的这点。[参考视频](https://youtu.be/1iaHxmfZGGc?list=UU_x5XG1OV2P6uZZ5FSM9Ttw&t=112)

以上说的是只有一块显示存储的情况，其实只要加锁就能解决。那么如果多增加一块显示存储区能解决吗？显卡绘制成功后，先写入BackBuffer，不影响当前正在展示的FrameBuffer，这就是双缓冲，但是理论上其实也不行，因为BackBuffer毕竟也是要展示的，也要”拷贝“到FrameBuffer，在A帧没画完，BackBuffer如果不加干预，直接”拷贝“到FrameBuffer同样出现撕裂。所以**同步锁的机制才是关键**，必须有这么一个机制告诉GPU显卡，**要等待当前帧绘完整，才能替换当前帧**。但如果仅仅单缓存加锁的话GPU显卡会被挂啊？这就让效率低了，那就一边加同步锁，同时再多加一个缓存，垂直同步（VSYNC）就可看做是这么个东西，其实两者是配合使用的。

![image.png](https://upload-images.jianshu.io/upload_images/1460468-30ac3ea4118e9390.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

再来看下VSYNC，屏幕刷新从左到右水平扫描（Horizontal Scanning），从上到下垂直扫描Vertical Scanning，**垂直扫描完成则整个屏幕刷新完毕，这便是告诉外界可以绘制下一帧的时机**，在这里发出VSync信号，通知GPU给FrameBuffer传数据，完成后，屏幕便可以开始刷新，所以或许称之为**帧同步**更合适。VSYNC**强制帧率和显示器刷新频率同步**，如果当前帧没绘制完，即使下一帧准备好了，也禁止使用下一帧，直到显示器绘制完当前帧，等下次刷新的时候，才会用下一帧。比如：如果显示器的刷新频率是60HZ显示器，开了垂直同步后，显示帧率就会被锁60，即使显卡输出高，也没用。对Android系统而言，垂直同步信号除了强制帧率和显示器刷新频率同步外，还有其他很多作用，VSYNC是APP端重绘、SurfaceFlinger图层合成的触发点，只有收到VSYNC信号，它们才会工作，以上便是个人对引入VSYNC与双缓冲的见解。

# 双缓冲的进阶：三缓冲

#### 双缓冲保证低延时，三缓冲保证稳定性，双缓冲不在16ms中间开始，有足够时间绘制 三缓冲增加其韧性。


在Android系统里，除了双缓冲，还有个三缓冲，不过这个三缓冲是对于**屏幕硬件刷新**之外而言，它关注的是整个Android图形系统的消费者模型，跟Android自身的VSYNC用法有关系，在 Jelly Bean 中Android扩大了VSYNC使用场景与效果，不仅用在屏幕刷新防撕裂，同时也用在APP端绘制及SurfaceFlinger合成那，此时对VSYNC利用有点像Pipeline流水线，贯穿整个绘制流程，对比下VSYNC扩展使用的区别：

![没有垂直同步](https://www.androidpolice.com/wp-content/uploads/2012/07/Untitled-11.png)

如果想要达到60FPS的流畅度，每16毫秒必须刷新一帧，否则动画、视频就没那么丝滑，扩展后：

![有垂直同步](https://www.androidpolice.com/wp-content/uploads/2012/07/0003_Layer-51.png)

对于没采用VSYNC做调度的系统来说，比如Project Butter之前的系统（4.1以下），CPU的对于显示帧的处理是凌乱的，优先级也没有保障，处理完一帧后，CPU可能并不会及时处理下一帧，可能会优先处理其他消息，等到它开始处理UI生成帧的时候，可能已经处于VSYNC的中间，这样就很**容易跨两个VYSNC**信号，导致掉帧。在Jelly Bean中，下一帧的处理被限定在VSync信号到达时，并且看Android的处理UI重绘消息的优先级是比较高的，其他的同步消息均不会执行，从而保证每16ms处理一帧有序进行，同时由于是**在每个VSYNC信号到达时就处理帧，可以尽量避免跨越两帧的情况出现**。

上面的流程中，Android已经采用了双缓冲，**双缓冲不仅仅是两份存储，它是一个概念，双缓冲是一条链路，不是某一个环节，是整个系统采用的一个机制，需要各个环节的支持，从APP到SurfaceFlinger、到图像显示都要参与协作。**对于APP端而言，每个Window都是一个双缓冲的模型，一个Window对应一个Surface，而每个Surface里至少映射两个存储区，一个给图层合成显示用，一个给APP端图形处理，这便是应于上层的双缓冲。Android4.0之后基本都是默认硬件加速，CPU跟GPU都是并发处理任务的，CPU处理完之后就完工，等下一个VSYNC到来就可以进行下一轮操作。也就是CPU、GPU、显示都会用到Buffer，VSYNC+双缓冲在理想情况下是没有问题的，但如果某个环节出现问题，那就不一样了如下（帧耗时超过16ms）：

![双缓冲jank](https://www.androidpolice.com/wp-content/uploads/2012/07/0001_Layer-72.png)

可以看到在第二个阶段，存在CPU资源浪费，为什么呢？双缓冲Surface只会提供两个Buffer，一个Buffer被DisPlay占用（SurfaceFlinger用完后不会释放当前的Buffer，只会释放旧的Buffer,**直观的想一下，如果新Buffer生成受阻，那么肯定要保留一个备份给SF用，才能不阻碍合成显示，就必定要一直占用一个Buffer，新的Buffer来了才释放老的**），另一个被GPU处理占用，所以，CPU就无法获取到Buffer处理当前UI，在Jank的阶段空空等待。一般出现这种场景都是连续的：比如复杂视觉效果每一帧可能需要20ms（CPU 8ms +GPU 12ms），GPU可能会一直超负荷，CPU跟GPU一直抢Buffer，这样带来的问题就是滚雪球似的掉帧，一直浪费，**完全没有利用CPU与GPU并行处理的效率，成了串行处理**，如下所示

![image.png](https://upload-images.jianshu.io/upload_images/1460468-3de0622bf2e05a14.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

如何处理呢？让多增加一个Buffer给CPU用，让它提前忙起来，这样就能做到三方都有Buffer可用，CPU跟GPU不用争一个Buffer，真正实现并行处理。如下：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-b88cf9b2eb3d6bb0.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

如上图所示，虽然即使每帧需要20ms（CPU 8ms +GPU 12ms），但是由于多加了一个Buffer，实现了CPU跟GPU并行，便可以做到了只在开始掉一帧，后续却不掉帧，双缓冲充分利用16ms做到低延时，三缓冲保障了其稳定性，为什么4缓冲没必要呢？因为三个既可保证并行，四个徒增资源浪费。


## 源码中的三缓冲数据流向 
 
 
**updateAndReleaseLocked会释放之前的Buffer，但是同时会抓住当前buffer**

mSurfaceFlingerConsumer->setReleaseFence(layer->getAndResetReleaseFence());
上面主要是针对Overlay的层，那对于GPU绘制的层呢？在收到INVALIDATE消息时，SurfaceFlinger会依次调用handleMessageInvalidate()->handlePageFlip()->Layer::latchBuffer()->SurfaceFlingerConsumer::updateTexImage() ，其中会调用该层对应Consumer的GLConsumer::updateAndReleaseLocked() 函数。该函数会释放老的GraphicBuffer，释放前会通过syncForReleaseLocked()函数插入releaseFence，代表如果触发时该GraphicBuffer消费者已经使用完毕。然后调用releaseBufferLocked()还给BufferQueue
 
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


#  Android图形系统中的双缓冲与三缓冲配置

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