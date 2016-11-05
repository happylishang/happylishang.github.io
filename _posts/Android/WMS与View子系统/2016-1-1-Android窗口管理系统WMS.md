---
layout: default
title: Android 窗口管理系统入门 
categories: [android]

---

> **分析Android框架的时候谨记：上层都是逻辑封装，包括Activity、View，所有的实现均有相应Servcie来处理，比如View的绘制等**

### 目录

* 窗口和图形系统 - Window and View Manager System.
* 显示合成系统 - Surface Flinger
* 用户输入系统 - InputManager System
* 应用框架系统 - Activity Manager System.
* 何时创建窗口performlauch
* Theme的设定，AMS在启动的时候就获取了style，然后在ActivityThread启动Activity的时候Lauch的时候，会主动将Themeid设置进去，其实APK在编译的时候，资源就已经编号了。
* 资源编号的的bundle
* 状态栏的创建

#几种Window的区别

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
	    
	          
           
### 参考文档

 图解Android - Android GUI 系统 (2) - 窗口管理 (View, Canvas, Window Manager) <http://www.cnblogs.com/samchen2009/p/3367496.html>
 
 Android 4.4(KitKat)窗口管理子系统 - 体系框架 <http://blog.csdn.net/jinzhuojun/article/details/37737439>
  
 Android桌面悬浮窗效果实现，仿360手机卫士悬浮窗效果 <http://blog.csdn.net/guolin_blog/article/details/8689140> 
 
 [ Android应用Activity、Dialog、PopWindow、Toast窗口添加机制及源码分析](http://blog.csdn.net/yanbober/article/details/46361191)