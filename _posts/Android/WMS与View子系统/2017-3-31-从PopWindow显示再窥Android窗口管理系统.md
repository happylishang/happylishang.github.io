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


这里是Activity Dialog复用的关键

    // zhe
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
