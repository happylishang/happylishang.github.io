
对于Android MediaPlayer服务首先要有个概念在心里，MediaPlayer是一个服务，而且位于单独进程，看init.rc中的配置

	service media /system/bin/mediaserver
	    class main
	    user media
	    group audio camera inet net_bt net_bt_admin net_bw_acct drmrpc mediadrm
	    ioprio rt 4
    
也就说说，我们在用MediaPlayer的时候，其实是基于Binder通信向MediaPlayer发送请求，让MediaPlayer进程来为APP处理请求的。

	status_t MediaPlayer::setDataSource(
	        const char *url, const KeyedVector<String8, String8> *headers)
	{
	    status_t err = BAD_VALUE;
	    if (url != NULL) {
	        // 获取mediaPlayer服务
	        const sp<IMediaPlayerService>& service(getMediaPlayerService());
	        if (service != 0) {
	        <!--创建IMediaPlayer代理对象-->
	            sp<IMediaPlayer> player(service->create(this, mAudioSessionId)); 
	            ...
	            // 如何绑定player 
	            err = attachNewPlayer(player);
	        }
	    }
	    return err;
	}

看看如何获取

	sp<IMediaPlayer> MediaPlayerService::create(const sp<IMediaPlayerClient>& client,
	        int audioSessionId)
	{
	    pid_t pid = IPCThreadState::self()->getCallingPid();
	    int32_t connId = android_atomic_inc(&mNextConnId);
		    sp<Client> c = new Client(
	            this, pid, connId, client, audioSessionId,
	            IPCThreadState::self()->getCallingUid());
		    wp<Client> w = c;
	    {
	        Mutex::Autolock lock(mLock);
	        mClients.add(w);
	    }
	    return c;
	}

多个APP可以同时使用	MediaPlayerService，可以多个音频同时播放，所以，这里并不是直接用MediaPlayerService，而是生成各自的代理，这种做法在Android中很常见，比如SurfaceFlinger，WMS等。很容易想到 上面的Client是一个Binder服务对象

	 class Client : public BnMediaPlayer 

也就说APP端最终是通过IMediaPlayer向Client服务发送请求，而MediaPlayerService主要负责为APP端创建Client服务，直接看看Client如何播放多媒体资源的：


	status_t MediaPlayerService::Client::start()
	{
	    sp<MediaPlayerBase> p = getPlayer();
	    if (p == 0) return UNKNOWN_ERROR;
	    p->setLooping(mLoop);
	    return p->start();
	}

这里的getPlayer获得是之前创建的Player

	sp<MediaPlayerBase> MediaPlayerService::Client::setDataSource_pre(  player_type playerType)
	{
	    sp<MediaPlayerBase> p = createPlayer(playerType);
	    if (p == NULL) {
	        return p;
	    }
	    if (!p->hardwareOutput()) {
	        mAudioOutput = new AudioOutput(mAudioSessionId);
	        static_cast<MediaPlayerInterface*>(p.get())->setAudioSink(mAudioOutput);
	    }
	    return p;
	}


	
	sp<MediaPlayerBase> MediaPlayerService::Client::createPlayer(player_type playerType)
	{
	    // determine if we have the right player type
	    sp<MediaPlayerBase> p = mPlayer;
	    
	    if (p == NULL) {
	        p = MediaPlayerFactory::createPlayer(playerType, this, notify);
	    }
	    return p;
	}

有这几种播放器构造器

	void MediaPlayerFactory::registerBuiltinFactories() {
	    Mutex::Autolock lock_(&sLock);
	
	    if (sInitComplete)
	        return;
	
	    registerFactory_l(new StagefrightPlayerFactory(), STAGEFRIGHT_PLAYER);
	    registerFactory_l(new NuPlayerFactory(), NU_PLAYER);
	    registerFactory_l(new SonivoxPlayerFactory(), SONIVOX_PLAYER);
	    registerFactory_l(new TestPlayerFactory(), TEST_PLAYER);
	
	    sInitComplete = true;
	}



如何区分类型

	player_type MediaPlayerFactory::getPlayerType(const sp<IMediaPlayer>& client,
	                                              const char* url) {
	    GET_PLAYER_TYPE_IMPL(client, url);
	}

GET_PLAYER_TYPE_IMPL是一个宏定义

	#define GET_PLAYER_TYPE_IMPL(a...)                      \
	    Mutex::Autolock lock_(&sLock);                      \
	                                                        \
	    player_type ret = STAGEFRIGHT_PLAYER;               \
	    float bestScore = 0.0;                              \
	                                                        \
	    for (size_t i = 0; i < sFactoryMap.size(); ++i) {   \
	                                                        \
	        IFactory* v = sFactoryMap.valueAt(i);           \
	        float thisScore;                                \
	        CHECK(v != NULL);                               \
	        thisScore = v->scoreFactory(a, bestScore);      \
	        if (thisScore > bestScore) {                    \
	            ret = sFactoryMap.keyAt(i);                 \
	            bestScore = thisScore;                      \
	        }                                               \
	    }                                                   \
	                                                        \
	    if (0.0 == bestScore) {                             \
	        ret = getDefaultPlayerType();                   \
	    }                                                   \
	                                                        \
	    return ret;
    
通过打分，比较，得出最合适的MediaPlayer构造器，常用的一般是NuPlayerFactory，创建的一般是NuPlayerDriver，

	
	NuPlayerDriver::NuPlayerDriver()
	    : mState(STATE_IDLE),
	      mIsAsyncPrepare(false),
	      mAsyncResult(UNKNOWN_ERROR),
	      mSetSurfaceInProgress(false),
	      mDurationUs(-1),
	      mPositionUs(-1),
	      mNumFramesTotal(0),
	      mNumFramesDropped(0),
	      mLooper(new ALooper),
	      mPlayerFlags(0),
	      mAtEOS(false),
	      mStartupSeekTimeUs(-1) {
	    mLooper->setName("NuPlayerDriver Looper");
	
	    mLooper->start(
	            false, /* runOnCallingThread */
	            true,  /* canCallJava */
	            PRIORITY_AUDIO);
	
	    mPlayer = new NuPlayer;
	    mLooper->registerHandler(mPlayer);
	
	    mPlayer->setDriver(this);
	}

看看它的start
	
	
	status_t NuPlayerDriver::start() {
	    Mutex::Autolock autoLock(mLock);
	    switch (mState) {
	        case STATE_UNPREPARED:
	        {
	            status_t err = prepare_l();
	
	            if (err != OK) {
	                return err;
	            }
	            CHECK_EQ(mState, STATE_PREPARED);
	        }
	        case STATE_PREPARED:
	        {
	            mAtEOS = false;
	            mPlayer->start();
	
	            if (mStartupSeekTimeUs >= 0) {
	                if (mStartupSeekTimeUs == 0) {
	                    notifySeekComplete();
	                } else {
	                    mPlayer->seekToAsync(mStartupSeekTimeUs);
	                }
	
	                mStartupSeekTimeUs = -1;
	            }
	            break;
	        }
	
	        case STATE_RUNNING:
	            break;
	
	        case STATE_PAUSED:
	        {
	            mPlayer->resume();
	            break;
	        }
	
	        default:
	            return INVALID_OPERATION;
	    }
	
	    mState = STATE_RUNNING;
	
	    return OK;
	}
	
可以看到，不能随便改状态，这里牵扯到MediaPlayer的状态机


        case kWhatStart:
        {
            ALOGV("kWhatStart");

            mVideoIsAVC = false;
            mAudioEOS = false;
            mVideoEOS = false;
            mSkipRenderingAudioUntilMediaTimeUs = -1;
            mSkipRenderingVideoUntilMediaTimeUs = -1;
            mVideoLateByUs = 0;
            mNumFramesTotal = 0;
            mNumFramesDropped = 0;
            mStarted = true;
            mSource->start();
            uint32_t flags = 0;

            if (mSource->isRealTime()) {
                flags |= Renderer::FLAG_REAL_TIME;
            }
           mRenderer = new Renderer(
                    mAudioSink,
                    new AMessage(kWhatRendererNotify, id()),
                    flags);

            looper()->registerHandler(mRenderer);

            postScanSources();
            break;
        }

**我们根据数据源的来源分为本地媒体和流媒体两种**。

　　本地媒体数据源：
　　本地媒体有两种读入的方式，一种是直接的路径读入， 在android_media_MediaPlayer中的jniGetFDFromFileDescriptor函数转化为fd，一种是数据库Uri的方式，contont：//在MediaPlayerService中的openContentProviderFile转化为fd；最后通过FileSource构造函数FileSource(int fd, int64_t offset, int64_t length)生成一个FileSource的实例。

　　流媒体数据源：
　　目前Android支持的流媒体协议有三种：http渐进流下载，httplive，rtsp。在流媒体播放器nuplayer中，HTTPLiveSource中有一个LiveSession的sp指针和一个ATSParser的sp指针，其中LiveSession中包含了一个LiveDataSource的数据源对象；RTSPSource的源有一些特殊，它没有继承DataSource，而是通过dequeueAccessUnit接口，Server端的压缩流通过queueAccessUnit保存到这里；GenericSource中包括了本地FileSource和http渐进流下载协议的源NuCachedSource2两种。

　　播放器是如何区分这些数据源：
　　在MediaPlayerService中，getPlayerType(int fd, int64_t offset, int64_t length)和player_type getPlayerType(const char* url)决定了使用何种player 
　　
　　
　　
		参数为fd :
　　　　oggs  STAGEFRIGHT_PLAYER
　　　　midi使用EAS_OpenFile测试是打开成功：STAGEFRIGHT_PLAYER
　　　　其它：media.stagefright.use_nuplayer{0  STAGEFRIGHT_PLAYER,  1  NU_PLAYER}
	
	　　参数为url:
	　　　　http:// https:// (含有.m3u8 .m3u m3u8 .56.com  NU_PLAYER, 其它  STAGEFRIGHT_PLAYER)
	　　　　midi mid smf xmf imy rtttl rtx ota  SONIVOX_PLAYER
	　　　　rtsp：//  NU_PLAYER
	　　　　aahRx：//  AAH_RX_PLAYER
	　　　　content://  STAGEFRIGHT_PLAYER
	　　　　mRetransmitEndpointValid标记为true: AAH_TX_PLAYER 中继 转播
　　
# 如何开始播放

MediaHTTPConnection，是利用socket，还是HTTP直接获取呢？  
　　
# 缓存

# 解析

# 参考文档

[Android Media Player 框架分析](http://blog.csdn.net/harman_zjc/article/details/53386010)     
[⑤NuPlayer播放框架之GenericSource源码分析](https://www.cnblogs.com/tocy/p/5-nuplayer-GenericSource-source-code-analysis.html)       
[④NuPlayer播放框架之Renderer源码分析](http://www.cnblogs.com/tocy/p/4-nuplayer-renderer-source-code-analysis.html)        
[Android流媒体的实现](http://blog.csdn.net/ffmpeg4976/article/details/52190993)        
[Android多媒体：AudioTrack](http://blog.csdn.net/ffmpeg4976/article/details/46939523)          
[ 视频格式基础知识：让你了解MKV、MP4、H.265、码率\码流、多码流等等](http://blog.csdn.net/xx326664162/article/details/51784440)         
[ 流媒体协议介绍（RTP/RTCP/RTSP/MMS/HLS/HTTP progressive streaming）](http://blog.csdn.net/xx326664162/article/details/51781399)