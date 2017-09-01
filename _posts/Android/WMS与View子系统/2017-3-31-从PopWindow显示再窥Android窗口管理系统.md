---
layout: post
title: 从PopupWindow、Dialog显示原理看Android窗口管理系统
category: Android
image: 

---

# 目录 

> PopupWindow使用原理分析
> Dialog使用原理
> WindowLeak问题分析
> 触摸事件的回调处理分析

PopupWindow、Dialog、Activity三者都有窗口的概念，但又各有不同，Activity属于应用窗口、PopupWindow属于子窗口，而Dialog位于两者之间，从性质上说属于应用窗口，但是从直观理解上，比较像子窗口（其实不是）。Android中的窗口主要分为三种：系统窗口、应用窗口、子窗口，Toast就属于系统窗口，而Dialog、Activity属于应用窗口，不过Dialog必须依附Activity才能存在。PopupWindow算是子窗口，必须依附到其他窗口，依附的窗口可以使应用窗口也可以是系统窗口，但是不能是子窗口。

* PopupWindow的窗口类型：子窗口             WindowManager.LayoutParams.TYPE_APPLICATION_PANEL
* Toast的窗口类型 ：系统窗口                WindowManager.LayoutParams.TYPE_TOAST;
* Dialog的窗口类型跟Activity一样 ：应用窗口  WindowManager.LayoutParams.TYPE_APPLICATION
* 
从直观上来说，Android窗口管理是有分组概念的，比如，一个Activity可以包含多个PopupWindow，当Activity隐藏或者销毁的时候，上面的PopupWindow也必须被隐藏、销毁。在WindowManagerService端，与分组对应的数据结构是WindowToken（窗口令牌），而与组内每个窗口对应的是WindowState对象，每块令牌（AppWindowToken、WindowToken）都对应一组窗口（WindowState），Activity与Dialog对应的是AppWindowToken，PopupWindow对应的是普通的WindowToken，**WindowState与窗口是一对一，而WindowToken与窗口是一对多**，WindowToken的意义就是批量管理窗口。

![窗口组织形式.jpg](http://upload-images.jianshu.io/upload_images/1460468-0e40c108c5017f1f.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# PopupWindow使用原理分析

PopupWindow一般用法：
	
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
        <!--关键点1-->
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
            <!--关键点1-->
            root = new ViewRootImpl(view.getContext(), display);
            view.setLayoutParams(wparams);
            mViews.add(view);
            mRoots.add(root);
            mParams.add(wparams);
            try {
            <!--关键点2-->
                root.setView(view, wparams, panelParentView);
            } catch (RuntimeException e) {
          ...
        } }
    
WindowManagerGloble会新建一个ViewRootImpl，里面有个关键对象 mWindow = new W(this)，这个是同WMS端WindowState一一对应的，也是WMS端想APP端远程通信的Binder通路。

    public ViewRootImpl(Context context, Display display) {
        mContext = context;
        mWindowSession = WindowManagerGlobal.getWindowSession();
        ...
        mWindow = new W(this);
        ...
    }
    
之后，利用ViewRootImpl的setView函数，通过mWindowSession将窗口添加到WMS并显示，mWindowSession是APP向WMS进行Binder通信的Bp端，对应的Bn端是WinowManagerService端Session：

		public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
				...
              res = mWindowSession.addToDisplay(mWindow, mSeq, mWindowAttributes,
                    getHostVisibility(), mDisplay.getDisplayId(),
                    mAttachInfo.mContentInsets, mAttachInfo.mStableInsets,
                    mAttachInfo.mOutsets, mInputChannel);
			    ...
				}

相应的服务端Session会将请求转发给WindowManagerService处理

    public int addWindow(Session session, IWindow client, int seq,
            WindowManager.LayoutParams attrs, int viewVisibility, int displayId,
            Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
            InputChannel outInputChannel) {

        boolean reportNewConfig = false;
        WindowState attachedWindow = null;
        long origId;
        <!--关键点1-->
        final int type = attrs.type;

        synchronized(mWindowMap) {
           ...
		<!--关键点2-->
            if (mWindowMap.containsKey(client.asBinder())) {
                Slog.w(TAG_WM, "Window " + client + " is already added");
                return WindowManagerGlobal.ADD_DUPLICATE_ADD;
            }
		<!--关键点3-->
            if (type >= FIRST_SUB_WINDOW && type <= LAST_SUB_WINDOW) {
                attachedWindow = windowForClientLocked(null, attrs.token, false);
                if (attachedWindow == null) {
                    Slog.w(TAG_WM, "Attempted to add window with token that is not a window: "
                          + attrs.token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_BAD_SUBWINDOW_TOKEN;
                }
                if (attachedWindow.mAttrs.type >= FIRST_SUB_WINDOW
                        && attachedWindow.mAttrs.type <= LAST_SUB_WINDOW) {
                    Slog.w(TAG_WM, "Attempted to add window with token that is a sub-window: "
                            + attrs.token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_BAD_SUBWINDOW_TOKEN;
                }
            }
			<!--关键点4-->
            boolean addToken = false;
            WindowToken token = mTokenMap.get(attrs.token);
            AppWindowToken atoken = null;
            if (token == null) {
            		<!--关键点5-->
                if (type >= FIRST_APPLICATION_WINDOW && type <= LAST_APPLICATION_WINDOW) {
                    Slog.w(TAG_WM, "Attempted to add application window with unknown token "
                          + attrs.token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
                if (type == TYPE_INPUT_METHOD) {
                    Slog.w(TAG_WM, "Attempted to add input method window with unknown token "
                          + attrs.token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
                ...
                token = new WindowToken(this, attrs.token, -1, false);
                addToken = true;
            } else if (type >= FIRST_APPLICATION_WINDOW && type <= LAST_APPLICATION_WINDOW) {
            <!--关键点6-->
                atoken = token.appWindowToken;
                if (atoken == null) {
                    Slog.w(TAG_WM, "Attempted to add window with non-application token "
                          + token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_NOT_APP_TOKEN;
                } else if (atoken.removed) {
                    Slog.w(TAG_WM, "Attempted to add window with exiting application token "
                          + token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_APP_EXITING;
                }
                if (type == TYPE_APPLICATION_STARTING && atoken.firstWindowDrawn) {
                    if (DEBUG_STARTING_WINDOW || localLOGV) Slog.v(
                            TAG_WM, "**** NO NEED TO START: " + attrs.getTitle());
                    return WindowManagerGlobal.ADD_STARTING_NOT_NEEDED;
                }
            } else if (type == TYPE_INPUT_METHOD) {
                if (token.windowType != TYPE_INPUT_METHOD) {
                    Slog.w(TAG_WM, "Attempted to add input method window with bad token "
                            + attrs.token + ".  Aborting.");
                      return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } ...
			
			<!--关键点7-->
           WindowState win = new WindowState(this, session, client, token,
                    attachedWindow, appOp[0], seq, attrs, viewVisibility, displayContent);
           ...
			<!--关键点8-->
            if (addToken) {
                mTokenMap.put(attrs.token, token);
            }
            win.attach();
           <!--关键点9-->
            mWindowMap.put(client.asBinder(), win);
           ...
           return res;
    }

函数很长，只看几个关键的点

* 关键点1： 确定窗口的类型，对于PopupWindow 属于TYPE_APPLICATION_PANEL = FIRST_SUB_WINDOW
* 关键点2：  判断是否对于当前窗口添加过，一个视图不能重复添加两次
* 关键点3： 如果添加的窗口是子窗口，那么父窗口必须已经存在，否则会抛出异常
* 关键点4与5： 查找当前窗口对应的WindowToken分组，对于应用窗口一定可以找到，除非窗口是系统窗口或者子窗口。
* 关键点6：对于AppWindowToken的验证，保证Activity级别的token不出问题
* 关键点7：新建与窗口一一对应WindowState
* 关键点8与9 将新建的WindowToken与WindowState加入对应的Map。

经过上面几步WMS对于窗口管理所做的基本完成，当然也会牵扯焦点窗口的切换等问题。

# Dialog使用原理分析



# Dialog为什么不能用Application的Context，只能用Activity

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

	
	final class WindowState implements WindowManagerPolicy.WindowState {
	
	    final WindowList mChildWindows = new WindowList();
	    ...
	    }
	    
	    
添加窗口的时候，如果是子窗口，就会被加入到父窗口的子窗口列表中去：
	
	WindowState(WindowManagerService service, Session s, IWindow c, WindowToken token,
	           WindowState attachedWindow, int appOp, int seq, WindowManager.LayoutParams a,
	           int viewVisibility, final DisplayContent displayContent) {
	       ...
	
	        if ((mAttrs.type >= FIRST_SUB_WINDOW &&
	                mAttrs.type <= LAST_SUB_WINDOW)) {
	             
	            mBaseLayer = mPolicy.windowTypeToLayerLw(
	                    attachedWindow.mAttrs.type) * WindowManagerService.TYPE_LAYER_MULTIPLIER
	                    + WindowManagerService.TYPE_LAYER_OFFSET;
	            mSubLayer = mPolicy.subWindowTypeToLayerLw(a.type);
	            mAttachedWindow = attachedWindow;
	            final WindowList childWindows = mAttachedWindow.mChildWindows;
	            final int numChildWindows = childWindows.size();
	            if (numChildWindows == 0) {
	                childWindows.add(this);
	            } else {
	                boolean added = false;
	                for (int i = 0; i < numChildWindows; i++) {
	                    final int childSubLayer = childWindows.get(i).mSubLayer;
	                    if (mSubLayer < childSubLayer
	                            || (mSubLayer == childSubLayer && childSubLayer < 0)) {
	                        // We insert the child window into the list ordered by the sub-layer. For
	                        // same sub-layers, the negative one should go below others; the positive
	                        // one should go above others.
	                        childWindows.add(i, this);
	                        added = true;
	                        break;
	                    }
	                }
	                if (!added) {
	                    childWindows.add(this);
	                }
	            }
	            
在移除一个窗口的时候也会将子窗口移除


    void removeWindowInnerLocked(WindowState win) {
        ...
        <!--递归移除子窗口-->
        for (int i = win.mChildWindows.size() - 1; i >= 0; i--) {
            WindowState cwin = win.mChildWindows.get(i);
            removeWindowInnerLocked(cwin);
        }

        win.mRemoved = true;
		 ...
        mPolicy.removeWindowLw(win);
        <!--移除自己-->
        win.removeLocked();

* 移除Dialog跟移除PopupWindow的区别
* 移除Activity窗口跟Dialog的区别

Activity的窗口移除让AMS控制 ，Dialog的移除APP控制：


    private void removeActivityFromHistoryLocked(
            ActivityRecord r, TaskRecord oldTop, String reason) {
        mStackSupervisor.removeChildActivityContainers(r);
        finishActivityResultsLocked(r, Activity.RESULT_CANCELED, null);
        r.makeFinishingLocked();
        if (DEBUG_ADD_REMOVE) Slog.i(TAG_ADD_REMOVE,
                "Removing activity " + r + " from stack callers=" + Debug.getCallers(5));

        r.takeFromHistory();
        removeTimeoutsForActivityLocked(r);
        if (DEBUG_STATES) Slog.v(TAG_STATES,
                "Moving to DESTROYED: " + r + " (removed from history)");
        r.state = ActivityState.DESTROYED;
        if (DEBUG_APP) Slog.v(TAG_APP, "Clearing app during remove for activity " + r);
        r.app = null;
        mWindowManager.removeAppToken(r.appToken);
        
        
	@Override
    public void removeAppToken(IBinder token) {
        if (!checkCallingPermission(android.Manifest.permission.MANAGE_APP_TOKENS,
                "removeAppToken()")) {
            throw new SecurityException("Requires MANAGE_APP_TOKENS permission");
        }        	
    
    
    
    
#     LeakWindow原理


    private void handleDestroyActivity(IBinder token, boolean finishing,
            int configChanges, boolean getNonConfigInstance) {
        ActivityClientRecord r = performDestroyActivity(token, finishing,
                configChanges, getNonConfigInstance);
        if (r != null) {
            cleanUpPendingRemoveWindows(r, finishing);
            WindowManager wm = r.activity.getWindowManager();
            View v = r.activity.mDecor;
            if (v != null) {
                if (r.activity.mVisibleFromServer) {
                    mNumVisibleActivities--;
                }
                IBinder wtoken = v.getWindowToken();
                if (r.activity.mWindowAdded) {
                    if (r.mPreserveWindow) {
                        r.mPendingRemoveWindow = r.window;
                        r.mPendingRemoveWindowManager = wm;
                        r.window.clearContentView();
                    } else {
                        wm.removeViewImmediate(v);
                    }
                }
                if (wtoken != null && r.mPendingRemoveWindow == null) {
                    WindowManagerGlobal.getInstance().closeAll(wtoken,
                            r.activity.getClass().getName(), "Activity");
                } else if (r.mPendingRemoveWindow != null) {
                    WindowManagerGlobal.getInstance().closeAllExceptView(token, v,
                            r.activity.getClass().getName(), "Activity");
                }
                r.activity.mDecor = null;
            }



    public void closeAllExceptView(IBinder token, View view, String who, String what) {
        synchronized (mLock) {
            int count = mViews.size();
            for (int i = 0; i < count; i++) {
                if ((view == null || mViews.get(i) != view)
                        && (token == null || mParams.get(i).token == token)) {
                    ViewRootImpl root = mRoots.get(i);

                    if (who != null) {
                        WindowLeaked leak = new WindowLeaked(
                                what + " " + who + " has leaked window "
                                + root.getView() + " that was originally added here");
                        leak.setStackTrace(root.getLocation().getStackTrace());
                        Log.e(TAG, "", leak);
                    }

                    removeViewLocked(i, false);
                }
            }
        }
    }
    
# Dialog 与Popwindow leak原因 WindowManager，后果什么样？ 没人管理？？不是的仍然管理
    
Android 中所有的视图都是通过 Window 来呈现的，不管是 Activity，Dialog，Toast。WindowManager 是外界访问 Window 的入口。Android 的每一个 Activity 都有个WindowManager，因此，构建在 Activity 之上的 Dialog、PopupWindow 也有相应的 WindowManager 。因为 Dialog、PopupWindow 不能脱离 Activity 而单独存在着，所以当Dialog 或者 PopupWindow 正在显示的时候去 finish() 了承载该 Dialog( 或 PopupWindow ) 的 Activity 时，就会抛 WindowLeaked 异常了，因为这个Dialog的 WindowManager 已经没有谁可以附属了，所以它的窗体管理器已经泄漏了。

那有什么后果呢？难道无法移除了吗？不应该吧，还是处理了的 ,照样处理onDetachedFromWindow回调：并没有导致Dialog不被回收，也许只是一个提醒，Google也不可能留

	   @Override
	    public void onDetachedFromWindow() {
	        super.onDetachedFromWindow();
	    }
    

他们都是靠着Activity的WindowManager 进行隐藏的？ 没有隐藏的入口，交给统一的管理，内存泄漏？ 并没发现
    
### View的Context来自何处 ，为什么FragmentDialog中View获取的Context不能添加Dialog
                                                                          
# 参考文档

[Android对话框Dialog，PopupWindow，Toast的实现机制](http://blog.csdn.net/feiduclear_up/article/details/49080587)      
[Android窗口机制（五）最终章：WindowManager.LayoutParams和Token以及其他窗口Dialog，Toast](http://www.jianshu.com/p/bac61386d9bf)       