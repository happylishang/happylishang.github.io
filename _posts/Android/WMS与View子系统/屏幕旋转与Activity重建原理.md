---
layout: post
title: 屏幕旋转与Activity重建原理 
categories: Android
image: http://upload-images.jianshu.io/upload_images/1460468-9e553667f4a41a8d.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---

手机在旋转屏幕的时候，如果没有做任何限制的话，系统默认的操作是是销毁当前Activity活动界面，并重建新的活动界面，在这种场景下，如果有些重要的数据没保存，或者某些地方处理不当，轻则引起交互体验差，重则造成程序崩溃，比如全屏播放视频时候，如果旋转屏幕，可能希望不要销毁当前Activity，或者就算销毁了也要从当前进度重新播放等。为什么旋转屏幕可能会销毁当前Activity呢，是怎么销毁并重建的呢，本篇文章主要就是分析旋转界面是，AMS如何销毁并重建Activity的。主要有以下几点问题

* 屏幕旋转的监听：系统如何知道屏幕旋转，并通知AMS与WMS的
* 屏幕旋转后，AMS如何判断是否需要销毁Activity并重建
* 屏幕旋转后，如何不让AMS销毁Activity
* 屏幕旋转后，View的重绘逻辑是怎么触发的
* 系统上的自动旋转是干嘛的？
* 强制横屏或者竖屏
* Destroy后又新建，这个流程是输入relauch，但是只对可见Activity做，怎么区分，不用区分只需要看之前的配置与当前配置是否一致，如果中间有改回去了，就不必，这是时时的，不会浪费资源去处理
* 除了重建最上面的Activity还要保证所有可见的Activity都是跟当前配置一致，悬浮的Activity

首先有一点先记到心里，屏幕旋转并不会导致整个app本身重启，也就是说进程不会新建，最多只会新建Activity。
通过以上几点，可能就会都屏幕旋转有个把控，在开发时候的注意的点也就会相对清晰一些。先声明一点，本篇不怎么涉及View的绘制及重绘，仅仅讲解Activity的重加，以及回调执行的机制。至于View重新绘制的逻辑会单独分析。

# 屏幕旋转的应用场景

* 布局文件是否替换
* 是否需呀保存现场数据
* DIrection变化后，视图的绘制怎么更新的


# 屏幕旋转的等级：Activity级别还是设备级别

通过setRequestedOrientation可以设置Activity级别的屏幕方向，这种情况下，不会对之前的Activity造成任何影响吗，换句话说，当前设置了横屏的Activity 在finish后，上一个Activity仍然走普通的唤醒流程，不存在重建之类的逻辑。屏幕旋转了，就相当于整个坐标系进行了旋转，X变成了Y，绘制的流程没有变，但是参数变了，比如1280X720，就好比变成了720X1280，整个Activity的布局包括顶部状态栏，底部导航栏都要重新绘制。

 handleConfigurationChanged


# Activity主动设置方向会不会导致之前的Activity重建

> 如果是悬浮Activity就会（两次）

finish就恢复吗？注意恢复在销毁之前，恢复的优先级高，也就是说上一个配置的Actvity还未销毁的情况下，前一个已经resume，还要保证配置的正确性，这里怎么保证的

跟踪setRequestedOrientation函数
 
> 如果是全屏Activity就不会

# AndroidManifest里设置屏幕方向，系统如何处理启动的Activity，设置方向的，

为什么不会执行config呢，屏幕方向仅仅是Activity的一个属性，属性而已，不会因为这个属性就导致其他一些额外的操作，只有一些非常规的操作才会导致重建。

        <activity
            android:name=".activity.SecondActivity"
            android:configChanges="orientation|screenSize"
            android:screenOrientation="landscape"/>
            
如何处理，注意区分handlerActivityConfig跟handleConfigurationChanged，前者是针对可见的Activity，但是后者是针对resume的Activity以及启动的Service等，但是不会执行两次。也许某些版本在处理源码的时候，没怎出处理好，就回调了两次，maybe。绘制的坐标系在哪定的

# 屏幕旋转之后，AMS如何处理可见Activity

注意，这里说的是可见Activity，并不是全部Activity，也不是只有TopActivity，首先，如果需要的话，会重建第一个TopActivity，以及当前可见的Activity，finish当前之后，负责relauch第二个，有与已经stop，边执行destroy
我们看一下源码中对于Config变化的处理，直接看AMS中对于Activity的处理：


> ActivityManagerService
	
	  boolean updateConfigurationLocked(Configuration values,
	            ActivityRecord starting, boolean persistent, boolean initLocale) {
	        // do nothing if we are headless
	        if (mHeadless) return true;
	
	        int changes = 0;
	        
	        boolean kept = true;
	        
	        if (values != null) {
	            Configuration newConfig = new Configuration(mConfiguration);
	               ...
	          	   mConfiguration = newConfig;
	              ...
	               关键点1 
	              for (int i=mLruProcesses.size()-1; i>=0; i--) {
                    ProcessRecord app = mLruProcesses.get(i);
                    try {
                        if (app.thread != null) {
                       // 更新每个应用，包括他内部的Service，providers，还有resumed的Activity
                            app.thread.scheduleConfigurationChanged(configCopy);
                        }
                    } catch (Exception e) {
                    }
                }
	        }
	        
	        <!--这里写的真是任性-->
	        找到TopActivity
	        if (changes != 0 && starting == null) {
	            // If the configuration changed, and the caller is not already
	            // in the process of starting an activity, then find the top
	            // activity to check if its configuration needs to change.
	            starting = mMainStack.topRunningActivityLocked(null);
	        }
	        关键点2 
	        if (starting != null) {
	            kept = mMainStack.ensureActivityConfigurationLocked(starting, changes);
	            // And we need to make sure at this point that all other activities
	            // are made visible with the correct configuration.
	            mMainStack.ensureActivitiesVisibleLocked(starting, changes);
	        }
	        关键点 3 
	        if (values != null && mWindowManager != null) {
	            mWindowManager.setNewConfiguration(mConfiguration);
	        }
	        
	        return kept;
	    }
	    
关键点1，这里是针对所有的APP进行一次更新，包括每个APP的Service、Providers等，接着看关键点2，这里首先找到TopActivity，通过mMainStack.ensureActivityConfigurationLocked保证顶层可见Activity配置的正确，之后通过mMainStack.ensureActivitiesVisibleLocked保证所有可见Activity的配置，注意，可见的Activity并不一定是resume的Activity，比如悬浮Activity。首先看一下ActivityStack的ensureActivityConfigurationLocked函数，这个函数主要针对传入的Activity保证其配置跟当前系统配置一致。

先看下ActivityThread如何处理的

> ActivityThread

	 final void handleConfigurationChanged(Configuration config, CompatibilityInfo compat) {
	        int configDiff = 0;
	        synchronized (mPackages) {
	           ...
	          // 更新资源，重启的话，就会用新资源
	            applyConfigurationToResourcesLocked(config, compat);
	            ...
	            
			// 这里收集需要处理的控件信息,包括Service resumed的Activity以及providers
        	ArrayList<ComponentCallbacks2> callbacks = collectComponentCallbacks(false, config);

	        if (callbacks != null) {
	            final int N = callbacks.size();
	            for (int i=0; i<N; i++) {
	                // 如何处理回调
	                performConfigurationChanged(callbacks.get(i), config);
	            }
	        }
	    }
	    
> ActivityStack
	

	    final boolean ensureActivityConfigurationLocked(ActivityRecord r,
	            int globalChanges) {
	            ...
	         <!--关键代码1：Activity无法应对所发生的Config变化-->
	         
	        if ((changes&(~r.info.getRealConfigChanged())) != 0 || r.forceNewConfig) {
	        		// 杀死,这注释，确定不是在搞笑        
	            // Aha, the activity isn't handling the change, so DIE DIE DIE.
	            r.configChangeFlags |= changes;
	            r.startFreezingScreenLocked(r.app, globalChanges);
	            r.forceNewConfig = false;
	            if (r.app == null || r.app.thread == null) {
	               ...
	               
	            } else if (r.state == ActivityState.RESUMED) {
	                //对于正在显示的top Activity，需要立刻restart，并resume，这样才能不影响体验

	                relaunchActivityLocked(r, r.configChangeFlags, true);
	                r.configChangeFlags = 0;
	            } else {
           	     //对于正在显示的Activity，但是没有resume，比如悬浮Activity下面的Activity，需要立刻restart，但是不需要resume获取焦点                
	                relaunchActivityLocked(r, r.configChangeFlags, false);
	                r.configChangeFlags = 0;
	            }
	         //  返回false，告诉AMS，需要重新resumeTopActivity
	            return false;
	        }
	        
	        // activity自己处理config变化，所以不用销毁
	
	        // Default case: the activity can handle this new configuration, so
	        // hand it over.  Note that we don't need to give it the new
	        // configuration, since we always send configuration changes to all
	        // process when they happen so it can just use whatever configuration
	        // it last got.
	        关键代码2 
	        
	        if (r.app != null && r.app.thread != null) {
	            try {
	  	            r.app.thread.scheduleActivityConfigurationChanged(r.appToken);
	            } catch (RemoteException e) {
	            }
	        }
	        r.stopFreezingScreenLocked(false);
	        return true;
	    }
	    

看一下关键代码1：如果Config变化，但是Activity自身没有配置如何处理这种变化，系统就会将当前的Activity杀死，再重建，即relaunchActivityLocked。而关键代码2 ，其实就是Activity本身配置了如何应对这这种变化，这个时候系统就不需要重建Activity，只需要处理Config变化的回调函数即可。不过对于可见Activity的回调究竟有什么不同呢？relaunchActivityLocked的最后一个参数代表什么呢？

	 private final boolean relaunchActivityLocked(ActivityRecord r,
	            int changes, boolean andResume) {
	        List<ResultInfo> results = null;
	        List<Intent> newIntents = null;
	        if (andResume) {
	            results = r.results;
	            newIntents = r.newIntents;
	        }
	        ...
	        r.startFreezingScreenLocked(r.app, 0);
	        try {
	            r.forceNewConfig = false;
	        // 并没有新建ActivityRecord，好像也确实没必要，只要更新就行，并告诉客户端销毁重建，
	        // 毕竟Activity的新建更ActiviytRecord是没太直接联系的
	            r.app.thread.scheduleRelaunchActivity(r.appToken, results, newIntents,
	                    changes, !andResume, new Configuration(mService.mConfiguration));
	        } 
	        ...
	        if (andResume) {
	            r.results = null;
	            r.newIntents = null;
	            if (mMainStack) {
	                mService.reportResumedActivityLocked(r);
	            }
	            r.state = ActivityState.RESUMED;
	        } else {
	            // 防止再执行stop
	            mHandler.removeMessages(PAUSE_TIMEOUT_MSG, r);
	            r.state = ActivityState.PAUSED;
	        }
	
	        return true;
	    }
    
可以看出，最后一个参数名andResume，从名字可以看出，是否要将重建的Activity resume，对于悬浮Activity之下的Activity，虽然可见，但是在AMS是不能看做resume，不然stack回退栈会乱的，ActivityStack里，只能有一个resume的Activity，那看看APP端有什么影响？要记得，对于悬浮的Activity，它的上一个Activity是没哟stop的，只是pause掉了，销毁的时候，要注意销毁的时候，要执行onstop，这也是为什么将状态设置为PAUSE，因为这个Activity的状态就是PAUSE。回来看APP端，我们知道，对于下面的Activity如果可见，我们也是要执行杀死重建的，ActivityThread获得重新信息会通过Handler发送个Relaunch消息，通过Handler执行handleRelaunchActivity：
    
    private void handleRelaunchActivity(ActivityClientRecord tmp) {
          ...
         关键点 1 
        // Need to ensure state is saved.
        if (!r.paused) {
            performPauseActivity(r.token, false, r.isPreHoneycomb());
        }
        if (r.state == null && !r.stopped && !r.isPreHoneycomb()) {
            r.state = new Bundle();
            r.state.setAllowFds(false);
            mInstrumentation.callActivityOnSaveInstanceState(r.activity, r.state);
        }
        关键点 2
        handleDestroyActivity(r.token, false, configChanges, true);
        ...
        关键点 3
        
        r.startsNotResumed = tmp.startsNotResumed;
        handleLaunchActivity(r, currentIntent);
    }
    
以上有三个关键点

* 判断是否安全的pause，并保证OnSaveInstanceState的执行，这样才能保存线程，之后会执行destroy，同样destroy也会保证stop函数的执行
* 通过handleLaunchActivity唤起函数，

到这里还没说刚才那个resume布尔值的意义，它真正起作用的地方在handleLaunchActivity,在进一步就是performLaunchActivity

	 private void handleLaunchActivity(ActivityClientRecord r, Intent customIntent) {
	        ...
	        //处理配置更新
	        handleConfigurationChanged(null, null);
			 //重建
	        Activity a = performLaunchActivity(r, customIntent);
	
	        if (a != null) {
	            r.createdConfig = new Configuration(mConfiguration);
	            Bundle oldState = r.state;
	            handleResumeActivity(r.token, false, r.isForward,
	                    !r.activity.mFinished && !r.startsNotResumed);
	                    ...
		}
    
    
 resume字段的真正意义就是是否告诉AMS真正唤醒一个Activity，对于悬浮的，其实是走了resume的，因为要显示，但是没有告诉ActivityStack，为了保证ActivityStack之后一个resumed的topActivity， 
 
     final void handleResumeActivity(IBinder token, boolean clearHide, boolean isForward,
            boolean reallyResume) {
             ...                
	        if (r != null) {
	            final Activity a = r.activity;
				...
				 // 关键就是这里，这里高不告诉AMS，是保证ActivityStack的流程的关键
	            // Tell the activity manager we have resumed.
	            if (reallyResume) {
	                try {
	                    ActivityManagerNative.getDefault().activityResumed(token);
	                } catch (RemoteException ex) {
	                }
	            }
	
	        } else {...}
	    }
    
可以温习一下performLaunchActivity

    private Activity performLaunchActivity(ActivityClientRecord r, Intent customIntent) {
        ...
        //新建Activity
        Activity activity = null;
        try {
            java.lang.ClassLoader cl = r.packageInfo.getClassLoader();
            activity = mInstrumentation.newActivity(
                    cl, component.getClassName(), r.intent);
            StrictMode.incrementExpectedActivityCount(activity.getClass());
            r.intent.setExtrasClassLoader(cl);
            if (r.state != null) {
                r.state.setClassLoader(cl);
            }
        } catch (Exception e) {
        ...
        }
     
        try {
            ...一系列回调等等
                mInstrumentation.callActivityOnCreate(activity, r.state);
                if (!activity.mCalled) {
                    throw new SuperNotCalledException(
                        "Activity " + r.intent.getComponent().toShortString() +
                        " did not call through to super.onCreate()");
                }
                r.activity = activity;
                r.stopped = true;
                if (!r.activity.mFinished) {
                    activity.performStart();
                    r.stopped = false;
                }
                if (!r.activity.mFinished) {
                    if (r.state != null) {
                        mInstrumentation.callActivityOnRestoreInstanceState(activity, r.state);
                    }
                }
                if (!r.activity.mFinished) {
                    activity.mCalled = false;
                    mInstrumentation.callActivityOnPostCreate(activity, r.state);
                  ...
                }
            }
            r.paused = true;
            mActivities.put(r.token, r);

        }
			...    

        return activity;
    }
    
 接着看AMS下半部分的，其实一致往下找，找到第一个全屏的Actvity，就不在处理配置更新的逻辑了，因为不可见，可以留到后面恢复的时候再用
 
  // 区分好starting跟top的区别，starting是正在启动的Activity top是顶层Activity

    final void ensureActivitiesVisibleLocked(ActivityRecord top,
            ActivityRecord starting, String onlyThisProcess, int configChanges) {
        ...
       关键点1 注意这里的注释，如果topActivity不是全屏（fullscreen），必须保证下面可见Activity的配置正确。
       这里是理解这个函数的核心，后面的逻辑都是在针对activity可见做的逻辑。
        // If the top activity is not fullscreen, then we need to
        // make sure any activities under it are now visible.
        final int count = mHistory.size();
        int i = count-1;
        <!--首先找到top-->
        while (mHistory.get(i) != top) {
            i--;
        }
        ActivityRecord r;
        // 找一个fullActivity就是true，这里其实有个一个找全屏activity的逻辑
        // 注意这里包括第一个acitivity，topActivity，也就是入托top全屏，剩下的就不用遍历了
        boolean behindFullscreen = false;
        for (; i>=0; i--) {
            r = mHistory.get(i);
            ...
            final boolean doThisProcess = onlyThisProcess == null
                    || onlyThisProcess.equals(r.processName);
            
            // First: if this is not the current activity being started, make
            // sure it matches the current configuration.
            if (r != starting && doThisProcess) {
                ensureActivityConfigurationLocked(r, 0);
            }
           ...            
           configChanges |= r.configChangeFlags;
			 
			 关键点2：终止条件找个全屏Activity
			 
            if (r.fullscreen) {
                // At this point, nothing else needs to be shown
                if (DEBUG_VISBILITY) Slog.v(
                        TAG, "Stopping: fullscreen at " + r);
                behindFullscreen = true;
                i--;
                break;
            }
        }
        ...剩下的就是处理不可见Activity逻辑
         
        }
    }
    
一直找到最上面的 全屏Activity完成重建，因为可能出现悬浮Actvity，也是可见的Activity，为了保证体验，在旋转屏幕的时候，可见的Activity多要根据配置更新，或者旋转，或者重建。
    
# View是如何重新绘制的

* 坐标系的更换，这里注意判断
    
 
	    public void setNewConfiguration(Configuration config) {
	        if (!checkCallingPermission(android.Manifest.permission.MANAGE_APP_TOKENS,
	                "setNewConfiguration()")) {
	            throw new SecurityException("Requires MANAGE_APP_TOKENS permission");
	        }
	
	        synchronized(mWindowMap) {
	            mCurConfiguration = new Configuration(config);
	            if (mWaitingForConfig) {
	                mWaitingForConfig = false;
	                mLastFinishedFreezeSource = "new-config";
	            }
	            // 
	            performLayoutAndPlaceSurfacesLocked();
	        }
	    }


	    private final void performLayoutAndPlaceSurfacesLocked() {
	        int loopCount = 6;
	        do {
	            mTraversalScheduled = false;
	            performLayoutAndPlaceSurfacesLockedLoop();
	            mH.removeMessages(H.DO_TRAVERSAL);
	            loopCount--;
	        } while (mTraversalScheduled && loopCount > 0);
	        mInnerFields.mWallpaperActionPending = false;
	    }
	    
  performLayoutAndPlaceSurfacesLocked是重绘的入口，这里会获取一些屏幕方向，以及尺寸的信息，并且将新界面的试图绘制出来，这里不对WMS扩展来讲。
  
# 参考文档

 
[ActivityStack分析](http://www.voidcn.com/blog/guoqifa29/article/p-578977.html)
  
