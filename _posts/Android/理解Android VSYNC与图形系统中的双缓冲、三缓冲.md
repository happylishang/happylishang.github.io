先接触两个图形概念： 帧率（Frame Rate，单位FPS）--GPU显卡生成帧的速率，也可以认为是数据处理的速度）， 屏幕刷新频率 （Refresh Rate单位赫兹/HZ）：是指硬件设备刷新屏幕的频率。屏幕刷新率一般是固定的，比如60Hz的每16ms就刷一次屏幕，可以类比一下黑白电视的电子扫描枪，每16ms电子枪从上到下从左到右一行一行逐渐把图片绘制出来，如果GPU显卡性能非常强悍，帧率可以非常高，甚至会高于屏幕刷新频率。

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

# 双缓冲与三缓冲

既然有了双缓冲，为什么还要引入三缓冲呢？这里跟Android自身的用法有关系，在 Jelly Bean Android扩大了VSYNC使用场景与效果，不仅仅用在屏幕刷新那防撕裂，同时也用在APP端绘制及SurfaceFlinger合成那，这个时候它对VSYNC利用有点像Pipeline流水线，贯穿整个绘制流程。如果想要达到60FPS的流畅度，那么每16毫秒必须刷新一帧，否则动画或者视频就没那么丝滑，先对比下APP端是否采用VSYNC的对比

![没有垂直同步](https://www.androidpolice.com/wp-content/uploads/2012/07/Untitled-11.png)


![有垂直同步](https://www.androidpolice.com/wp-content/uploads/2012/07/0003_Layer-51.png)

对于没采用VSYNC做调度的系统来说，比如Project Butter之前的系统（4.1以下），

在Jelly Bean中，制作下一帧的整个过程在VSync脉冲开始时就在最后一帧结束后立即开始。
换句话说，他们正在使用尽可能多的16ms。

Most Android displays run at or around 60 frames per second (or 60 Hz, in display jargon). In order to have a smooth animation, you have to actually be able to process 60 frames per second - that means you've got 16ms to process each frame. If you take longer than 16ms, the animation will stutter, and that buttery smooth feeling we're aiming for melts away.

16 milliseconds isn't a lot of time, so you're going to want to make the most of it. In Ice Cream Sandwich, processing for the next frame would just kind-of lazily start whenever the system got around to it. In Jelly Bean, the whole process of making the next frame starts as soon as the last frame is finished, at the beginning of the VSync pulse. In other words, they're using as much of the 16ms as they can. Here's an example:

两个缓存区分别为 Back Buffer 和 Frame Buffer。GPU 向 Back Buffer 中写数据，屏幕从 Frame Buffer 中读数据。VSync 信号负责调度从 Back Buffer 到 Frame Buffer 的复制操作，可认为该复制操作在瞬间完成。其实，该复制操作是等价后的效果，实际上双缓冲的实现方式是交换 Back Buffer 和 Frame Buffer 的名字，更具体的说是交换内存地址（有没有联想到那道经典的笔试题目：“有两个整型数，如何用最优的方法交换二者的值？”），通过二位运算“与”即可完成，所以可认为是瞬间完成。
双缓冲的模型下，工作流程这样的：
在某个时间点，一个屏幕刷新周期完成，进入短暂的刷新空白期。此时，VSync 信号产生，先完成复制操作，然后通知 CPU/GPU 绘制下一帧图像。复制操作完成后屏幕开始下一个刷新周期，即将刚复制到 Frame Buffer 的数据显示到屏幕上。

在这种模型下，只有当 VSync 信号产生时，CPU/GPU 才会开始绘制。这样，当帧率大于刷新频率时，帧率就会被迫跟刷新频率保持同步，从而避免“tearing”现象。

注意，当 VSync 信号发出时，如果 GPU/CPU 正在生产帧数据，此时不会发生复制操作。屏幕进入下一个刷新周期时，从 Frame Buffer 中取出的是“老”数据，而非正在产生的帧数据，即两个刷新周期显示的是同一帧数据。这是我们称发生了“掉帧”（Dropped Frame，Skipped Frame，Jank）现象。





![双缓冲jank](https://www.androidpolice.com/wp-content/uploads/2012/07/0001_Layer-72.png)

![三缓冲缓解](https://www.androidpolice.com/wp-content/uploads/2012/07/0000_Layer-82.png)


## VSYNC（垂直同步）、Triple Buffer（三重缓存） 和 Choreographer三者配合 

[参考](https://www.youtube.com/watch?annotation_id=annotation_1709176545&feature=iv&list=UU_x5XG1OV2P6uZZ5FSM9Ttw&src_vid=HXQhu6qfTVU&v=1iaHxmfZGGc)


具体的是什么情况，一张gif就能很好的说明了。除了逐行扫描外还有隔行扫描，至于区别就是一个是一行一行画，一个是隔着一行画。目前大多数显示器采用的都是逐行扫描。老设备带宽不足只能隔行扫描，现在的新设备基本都是逐行扫描了。



这样一方面可以解决画面撕裂现象，因为不会出现缓冲还没画完被覆写的情况了。

另一方面也可以解决错帧现象。这里你可以做一个实验：

不开垂直同步，锁定60帧，然后玩一分钟，再打开垂直同步，再玩一分钟，你会发现，同样是60帧，开了垂直同步会比不开画面流畅，因为不会发生错帧了。

除此之外，由于垂直同步的开启，强制每帧间隔完全一样，这样因为帧生成时间不平滑导致的不流畅也会解决。



### 双缓冲显示

**双缓冲不仅仅是两份存储，它是一个概念，双缓冲是一条链路，不是某一个环节，是整个系统采用的一个机制，需要各个环节的支持，从APP到SurfaceFlinger、到图像显示都要参与协作。**

* 为什么VSYNC：刷新频率同步与不同步
* 为什么双缓冲
* 为什么三缓冲
 
 
* 每次屏幕刷新，SurfaceFlinger都要重新合成，所以，无论何种时间，Surface必须要为SF保留一个用于显示的Buffer 
* 16ms根据内存内容刷新一次屏幕，点亮一屏幕led，等下一个vsync到来，再点亮一次，data与屏幕是分离的的
* 使用双缓冲是因为：一个存储区不适合同时写跟读，可能用更新一半的时候就被用了
 
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