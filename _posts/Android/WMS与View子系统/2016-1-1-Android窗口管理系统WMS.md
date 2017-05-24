---
layout: default
title: Android 窗口管理系统入门 
categories: [android]

---


# Activity是Android的显示封装，Service是服务封装，BroaderCast是通知封装，而ContentProvider是持久化数据库共享封装

## Android 窗口管理系统入门 

> **分析Android框架的时候谨记：上层都是逻辑封装，包括Activity、View，所有的实现均有相应Servcie来处理，比如View的绘制等**


**（1）WmS 眼中的，窗口是可以显示用来显示的 View。对于 WmS 而言，所谓的窗口就是一个通过 WindowManagerGlobal.addView()添加的 View 罢了；<br>
（2）Window 类是一个针对窗口交互的抽象，也就是对于 WmS 来讲所有的用户消息是直接交给 View/ViewGroup 来处理的。而 Window 类把一些**交互从 View/ViewGroup 中抽离出来，定义了一些窗口的行为，例如菜单，以及处理系统按钮，如“Home”，“Back”等等。

IWindow继承自Binder，并且其Bn端位于应用程序一侧（在例子中IWindow的实现类MyWindow就继承自IWindow.Stub），于是其在WMS一侧只能作为一个回调，以及起到窗口Id的作用。
那么，窗口的本质是什么呢？
是进行绘制所使用的画布：Surface。
当一块Surface显示在屏幕上时，就是用户所看到的窗口了。客户端向WMS添加一个窗口的过程，其实就是WMS为其分配一块Surface的过程，一块块Surface在WMS的管理之下有序地排布在屏幕上，Android才得以呈现出多姿多彩的界面来。所以从这个意义上来讲，**WindowManagerService被称之为SurfaceManagerService也说得通的**。


WindowToken指代一个应用组件。例如在进行窗口ZOrder排序时，属于同一个WindowToken的窗口会被安排在一起，而且在其中定义的一些属性将会影响所有属于此WindowToken的窗口。这些都表明了属于同一个WindowToken的窗口之间的紧密联系

AMS通过ActivityRecord表示一个Activity。而ActivityRecord的appToken在其构造函数中被创建，所以每个ActivityRecord拥有其各自的appToken。而WMS接受AMS对Token的声明，并为appToken创建了唯一的一个AppWindowToken。因此，这个类型为IApplicationToken的Binder对象appToken粘结了AMS的ActivityRecord与WMS的AppWindowToken，只要给定一个ActivityRecord，都可以通过appToken在WMS中找到一个对应的AppWindowToken，从而使得AMS拥有了操纵Activity的窗口绘制的能力。例如，当AMS认为一个Activity需要被隐藏时，以Activity对应的ActivityRecord所拥有的appToken作为参数调用WMS的setAppVisibility()函数。此函数通过appToken找到其对应的AppWindowToken，然后将属于这个Token的所有窗口隐藏。
注意**每当AMS因为某些原因（如启动/结束一个Activity，或将Task移到前台或后台）而调整ActivityRecord在mHistory中的顺序时，都会调用WMS相关的接口移动AppWindowToken在mAppTokens中的顺序，以保证两者的顺序一致**。在后面讲解窗口排序规则时会介绍到，AppWindowToken的顺序对窗口的顺序影响非常大。


# 对于WMS的客户端来说，Token仅仅是一个Binder对象而已

# WindowState与WindowToken区别

* WindowState表示一个窗口的所有属性，所以它是WMS中事实上的窗口
* WindowToken具有令牌的作用，是对应用组件的行为进行规范管理的一个手段。

![一个正在回放视频并弹出两个对话框的Activity为例](http://img.blog.csdn.net/20150814130611265?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQv/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/Center)
// 可以看出，WMS无法直接管理View，只能通过mWindow，至于Window里面到底是什么，不关系
// WMS应该只关心Window的管理（添加Window、删除Window、切换Window、Window发生过）
// 至于View如何绘制，那是每个Window自己管理的事情

说明对比一下mTokenMap和mWindowMap。这两个HashMap维护了WMS中最重要的两类数据：WindowToken及WindowState。它们的键都是IBinder，区别是： mTokenMap的键值可能是IAppWindowToken的Bp端（使用addAppToken()进行声明），或者是其他任意一个Binder的Bp端(使用addWindowToken()进行声明)；而mWindowMap的键值一定是IWindow的Bp端


    res = mWindowSession.addToDisplay(mWindow, mSeq, mWindowAttributes,
            getHostVisibility(), mDisplay.getDisplayId(),
            mAttachInfo.mContentInsets, mInputChannel);

与窗口交互的几个关键函数

	interface IWindowSession {
	    int add(IWindow window, int seq, in WindowManager.LayoutParams attrs,
	            in int viewVisibility, out Rect outContentInsets,
	            out InputChannel outInputChannel);
	
	    int addToDisplay(IWindow window, int seq, in WindowManager.LayoutParams attrs,
	                     in int viewVisibility, in int layerStackId, out Rect outContentInsets,
	                     out InputChannel outInputChannel);
	    int addWithoutInputChannel(IWindow window, int seq, in WindowManager.LayoutParams attrs,
	                               in int viewVisibility, out Rect outContentInsets);
	    int addToDisplayWithoutInputChannel(IWindow window, int seq, in WindowManager.LayoutParams attrs,
	
	    void remove(IWindow window);
	
	    int relayout(IWindow window, int seq, in WindowManager.LayoutParams attrs,
	                 int requestedWidth, int requestedHeight, int viewVisibility,
	                 int flags, out Rect outFrame, out Rect outOverscanInsets,
	                 out Rect outContentInsets, out Rect outVisibleInsets,
	                 out Configuration outConfig, out Surface outSurface);
	
	    void performDeferredDestroy(IWindow window);
	
	    boolean outOfMemory(IWindow window);
	
	    void setTransparentRegion(IWindow window, in Region region);
	
	    void setInsets(IWindow window, int touchableInsets, in Rect contentInsets,
	                   in Rect visibleInsets, in Region touchableRegion);
	
	    void getDisplayFrame(IWindow window, out Rect outDisplayFrame);
	
	    void onRectangleOnScreenRequested(IBinder token, in Rect rectangle, boolean immediate);
	
	    IWindowId getWindowId(IBinder window);
	}
	
从与WMS交互的函数中，可以看到其实并未涉及View的测量重回之类的逻辑。	
	                            
### 目录

* 窗口和图形系统 - Window and View Manager System.
* 显示合成系统 - Surface Flinger
* 用户输入系统 - InputManager System
* 应用框架系统 - Activity Manager System.
* 何时创建窗口performlauch
* Theme的设定，AMS在启动的时候就获取了style，然后在ActivityThread启动Activity的时候Lauch的时候，会主动将Themeid设置进去，其实APK在编译的时候，资源就已经编号了。
* 资源编号的的bundle
* 状态栏的创建

# 几种Window的区别

可以看见上面无论Acitivty、Dialog、PopWindow、Toast的实质其实都是如下接口提供的方法操作：

	public interface ViewManager
	{
	    public void addView(View view, ViewGroup.LayoutParams params);
	    public void updateViewLayout(View view, ViewGroup.LayoutParams params);
	    public void removeView(View view);
	}
	
整个应用各种窗口的显示都离不开这三个方法而已，只是token及type与Window是否共用的问题。

### 导读，问题引入原理

我们知道，启动一个Activity，之后setContentView之后，就可以显示界面了，那么具体的实现是怎么样子的，界面的绘制是在当前进程吗，还是由那个服务来完成的，set后的后续处理如何做到，view的布局如何解析并绘制的，


#### 窗口的理解

窗口其实也是独立的东西，只是同Activity绑定，位于Application中，其实将来也许窗口会同其他的Service或者管理方式结合，AMS管理窗口，不关心窗口在哪，其实对AMS完全不知情，窗口WMS也是独立的，也有系统窗口，WMS管理，但是，系统窗口不属于任何Activity界面，窗口的管理，交给窗口。比如悬浮球

		            

### 添加窗口   onResume的时候，保证DocView一定创建，其实就算不setContentView也有界面显示，因此，还有其他入口的。

不过窗口在WMS的管理与添加是	reusme流程里面做的，create的里面只是用了本地需要的东西，如果不显示是不会参与窗口交互。

	      

#### 移除窗口

窗口的管理独立于AMS，ActivityTHread根据AMS，再次与WMS交互。WMS其实更独立，SysytemUI，其实就不是线性的APP，没有Activity界面，但是任然可以呈现StatusBar

开始于detroy,否则保留，以便于恢复，

    private void handleDestroyActivity(IBinder token, boolean finishing,
            int configChanges, boolean getNonConfigInstance) {
        ActivityClientRecord r = performDestroyActivity(token, finishing,
                configChanges, getNonConfigInstance);
        if (r != null) {
            cleanUpPendingRemoveWindows(r);
            WindowManager wm = r.activity.getWindowManager();
            View v = r.activity.mDecor;
            if (v != null) {
                if (r.activity.mVisibleFromServer) {
                    mNumVisibleActivities--;
                }
                IBinder wtoken = v.getWindowToken();
                if (r.activity.mWindowAdded) {
                    if (r.onlyLocalRequest) {
                        // Hold off on removing this until the new activity's
                        // window is being added.
                        r.mPendingRemoveWindow = v;
                        r.mPendingRemoveWindowManager = wm;
                    } else {
                        wm.removeViewImmediate(v);
                    }
                }
                if (wtoken != null && r.mPendingRemoveWindow == null) {
                    WindowManagerGlobal.getInstance().closeAll(wtoken,
                            r.activity.getClass().getName(), "Activity");
                }
                r.activity.mDecor = null;
            }
            
 如果被移除，View post的很多Runable就无法执行，如果牵扯到内存泄露，那就会很麻烦。
            
	   public void closeAll(IBinder token, String who, String what) {
	        synchronized (mLock) {
	            if (mViews == null)
	                return;
	
	            int count = mViews.length;
	            //Log.i("foo", "Closing all windows of " + token);
	            for (int i=0; i<count; i++) {
	                //Log.i("foo", "@ " + i + " token " + mParams[i].token
	                //        + " view " + mRoots[i].getView());
	                if (token == null || mParams[i].token == token) {
	                    ViewRootImpl root = mRoots[i];
	
	                    //Log.i("foo", "Force closing " + root);
	                    if (who != null) {
	                        WindowLeaked leak = new WindowLeaked(
	                                what + " " + who + " has leaked window "
	                                + root.getView() + " that was originally added here");
	                        leak.setStackTrace(root.getLocation().getStackTrace());
	                        Log.e(TAG, leak.getMessage(), leak);
	                    }
	
	                    removeViewLocked(i, false);
	                    i--;
	                    count--;
	                }
	            }
	        }
	    }
	    
	    
	    
        Call<PhoneResult> call = service.getResult("3ce2066cc7c59d8d602dd9d743e449a5", 
        
           Call<PhoneResult> getResult(@Header("apikey") String apikey, @Query("phone") String phone);
	    

# AMS 与WMS交互  mService.mWindowManager.addAppToken(

注意AMS与WMS对象在同一个SystemServer进程


            wm = WindowManagerService.main(context, power, display, inputManager,
                    uiHandler, wmHandler,
                    factoryTest != SystemServer.FACTORY_TEST_LOW_LEVEL,
                    !firstBoot, onlyCore);
            ServiceManager.addService(Context.WINDOW_SERVICE, wm);
            ServiceManager.addService(Context.INPUT_SERVICE, inputManager);

            ActivityManagerService.self().setWindowManager(wm);
            
            

    private final void startActivityLocked(ActivityRecord r, boolean newTask,
            boolean doResume, boolean keepCurTransition, Bundle options) {
        final int NH = mHistory.size();

        int addPos = -1;
        
        if (!newTask) {
            // If starting in an existing task, find where that is...
            boolean startIt = true;
            for (int i = NH-1; i >= 0; i--) {
                ActivityRecord p = mHistory.get(i);
                if (p.finishing) {
                    continue;
                }
                if (p.task == r.task) {
                    // Here it is!  Now, if this is not yet visible to the
                    // user, then just add it without starting; it will
                    // get started when the user navigates back to it.
                    addPos = i+1;
                    if (!startIt) {
                        if (DEBUG_ADD_REMOVE) {
                            RuntimeException here = new RuntimeException("here");
                            here.fillInStackTrace();
                            Slog.i(TAG, "Adding activity " + r + " to stack at " + addPos,
                                    here);
                        }
                        mHistory.add(addPos, r);
                        r.putInHistory();
                        mService.mWindowManager.addAppToken(addPos, r.appToken, r.task.taskId,
                                r.info.screenOrientation, r.fullscreen,
                                (r.info.flags & ActivityInfo.FLAG_SHOW_ON_LOCK_SCREEN) != 0);
                        if (VALIDATE_TOKENS) {
                            validateAppTokensLocked();
                        }
                        ActivityOptions.abort(options);
                        return;
                    }
                    break;
                }
                if (p.fullscreen) {
                    startIt = false;
                }
            }
        }
        
        
	          
           
### 参考文档

 [图解Android - Android GUI 系统 (2) - 窗口管理 (View, Canvas, Window Manager)](http://www.cnblogs.com/samchen2009/p/3367496.html)      
 [Android 4.4(KitKat)窗口管理子系统 - 体系框架](http://blog.csdn.net/jinzhuojun/article/details/37737439)   
 [Android桌面悬浮窗效果实现，仿360手机卫士悬浮窗效果] (http://blog.csdn.net/guolin_blog/article/details/8689140)    
 [ Android应用Activity、Dialog、PopWindow、Toast窗口添加机制及源码分析](http://blog.csdn.net/yanbober/article/details/46361191)