---
layout: post
title: 从PopupWindow显示再窥Android窗口管理系统
category: Android
image: 

---

PopupWindow与Dialog、Activity不同，PopupWindow不属于独立的窗口，如果稍微了解WMS就会知道，Dialog、Activity都属于应用窗口，而PopupWindow属于子窗口，本文就来分析下PopupWindow的原理。

* PopupWindow同Activity的区别是：PopupWindow没有自己的WindowToke，它必须依附到其他窗口。
* PopupWindow同Dialog的区别是：PopupWindow所依附的窗口没有限制，但是Dialog依附的窗口必须是Activity，Dialog中可以弹PopupWindow，但是PopupWindow不能弹出Dialog。
* PopupWindow同系统窗口的区别是：系统窗口可以独立存在，但是PopupWindow不可以，系统窗口可以使PopupWindow的父窗口，反过来却不行。

PopupWindow一般用法：
	
			 View root = LayoutInflater.from(AppProfile.getAppContext()).inflate(R.layout.pop_window, null);
	        PopupWindow popupWindow = new PopupWindow(root, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, true);
	        popupWindow.setBackgroundDrawable(new BitmapDrawable());
	        popupWindow.showAsDropDown(archorView);

主要看PopupWindow的构造函数很普通，主要是一些默认入场、出厂动画的设置，当然如果在新建PopupWindow的时候已经将根View传递到构造函数中去的时候，PopupWindow的构造函数会调用setContentView，如果在show之前，没有调用setContentView，会抛出异常的。

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
        <!--关键点1-->
        final WindowManager.LayoutParams p = createPopupLayoutParams(anchor.getWindowToken());
        <!--关键点2-->
        preparePopup(p);
        ...
        <!--关键点3-->
        invokePopup(p);
    }
    
showAsDropDown有3个关键点，关键点1是生成WindowManager.LayoutParams，WindowManager.LayoutParams里面的type、token是非常重要参数，PopupWindow的type是TYPE_APPLICATION_PANEL = FIRST_SUB_WINDOW，它是一个子窗口，必须依附父窗口才能存在。WMS管理窗口其实是有分组的，比如Activity不可见的时候，Activity内部的PopupWindow等子窗口也需要变得不可见，而token就是决定谁是它的父窗口，之后，PopupWindow会跟随那个窗口显示/隐藏。关键点2是PopupDecorView的生成，这个View是PopupWindow的根ViewGroup，地位类似于Activity的DecorView，关键3利用WindowManagerService的代理，将View添加到WMS窗口管理中去显示，先看关键点1：

    private WindowManager.LayoutParams createPopupLayoutParams(IBinder token) {
        final WindowManager.LayoutParams p = new WindowManager.LayoutParams();
        p.gravity = computeGravity();
        p.flags = computeFlags(p.flags);
        p.type = mWindowLayoutType;
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

上面的Token其实用的是anchor.getWindowToken()，如果是Activity中的View，其实用的Token就是Activity的ViewRootImpl中的IWindow对象。如果这个View是一个系统窗口中的View，比如是Toast窗口中弹出来的，用的就是Toast ViewRootImpl的IWindow对象，归根到底，PopupWindow自窗口中的Token是ViewRootImpl的IWindow对象，该Token标识着PopupWindow在WMS所处的分组。接着往下看preparePopup：
    
	  private void preparePopup(WindowManager.LayoutParams p) {
	  
	        <!--关键点1-->
	        // When a background is available, we embed the content view within
	        // another view that owns the background drawable.
	        if (mBackground != null) {
	            mBackgroundView = createBackgroundView(mContentView);
	            mBackgroundView.setBackground(mBackground);
	        } else {
	            mBackgroundView = mContentView;
	        }
		    <!--关键点2-->
	        mDecorView = createDecorView(mBackgroundView);
	        ..
	    }

上面的代码主要是根据我们设置的contentview，以及是否设置了背景来创建PopupDecorView，其实就是一层简单的封装。最后来看一下PopupWindow的显示:

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

这的context 可以是Activity，也可以是Application，因此WindowManagerImpl也可能不同

    @Override
    public void addView(@NonNull View view, @NonNull ViewGroup.LayoutParams params) {
        applyDefaultToken(params);
        mGlobal.addView(view, params, mContext.getDisplay(), mParentWindow);
    }
 
 如果是Activity的WindowManagerImpl，其mParentWindow就不为null，否则为null，虽然对Popwindow没啥影响，还是要提一下，之后会新建ViewRootImpl，并利用其setView将View添加显示。

    public void addView(View view, ViewGroup.LayoutParams params,
            Display display, Window parentWindow) {
     	  ...
        ViewRootImpl root;
        View panelParentView = null;
        synchronized (mLock) {
         ...
            root = new ViewRootImpl(view.getContext(), display);
            view.setLayoutParams(wparams);
            mViews.add(view);
            mRoots.add(root);
            mParams.add(wparams);
            try {
                root.setView(view, wparams, panelParentView);
            } catch (RuntimeException e) {
          ...
        } }


* PopupWindow的窗口类型：子窗口             WindowManager.LayoutParams.TYPE_APPLICATION_PANEL
* Toast的窗口类型 ：系统窗口                WindowManager.LayoutParams.TYPE_TOAST;
* Dialog的窗口类型跟Activity一样 ：应用窗口  WindowManager.LayoutParams.TYPE_APPLICATION


# Dialog为什么不能用Application的Context

Dialog的窗口属性是WindowManager.LayoutParams.TYPE_APPLICATION，同样属于应用窗口，在添加到WMS的时候，必须使用Activity的AppToken才行，换句话说，必须使用Activity内部的WindowManagerImpl进行addView才可以。

实现也确实如此，Dialog和Activity共享同一个WindowManager（也就是WindowManagerImpl），而WindowManagerImpl里面有个Window类型的mParentWindow变量，这个变量在Activity的attach中创建WindowManagerImpl时传入的为当前Activity的Window，而当前Activity的Window里面的mAppToken值又为当前Activity的token，所以Activity与Dialog共享了同一个mAppToken值，只是Dialog和Activity的Window对象不同。

这里是Activity Dialog复用的关键， 是Activity覆盖了  getSystemService函数里面的  mWindowManager就是Dialog使用的Manager，并且Window的Manager中，有个mParentWindow变量，是Activity中window自己。  mWindowManager = mWindow.getWindowManager();

> Activity.java

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

WindowManagerImpl.java

    @Override
    public void addView(View view, ViewGroup.LayoutParams params) {
        // 如果是dialog，这里的mParentWindow是Activity的Window
        mGlobal.addView(view, params, mDisplay, mParentWindow);
    }

LayoutParams中token是WMS用来处理TokenMap,而IWindow主要是用来处理mWindowMap的。

*  tokenmap 传递的token竟然在 WindowManager.LayoutParams attrs中
*  windowmap的key用的是IWindow
*  mWindowMap 与 mTokenMap都是系统唯一的。这个系统维护一份

多个Windowstate对应一个windowToken

如何理解WindowToken 对于Popinwindow，它是个子窗口，需要有相应的Token，什么样的Token？

    private View mview;
    private Runnable runnable0 = new Runnable() {
        @Override
        public void run() {
            mview = LayoutInflater.from(MainActivity.this).inflate(R.layout.popcontianer, null);
            mTextView = mview.findViewById(R.id.show);
            mTextView.setOnClickListener(new View.OnClickListener() {
                @TargetApi(Build.VERSION_CODES.KITKAT)
                @Override
                public void onClick(View v) {
                    PopupWindow popupWindow = new PopupWindow();
                    View view = LayoutInflater.from(MainActivity.this).inflate(R.layout.content_main, null);
                    popupWindow.setContentView(view);
                    popupWindow.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
                    popupWindow.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
                    popupWindow.showAsDropDown(mTextView);

                    mTextView.setOnClickListener(new View.OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            WindowManager mWindowManager = (WindowManager) getApplication().getSystemService(Context.WINDOW_SERVICE);
                            mWindowManager.removeView(mview);
                        }
                    });
                }
            });
            WindowManager mWindowManager = (WindowManager) getApplication().getSystemService(Context.WINDOW_SERVICE);
            mWindowManager.addView(mview, getParams());
        }
    };

    View mTextView = null;
    Handler handler = null;

    @OnClick(R.id.first)
    void first() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                Looper.prepare();
                handler = new Handler();
                handler.post(runnable0);
                Looper.loop();
            }
        }).start();
    }
    
注意，虽然添加View，但是从来没有向WMS直接传递View对象，真正与WMS通信的接口IWindowSession没有给任何View参数的传递，都是IWindow window加上其他的必要参数，也就是View的管理不是WMS的范畴，WMS只负责抽象Window的管理。

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
	            in int viewVisibility, in int layerStackId, out Rect outContentInsets);
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
	...
	}

WMS 究竟管理什么呢？有人说WingdowManagerService也可以成为SurfaceManagerService，为何？


如果有背景，则会在contentView外面包一层PopupViewContainer之后作为mPopupView，如果没有背景，则直接用contentView作为mPopupView。
而这个PopupViewContainer是一个内部私有类，它继承了FrameLayout，在其中重写了Key和Touch事件的分发处理 

        if (mBackground != null) {
            final ViewGroup.LayoutParams layoutParams = mContentView.getLayoutParams();
            int height = ViewGroup.LayoutParams.MATCH_PARENT;
            if (layoutParams != null &&
                    layoutParams.height == ViewGroup.LayoutParams.WRAP_CONTENT) {
                height = ViewGroup.LayoutParams.WRAP_CONTENT;
            }

            // when a background is available, we embed the content view
            // within another view that owns the background drawable
            PopupViewContainer popupViewContainer = new PopupViewContainer(mContext);
            PopupViewContainer.LayoutParams listParams = new PopupViewContainer.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, height
            );
            popupViewContainer.setBackgroundDrawable(mBackground);
            popupViewContainer.addView(mContentView, listParams);

            mPopupView = popupViewContainer;
        } else {
            mPopupView = mContentView;
        }
        
 

这里的WindowToken的作用是窗口的分类，比如Activit1 Activity2 ，Activit1子窗口分组，Activity2子窗口分组，是什么呢？是窗口类型的分组   

**在Window管理服务WindowManagerService中，无论是AppWindowToken对象，还是WindowToken对象，它们都是用来描述一组有着相同令牌的窗口的，每一个窗口都是通过一个WindowState对象来描述的。例如，一个Activity组件窗口可能有一个启动窗口（Starting Window），还有若干个子窗口，那么这些窗口就会组成一组，并且都是以Activity组件在Window管理服务WindowManagerService中所对应的AppWindowToken对象为令牌的。从抽象的角度来看，就是在Window管理服务WindowManagerService中，每一个令牌（AppWindowToken或者WindowToken）都是用来描述一组窗口（WindowState）的，并且每一个窗口的子窗口也是与它同属于一个组，即都有着相同的令牌。**

# 参考文档

[Android对话框Dialog，PopupWindow，Toast的实现机制  ](http://blog.csdn.net/feiduclear_up/article/details/49080587)     
[Android窗口机制（五）最终章：WindowManager.LayoutParams和Token以及其他窗口Dialog，Toast](http://www.jianshu.com/p/bac61386d9bf)