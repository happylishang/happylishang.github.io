# Android中所谓的双缓冲到底是个什么东西



	void Layer::onFirstRef() {
	// Creates a custom BufferQueue for SurfaceFlingerConsumer to use
	sp<IGraphicBufferProducer> producer;
	sp<IGraphicBufferConsumer> consumer;
	BufferQueue::createBufferQueue(&producer, &consumer, true);
	mProducer = new MonitoredProducer(producer, mFlinger, this);
	mSurfaceFlingerConsumer = new SurfaceFlingerConsumer(consumer, mTextureName, this);
	mSurfaceFlingerConsumer->setConsumerUsageBits(getEffectiveUsage(0));
	mSurfaceFlingerConsumer->setContentsChangedListener(this);
	mSurfaceFlingerConsumer->setName(mName);
	
	// 如果禁止三倍缓冲，那就采用双缓冲
	if (mFlinger->isLayerTripleBufferingDisabled()) {
	    mProducer->setMaxDequeuedBufferCount(2);
	}
	
	const sp<const DisplayDevice> hw(mFlinger->getDefaultDisplayDevice());
	updateTransformHint(hw);
	}
	
 mProducer->setMaxDequeuedBufferCount(2);就是设置双缓冲，其实就是SurfaceFlinger能够为一个surface DequeuedBuffer的数量
	
	
	