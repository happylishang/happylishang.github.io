


Android O之后，很多后台启动的行为都开始受限，比如O的时候，不能后台启动Service，而在Android10之后，连Activity也加到了后台限制中。在[Android O 后台startService限制简析](https://www.jianshu.com/p/f2db0f58d47f)中，层分析Android O之后，后台限制启动Service的场景，一般而言，APP退到后台（比如按Home键），1分钟之后变为后台APP，虽然进程存活，但是已经不能通过startService启动服务，但是发送通知并不受限制，可以通过通知启动Service，这个时候，Service不会被当做后台启动，同样通过通知栏打开Activity也不受限制？ 为什么，直观来讲，通知已经属于用户感知的交互，本就不应该算到后台启动。本文先发对比之前的[Android O 后台startService限制简析](https://www.jianshu.com/p/f2db0f58d47f)，分析下Service，之后再看Activity在Android10中的限制

## 通知借助PendingIntent启动Service

可以模拟这样一个场景，发送一个通知，然后将APP杀死，之后在通知栏通过PendingIntent启动Service，看看是否会出现禁止后台启动Service的场景。

    void notify() {
        NotificationCompat.Builder builder = new NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID);
        builder.setContentIntent(PendingIntent.getService(this, (int) System.currentTimeMillis(),
                new Intent(this,
                        BackGroundService.class),
                PendingIntent.FLAG_UPDATE_CURRENT))
                .setContentText("content")...)  

        NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(NOTIFICATION_CHANNEL_ID,
                    "Channel human readable title",
                    NotificationManager.IMPORTANCE_DEFAULT);
            if (nm != null) {
                nm.createNotificationChannel(channel);
            }
        }
        nm.notify(1, builder.build());
    }
  
实际结果是：点击通知后Service正常启动。下面逐步分析下。

同普通的Intent启动Service不同，这里的通知通过PendingIntent启动，是不是只要PendingIntent就足够了呢，并不是（后面分析）。通过通知启动Service的第一步是通过PendingIntent.getService获得一个用于启动特定Service的PendingIntent：

	    public static PendingIntent getService(Context context, int requestCode,
	            @NonNull Intent intent, @Flags int flags) {
	        return buildServicePendingIntent(context, requestCode, intent, flags,
	                ActivityManager.INTENT_SENDER_SERVICE);
         }
    
        private static PendingIntent buildServicePendingIntent(Context context, int requestCode,
            Intent intent, int flags, int serviceKind) {
        String packageName = context.getPackageName();
        String resolvedType = intent != null ? intent.resolveTypeIfNeeded(
                context.getContentResolver()) : null;
        try {
            intent.prepareToLeaveProcess(context);
            IIntentSender target =
                ActivityManager.getService().getIntentSender(
                    serviceKind, packageName,
                    null, null, requestCode, new Intent[] { intent },
                    resolvedType != null ? new String[] { resolvedType } : null,
                    flags, null, context.getUserId());
            return target != null ? new PendingIntent(target) : null;
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

IIntentSender在APP端其实是一个Binder代理，这里是典型的Binder双向通信模型，AMS端会为APP构建一个PendingIntentRecord extends IIntentSender.Stub实体， PendingIntentRecord可以看做PendingIntent在AMS端的记录，最终形成两者对应的双向通信通道。之后通知就会通过nm.notify显示在通知栏，这一步先略过，先看最后一步，通过点击通知启动Service，通知点击这不细看，只要明白最后调用的是PendingIntent的sendAndReturnResult函数，

    public int sendAndReturnResult(Context context, int code, @Nullable Intent intent,
            @Nullable OnFinished onFinished, @Nullable Handler handler,
            @Nullable String requiredPermission, @Nullable Bundle options)
            throws CanceledException {
        try {
            String resolvedType = intent != null ?
                    intent.resolveTypeIfNeeded(context.getContentResolver())
                    : null;
            return ActivityManager.getService().sendIntentSender(
                    mTarget, mWhitelistToken, code, intent, resolvedType,
                    onFinished != null
                            ? new FinishedDispatcher(this, onFinished, handler)
                            : null,
                    requiredPermission, options);
        } catch (RemoteException e) {
            throw new CanceledException(e);
        }
    }

通过Binder最终到AMS端，查找到对应的PendingIntentRecord，进入其sendInner函数，前文buildIntent的时候，用的是 ActivityManager.INTENT_SENDER_SERVICE，进入对应分支：

    public int sendInner(int code, Intent intent, String resolvedType, IBinder whitelistToken,
            IIntentReceiver finishedReceiver, String requiredPermission, IBinder resultTo,
            String resultWho, int requestCode, int flagsMask, int flagsValues, Bundle options) {


			    if (whitelistDuration != null) {
                  duration = whitelistDuration.get(whitelistToken);
                }
				 <!--是否可以启动的一个关键点 ，后面分析-->
		        int res = START_SUCCESS;
		        try {
		        <!--duration非null才会执行tempWhitelistForPendingIntent添加到白名单-->
		            if (duration != null) {
		                int procState = controller.mAmInternal.getUidProcessState(callingUid);
		                
		                <!--u0_a16   2102  1742 4104448 174924 0    0 S com.android.systemui 通知是systemui进程 优先级高没后台问题-->
		                if (!ActivityManager.isProcStateBackground(procState)) {
		                    ...
		                    <!--更新临时白名单, duration设定白名单的有效时长，这个是在发通知的时候设定的-->
		                    controller.mAmInternal.tempWhitelistForPendingIntent(callingPid, callingUid,
		                            uid, duration, tag.toString());
		                } else {
 		                }
		            }
     
     		        ...
                case ActivityManager.INTENT_SENDER_SERVICE:
                case ActivityManager.INTENT_SENDER_FOREGROUND_SERVICE:
                    try {
                        controller.mAmInternal.startServiceInPackage(uid, finalIntent, resolvedType,
                                key.type == ActivityManager.INTENT_SENDER_FOREGROUND_SERVICE,
                                key.packageName, userId,
                                mAllowBgActivityStartsForServiceSender.contains(whitelistToken)
                                || allowTrampoline);
                    } catch (RuntimeException e) {				...
                    
 ![](https://user-gold-cdn.xitu.io/2019/9/2/16cf00d623a1f176?w=1866&h=474&f=png&s=131547)

其实最后进入controller.mAmInternal.startServiceInPackage，最后流到AMS的startServiceInPackage，接下来的流程在[Android O 后台startService限制简析](https://www.jianshu.com/p/f2db0f58d47f)分析过，包括后台限制的检测，不过这里有一点是前文没分析的，


	 int appServicesRestrictedInBackgroundLocked(int uid, String packageName, int packageTargetSdk) {
	       ...        
	       
	       // Is this app on the battery whitelist?
	        if (isOnDeviceIdleWhitelistLocked(uid, /*allowExceptIdleToo=*/ false)) {
	            return ActivityManager.APP_START_MODE_NORMAL;
	        }
	
	        // None of the service-policy criteria apply, so we apply the common criteria
	        return appRestrictedInBackgroundLocked(uid, packageName, packageTargetSdk);
	    }

     */
    boolean isOnDeviceIdleWhitelistLocked(int uid, boolean allowExceptIdleToo) {
        final int appId = UserHandle.getAppId(uid);

        final int[] whitelist = allowExceptIdleToo
                ? mDeviceIdleExceptIdleWhitelist
                : mDeviceIdleWhitelist;

        return Arrays.binarySearch(whitelist, appId) >= 0
                || Arrays.binarySearch(mDeviceIdleTempWhitelist, appId) >= 0
                || mPendingTempWhitelist.indexOfKey(uid) >= 0;
    }

**那就是mPendingTempWhitelist白名单 **，这个是通知启动Service不受限制的关键。

![](https://user-gold-cdn.xitu.io/2019/8/30/16ce15403e76ff8a?w=2530&h=1098&f=png&s=451637)

前文说过，通知发送时会设定一个临时白名单的有效存活时间，只有设置了，才能进mPendingTempWhitelist，这是存活时间是从点击到真正start中间所能存活的时间，如果在此间还未启动，则判断启动无效。**有效存活时间是什么时候设置的，是发送通知的时候，而且，这个时机只在发送通知的时候，其他没入口**：


> 		/Users/XXX/server/notification/NotificationManagerService.java:

    void enqueueNotificationInternal(final String pkg, final String opPkg, final int callingUid,
            final int callingPid, final String tag, final int id, final Notification notification,
            int incomingUserId) {
            ...
        // Whitelist pending intents.
        if (notification.allPendingIntents != null) {
            final int intentCount = notification.allPendingIntents.size();
            if (intentCount > 0) {
                final ActivityManagerInternal am = LocalServices
                        .getService(ActivityManagerInternal.class);
                final long duration = LocalServices.getService(
                        DeviceIdleController.LocalService.class).getNotificationWhitelistDuration();
                for (int i = 0; i < intentCount; i++) {
                    PendingIntent pendingIntent = notification.allPendingIntents.valueAt(i);
                    if (pendingIntent != null) {
                    <!--更新白名单机制的一环 ，只有通过这个检测才能加到mPendingTempWhitelist白名单-->
                        am.setPendingIntentWhitelistDuration(pendingIntent.getTarget(),
                                WHITELIST_TOKEN, duration);
                    }
                }
            }
        }
        
setPendingIntentWhitelistDuration会更新PendingIntentRecord的whitelistDuration列表，这个列表标识着这个
        
        public void setPendingIntentWhitelistDuration(IIntentSender target, IBinder whitelistToken,
                long duration) {

            synchronized (ActivityManagerService.this) {
                ((PendingIntentRecord) target).setWhitelistDurationLocked(whitelistToken, duration);
            }
        }
		 
    void setWhitelistDurationLocked(IBinder whitelistToken, long duration) {
        if (duration > 0) {
            if (whitelistDuration == null) {
                whitelistDuration = new ArrayMap<>();
            }
            <!--设置存活时长-->
            whitelistDuration.put(whitelistToken, duration);
        }  ...
    }

存活时长设置后，通过点击，启动Service Intent就会被放到mPendingTempWhitelist，从而避免后台检测。如果不走通知，直接用PendingIntent的send呢，效果其实跟普通Intent没太大区别，也会受后台启动限制，不过多分析。

# Android10后台启动Activity限制 (android10-release源码分支)


Android10之后，禁止后台启动Activity，Activity的后台定义比Service更严格，延时10s，退到后台，便可以模拟后台启动Activity，注意这里并没有像Service限定到60之后，Activity的后台限制更严格一些，直观上理解：没有可见窗口都可以算作后台，中间的间隔最多可能就几秒，比如我们延时10s就能看到这种效果。

    void delayStartActivity() {
        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(LabApplication.getContext(), MainActivity.class);
                startActivity(intent);
            }
        }, 1000 * 10);

    }

时间到了，在Android Q的手机上startActivity会报如下异常：

	Background activity start [callingPackage: com.snail.labaffinity; callingUid: 10102; 
			
		* 			 isCallingUidForeground: false; 
		* 			 isCallingUidPersistentSystemProcess: false; 
		* 			 realCallingUid: 10102; 
		* 			 sRealCallingUidForeground: false; 
		* 			 isRealCallingUidPersistentSystemProcess: false; 
		* 			 originatingPendingIntent: null; 
		* 			 isBgStartWhitelisted: false; 
	
	 intent: Intent { cmp=com.snail.labaffinity/.activity.MainActivity }; callerApp: ProcessRecord{f17cc20 4896:com.snail.labaffinity/u0a102}]

未正式发行的版本上还能看到如下Toast

![](https://user-gold-cdn.xitu.io/2019/9/3/16cf577e34234ea5?w=798&h=338&f=png&s=58579)

大概意思就是：限制后台应用启动Activity。

> 核心逻辑在这一段 ActivityStarter

	 boolean shouldAbortBackgroundActivityStart(int callingUid, int callingPid,
	            final String callingPackage, int realCallingUid, int realCallingPid,
	            WindowProcessController callerApp, PendingIntentRecord originatingPendingIntent,
	            boolean allowBackgroundActivityStart, Intent intent) {
	         <!--系统应用不受限制-->
	        // don't abort for the most important UIDs
	        final int callingAppId = UserHandle.getAppId(callingUid);
	        if (callingUid == Process.ROOT_UID || callingAppId == Process.SYSTEM_UID
	                || callingAppId == Process.NFC_UID) {
	            return false;
	        }
	        <!--有可见窗口及系统进程不受限制-->
	        // don't abort if the callingUid has a visible window or is a persistent system process
	        final int callingUidProcState = mService.getUidState(callingUid);
	        <!--是否有可见窗口-->
	        final boolean callingUidHasAnyVisibleWindow =
	                mService.mWindowManager.mRoot.isAnyNonToastWindowVisibleForUid(callingUid);
	        <!--CallingUid是否前台展示-->
	        final boolean isCallingUidForeground = callingUidHasAnyVisibleWindow
	                || callingUidProcState == ActivityManager.PROCESS_STATE_TOP
	                || callingUidProcState == ActivityManager.PROCESS_STATE_BOUND_TOP;
	         <!--是否PersistentSystemProcess-->
	        final boolean isCallingUidPersistentSystemProcess =
	                callingUidProcState <= ActivityManager.PROCESS_STATE_PERSISTENT_UI;
	        if (callingUidHasAnyVisibleWindow || isCallingUidPersistentSystemProcess) {
	            return false;
	        }
	        // take realCallingUid into consideration
	        final int realCallingUidProcState = (callingUid == realCallingUid)
	                ? callingUidProcState
	                : mService.getUidState(realCallingUid);
	        final boolean realCallingUidHasAnyVisibleWindow = (callingUid == realCallingUid)
	                ? callingUidHasAnyVisibleWindow
	                : mService.mWindowManager.mRoot.isAnyNonToastWindowVisibleForUid(realCallingUid);
	        final boolean isRealCallingUidForeground = (callingUid == realCallingUid)
	                ? isCallingUidForeground
	                : realCallingUidHasAnyVisibleWindow
	                        || realCallingUidProcState == ActivityManager.PROCESS_STATE_TOP;
	        final int realCallingAppId = UserHandle.getAppId(realCallingUid);
	        final boolean isRealCallingUidPersistentSystemProcess = (callingUid == realCallingUid)
	                ? isCallingUidPersistentSystemProcess
	                : (realCallingAppId == Process.SYSTEM_UID)
	                        || realCallingUidProcState <= ActivityManager.PROCESS_STATE_PERSISTENT_UI;
	        ...
	        <!--这个权限不一定是谁都能拿到-->
	        // don't abort if the callingUid has START_ACTIVITIES_FROM_BACKGROUND permission
	        if (mService.checkPermission(START_ACTIVITIES_FROM_BACKGROUND, callingPid, callingUid)
	                == PERMISSION_GRANTED) {
	            return false;
	        }
	        // don't abort if the caller has the same uid as the recents component
	        if (mSupervisor.mRecentTasks.isCallerRecents(callingUid)) {
	            return false;
	        }
	        
	        ...一些系统判断
	        
	        <!--是否白名单-->
	        // don't abort if the callerApp or other processes of that uid are whitelisted in any way
	        
	        if (callerApp != null) {
	            // first check the original calling process
	            if (callerApp.areBackgroundActivityStartsAllowed()) {
	                return false;
	            }
	            // only if that one wasn't whitelisted, check the other ones
	            final ArraySet<WindowProcessController> uidProcesses =
	                    mService.mProcessMap.getProcesses(callerAppUid);
	            if (uidProcesses != null) {
	                for (int i = uidProcesses.size() - 1; i >= 0; i--) {
	                    final WindowProcessController proc = uidProcesses.valueAt(i);
	                    if (proc != callerApp && proc.areBackgroundActivityStartsAllowed()) {
	                        return false;
	                    }
	                }
	            }
	        }
	        <!--如果callAPP有悬浮窗权限-->
	        // don't abort if the callingUid has SYSTEM_ALERT_WINDOW permission
	        if (mService.hasSystemAlertWindowPermission(callingUid, callingPid, callingPackage)) {
	            Slog.w(TAG, "Background activity start for " + callingPackage
	                    + " allowed because SYSTEM_ALERT_WINDOW permission is granted.");
	            return false;
	        }
	        <!--其余全部禁止-->
	        // anything that has fallen through would currently be aborted
	        Slog.w(TAG, "Background activity start [callingPackage: " + callingPackage
	                + "; callingUid: " + callingUid
	                + "; isCallingUidForeground: " + isCallingUidForeground
	                + "; isCallingUidPersistentSystemProcess: " + isCallingUidPersistentSystemProcess
	                + "; realCallingUid: " + realCallingUid
	                + "; isRealCallingUidForeground: " + isRealCallingUidForeground
	                + "; isRealCallingUidPersistentSystemProcess: "
	                + isRealCallingUidPersistentSystemProcess
	                + "; originatingPendingIntent: " + originatingPendingIntent
	                + "; isBgStartWhitelisted: " + allowBackgroundActivityStart
	                + "; intent: " + intent
	                + "; callerApp: " + callerApp
	                + "]");
	        // log aborted activity start to TRON
	        if (mService.isActivityStartsLoggingEnabled()) {
	            mSupervisor.getActivityMetricsLogger().logAbortedBgActivityStart(intent, callerApp,
	                    callingUid, callingPackage, callingUidProcState, callingUidHasAnyVisibleWindow,
	                    realCallingUid, realCallingUidProcState, realCallingUidHasAnyVisibleWindow,
	                    (originatingPendingIntent != null));
	        }
	        return true;
	    }


按照Google要求，在Android Q上运行的应用只有在满足以下一个或多个条件时才能启动Activity：常见的有如下几种

* 具有可见窗口，例如在前台运行的Activity。（前台服务不会将应用限定为在前台运行。）
* 该应用在前台任务的返回栈中具有一项 Activity。（必须同前台Activity位于同一个Task返回栈，如果两个Task栈不行。）
* 该应用已获得用户授予的 SYSTEM_ALERT_WINDOW 权限。
* pendingIntent临时白名单机制，不拦截通过通知拉起的应用。
		
		通过通知，利用pendingIntent启动 Activity。
		通过通知，在 PendingIntent中发送广播，接收广播后启动 Activity。
		通过通知，在 PendingIntent中启动 Service（一定可以启动Service），在 Service 中启动 Activity。

* 该应用的某一项服务被其他可见应用绑定（进程优先级其实一致）。请注意，绑定到该服务的应用必须在后台对该应用保持可见，才能成功启动 Activity。
 
这里有一个比较有趣的点：**如果应用在前台任务的返回栈中具有一项Activity，并不是说一定要自己APP的Activity在展示，而是说，当前展示的Task栈里有自己的Activity就可以**，这点判断如下
      
	  boolean areBackgroundActivityStartsAllowed() {
	  		
	  		<!--白名单-->
	        // allow if the whitelisting flag was explicitly set
	        if (mAllowBackgroundActivityStarts) {
	            return true;
	        }
	        
	        ...
           <!--是否有Actvity位于前台任务栈中-->
	        // allow if the caller has an activity in any foreground task
	        if (hasActivityInVisibleTask()) {
	            return true;
	        }
	        <!--被前台APP绑定-->
	        // allow if the caller is bound by a UID that's currently foreground
	        if (isBoundByForegroundUid()) {
	            return true;
	        }
	        return false;
	    }
	    
hasActivityInVisibleTask 判断前台TASK栈是否有CallAPP的Activity
    
    private boolean hasActivityInVisibleTask() {
        for (int i = mActivities.size() - 1; i >= 0; --i) {
            TaskRecord task = mActivities.get(i).getTaskRecord();
            if (task == null) {
                continue;
            }
            ActivityRecord topActivity = task.getTopActivity();
            if (topActivity == null) {
                continue;
            }
            // If an activity has just been started it will not yet be visible, but
            // is expected to be soon. We treat this as if it were already visible.
            // This ensures a subsequent activity can be started even before this one
            // becomes visible.
            
            <!--只要是Task中的TOPActivity在展示，就判断CallAPP可见或者即将可见，TOPActivity不一定是CallAPP的-->
            if (topActivity.visible || topActivity.isState(INITIALIZING)) {
                return true;
            }
        }
        return false;
    }
    
只要是Task中的TOPActivity在展示，就判断CallAPP可见或者即将可见，TOPActivity不一定是CallAPP的，比如APP打开微信分享，如果直接上看APP是在后台，但是微信分享Activity没有单独开一Activity Task，那么CallAPP还是被看做前台，也就是他还可以启动Activity，在前后台的判断上，更像下沉到Task维度，而不是Activity维度。同Service不同，Activity严重依赖CallAPP的状态，而Service更关心被启动APP的状态。

# Android10后台限制启动Activity的系统bug

> 连续两次启动Activity，后台启动的限制会被打破

    private boolean hasActivityInVisibleTask() {
        for (int i = mActivities.size() - 1; i >= 0; --i) {
            TaskRecord task = mActivities.get(i).getTaskRecord();
            if (task == null) {
                continue;
            }
            ActivityRecord topActivity = task.getTopActivity();
            if (topActivity == null) {
                continue;
            }
            <!--bug起源-->
            // If an activity has just been started it will not yet be visible, but
            // is expected to be soon. We treat this as if it were already visible.
            // This ensures a subsequent activity can be started even before this one
            // becomes visible.
            if (topActivity.visible || topActivity.isState(INITIALIZING)) {
                return true;
            }
        }
        return false;
    }
    

如果应用位于后台，第一次启动Activity会被当做后台启动，但是ActiivityRecord仍然会被创建，同时State会被设置成INITIALIZING，并且位于当前将要启动Task的栈顶，

	  ActivityRecord(ActivityTaskManagerService _service, WindowProcessController _caller,
	            int _launchedFromPid, int _launchedFromUid, String _launchedFromPackage, Intent _intent,
	           ...
	        setState(INITIALIZING, "ActivityRecord ctor");
	        

那么如果在后台，再次通过startActivity启动，当前进程就会被认为是在前台，应用就会被拉起，**真是个奇葩bug**。因为满足如下条件。

	 topActivity.isState(INITIALIZING)

这个时候，Activity就可以在后台被启动。

# PendingIntent启动Activity不受限制原理


通知的进程是系统进程

	u0_a16        2102  1742 4104448 174924 0                   0 S com.android.systemui
	
系统进程不受限制，就是这么流弊。	

## 总结

* 通过通知启动Service不受后台限制的原因是存在可更新PendingTempWhitelist白名单
* 后台启动Activity严重依赖CallAPP的状态，而Service更关心被启动APP的状态
* 位于后台，连续多次startActivity就可以启动Activity，目前看是个系统bug
