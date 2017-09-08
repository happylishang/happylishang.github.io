---
layout: post
title: Android窗口管理分析（3）：WMS窗口的组织形式
category: Android
image: http://upload-images.jianshu.io/upload_images/1460468-26d924e59a4b00f8.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---


在Android系统中，窗口是有分组概念的，例如，Activity中弹出的所有PopupWindow会随着Activity的隐藏而隐藏，可以说这些都附属于Actvity的子窗口分组，对于Dialog也同样如此，只不过Dialog与Activity属于同一个分组。之间已经简单介绍了窗口类型划分：应用窗口、子窗口、系统窗口，Activity与Dialog都属于应用窗口，而PopupWindow属于子窗口，Toast、输入法等属于系统窗口。只有应用窗口与系统窗口可以作为父窗口，子窗口不能作为子窗口的父窗口，也就说Activity与Dialog或者系统窗口中可以弹出PopupWindow，但是PopupWindow不能在自己内部弹出PopupWindow子窗口。日常开发中，一些常见的问题都同窗口的分组有关系，比如为什么新建Dialog的时候必须要用Activity的Context，而不能用Application的；为什么不能以PopupWindow的View为锚点弹出子PopupWindow？其实这里面就牵扯都Android的窗口组织管理形式，本文主要包含一下几点：

*  窗口的分组管理 ：应用窗口组、子窗口组、系统窗口组
*  Activity、Dialg应用窗口及PopWindow子窗口的添加原理跟注意事项
*  窗口的Z次序管理：窗口的分配序号、次序调整等
*  WMS中窗口次序分配如何影响SurfaceFlinger服务


在[WMS窗口添加一文](http://www.jianshu.com/p/e4b19fc36a0e)中分析过,窗口的添加是通过WindowManagerGlobal.addView()来完成 函数原型如下

	public void addView(View view, ViewGroup.LayoutParams params,
	            Display display, Window parentWindow)
            

前三个参数是必不可少的，view、params、display，其中display表示要输出的显示设备，先不考虑。view 就是APP要添加到WindowManagerGlobal管理的View，而 params是WindowManager.LayoutParams，主要用来描述窗口属性，WindowManager.LayoutParams有两个很重要的参数type与token，

    public static class LayoutParams extends ViewGroup.LayoutParams
            implements Parcelable {
      ...
      public int type;
      ...
      public IBinder token = null;
      
      }
      
type用来描述窗口的类型，而token其实是标志窗口的分组，token相同的窗口属于同一分组，后面会知道这个token其实是WMS在APP端对应的一个WindowToken的键值。这里先看一下type参数，之前曾添加过Toast窗口，它的type值是TYPE_TOAST，标识是一个系统提示窗口，下面先简单看下三种窗口类型的Type对应的值，首先看一下应用窗口

窗口TYPE值           |窗口类型      |  
--------------------|------------------| 
FIRST_APPLICATION_WINDOW = 1 | 开始应用程序窗口    | 
TYPE_BASE_APPLICATION=1|所有程序窗口的base窗口，其他应用程序窗口都显示在它上面  |
 TYPE_APPLICATION    =2 |普通应用程序窗口，token必须设置为Activity的token  |
TYPE_APPLICATION_STARTING =3|应用程序启动时所显示的窗口|
LAST_APPLICATION_WINDOW = 99|结束应用程序窗口|

一般Activity都是TYPE_BASE_APPLICATION类型的，而TYPE_APPLICATION主要是用于Dialog，再看下子窗口类型
 
 窗口TYPE值           |窗口类型      |  
--------------------|------------------| 
FIRST_SUB_WINDOW = 1000 | SubWindows子窗口，子窗口的Z序和坐标空间都依赖于他们的宿主窗口    | 
TYPE_APPLICATION_PANEL =1000| 面板窗口，显示于宿主窗口的上层 |
 TYPE_APPLICATION_MEDIA    =1001 |媒体窗口（例如视频），显示于宿主窗口下层|
TYPE_APPLICATION_SUB_PANEL =1002|应用程序窗口的子面板，显示于所有面板窗口的上层|
TYPE_APPLICATION_ATTACHED_DIALOG = 1003 |对话框，类似于面板窗口，绘制类似于顶层窗口，而不是宿主的子窗口|
TYPE_APPLICATION_MEDIA_OVERLAY =1004|媒体信息，显示在媒体层和程序窗口之间，需要实现半透明效果|
LAST_SUB_WINDOW=1999 |结束子窗口|

最后看几个系统窗口类型，

 窗口TYPE值          |窗口类型           |  
--------------------|------------------| 
FIRST_SYSTEM_WINDOW = 2000 | 系统窗口| 
TYPE_STATUS_BAR     = FIRST_SYSTEM_WINDOW| 状态栏|
TYPE_SYSTEM_ALERT   = FIRST_SYSTEM_WINDOW+3 |系统提示，出现在应用程序窗口之上|
TYPE_TOAST          = FIRST_SYSTEM_WINDOW+5|显示Toast|


了解窗口类型后，我们需要面对的首要问题是：**窗口如何根据类型进行分组归类的？Dialog是如何确定附属Activity，PopupWindow如何确定附属父窗口？**。日常开发中，一些常见的BUG都同窗口分组有关系，比如为什么新建Dialog的时候必须要用Activity的Context，而不能用Application的Context；在PopupWindow中，不能以它的View为锚点弹出子PopupWindow？其实这里面就牵扯都Android的窗口组织管理形式。

## 窗口的分组原理

如果用一句话概括窗口分组的话：**Android窗口是以token来进行分组的**，同一组窗口握着相同的token，什么是token呢？在 Android WMS管理框架中，token一个IBinder对象，IBinder在实体端与代理端会相互转换，这里只看实体端，它的取值只有两种:ViewRootImpl中ViewRootImpl.W，或者是ActivityRecord中的IApplicationToken.Stub对象，其中ViewRootImpl.W的实体对象在ViewRootImpl中实例化，而IApplicationToken.Stub在ActivityManagerService端实例化，之后被AMS添加到WMS服务中去，作为Activity应用窗口的键值标识。之前说过Activity跟Dialog属于同一分组，现在就来看一下Activity跟Dialog的token是如何复用的，这里的复用分为APP端及WMS服务端，关于窗口的添加流程之前已经分析过，这里只跟随窗口token来分析窗口的分组，我们知道在WMS端，WindowState与窗口的一一对应，而WindowToken与窗口分组，这可以从两者的定义看出如下：

	class WindowToken {
	
		final WindowManagerService service;
		final IBinder token;
		final int windowType;
		final boolean explicit;
		<!--当前窗口对应appWindowToken，是不是同Activity存在依附关系-->
		AppWindowToken appWindowToken;
		<!--关键点1 当前WindowToken对应的窗口列表-->
		final WindowList windows = new WindowList();
		...
	}

	final class WindowState implements WindowManagerPolicy.WindowState {
	    static final String TAG = "WindowState";
	
	    final WindowManagerService mService;
	    final WindowManagerPolicy mPolicy;
	    final Context mContext;
	    final Session mSession;
	    <!--当前WindowState对应IWindow窗口代理-->
	    final IWindow mClient;
	    <!--当前WindowState对应的父窗口-->
	    final WindowState mAttachedWindow;
	    ...
	    <!--当前WindowState隶属的token-->
	    WindowToken mToken;
	    WindowToken mRootToken;
	    AppWindowToken mAppToken;
	    AppWindowToken mTargetAppToken;
	    ...
	    }

可以看到WindowToken包含一个 WindowList windows = new WindowList()，其实就是WindowState列表；而WindowState有一个WindowToken mToken，也就是WindowToken包含一个WindowState列表，而每个WindowState附属一个WindowToken窗口组，示意图如下：
	    
![WindowToken与WindowState关系.jpg](http://upload-images.jianshu.io/upload_images/1460468-8e9cfc3cc05ba860.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


### Activity对应token及WindowToken（AppWindowToken）的添加

AMS在为Activity创建ActivityRecord的时候，会新建IApplicationToken.Stub appToken对象，在startActivity之前会首先向WMS服务登记当前Activity的Token，随后，通过Binder通信将IApplicationToken传递给APP端，在通知ActivityThread新建Activity对象之后，利用Activity的attach方法添加到Activity中，先看第一步AMS将Activity的token加入到WMS中，并且为Activity创建APPWindowToken。
	
	<!--AMS ActivityStack.java中代码 -->
	 final void startActivityLocked(ActivityRecord r, boolean newTask,
	            boolean doResume, boolean keepCurTransition, Bundle options) {
	    ...<!--关键点1  添加Activity token到WMS-->
	    mWindowManager.addAppToken(task.mActivities.indexOf(r), r.appToken,XXX);
       }
       
   <!-- WMS 代码 -->
   
	  @Override
	    public void addAppToken(int addPos, IApplicationToken token, int taskId, int stackId,
	            int requestedOrientation, boolean fullscreen, boolean showForAllUsers, int userId,
	            int configChanges, boolean voiceInteraction, boolean launchTaskBehind) {
		         synchronized(mWindowMap) {
		         <!--新建AppWindowToken-->
	            AppWindowToken atoken = findAppWindowToken(token.asBinder());
	            atoken = new AppWindowToken(this, token, voiceInteraction);
	            ...
	            <!--将AppWindowToken以IApplicationToken.Stub为键值放如WMS的mTokenMap中-->
	            mTokenMap.put(token.asBinder(), atoken);
	            <!--开始肯定是隐藏状态，因为还没有resume-->
	            atoken.hidden = true;
	            atoken.hiddenRequested = true;
	        }
	    }
	    
也就是说Activity分组的Token其实是早在Activity显示之前就被AMS添加到WMS中去的，之后AMS才会通知App端去新建Activity，并将Activity的Window添加到WMS中去，接着看下APP端的流程：
    
    private Activity performLaunchActivity(ActivityClientRecord r, Intent customIntent) {
        <!--关键点1 新建Activity-->
        Activity activity = null;
        try {
            java.lang.ClassLoader cl = r.packageInfo.getClassLoader();
            activity = mInstrumentation.newActivity(
                    cl, component.getClassName(), r.intent);
          ...
       try {
            Application app = r.packageInfo.makeApplication(false, mInstrumentation);
            if (activity != null) {
            <!--关键点2 新建appContext-->
                Context appContext = createBaseContextForActivity(r, activity);
                CharSequence title = r.activityInfo.loadLabel(appContext.getPackageManager());
                Configuration config = new Configuration(mCompatConfiguration);
			<!--关键点3 attach到WMS-->
               activity.attach(appContext, this, getInstrumentation(), r.token，XXX);
          	...
          } 
          
关键点1，新建一个Activity，之后会为Activiyt创建一个appContext，这个Context主要是为了activity.attach使用的，其实就是单纯new一个ContextImpl，之后Activity会利用attach函数将ContextImpl绑定到自己身上。
                     
    static ContextImpl createActivityContext(ActivityThread mainThread,
            LoadedApk packageInfo, int displayId, Configuration overrideConfiguration) {
        return new ContextImpl(null, mainThread, packageInfo, null, null, false,
                null, overrideConfiguration, displayId);
    }
    
	 final void attach(Context context, ActivityThread aThread,
	            Instrumentation instr, IBinder token, int ident,
	            Application application, Intent intent, ActivityInfo info,
	            CharSequence title, Activity parent, String id,
	            NonConfigurationInstances lastNonConfigurationInstances,
	            Configuration config, String referrer, IVoiceInteractor voiceInteractor) {
	        <!--关键点1 为Activity绑定ContextImpl 因为Activity只是一个ContextWraper-->
	        attachBaseContext(context);
	        mFragments.attachHost(null /*parent*/);
	        <!--关键点2 new一个PhoneWindow 并设置回调-->
	        mWindow = new PhoneWindow(this);
	        mWindow.setCallback(this);
	        mWindow.setOnWindowDismissedCallback(this);
	        mWindow.getLayoutInflater().setPrivateFactory(this);
	        ...
	        <!--关键点3 Token的传递-->
	        mToken = token;
	        mIdent = ident;
	        mApplication = application;
	        ...
	        mWindow.setWindowManager(
	                (WindowManager)context.getSystemService(Context.WINDOW_SERVICE),
	                mToken, mComponent.flattenToString(),
	                (info.flags & ActivityInfo.FLAG_HARDWARE_ACCELERATED) != 0);
	        <!--将Window的WindowManager赋值给Activity-->
	        mWindowManager = mWindow.getWindowManager();
	        mCurrentConfig = config;
	    }

mWindow.setWindowManager并不是直接为Window设置WindowManagerImpl，而是利用当前的WindowManagerImpl重新为Window创建了一个WindowManagerImpl，并将自己设置此WindowManagerImpl的parentWindow：

    public void setWindowManager(WindowManager wm, IBinder appToken, String appName,
            boolean hardwareAccelerated) {
        mAppToken = appToken;
        mAppName = appName;
        mHardwareAccelerated = hardwareAccelerated
                || SystemProperties.getBoolean(PROPERTY_HARDWARE_UI, false);
        if (wm == null) {
            wm = (WindowManager)mContext.getSystemService(Context.WINDOW_SERVICE);
        }
        mWindowManager = ((WindowManagerImpl)wm).createLocalWindowManager(this);
    }
    
     public WindowManagerImpl createLocalWindowManager(Window parentWindow) {
        return new WindowManagerImpl(mDisplay, parentWindow);
    }
    
之后将Window的WindowManagerImpl传递给Activity，作为Activity的WindowManager将来Activity通过getSystemService获取WindowManager服务的时候，其实是直接返回了Window的WindowManagerImpl，
 
    @Override
    public Object getSystemService(String name) {

        if (WINDOW_SERVICE.equals(name)) {
            return mWindowManager;
        } else if (SEARCH_SERVICE.equals(name)) {
            ensureSearchManager();
            return mSearchManager;
        }
        return super.getSystemService(name);
    }
        
之后看一下关键点3，这里传递的token其实就是AMS端传递过来的IApplicationToken代理，一个IBinder对象。之后利用ContextImpl的getSystemService()函数得到一个一个WindowManagerImpl对象，再通过setWindowManager为Activity创建自己的WindowManagerImpl。到这一步，Activity已经准备完毕，剩下的就是在resume中通过addView将窗口添加到到WMS，具体实现在ActivityThread的handleResumeActivity函数中：

	 final void handleResumeActivity(IBinder token,
	            boolean clearHide, boolean isForward, boolean reallyResume) {
	        ActivityClientRecord r = performResumeActivity(token, clearHide);
	        
	        if (r != null) {
	            final Activity a = r.activity;
	            ...
	            if (r.window == null && !a.mFinished && willBeVisible) {
	               <!--关键点1-->
	                r.window = r.activity.getWindow();
	                View decor = r.window.getDecorView();
	                decor.setVisibility(View.INVISIBLE);
	                <!--关键点2 获取WindowManager-->
	                ViewManager wm = a.getWindowManager();
	                WindowManager.LayoutParams l = r.window.getAttributes();
	                a.mDecor = decor;
	                l.type = WindowManager.LayoutParams.TYPE_BASE_APPLICATION;
	                l.softInputMode |= forwardBit;
	                if (a.mVisibleFromClient) {
	                    a.mWindowAdded = true;
	                <!--关键点3 添加到WMS管理-->
	                    wm.addView(decor, l);
	                }
	             ...
	             }   
关键点1是为了获取Activit的Window及DecorView对象，如果用户没有通过setContentView方式新建DecorView，这里会利用PhoneWindow的getDecorView()新建DecorView，

    @Override
    public final View getDecorView() {
        if (mDecor == null) {
            installDecor();
        }
        return mDecor;
    }
    	             	
之后通过Activity的getWindowManager()获取WindowManagerImpl对象，这里获取的WindowManagerImpl其实是Activity自己的WindowManagerImpl，

    private WindowManagerImpl(Display display, Window parentWindow) {
        mDisplay = display;
        mParentWindow = parentWindow;
    }
    
它的mParentWindow 是非空的，获取WindowManagerImpl之后，便利用 addView(decor, l)将DecorView对应的窗口添加到WMS中去，最后调用的是

     @Override
    public void addView(@NonNull View view, @NonNull ViewGroup.LayoutParams params) {
        applyDefaultToken(params);
        mGlobal.addView(view, params, mDisplay, mParentWindow);
    }

可以看到这里会传递mParentWindow给WindowManagerGlobal对象，作为调整WindowMangaer.LayoutParams 中token的依据：

    public void addView(View view, ViewGroup.LayoutParams params,
            Display display, Window parentWindow) {
        
        final WindowManager.LayoutParams wparams = (WindowManager.LayoutParams) params;
        <!--调整wparams的token参数-->
        if (parentWindow != null) {
            parentWindow.adjustLayoutParamsForSubWindow(wparams);
        } 
         ViewRootImpl root;
         View panelParentView = null;
			 ..
            <!--新建ViewRootImpl ,并利用wparams参数添加窗口-->
            root = new ViewRootImpl(view.getContext(), display);
            view.setLayoutParams(wparams);
           ..
          <!--新建ViewRootImpl -->
          root.setView(view, wparams, panelParentView);
         }
         
parentWindow.adjustLayoutParamsForSubWindow是一个很关键的函数，从名字就能看出，这是为了他调整子窗口的参数：

	   void adjustLayoutParamsForSubWindow(WindowManager.LayoutParams wp) {
	        CharSequence curTitle = wp.getTitle();
	        <!--如果是子窗口如何处理-->
	        if (wp.type >= WindowManager.LayoutParams.FIRST_SUB_WINDOW &&
	            wp.type <= WindowManager.LayoutParams.LAST_SUB_WINDOW) {
	            <!--后面会看到，其实PopupWindow类的子窗口的wp.token是在上层显示赋值的-->
	            if (wp.token == null) {
	                View decor = peekDecorView();
	                if (decor != null) {
	                    // 这里其实是父窗口的IWindow对象 Window只有Dialog跟Activity才有
	                    wp.token = decor.getWindowToken();
	                }
	            }
	             
	        } else {
	        <!--这里其实只对应用窗口有用 Activity与Dialog都一样-->
	            if (wp.token == null) {
	                wp.token = mContainer == null ? mAppToken : mContainer.mAppToken;
	            }
	        }
	    }
	    
对于Activity来说，wp.token = mContainer == null ? mAppToken : mContainer.mAppToken，其实就是AMS端传过来的IApplicationToken，之后在ViewRootImpl中setView的时候，会利用IWindowSession代理与WMS端的Session通信，将窗口以及token信息传递到WMS端，其中IApplicationToken就是该Activity所处于的分组，在WMS端，会根据IApplicationToken IBinder键值，从全局的mTokenMap中找到对应的AppWindowToken。既然说分组，就应该有其他的子元素，下面看一下Activity上弹出Dialog的流程，进一步了解为什么Activity与它弹出的Dialog是统一分组（复用同一套token）。

##  Dialg分组及显示原理：为什么Activity与Dialog算同一组？


在添加到WMS的时候，Dialog的窗口属性是WindowManager.LayoutParams.TYPE_APPLICATION，同样属于应用窗口，因此，必须使用Activity的AppToken才行，换句话说，必须使用Activity内部的WindowManagerImpl进行addView才可以。Dialog和Activity共享同一个WindowManager（也就是WindowManagerImpl），而WindowManagerImpl里面有个Window类型的mParentWindow变量，这个变量在Activity的attach中创建WindowManagerImpl时传入的为当前Activity的Window，而Activity的Window里面的mAppToken值又为当前Activity的token，所以Activity与Dialog共享了同一个mAppToken值，只是Dialog和Activity的Window对象不同,下面用代码确认一下：

    Dialog(@NonNull Context context, @StyleRes int themeResId, boolean createContextThemeWrapper) {
    <!--关键点 1 根据theme封装context-->
        if (createContextThemeWrapper) {
            ...
            mContext = new ContextThemeWrapper(context, themeResId);
        } else {
            mContext = context;
        }
   		 <!--获取mWindowManager-->
   		 
        mWindowManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        <!--创建PhoneWindow-->
        final Window w = new PhoneWindow(mContext);
        mWindow = w;
        w.setCallback(this);
        w.setOnWindowDismissedCallback(this);
        w.setWindowManager(mWindowManager, null, null);
        w.setGravity(Gravity.CENTER);
        mListenersHandler = new ListenersHandler(this);
    }

以上代码先根据Theme调整context，之后利用context.getSystemService(Context.WINDOW_SERVICE)，这里Dialog是从Activity弹出来的，所以context是Activity，如果你设置Application，会有如下error，至于为什么，后面分析会看到。

	 android.view.WindowManager$BadTokenException: Unable to add window -- token null is not for an application
	       at android.view.ViewRootImpl.setView(ViewRootImpl.java:563)
	       at android.view.WindowManagerGlobal.addView(WindowManagerGlobal.java:269)
	       at android.view.WindowManagerImpl.addView(WindowManagerImpl.java:69)
       
接着看Activity的getSystemService，上文分析过这种方法获取的其实是Activity中PhoneWindow的WindowManagerImpl，所以后面利用WindowManagerImpl addView的时候，走的流程与Activity一样。看一下show的代码：

    public void show() {
        ...
        onStart();
        mDecor = mWindow.getDecorView();
        ...
        <!--关键点 WindowManager.LayoutParams的获取-->
        WindowManager.LayoutParams l = mWindow.getAttributes();
        ...
        try {
            mWindowManager.addView(mDecor, l);
            mShowing = true;
            sendShowMessage();
        } finally {
        }
    }

Window在创建的时候，默认新建WindowManager.LayoutParams mWindowAttributes

    private final WindowManager.LayoutParams mWindowAttributes =
        new WindowManager.LayoutParams();
            
采用的是无参构造方法，

        public LayoutParams() {
            super(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
            type = TYPE_APPLICATION;
            format = PixelFormat.OPAQUE;
        }
        
因此这里的type = TYPE_APPLICATION，也就是说Dialog的窗口类型其实是应用窗口。因此在addView走到上文的adjustLayoutParamsForSubWindow的时候，仍然按照Activity的WindowManagerImpl addView的方式处理，并利用Activity的PhoneWindow的 adjustLayoutParamsForSubWindow调整参数，赋值给WindowManager.LayoutParams token的值仍然是Activity的IApplicationToken，同样在WMS端，对应就是APPWindowToken，也就是Activity与Dialog属于同一分组。

	   void adjustLayoutParamsForSubWindow(WindowManager.LayoutParams wp) {
	        CharSequence curTitle = wp.getTitle();
	     	        <!--这里其实只对应用窗口有用 Activity与Dialog都一样-->
	            if (wp.token == null) {
	                wp.token = mContainer == null ? mAppToken : mContainer.mAppToken;
	        }
	    }

回到之前遗留的一个问题，为什么Dialog用Application作为context不行呢？Dialog的窗口类型属于应用窗口，如果采用Application作为context，那么通过context.getSystemService(Context.WINDOW_SERVICE)获取的WindowManagerImpl就不是Activity的WindowManagerImpl，而是Application，它同Activity的WindowManagerImpl的区别是没有parentWindow，所以adjustLayoutParamsForSubWindow函数不会被调用，WindowManager.LayoutParams的token就不会被赋值，因此ViewRootImpl在通过setView向WMS在添加窗口的时候会失败：

    public int addWindow(Session session, IWindow client, XXX )
            ...
            <!--对于应用窗口 token不可以为null-->
            WindowToken token = mTokenMap.get(attrs.token);
            if (token == null) {
                if (type >= FIRST_APPLICATION_WINDOW && type <= LAST_APPLICATION_WINDOW) {
                    Slog.w(TAG, "Attempted to add application window with unknown token "
                          + attrs.token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
                
                
WMS会返回WindowManagerGlobal.ADD_BAD_APP_TOKEN的错误给APP端，APP端ViewRootImpl端收到后会抛出如下异常

    public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
        synchronized (this) {
                       ....
                        case WindowManagerGlobal.ADD_NOT_APP_TOKEN:
                            throw new WindowManager.BadTokenException(
                                    "Unable to add window -- token " + attrs.token
                                    + " is not for an application");

以上就为什么不能用Application作为Dialog的context的理由（**不能为Dialog提供正确的token**），接下来看一下PopupWindow是如何处理分组的。
        
## PopupWindow类子窗口的添加流程及WindowToken分组


PopupWindow是最典型的子窗口，必须依附父窗口才能存在，先看下PopupWindow一般用法：
	
		 View root = LayoutInflater.from(AppProfile.getAppContext()).inflate(R.layout.pop_window, null);
        PopupWindow popupWindow = new PopupWindow(root, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, true);
        popupWindow.setBackgroundDrawable(new BitmapDrawable());
        popupWindow.showAsDropDown(archorView);

PopupWindow的构造函数很普通，主要是一些默认入场、出厂动画的设置，如果在新建PopupWindow的时候已经将根View传递到构造函数中去，PopupWindow的构造函数会调用setContentView，如果在show之前，没有调用setContentView，则抛出异常。

    public PopupWindow(View contentView, int width, int height, boolean focusable) {
        if (contentView != null) {
            mContext = contentView.getContext();
            mWindowManager = (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
        }
        setContentView(contentView);
        setWidth(width);
        setHeight(height);
        setFocusable(focusable);
    }
    
下面主要看PopupWindow的showAsDropDown函数  

    public void showAsDropDown(View anchor, int xoff, int yoff, int gravity) {
        <!--关键点1  利用通过View锚点所在窗口显性构建PopupWindow的token-->
        final WindowManager.LayoutParams p = createPopupLayoutParams(anchor.getWindowToken());
        <!--关键点2-->
        preparePopup(p);
        ...
        <!--关键点3-->
        invokePopup(p);
    }
    
showAsDropDown有3个关键点，关键点1是生成WindowManager.LayoutParams参数，WindowManager.LayoutParams参数里面的type、token是非常重要参数，**PopupWindow的type是TYPE_APPLICATION_PANEL = FIRST_SUB_WINDOW**，是一个子窗口。关键点2是PopupDecorView的生成，这个View是PopupWindow的根ViewGroup，类似于Activity的DecorView，关键3利用WindowManagerService的代理，将View添加到WMS窗口管理中去显示，先看关键点1：

    private WindowManager.LayoutParams createPopupLayoutParams(IBinder token) {
        final WindowManager.LayoutParams p = new WindowManager.LayoutParams();
        p.gravity = computeGravity();
        p.flags = computeFlags(p.flags);
        p.type = mWindowLayoutType;
        <!--显性赋值token-->
        p.token = token;
        p.softInputMode = mSoftInputMode;
        p.windowAnimations = computeAnimationResource();
        if (mBackground != null) {
            p.format = mBackground.getOpacity();
        } else {
            p.format = PixelFormat.TRANSLUCENT;
        }
        ..
        p.privateFlags = PRIVATE_FLAG_WILL_NOT_REPLACE_ON_RELAUNCH
                | PRIVATE_FLAG_LAYOUT_CHILD_WINDOW_IN_PARENT_FRAME;
        return p;
    }

上面的Token其实用的是anchor.getWindowToken()，**如果是Activity中的View，其实用的Token就是Activity的ViewRootImpl中的IWindow对象**，如果这个View是一个系统窗口中的View，比如是Toast窗口中弹出来的，用的就是Toast ViewRootImpl的IWindow对象，归根到底，PopupWindow自窗口中的Token是ViewRootImpl的IWindow对象，同Activity跟Dialog的token（IApplicationToken）不同，该Token标识着PopupWindow在WMS所处的分组，最后来看一下PopupWindow的显示:

    private void invokePopup(WindowManager.LayoutParams p) {
        if (mContext != null) {
            p.packageName = mContext.getPackageName();
        }
        final PopupDecorView decorView = mDecorView;
        decorView.setFitsSystemWindows(mLayoutInsetDecor);
        setLayoutDirectionFromAnchor();
        <!--关键点1-->
        mWindowManager.addView(decorView, p);
        if (mEnterTransition != null) {
            decorView.requestEnterTransition(mEnterTransition);
        }
    }
 
主要是调用了WindowManager的addView添加视图并显示，这里首先需要关心一下mWindowManager，

        mWindowManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);

这的context 可以是Activity，也可以是Application，因此WindowManagerImpl也可能不同，不过这里并没有多大关系，因为PopupWindow的token是显性赋值的，就是是就算用Application，也不会有什么问题，对于PopupWindow子窗口，关键点是View锚点决定其token，而不是WindowManagerImpl对象：

    @Override
    public void addView(@NonNull View view, @NonNull ViewGroup.LayoutParams params) {
        applyDefaultToken(params);
        mGlobal.addView(view, params, mContext.getDisplay(), mParentWindow);
    }
 
之后利用ViewRootImpl的setView函数的时候，WindowManager.LayoutParams里的token其实就是view锚点获取的IWindow对象，WindowManagerService在处理该请求的时候，

    public int addWindow(Session session, IWindow client, XXX ) {

		  <!--关键点1，必须找到子窗口的父窗口，否则添加失败-->
		   WindowState attachedWindow = null;
            if (type >= FIRST_SUB_WINDOW && type <= LAST_SUB_WINDOW) {
                attachedWindow = windowForClientLocked(null, attrs.token, false);
                if (attachedWindow == null) {
                    return WindowManagerGlobal.ADD_BAD_SUBWINDOW_TOKEN;
                }
            }
			<!--关键点2 如果Activity第一次添加子窗口 ，子窗口分组对应的WindowToken一定是null-->
            boolean addToken = false;
            WindowToken token = mTokenMap.get(attrs.token);
            AppWindowToken atoken = null;
            if (token == null) {
            ...
                token = new WindowToken(this, attrs.token, -1, false);
                addToken = true;
            } 			
			<!--关键点2 新建窗口WindowState对象 注意这里的attachedWindow非空-->
           WindowState win = new WindowState(this, session, client, token,
                    attachedWindow, appOp[0], seq, attrs, viewVisibility, displayContent);
           ...
			<!--关键点4 添加更新全部map，-->
            if (addToken) {
                mTokenMap.put(attrs.token, token);
             }
            mWindowMap.put(client.asBinder(), win);
            }
            
从上面的分析可以看出，WMS会为PopupWindow窗口创建一个子窗口分组WindowToken，每个子窗口都会有一个指向父窗口的引用，因为是利用父窗口的IWindow作为键值，父窗口可以很方便的利用自己的IWindow获取WindowToken，进而得到全部的子窗口，

关于系统窗口，前文层分析过Toast系统窗口，Toast类系统窗口在WMS端只有一个WindowToken，键值是null，这个比较奇葩，不过还没验证过。

## 窗口的Z次序管理：窗口的分配序号、次序调整等

虽然我们看到的手机屏幕只是一个二维平面X*Y，但其实Android系统是有隐形的Z坐标轴的，其方向与手机屏幕垂直，与我们的实现平行，所以并不能感知到。

![Z order.jpg](http://upload-images.jianshu.io/upload_images/1460468-26d924e59a4b00f8.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

前面分析了窗口分组的时候涉及了两个对象WindowState与Windtoken，但仅限分组，分组无法决定窗口的显示的Z-order，那么再WMS是怎么管理所有窗口的Z-order的？  在WMS中窗口被抽象成WindowState，因此WindowState内部一定有属性来标志这个窗口的Z-order，实现也确实如此，WindowState采用三个个int值mBaseLayer+ mSubLayer + mLayer 来标志窗口所处的位置，前两个主要是根据窗口类型确定窗口位置，mLayer才是真正的值，定义如下：

	final class WindowState implements WindowManagerPolicy.WindowState {
	    
	    final WindowList mChildWindows = new WindowList();
	    final int mBaseLayer;
	    final int mSubLayer;
	     <!--最终Z次序的赋值-->
       int mLayer;
	    
	    }
	    
从名字很容知道mBaseLayer是标志窗口的主次序，面向的是一个窗口组，而mSubLayer主要面向单独窗口，要来标志一个窗口在一组窗口中的位置，对两者来说值越大，窗口越靠前，从此final属性知道，两者的值是不能修改的，而mLayer可以修改，对于系统窗口，一般不会同时显示两个，因此，可以用主序决定，比较特殊的就是Activity与子窗口，首先子窗口的主序肯定是父窗口决定的，子窗口只关心次序就行。而父窗口的主序却相对麻烦，比如对于应用窗口来说，他们的主序都是一样的，因此还要有一个其他的维度来作为参考，比如**对于Activity，主序都是一样的，怎么定他们真正的Z-order呢？其实Activity的顺序是由AMS保证的，这个顺序定了，WMS端Activity窗口的顺序也是定了，这样下来次序也方便定了**。
    
	WindowState(WindowManagerService service, Session s, IWindow c, WindowToken token,
	           WindowState attachedWindow, int appOp, int seq, WindowManager.LayoutParams a,
	           int viewVisibility, final DisplayContent displayContent) {
	        ...
	        	<!--关键点1  子窗口类型的Z order-->
	        if ((mAttrs.type >= FIRST_SUB_WINDOW &&
	                mAttrs.type <= LAST_SUB_WINDOW)) {
	            mBaseLayer = mPolicy.windowTypeToLayerLw(
	                    attachedWindow.mAttrs.type) * WindowManagerService.TYPE_LAYER_MULTIPLIER
	                    + WindowManagerService.TYPE_LAYER_OFFSET;
	            mSubLayer = mPolicy.subWindowTypeToLayerLw(a.type);
	            mAttachedWindow = attachedWindow;	            final WindowList childWindows = mAttachedWindow.mChildWindows;
	            final int numChildWindows = childWindows.size();
	            if (numChildWindows == 0) {
	                childWindows.add(this);
	            } else {
	             ...
	        } else {
	        	<!--关键点2  普通窗口类型的Z order-->
	            mBaseLayer = mPolicy.windowTypeToLayerLw(a.type)
	                    * WindowManagerService.TYPE_LAYER_MULTIPLIER
	                    + WindowManagerService.TYPE_LAYER_OFFSET;
	            mSubLayer = 0;
	            mAttachedWindow = null;
	            mLayoutAttached = false;
	        }
	       ...
	    }
	    
由于窗口所能选择的类型是确定的，因此mBaseLayer与mSubLayer所能选择的值只有固定几个，很明显这两个参数不能精确的确定Z-order，还会有其他微调的手段，也仅限微调，**在系统层面，决定了不同类型窗口所处的位置，比如系统Toast类型的窗口一定处于所有应用窗口之上**，不过我们最关心的是Activity类的窗口如何确定Z-order的，在new WindowState之后，只是粗略的确定了Activity窗口的次序，看一下添加窗口的示意代码：

	addWindow(){
	    <!--1-->
	    new WindowState
	    <!--2-->
	    addWindowToListInOrderLocked(win, true);
	    <!--3-->
	    assignLayersLocked(displayContent.getWindowList());
	 	}

新建state对象之后，Z-order还要通过addWindowToListInOrderLocked及assignLayersLocked才能确定，addWindowToListInOrderLocked主要是根据窗口的Token找到归属，插入到对应Token的WindowState列表，如果是子窗口还要插入到父窗口的对应位置中：

![次序确定.jpg](http://upload-images.jianshu.io/upload_images/1460468-0022179c69462bf3.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

插入到特定位置后其实Z-order就确定了，接下来就是通过assignLayersLocked为WindowState分配真正的Z-order mLayer,
	
	   private final void assignLayersLocked(WindowList windows) {
	        int N = windows.size();
	        int curBaseLayer = 0;
	        int curLayer = 0;
	        int i;

	        boolean anyLayerChanged = false;
		        for (i=0; i<N; i++) {
	            final WindowState w = windows.get(i);
	            final WindowStateAnimator winAnimator = w.mWinAnimator;
	            boolean layerChanged = false;
	            int oldLayer = w.mLayer;
	            if (w.mBaseLayer == curBaseLayer || w.mIsImWindow
	                    || (i > 0 && w.mIsWallpaper)) {
	                <!--通过偏移量-->
	                curLayer += WINDOW_LAYER_MULTIPLIER;
	                w.mLayer = curLayer;
	            } else {
	                curBaseLayer = curLayer = w.mBaseLayer;
	                w.mLayer = curLayer;
	            }
	            if (w.mLayer != oldLayer) {
	                layerChanged = true;
	                anyLayerChanged = true;
	            }
	            ...
	    }
    
mLayer最终确定后，窗口的次序也就确定了，这个顺序要最终通过后续的relayout更新到SurfaceFlinger服务，之后，SurfaceFlinger在图层混排的时候才知道如何处理。

## WMS中窗口次序分配如何影响SurfaceFlinger服务

SurfaceFlinger在图层混排的时候应该不会混排所有的窗口，只会混排可见的窗口，比如有多个全屏Activity的时候，SurfaceFlinger只会处理最上面的，那么SurfaceFlinger如何知道哪些窗口可见哪些不可见呢？前文分析了WMS分配Z-order之后，要通过setLayer更新到SurfaceFlinger，接下来看具体流程，创建SurfaceControl之后，会创建一次事务，确定Surface的次序：

	   SurfaceControl.openTransaction();
	            try {
	                mSurfaceX = left;
	                mSurfaceY = top;
		                try {
	                    mSurfaceControl.setPosition(left, top);
	                    mSurfaceLayer = mAnimLayer;
	                    final DisplayContent displayContent = w.getDisplayContent();
	                    if (displayContent != null) {
	                        mSurfaceControl.setLayerStack(displayContent.getDisplay().getLayerStack());
	                    }
	                    <!--设置次序-->
	                    mSurfaceControl.setLayer(mAnimLayer);
	                    mSurfaceControl.setAlpha(0);
	                    mSurfaceShown = false;
	                } catch (RuntimeException e) {
	                    mService.reclaimSomeSurfaceMemoryLocked(this, "create-init", true);
	                }
	                mLastHidden = true;
	            } finally {
	                SurfaceControl.closeTransaction();
	            }
	        }
        
这里通过openTransaction与closeTransaction保证一次事务的完整性，中间就Surface次序的调整，closeTransaction会与SurfaceFlinger通信，通知SurfaceFlinger更新Surface信息，这其中就包括Z-order。

# 总结

本文简要分析了Android窗口的分组，以及WMS窗口次序的确定，最后简单提及了一下窗口次序如何更新到SurfaceFlinger服务的，也方便将来理解图层合成。
                    
# 	参考文档
[ Android6.0 SurfaceControl分析（二）SurfaceControl和SurfaceFlinger通信](http://blog.csdn.net/kc58236582/article/details/65445141)       
[ GUI系统之SurfaceFlinger(11)SurfaceComposerClient](http://blog.csdn.net/xuesen_lin/article/details/8954957)                 
[ Skia深入分析1——skia上下文](http://blog.csdn.net/jxt1234and2010/article/details/42572559)        
[ Android图形显示系统——概述](http://blog.csdn.net/jxt1234and2010/article/details/44164691)           
[Linux环境进程间通信（五）: 共享内存（下）](https://www.ibm.com/developerworks/cn/linux/l-ipc/part5/index2.html)      
[Android Binder 分析——匿名共享内存（Ashmem）
By Mingming](http://light3moon.com/2015/01/28/Android%20Binder%20%E5%88%86%E6%9E%90%E2%80%94%E2%80%94%E5%8C%BF%E5%90%8D%E5%85%B1%E4%BA%AB%E5%86%85%E5%AD%98[Ashmem]/)     
[Android 匿名共享内存驱动源码分析](http://blog.csdn.net/yangwen123/article/details/9318319)       
[ Android窗口管理服务WindowManagerService的简要介绍和学习计划](http://blog.csdn.net/luoshengyang/article/details/8462738)                      
[Android4.2.2 SurfaceFlinger之图形渲染queueBuffer实现和VSYNC的存在感](http://blog.csdn.net/gzzaigcnforever/article/details/22046141)          
[Android6.0 显示系统GraphicBuffer分配内存](http://www.voidcn.com/blog/kc58236582/article/p-6238474.html)   
[InputManagerService分析一：IMS的启动与事件传递](http://blog.csdn.net/lilian0118/article/details/28617185)        
[Android 5.0(Lollipop)事件输入系统(Input System)](http://blog.csdn.net/jinzhuojun/article/details/41909159)    
[浅析 Android 的窗口](https://dev.qq.com/topic/5923ef85bdc9739041a4a798)      
[ 【Linux】进程间通信（IPC）之共享内存详解与测试用例](http://blog.csdn.net/a1414345/article/details/69389647)     
[Android6.0 显示系统（三） 管理图像缓冲区](http://blog.csdn.net/kc58236582/article/details/52681363)       
