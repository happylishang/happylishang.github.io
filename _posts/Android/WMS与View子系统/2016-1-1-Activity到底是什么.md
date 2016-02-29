---
layout: default
title: Activity到底是什么 
categories: [android]

---

> **分析Android框架的时候谨记：上层都是逻辑封装，包括Activity、View所有的实现均由相应Servcie来处理，比如View的绘制等**


### Activity


Activity是四大组件之一，那么组件到底是个什么东西，虽然知道Activity是用来显示交互界面，可是Activity本身是一个界面的抽象类吗？是View吗，如果不是那么到底谁负责显示，Activity到底扮演什么角色。
Android2.3 采用ViewRoot Android4.3 采用ViewRootImp，其实两者对应，只是采用的不同的类名，ViewRoot也容易混淆。

* ViewRootImp,  WindowManagerImpl,  WindowManagerGlobals
* WindowManagerImpl: 实现了WindowManager 和 ViewManager的接口，但大部分是调用WindowManagerGlobals的接口实现的。

WindowManagerGlobals: 一个SingleTon对象，对象里维护了三个数组：

* mRoots[ ]: 存放所有的docorView
* mViews[ ]: 存放所有的ViewRoot
* mParams[ ]: 存放所有的LayoutParams. 
 同时，它还维护了两个全局IBinder对象，用于访问WindowManagerService 提供的两套接口：

IWindowManager:  主要接口是OpenSession(), 用于在WindowManagerService内部创建和初始化Session, 并返回IBinder对象。
ISession:  是Activity Window与WindowManagerService 进行对话的主要接口.
<img src="http://img.blog.csdn.net/20140713113402196?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQvamluemh1b2p1bg==/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast/" width="900"/>

>ActivityThread  首先启动performLaunchActivity

	public final class ActivityThread {  
	    ......  
	    
	    Instrumentation mInstrumentation;  
	    ......  
	  
	    private final Activity performLaunchActivity(ActivityClientRecord r, Intent customIntent) {  
	        ......  
	  
	        ComponentName component = r.intent.getComponent();  
	        ......  
	        
                try {  
            Application app = r.packageInfo.makeApplication(false, mInstrumentation);  
            ......  
  
            if (activity != null) {  
            
                ContextImpl appContext = new ContextImpl();  
                ......  
                appContext.setOuterContext(activity);  
                ......  
                Configuration config = new Configuration(mConfiguration);  
                ......  
  
                activity.attach(appContext, this, getInstrumentation(), r.token,  
                        r.ident, app, r.intent, r.activityInfo, title, r.parent,  
                        r.embeddedID, r.lastNonConfigurationInstance,  
                        r.lastNonConfigurationChildInstances, config); 
                        
                        
   
      final void attach(Context context, ActivityThread aThread,
            Instrumentation instr, IBinder token, int ident,
            Application application, Intent intent, ActivityInfo info,
            CharSequence title, Activity parent, String id,
            Object lastNonConfigurationInstance,
            HashMap<String,Object> lastNonConfigurationChildInstances,
            Configuration config) {
        attachBaseContext(context);
         //常见窗口
        mWindow = PolicyManager.makeNewWindow(this);
        mWindow.setCallback(this);
        if (info.softInputMode != WindowManager.LayoutParams.SOFT_INPUT_STATE_UNSPECIFIED) {
            mWindow.setSoftInputMode(info.softInputMode);
        }
        
        mUiThread = Thread.currentThread();
        mMainThread = aThread;
        mInstrumentation = instr;
        mToken = token;
        mIdent = ident;
        mApplication = application;
        mIntent = intent;
        mComponent = intent.getComponent();
        mActivityInfo = info;
        mTitle = title;
        mParent = parent;
        mEmbeddedID = id;
        mLastNonConfigurationInstance = lastNonConfigurationInstance;
        mLastNonConfigurationChildInstances = lastNonConfigurationChildInstances;
       
       //Window ->  PhoneWindow  -> setWindowManager 
       
        mWindow.setWindowManager(null, mToken, mComponent.flattenToString());
        if (mParent != null) {
            mWindow.setContainer(mParent.getWindow());
        }
        
       // private WindowManager mWindowManager;

        mWindowManager = mWindow.getWindowManager();
        
        mCurrentConfig = config;
    }                     
	
> Window.java	

	    public void setWindowManager(WindowManager wm,
            IBinder appToken, String appName) {
        mAppToken = appToken;
        mAppName = appName;
        if (wm == null) {
            wm = WindowManagerImpl.getDefault();
        }
        mWindowManager = new LocalWindowManager(wm);
    }
  
>WindowManagerImpl
    
     public static WindowManagerImpl getDefault()
    {
        return mWindowManager;
    }
    
    private static WindowManagerImpl mWindowManager = new WindowManagerImpl();

> Activity
		
	public void setContentView(View view) {
        getWindow().setContentView(view);
    }
    public Window getWindow() {
        return mWindow;
    }

> Activity

        mWindow = PolicyManager.makeNewWindow(this);
        
> Policy
    
    public PhoneWindow makeNewWindow(Context context) {
        return new PhoneWindow(context);
    }
> PhoneWindow
    
       @Override
    public void setContentView(View view) {
        setContentView(view, new ViewGroup.LayoutParams(MATCH_PARENT, MATCH_PARENT));
    }
    
    @Override
    public void setContentView(View view, ViewGroup.LayoutParams params) {
        if (mContentParent == null) {
            installDecor();
        } else {
            mContentParent.removeAllViews();
        }
        mContentParent.addView(view, params);
        final Callback cb = getCallback();
        if (cb != null) {
            cb.onContentChanged();
        }
    }    
    
    private void installDecor() {
        if (mDecor == null) {
            mDecor = generateDecor();
            mDecor.setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);
            mDecor.setIsRootNamespace(true);
        }
        if (mContentParent == null) {
            mContentParent = generateLayout(mDecor);
            
 
    private final class DecorView extends FrameLayout implements RootViewSurfaceTaker {
      
    protected ViewGroup generateLayout(DecorView decor) {
        // Apply data from current theme.

        TypedArray a = getWindowStyle();

        if (false) {
            System.out.println("From style:");
            String s = "Attrs:";
            for (int i = 0; i < com.android.internal.R.styleable.Window.length; i++) {
                s = s + " " + Integer.toHexString(com.android.internal.R.styleable.Window[i]) + "="
                        + a.getString(i);
            }
            System.out.println(s);
        }

        mIsFloating = a.getBoolean(com.android.internal.R.styleable.Window_windowIsFloating, false);
        int flagsToUpdate = (FLAG_LAYOUT_IN_SCREEN|FLAG_LAYOUT_INSET_DECOR)
                & (~getForcedWindowFlags());
        if (mIsFloating) {
            setLayout(WRAP_CONTENT, WRAP_CONTENT);
            setFlags(0, flagsToUpdate);
        } else {
            setFlags(FLAG_LAYOUT_IN_SCREEN|FLAG_LAYOUT_INSET_DECOR, flagsToUpdate);
        }

        if (a.getBoolean(com.android.internal.R.styleable.Window_windowNoTitle, false)) {
            requestFeature(FEATURE_NO_TITLE);
        }

        if (a.getBoolean(com.android.internal.R.styleable.Window_windowFullscreen, false)) {
            setFlags(FLAG_FULLSCREEN, FLAG_FULLSCREEN&(~getForcedWindowFlags()));
        }

        if (a.getBoolean(com.android.internal.R.styleable.Window_windowShowWallpaper, false)) {
            setFlags(FLAG_SHOW_WALLPAPER, FLAG_SHOW_WALLPAPER&(~getForcedWindowFlags()));
        }

        WindowManager.LayoutParams params = getAttributes();

        if (!hasSoftInputMode()) {
            params.softInputMode = a.getInt(
                    com.android.internal.R.styleable.Window_windowSoftInputMode,
                    params.softInputMode);
        }

        if (a.getBoolean(com.android.internal.R.styleable.Window_backgroundDimEnabled,
                mIsFloating)) {
            /* All dialogs should have the window dimmed */
            if ((getForcedWindowFlags()&WindowManager.LayoutParams.FLAG_DIM_BEHIND) == 0) {
                params.flags |= WindowManager.LayoutParams.FLAG_DIM_BEHIND;
            }
            params.dimAmount = a.getFloat(
                    android.R.styleable.Window_backgroundDimAmount, 0.5f);
        }

        if (params.windowAnimations == 0) {
            params.windowAnimations = a.getResourceId(
                    com.android.internal.R.styleable.Window_windowAnimationStyle, 0);
        }

        // The rest are only done if this window is not embedded; otherwise,
        // the values are inherited from our container.
        if (getContainer() == null) {
            if (mBackgroundDrawable == null) {
                if (mBackgroundResource == 0) {
                    mBackgroundResource = a.getResourceId(
                            com.android.internal.R.styleable.Window_windowBackground, 0);
                }
                if (mFrameResource == 0) {
                    mFrameResource = a.getResourceId(com.android.internal.R.styleable.Window_windowFrame, 0);
                }
                if (false) {
                    System.out.println("Background: "
                            + Integer.toHexString(mBackgroundResource) + " Frame: "
                            + Integer.toHexString(mFrameResource));
                }
            }
            mTextColor = a.getColor(com.android.internal.R.styleable.Window_textColor, 0xFF000000);
        }

        // Inflate the window decor.

        int layoutResource;
        int features = getLocalFeatures();
        // System.out.println("Features: 0x" + Integer.toHexString(features));
        if ((features & ((1 << FEATURE_LEFT_ICON) | (1 << FEATURE_RIGHT_ICON))) != 0) {
            if (mIsFloating) {
                layoutResource = com.android.internal.R.layout.dialog_title_icons;
            } else {
                layoutResource = com.android.internal.R.layout.screen_title_icons;
            }
            // System.out.println("Title Icons!");
        } else if ((features & ((1 << FEATURE_PROGRESS) | (1 << FEATURE_INDETERMINATE_PROGRESS))) != 0) {
            // Special case for a window with only a progress bar (and title).
            // XXX Need to have a no-title version of embedded windows.
            layoutResource = com.android.internal.R.layout.screen_progress;
            // System.out.println("Progress!");
        } else if ((features & (1 << FEATURE_CUSTOM_TITLE)) != 0) {
            // Special case for a window with a custom title.
            // If the window is floating, we need a dialog layout
            if (mIsFloating) {
                layoutResource = com.android.internal.R.layout.dialog_custom_title;
            } else {
                layoutResource = com.android.internal.R.layout.screen_custom_title;
            }
        } else if ((features & (1 << FEATURE_NO_TITLE)) == 0) {
            // If no other features and not embedded, only need a title.
            // If the window is floating, we need a dialog layout
            if (mIsFloating) {
                layoutResource = com.android.internal.R.layout.dialog_title;
            } else {
                layoutResource = com.android.internal.R.layout.screen_title;
            }
            // System.out.println("Title!");
        } else {
            // Embedded, so no decoration is needed.
            layoutResource = com.android.internal.R.layout.screen_simple;
            // System.out.println("Simple!");
        }

        mDecor.startChanging();

        View in = mLayoutInflater.inflate(layoutResource, null);
        decor.addView(in, new ViewGroup.LayoutParams(MATCH_PARENT, MATCH_PARENT));

        ViewGroup contentParent = (ViewGroup)findViewById(ID_ANDROID_CONTENT);
        if (contentParent == null) {
            throw new RuntimeException("Window couldn't find content container view");
        }

        if ((features & (1 << FEATURE_INDETERMINATE_PROGRESS)) != 0) {
            ProgressBar progress = getCircularProgressBar(false);
            if (progress != null) {
                progress.setIndeterminate(true);
            }
        }

        // Remaining setup -- of background and title -- that only applies
        // to top-level windows.
        if (getContainer() == null) {
            Drawable drawable = mBackgroundDrawable;
            if (mBackgroundResource != 0) {
                drawable = getContext().getResources().getDrawable(mBackgroundResource);
            }
            mDecor.setWindowBackground(drawable);
            drawable = null;
            if (mFrameResource != 0) {
                drawable = getContext().getResources().getDrawable(mFrameResource);
            }
            mDecor.setWindowFrame(drawable);

            // System.out.println("Text=" + Integer.toHexString(mTextColor) +
            // " Sel=" + Integer.toHexString(mTextSelectedColor) +
            // " Title=" + Integer.toHexString(mTitleColor));

            if (mTitleColor == 0) {
                mTitleColor = mTextColor;
            }

            if (mTitle != null) {
                setTitle(mTitle);
            }
            setTitleColor(mTitleColor);
        }

        mDecor.finishChanging();

        return contentParent;
    }
        
> platform_frameworks_base/core/res/res/layout/screen_title.xml   

	<LinearLayout xmlns:android="http://schemas.android.com/apk/res/android"
	    android:orientation="vertical"
	    android:fitsSystemWindows="true">
	    <!-- Popout bar for action modes -->
	    <ViewStub android:id="@+id/action_mode_bar_stub"
	              android:inflatedId="@+id/action_mode_bar"
	              android:layout="@layout/action_mode_bar"
	              android:layout_width="match_parent"
	              android:layout_height="wrap_content"
	              android:theme="?attr/actionBarTheme" />
	    <FrameLayout
	        android:layout_width="match_parent" 
	        android:layout_height="?android:attr/windowTitleSize"
	        style="?android:attr/windowTitleBackgroundStyle">
	        <TextView android:id="@android:id/title" 
	            style="?android:attr/windowTitleStyle"
	            android:background="@null"
	            android:fadingEdge="horizontal"
	            android:gravity="center_vertical"
	            android:layout_width="match_parent"
	            android:layout_height="match_parent" />
	    </FrameLayout>
	    <FrameLayout android:id="@android:id/content"
	        android:layout_width="match_parent" 
	        android:layout_height="0dip"
	        android:layout_weight="1"
	        android:foregroundGravity="fill_horizontal|top"
	        android:foreground="?android:attr/windowContentOverlay" />
	</LinearLayout>

> 接下来 就会有onCreate onResume流程 handleLauchResume，创建ViewRoot

ViewRoot相当于是MVC模型中的Controller，它有以下职责：

        1. 负责为应用程序窗口视图创建Surface。

        2. 配合WindowManagerService来管理系统的应用程序窗口。

        3. 负责管理、布局和渲染应用程序窗口视图的UI。
        
当Activity组件被激活的时候，系统如果发现与它的应用程序窗口视图对象所关联的ViewRoot对象还没有创建，那么就会先创建这个ViewRoot对象，以便接下来可以将它的UI渲染出来。Activity组件创建完成之后，就可以将它激活起来了，这是通过调用ActivityThread类的成员函数handleResumeActivity来执行的。 从前面Android应用程序窗口（Activity）的窗口对象（Window）的创建过程分析一文可以知道，

>ActivityThread.java  ViewRoot的创建在ActivityThread的 WindowManagerImpl addView地方

     final void handleResumeActivity(IBinder token, boolean clearHide, boolean isForward) {
        // If we are getting ready to gc after going to the background, well
        // we are back active so skip it.
        unscheduleGcIdler();

        ActivityClientRecord r = performResumeActivity(token, clearHide);               ViewManager wm = a.getWindowManager();
                WindowManager.LayoutParams l = r.window.getAttributes();
                a.mDecor = decor;
                l.type = WindowManager.LayoutParams.TYPE_BASE_APPLICATION;
                l.softInputMode |= forwardBit;
                if (a.mVisibleFromClient) {
                    a.mWindowAdded = true;
                    wm.addView(decor, l);
                }
                
LocalWindowManager类的成员变量mWindowManager指向的是一个WindowManagerImpl对

>LocalWindowManager.java     
 
    private void addView(View view, ViewGroup.LayoutParams params, boolean nest)
    {
        if (Config.LOGV) Log.v("WindowManager", "addView view=" + view);

        if (!(params instanceof WindowManager.LayoutParams)) {
            throw new IllegalArgumentException(
                    "Params must be WindowManager.LayoutParams");
        }

        final WindowManager.LayoutParams wparams
                = (WindowManager.LayoutParams)params;
        
        ViewRoot root;
        View panelParentView = null;
        
        synchronized (this) {
            // Here's an odd/questionable case: if someone tries to add a
            // view multiple times, then we simply bump up a nesting count
            // and they need to remove the view the corresponding number of
            // times to have it actually removed from the window manager.
            // This is useful specifically for the notification manager,
            // which can continually add/remove the same view as a
            // notification gets updated.
            int index = findViewLocked(view, false);
            if (index >= 0) {
                if (!nest) {
                    throw new IllegalStateException("View " + view
                            + " has already been added to the window manager.");
                }
                root = mRoots[index];
                root.mAddNesting++;
                // Update layout parameters.
                view.setLayoutParams(wparams);
                root.setLayoutParams(wparams, true);
                return;
            }
            
            // If this is a panel window, then find the window it is being
            // attached to for future reference.
            if (wparams.type >= WindowManager.LayoutParams.FIRST_SUB_WINDOW &&
                    wparams.type <= WindowManager.LayoutParams.LAST_SUB_WINDOW) {
                final int count = mViews != null ? mViews.length : 0;
                for (int i=0; i<count; i++) {
                    if (mRoots[i].mWindow.asBinder() == wparams.token) {
                        panelParentView = mViews[i];
                    }
                }
            }
            
            root = new ViewRoot(view.getContext());      
            ... 
                  // do this last because it fires off messages to start doing things
        root.setView(view, wparams, panelParentView);  
        
        
### ViewRoot本质   root.setView(
      
      public final class ViewRoot extends Handler implements ViewParent,
        View.AttachInfo.Callbacks {
              
ViewRoot是GUI管理系统与GUI呈现系统之间的桥梁，根据ViewRoot的定义，我们发现它并不是一个View类型，而是一个Handler。ViewRoot这个类在android的UI结构中扮演的是一个中间者的角色，连接的是PhoneWindow跟WindowManagerService，

它的主要作用如下：

A. 向DecorView分发收到的用户发起的event事件，如按键，触屏，轨迹球等事件；

> ViewRoot.java   通过IWindowSession与WindowManagerService交互，完成整个Activity的GUI的绘制。


    public static IWindowSession getWindowSession(Looper mainLooper) {
        synchronized (mStaticInit) {
            if (!mInitialized) {
                try {
                    InputMethodManager imm = InputMethodManager.getInstance(mainLooper);
                    sWindowSession = IWindowManager.Stub.asInterface(
                            ServiceManager.getService("window"))
                            .openSession(imm.getClient(), imm.getInputContext());
                    mInitialized = true;
                } catch (RemoteException e) {
                }
            }
            return sWindowSession;
        }
    }
  
      
    public void setView(View view, WindowManager.LayoutParams attrs,
            View panelParentView) {
            void requestLayout() 
            
            ...
           mInputChannel = new InputChannel();
                try {
                    res = sWindowSession.add(mWindow, mWindowAttributes,
                            getHostVisibility(), mAttachInfo.mContentInsets,
                            mInputChannel);
            
            
   
       public void requestLayout() {
        checkThread();
        mLayoutRequested = true;
        scheduleTraversals();
    }
 
 
     private void performTraversals() {
        // cache mView since it is used so much below...
        final View host = mView;

注意performTraversals完完全全在UI线程中，所以布局是否合理直接影响用户体验。

> WindowManagerService通过W与Activity的Window交互，完成手势派发等等。   
 
    static class W extends IWindow.Stub {
        private final WeakReference<ViewRoot> mViewRoot;

        public W(ViewRoot viewRoot, Context context) {
            mViewRoot = new WeakReference<ViewRoot>(viewRoot);
        }

在这个方法中只需要关注两个步骤

* requestLayout()

  请求WindowManagerService绘制GUI，但是注意一点的是它是在与WindowManagerService建立连接之前绘制，为什么要在建立之前请求绘制呢？其实两者实际的先后顺序是正好相反的，与WMS建立连接在前，绘制GUI在后，那么为什么代码的顺序和执行的顺序不同呢？这里就涉及到ViewRoot的属性了，我们前面提到ViewRoot并不是一个View，而是一个Handler，那么执行的具体流程就是这样的：
    从字面意思理解的话，IWindowSession sWindowSessoin是ViewRoot和WindowManagerService之间的一个会话层，它的实体是在WMS中定义，作为ViewRoot requests WMS的桥梁。

add()方法的第一个参数mWindow是ViewRoot提供给WMS，以便WMS反向通知ViewRoot的接口。由于ViewRoot处在application端，而WMS处在system_server进程，它们处在不同的进程间，因此需要添加这个IWindow接口便于GUI绘制状态的同步。

a)  ActivityThread的handler函数注册了启动一个新的Activity的请求处理LAUNCH_ACTIVITY，LAUNCH_ACTIVITY的处理过程调用到了ViewRoot的setView()方法，因此上图代码在被执行时正处于LAUNCH_ACTIVITY消息的处理过程中。

b)   requestLayout()其实是向messagequeue发送了一个请求绘制GUI的消息，并且ViewRoot和ActivityThread共用同一个MessageQueue(如下图)，因此绘制GUI的过程一定是在LAUNCH_ACTIVITY消息被处理完之后，也就是sWindowSessoin.add()方法调用完之后。


* sWindowSessoin.add()

从字面意思理解的话，IWindowSession sWindowSessoin是ViewRoot和WindowManagerService之间的一个会话层，它的实体是在WMS中定义，作为ViewRoot requests WMS的桥梁。

add()方法的第一个参数mWindow是ViewRoot提供给WMS，以便WMS反向通知ViewRoot的接口。由于ViewRoot处在application端，而WMS处在system_server进程，它们处在不同的进程间，因此需要添加这个IWindow接口便于GUI绘制状态的同步。

![](http://hi.csdn.net/attachment/201111/10/0_13209336991GIN.gif)


> WindowServiceManager


            win = new WindowState(session, client, token,
                    attachedWindow, attrs, viewVisibility);
                    
  
### WindowToken与WindowState
  
WindowToken是一个句柄，保存了所有具有同一个token的WindowState。应用请求WindowManagerService添加窗口的时候，提供了一个token，该token标识了被添加窗口的归属，WindowManagerService为该token生成一个WindowToken对象，所有token相同的WindowState被关联到同一个WindowToken。如输入法添加窗口时，会传递一个mCurrToken，墙纸服务添加窗口时，会传递一个newConn.mToken。

        mService.mWindowManager.addAppToken(addPos, r, r.task.taskId,  
                            r.info.screenOrientation, r.fullscreen);  
                      
        r是ActivityRecord类                 
        class ActivityRecord extends IApplicationToken.Stub {

AppWindowToken继承于WindowToken，专门用于标识一个Activity。AppWindowToken里的token实际上就是指向了一个Activity。ActivityManagerService通知应用启动的时候，在服务端生成一个token用于标识该Activity，其实是ActivityRecord，并且把该token传递到应用客户端，客户端的Activity在申请添加窗口时，以该token作为标识传递到WindowManagerService。同一个Activity中的主窗口、对话框窗口、菜单窗口都关联到同一个AppWindowToken。
                  
如果是应用窗口，通过 token 检索出来的 WindowToken，一定不能为空，而且还必须是 Activity 的 mAppToken，同时对应的 Activity 还必须是没有被 finish。之前分析 Activity 的启动过程我们知道，Activity 在启动过程中，会先通过 WmS 的 addAppToken( )添加一个 AppWindowToken 到 mTokenMap 中，其中 key 就用了 IApplicationToken token。而 Activity 中的 mToken，以及 Activity 对应的 PhoneWindow 中的 mAppToken 就是来自 AmS 的 token (代码见 Activity 的 attach 方法)。

如果是子窗口，会通过 attrs.token 去通过 windowForClientLocked 查找其父窗口，如果找不到其父窗口，会抛出异常。或者如果找到的父窗口的类型还是子窗口类型，也会抛出异常。这里查找父窗口的过程，是直接取了 attrs.token 去 mWindowMap 中找对应的 WindowState，而 mWindowMap 中的 key 是 IWindow。所以，由此可见，创建一个子窗口类型，token 必须赋值为其父窗口的 ViewRootImpl 中的 W 类对象 mWindow。

WindowState 创建后，会以 IWindow 为 key (对应应用进程中的 ViewRootImpl.W 类对象 mWindow，重要的事强调多遍！！)，添加到 mWindowMap 中。                  


    private final class WindowState implements WindowManagerPolicy.WindowState {
        final Session mSession;
        final IWindow mClient;
        WindowToken mToken;
        WindowToken mRootToken;
        AppWindowToken mAppToken;
        AppWindowToken mTargetAppToken;
        
上面的部分足够各部分通信使用
       
### 参考文档

图解Android - Android GUI 系统 (2) - 窗口管理 (View, Canvas, Window Manager) <http://www.cnblogs.com/samchen2009/p/3367496.html>

** Android 4.4(KitKat)窗口管理子系统 - 体系框架** <http://blog.csdn.net/jinzhuojun/article/details/37737439>***（推荐）***

浅析Android的窗口  <http://bugly.qq.com/bbs/forum.php?mod=viewthread&tid=555>** [赞]**
  
