---
layout: default
title:  InputManager输入管理子系统 
category: [android]
image:  http://upload-images.jianshu.io/upload_images/1460468-8fa8e9bc442afab2.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---

自定义View的时候经常会遇到Touch事件的处理，这些事件到底是怎么来的呢？源头是哪呢？从手指接触屏幕到MotionEvent被传送到Activity或者View，中间究竟经历了什么？

# Android触摸事件模型

触摸事件肯定要先捕获才能传给窗口，因此，首先应该有一个线程在不断的监听屏幕，一旦有触摸事件，就将事件捕获；其次，还应该存在某种手段可以找到目标窗口，因为可能有多个APP的多个界面为用户可见，必须确定这个事件究竟通知那个窗口；最后，才是目标窗口如何消费事件的问题。

![触摸事件模型.jpg](http://upload-images.jianshu.io/upload_images/1460468-bf044f9479ef8f3d.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

InputManagerService是Android为了处理各种用户操作而抽象的一个服务，自身可以看做是一个Binder服务实体，在SystemServer进程启动的时候实例化，并注册到ServiceManager中去，不过这个服务对外主要是用来提供一些输入设备的信息的作用，作为Binder服务的作用比较小：
 
    private void startOtherServices() {
            ...
            inputManager = new InputManagerService(context);
            wm = WindowManagerService.main(context, inputManager,
                    mFactoryTestMode != FactoryTest.FACTORY_TEST_LOW_LEVEL,
                    !mFirstBoot, mOnlyCore);
            ServiceManager.addService(Context.WINDOW_SERVICE, wm);
            ServiceManager.addService(Context.INPUT_SERVICE, inputManager);
           ...
           }

InputManagerService跟WindowManagerService几乎同时被添加，从一定程度上也能说明两者几乎是相生的关系，而触摸事件的处理也确实同时涉及两个服务，最好的证据就是WindowManagerService需要直接握着InputManagerService的引用，如果对照上面的处理模型，InputManagerService主要负责触摸事件的采集，而WindowManagerService负责找到目标窗口。接下来，先看看InputManagerService如何完成触摸事件的采集。

# 如何捕获触摸事件

InputManagerService会单独开一个线程专门用来读取触摸事件，

	NativeInputManager::NativeInputManager(jobject contextObj,
	        jobject serviceObj, const sp<Looper>& looper) :
	        mLooper(looper), mInteractive(true) {
	  	 ...
	    sp<EventHub> eventHub = new EventHub();
	    mInputManager = new InputManager(eventHub, this, this);
	}

这里有个EventHub，它主要是利用Linux的inotify和epoll机制，监听设备事件：包括设备插拔及各种触摸、按钮事件等，可以看做是一个不同设备的集线器，主要面向的是/dev/input目录下的设备节点，比如说/dev/input/event0上的事件就是输入事件，通过EventHub的getEvents就可以监听并获取该事件：

![EventHub模型.jpg](http://upload-images.jianshu.io/upload_images/1460468-b6d934a08d75bdfc.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

在new InputManager时候，会新建一个InputReader对象及InputReaderThread Loop线程，这个loop线程的主要作用就是通过EventHub的getEvents获取Input事件

![InputRead线程启动流程](http://upload-images.jianshu.io/upload_images/1460468-57833de14e98f7f0.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

	InputManager::InputManager(
	        const sp<EventHubInterface>& eventHub,
	        const sp<InputReaderPolicyInterface>& readerPolicy,
	        const sp<InputDispatcherPolicyInterface>& dispatcherPolicy) {
	    <!--事件分发执行类-->
	    mDispatcher = new InputDispatcher(dispatcherPolicy);
	    <!--事件读取执行类-->
	    mReader = new InputReader(eventHub, readerPolicy, mDispatcher);
	    initialize();
	}

	void InputManager::initialize() {
	    mReaderThread = new InputReaderThread(mReader);
	    mDispatcherThread = new InputDispatcherThread(mDispatcher);
	}
	
	bool InputReaderThread::threadLoop() {
	    mReader->loopOnce();
	    return true;
	}

	void InputReader::loopOnce() {
		    int32_t oldGeneration;
		    int32_t timeoutMillis;
		    bool inputDevicesChanged = false;
		    Vector<InputDeviceInfo> inputDevices;
		    {  
		  ...<!--监听事件-->
		    size_t count = mEventHub->getEvents(timeoutMillis, mEventBuffer, EVENT_BUFFER_SIZE);
		   ....<!--处理事件-->
		       processEventsLocked(mEventBuffer, count);
		   ...
		   <!--通知派发-->
		    mQueuedListener->flush();
		}

通过上面流程，输入事件就可以被读取，经过processEventsLocked被初步封装成RawEvent，最后发通知，请求派发消息。以上就解决了事件读取问题，下面重点来看一下事件的分发。

# 事件的派发

在新建InputManager的时候，不仅仅创建了一个事件读取线程，还创建了一个事件派发线程，虽然也可以直接在读取线程中派发，但是这样肯定会增加耗时，不利于事件的及时读取，因此，事件读取完毕后，直接向派发线程发个通知，请派发线程去处理，这样读取线程就可以更加敏捷，防止事件丢失，因此InputManager的模型就是如下样式：

![InputManager模型.jpg](http://upload-images.jianshu.io/upload_images/1460468-5692afaef6fdc134.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

InputReader的mQueuedListener其实就是InputDispatcher对象，所以mQueuedListener->flush()就是通知InputDispatcher事件读取完毕，可以派发事件了, InputDispatcherThread是一个典型Looper线程，基于native的Looper实现了Hanlder消息处理模型，如果有Input事件到来就被唤醒处理事件，处理完毕后继续睡眠等待，简化代码如下：

	bool InputDispatcherThread::threadLoop() {
	    mDispatcher->dispatchOnce();
	    return true;
	}

	void InputDispatcher::dispatchOnce() {
	    nsecs_t nextWakeupTime = LONG_LONG_MAX;
	    {  
	      <!--被唤醒 ，处理Input消息-->
	        if (!haveCommandsLocked()) {
	            dispatchOnceInnerLocked(&nextWakeupTime);
	        }
           ...
	    } 
	    nsecs_t currentTime = now();
	    int timeoutMillis = toMillisecondTimeoutDelay(currentTime, nextWakeupTime);
	    <!--睡眠等待input事件-->
	    mLooper->pollOnce(timeoutMillis);
	}
	
以上就是派发线程的模型，dispatchOnceInnerLocked是具体的派发处理逻辑，这里看其中一个分支，触摸事件：

	void InputDispatcher::dispatchOnceInnerLocked(nsecs_t* nextWakeupTime) {
		    ...
	    case EventEntry::TYPE_MOTION: {
	        MotionEntry* typedEntry = static_cast<MotionEntry*>(mPendingEvent);
	        ...
	        done = dispatchMotionLocked(currentTime, typedEntry,
	                &dropReason, nextWakeupTime);
	        break;
	    }
    
	bool InputDispatcher::dispatchMotionLocked(
	        nsecs_t currentTime, MotionEntry* entry, DropReason* dropReason, nsecs_t* nextWakeupTime) {
	    ...     
	    Vector<InputTarget> inputTargets;
	    bool conflictingPointerActions = false;
	    int32_t injectionResult;
	    if (isPointerEvent) {
	    <!--关键点1 找到目标Window-->
	        injectionResult = findTouchedWindowTargetsLocked(currentTime,
	                entry, inputTargets, nextWakeupTime, &conflictingPointerActions);
	    } else {
	        injectionResult = findFocusedWindowTargetsLocked(currentTime,
	                entry, inputTargets, nextWakeupTime);
	    }
	    ...
	    <!--关键点2  派发-->
	    dispatchEventLocked(currentTime, entry, inputTargets);
	    return true;
	}

从以上代码可以看出，对于触摸事件会首先通过findTouchedWindowTargetsLocked找到目标Window，进而通过dispatchEventLocked将消息发送到目标窗口，下面看一下如何找到目标窗口，以及这个窗口列表是如何维护的。

# 如何为触摸事件找到目标窗口

Android系统能够同时支持多块屏幕，每块屏幕被抽象成一个DisplayContent对象，内部维护一个WindowList列表对象，用来记录当前屏幕中的所有窗口，包括状态栏、导航栏、应用窗口、子窗口等。对于触摸事件，我们比较关心可见窗口，用adb shell dumpsys SurfaceFlinger看一下可见窗口的组织形式：

![焦点窗口](http://upload-images.jianshu.io/upload_images/1460468-1716580503003c5c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

那么，如何找到触摸事件对应的窗口呢，是状态栏、导航栏还是应用窗口呢，这个时候DisplayContent的WindowList就发挥作用了，DisplayContent握着所有窗口的信息，因此，可以**根据触摸事件的位置及窗口的属性来确定将事件发送到哪个窗口**，当然其中的细节比一句话复杂的多，跟窗口的状态、透明、分屏等信息都有关系，下面简单瞅一眼，达到主观理解的流程就可以了，


	int32_t InputDispatcher::findTouchedWindowTargetsLocked(nsecs_t currentTime,
	        const MotionEntry* entry, Vector<InputTarget>& inputTargets, nsecs_t* nextWakeupTime,
	        bool* outConflictingPointerActions) {
	        ...
	        sp<InputWindowHandle> newTouchedWindowHandle;
	        bool isTouchModal = false;
	        <!--遍历所有窗口-->
	        size_t numWindows = mWindowHandles.size();
	        for (size_t i = 0; i < numWindows; i++) {
	            sp<InputWindowHandle> windowHandle = mWindowHandles.itemAt(i);
	            const InputWindowInfo* windowInfo = windowHandle->getInfo();
	            if (windowInfo->displayId != displayId) {
	                continue; // wrong display
	            }
	            int32_t flags = windowInfo->layoutParamsFlags;
	            if (windowInfo->visible) {
	                if (! (flags & InputWindowInfo::FLAG_NOT_TOUCHABLE)) {
	                    isTouchModal = (flags & (InputWindowInfo::FLAG_NOT_FOCUSABLE
	                            | InputWindowInfo::FLAG_NOT_TOUCH_MODAL)) == 0;
		     <!--找到目标窗口-->
	                    if (isTouchModal || windowInfo->touchableRegionContainsPoint(x, y)) {
	                        newTouchedWindowHandle = windowHandle;
	                        break; // found touched window, exit window loop
	                    }
	                }
	              ...
	              
mWindowHandles代表着所有窗口，findTouchedWindowTargetsLocked的就是从mWindowHandles中找到目标窗口，规则太复杂，总之就是根据点击位置更窗口Z order之类的特性去确定，有兴趣可以自行分析。不过这里需要关心的是mWindowHandles，它就是是怎么来的，另外窗口增删的时候如何保持最新的呢？这里就牵扯到跟WindowManagerService交互的问题了，mWindowHandles的值是在InputDispatcher::setInputWindows中设置的，
	
	void InputDispatcher::setInputWindows(const Vector<sp<InputWindowHandle> >& inputWindowHandles) {
	        ...
	        mWindowHandles = inputWindowHandles;
           ...

谁会调用这个函数呢？ 真正的入口是WindowManagerService中的InputMonitor会简介调用InputDispatcher::setInputWindows，这个时机主要是跟窗口增改删除等逻辑相关，以addWindow为例：

![更新窗口逻辑.png](http://upload-images.jianshu.io/upload_images/1460468-aaf6043e34fc61ec.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
     
从上面流程可以理解为什么说WindowManagerService跟InputManagerService是相辅相成的了，到这里，如何找到目标窗口已经解决了，下面就是如何将事件发送到目标窗口的问题了。
	              
# 如何将事件发送到目标窗口
	
找到了目标窗口，同时也将事件封装好了，剩下的就是通知目标窗口，可是有个最明显的问题就是，目前所有的逻辑都是在SystemServer进程，而要通知的窗口位于APP端的用户进程，那么如何通知呢？下意识的可能会想到Binder通信，毕竟Binder在Android中是使用最多的IPC手段了，不过Input事件处理这采用的却不是Binder：**高版本的采用的都是Socket的通信方式，而比较旧的版本采用的是Pipe管道的方式**。

	void InputDispatcher::dispatchEventLocked(nsecs_t currentTime,
	        EventEntry* eventEntry, const Vector<InputTarget>& inputTargets) {
	    pokeUserActivityLocked(eventEntry);
	    for (size_t i = 0; i < inputTargets.size(); i++) {
	        const InputTarget& inputTarget = inputTargets.itemAt(i);
	        ssize_t connectionIndex = getConnectionIndexLocked(inputTarget.inputChannel);
	        if (connectionIndex >= 0) {
	            sp<Connection> connection = mConnectionsByFd.valueAt(connectionIndex);
	            prepareDispatchCycleLocked(currentTime, connection, eventEntry, &inputTarget);
	        } else {
	        }
	    }
	}
	
代码逐层往下看会发现最后会调用到InputChannel的sendMessage函数，最会通过socket发送到APP端（Socket怎么来的接下来会分析），

![send流程.png](http://upload-images.jianshu.io/upload_images/1460468-6977678cba0df4b7.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

这个Socket是怎么来的呢？或者说两端通信的一对Socket是怎么来的呢？其实还是要牵扯到WindowManagerService，在APP端向WMS请求添加窗口的时候，会伴随着Input通道的创建，窗口的添加一定会调用ViewRootImpl的setView函数：

>ViewRootImpl
 
    public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
    				...
                requestLayout();
                if ((mWindowAttributes.inputFeatures
                        & WindowManager.LayoutParams.INPUT_FEATURE_NO_INPUT_CHANNEL) == 0) {
                     <!--创建InputChannel容器-->
                    mInputChannel = new InputChannel();
                }
                try {
                    mOrigWindowType = mWindowAttributes.type;
                    mAttachInfo.mRecomputeGlobalAttributes = true;
                    collectViewAttributes();
                    <!--添加窗口，并请求开辟Socket Input通信通道-->
                    res = mWindowSession.addToDisplay(mWindow, mSeq, mWindowAttributes,
                            getHostVisibility(), mDisplay.getDisplayId(),
                            mAttachInfo.mContentInsets, mAttachInfo.mStableInsets,
                            mAttachInfo.mOutsets, mInputChannel);
                }...
                <!--监听，开启Input信道-->
                if (mInputChannel != null) {
                    if (mInputQueueCallback != null) {
                        mInputQueue = new InputQueue();
                        mInputQueueCallback.onInputQueueCreated(mInputQueue);
                    }
                    mInputEventReceiver = new WindowInputEventReceiver(mInputChannel,
                            Looper.myLooper());
                }
                
                
在IWindowSession.aidl定义中 InputChannel是out类型，也就是说需要服务端进行填充，那么接着看服务端WMS如何填充的呢？

    public int addWindow(Session session, IWindow client, int seq,
            WindowManager.LayoutParams attrs, int viewVisibility, int displayId,
            Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
            InputChannel outInputChannel) {            
			  ...
            if (outInputChannel != null && (attrs.inputFeatures
                    & WindowManager.LayoutParams.INPUT_FEATURE_NO_INPUT_CHANNEL) == 0) {
                String name = win.makeInputChannelName();
                <!--关键点1创建通信信道 -->
                InputChannel[] inputChannels = InputChannel.openInputChannelPair(name);
                <!--本地用-->
                win.setInputChannel(inputChannels[0]);
                <!--APP端用-->
                inputChannels[1].transferTo(outInputChannel);
                <!--注册信道与窗口-->
                mInputManager.registerInputChannel(win.mInputChannel, win.mInputWindowHandle);
            }

WMS首先创建socketpair作为全双工通道，并分别填充到Client与Server的InputChannel中去；之后让InputManager将Input通信信道与当前的窗口ID绑定，这样就能知道哪个窗口用哪个信道通信了；最后通过Binder将outInputChannel回传到APP端，下面是SocketPair的创建代码：
	
	status_t InputChannel::openInputChannelPair(const String8& name,
	        sp<InputChannel>& outServerChannel, sp<InputChannel>& outClientChannel) {
	    int sockets[2];
	    if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets)) {
	        status_t result = -errno;
	        ...
	        return result;
	    }
	
	    int bufferSize = SOCKET_BUFFER_SIZE;
	    setsockopt(sockets[0], SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize));
	    setsockopt(sockets[0], SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize));
	    setsockopt(sockets[1], SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize));
	    setsockopt(sockets[1], SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize));
		<!--填充到server inputchannel-->
	    String8 serverChannelName = name;
	    serverChannelName.append(" (server)");
	    outServerChannel = new InputChannel(serverChannelName, sockets[0]);
		 <!--填充到client inputchannel-->
	    String8 clientChannelName = name;
	    clientChannelName.append(" (client)");
	    outClientChannel = new InputChannel(clientChannelName, sockets[1]);
	    return OK;
	}

这里socketpair的创建与访问其实是还是借助文件描述符，**WMS需要借助Binder通信向APP端回传文件描述符fd**，这部分只是可以参考Binder知识，主要是在内核层面实现两个进程fd的转换，窗口添加成功后，socketpair被创建，被传递到了APP端，但是信道并未完全建立，因为还需要一个主动的监听，毕竟消息到来是需要通知的，先看一下信道模型

![InputChannl信道.jpg](http://upload-images.jianshu.io/upload_images/1460468-1de01a884b9c26d9.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


**APP端的监听消息的手段是：将socket添加到Looper线程的epoll数组中去**，一有消息到来Looper线程就会被唤醒，并获取事件内容，从代码上来看，通信信道的打开是伴随WindowInputEventReceiver的创建来完成的。

![fd打开通信信道.png](http://upload-images.jianshu.io/upload_images/1460468-67a08564ae0785ae.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

信息到来，Looper根据fd找到对应的监听器：NativeInputEventReceiver，并调用handleEvent处理对应事件

	int NativeInputEventReceiver::handleEvent(int receiveFd, int events, void* data) {
       ...
	    if (events & ALOOPER_EVENT_INPUT) {
	        JNIEnv* env = AndroidRuntime::getJNIEnv();
	        status_t status = consumeEvents(env, false /*consumeBatches*/, -1, NULL);
	        mMessageQueue->raiseAndClearException(env, "handleReceiveCallback");
	        return status == OK || status == NO_MEMORY ? 1 : 0;
	    }
      ...
      
之后会进一步读取事件，并封装成Java层对象，传递给Java层，进行相应的回调处理：

	status_t NativeInputEventReceiver::consumeEvents(JNIEnv* env,  
	        bool consumeBatches, nsecs_t frameTime, bool* outConsumedBatch) {  
	        ...
	    for (;;) {  
	        uint32_t seq;  
	        InputEvent* inputEvent;  
	        <!--获取事件-->
	        status_t status = mInputConsumer.consume(&mInputEventFactory,  
	                consumeBatches, frameTime, &seq, &inputEvent);  
	        ...
	        <!--处理touch事件-->
          case AINPUT_EVENT_TYPE_MOTION: {
            MotionEvent* motionEvent = static_cast<MotionEvent*>(inputEvent);
            if ((motionEvent->getAction() & AMOTION_EVENT_ACTION_MOVE) && outConsumedBatch) {
                *outConsumedBatch = true;
            }
            inputEventObj = android_view_MotionEvent_obtainAsCopy(env, motionEvent);
            break;
            } 
            <!--回调处理函数-->
		   if (inputEventObj) {
		                env->CallVoidMethod(receiverObj.get(),
		                        gInputEventReceiverClassInfo.dispatchInputEvent, seq, inputEventObj);
		                env->DeleteLocalRef(inputEventObj);
		            }

所以最后就是触摸事件被封装成了inputEvent，并通过InputEventReceiver的dispatchInputEvent（WindowInputEventReceiver）进行处理，这里就返回到我们常见的Java世界了。

# 目标窗口中的事件处理

最后简单看一下事件的处理流程，Activity或者Dialog等是如何获得Touch事件的呢？如何处理的呢？直白的说就是将监听事件交给ViewRootImpl中的rootView，让它自己去负责完成事件的消费，究竟最后被哪个View消费了要看具体实现了，而对于Activity与Dialog中的DecorView重写了View的事件分配函数dispatchTouchEvent，将事件处理交给了CallBack对象处理，至于View及ViewGroup的消费，算View自身的逻辑了。

![APP端事件处理流程](http://upload-images.jianshu.io/upload_images/1460468-2fd6f0dc8942ad3c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


# 总结

现在把所有的流程跟模块串联起来，流程大致如下：

* 点击屏幕
* InputManagerService的Read线程捕获事件，预处理后发送给Dispatcher线程
* Dispatcher找到目标窗口
* 通过Socket将事件发送到目标窗口
* APP端被唤醒
* 找到目标窗口处理事件
 
![InputManager完整模型.jpg](http://upload-images.jianshu.io/upload_images/1460468-d3f047d27598fcf8.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)



  
###  参考文档

[Android输入系统之InputChannel(上)](http://blog.csdn.net/itleaks/article/details/27165657)            
[http://gityuan.com/2015/09/19/android-touch/](http://gityuan.com/2015/09/19/android-touch/)