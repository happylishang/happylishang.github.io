---
layout: default
title: 一次点击事件的旅程 - InputManager输入子系统 
category: [android]

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

这个Socket是怎么来的呢？或者说两端通信的一对Socket是怎么来的呢？这里其实还是要牵扯到WindowManagerService，在APP端向WMS请求添加窗口的时候，会伴随着Input通道的创建，窗口的添加一定会调用ViewRootImpl的setView函数：

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
                }
                
在IWindowSession.aidl定义中 InputChannel是out类型，也就是需要服务端进行填充的，那么接着看服务端WMS如何填充的呢？

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

# Linux内核输入子系统

触摸事件是由Linux内核的一个Input子系统来管理的(InputManager)，Linux子系统会在 /dev/input/ 这个路径下创建硬件输入设备节点(这里的硬件设备就是我们的触摸屏了)。当手指触动触摸屏时，硬件设备通过设备节点像内核(其实是InputManager管理)报告事件，InputManager 经过处理将此事件传给 Android系统的一个系统Service： WindowManagerService 。

WindowManagerService调用dispatchPointer()从存放WindowState的z-order顺序列表中找到能接收当前touch事件的 WindowState，通过IWindow代理将此消息发送到IWindow服务端(IWindow.Stub子类)，这个IWindow.Stub属于ViewRoot(这个类继承Handler，主要用于连接PhoneWindow和WindowManagerService)，所以事件就传到了ViewRoot.dispatchPointer()中.
 
 我们来看一下ViewRoot的dispatchPointer方法：、
 
	  public void dispatchPointer(MotionEvent event, long eventTime,
	              boolean callWhenDone) {
	         Message msg = obtainMessage(DISPATCH_POINTER);
	         msg.obj = event;
	         msg.arg1 = callWhenDone ? 1 : 0;
	         sendMessageAtTime(msg, eventTime);
	      }
	
dispatchPointer方法就是把这个事件封装成Message发送出去，在ViewRoot Handler的handleMessage中被处理，其调用了mView.dispatchTouchEvent方法(mView是一个PhoneWindow.DecorView对象)，PhoneWindow.DecorView继承FrameLayout(FrameLayout继承ViewGroup，ViewGroup继承自View),DecorView里的dispatchTouchEvent方法如下. 这里的Callback的cb其实就是Activity的attach()方法里的设置回调。

        @Override
        public boolean dispatchTouchEvent(MotionEvent ev) {
            final Callback cb = getCallback();
            return cb != null && mFeatureId < 0 ? cb.dispatchTouchEvent(ev) : super
                    .dispatchTouchEvent(ev);
        }
	
也就是说，正常情形下，当前的Activity就是这里的cb，即调用了Activity的dispatchTouchEvent方法。

下面来分析一下从Activity到各个子View的事件传递和处理过程。
首先先分析Activity的dispatchTouchEvent方法。

    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (ev.getAction() == MotionEvent.ACTION_DOWN) {
            onUserInteraction();
        }
        if (getWindow().superDispatchTouchEvent(ev)) {
            return true;
        }
        return onTouchEvent(ev);
    }
	
onUserInteraction() 是一个空方法，开发者可以根据自己的需求覆写这个方法(这个方法在一个Touch事件的周期肯定会调用到的)。如果判断成立返回True，当前事件就不在传播下去了。 superDispatchTouchEvent(ev) 这个方法做了什么呢？ getWindow().superDispatchTouchEvent(ev) 也就是调用了 PhoneWindow.superDispatchTouchEvent 方法，而这个方法返回的是 mDecor.superDispatchTouchEvent(event)，在内部类 DecorView(上文中的mDecor) 的superDispatchTouchEvent 中调用super.dispatchTouchEvent(event)，而DecorView继承自ViewGroup(通过FrameLayout，FrameLayout没有dispatchTouchEvent)，最终调用的是ViewGroup的dispatchTouchEvent方法。

	               --> performLaunchActivity(ActivityRecord, Intent) : Activity - android.app.ActivityThread
	               
performLaunchActivity我们很熟识，因为我前面在讲Activity启动过程详解时候讲过，在启动一个新的Activity会执行该方法，在该方法里面会执行attach方法，找到attach方法对应代码可以看到：

	        mWindow = PolicyManager.makeNewWindow(this);
	        mWindow.setCallback(this);

mWindow就是一个PhoneWindow，它是Activity的一个内部成员，通过调用mWindow的setCallback(this)，把新建立的Activity设置为PhoneWindow一个mCallback成员，这样我们就清楚了，前面的cb就是拥有这个PhoneWindow的Activity,cb.dispatchTouchEvent(ev)也就是执行：Activity.dispatchTouchEvent


Event事件是首先到了 PhoneWindow 的 DecorView 的 dispatchTouchEvent 方法，此方法通过 CallBack 调用了 Activity 的 dispatchTouchEvent 方法，在 Activity 这里，我们可以重写 Activity 的dispatchTouchEvent 方法阻断 touch事件的传播。接着在Activity里的dispatchTouchEvent 方法里，事件又再次传递到DecorView，DecorView通过调用父类(ViewGroup)的dispatchTouchEvent 将事件传给父类处理，也就是我们下面要分析的方法，这才进入网上大部分文章讲解的touch事件传递流程。

为什么要从 PhoneWindow.DecorView 中 传到 Activity，然后在传回 PhoneWindow.DecorView 中呢？ 主要是为了方便在Activity中通过控制dispatchTouchEvent 来控制当前Activity 事件的分发， 下一篇关于数据埋点文章就应用了这个机制，我们要重点分析的就是ViewGroup中的dispatchTouchEvent方法。 

 ViewGroup 的 dispatchTouchEvent 的调用过程。
 
* 首先判断此 MotionEvent 能否被拦截，如果是的话，能调用我们覆写 onInterceptTouchEvent来处理拦截到的事件；如果此方法返回TRUE，表示需要拦截，那么事件到此为止，就不会传递到子View中去。这里要注意，onInterceptTouchEvent 方法默认是返回FALSE。
  
* 若没有拦截此Event，首先找到此ViewGroup中所有的子View，通过方法 canViewReceivePointerEvents和isTransformedTouchPointInView，对每个子View通过坐标(Event事件坐标和子View坐标比对)计算，找到坐标匹配的View。
 
* 调用dispatchTransformedTouchEvent方法，处理Event事件。


> **用户点击屏幕产生Touch(包括DOWN、UP、MOVE，本文分析的是DOWN)事件 
> -> InputManager
> -> WindowManagerService.dispatchPointer() 
> -> IWindow.Stub 
> -> ViewRoot.dispatchPointer() 
> -> PhoneWindow.DecorView.dispatchTouchEvent() 
> -> Activity.dispatchTouchEvent() 
> -> PhoneWindow.superDispatchTouchEvent 
> -> PhoneWindow.DecorView.superDispatchTouchEvent 
> -> ViewGroup.dispatchTouchEvent() 
> -> ViewGroup.dispatchTransformedTouchEvent() 
> -> 子View.dispatchTouchEvent() 
> -> 子View.onTouch() 
> -> 子View.onTouchEvent() 
> -> 事件被消费结束。(这个过程是由上往下传导)
> -> 如果事件没有被子View消费，也就是说子View的dispatchTouchEvent返回false，此时事件由其父类处理(由下往上传导)，最后到达系统边界也没处理，就将此事件抛弃了。**


#### 管道是半双工的，数据只能向一个方向流动；需要双方通信时，需要建立起两个管道；

在Looper类内部，会创建一个管道，然后Looper会睡眠在这个管道的读端，等待另外一个线程来往这个管道的写端写入新的内容，从而唤醒等待在这个管道读端的线程，除此之外，Looper还可以同时睡眠等待在其它的文件描述符上，因为它是通过Linux系统的epoll机制来批量等待指定的文件有新的内容可读的。这些其它的文件描述符就是通过Looper类的addFd成函数添加进去的了，在添加的时候，还可以指定回调函数，即当这个文件描述符所指向的文件有新的内容可读时，Looper就会调用这个handleReceiveCallback函数，有兴趣的读者可以自己研究一下Looper类的addFd函数的实现，它位于frameworks/base/libs/utils/Looper.cpp文件中。


Client 

	InputQueue* InputQueue::createQueue(jobject inputQueueObj, const sp<Looper>& looper) {
	
	    int pipeFds[2];
	    if (pipe(pipeFds)) {
	        ALOGW("Could not create native input dispatching pipe: %s", strerror(errno));
	        return NULL;
	    }
	    fcntl(pipeFds[0], F_SETFL, O_NONBLOCK);
	    fcntl(pipeFds[1], F_SETFL, O_NONBLOCK);
	    return new InputQueue(inputQueueObj, looper, pipeFds[0], pipeFds[1]);
	}

#### 2.3采用了管道但是4.4采用了Socket


       inputChannels[1].transferTo(outInputChannel);这实际上是将生成的outClientChannel赋值给outInputChannel，但这里并没有赋值给client端的InputChannel啊？这里到底是怎么影响到client端的InputChannel呢？

	这里实际上是利用binder调用中out关键字，我们来看一下IWindowSession.aidl中的addToDisplay方法声明：
	
	 int addToDisplay(IWindow window, int seq, in WindowManager.LayoutParams attrs,
	            in int viewVisibility, in int layerStackId, out Rect outContentInsets,
	            out Rect outStableInsets, out InputChannel outInputChannel);
	  很明显这里使用了out关键字，对binder调用熟悉的同学就知道实际上这个关键字的作用就是我们传递给server端的参数在server进程中被改变的话会被反馈回给client端。这样就相当于我们在client端可以拿到之前打开的那个socket，最终回到第二部分，就可以被Looper监听这个socket了。         
	            
	 
 EventHub是输入设备的控制中心，它直接与input driver打交道。负责处理输入设备的增减，查询，输入事件的处理并向上层提供getEvents()接口接收事件。在它的构造函数中，主要做三件事：
1. 创建epoll对象，之后就可以把各输入设备的fd挂在上面多路等待输入事件。
2. 建立用于唤醒的pipe，把读端挂到epoll上，以后如果有设备参数的变化需要处理，而getEvents()又阻塞在设备上，就可以调用wake()在pipe的写端写入，就可以让线程从等待中返回。
3. 利用inotify机制监听/dev/input目录下的变更，如有则意味着设备的变化，需要处理。
 
 参考 
 
 <img src="http://doc.ithao123.cn/d/b6/90/86/b69086c86d7822f6ef3fa099b006c77a.jpg"/width=800>          
 
 InputReader 仅负责读取、并唤醒InputDispatch
 InputDispatch负责派发 ，


     
# WMS 与PhoneWindow与Touch事件

SystemServer进程中新建InputManagerService

	HandlerThread wmHandlerThread = new HandlerThread("WindowManager");  
	wmHandlerThread.start();  
	Handler wmHandler = new Handler(wmHandlerThread.getLooper());    
	  
	    inputManager = new InputManagerService(context, wmHandler);             
	    wm = WindowManagerService.main(context, power, display, inputManager,  
	            wmHandler, factoryTest != SystemServer.FACTORY_TEST_LOW_LEVEL,  
	            !firstBoot, onlyCore);  
	    ServiceManager.addService(Context.WINDOW_SERVICE, wm);  
	    ServiceManager.addService(Context.INPUT_SERVICE, inputManager);  
	  
	    inputManager.setWindowManagerCallbacks(wm.getInputMonitor());  
    inputManager.start(); 
	
InputManagerService的启动

	static jint nativeInit(JNIEnv* env, jclass clazz,
	        jobject serviceObj, jobject contextObj, jobject messageQueueObj) {
	    sp<MessageQueue> messageQueue = android_os_MessageQueue_getMessageQueue(env, messageQueueObj);
	    if (messageQueue == NULL) {
	        jniThrowRuntimeException(env, "MessageQueue is not initialized.");
	        return 0;
	    }
	
	    NativeInputManager* im = new NativeInputManager(contextObj, serviceObj,
	            messageQueue->getLooper());
	    im->incStrong(0);
	    return reinterpret_cast<jint>(im);
	}
	
因为Java层的MessageQueue总是对应native层的NativeMessageQueue对象，所以首先先取得native层的messageQueue，并构造NativeInputManager对象：

	public class InputManagerService extends IInputManager.Stub	
	
	
	NativeInputManager::NativeInputManager(jobject contextObj,  
	        jobject serviceObj, const sp<Looper>& looper) :  
	        mLooper(looper) {  
	    JNIEnv* env = jniEnv();  
	  
	    mContextObj = env->NewGlobalRef(contextObj);  
	    mServiceObj = env->NewGlobalRef(serviceObj);  
	  
	    {  
	        AutoMutex _l(mLock);  
	        mLocked.systemUiVisibility = ASYSTEM_UI_VISIBILITY_STATUS_BAR_VISIBLE;  
	        mLocked.pointerSpeed = 0;  
	        mLocked.pointerGesturesEnabled = true;  
	        mLocked.showTouches = false;  
	    }  
	  
	    sp<EventHub> eventHub = new EventHub();  
	    mInputManager = new InputManager(eventHub, this, this);  
	} 
	
EventHub是监听的关键类，EventHub采用了管道，我们知道新版本的Looper采用了eventfd实现唤醒，而这里的EventHub还是采用管道
	
	EventHub::EventHub(void) :
	        mBuiltInKeyboardId(NO_BUILT_IN_KEYBOARD), mNextDeviceId(1), mControllerNumbers(),
	        mOpeningDevices(0), mClosingDevices(0),
	        mNeedToSendFinishedDeviceScan(false),
	        mNeedToReopenDevices(false), mNeedToScanDevices(true),
	        mPendingEventCount(0), mPendingEventIndex(0), mPendingINotify(false) {
	    acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKE_LOCK_ID);
	    mEpollFd = epoll_create(EPOLL_SIZE_HINT);
	    mINotifyFd = inotify_init();
	    int result = inotify_add_watch(mINotifyFd, DEVICE_PATH, IN_DELETE | IN_CREATE);
	    struct epoll_event eventItem;
	    memset(&eventItem, 0, sizeof(eventItem));
	    eventItem.events = EPOLLIN;
	    eventItem.data.u32 = EPOLL_ID_INOTIFY;
	    result = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mINotifyFd, &eventItem);
	    int wakeFds[2];
	    result = pipe(wakeFds);
	 	 mWakeReadPipeFd = wakeFds[0];
	    mWakeWritePipeFd = wakeFds[1];
	    result = fcntl(mWakeReadPipeFd, F_SETFL, O_NONBLOCK);
	    result = fcntl(mWakeWritePipeFd, F_SETFL, O_NONBLOCK);	    eventItem.data.u32 = EPOLL_ID_WAKE;
	    result = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mWakeReadPipeFd, &eventItem);
	}
	
	InputManager::InputManager(
        const sp<EventHubInterface>& eventHub,
        const sp<InputReaderPolicyInterface>& readerPolicy,
        const sp<InputDispatcherPolicyInterface>& dispatcherPolicy) {
    mDispatcher = new InputDispatcher(dispatcherPolicy);
    mReader = new InputReader(eventHub, readerPolicy, mDispatcher);
    initialize();
}

InputManagerService不是Android中传统的WMS AMS类Binder服务，InputManagerService更像是守护线程类服务，监听底层事件，将事件分发给上层需求。 epoll事件轮询输入事件

input有个管道，用来监听ANR ？？[Input系统—ANR原理分析](http://gityuan.com/2017/01/01/input-anr/) 利用Watching-Dog

Phonewindow Actvity Dialog PopWindow的触摸事件响应

Actvity Dialog有Phonwindow，里面有Decorview，DecorView在分发事件的时候，会首先利用Phonwindow的callBack，调用Activity或者Dialog的处理，而普通的是没有的，比如Toast类型的，触摸事件事件直接就会发送到View中去

可以有多个窗口接收触摸事件，比如Activity可以同Popwindow懂事接收触摸事件，

# 如何找到对应的窗口呢，或者说如何找到对应的InputChannal，通过管道发送消息

![整体框架图](http://gityuan.com/images/input/input_summary.jpg) 

[整体框架图](http://gityuan.com/2016/12/31/input-ipc/)

不同版本不一样，低版本用的是管道，高版本用的是本地socket，依托Linux的Android是很灵活的，所以有时候，理解其大概原理就行，因为具体的实现方式可能会不断优化。
WMS 在addWindow的时候，会利用           

	 mInputMonitor.setUpdateInputWindowsNeededLw();

    final InputMonitor mInputMonitor = new InputMonitor(this)
    
       public void updateInputWindowsLw(boolean force) {
        if (!force && !mUpdateInputWindowsNeeded) {
            return;
        }
        mUpdateInputWindowsNeeded = false;

        if (false) Slog.d(WindowManagerService.TAG, ">>>>>> ENTERED updateInputWindowsLw");

        // Populate the input window list with information about all of the windows that
        // could potentially receive input.
        // As an optimization, we could try to prune the list of windows but this turns
        // out to be difficult because only the native code knows for sure which window
        // currently has touch focus.
        final WindowStateAnimator universeBackground = mService.mAnimator.mUniverseBackground;
        final int aboveUniverseLayer = mService.mAnimator.mAboveUniverseLayer;
        boolean addedUniverse = false;

        // If there's a drag in flight, provide a pseudowindow to catch drag input
        final boolean inDrag = (mService.mDragState != null);
        if (inDrag) {
            if (WindowManagerService.DEBUG_DRAG) {
                Log.d(WindowManagerService.TAG, "Inserting drag window");
            }
            final InputWindowHandle dragWindowHandle = mService.mDragState.mDragWindowHandle;
            if (dragWindowHandle != null) {
                addInputWindowHandleLw(dragWindowHandle);
            } else {
                Slog.w(WindowManagerService.TAG, "Drag is in progress but there is no "
                        + "drag window handle.");
            }
        }

        final int NFW = mService.mFakeWindows.size();
        for (int i = 0; i < NFW; i++) {
            addInputWindowHandleLw(mService.mFakeWindows.get(i).mWindowHandle);
        }

        // Add all windows on the default display.
        final int numDisplays = mService.mDisplayContents.size();
        for (int displayNdx = 0; displayNdx < numDisplays; ++displayNdx) {
            WindowList windows = mService.mDisplayContents.valueAt(displayNdx).getWindowList();
            for (int winNdx = windows.size() - 1; winNdx >= 0; --winNdx) {
                final WindowState child = windows.get(winNdx);
                final InputChannel inputChannel = child.mInputChannel;
                final InputWindowHandle inputWindowHandle = child.mInputWindowHandle;
                if (inputChannel == null || inputWindowHandle == null || child.mRemoved) {
                    // Skip this window because it cannot possibly receive input.
                    continue;
                }

                final int flags = child.mAttrs.flags;
                final int privateFlags = child.mAttrs.privateFlags;
                final int type = child.mAttrs.type;

                final boolean hasFocus = (child == mInputFocus);
                final boolean isVisible = child.isVisibleLw();
                final boolean hasWallpaper = (child == mService.mWallpaperTarget)
                        && (type != WindowManager.LayoutParams.TYPE_KEYGUARD);
                final boolean onDefaultDisplay = (child.getDisplayId() == Display.DEFAULT_DISPLAY);

                // If there's a drag in progress and 'child' is a potential drop target,
                // make sure it's been told about the drag
                if (inDrag && isVisible && onDefaultDisplay) {
                    mService.mDragState.sendDragStartedIfNeededLw(child);
                }

                if (universeBackground != null && !addedUniverse
                        && child.mBaseLayer < aboveUniverseLayer && onDefaultDisplay) {
                    final WindowState u = universeBackground.mWin;
                    if (u.mInputChannel != null && u.mInputWindowHandle != null) {
                        addInputWindowHandleLw(u.mInputWindowHandle, u, u.mAttrs.flags,
                                u.mAttrs.privateFlags, u.mAttrs.type,
                                true, u == mInputFocus, false);
                    }
                    addedUniverse = true;
                }

                if (child.mWinAnimator != universeBackground) {
                    addInputWindowHandleLw(inputWindowHandle, child, flags, privateFlags, type,
                            isVisible, hasFocus, hasWallpaper);
                }
            }
        }
 
        mService.mInputManager.setInputWindows(mInputWindowHandles);
 
        clearInputWindowHandlesLw();

     }

    private void addInputWindowHandleLw(final InputWindowHandle windowHandle) {
        if (mInputWindowHandles == null) {
            mInputWindowHandles = new InputWindowHandle[16];
        }
        if (mInputWindowHandleCount >= mInputWindowHandles.length) {
            mInputWindowHandles = Arrays.copyOf(mInputWindowHandles,
                    mInputWindowHandleCount * 2);
        }
        mInputWindowHandles[mInputWindowHandleCount++] = windowHandle;
    }
    
   WMS  addWindow-》updateFocusedWindowLocked-》mInputMonitor.updateInputWindowsLw-》mInputManager.setInputWindows-》NativeInputManager::setInputWindows-》getDispatcher()->setInputWindows
   
也就说窗口变化的时候WMS会将需要获取Input事件的窗口告诉InputManager，之后InputDisPatch就能知道需要将事件发送给哪个窗口

void InputDispatcher::setInputWindows

![](http://img.blog.csdn.net/20141213164750258?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQvamluemh1b2p1bg==/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast)




###  参考文档



[http://gityuan.com/2015/09/19/android-touch/](http://gityuan.com/2015/09/19/android-touch/)