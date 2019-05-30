不准后台应用startService

* 后台启动Service的场景
* 后台启动Service的问题及原因
* 如何修改达到兼容



对于普通APP而言，启动服务分下面两种，每一种有分别有几种情况

*  通过其他应用startService
* 通过自己应用startService


# Application杀死恢复

第一次启动 UidRecord: UidRecord中默认 idle = true

![image.png](https://upload-images.jianshu.io/upload_images/1460468-aac4771c031e350f.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

    public UidRecord(int _uid) {
        uid = _uid;
        idle = true;
        reset();
    }
    

![image.png](https://upload-images.jianshu.io/upload_images/1460468-fe2213350e0b0324.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)



启动的Application如果有Activity会先启动Activity，如果是恢复，会先恢复service，然后才会走Application的onCreate

在ActivityThread attachApplication的时候realStartActivityLocked 会更改process的优先级，并更改UidRecord的record
 
realStartActivityLocked

	 @VisibleForTesting
	    void dispatchUidsChanged() {
	        int N;
	        synchronized (this) {
	            N = mPendingUidChanges.size();
	            if (mActiveUidChanges.length < N) {
	                mActiveUidChanges = new UidRecord.ChangeItem[N];
	            }
	            for (int i=0; i<N; i++) {
	                final UidRecord.ChangeItem change = mPendingUidChanges.get(i);
	                mActiveUidChanges[i] = change;
	                if (change.uidRecord != null) {
	                    change.uidRecord.pendingChange = null;
	                    change.uidRecord = null;
	                }
	            }
	            mPendingUidChanges.clear();
	            if (DEBUG_UID_OBSERVERS) Slog.i(TAG_UID_OBSERVERS,
	                    "*** Delivering " + N + " uid changes");
	        }
	
	        mUidChangeDispatchCount += N;
	        int i = mUidObservers.beginBroadcast();
	        while (i > 0) {
	            i--;
	            dispatchUidsChangedForObserver(mUidObservers.getBroadcastItem(i),
	                    (UidObserverRegistration) mUidObservers.getBroadcastCookie(i), N);
	        }
	        mUidObservers.finishBroadcast();
	
	        if (VALIDATE_UID_STATES && mUidObservers.getRegisteredCallbackCount() > 0) {
	            for (int j = 0; j < N; ++j) {
	                final UidRecord.ChangeItem item = mActiveUidChanges[j];
	                if ((item.change & UidRecord.CHANGE_GONE) != 0) {
	                    mValidateUids.remove(item.uid);
	                } else {
	                    UidRecord validateUid = mValidateUids.get(item.uid);
	                    if (validateUid == null) {
	                        validateUid = new UidRecord(item.uid);
	                        mValidateUids.put(item.uid, validateUid);
	                    }
	                    if ((item.change & UidRecord.CHANGE_IDLE) != 0) {
	                        validateUid.idle = true;
	                    } else if ((item.change & UidRecord.CHANGE_ACTIVE) != 0) {
	                        validateUid.idle = false;
	                    }
	                    validateUid.curProcState = validateUid.setProcState = item.processState;
	                    validateUid.lastDispatchedProcStateSeq = item.procStateSeq;
	                }
	            }
	        }
	
	        synchronized (this) {
	            for (int j = 0; j < N; j++) {
	                mAvailUidChanges.add(mActiveUidChanges[j]);
	            }
	        }
	    }
	



clean的时候会清理UidRecord

![image.png](https://upload-images.jianshu.io/upload_images/1460468-6847eaadff8c26e7.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


现在的APP必须有启动Activity，否则无法安装，正常启动是启动Activity，那么会更新idle，如果不启动则idle就是true，必然失败。 伴随Activity启动的进程都会被设置成优先级高度active进程，被杀死后启动的进程是被Service唤醒的进程，仍然idle未激活




    @GuardedBy("this")
    private final boolean attachApplicationLocked(IApplicationThread thread,
            int pid, int callingUid, long startSeq) {

    ...
                thread.bindApplication(processName, appInfo, providers,
                        app.instr.mClass,
                        profilerInfo, app.instr.mArguments,
                        app.instr.mWatcher,
                        app.instr.mUiAutomationConnection, testMode,
                        mBinderTransactionTrackingEnabled, enableTrackAllocation,
                        isRestrictedBackupMode || !normalMode, app.persistent,
                        new Configuration(getGlobalConfiguration()), app.compat,
                        getCommonServicesLocked(app.isolated),
                        mCoreSettingsObserver.getCoreSettingsLocked(),
                        buildSerial, isAutofillCompatEnabled);
            ...
        boolean badApp = false;
        boolean didSomething = false;

        // See if the top visible activity is waiting to run in this process...
        if (normalMode) {
            try {
            	// 需要启动的Activity
                if (mStackSupervisor.attachApplicationLocked(app)) {
                    didSomething = true;
                }
            } catch (Exception e) {
                Slog.wtf(TAG, "Exception thrown launching activities in " + app, e);
                badApp = true;
            }
        }

        // Find any services that should be running in this process...
        if (!badApp) {
            try {
            	// 需要恢复的Service
                didSomething |= mServices.attachApplicationLocked(app, processName);
                checkTime(startTime, "attachApplicationLocked: after mServices.attachApplicationLocked");
            } catch (Exception e) {
                Slog.wtf(TAG, "Exception thrown starting services in " + app, e);
                badApp = true;
            }
        }


	 boolean attachApplicationLocked(ProcessRecord app) throws RemoteException {
	        final String processName = app.processName;
	        boolean didSomething = false;
	        for (int displayNdx = mActivityDisplays.size() - 1; displayNdx >= 0; --displayNdx) {
	            final ActivityDisplay display = mActivityDisplays.valueAt(displayNdx);
	            for (int stackNdx = display.getChildCount() - 1; stackNdx >= 0; --stackNdx) {
	                final ActivityStack stack = display.getChildAt(stackNdx);
	                if (!isFocusedStack(stack)) {
	                    continue;
	                }
	                stack.getAllRunningVisibleActivitiesLocked(mTmpActivityList);
	                final ActivityRecord top = stack.topRunningActivityLocked();
	                final int size = mTmpActivityList.size();
	                for (int i = 0; i < size; i++) {
	                    final ActivityRecord activity = mTmpActivityList.get(i);
	                    if (activity.app == null && app.uid == activity.info.applicationInfo.uid
	                            && processName.equals(activity.processName)) {
	                        try {
	                            if (realStartActivityLocked(activity, app,
	                                    top == activity /* andResume */, true /* checkConfig */))  
	                               ...
	        return didSomething;
	    }
  
 realStartActivityLocked会更新oom，更新oom的时候会设置idle为false，因为有要启动的Activity就不在是后台进程，而对于杀死并通过Service恢复的进程，没有明确的Activity，所以不会立刻归为前台进程。
    
![image.png](https://upload-images.jianshu.io/upload_images/1460468-88d3dbe1a8fd4c94.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 
 启动新的Activity老的将要走stop逻辑，先加到要走stop的列表中：
    
![image.png](https://upload-images.jianshu.io/upload_images/1460468-5d14d164161c6a0f.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


![image.png](https://upload-images.jianshu.io/upload_images/1460468-03f878f808b954d8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

scheduleIdleLocked会被调用：
  
    final void scheduleIdleLocked() {
        mHandler.sendEmptyMessage(IDLE_NOW_MSG);
    }

接着会调用scheduleResumeTopActivities启动Activity

    final void scheduleResumeTopActivities() {
        if (!mHandler.hasMessages(RESUME_TOP_ACTIVITY_MSG)) {
            mHandler.sendEmptyMessage(RESUME_TOP_ACTIVITY_MSG);
        }
    }   
    
# 场景：探究下什么是后台启动Service

	public class LabApplication extends Application {
	    @Override
	    public void onCreate() {
	        super.onCreate();
	          Intent intent = new Intent( this, BackGroundService.class);
	        startService(intent);
	    }
	 }
  
	  public class BackGroundService extends Service {
	    @Nullable
	    @Override
	    public IBinder onBind(Intent intent) {
	        return null;
	    }
	
	    @Override
	    public int onStartCommand(Intent intent, int flags, int startId) {
	        LogUtils.v("onStartCommand");
	        return START_STICKY;
	    }
	}
	
第一次通过Launcher冷启动没问题，如果我们杀死APP后，应用再回复的时候就会出现如下Crash（禁止后台启动Service的Crash Log）：
  
![image.png](https://upload-images.jianshu.io/upload_images/1460468-c5c9ad3821e02d49.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

    java.lang.RuntimeException: Unable to create application com.snail.labaffinity.app.LabApplication: java.lang.IllegalStateException: Not allowed to start service Intent { cmp=com.snail.labaffinity/.service.BackGroundService }: app is in background uid UidRecord{72bb30d u0a238 SVC  idle change:idle|uncached procs:1 seq(0,0,0)}
        at android.app.ActivityThread.handleBindApplication(ActivityThread.java:5925)
        at android.app.ActivityThread.access$1100(ActivityThread.java:200)
        at android.app.ActivityThread$H.handleMessage(ActivityThread.java:1656)
        at android.os.Handler.dispatchMessage(Handler.java:106)
        at android.os.Looper.loop(Looper.java:193)
        at android.app.ActivityThread.main(ActivityThread.java:6718)
        at java.lang.reflect.Method.invoke(Native Method)
        at com.android.internal.os.RuntimeInit$MethodAndArgsCaller.run(RuntimeInit.java:493)
        at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:858)

这个是为什么呢？为什么冷启动没问题，后台杀死自启动恢复就有问题，看日志是因为当app is in background，Not allowed to start service，也就是后台进程不能通过startService启动服务，在LabApplication的onCreate中我们确实主动	        startService(intent)，这个就是crash的原因，具体为啥呢？什么样的进程才算后台进程呢？
          
>  ContextImpl.java 
  
    private ComponentName startServiceCommon(Intent service, boolean requireForeground,
            UserHandle user) {
        try {
            validateServiceIntent(service);
            service.prepareToLeaveProcess(this);
            ComponentName cn = ActivityManager.getService().startService(
                mMainThread.getApplicationThread(), service, service.resolveTypeIfNeeded(
                            getContentResolver()), requireForeground,
                            getOpPackageName(), user.getIdentifier());
            if (cn != null) {
            <!--返回值是？的情况下就是后台启动service的异常-->
                 if (cn.getPackageName().equals("?")) {
                    throw new IllegalStateException(
                            "Not allowed to start service " + service + ": " + cn.getClassName());
                }
    }

什么时候ActivityManager.getService().startService的返回值是**？**，ActivityManagerService最终会调用ActiveServices.java的startService，
    
> ActiveServices.java
 

    ComponentName startServiceLocked(IApplicationThread caller, Intent service, String resolvedType,
            int callingPid, int callingUid, boolean fgRequired, String callingPackage, final int userId)
            throws TransactionTooLargeException {
             final boolean callerFg;
             
        if (caller != null) {
            final ProcessRecord callerApp = mAm.getRecordForAppLocked(caller);
			  ...
            callerFg = callerApp.setSchedGroup != ProcessList.SCHED_GROUP_BACKGROUND;
        } else {
            callerFg = true;
        }

        ServiceLookupResult res =
            retrieveServiceLocked(service, resolvedType, callingPackage,
                    callingPid, callingUid, userId, true, callerFg, false, false);
        ...
        ServiceRecord r = res.record;
        
        // If we're starting indirectly (e.g. from PendingIntent), figure out whether
        // we're launching into an app in a background state.  This keys off of the same
        // idleness state tracking as e.g. O+ background service start policy.
      
        <!--通过PendingIntent启动的也要检查-->
        // 是否当前Uid Active 不过不是activity就是后台启动
        final boolean bgLaunch = !mAm.isUidActiveLocked(r.appInfo.uid);
       // If the app has strict background restrictions, we treat any bg service
        // start analogously to the legacy-app forced-restrictions case, regardless
        // of its target SDK version.
        boolean forcedStandby = false;
        <!--appRestrictedAnyInBackground 一般人不会主动设置，所以这个经常是返回false-->
        if (bgLaunch && appRestrictedAnyInBackground(r.appInfo.uid, r.packageName)) {
	        ...
           forcedStandby = true;
        }

        <!--forcedStandby可以先无视 这里注意两点，第一点 ：r.startRequested标志是否被startService调用启动过，第一次进来的时候是false，第二：fgRequired普通是starService是false-->  
        
        if (forcedStandby || (!r.startRequested && !fgRequired)) {

            <!--检测当前app是否允许后台启动-->
            final int allowed = mAm.getAppStartModeLocked(r.appInfo.uid, r.packageName,
                    r.appInfo.targetSdkVersion, callingPid, false, false, forcedStandby);
                    <!--如果不允许  Background start not allowed-->
            if (allowed != ActivityManager.APP_START_MODE_NORMAL) {
                ...
                <!--返回 ? 告诉客户端现在处于后台启动状态，禁止你-->
                return new ComponentName("?", "app is in background uid " + uidRec);
            }
        }
可以看到，整个关键就是mAm.getAppStartModeLocked，看看当前进程是否处于后台非激活状态。

> ActivityManagerService.java

	  int getAppStartModeLocked(int uid, String packageName, int packageTargetSdk,
	            int callingPid, boolean alwaysRestrict, boolean disabledOnly, boolean forcedStandby) {
	        UidRecord uidRec = mActiveUids.get(uid);
 
 			 <!--通过其他APP启动的话UidRecord是null，自启动非null，无轮是恢复启动还是主动启动 uidRec.idle -->
 			 
	        if (uidRec == null || alwaysRestrict || forcedStandby || uidRec.idle) {
	            boolean ephemeral;
	            ...
	            <!--ephemeral  InstantApp相关，不考虑-->
		        if (ephemeral) {
	                return ActivityManager.APP_START_MODE_DISABLED;
	            } else {
	                final int startMode = (alwaysRestrict)
	                        ? appRestrictedInBackgroundLocked(uid, packageName, packageTargetSdk)
	                        : appServicesRestrictedInBackgroundLocked(uid, packageName,
	                                packageTargetSdk);
                   ...
	                return startMode;
	            }
	        }
	        return ActivityManager.APP_START_MODE_NORMAL;
	    }
	

 appServicesRestrictedInBackgroundLocked
	
	
	    int appServicesRestrictedInBackgroundLocked(int uid, String packageName, int packageTargetSdk) {
	    <!--永久进程 -->
        // Persistent app?
        if (mPackageManagerInt.isPackagePersistent(packageName)) {
            return ActivityManager.APP_START_MODE_NORMAL;
        }

        <!--白名单-->
        // Non-persistent but background whitelisted?
        if (uidOnBackgroundWhitelist(uid)) {
            return ActivityManager.APP_START_MODE_NORMAL;
        }
        <!--白名单-->
        // Is this app on the battery whitelist?
        if (isOnDeviceIdleWhitelistLocked(uid, /*allowExceptIdleToo=*/ false)) {
            return ActivityManager.APP_START_MODE_NORMAL;
        }

        // 普通进程
        return appRestrictedInBackgroundLocked(uid, packageName, packageTargetSdk);
    }
 
 对于普通进程看看O限制 
    
	    int appRestrictedInBackgroundLocked(int uid, String packageName, int packageTargetSdk) {
	        <!--对于targetSDKVersion>O 的直接 返回ActivityManager.APP_START_MODE_DELAYED_RIGID-->
	        if (packageTargetSdk >= Build.VERSION_CODES.O) {
	            return ActivityManager.APP_START_MODE_DELAYED_RIGID;
	        }
	        // 否则仅仅对老版本做兼容性限制
	        int appop = mAppOpsService.noteOperation(AppOpsManager.OP_RUN_IN_BACKGROUND,
	                uid, packageName);
	        if (DEBUG_BACKGROUND_CHECK) {
	            Slog.i(TAG, "Legacy app " + uid + "/" + packageName + " bg appop " + appop);
	        }
	        switch (appop) {
	            case AppOpsManager.MODE_ALLOWED:
	                // If force-background-check is enabled, restrict all apps that aren't whitelisted.
	                if (mForceBackgroundCheck &&
	                        !UserHandle.isCore(uid) &&
	                        !isOnDeviceIdleWhitelistLocked(uid, /*allowExceptIdleToo=*/ true)) {
	                    return ActivityManager.APP_START_MODE_DELAYED;
	                }
	           ...
	    }
	    

同一个应用不会出现startService问题？？

## 没有进程的情况下

>  am startservice -n com.snail.labaffinity/com.snail.labaffinity.service.BackGroundService 

	app is in background uid null

## 有进程的情况下

Application中start的话此时UidRecord新建，默认UidRecord中的idle是false


    /**
     * Track all uids that have actively running processes.
     */
    final SparseArray<UidRecord> mActiveUids = new SparseArray<>();

idle之后，线程停止 ，handler停止

要看UidRecord的idle为何为true

	app is in background uid UidRecord{7634f85 u0a73 SVC  idle change:idle|uncached procs:1 seq(0,0,0)}

![image.png](https://upload-images.jianshu.io/upload_images/1460468-bdee9bfa6da18fd1.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以延迟复现。延迟1分钟，退到后台，之后就可复现：

    @OnClick(R.id.first)
    void first() {
        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(LabApplication.getContext(), BackGroundService.class);
                startService(intent);
                LogUtils.v("延迟执行");
            }
        },1000*65);

    }
    
		
		    --------- beginning of crash
	2019-06-17 19:47:43.148 25916-25916/com.snail.labaffinity E/AndroidRuntime: FATAL EXCEPTION: main
	    Process: com.snail.labaffinity, PID: 25916
	    java.lang.IllegalStateException: Not allowed to start service Intent { cmp=com.snail.labaffinity/.service.BackGroundService }: app is in background uid UidRecord{9048c2c u0a73 LAST bg:+1m4s376ms idle change:idle procs:1 seq(0,0,0)}
	        at android.app.ContextImpl.startServiceCommon(ContextImpl.java:1577)
	        at android.app.ContextImpl.startService(ContextImpl.java:1532)
	        at android.content.ContextWrapper.startService(ContextWrapper.java:664)
	        at com.snail.labaffinity.activity.MainActivity$2.run(MainActivity.java:41)
	        at android.os.Handler.handleCallback(Handler.java:873)
	        at android.os.Handler.dispatchMessage(Handler.java:99)
	        at android.os.Looper.loop(Looper.java:193)
	        at android.app.ActivityThread.main(ActivityThread.java:6669)
	        at java.lang.reflect.Method.invoke(Native Method)
	        at com.android.internal.os.RuntimeInit$MethodAndArgsCaller.run(RuntimeInit.java:493)
	        at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:858)
	        
        
active与idle的变换规则
如果应用变为前台，即procState小于PROCESS_STATE_TRANSIENT_BACKGROUND(8)时，UID状态马上变更为active状态
如果应用变为后台，即procState大于等于PROCESS_STATE_TRANSIENT_BACKGROUND(8)时，应用持续在后台60s后，UID状态会变更为idle状态
调试方法：

idle为false，激活 resumetopActivity的时候，会直接修改当前的，

![image.png](https://upload-images.jianshu.io/upload_images/1460468-88d3dbe1a8fd4c94.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
    
转变为idle，trimeApplication

 ![image.png](https://upload-images.jianshu.io/upload_images/1460468-845e10c8558c8569.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 
     @GuardedBy("this")
    final void updateOomAdjLocked() {
    
      ... 60s后，扫描一遍，设置后台进程
    
                if (!ActivityManager.isProcStateBackground(uidRec.setProcState)
                    || uidRec.setWhitelist) {
                uidRec.lastBackgroundTime = nowElapsed;
                if (!mHandler.hasMessages(IDLE_UIDS_MSG)) {
                    // Note: the background settle time is in elapsed realtime, while
                    // the handler time base is uptime.  All this means is that we may
                    // stop background uids later than we had intended, but that only
                    // happens because the device was sleeping so we are okay anyway.
                    mHandler.sendEmptyMessageDelayed(IDLE_UIDS_MSG,
                            mConstants.BACKGROUND_SETTLE_TIME);
                }
            }
    
App退到后台，或者不可见的时候


![image.png](https://upload-images.jianshu.io/upload_images/1460468-25d3b59ce9d85e4c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


会调用

    final void scheduleIdleLocked() {
        mHandler.sendEmptyMessage(IDLE_NOW_MSG);
    }
    
 进而trime，重新计算进程优先级，同时60后设置后台进程，限制活动性。   
    
 最终通过idleUids更新  uidRec.idle，如果设置true，就是后台进程，标准就是后台存活时间是否大于
 
     private static final long DEFAULT_BACKGROUND_SETTLE_TIME = 60*1000;
     
 也就是60s，超过60s的就是后台进程
 
 
     final void idleUids() {
        synchronized (this) {
            final int N = mActiveUids.size();
            if (N <= 0) {
                return;
            }
            final long nowElapsed = SystemClock.elapsedRealtime();
            final long maxBgTime = nowElapsed - mConstants.BACKGROUND_SETTLE_TIME;
            long nextTime = 0;
            if (mLocalPowerManager != null) {
                mLocalPowerManager.startUidChanges();
            }
            for (int i=N-1; i>=0; i--) {
                final UidRecord uidRec = mActiveUids.valueAt(i);
                final long bgTime = uidRec.lastBackgroundTime;
                if (bgTime > 0 && !uidRec.idle) {
                <!--设置标准：后台存活时间-->
                    if (bgTime <= maxBgTime) {
                        EventLogTags.writeAmUidIdle(uidRec.uid);
                        uidRec.idle = true;
                        uidRec.setIdle = true;
                        doStopUidLocked(uidRec.uid, uidRec);
                    } else {
                        if (nextTime == 0 || nextTime > bgTime) {
                            nextTime = bgTime;
                        }
                    }
                }
            }
            if (mLocalPowerManager != null) {
                mLocalPowerManager.finishUidChanges();
            }
            if (nextTime > 0) {
                mHandler.removeMessages(IDLE_UIDS_MSG);
                mHandler.sendEmptyMessageDelayed(IDLE_UIDS_MSG,
                        nextTime + mConstants.BACKGROUND_SETTLE_TIME - nowElapsed);
            }
        }
    }   
  
 时间更新:之前是前台，现在变后台，那么uidRec.lastBackgroundTime = nowElapsed赋值，如果再次切前台，要清零
 
	 final void updateOomAdjLocked() {
	  ...
	         for (int i=mActiveUids.size()-1; i>=0; i--) {
	            final UidRecord uidRec = mActiveUids.valueAt(i);
	            int uidChange = UidRecord.CHANGE_PROCSTATE;
	            if (uidRec.curProcState != ActivityManager.PROCESS_STATE_NONEXISTENT
	                    && (uidRec.setProcState != uidRec.curProcState
	                           || uidRec.setWhitelist != uidRec.curWhitelist)) {
	                if (DEBUG_UID_OBSERVERS) Slog.i(TAG_UID_OBSERVERS,
	                        "Changes in " + uidRec + ": proc state from " + uidRec.setProcState
	                        + " to " + uidRec.curProcState + ", whitelist from " + uidRec.setWhitelist
	                        + " to " + uidRec.curWhitelist);
	                if (ActivityManager.isProcStateBackground(uidRec.curProcState)
	                        && !uidRec.curWhitelist) {
	                    // UID is now in the background (and not on the temp whitelist).  Was it
	                    // previously in the foreground (or on the temp whitelist)?
	                    if (!ActivityManager.isProcStateBackground(uidRec.setProcState)
	                            || uidRec.setWhitelist) {
	                        uidRec.lastBackgroundTime = nowElapsed;
	                        if (!mHandler.hasMessages(IDLE_UIDS_MSG)) {
	                            // Note: the background settle time is in elapsed realtime, while
	                            // the handler time base is uptime.  All this means is that we may
	                            // stop background uids later than we had intended, but that only
	                            // happens because the device was sleeping so we are okay anyway.
	                            mHandler.sendEmptyMessageDelayed(IDLE_UIDS_MSG,
	                                    mConstants.BACKGROUND_SETTLE_TIME);
	                        }
	                    }


> active与idle的变换规则

* 如果应用变为前台，即procState小于PROCESS_STATE_TRANSIENT_BACKGROUND(8)时，UID状态马上变更为active状态
* 如果应用变为后台，即procState大于等于PROCESS_STATE_TRANSIENT_BACKGROUND(8)时，应用持续在后台60s后，UID状态会变更为idle状态

> startForegroundService的ANR与FC：
 
* 调用startForegroundService后，如果5s内没有在Service中调用startForeground，那么就会发生ANR； “Context.startForegroundService() did not then call Service.startForeground()”
* 调用startForegroundService后，直到将Service停止之前都没有在Service中调用startForeground，那么就会发生FC
	 
# 如何解决这个问题        
 
 
如果已经是前台，不需要关心timeout，如果不是前台，需要关心timeout
 
             if (r.fgRequired && !r.fgWaiting) {
                if (!r.isForeground) {
                    if (DEBUG_BACKGROUND_CHECK) {
                        Slog.i(TAG, "Launched service must call startForeground() within timeout: " + r);
                    }
                    scheduleServiceForegroundTransitionTimeoutLocked(r);
                } else {
                    if (DEBUG_BACKGROUND_CHECK) {
                        Slog.i(TAG, "Service already foreground; no new timeout: " + r);
                    }
                    r.fgRequired = false;
                }
            }


    void scheduleServiceForegroundTransitionTimeoutLocked(ServiceRecord r) {
        if (r.app.executingServices.size() == 0 || r.app.thread == null) {
            return;
        }
        Message msg = mAm.mHandler.obtainMessage(
                ActivityManagerService.SERVICE_FOREGROUND_TIMEOUT_MSG);
        msg.obj = r;
        r.fgWaiting = true;
        mAm.mHandler.sendMessageDelayed(msg, SERVICE_START_FOREGROUND_TIMEOUT);
    }

    final class MainHandler extends Handler {
        public MainHandler(Looper looper) {
            super(looper, null, true);
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
            ...
            case SERVICE_FOREGROUND_TIMEOUT_MSG: {
                mServices.serviceForegroundTimeout((ServiceRecord)msg.obj);
            } break;


	   void serviceForegroundTimeout(ServiceRecord r) {
	        ProcessRecord app;
	        synchronized (mAm) {
	            if (!r.fgRequired || r.destroying) {
	                return;
	            }
	
	            app = r.app;
	            if (app != null && app.debugging) {
	                // The app's being debugged; let it ride
	                return;
	            }
	
	            if (DEBUG_BACKGROUND_CHECK) {
	                Slog.i(TAG, "Service foreground-required timeout for " + r);
	            }
	            r.fgWaiting = false;
	            stopServiceLocked(r);
	        }
	
	        if (app != null) {
	            mAm.mAppErrors.appNotResponding(app, null, null, false,
	                    "Context.startForegroundService() did not then call Service.startForeground(): "
	                        + r);
	        }
	    }
	
 另外，如果再调用startForGround前调用了stop 会Crash
	
	
	
    private final void bringDownServiceLocked(ServiceRecord r) {
        //Slog.i(TAG, "Bring down service:");
        //r.dump("  ");
 

        // Check to see if the service had been started as foreground, but being
        // brought down before actually showing a notification.  That is not allowed.
        
        if (r.fgRequired) {
            r.fgRequired = false;
            r.fgWaiting = false;
            mAm.mAppOpsService.finishOperation(AppOpsManager.getToken(mAm.mAppOpsService),
                    AppOpsManager.OP_START_FOREGROUND, r.appInfo.uid, r.packageName);
            mAm.mHandler.removeMessages(
                    ActivityManagerService.SERVICE_FOREGROUND_TIMEOUT_MSG, r);
            if (r.app != null) {
                Message msg = mAm.mHandler.obtainMessage(
                        ActivityManagerService.SERVICE_FOREGROUND_CRASH_MSG);
                msg.obj = r.app;
                msg.getData().putCharSequence(
                    ActivityManagerService.SERVICE_RECORD_KEY, r.toString());
                mAm.mHandler.sendMessage(msg);
            }
        }
 

    final class MainHandler extends Handler {
        public MainHandler(Looper looper) {
            super(looper, null, true);
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
            case UPDATE_CONFIGURATION_MSG: {
                final ContentResolver resolver = mContext.getContentResolver();
                Settings.System.putConfigurationForUser(resolver, (Configuration) msg.obj,
                        msg.arg1);
            } break;
            case GC_BACKGROUND_PROCESSES_MSG: {
                synchronized (ActivityManagerService.this) {
                    performAppGcsIfAppropriateLocked();
                }
            } break;
            case SERVICE_TIMEOUT_MSG: {
                mServices.serviceTimeout((ProcessRecord)msg.obj);
            } break;
            case SERVICE_FOREGROUND_TIMEOUT_MSG: {
                mServices.serviceForegroundTimeout((ServiceRecord)msg.obj);
            } break;
            case SERVICE_FOREGROUND_CRASH_MSG: {
                mServices.serviceForegroundCrash(
                    (ProcessRecord) msg.obj, msg.getData().getCharSequence(SERVICE_RECORD_KEY));
            }
            
调用startForeground后
    
    
    public final void startForeground(int id, Notification notification) {
        try {
            mActivityManager.setServiceForeground(
                    new ComponentName(this, mClassName), mToken, id,
                    notification, 0);
        } catch (RemoteException ex) {
        }
}
 
 会调用mActivityManager.setServiceForeground 将r.fgRequired = false
 
	 private void setServiceForegroundInnerLocked(ServiceRecord r, int id,
	            Notification notification, int flags) {
	        if (r.fgRequired) {
	                r.fgRequired = false;
	                r.fgWaiting = false;
	                mAm.mHandler.removeMessages(
	                        ActivityManagerService.SERVICE_FOREGROUND_TIMEOUT_MSG, r);
	        }
	}


正确做法，onCreate中startForeground     

也就是 当Service被启动后，客户端需要调用Service.startForeground才能同时解除ANR和FC  


#    总结

*  Application里面不要startService，否则恢复的时候可能有问题     
*  不要通过Handler延迟太久再startService，否则也会有问题
*  60s原则