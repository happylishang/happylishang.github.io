---
layout: post
title: 屏幕旋转与Activity重建原理 
categories: Android

---

在屏幕旋转的时候，经常发生一些异常或者重建，比如全屏播放视频在关闭屏幕的时候，等等，本篇文章主要讲解一下几点问题

* 屏幕旋转的监听：系统如何知道屏幕旋转，并通知AMS与WMS的
* 屏幕旋转后的处理，需要销毁Activity并重建吗
* 屏幕旋转后端的处理，应对杀死及configchange
* 系统上的自动旋转是干嘛的？
* 强制横屏或者竖屏
* app本身不会从起，进程不会新建，只会新建Activity
* Destroy后又新建，这个流程是输入relauch，但是只对可见Activity做，怎么区分，不用区分只需要看之前的配置与当前配置是否一致，如果中间有改回去了，就不必，这是时时的，不会浪费资源去处理
* 强制竖屏怎么处理的
* 除了重建最上面的Activity还要保证所有可见的Activity都是跟当前配置一致，悬浮的Activity

通过以上几点，可能就会都屏幕旋转有个把控，在开发时候的注意的点也就会相对清晰一些。先声明一点，本篇不怎么涉及View的绘制及重绘，仅仅讲解Activity的重加，以及回调执行的机制。至于View重新绘制的逻辑会单独分析。

# 屏幕旋转的应用场景

* 布局文件是否替换
* 是否需呀保存现场数据
* DIrection变化后，视图的绘制怎么更新的


# 屏幕旋转的等级：Activity级别还是设备级别

Activity通过setRequestedOrientation设置的Activity级别的屏幕方向会对之前的Activity造成什么影响吗？NO，一点影响没有系统的方向没变的。其实屏幕旋转了，就相当于整个坐标系进行了旋转，X变成了Y，绘制的流程没有变，但是参数变了，比如1280X720，就好比变成了720X1280，整个Activity的布局包括顶部状态栏，底部导航栏都要重新绘制。

	    boolean updateConfigurationLocked(Configuration values,
	            ActivityRecord starting, boolean persistent, boolean initLocale) {
	       ...
	       if (values != null) {
	         ...
	                for (int i=mLruProcesses.size()-1; i>=0; i--) {
	                    ProcessRecord app = mLruProcesses.get(i);
	                    try {
	                        if (app.thread != null) {
	                            if (DEBUG_CONFIGURATION) Slog.v(TAG, "Sending to proc "
	                                    + app.processName + " new config " + mConfiguration);
	                                // 这里的配置是做什么的，更新每个应用？
	                            app.thread.scheduleConfigurationChanged(configCopy);
	                        }
	                    } catch (Exception e) {
	                    }
	                }
	                ...
	}                

handleConfigurationChanged

	 final void handleConfigurationChanged(Configuration config, CompatibilityInfo compat) {
	        int configDiff = 0;
	        synchronized (mPackages) {
	           ...
	          // 更新资源，重启的话，就会用新资源
	            applyConfigurationToResourcesLocked(config, compat);
	            ...
	        if (callbacks != null) {
	            final int N = callbacks.size();
	            for (int i=0; i<N; i++) {
	                // 如何处理回调
	                performConfigurationChanged(callbacks.get(i), config);
	            }
	        }
	    }
    
 

# AndroidManifest里设置屏幕方向，系统如何处理启动的Activity，设置方向的，

为什么不会执行config呢，屏幕方向仅仅是Activity的一个属性，属性而已，不会因为这个属性就导致其他一些额外的操作，只有一些非常规的操作才会导致重建。

        <activity
            android:name=".activity.SecondActivity"
            android:configChanges="orientation|screenSize"
            android:screenOrientation="landscape"/>
            
如何处理，注意区分handlerActivityConfig跟handleConfigurationChanged，前者是针对可见的Activity，但是后者是针对resume的Activity以及启动的Service等，但是不会执行两次。也许某些版本在处理源码的时候，没怎出处理好，就回调了两次，maybe。

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
                                // 更新每个应用
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
  
