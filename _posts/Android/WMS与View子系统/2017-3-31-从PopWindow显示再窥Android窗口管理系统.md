---
layout: post
title: 从PopWindow显示再窥Android窗口管理系统
category: Android
image: 

---


PopupWindow#invokePopup源码如下：

private void invokePopup(WindowManager.LayoutParams p) {
        if (mContext != null) {
            p.packageName = mContext.getPackageName();
        }
        mPopupView.setFitsSystemWindows(mLayoutInsetDecor);
        setLayoutDirectionFromAnchor();
        mWindowManager.addView(mPopupView, p);
    }

分析： 
该方法也很简单，主要是调用了WindowManager#addView方法来添加对话框视图。从而PopupWindow对话框显示在Activity应用窗口之上了。


[Android对话框Dialog，PopupWindow，Toast的实现机制  ](http://blog.csdn.net/feiduclear_up/article/details/49080587)          

Activity有一个  android.view.Window PhoneWindow   public abstract class Window { 管理抽象的Activity窗口 Dialog用的

ViewrootImpl有个  final W mWindow; 这两个不同      static class W extends IWindow.Stub {


PopWindow的窗口类型      private int mWindowLayoutType = WindowManager.LayoutParams.TYPE_APPLICATION_PANEL;

Toast的窗口类型              params.type = WindowManager.LayoutParams.TYPE_TOAST;

Dialog的窗口类型跟Activity一样  WindowManager.LayoutParams.TYPE_APPLICATION


WmS 眼中的，窗口是可以显示用来显示的 View。对于 WmS 而言，所谓的窗口就是一个通过 WindowManagerGlobal.addView()添加的 View 罢了


Dialog和Activity共享同一个WindowManager（也就是上面分析的WindowManagerImpl），而WindowManagerImpl里面有个Window类型的mParentWindow变量，这个变量在Activity的attach中创建WindowManagerImpl时传入的为当前Activity的Window，而当前Activity的Window里面的mAppToken值又为当前Activity的token，所以Activity与Dialog共享了同一个mAppToken值，只是Dialog和Activity的Window对象不同。

[Android窗口机制（五）最终章：WindowManager.LayoutParams和Token以及其他窗口Dialog，Toast](http://www.jianshu.com/p/bac61386d9bf)


这里是Activity Dialog复用的关键， 是Activity覆盖了  getSystemService函数里面的  mWindowManager就是Dialog使用的Manager，并且Window的Manager中，有个mParentWindow变量，是Activity中window自己。  mWindowManager = mWindow.getWindowManager();


> Activity.java

    @Override
    public Object getSystemService(String name) {
        if (getBaseContext() == null) {
            throw new IllegalStateException(
                    "System services not available to Activities before onCreate()");
        }

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


LayoutParams中token是WMS用来处理tokenmap跟,而IWindow主要是用来处理mWindowMap的。

*  tokenmap 传递的token竟然在 WindowManager.LayoutParams attrs中
*  windowmap的key用的是IWindow
*  mWindowMap 与 mTokenMap都是系统唯一的。这个系统维护一份

多个Windowstate 对应一个windowToken

如何理解WindowToken 对于Popinwindow，是个子窗口，需要有响应的Token，什么样的Token？

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

    