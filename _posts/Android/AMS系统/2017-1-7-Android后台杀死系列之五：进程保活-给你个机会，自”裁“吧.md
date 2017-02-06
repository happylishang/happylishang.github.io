---
layout: post
title: "Android后台杀死系列之五：进程保活-给你个机会，自”裁“吧"
category: Android

---
 
 


# 优先级的计算

1、是否只是改变自身的优先级
2、是否引起其他有限级的改变
3、后台的优先级变化，退回后台引起整个列表的变化吗？，还是只是几个


# 进程保活

全集中在一个函数中  final void updateOomAdjLocked() ，这个函数先计算优先级，再清理，再瘦身
 
# 进程过多杀

# 优先级改变杀

# MAS杀进程

# AMS会杀死进程吗？

# trimeMemory时机

# 什么时候回到onLowmemory-app.reportLowMemory==true，所有的后台进程，都被干掉的情况下


	
	 final void appDiedLocked(ProcessRecord app, int pid,
	            IApplicationThread thread) {
	
	        mProcDeaths[0]++;
	        
	        BatteryStatsImpl stats = mBatteryStatsService.getActiveStatistics();
	        synchronized (stats) {
	            stats.noteProcessDiedLocked(app.info.uid, pid);
	        }
	
	        // Clean up already done if the process has been re-started.
	        // 重启？
	        if (app.pid == pid && app.thread != null &&
	                app.thread.asBinder() == thread.asBinder()) {
	            if (!app.killedBackground) {
	                Slog.i(TAG, "Process " + app.processName + " (pid " + pid
	                        + ") has died.");
	            }
	            EventLog.writeEvent(EventLogTags.AM_PROC_DIED, app.userId, app.pid, app.processName);
	            if (DEBUG_CLEANUP) Slog.v(
	                TAG, "Dying app: " + app + ", pid: " + pid
	                + ", thread: " + thread.asBinder());
	            boolean doLowMem = app.instrumentationClass == null;
	            handleAppDiedLocked(app, false, true);
	            // 是不是因为内存紧张导致的LowmemoryKiller机制生效，杀死的进程
	            if (doLowMem) {
	                // If there are no longer any background processes running,
	                // and the app that died was not running instrumentation,
	                // then tell everyone we are now low on memory.
	
	                // 通知低内存，大家注意，已经有富农被杀了，打土豪分田地的那哥们出来了，自觉的破财消灾吧
	
	                boolean haveBg = false;
	                for (int i=mLruProcesses.size()-1; i>=0; i--) {
	                    ProcessRecord rec = mLruProcesses.get(i);
	                    if (rec.thread != null && rec.setAdj >= ProcessList.HIDDEN_APP_MIN_ADJ) {
	                        haveBg = true;
	                        break;
	                    }
	                }
	                // 是不是所有的后台都被杀死了，如果都被杀死了，通知前台的app，快向组织交钱，包名要紧
	                if (!haveBg) {
	                    EventLog.writeEvent(EventLogTags.AM_LOW_MEMORY, mLruProcesses.size());
	                    long now = SystemClock.uptimeMillis();
	                    for (int i=mLruProcesses.size()-1; i>=0; i--) {
	                        ProcessRecord rec = mLruProcesses.get(i);
	                        if (rec != app && rec.thread != null &&
	                                (rec.lastLowMemory+GC_MIN_INTERVAL) <= now) {
	                            // The low memory report is overriding any current
	                            // state for a GC request.  Make sure to do
	                            // heavy/important/visible/foreground processes first.
	                            if (rec.setAdj <= ProcessList.HEAVY_WEIGHT_APP_ADJ) {
	                                rec.lastRequestedGc = 0;
	                            } else {
	                                rec.lastRequestedGc = rec.lastLowMemory;
	                            }
	                            rec.reportLowMemory = true;
	                            rec.lastLowMemory = now;
	                            mProcessesToGc.remove(rec);
	                            addProcessToGcListLocked(rec);
	                        }
	                    }
	                    // gc 
	                    mHandler.sendEmptyMessage(REPORT_MEM_USAGE);
	                    scheduleAppGcsLocked();
	                }
	            }
	        } else if (app.pid != pid) {
	            // A new process has already been started.
	            Slog.i(TAG, "Process " + app.processName + " (pid " + pid
	                    + ") has died and restarted (pid " + app.pid + ").");
	            EventLog.writeEvent(EventLogTags.AM_PROC_DIED, app.userId, app.pid, app.processName);
	        } else if (DEBUG_PROCESSES) {
	            Slog.d(TAG, "Received spurious death notification for thread "
	                    + thread.asBinder());
	        }
	    }
	


    final void performAppGcLocked(ProcessRecord app) {
        try {
            app.lastRequestedGc = SystemClock.uptimeMillis();
            if (app.thread != null) {
                // app.reportLowMemory app虚拟机的内存是不是不够了？？
                if (app.reportLowMemory) {
                    app.reportLowMemory = false;
                    // 这里才真正的调用APP 的onLowmemory
                    app.thread.scheduleLowMemory();
                } else {
                    app.thread.processInBackground();
                }
            }
        } catch (Exception e) {
            // whatever.
        }
    }
    

答案是肯定的AMS，也会有选择的杀死进程，不过跟当前的内存没太大关系，而是根据当前启动APP的数量，比如空的APP过多，或者后台APP过多，都可能引起后台杀死，比如在4.3上，如果后台的APP超过24个一般就会触发AMS杀进程，要么杀空进程，要么杀靠后的隐藏进程。


OnTrimMemory:Android系统从4.0开始还提供了onTrimMemory()的回调，当系统内存达到某些条件的时候，所有正在运行的应用都会收到这个回调， 同时在这个回调里面会传递以下的参数，代表不同的内存使用情况，收到onTrimMemory()回调的时候，需要根据传递的参数类型进行判断， 合理的选择释放自身的一些内存占用，一方面可以提高系统的整体运行流畅度，另外也可以避免自己被系统判断为优先需要杀掉的应用。

# 进程保活 

前面讲到，后台杀死的原理，假如进程进入后台，系统就不管了了？知道内存不够才去回收，当然不是，总要提前警告一次，才能抄家伙，上来就杀，太不讲人情，Android也是如此，先给App一个悔过的机会，让APP瘦身。

两个入口：要根据自己的场景判断

    @Override
    public void onTerminate() {
        super.onTerminate();
     }

    @Override
    public void onLowMemory() {
        super.onLowMemory();
     }



# app.trimMemoryLevel 裁剪APP的时机，

其实在每次updateOomAdjLocked更新优先级的时候，都会对一些后台的进程，还有空进程发出警告，告诉他们去裁剪内存，以尽可能的避免内存不足造成被异常杀死，其实AMS也会杀死进程开动太多的话


    final void updateOomAdjLocked() {
        final ActivityRecord TOP_ACT = resumedAppLocked();
        final ProcessRecord TOP_APP = TOP_ACT != null ? TOP_ACT.app : null;
        final long oldTime = SystemClock.uptimeMillis() - ProcessList.MAX_EMPTY_TIME;

        if (false) {
            RuntimeException e = new RuntimeException();
            e.fillInStackTrace();
            Slog.i(TAG, "updateOomAdj: top=" + TOP_ACT, e);
        }

        mAdjSeq++;
        mNewNumServiceProcs = 0;

        final int emptyProcessLimit;
        final int hiddenProcessLimit;

        // 默认是24个最大
        if (mProcessLimit <= 0) {
            emptyProcessLimit = hiddenProcessLimit = 0;
        } else if (mProcessLimit == 1) {
            emptyProcessLimit = 1;
            hiddenProcessLimit = 0;
        } else {
            // 16个空APP进程
            emptyProcessLimit = (mProcessLimit*2)/3;
            // 8个后台进程
            hiddenProcessLimit = mProcessLimit - emptyProcessLimit;
        }

        // Let's determine how many processes we have running vs.
        // how many slots we have for background processes; we may want
        // to put multiple processes in a slot of there are enough of
        // them.
        int numSlots = (ProcessList.HIDDEN_APP_MAX_ADJ
                - ProcessList.HIDDEN_APP_MIN_ADJ + 1) / 2;
        // 后台 前台 空
        int numEmptyProcs = mLruProcesses.size()-mNumNonHiddenProcs-mNumHiddenProcs;
        if (numEmptyProcs > hiddenProcessLimit) {
            // If there are more empty processes than our limit on hidden
            // processes, then use the hidden process limit for the factor.
            // This ensures that the really old empty processes get pushed
            // down to the bottom, so if we are running low on memory we will
            // have a better chance at keeping around more hidden processes
            // instead of a gazillion empty processes.
            // 保存尽量多的后台非空APP
            numEmptyProcs = hiddenProcessLimit;
        }
        int emptyFactor = numEmptyProcs/numSlots;
        if (emptyFactor < 1) emptyFactor = 1;
        int hiddenFactor = (mNumHiddenProcs > 0 ? mNumHiddenProcs : 1)/numSlots;
        if (hiddenFactor < 1) hiddenFactor = 1;
        int stepHidden = 0;
        int stepEmpty = 0;
        int numHidden = 0;
        int numEmpty = 0;
        int numTrimming = 0;

        mNumNonHiddenProcs = 0;
        mNumHiddenProcs = 0;

        // 评估优先级 oomADJ
        // First update the OOM adjustment for each of the
        // application processes based on their current state.
        int i = mLruProcesses.size();
        // 优先级
        int curHiddenAdj = ProcessList.HIDDEN_APP_MIN_ADJ;
        // 初始化的一些值
        int nextHiddenAdj = curHiddenAdj+1;
        // 优先级
        int curEmptyAdj = ProcessList.HIDDEN_APP_MIN_ADJ;
        // 有意思
        int nextEmptyAdj = curEmptyAdj+2;

        int curClientHiddenAdj = curEmptyAdj;
        // 计算优先级，倒着计算
        while (i > 0) {
            i--;
            ProcessRecord app = mLruProcesses.get(i);
            //Slog.i(TAG, "OOM " + app + ": cur hidden=" + curHiddenAdj);
            updateOomAdjLocked(app, curHiddenAdj, curClientHiddenAdj, curEmptyAdj, TOP_APP, true);
           // 还未被杀死
            if (!app.killedBackground) {
                // 有Activity
                if (app.curRawAdj == curHiddenAdj && app.hasActivities) {
                    // This process was assigned as a hidden process...  step the
                    // hidden level.
                    mNumHiddenProcs++;
                    if (curHiddenAdj != nextHiddenAdj) {
                        stepHidden++;
                        if (stepHidden >= hiddenFactor) {
                            stepHidden = 0;
                            curHiddenAdj = nextHiddenAdj;
                            nextHiddenAdj += 2;
                            if (nextHiddenAdj > ProcessList.HIDDEN_APP_MAX_ADJ) {
                                nextHiddenAdj = ProcessList.HIDDEN_APP_MAX_ADJ;
                            }
                            if (curClientHiddenAdj <= curHiddenAdj) {
                                curClientHiddenAdj = curHiddenAdj + 1;
                                if (curClientHiddenAdj > ProcessList.HIDDEN_APP_MAX_ADJ) {
                                    curClientHiddenAdj = ProcessList.HIDDEN_APP_MAX_ADJ;
                                }
                            }
                        }
                    }
                    // hiden的进程，开始杀进程，过多了开始杀，如果超过了，可以杀死
                    numHidden++;
                    if (numHidden > hiddenProcessLimit) {
                        Slog.i(TAG, "No longer want " + app.processName
                                + " (pid " + app.pid + "): hidden #" + numHidden);
                        EventLog.writeEvent(EventLogTags.AM_KILL, app.userId, app.pid,
                                app.processName, app.setAdj, "too many background");
                        app.killedBackground = true;
                        Process.killProcessQuiet(app.pid);
                    }
                } else if (app.curRawAdj == curHiddenAdj && app.hasClientActivities) {
                    // This process has a client that has activities.  We will have
                    // given it the current hidden adj; here we will just leave it
                    // without stepping the hidden adj.
                    curClientHiddenAdj++;
                    if (curClientHiddenAdj > ProcessList.HIDDEN_APP_MAX_ADJ) {
                        curClientHiddenAdj = ProcessList.HIDDEN_APP_MAX_ADJ;
                    }
                } else {
                    // 空进程，没activity
                    if (app.curRawAdj == curEmptyAdj || app.curRawAdj == curHiddenAdj) {
                        // This process was assigned as an empty process...  step the
                        // empty level.
                        if (curEmptyAdj != nextEmptyAdj) {
                            stepEmpty++;
                            if (stepEmpty >= emptyFactor) {
                                stepEmpty = 0;
                                curEmptyAdj = nextEmptyAdj;
                                nextEmptyAdj += 2;
                                if (nextEmptyAdj > ProcessList.HIDDEN_APP_MAX_ADJ) {
                                    nextEmptyAdj = ProcessList.HIDDEN_APP_MAX_ADJ;
                                }
                            }
                        }
                    } else if (app.curRawAdj < ProcessList.HIDDEN_APP_MIN_ADJ) {
                        mNumNonHiddenProcs++;
                    }
                    // 空进程可以杀死，计算的优先级是不是更低
                    if (app.curAdj >= ProcessList.HIDDEN_APP_MIN_ADJ
                            && !app.hasClientActivities) {
                        // 如果超过限制的numEmpty，也开始杀空进程，超过三个，并且lastActivityTime时间久了
                        if (numEmpty > ProcessList.TRIM_EMPTY_APPS
                                && app.lastActivityTime < oldTime) {
                            Slog.i(TAG, "No longer want " + app.processName
                                    + " (pid " + app.pid + "): empty for "
                                    + ((oldTime+ProcessList.MAX_EMPTY_TIME-app.lastActivityTime)
                                            / 1000) + "s");
                            EventLog.writeEvent(EventLogTags.AM_KILL, app.userId, app.pid,
                                    app.processName, app.setAdj, "old background process");
                            app.killedBackground = true;
                            Process.killProcessQuiet(app.pid);
                        } else {
                           // 如果超过限制的numEmpty，也开始杀空进程
                            numEmpty++;
                            if (numEmpty > emptyProcessLimit) {
                                Slog.i(TAG, "No longer want " + app.processName
                                        + " (pid " + app.pid + "): empty #" + numEmpty);
                                EventLog.writeEvent(EventLogTags.AM_KILL, app.userId, app.pid,
                                        app.processName, app.setAdj, "too many background");
                                app.killedBackground = true;
                                Process.killProcessQuiet(app.pid);
                            }
                        }
                    }
                }
                if (app.isolated && app.services.size() <= 0) {
                    // If this is an isolated process, and there are no
                    // services running in it, then the process is no longer
                    // needed.  We agressively kill these because we can by
                    // definition not re-use the same process again, and it is
                    // good to avoid having whatever code was running in them
                    // left sitting around after no longer needed.
                    Slog.i(TAG, "Isolated process " + app.processName
                            + " (pid " + app.pid + ") no longer needed");
                    EventLog.writeEvent(EventLogTags.AM_KILL, app.userId, app.pid,
                            app.processName, app.setAdj, "isolated not needed");
                    app.killedBackground = true;
                    Process.killProcessQuiet(app.pid);
                }

                // 裁剪的numTrimming
                if (app.nonStoppingAdj >= ProcessList.HOME_APP_ADJ
                        && app.nonStoppingAdj != ProcessList.SERVICE_B_ADJ
                        && !app.killedBackground) {
                    numTrimming++;
                }
            }
        }


        mNumServiceProcs = mNewNumServiceProcs;

        // Now determine the memory trimming level of background processes.
        // Unfortunately we need to start at the back of the list to do this
        // properly.  We only do this if the number of background apps we
        // are managing to keep around is less than half the maximum we desire;
        // if we are keeping a good number around, we'll let them use whatever
        // memory they want.

        // 裁剪内存，杀死的时候，内核的Lowmemorykiller会自动计算
        // 决定后台进程裁剪等级，尽量让存活的app保持在最大量的一半，如果本来就少于一半，就不太用关心

        if (numHidden <= ProcessList.TRIM_HIDDEN_APPS
                && numEmpty <= ProcessList.TRIM_EMPTY_APPS) {
            final int numHiddenAndEmpty = numHidden + numEmpty;
            final int N = mLruProcesses.size();
            int factor = numTrimming/3;
            int minFactor = 2;
            if (mHomeProcess != null) minFactor++;
            if (mPreviousProcess != null) minFactor++;
            if (factor < minFactor) factor = minFactor;
            // step干嘛用的
            int step = 0;
            int fgTrimLevel;
            // 看看总数的有多少隐藏APP，决定一下修建的level，裁剪的等级，裁剪的等级，跟后台数量的关系
            // 进程越多，优先级就越高，越紧急，你们看着办，不办，就杀你们

            if (numHiddenAndEmpty <= ProcessList.TRIM_CRITICAL_THRESHOLD) {
                fgTrimLevel = ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL;
            } else if (numHiddenAndEmpty <= ProcessList.TRIM_LOW_THRESHOLD) {
                fgTrimLevel = ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW;
            } else {
                fgTrimLevel = ComponentCallbacks2.TRIM_MEMORY_RUNNING_MODERATE;
            }
            // 预先给个level什么意思 ，好像curLevel跟当前APP数量有关系，数量少的时候才会参考

            int curLevel = ComponentCallbacks2.TRIM_MEMORY_COMPLETE;//使劲释放

            
            // 这里正序的裁剪
            for (i=0; i<N; i++) {
                ProcessRecord app = mLruProcesses.get(i);
                if (app.nonStoppingAdj >= ProcessList.HOME_APP_ADJ
                        && app.nonStoppingAdj != ProcessList.SERVICE_B_ADJ
                        && !app.killedBackground) {
                    // 什么时候裁剪内存？
                    // 什么时候发出内存低的信号
                    if (app.trimMemoryLevel < curLevel && app.thread != null) {
                        try {
                            app.thread.scheduleTrimMemory(curLevel);
                        } catch (RemoteException e) {
                        }
                        if (false) {
                            // For now we won't do this; our memory trimming seems
                            // to be good enough at this point that destroying
                            // activities causes more harm than good.
                            if (curLevel >= ComponentCallbacks2.TRIM_MEMORY_COMPLETE
                                    && app != mHomeProcess && app != mPreviousProcess) {
                                // Need to do this on its own message because the stack may not
                                // be in a consistent state at this point.
                                // For these apps we will also finish their activities
                                // to help them free memory.
                                mMainStack.scheduleDestroyActivities(app, false, "trim");
                            }
                        }
                    }
                    // 是否被裁剪过，被裁剪过，就设置一个等级
                    // 这里裁剪是干嘛的 // 先调节到最大
                    app.trimMemoryLevel = curLevel; 
                    step++;
                    if (step >= factor) {
                        step = 0;
                        switch (curLevel) {
                            case ComponentCallbacks2.TRIM_MEMORY_COMPLETE:
                                curLevel = ComponentCallbacks2.TRIM_MEMORY_MODERATE;
                                break;
                            case ComponentCallbacks2.TRIM_MEMORY_MODERATE:
                                curLevel = ComponentCallbacks2.TRIM_MEMORY_BACKGROUND;
                                break;
                        }
                    }
                } else if (app.nonStoppingAdj == ProcessList.HEAVY_WEIGHT_APP_ADJ) {
                    if (app.trimMemoryLevel < ComponentCallbacks2.TRIM_MEMORY_BACKGROUND
                            && app.thread != null) {
                        try {
                            app.thread.scheduleTrimMemory(
                                    ComponentCallbacks2.TRIM_MEMORY_BACKGROUND);
                        } catch (RemoteException e) {
                        }
                    }
                    app.trimMemoryLevel = ComponentCallbacks2.TRIM_MEMORY_BACKGROUND;
                } else {
                    if ((app.nonStoppingAdj > ProcessList.VISIBLE_APP_ADJ || app.systemNoUi)
                            && app.pendingUiClean) {
                        // If this application is now in the background and it
                        // had done UI, then give it the special trim level to
                        // have it free UI resources.
                        final int level = ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN;
                        if (app.trimMemoryLevel < level && app.thread != null) {
                            try {
                                app.thread.scheduleTrimMemory(level);
                            } catch (RemoteException e) {
                            }
                        }
                        app.pendingUiClean = false;
                    }
                    if (app.trimMemoryLevel < fgTrimLevel && app.thread != null) {
                        try {
                            app.thread.scheduleTrimMemory(fgTrimLevel);
                        } catch (RemoteException e) {
                        }
                    }
                    app.trimMemoryLevel = fgTrimLevel;
                }
            }
        } else {
            // 如果超过一定数量，都会调用
            final int N = mLruProcesses.size();
            for (i=0; i<N; i++) {
                ProcessRecord app = mLruProcesses.get(i);
                if ((app.nonStoppingAdj > ProcessList.VISIBLE_APP_ADJ || app.systemNoUi)
                        && app.pendingUiClean) {
                    if (app.trimMemoryLevel < ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN
                            && app.thread != null) {
                        try {
                            //裁剪的等级TRIM_MEMORY_UI_HIDDEN，多了都裁剪
                            app.thread.scheduleTrimMemory(
                                    ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN);
                        } catch (RemoteException e) {
                        }
                    }
                    app.pendingUiClean = false;
                }
                app.trimMemoryLevel = 0;
            }
        }

        // 这个是调试模式的时候，不保留活动的入口

        if (mAlwaysFinishActivities) {
            // Need to do this on its own message because the stack may not
            // be in a consistent state at this point.
            mMainStack.scheduleDestroyActivities(null, false, "always-finish");
        }
    }
    


	 final void updateOomAdjLocked() 
	  
	 app.thread.scheduleTrimMemory(curLevel);

并不是所有的都要裁剪，4.3系统上，后台APP存在Activity即不存在存活Activity的都不多于三个，就不用裁剪

        // 决定后台进程裁剪等级，尽量让存活的app保持在最大量的一半，如果本来就少于一半，就不用关心

        // Now determine the memory trimming level of background processes.
        // Unfortunately we need to start at the back of the list to do this
        // properly.  We only do this if the number of background apps we
        // are managing to keep around is less than half the maximum we desire;
        // if we are keeping a good number around, we'll let them use whatever
        // memory they want.

        if (numHidden <= ProcessList.TRIM_HIDDEN_APPS
                && numEmpty <= ProcessList.TRIM_EMPTY_APPS) {
            final int numHiddenAndEmpty = numHidden + numEmpty;
            final int N = mLruProcesses.size();
            int factor = numTrimming/3;

  
  
OnLowMemory()和OnTrimMemory()的比较

* OnLowMemory被回调时，已经没有后台进程；而onTrimMemory被回调时，还有后台进程。
* OnLowMemory是在最后一个后台进程被杀时调用，一般情况是low memory killer 杀进程后触发；而OnTrimMemory的触发更频繁， 每次计算进程优先级时，只要满足条件，都会触发。
* 通过一键清理后，OnLowMemory不会被触发，而OnTrimMemory会被触发一次。



# 不要吃得太胖，会被杀死的（千与千寻）迟到的scheduleLowMemory

在LowmemoryKiller原理的讲解中，我们分析了低优先级的进程会被杀死，那些不可见的进程优先级在相同的情况下，系统会先杀占用内存多的。


	
	scheduleAppGcsLocked
	
	performAppGcsIfAppropriateLocked
	
	performAppGcsLocked

    /**
     * Ask a given process to GC right now.
     */
    final void performAppGcLocked(ProcessRecord app) {
        try {
            app.lastRequestedGc = SystemClock.uptimeMillis();
            if (app.thread != null) {
                if (app.reportLowMemory) {
                    app.reportLowMemory = false;
                    app.thread.scheduleLowMemory();
                } else {
                    app.thread.processInBackground();
                }
            }
        } catch (Exception e) {
            // whatever.
        }
    }
    
这时候会回调APP的scheduleLowMemory，提供一个瘦身的机会减少内存

Runtime.getRuntime().gc();

    
 
void onLowMemory ()

This is called when the overall system is running low on memory, and actively running processes should trim their memory usage. While the exact point at which this will be called is not defined, generally it will happen when all background process have been killed. That is, before reaching the point of killing processes hosting service and foreground UI that we would like to avoid killing.

You should implement this method to release any caches or other unnecessary resources you may be holding on to. The system will perform a garbage collection for you after returning from this method.

Preferably, you should implement onTrimMemory(int) from ComponentCallbacks2 to incrementally unload your resources based on various levels of memory demands. That API is available for API level 14 and higher, so you should only use this onLowMemory() method as a fallback for older versions, which can be treated the same as onTrimMemory(int) with the TRIM_MEMORY_COMPLETE level.


推荐用trim来解决不同等级的内存缩放

是否给一个自我瘦身的机会，杀鸡儆猴，如果你是那只鸡，那就没办法了！onLowMemory是在杀死所有后台进程的时候，给前台进程回调用的，该杀的都杀了，如果你再不释放资源，并且内存还是不够的话，就别怪连前台进程也杀掉。
	        
###  参考文档

[谷歌文档Application ](https://developer.android.com/reference/android/app/Application.html#onLowMemory%28%29)                 
[Android四大组件与进程启动的关系](http://gityuan.com/2016/10/09/app-process-create-2/)     