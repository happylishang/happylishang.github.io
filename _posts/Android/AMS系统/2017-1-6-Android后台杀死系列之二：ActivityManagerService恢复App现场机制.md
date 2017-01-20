---
layout: post
title: "Android后台杀死系列之二：ActivityManagerService恢复App现场机制"
category: Android
image: http://upload-images.jianshu.io/upload_images/1460468-00df66d0bf4dec82.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---


本篇是Android后台杀死系列的第二篇，主要讲解ActivityMangerService是如何恢复被后台杀死的进程的（基于4.3 ），在开篇FragmentActivity及PhoneWindow后台杀死处理机制中，简述了后台杀死所引起的一些常见问题，还有Android系统控件对后台杀死所做的一些兼容，以及onSaveInstance跟onRestoreInstance的作用于执行时机，最后说了如何应对后台杀死。但是，对于被后台杀死的进程如何恢复的并没有讲解，本篇不涉及后台杀死，比如LowmemoryKiller机制，只讲述被杀死的进程如何恢复的。假设，一个应用被后台杀死，再次从最近的任务列表唤起App时候，系统是如何处理的呢？有这么几个问题可能需要解决：

* **Android框架层（AMS）如何知道App被杀死了**
* App被杀前的场景是如何保存的
* 系统（AMS）如何恢复被杀的App
* Activity的恢复顺序为什么是倒序恢复
 
[Android后台杀死系列之一：FragmentActivity及Fragment本质及后台杀死处理机制](http://www.jianshu.com/p/00fef8872b68) 

# Android框架层（AMS）如何知道App被杀死了

首先来看第一个问题，系统如何知道Application被杀死了，Android使用了Linux的oomKiller机制，只是简单的做了个变种，采用分等级的LowmemoryKiller，但这个其实是内核层面，LowmemoryKiller杀死进程后，如何向用户空间发送通知，并告诉框架层的ActivityMangerService呢？只有AMS在知道App或者Activity是否被杀死后，AMS（ActivityMangerService）才能正确的走重建或者唤起流程，比如，APP死了，但是由于存在需要复活的Service，那么这个时候，进程需要重新启动，这个时候怎么处理的，那么AMS究竟是在什么时候知道App或者Activity被后台杀死了呢？我们先看一下从最近的任务列表进行唤起的时候，究竟发生了什么。

## 从最近的任务列表或者Icon再次唤起App流程

在系统源码systemUi的包里，有个RecentActivity，这个其实就是最近的任务列表的入口，而其呈现界面是通过RecentsPanelView来展现的，点击最近的App其执行代码如下：


    public void handleOnClick(View view) {
        ViewHolder holder = (ViewHolder)view.getTag();
        TaskDescription ad = holder.taskDescription;
        final Context context = view.getContext();
        final ActivityManager am = (ActivityManager)
                context.getSystemService(Context.ACTIVITY_SERVICE);
        Bitmap bm = holder.thumbnailViewImageBitmap;
        ...
        // 关键点 1  如果TaskDescription没有被主动关闭，正常关闭，ad.taskId就是>=0
        if (ad.taskId >= 0) {
            // This is an active task; it should just go to the foreground.
            am.moveTaskToFront(ad.taskId, ActivityManager.MOVE_TASK_WITH_HOME,
                    opts);
        } else {
            Intent intent = ad.intent;
            intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY
                    | Intent.FLAG_ACTIVITY_TASK_ON_HOME
                    | Intent.FLAG_ACTIVITY_NEW_TASK);
            try {
                context.startActivityAsUser(intent, opts,
                        new UserHandle(UserHandle.USER_CURRENT));
            }...
    }

在上面的代码里面，有个判断ad.taskId >= 0，如果满足这个条件，就通过moveTaskToFront唤起APP，那么ad.taskId是如何获取的？recent包里面有各类RecentTasksLoader，这个类就是用来加载最近任务列表的一个Loader，看一下它的源码，主要看一下加载：

     @Override
            protected Void doInBackground(Void... params) {
                // We load in two stages: first, we update progress with just the first screenful
                // of items. Then, we update with the rest of the items
                final int origPri = Process.getThreadPriority(Process.myTid());
                Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);
                final PackageManager pm = mContext.getPackageManager();
                final ActivityManager am = (ActivityManager)
                mContext.getSystemService(Context.ACTIVITY_SERVICE);

                final List<ActivityManager.RecentTaskInfo> recentTasks =
                        am.getRecentTasks(MAX_TASKS, ActivityManager.RECENT_IGNORE_UNAVAILABLE);
                 
                ....
                    TaskDescription item = createTaskDescription(recentInfo.id,
                            recentInfo.persistentId, recentInfo.baseIntent,
                            recentInfo.origActivity, recentInfo.description);
                ....
                } 
                           
可以看到，其实就是通过ActivityManger的getRecentTasks向AMS请求最近的任务信息，然后通过createTaskDescription创建TaskDescription，这里传递的recentInfo.id其实就是TaskDescription的taskId，来看一下它的意义：

    public List<ActivityManager.RecentTaskInfo> getRecentTasks(int maxNum,
            int flags, int userId) {
            ...           
            IPackageManager pm = AppGlobals.getPackageManager();

            final int N = mRecentTasks.size();
            ...
            for (int i=0; i<N && maxNum > 0; i++) {
                TaskRecord tr = mRecentTasks.get(i);
                if (i == 0
                        || ((flags&ActivityManager.RECENT_WITH_EXCLUDED) != 0)
                        || (tr.intent == null)
                        || ((tr.intent.getFlags()
                                &Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS) == 0)) {
                    ActivityManager.RecentTaskInfo rti
                            = new ActivityManager.RecentTaskInfo();
                    rti.id = tr.numActivities > 0 ? tr.taskId : -1;
                    rti.persistentId = tr.taskId;
                    rti.baseIntent = new Intent(
                            tr.intent != null ? tr.intent : tr.affinityIntent);
                    if (!detailed) {
                        rti.baseIntent.replaceExtras((Bundle)null);
                    }
                    
可以看出RecentTaskInfo的id是由TaskRecord决定的，如果TaskRecord中numActivities > 0就去TaskRecord的Id，否则就取-1，这里的numActivities其实就是TaskRecode中记录的ActivityRecord的数目，更具体的细节可以自行查看ActivityManagerService及ActivityStack，那么这里就容易解释了，只要是存活的APP，或者被LowmemoryKiller杀死的APP，其AMS的ActivityRecord是完整保存的，这也是恢复的依据。对于RecentActivity获取的数据其实就是AMS中的翻版，它也是不知道将要唤起的APP是否是存活的，只要TaskRecord告诉RecentActivity是存货的，那么久直接走唤起流程。也就是通过ActivityManager的moveTaskToFront唤起App，至于后续的工作，就完全交给AMS来处理。现看一下到这里的流程图：

![从最近的任务列表唤起App的流程](http://upload-images.jianshu.io/upload_images/1460468-e9834e9ea80ad648.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# 整个APP被后台杀死的情况下AMS是如何恢复现场的

AMS与客户端的通信是通过Binder来进行的，并且通信是”全双工“的，且互为客户端跟服务器，也就说AMS向客户端发命令的时候，AMS是客户端，反之亦然。注意 **Binder有个讣告的功能的**：如果基于Binder通信的服务端（S）如果挂掉了，客户端（C）能够收到Binder驱动发送的一份讣告，告知客户端Binder服务挂了，可以把Binder驱动看作是第三方不死邮政机构，专门向客户端发偶像死亡通知。对于APP被异常杀死的情况下，这份讣告是发送给AMS的，AMS在收到通知后，就会针对APP被异常杀死的情况作出整理，这里牵扯到Binder驱动的代码有兴趣可以自己翻一下。之类直接冲讣告接受后端处理逻辑来分析,在AMS源码中，入口其实就是appDiedLocked.
	
	final void appDiedLocked(ProcessRecord app, int pid,
	            IApplicationThread thread) {
				  ...
		        if (app.pid == pid && app.thread != null &&
	                app.thread.asBinder() == thread.asBinder()) {
	            boolean doLowMem = app.instrumentationClass == null;
	            关键点1 
	            handleAppDiedLocked(app, false, true);
	        	 // 如果是被后台杀了，怎么处理
	        	 关键点2 
	            if (doLowMem) {
	                boolean haveBg = false;
	                for (int i=mLruProcesses.size()-1; i>=0; i--) {
	                    ProcessRecord rec = mLruProcesses.get(i);
	                    if (rec.thread != null && rec.setAdj >= ProcessList.HIDDEN_APP_MIN_ADJ) {
	                        haveBg = true;
	                        break;
	                    }
	                }
	                if (!haveBg) {
	                <!--如果被LowmemoryKiller杀了，就说明内存紧张，这个时候就会通知其他后台APP，小心了，赶紧释放资源-->
	                    EventLog.writeEvent(EventLogTags.AM_LOW_MEMORY, mLruProcesses.size());
	                    long now = SystemClock.uptimeMillis();
	                    for (int i=mLruProcesses.size()-1; i>=0; i--) {
	                        ProcessRecord rec = mLruProcesses.get(i);
	                        if (rec != app && rec.thread != null &&
	                                (rec.lastLowMemory+GC_MIN_INTERVAL) <= now) {
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
	                    mHandler.sendEmptyMessage(REPORT_MEM_USAGE);
	                    <!--缩减资源-->
	                    scheduleAppGcsLocked();
	                }
	            }
	        }
	        ...
	    }

先看关键点1：在进程被杀死后，AMS端要选择性清理进程相关信息，清理后，再根据是不是内存低引起的后台杀死，决定是不是需要清理其他后台进程。接着看handleAppDiedLocked如何清理的，这里有重建时的依据：**ActivityRecord不清理，但是为它设置个APP未绑定的标识**

    private final void handleAppDiedLocked(ProcessRecord app,
            boolean restarting, boolean allowRestart) {
        
        关键点1
        cleanUpApplicationRecordLocked(app, restarting, allowRestart, -1);
        ...
        关键点2
         // Remove this application's activities from active lists.
        boolean hasVisibleActivities = mMainStack.removeHistoryRecordsForAppLocked(app);

        app.activities.clear();
        ...
		 关键点3
        if (!restarting) {
            if (!mMainStack.resumeTopActivityLocked(null)) {
                // If there was nothing to resume, and we are not already
                // restarting this process, but there is a visible activity that
                // is hosted by the process...  then make sure all visible
                // activities are running, taking care of restarting this
                // process.
                if (hasVisibleActivities) {
                    mMainStack.ensureActivitiesVisibleLocked(null, 0);
                }
            }
        }
    }

看关键点1，cleanUpApplicationRecordLocked，主要负责清理一些Providers，receivers，service之类的信息，并且在清理过程中根据配置的一些信息决定是否需要重建进程并启动，关键点2 就是关系到唤起流程的判断，关键点3，主要是被杀的进程是否是当前前台进程，如果是，需要重建，并立即显示：先简单看cleanUpApplicationRecordLocked的清理流程
    
     private final void cleanUpApplicationRecordLocked(ProcessRecord app,
            boolean restarting, boolean allowRestart, int index) {
            
       <!--清理service-->
        mServices.killServicesLocked(app, allowRestart);
        ...
        boolean restart = false;
       <!--清理Providers.-->
        if (!app.pubProviders.isEmpty()) {
            Iterator<ContentProviderRecord> it = app.pubProviders.values().iterator();
            while (it.hasNext()) {
                ContentProviderRecord cpr = it.next();
				。。。
            app.pubProviders.clear();
        } ...
         <!--清理receivers.-->
         // Unregister any receivers.
        if (app.receivers.size() > 0) {
            Iterator<ReceiverList> it = app.receivers.iterator();
            while (it.hasNext()) {
                removeReceiverLocked(it.next());
            }
            app.receivers.clear();
        }
        ... 关键点1，进程是够需要重启，
        if (restart && !app.isolated) {
            // We have components that still need to be running in the
            // process, so re-launch it.
            mProcessNames.put(app.processName, app.uid, app);
            startProcessLocked(app, "restart", app.processName);
        } 
		 ...
    }
 
 从关键点1就能知道，这里是隐藏了进程是否需要重启的逻辑，比如一个Service设置了START_STICKY，被杀后，就需要重新唤起，这里也是流氓软件肆虐的原因。再接着看mMainStack.removeHistoryRecordsForAppLocked(app)，对于直观理解APP重建
，这句代码处于核心的地位，

    boolean removeHistoryRecordsForAppLocked(ProcessRecord app) {
        ...
        while (i > 0) {
            i--;
            ActivityRecord r = (ActivityRecord)mHistory.get(i);
            if (r.app == app) {
                boolean remove;
                <!--关键点1-->
                if ((!r.haveState && !r.stateNotNeeded) || r.finishing) {
                    remove = true;
                } else if (r.launchCount > 2 &&
                    remove = true;
                } else {
                 //一般来讲，走false
                    remove = false;
                }
                <!--关键点2-->
                if (remove) {
                    ...
                    removeActivityFromHistoryLocked(r);

                } else {
                    ...
                    r.app = null;
                    ...
        }

        return hasVisibleActivities;
    }
        
在Activity跳转的时候，准确的说，在stopActivity之前，会保存Activity的现场，这样在AMS端r.haveState==true，也就是说，其ActivityRecord不会被从ActivityStack中移除，同时ActivityRecord的app字段被置空，这里在恢复的时候，是决定resume还是重建的关键。接着往下看moveTaskToFrontLocked，这个函数在ActivityStack中，主要管理ActivityRecord栈的，所有start的Activity都在ActivityStack中保留一个ActivityRecord，这个也是AMS管理Activiyt的一个依据，最终moveTaskToFrontLocked会调用resumeTopActivityLocked来唤起Activity，AMS获取即将resume的Activity信息的方式主要是通过ActivityRecord，它并不知道Activity本身是否存活，获取之后，AMS在唤醒Activity的环节才知道App或者Activity被杀死，具体看一下resumeTopActivityLocked源码：

	final boolean resumeTopActivityLocked(ActivityRecord prev, Bundle options) {
			  ...
			 关键点1 
	        if (next.app != null && next.app.thread != null) { 
	        ...
	        
	        } else {
	            // Whoops, need to restart this activity!
			  ...
	            startSpecificActivityLocked(next, true, true);
	        }
	
	        return true;
	    }
	    
看关键点1的判断条件，由于已经将ActivityRecord的app字段置空，AMS就知道了这个APP或者Activity被异常杀死过，因此，就会走startSpecificActivityLocked进行重建。 其实仔细想想很简单，对于主动调用finish的，AMS并不会清理掉ActivitRecord，在唤起APP的时候，如果AMS检测到APP还存活，就走scheduleResumeActivity进行唤起上一个Activity，但是如果APP或者Activity被异常杀死过，那么AMS就通过startSpecificActivityLocked再次将APP重建，并且将最后的Activity重建。

# APP存活，但是Activity被后台杀死的情况下AMS是如何恢复现场的

还有一种可能，APP没有被kill，但是Activity被Kill掉了，这个时候会怎么样。首先，Activity的管理是一定通过AMS的，Activity的kill一定是是AMS操刀的，是有记录的，严格来说，这种情况并不属于后台杀死，因为这属于AMS正常的管理，在可控范围，比如打开了开发者模式中的“不保留活动”,这个时候，虽然会杀死Activity，但是仍然保留了ActivitRecord，所以再唤醒，或者回退的的时候仍然有迹可循,看一下ActivityStack的Destroy回调代码，
 
	  final boolean destroyActivityLocked(ActivityRecord r,
	            boolean removeFromApp, boolean oomAdj, String reason) {
	        ...
	        if (hadApp) {
	          ...
	           boolean skipDestroy = false;
	            try {
	             关键代码 1
	                r.app.thread.scheduleDestroyActivity(r.appToken, r.finishing,
	                        r.configChangeFlags);
	         	...
	            if (r.finishing && !skipDestroy) {
	                if (DEBUG_STATES) Slog.v(TAG, "Moving to DESTROYING: " + r
	                        + " (destroy requested)");
	                r.state = ActivityState.DESTROYING;
	                Message msg = mHandler.obtainMessage(DESTROY_TIMEOUT_MSG);
	                msg.obj = r;
	                mHandler.sendMessageDelayed(msg, DESTROY_TIMEOUT);
	            } else {
	          关键代码 2
	                r.state = ActivityState.DESTROYED;
	                if (DEBUG_APP) Slog.v(TAG, "Clearing app during destroy for activity " + r);
	                r.app = null;
	            }
	        } 
	        return removedFromHistory;
	    }  
    
这里有两个关键啊你单，**1**是告诉客户端的AcvitityThread清除Activity，**2**是标记如果AMS自己非正常关闭的Activity，就将ActivityRecord的state设置为ActivityState.DESTROYED，并且**清空它的ProcessRecord引用**：r.app = null。这里是唤醒时候的一个重要标志，通过这里AMS就能知道Activity被自己异常关闭了，设置ActivityState.DESTROYED是为了让避免后面的清空逻辑。

    final void activityDestroyed(IBinder token) {
        synchronized (mService) {
            final long origId = Binder.clearCallingIdentity();
            try {
                ActivityRecord r = ActivityRecord.forToken(token);
                if (r != null) {
                    mHandler.removeMessages(DESTROY_TIMEOUT_MSG, r);
                }
               int index = indexOfActivityLocked(r);
                if (index >= 0) {
                1  <!--这里会是否从history列表移除ActivityRecord-->
                    if (r.state == ActivityState.DESTROYING) {
                        cleanUpActivityLocked(r, true, false);
                        removeActivityFromHistoryLocked(r);
                    }
                }
                resumeTopActivityLocked(null);
            } finally {
                Binder.restoreCallingIdentity(origId);
            }
        }
    }

看代码关键点**1**，只有r.state == ActivityState.DESTROYING的时候，才会移除ActivityRecord，但是对于不非正常finish的Activity，其状态是不会被设置成ActivityState.DESTROYING，是直接跳过了ActivityState.DESTROYING，被设置成了ActivityState.DESTROYED，所以不会removeActivityFromHistoryLocked，也就是保留了ActivityRecord现场，好像也是依靠异常来区分是否是正常的结束掉Activity。这种情况下是如何启动Activity的呢？ 通过上面两点分析，就知道了两个关键点

1. ActivityRecord没有动HistoryRecord列表中移除
2. ActivityRecord 的ProcessRecord字段被置空，r.app = null 

这样就保证了在resumeTopActivityLocked的时候，走startSpecificActivityLocked分支
	
	final boolean resumeTopActivityLocked(ActivityRecord prev, Bundle options) {
			  ...
			 	
	        if (next.app != null && next.app.thread != null) { 
	        ...
	        
	        } else {
	            // Whoops, need to restart this activity!
			  ...
	            startSpecificActivityLocked(next, true, true);
	        }
	
	        return true;
	    }

到这里，AMS就知道了这个APP或者Activity是不是被异常杀死过，从而，决定是走resume流程还是restore流程。

# App被杀前的场景是如何保存：新Activity启动跟旧Activity的保存

App现场的保存流程相对是比较简单的，入口基本就是startActivity的时候，只要是界面的跳转基本都牵扯到Activity的切换跟当前Activity场景的保存：先画个简单的图形，开偏里面讲FragmentActivity的时候，简单说了一些onSaveInstance的执行时机，这里详细看一下AMS是如何管理这些跳转以及场景保存的，模拟场景：Activity A 启动Activity B的时候，这个时候A不可见，可能会被销毁，需要保存A的现场，这个流程是什么样的：简述如下

* ActivityA startActivity ActivityB
* ActivityA pause 
* ActivityB create
* ActivityB start
* ActivityB resume
* ActivityA onSaveInstance
* ActivityA stop 

流程大概是如下样子：

![新Activity加载以及前Activity保存流程](http://upload-images.jianshu.io/upload_images/1460468-a94bb95b307325f4.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


现在我们通过源码一步一步跟一下，看看AMS在新Activity启动跟旧Activity的保存的时候，到底做了什么：跳过简单的startActivity,直接去AMS中去看
	        
> ActivityManagerService    
     
      public final int startActivityAsUser(IApplicationThread caller, String callingPackage,
            Intent intent, String resolvedType, IBinder resultTo,
            String resultWho, int requestCode, int startFlags,
            String profileFile, ParcelFileDescriptor profileFd, Bundle options, int userId) {
        enforceNotIsolatedCaller("startActivity");
         ...
        return mMainStack.startActivityMayWait(caller, -1, callingPackage, intent, resolvedType,
                resultTo, resultWho, requestCode, startFlags, profileFile, profileFd,
                null, null, options, userId);
    }
    
> ActivityStack

    final int startActivityMayWait(IApplicationThread caller, int callingUid,
                      
            int res = startActivityLocked(caller, intent, resolvedType,
                    aInfo, resultTo, resultWho, requestCode, callingPid, callingUid,
                    callingPackage, startFlags, options, componentSpecified, null);
            
         。。。
    } 
 
这里通过startActivityMayWait启动新的APP，或者新Activity，这里只看简单的，至于从桌面启动App的流程，可以去参考更详细的文章，比如老罗的startActivity流程，大概就是新建ActivityRecord，ProcessRecord之类，并加入AMS中相应的堆栈等，resumeTopActivityLocked是界面切换的统一入口，第一次进来的时候，由于ActivityA还在没有pause，因此需要先暂停ActivityA，这些完成后，
  
  ActivityStack  
    
      final boolean resumeTopActivityLocked(ActivityRecord prev, Bundle options) {       
         ...
         <!--必须将当前Resume的Activity设置为pause 然后stop才能继续-->
       // We need to start pausing the current activity so the top one
        // can be resumed...
        if (mResumedActivity != null) {            
            if (next.app != null && next.app.thread != null) {
                
                mService.updateLruProcessLocked(next.app, false);
            }
            startPausingLocked(userLeaving, false);
            return true;
            }
            ....
    }
   
 其实这里就是暂停ActivityA，AMS通过Binder告诉ActivityThread需要暂停的ActivityA，ActivityThread完成后再通过Binder通知AMS，AMS会开始resume ActivityB， 
   
    private final void startPausingLocked(boolean userLeaving, boolean uiSleeping) {

        if (prev.app != null && prev.app.thread != null) {
           ...
            try {
                prev.app.thread.schedulePauseActivity(prev.appToken, prev.finishing,
                        userLeaving, prev.configChangeFlags);
 
>  ActivityThread  
	   
	   private void handlePauseActivity(IBinder token, boolean finished,
	            boolean userLeaving, int configChanges) {
	        ActivityClientRecord r = mActivities.get(token);
	        if (r != null) {
	            ...
	            performPauseActivity(token, finished, r.isPreHoneycomb());
	            ...
	            // Tell the activity manager we have paused.
	            try {
	                ActivityManagerNative.getDefault().activityPaused(token);
	            } catch (RemoteException ex) {
	            }
	        }
	    }

AMS收到ActivityA发送过来的pause消息之后，就会唤起ActivityB，入口还是resumeTopActivityLocked，唤醒B，之后还会A给进一步stop掉，这个时候就牵扯到现场的保存，
 
ActivityStack
 
     private final void completePauseLocked() {
       
        if (!mService.isSleeping()) {
            resumeTopActivityLocked(prev);
        } else {
        
       ...
    }   
ActivityB如何启动的，本文不关心，只看ActivityA如何保存现场的，ActivityB起来后，会通过ActivityStack的stopActivityLocked去stop ActivityA，


    private final void stopActivityLocked(ActivityRecord r) {
           ...
            if (mMainStack) {
                 
                r.app.thread.scheduleStopActivity(r.appToken, r.visible, r.configChangeFlags);
            ...
           }    
    
    
  
回看APP端，看一下ActivityThread中的调用：首先通过callActivityOnSaveInstanceState，将现场保存到Bundle中去，

        private void performStopActivityInner(ActivityClientRecord r,
            StopInfo info, boolean keepShown, boolean saveState) {
           ...
            // Next have the activity save its current state and managed dialogs...
            if (!r.activity.mFinished && saveState) {
                if (r.state == null) {
                    state = new Bundle();
                    state.setAllowFds(false);
                    mInstrumentation.callActivityOnSaveInstanceState(r.activity, state);
                    r.state = state;
             。。。
             }
 
 之后，通过ActivityManagerNative.getDefault().activityStopped，通知AMS Stop动作完成，在通知的时候，还会将保存的现场数据带过去。
  
  
    private static class StopInfo implements Runnable {
        ActivityClientRecord activity;
        Bundle state;
        Bitmap thumbnail;
        CharSequence description;

        @Override public void run() {
            // Tell activity manager we have been stopped.
            try {
   
                ActivityManagerNative.getDefault().activityStopped(
                    activity.token, state, thumbnail, description);
            } catch (RemoteException ex) {
            }
        }
    }
    
通过上面流程，AMS不仅启动了新的Activity，同时也将上一个Activity的现场进行了保存，及时由于种种原因上一个Actiivity被杀死，在回退，或者重新唤醒的过程中AMS也能知道如何唤起Activiyt，并恢复。

现在解决两个问题，1、如何保存现场，2、AMS怎么判断知道APP或者Activity是否被异常杀死，那么就剩下最后一个问题了，AMS如何恢复被异常杀死的APP或者Activity呢。
  
# Activity或者Application恢复流程

## Application被后台杀死

其实在讲解AMS怎么判断知道APP或者Activity是否被异常杀死的时候，就已经涉及了恢复的逻辑，也知道了一旦AMS知道了APP被后台杀死了，那就不是正常的resuem流程了，而是要重新laucher，先来看一下整个APP被干掉的会怎么处理，看resumeTopActivityLocked部分,从上面的分析已知，这种场景下，会因为Binder通信抛异常走异常分支，如下：

    final boolean resumeTopActivityLocked(ActivityRecord prev, Bundle options) {
      ....
	  if (next.app != null && next.app.thread != null) {
	            if (DEBUG_SWITCH) Slog.v(TAG, "Resume running: " + next);
	            ...            
	            try {
	             ...
	            } catch (Exception e) {
	                // Whoops, need to restart this activity!
	                这里是知道整个app被杀死的
	                Slog.i(TAG, "Restarting because process died: " + next);
	                next.state = lastState;
	                mResumedActivity = lastResumedActivity;
	                Slog.i(TAG, "Restarting because process died: " + next);
	              
	                startSpecificActivityLocked(next, true, false);
	                return true;
	            }
            
从上面的代码可以知道，其实就是走startSpecificActivityLocked，这根第一次从桌面唤起APP没多大区别，只是有一点需要注意，那就是这种时候启动的Activity是有上一次的现场数据传递过得去的，因为上次在退到后台的时候，所有Activity界面的现场都是被保存了，并且传递到AMS中去的，那么这次的**恢复启动**就会将这些数据返回给ActivityThread，再来仔细看一下performLaunchActivity里面关于恢复的特殊处理代码：


    private Activity performLaunchActivity(ActivityClientRecord r, Intent customIntent) {
 

        ActivityInfo aInfo = r.activityInfo;
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
            Application app = r.packageInfo.makeApplication(false, mInstrumentation);
                ...
                 关键点 1 
                mInstrumentation.callActivityOnCreate(activity, r.state);
                ...
                r.activity = activity;
                r.stopped = true;
                if (!r.activity.mFinished) {
                    activity.performStart();
                    r.stopped = false;
                }
                关键点 1 
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
    
看一下关键点1跟2，先看关键点1，mInstrumentation.callActivityOnCreate会回调Actiivyt的onCreate，这个函数里面其实主要针对FragmentActivity做一些Fragment恢复的工作，ActivityClientRecord中的r.state是AMS传给APP用来恢复现场的，正常启动的时候，这些都是null。再来看关键点2 ，在r.state != null非空的时候执行mInstrumentation.callActivityOnRestoreInstanceState，这个函数默认主要就是针对Window做一些恢复工作，比如ViewPager恢复之前的显示位置等，也可以用来恢复用户保存数据。

## Application没有被后台杀死

打开开发者模式”不保留活动“，就是这种场景，在上面的分析中，知道，AMS主动异常杀死Activity的时候，将AcitivityRecord的app字段置空，因此resumeTopActivityLocked同整个APP被杀死不同，会走下面的分支

     final boolean resumeTopActivityLocked(ActivityRecord prev, Bundle options) {
         ...
        	
        if (next.app != null && next.app.thread != null) { 
			...
			
        } else {
        		关键点 1 只是重启Activity，可见这里其实是知道的，进程并没死，
            // Whoops, need to restart this activity!
            
            startSpecificActivityLocked(next, true, true);
        }

        return true;
    }
    
虽然不太一样，但是同样走startSpecificActivityLocked流程，只是不新建APP进程，其余的都是一样的，不再讲解。到这里，我们应该就了解了，

* Android是如何在预防的情况下保存场景
* AMS如何知道APP是否被后台杀死
* AMS如何根据ActivityStack重建APP被杀死时的场景

到这里ActivityManagerService恢复APP场景的逻辑就应该讲完了。再碎碎念一些问题，可能是一些面试的点。

*  主动清除最近任务跟异常杀死的区别：ActivityStack是否正常清除
*  恢复的时候，为什么是倒序恢复：因为这是ActivityStack中的HistoryRecord中栈的顺序，严格按照AMS端来
*  一句话概括Android后台杀死**恢复**原理：Application进程被Kill，但现场被AMS保存，AMS能根据保存恢复Application现场

**仅供参考，欢迎指正**
   	    
# 参考文档

[Android应用程序启动过程源代码分析](http://blog.csdn.net/luoshengyang/article/details/6689748)         
[Android Framework架构浅析之【近期任务】](http://blog.csdn.net/lnb333666/article/details/7869465)        
[Android Low Memory Killer介绍](http://mysuperbaby.iteye.com/blog/1397863)         
[Android开发之InstanceState详解]( http://www.cnblogs.com/hanyonglu/archive/2012/03/28/2420515.html )        
[对Android近期任务列表（Recent Applications）的简单分析](http://www.cnblogs.com/coding-way/archive/2013/06/05/3118732.html)       
[Android——内存管理-lowmemorykiller 机制](http://blog.csdn.net/jscese/article/details/47317765)         
[Android 操作系统的内存回收机制](https://www.ibm.com/developerworks/cn/opensource/os-cn-android-mmry-rcycl/)        
[Android LowMemoryKiller原理分析](http://gityuan.com/2016/09/17/android-lowmemorykiller/)      
[Android进程生命周期与ADJ](http://gityuan.com/2015/10/01/process-lifecycle/)       
[Linux下/proc目录简介](http://blog.csdn.net/zdwzzu2006/article/details/7747977)      
[startActivity启动过程分析 精](http://gityuan.com/2016/03/12/start-activity/)
[Activity销毁流程](http://blog.csdn.net/qq_23547831/article/details/51232309) 
[android binder 进程间通信机制3-Binder 对象生死](http://www.bozhiyue.com/anroid/wenzhang/2016/1026/571932.html)                
[linux驱动之定时任务timer，队列queue，小任务tasklet机制及用法](http://blog.csdn.net/u013256018/article/details/47803941)