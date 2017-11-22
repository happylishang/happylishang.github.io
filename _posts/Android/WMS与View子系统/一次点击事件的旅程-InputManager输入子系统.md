---
layout: default
title: 一次点击事件的旅程 - InputManager输入子系统 
category: [android]

---


# 如何找到焦点窗口

* 是否支持多窗口获得焦点
* 输入法如何处理
* 系统窗口如何处理

### Linux内核输入子系统

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

Android 事件分发机制详解 <http://stackvoid.com/details-dispatch-onTouch-Event-in-Android/>

[http://gityuan.com/2015/09/19/android-touch/](http://gityuan.com/2015/09/19/android-touch/)