---
layout: default
title: Activity到底是什么 
categories: [android]

---

> **分析Android框架的时候谨记：上层都是逻辑封装，包括Activity、View，所有的实现均有相应Servcie来处理，比如View的绘制等**

### 目录

 

### Activity

Activity是四大组件之一，那么组件到底是个什么东西，虽然知道Activity是用来显示交互界面，可是Activity本身是一个界面的抽象类吗？是View吗，如果不是那么到底谁负责显示，Activity到底扮演什么角色。

一个应用可以有多个Activity，每个 Activity 一个Window(PhoneWindow)， 每个Window 有一个DecorView, 一个ViewRootImpl, 对应在WindowManagerService 里有一个Window(WindowState).

3. ViewRootImple,  WindowManagerImpl,  WindowManagerGlobals
WindowManagerImpl: 实现了WindowManager 和 ViewManager的接口，但大部分是调用WindowManagerGlobals的接口实现的。

WindowManagerGlobals: 一个SingleTon对象，对象里维护了三个数组：

mRoots[ ]: 存放所有的ViewRootImpl
mViews[ ]: 存放所有的ViewRoot
mParams[ ]: 存放所有的LayoutParams.
IWindowManager:  主要接口是OpenSession(), 用于在WindowManagerService 内部创建和初始化Session, 并返回IBinder对象。
ISession:  是Activity Window与WindowManagerService 进行对话的主要接口.
我们知道，set，但是在此之前就已经实现了一些东西通过 

	final void attach(Context context, ActivityThread aThread,

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


ViewRoot相当于是MVC模型中的Controller，它有以下职责：

        1. 负责为应用程序窗口视图创建Surface。

        2. 配合WindowManagerService来管理系统的应用程序窗口。

        3. 负责管理、布局和渲染应用程序窗口视图的UI。
        
当Activity组件被激活的时候，系统如果发现与它的应用程序窗口视图对象所关联的ViewRoot对象还没有创建，那么就会先创建这个ViewRoot对象，以便接下来可以将它的UI渲染出来。Activity组件创建完成之后，就可以将它激活起来了，这是通过调用ActivityThread类的成员函数handleResumeActivity来执行的。 从前面Android应用程序窗口（Activity）的窗口对象（Window）的创建过程分析一文可以知道，LocalWindowManager类的成员变量mWindowManager指向的是一个WindowManagerImpl对
    
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
            
### ViewRoot本质
            
ViewRoot是GUI管理系统与GUI呈现系统之间的桥梁，根据ViewRoot的定义，我们发现它并不是一个View类型，而是一个Handler。ViewRoot这个类在android的UI结构中扮演的是一个中间者的角色，连接的是PhoneWindow跟WindowManagerService，

它的主要作用如下：

A. 向DecorView分发收到的用户发起的event事件，如按键，触屏，轨迹球等事件；

B. 与WindowManagerService交互，完成整个Activity的GUI的绘制。
(2)   sWindowSessoin.add()


requestLayout();  

try {  
    res = sWindowSession.add(mWindow, mWindowAttributes,  
            getHostVisibility(), mAttachInfo.mContentInsets);  
} catch (RemoteException e) {  

在这个方法中只需要关注两个步骤

> requestLayout()

  请求WindowManagerService绘制GUI，但是注意一点的是它是在与WindowManagerService建立连接之前绘制，为什么要在建立之前请求绘制呢？其实两者实际的先后顺序是正好相反的，与WMS建立连接在前，绘制GUI在后，那么为什么代码的顺序和执行的顺序不同呢？这里就涉及到ViewRoot的属性了，我们前面提到ViewRoot并不是一个View，而是一个Handler，那么执行的具体流程就是这样的：
    从字面意思理解的话，IWindowSession sWindowSessoin是ViewRoot和WindowManagerService之间的一个会话层，它的实体是在WMS中定义，作为ViewRoot requests WMS的桥梁。

add()方法的第一个参数mWindow是ViewRoot提供给WMS，以便WMS反向通知ViewRoot的接口。由于ViewRoot处在application端，而WMS处在system_server进程，它们处在不同的进程间，因此需要添加这个IWindow接口便于GUI绘制状态的同步。

![](http://hi.csdn.net/attachment/201111/10/0_13209336991GIN.gif)

### 参考文档

图解Android - Android GUI 系统 (2) - 窗口管理 (View, Canvas, Window Manager) <http://www.cnblogs.com/samchen2009/p/3367496.html>
 Android 4.4(KitKat)窗口管理子系统 - 体系框架 <http://blog.csdn.net/jinzhuojun/article/details/37737439>
 