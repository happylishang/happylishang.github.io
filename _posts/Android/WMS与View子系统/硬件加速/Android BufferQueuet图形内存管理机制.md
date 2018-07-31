


	
	
	// 更新Layer
	void DeferredLayerUpdater::doUpdateTexImage() {
	    if (mSurfaceTexture->updateTexImage() == NO_ERROR) {
	        float transform[16];
	
	        int64_t frameNumber = mSurfaceTexture->getFrameNumber();
	        // If the GLConsumer queue is in synchronous mode, need to discard all
	        // but latest frame, using the frame number to tell when we no longer
	        // have newer frames to target. Since we can't tell which mode it is in,
	        // do this unconditionally.
	
	
	        int dropCounter = 0;
	        while (mSurfaceTexture->updateTexImage() == NO_ERROR) {
	            int64_t newFrameNumber = mSurfaceTexture->getFrameNumber();
	            // 
	            if (newFrameNumber == frameNumber) break;
	            frameNumber = newFrameNumber;
	            dropCounter++;
	        }
	
	        bool forceFilter = false;
	        // 获取
	        sp<GraphicBuffer> buffer = mSurfaceTexture->getCurrentBuffer();
	        if (buffer != nullptr) {
	            // force filtration if buffer size != layer size
	            forceFilter = mWidth != static_cast<int>(buffer->getWidth())
	                    || mHeight != static_cast<int>(buffer->getHeight());
	        }
	
	        #if DEBUG_RENDERER
	        if (dropCounter > 0) {
	            RENDERER_LOGD("Dropped %d frames on texture layer update", dropCounter);
	        }
	        #endif
	        mSurfaceTexture->getTransformMatrix(transform);
	        GLenum renderTarget = mSurfaceTexture->getCurrentTextureTarget();
	
	        LOG_ALWAYS_FATAL_IF(renderTarget != GL_TEXTURE_2D && renderTarget != GL_TEXTURE_EXTERNAL_OES,
	                "doUpdateTexImage target %x, 2d %x, EXT %x",
	                renderTarget, GL_TEXTURE_2D, GL_TEXTURE_EXTERNAL_OES);
	        LayerRenderer::updateTextureLayer(mLayer, mWidth, mHeight,
	                !mBlend, forceFilter, renderTarget, transform);
	    }
	}
	
GLConsumer如何获取当前buffer，最新的吗？		
		
		sp<GraphicBuffer> GLConsumer::getCurrentBuffer() const {
    Mutex::Autolock lock(mMutex);
    return (mCurrentTextureImage == NULL) ?
            NULL : mCurrentTextureImage->graphicBuffer();
}



status_t GLConsumer::updateTexImage() {
    ATRACE_CALL();
    GLC_LOGV("updateTexImage");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        GLC_LOGE("updateTexImage: GLConsumer is abandoned!");
        return NO_INIT;
    }

    // Make sure the EGL state is the same as in previous calls.
    status_t err = checkAndUpdateEglStateLocked();
    if (err != NO_ERROR) {
        return err;
    }

    BufferItem item;

    // Acquire the next buffer.
    // In asynchronous mode the list is guaranteed to be one buffer
    // deep, while in synchronous mode we use the oldest buffer.
    err = acquireBufferLocked(&item, 0);
    if (err != NO_ERROR) {
        if (err == BufferQueue::NO_BUFFER_AVAILABLE) {
            // We always bind the texture even if we don't update its contents.
            GLC_LOGV("updateTexImage: no buffers were available");
            //glBindTexture 渲染的 Texture
            glBindTexture(mTexTarget, mTexName);
            err = NO_ERROR;
        } else {
            GLC_LOGE("updateTexImage: acquire failed: %s (%d)",
                strerror(-err), err);
        }
        return err;
    }

    // Release the previous buffer.
    err = updateAndReleaseLocked(item);
    if (err != NO_ERROR) {
        // We always bind the texture.
        glBindTexture(mTexTarget, mTexName);
        return err;
    }

    // Bind the new buffer to the GL texture, and wait until it's ready.
    return bindTextureImageLocked();
}



status_t GLConsumer::updateAndReleaseLocked(const BufferItem& item)
{
    status_t err = NO_ERROR;

    int buf = item.mBuf;

    if (!mAttached) {
        GLC_LOGE("updateAndRelease: GLConsumer is not attached to an OpenGL "
                "ES context");
        releaseBufferLocked(buf, mSlots[buf].mGraphicBuffer,
                mEglDisplay, EGL_NO_SYNC_KHR);
        return INVALID_OPERATION;
    }

    // Confirm state.
    err = checkAndUpdateEglStateLocked();
    if (err != NO_ERROR) {
        releaseBufferLocked(buf, mSlots[buf].mGraphicBuffer,
                mEglDisplay, EGL_NO_SYNC_KHR);
        return err;
    }

    // Ensure we have a valid EglImageKHR for the slot, creating an EglImage
    // if nessessary, for the gralloc buffer currently in the slot in
    // ConsumerBase.
    // We may have to do this even when item.mGraphicBuffer == NULL (which
    // means the buffer was previously acquired).
    err = mEglSlots[buf].mEglImage->createIfNeeded(mEglDisplay, item.mCrop);
    if (err != NO_ERROR) {
        GLC_LOGW("updateAndRelease: unable to createImage on display=%p slot=%d",
                mEglDisplay, buf);
        releaseBufferLocked(buf, mSlots[buf].mGraphicBuffer,
                mEglDisplay, EGL_NO_SYNC_KHR);
        return UNKNOWN_ERROR;
    }

    // Do whatever sync ops we need to do before releasing the old slot.
    err = syncForReleaseLocked(mEglDisplay);
    if (err != NO_ERROR) {
        // Release the buffer we just acquired.  It's not safe to
        // release the old buffer, so instead we just drop the new frame.
        // As we are still under lock since acquireBuffer, it is safe to
        // release by slot.
        releaseBufferLocked(buf, mSlots[buf].mGraphicBuffer,
                mEglDisplay, EGL_NO_SYNC_KHR);
        return err;
    }

    GLC_LOGV("updateAndRelease: (slot=%d buf=%p) -> (slot=%d buf=%p)",
            mCurrentTexture, mCurrentTextureImage != NULL ?
                    mCurrentTextureImage->graphicBufferHandle() : 0,
            buf, mSlots[buf].mGraphicBuffer->handle);

    // release old buffer
    if (mCurrentTexture != BufferQueue::INVALID_BUFFER_SLOT) {
        status_t status = releaseBufferLocked(
                mCurrentTexture, mCurrentTextureImage->graphicBuffer(),
                mEglDisplay, mEglSlots[mCurrentTexture].mEglFence);
        if (status < NO_ERROR) {
            GLC_LOGE("updateAndRelease: failed to release buffer: %s (%d)",
                   strerror(-status), status);
            err = status;
            // keep going, with error raised [?]
        }
    }

    // Update the GLConsumer state.
    mCurrentTexture = buf;
    mCurrentTextureImage = mEglSlots[buf].mEglImage;
    mCurrentCrop = item.mCrop;
    mCurrentTransform = item.mTransform;
    mCurrentScalingMode = item.mScalingMode;
    mCurrentTimestamp = item.mTimestamp;
    mCurrentFence = item.mFence;
    mCurrentFrameNumber = item.mFrameNumber;

    computeCurrentTransformMatrixLocked();

    return err;
}


EglSlot


GLConsumer::EglImage::EglImage(sp<GraphicBuffer> graphicBuffer) :
    mGraphicBuffer(graphicBuffer),
    mEglImage(EGL_NO_IMAGE_KHR),
    mEglDisplay(EGL_NO_DISPLAY) {
}


EGLImageKHR GLConsumer::EglImage::createImage(EGLDisplay dpy,
        const sp<GraphicBuffer>& graphicBuffer, const Rect& crop) {
    EGLClientBuffer cbuf =
            static_cast<EGLClientBuffer>(graphicBuffer->getNativeBuffer());
    EGLint attrs[] = {
        EGL_IMAGE_PRESERVED_KHR,        EGL_TRUE,
        EGL_IMAGE_CROP_LEFT_ANDROID,    crop.left,
        EGL_IMAGE_CROP_TOP_ANDROID,     crop.top,
        EGL_IMAGE_CROP_RIGHT_ANDROID,   crop.right,
        EGL_IMAGE_CROP_BOTTOM_ANDROID,  crop.bottom,
        EGL_NONE,
    };
    if (!crop.isValid()) {
        // No crop rect to set, so terminate the attrib array before the crop.
        attrs[2] = EGL_NONE;
    } else if (!isEglImageCroppable(crop)) {
        // The crop rect is not at the origin, so we can't set the crop on the
        // EGLImage because that's not allowed by the EGL_ANDROID_image_crop
        // extension.  In the future we can add a layered extension that
        // removes this restriction if there is hardware that can support it.
        attrs[2] = EGL_NONE;
    }
    eglInitialize(dpy, 0, 0);
    EGLImageKHR image = eglCreateImageKHR(dpy, EGL_NO_CONTEXT,
            EGL_NATIVE_BUFFER_ANDROID, cbuf, attrs);
    if (image == EGL_NO_IMAGE_KHR) {
        EGLint error = eglGetError();
        ALOGE("error creating EGLImage: %#x", error);
        eglTerminate(dpy);
    }
    return image;
}


最终返回  native_buffer
	
	
	EGLImageKHR eglCreateImageKHR(EGLDisplay dpy, EGLContext ctx, EGLenum target,
	        EGLClientBuffer buffer, const EGLint* /*attrib_list*/)
	{
	    if (egl_display_t::is_valid(dpy) == EGL_FALSE) {
	        return setError(EGL_BAD_DISPLAY, EGL_NO_IMAGE_KHR);
	    }
	    if (ctx != EGL_NO_CONTEXT) {
	        return setError(EGL_BAD_CONTEXT, EGL_NO_IMAGE_KHR);
	    }
	    if (target != EGL_NATIVE_BUFFER_ANDROID) {
	        return setError(EGL_BAD_PARAMETER, EGL_NO_IMAGE_KHR);
	    }
	
	    ANativeWindowBuffer* native_buffer = (ANativeWindowBuffer*)buffer;
	
	    if (native_buffer->common.magic != ANDROID_NATIVE_BUFFER_MAGIC)
	        return setError(EGL_BAD_PARAMETER, EGL_NO_IMAGE_KHR);
	
	    if (native_buffer->common.version != sizeof(ANativeWindowBuffer))
	        return setError(EGL_BAD_PARAMETER, EGL_NO_IMAGE_KHR);
	
	    switch (native_buffer->format) {
	        case HAL_PIXEL_FORMAT_RGBA_8888:
	        case HAL_PIXEL_FORMAT_RGBX_8888:
	        case HAL_PIXEL_FORMAT_RGB_888:
	        case HAL_PIXEL_FORMAT_RGB_565:
	        case HAL_PIXEL_FORMAT_BGRA_8888:
	            break;
	        default:
	            return setError(EGL_BAD_PARAMETER, EGL_NO_IMAGE_KHR);
	    }
	
	    native_buffer->common.incRef(&native_buffer->common);
	    return (EGLImageKHR)native_buffer;
	}

可以说，是直接返回了一块内存，并没有怎么封装的引用

typedef void *EGLImageKHR; 

全你麻痹 typedef void *EGLImageKHR;



# CPU跟GPU交互

着色器程序在GPU上执行，OpenGL主程序在CPU上执行，主程序（CPU）向显存输入顶点等数据，启动渲染过程，并对渲染过程进行控制。了解到这一点就可以了解显示列表（Display Lists）以及像 glFinish() 这种函数存在的原因了，前者（显示列表）将一组绘制指令放到GPU上，CPU只要发一条“执行这个显示列表”这些指令就执行，而不必CPU每次渲染都发送大量指令到GPU，从而节约PCI带宽（因为PCI总线比显存慢）；后者（glFinish）让CPU等待GPU将已发送的渲染指令执行完。



# 为什么TetureView比SurfaceView占用内存

# SurfaceView的硬件加速跟软件绘制

视频播放应该是数据直接填充到SurfaceView的那块内存

# Surface的内存分配与数据流 还是只看6.0

Surface都是归SF管理，所有的分配最后都会走到SF，一个Surface有一个BufferQueue，一个Queue有多个slot，    

	BufferQueueDefs::SlotsType mSlots;

producer跟consumer都会映射这个slots，一个surface有一块内存，这块内存有很多歌slot 32 或者64 

    
不过SurfaceView传说的前后双缓冲是怎么回事？    不同的版本不同，看6.0跟8.0差别很大，只看6.0

>surface.cpp中的slots

    // mSlots stores the buffers that have been allocated for each buffer slot.
    // It is initialized to null pointers, and gets filled in with the result of
    // IGraphicBufferProducer::requestBuffer when the client dequeues a buffer from a
    // slot that has not yet been used. The buffer allocated to a slot will also
    // be replaced if the requested buffer usage or geometry differs from that
    // of the buffer allocated to a slot.
    
    BufferSlot mSlots[NUM_BUFFER_SLOTS];

>BufferQueueCore.cpp中的slots

    // mSlots is an array of buffer slots that must be mirrored on the producer
    // side. This allows buffer ownership to be transferred between the producer
    // and consumer without sending a GraphicBuffer over Binder. The entire
    // array is initialized to NULL at construction time, and buffers are
    // allocated for a slot when requestBuffer is called with that slot's index.

    BufferQueueDefs::SlotsType mSlots;
    
        namespace BufferQueueDefs {
        // BufferQueue will keep track of at most this value of buffers.
        // Attempts at runtime to increase the number of buffers past this
        // will fail.
        enum { NUM_BUFFER_SLOTS = 64 };
        typedef BufferSlot SlotsType[NUM_BUFFER_SLOTS];
    } // namespace BufferQueueDefs
    
    
![31530101142_.pic.jpg](https://upload-images.jianshu.io/upload_images/1460468-617b3362ee32a84a.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

一个Surface对应内存块最多64块内存，如何管理，如何申请一个slot

	status_t BufferQueueProducer::dequeueBuffer(int *outSlot,
	        sp<android::Fence> *outFence, bool async,
	        uint32_t width, uint32_t height, PixelFormat format, uint32_t usage) {
 
	    
	    status_t returnFlags = NO_ERROR;
	    EGLDisplay eglDisplay = EGL_NO_DISPLAY;
	    EGLSyncKHR eglFence = EGL_NO_SYNC_KHR;
	    bool attachedByConsumer = false;
	    { 
	    
	    	// 保护
	        Mutex::Autolock lock(mCore->mMutex);
	        mCore->waitWhileAllocatingLocked();
		        if (format == 0) {
	            format = mCore->mDefaultBufferFormat;
	        }
	        // Enable the usage bits the consumer requested
	        usage |= mCore->mConsumerUsageBits;
	        
			<!--是否需要使用默认尺寸-->
	        const bool useDefaultSize = !width && !height;
	        if (useDefaultSize) {
	            width = mCore->mDefaultWidth;
	            height = mCore->mDefaultHeight;
	        }
			<!--找SLOT-->
	        int found = BufferItem::INVALID_BUFFER_SLOT;
	        while (found == BufferItem::INVALID_BUFFER_SLOT) {
	            status_t status = waitForFreeSlotThenRelock("dequeueBuffer", async,
	                    &found, &returnFlags);
	            if (status != NO_ERROR) {
	                return status;
	            }
	
	            // This should not happen
	            if (found == BufferQueueCore::INVALID_BUFFER_SLOT) {
	                BQ_LOGE("dequeueBuffer: no available buffer slots");
	                return -EBUSY;
	            }
	
	            const sp<GraphicBuffer>& buffer(mSlots[found].mGraphicBuffer);
	
	            // If we are not allowed to allocate new buffers,
	            // waitForFreeSlotThenRelock must have returned a slot containing a
	            // buffer. If this buffer would require reallocation to meet the
	            // requested attributes, we free it and attempt to get another one.
	            if (!mCore->mAllowAllocation) {
	                if (buffer->needsReallocation(width, height, format, usage)) {
	                    mCore->freeBufferLocked(found);
	                    found = BufferItem::INVALID_BUFFER_SLOT;
	                    continue;
	                }
	            }
	        }
	
	        *outSlot = found;
	        ATRACE_BUFFER_INDEX(found);
	
	        attachedByConsumer = mSlots[found].mAttachedByConsumer;
	
	        mSlots[found].mBufferState = BufferSlot::DEQUEUED;
	
	        const sp<GraphicBuffer>& buffer(mSlots[found].mGraphicBuffer);
	        if ((buffer == NULL) ||
	                buffer->needsReallocation(width, height, format, usage))
	        {
	            mSlots[found].mAcquireCalled = false;
	            mSlots[found].mGraphicBuffer = NULL;
	            mSlots[found].mRequestBufferCalled = false;
	            mSlots[found].mEglDisplay = EGL_NO_DISPLAY;
	            mSlots[found].mEglFence = EGL_NO_SYNC_KHR;
	            mSlots[found].mFence = Fence::NO_FENCE;
	            mCore->mBufferAge = 0;
	
	            returnFlags |= BUFFER_NEEDS_REALLOCATION;
	        } else {
	            // We add 1 because that will be the frame number when this buffer
	            // is queued
	            mCore->mBufferAge =
	                    mCore->mFrameCounter + 1 - mSlots[found].mFrameNumber;
	        }
	
	        BQ_LOGV("dequeueBuffer: setting buffer age to %" PRIu64,
	                mCore->mBufferAge);
	
	        if (CC_UNLIKELY(mSlots[found].mFence == NULL)) {
	            BQ_LOGE("dequeueBuffer: about to return a NULL fence - "
	                    "slot=%d w=%d h=%d format=%u",
	                    found, buffer->width, buffer->height, buffer->format);
	        }
	
	        eglDisplay = mSlots[found].mEglDisplay;
	        eglFence = mSlots[found].mEglFence;
	        *outFence = mSlots[found].mFence;
	        mSlots[found].mEglFence = EGL_NO_SYNC_KHR;
	        mSlots[found].mFence = Fence::NO_FENCE;
	
	        mCore->validateConsistencyLocked();
	    } // Autolock scope
	
	    if (returnFlags & BUFFER_NEEDS_REALLOCATION) {
	        status_t error;
	        BQ_LOGV("dequeueBuffer: allocating a new buffer for slot %d", *outSlot);
	        sp<GraphicBuffer> graphicBuffer(mCore->mAllocator->createGraphicBuffer(
	                width, height, format, usage, &error));
	        if (graphicBuffer == NULL) {
	            BQ_LOGE("dequeueBuffer: createGraphicBuffer failed");
	            return error;
	        }
	
	        { // Autolock scope
	            Mutex::Autolock lock(mCore->mMutex);
	
	            if (mCore->mIsAbandoned) {
	                BQ_LOGE("dequeueBuffer: BufferQueue has been abandoned");
	                return NO_INIT;
	            }
	
	            graphicBuffer->setGenerationNumber(mCore->mGenerationNumber);
	            mSlots[*outSlot].mGraphicBuffer = graphicBuffer;
	        } // Autolock scope
	    }
	
	    if (attachedByConsumer) {
	        returnFlags |= BUFFER_NEEDS_REALLOCATION;
	    }
	
	    if (eglFence != EGL_NO_SYNC_KHR) {
	        EGLint result = eglClientWaitSyncKHR(eglDisplay, eglFence, 0,
	                1000000000);
	        // If something goes wrong, log the error, but return the buffer without
	        // synchronizing access to it. It's too late at this point to abort the
	        // dequeue operation.
	        if (result == EGL_FALSE) {
	            BQ_LOGE("dequeueBuffer: error %#x waiting for fence",
	                    eglGetError());
	        } else if (result == EGL_TIMEOUT_EXPIRED_KHR) {
	            BQ_LOGE("dequeueBuffer: timeout waiting for fence");
	        }
	        eglDestroySyncKHR(eglDisplay, eglFence);
	    }
	
	    BQ_LOGV("dequeueBuffer: returning slot=%d/%" PRIu64 " buf=%p flags=%#x",
	            *outSlot,
	            mSlots[*outSlot].mFrameNumber,
	            mSlots[*outSlot].mGraphicBuffer->handle, returnFlags);
	
	    return returnFlags;
	}



	
	status_t BufferQueueProducer::waitForFreeSlotThenRelock(const char* caller,
	        bool async, int* found, status_t* returnFlags) const {
	    bool tryAgain = true;
	    while (tryAgain) {
	        if (mCore->mIsAbandoned) {
	            BQ_LOGE("%s: BufferQueue has been abandoned", caller);
	            return NO_INIT;
	        }
	
	        const int maxBufferCount = mCore->getMaxBufferCountLocked(async);
	        if (async && mCore->mOverrideMaxBufferCount) {
	            // FIXME: Some drivers are manually setting the buffer count
	            // (which they shouldn't), so we do this extra test here to
	            // handle that case. This is TEMPORARY until we get this fixed.
	            if (mCore->mOverrideMaxBufferCount < maxBufferCount) {
	                BQ_LOGE("%s: async mode is invalid with buffer count override",
	                        caller);
	                return BAD_VALUE;
	            }
	        }
	
	        // Free up any buffers that are in slots beyond the max buffer count
	        for (int s = maxBufferCount; s < BufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
	            assert(mSlots[s].mBufferState == BufferSlot::FREE);
	            if (mSlots[s].mGraphicBuffer != NULL) {
	                mCore->freeBufferLocked(s);
	                *returnFlags |= RELEASE_ALL_BUFFERS;
	            }
	        }
	
	        int dequeuedCount = 0;
	        int acquiredCount = 0;
	        for (int s = 0; s < maxBufferCount; ++s) {
	            switch (mSlots[s].mBufferState) {
	                case BufferSlot::DEQUEUED:
	                    ++dequeuedCount;
	                    break;
	                case BufferSlot::ACQUIRED:
	                    ++acquiredCount;
	                    break;
	                default:
	                    break;
	            }
	        }
	
	        // Producers are not allowed to dequeue more than one buffer if they
	        // did not set a buffer count
	        if (!mCore->mOverrideMaxBufferCount && dequeuedCount) {
	            BQ_LOGE("%s: can't dequeue multiple buffers without setting the "
	                    "buffer count", caller);
	            return INVALID_OPERATION;
	        }
	
	        // See whether a buffer has been queued since the last
	        // setBufferCount so we know whether to perform the min undequeued
	        // buffers check below
	        if (mCore->mBufferHasBeenQueued) {
	            // Make sure the producer is not trying to dequeue more buffers
	            // than allowed
	            const int newUndequeuedCount =
	                maxBufferCount - (dequeuedCount + 1);
	            const int minUndequeuedCount =
	                mCore->getMinUndequeuedBufferCountLocked(async);
	            if (newUndequeuedCount < minUndequeuedCount) {
	                BQ_LOGE("%s: min undequeued buffer count (%d) exceeded "
	                        "(dequeued=%d undequeued=%d)",
	                        caller, minUndequeuedCount,
	                        dequeuedCount, newUndequeuedCount);
	                return INVALID_OPERATION;
	            }
	        }
	
	        *found = BufferQueueCore::INVALID_BUFFER_SLOT;
	
	        // If we disconnect and reconnect quickly, we can be in a state where
	        // our slots are empty but we have many buffers in the queue. This can
	        // cause us to run out of memory if we outrun the consumer. Wait here if
	        // it looks like we have too many buffers queued up.
	        bool tooManyBuffers = mCore->mQueue.size()
	                            > static_cast<size_t>(maxBufferCount);
	        if (tooManyBuffers) {
	            BQ_LOGV("%s: queue size is %zu, waiting", caller,
	                    mCore->mQueue.size());
	        } else {
	            if (!mCore->mFreeBuffers.empty()) {
	                auto slot = mCore->mFreeBuffers.begin();
	                *found = *slot;
	                mCore->mFreeBuffers.erase(slot);
	            } else if (mCore->mAllowAllocation && !mCore->mFreeSlots.empty()) {
	                auto slot = mCore->mFreeSlots.begin();
	                // Only return free slots up to the max buffer count
	                if (*slot < maxBufferCount) {
	                    *found = *slot;
	                    mCore->mFreeSlots.erase(slot);
	                }
	            }
	        }
	
	        // If no buffer is found, or if the queue has too many buffers
	        // outstanding, wait for a buffer to be acquired or released, or for the
	        // max buffer count to change.
	        tryAgain = (*found == BufferQueueCore::INVALID_BUFFER_SLOT) ||
	                   tooManyBuffers;
	        if (tryAgain) {
	            // Return an error if we're in non-blocking mode (producer and
	            // consumer are controlled by the application).
	            // However, the consumer is allowed to briefly acquire an extra
	            // buffer (which could cause us to have to wait here), which is
	            // okay, since it is only used to implement an atomic acquire +
	            // release (e.g., in GLConsumer::updateTexImage())
	            if (mCore->mDequeueBufferCannotBlock &&
	                    (acquiredCount <= mCore->mMaxAcquiredBufferCount)) {
	                return WOULD_BLOCK;
	            }
	            mCore->mDequeueCondition.wait(mCore->mMutex);
	        }
	    } // while (tryAgain)
	
	    return NO_ERROR;
	}

# BufferSlot跟mGraphicBuffer的关系

    BufferSlot()
    : mEglDisplay(EGL_NO_DISPLAY),
      mBufferState(BufferSlot::FREE),
      mRequestBufferCalled(false),
      mFrameNumber(0),
      mEglFence(EGL_NO_SYNC_KHR),
      mAcquireCalled(false),
      mNeedsCleanupOnRelease(false),
      mAttachedByConsumer(false) {
    }

    // mGraphicBuffer points to the buffer allocated for this slot or is NULL
    // if no buffer has been allocated.
    sp<GraphicBuffer> mGraphicBuffer;
    
    Graphics是哪块内存，算是本APP所处理的内存吗？但是它是native的内存吧，并且，好像不算到当前App中，不会导致OOM，除非系统内存不足，

    

# 为什么TetureView比SurfaceView占用内存

拿两个播放视频来对比下：CPU跟内存使用

>CPU对比

![cpu使用对比.png](https://upload-images.jianshu.io/upload_images/1460468-8f398182e3e1cddb.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

>内存使用对比

![内存使用对比.png](https://upload-images.jianshu.io/upload_images/1460468-adb477885b1c6814.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

TextureView播放视频同样需要Surface，在SurfaceTextureAvailable的时候，需要用SurfaceTexture创建Surface，之后再使用这个Surface：


    @Override
    public void onSurfaceTextureAvailable(SurfaceTexture surfaceTexture, int width, int height) {
        if (mSurfaceTexture == null) {
            mSurfaceTexture = surfaceTexture;
            mSurface = new Surface(surfaceTexture);
            //   这里是设置数据的输出流吗？
            mMediaPlayer.setSurface(mSurface);
            if (mTargetState == PlayState.PLAYING) {
                start();
            }
        } else {
            mTextureView.setSurfaceTexture(mSurfaceTexture);
        }
    }

那么究竟如何新建的呢new Surface(surfaceTexture)

    public Surface(SurfaceTexture surfaceTexture) {
        if (surfaceTexture == null) {
            throw new IllegalArgumentException("surfaceTexture must not be null");
        }
        mIsSingleBuffered = surfaceTexture.isSingleBuffered();
        synchronized (mLock) {
            mName = surfaceTexture.toString();
            setNativeObjectLocked(nativeCreateFromSurfaceTexture(surfaceTexture));
        }
    }
    
会调用native

	static jlong nativeCreateFromSurfaceTexture(JNIEnv* env, jclass clazz,
	        jobject surfaceTextureObj) {
	     
	     <!--获取SurfaceTexture中已经创建的GraphicBufferProducer-->
	    sp<IGraphicBufferProducer> producer(SurfaceTexture_getProducer(env, surfaceTextureObj));
 		 
	   <!--根据producer直接创建Surface，其实Surface只是为了表示数据从哪来，由谁填充，其实数据是由MediaPlayer填充的，只是这里的Surface不是归属SurfaceFlinger管理，SurfaceFlinger感知不到-->
	   <!--关键点2 -->
	    sp<Surface> surface(new Surface(producer, true));
	    surface->incStrong(&sRefBaseOwner);
	    return jlong(surface.get());
	}

SurfaceView跟TexutureView在使用Surface的时候，SurfaceView的Surface的Consumer是SurfaceFlinger（BnGraphicBufferProducer是在SF中创建的），但是TexutureView中SurfaceView的consumer却是TexutureView（BnGraphicBufferProducer是在APP中创建的），所以数据必须再由TexutureView处理后，给SF才可以，这也是TextureView效率低的原因。 

# SurfaceView的硬件加速跟软件绘制

视频播放应该是数据直接填充到SurfaceView的那块内存

# Surface的内存分配与数据流

Surface都是归SF管理，所有的分配最后都会走到SF，一个Surface有一个BufferQueue，一个Queue有多个slot，    

	BufferQueueDefs::SlotsType mSlots;

producer跟consumer都会映射这个slots，一个surface有一块内存，这块内存有很多歌slot 32 或者64 

    // mSlots is an array of buffer slots that must be mirrored on the producer
    // side. This allows buffer ownership to be transferred between the producer
    // and consumer without sending a GraphicBuffer over Binder. The entire
    // array is initialized to NULL at construction time, and buffers are
    // allocated for a slot when requestBuffer is called with that slot's index.
    BufferQueueDefs::SlotsType mSlots;
    
不过SurfaceView传说的前后双缓冲是怎么回事？    

# SurfaceView如何支持视频播放，到底有几块缓存back front？

    // must be used from the lock/unlock thread
    
    // 之类的GraphicBuffer很明显不止一块
    sp<GraphicBuffer>           mLockedBuffer;
    sp<GraphicBuffer>           mPostedBuffer;
    
 同一时刻，有几块内存生效呢？ 

# OpenGl绘制后，仍然需要入队swapBuffers，是通知绘制或者合成的关键，

TextureView会触发重绘（硬件加速），并通知SF合成，但是SurfaceView会直接通知SF合成，
	
	EGLBooleanegl_window_surface_v2_t::swapBuffers()
	 
	{
	 
	    //………….
	 
	    nativeWindow->queueBuffer(nativeWindow,buffer, -1);
	 
	 
	 
	    // dequeue a new buffer
	 
	    if (nativeWindow->dequeueBuffer（nativeWindow, &buffer, &fenceFd)== NO_ERROR) {
	 
	        sp<Fence> fence(new Fence(fenceFd));
	 
	        if(fence->wait(Fence::TIMEOUT_NEVER)) {
	 
	           nativeWindow->cancelBuffer(nativeWindow, buffer, fenceFd);
	 
	            return setError(EGL_BAD_ALLOC,EGL_FALSE);
	 
	        }
	 
	//。。。。。。

消费方会获得通知
	
	status_t BufferQueueProducer::queueBuffer(int slot,
	        const QueueBufferInput &input, QueueBufferOutput *output) {
	    ATRACE_CALL();
	    ATRACE_BUFFER_INDEX(slot);
	
	    int64_t timestamp;
	    bool isAutoTimestamp;
	    android_dataspace dataSpace;
	    Rect crop;
	    int scalingMode;
	    uint32_t transform;
	    uint32_t stickyTransform;
	    bool async;
	    sp<Fence> fence;
	    input.deflate(&timestamp, &isAutoTimestamp, &dataSpace, &crop, &scalingMode,
	            &transform, &async, &fence, &stickyTransform);
	    Region surfaceDamage = input.getSurfaceDamage();
	
	    if (fence == NULL) {
	        BQ_LOGE("queueBuffer: fence is NULL");
	        return BAD_VALUE;
	    }
	
	    switch (scalingMode) {
	        case NATIVE_WINDOW_SCALING_MODE_FREEZE:
	        case NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW:
	        case NATIVE_WINDOW_SCALING_MODE_SCALE_CROP:
	        case NATIVE_WINDOW_SCALING_MODE_NO_SCALE_CROP:
	            break;
	        default:
	            BQ_LOGE("queueBuffer: unknown scaling mode %d", scalingMode);
	            return BAD_VALUE;
	    }
	
		// queue之后就会通知就IConsumerListener
	    sp<IConsumerListener> frameAvailableListener;
	    sp<IConsumerListener> frameReplacedListener;
	    int callbackTicket = 0;
	    BufferItem item;
	    { // Autolock scope
	        Mutex::Autolock lock(mCore->mMutex);
	
	        if (mCore->mIsAbandoned) {
	            BQ_LOGE("queueBuffer: BufferQueue has been abandoned");
	            return NO_INIT;
	        }
	
	        const int maxBufferCount = mCore->getMaxBufferCountLocked(async);
	        if (async && mCore->mOverrideMaxBufferCount) {
	            // FIXME: Some drivers are manually setting the buffer count
	            // (which they shouldn't), so we do this extra test here to
	            // handle that case. This is TEMPORARY until we get this fixed.
	            if (mCore->mOverrideMaxBufferCount < maxBufferCount) {
	                BQ_LOGE("queueBuffer: async mode is invalid with "
	                        "buffer count override");
	                return BAD_VALUE;
	            }
	        }
	
	        if (slot < 0 || slot >= maxBufferCount) {
	            BQ_LOGE("queueBuffer: slot index %d out of range [0, %d)",
	                    slot, maxBufferCount);
	            return BAD_VALUE;
	        } else if (mSlots[slot].mBufferState != BufferSlot::DEQUEUED) {
	            BQ_LOGE("queueBuffer: slot %d is not owned by the producer "
	                    "(state = %d)", slot, mSlots[slot].mBufferState);
	            return BAD_VALUE;
	        } else if (!mSlots[slot].mRequestBufferCalled) {
	            BQ_LOGE("queueBuffer: slot %d was queued without requesting "
	                    "a buffer", slot);
	            return BAD_VALUE;
	        }
	
	        BQ_LOGV("queueBuffer: slot=%d/%" PRIu64 " time=%" PRIu64 " dataSpace=%d"
	                " crop=[%d,%d,%d,%d] transform=%#x scale=%s",
	                slot, mCore->mFrameCounter + 1, timestamp, dataSpace,
	                crop.left, crop.top, crop.right, crop.bottom, transform,
	                BufferItem::scalingModeName(static_cast<uint32_t>(scalingMode)));
	
	        const sp<GraphicBuffer>& graphicBuffer(mSlots[slot].mGraphicBuffer);
	        Rect bufferRect(graphicBuffer->getWidth(), graphicBuffer->getHeight());
	        Rect croppedRect;
	        crop.intersect(bufferRect, &croppedRect);
	        if (croppedRect != crop) {
	            BQ_LOGE("queueBuffer: crop rect is not contained within the "
	                    "buffer in slot %d", slot);
	            return BAD_VALUE;
	        }
	
	        // Override UNKNOWN dataspace with consumer default
	        if (dataSpace == HAL_DATASPACE_UNKNOWN) {
	            dataSpace = mCore->mDefaultBufferDataSpace;
	        }
	
	        mSlots[slot].mFence = fence;
	        mSlots[slot].mBufferState = BufferSlot::QUEUED;
	        ++mCore->mFrameCounter;
	        mSlots[slot].mFrameNumber = mCore->mFrameCounter;
	
	        item.mAcquireCalled = mSlots[slot].mAcquireCalled;
	        item.mGraphicBuffer = mSlots[slot].mGraphicBuffer;
	        item.mCrop = crop;
	        item.mTransform = transform &
	                ~static_cast<uint32_t>(NATIVE_WINDOW_TRANSFORM_INVERSE_DISPLAY);
	        item.mTransformToDisplayInverse =
	                (transform & NATIVE_WINDOW_TRANSFORM_INVERSE_DISPLAY) != 0;
	        item.mScalingMode = static_cast<uint32_t>(scalingMode);
	        item.mTimestamp = timestamp;
	        item.mIsAutoTimestamp = isAutoTimestamp;
	        item.mDataSpace = dataSpace;
	        item.mFrameNumber = mCore->mFrameCounter;
	        item.mSlot = slot;
	        item.mFence = fence;
	        item.mIsDroppable = mCore->mDequeueBufferCannotBlock || async;
	        item.mSurfaceDamage = surfaceDamage;
	
	        mStickyTransform = stickyTransform;
	
	        if (mCore->mQueue.empty()) {
	            // When the queue is empty, we can ignore mDequeueBufferCannotBlock
	            // and simply queue this buffer
	            mCore->mQueue.push_back(item);
	            frameAvailableListener = mCore->mConsumerListener;
	        } else {
	            // When the queue is not empty, we need to look at the front buffer
	            // state to see if we need to replace it
	            BufferQueueCore::Fifo::iterator front(mCore->mQueue.begin());
	            if (front->mIsDroppable) {
	                // If the front queued buffer is still being tracked, we first
	                // mark it as freed
	                if (mCore->stillTracking(front)) {
	                    mSlots[front->mSlot].mBufferState = BufferSlot::FREE;
	                    mCore->mFreeBuffers.push_front(front->mSlot);
	                }
	                // Overwrite the droppable buffer with the incoming one
	                *front = item;
	                frameReplacedListener = mCore->mConsumerListener;
	            } else {
	                mCore->mQueue.push_back(item);
	                frameAvailableListener = mCore->mConsumerListener;
	            }
	        }
	
	        mCore->mBufferHasBeenQueued = true;
	        mCore->mDequeueCondition.broadcast();
	
	        output->inflate(mCore->mDefaultWidth, mCore->mDefaultHeight,
	                mCore->mTransformHint,
	                static_cast<uint32_t>(mCore->mQueue.size()));
	
	        ATRACE_INT(mCore->mConsumerName.string(), mCore->mQueue.size());
	
	        // Take a ticket for the callback functions
	        callbackTicket = mNextCallbackTicket++;
	
	        mCore->validateConsistencyLocked();
	    } // Autolock scope
	
	    // Wait without lock held
	    if (mCore->mConnectedApi == NATIVE_WINDOW_API_EGL) {
	        // Waiting here allows for two full buffers to be queued but not a
	        // third. In the event that frames take varying time, this makes a
	        // small trade-off in favor of latency rather than throughput.
	        mLastQueueBufferFence->waitForever("Throttling EGL Production");
	        mLastQueueBufferFence = fence;
	    }
	
	    // Don't send the GraphicBuffer through the callback, and don't send
	    // the slot number, since the consumer shouldn't need it
	    item.mGraphicBuffer.clear();
	    item.mSlot = BufferItem::INVALID_BUFFER_SLOT;
	
	    // Call back without the main BufferQueue lock held, but with the callback
	    // lock held so we can ensure that callbacks occur in order
	    {
	        Mutex::Autolock lock(mCallbackMutex);
	        while (callbackTicket != mCurrentCallbackTicket) {
	            mCallbackCondition.wait(mCallbackMutex);
	        }
	
	        if (frameAvailableListener != NULL) {
	            // 调用回调，就是这里看到了吧，就是这么强大
	            frameAvailableListener->onFrameAvailable(item);
	        } else if (frameReplacedListener != NULL) {
	            frameReplacedListener->onFrameReplaced(item);
	        }
	
	        ++mCurrentCallbackTicket;
	        mCallbackCondition.broadcast();
	    }
	
	    return NO_ERROR;
	}

TextureView收到通知后会重绘，并且这个时候已经拿到了数据，OpenGL重绘即可，比SurfaceView多一步，这部分的更新是直接到SF吗？按理说，SF那段没对应的Layer

    private void applyUpdate() {
        if (mLayer == null) {
            return;
        }

        synchronized (mLock) {
            if (mUpdateLayer) {
                mUpdateLayer = false;
            } else {
                return;
            }
        }

        // 更新Layer
        mLayer.prepare(getWidth(), getHeight(), mOpaque);
        mLayer.updateSurfaceTexture();

        if (mListener != null) {
            mListener.onSurfaceTextureUpdated(mSurface);
        }
    }
  
# 	consumer更新  
	  
	status_t GLConsumer::updateTexImage() {
	    ATRACE_CALL();
	    GLC_LOGV("updateTexImage");
	    Mutex::Autolock lock(mMutex);
	
	    if (mAbandoned) {
	        GLC_LOGE("updateTexImage: GLConsumer is abandoned!");
	        return NO_INIT;
	    }
	
	    // Make sure the EGL state is the same as in previous calls.
	    status_t err = checkAndUpdateEglStateLocked();
	    if (err != NO_ERROR) {
	        return err;
	    }
	
	    BufferItem item;
	
	    // Acquire the next buffer.
	    // In asynchronous mode the list is guaranteed to be one buffer
	    // deep, while in synchronous mode we use the oldest buffer.
	    err = acquireBufferLocked(&item, 0);
	    if (err != NO_ERROR) {
	        if (err == BufferQueue::NO_BUFFER_AVAILABLE) {
	            // We always bind the texture even if we don't update its contents.
	            GLC_LOGV("updateTexImage: no buffers were available");
	            glBindTexture(mTexTarget, mTexName);
	            err = NO_ERROR;
	        } else {
	            GLC_LOGE("updateTexImage: acquire failed: %s (%d)",
	                strerror(-err), err);
	        }
	        return err;
	    }
	
	    // Release the previous buffer.
	    err = updateAndReleaseLocked(item);
	    if (err != NO_ERROR) {
	        // We always bind the texture.
	        glBindTexture(mTexTarget, mTexName);
	        return err;
	    }
	
	    // Bind the new buffer to the GL texture, and wait until it's ready.
	    return bindTextureImageLocked();
	}

如何绑定Texuture
  
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
	            mCurrentTextureImage == NULL) {
	        GLC_LOGE("bindTextureImage: no currently-bound texture");
	        return NO_INIT;
	    }
	
	    status_t err = mCurrentTextureImage->createIfNeeded(mEglDisplay,
	                                                        mCurrentCrop);
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
	        status_t result = mCurrentTextureImage->createIfNeeded(mEglDisplay,
	                                                               mCurrentCrop,
	                                                               true);
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

# 为什么会跳过前几帧？

The image stream may come from either camera preview or video decode. A Surface created from a SurfaceTexture can be used as an output destination for the android.hardware.camera2, MediaCodec, MediaPlayer, and Allocation APIs. When updateTexImage() is called, the contents of the texture object specified when the SurfaceTexture was created are updated to contain the most recent image from the image stream. This may cause some frames of the stream to be skipped.

A SurfaceTexture may also be used in place of a SurfaceHolder when specifying the output destination of the older Camera API. Doing so will cause all the frames from the image stream to be sent to the SurfaceTexture object rather than to the device's display.


# TexutView跟SurfaceTexure，是可以先创建纹理，在绑定上去，

    public SurfaceTexture(int texName) {
        this(texName, false);
    }

使用SurfaceTexture实现滤镜的关键，就是要自己创建有id的textture，之后再处理，是直接输出到帧缓冲区，还是怎么处理，要看

# glGenFramebuffers

获取帧缓冲区，直接渲染到屏幕，

# Frame Buffer Object（FBO）

Frame Buffer Object（FBO）即为帧缓冲对象，用于离屏渲染缓冲。相对于其它同类技术，如数据拷贝或交换缓冲区等，使用FBO技术会更高效并且更容易实现。而且FBO不受窗口大小限制。FBO可以包含许多颜色缓冲区，可以同时从一个片元着色器写入。FBO是一个容器，自身不能用于渲染，需要与一些可渲染的缓冲区绑定在一起，像纹理或者渲染缓冲区。 
Render Buffer Object（RBO）即为渲染缓冲对象，分为color buffer(颜色)、depth buffer(深度)、stencil buffer(模板)。 
在使用FBO做离屏渲染时，可以只绑定纹理，也可以只绑定Render Buffer，也可以都绑定或者绑定多个，视使用场景而定。如只是对一个图像做变色处理等，只绑定纹理即可。如果需要往一个图像上增加3D的模型和贴纸，则一定还要绑定depth Render Buffer。 

# GLsurfaceView方便在它创建了OpenGL上下文

猜测还是走SF那一套，不过EglSurface创建后，都是从这个对应的控件申请内存，也就说其实还是SF那一套

# OpenGL的绘图内存如何获取的

EglSurface的概念，是不是所有的内容都会流入EglSurface，它跟Surface绑定

            mEglSurface = mEgl.eglCreateWindowSurface(mEglDisplay, mEglConfig, mSurface, null);

EglSurface其实就是映射到Surface，当EglSurface bindTexre的时候，其实就是将数据传递到Surface，所以也是直接传递。 

# Texture是纹理，纹理是一个集合，采样用的，本身不算到绘制内存中去

绘制的内容是从纹理中采样得到的，但是纹理本身不是绘制，纹理是模板，但是模板不是画。OpenGL是个标准的框架，按照里面走就行。





#     参考文档

[Android BufferQueue简析](https://www.jianshu.com/p/edd7d264be73)           
[不错的demo GraphicsTestBed ](https://github.com/lb377463323/GraphicsTestBed)        
[小窗播放视频的原理和实现（上）](http://www.10tiao.com/html/223/201712/2651232830/1.html)         
[GLTextureViewActivity.java ](https://android.googlesource.com/platform/frameworks/base/+/master/tests/HwAccelerationTest/src/com/android/test/hwui/GLTextureViewActivity.java)