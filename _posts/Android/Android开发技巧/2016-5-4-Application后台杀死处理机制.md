---
layout: post
title: "Application后台杀死处理机制"
description: "Java"
category: android开发

---


####  基于Android源码4.3
#### Activity后台杀死原理--总结一句话，进程死了，但是现场还在，AMS端根据保留的现场恢复进程 --ActivityStack

#### 场景
#### 原理
#### 注意事项

#### 场景

Android开发的时候经常会遇到这样的问题，App在后台久置之后，再次点击图标或者从最近的任务列表打开时，App可能会崩溃，这种情况往往是App在后台被系统杀死，在恢复的时候遇到了问题，这种问题经常出现在FragmentActivity中，尤其是里面添加了Fragment的时候。

其实我们可以模拟一下后台杀死，

    @OnClick(R.id.kill)
    void killSelf() {

        moveTaskToBack(true);
        new Handler(Looper.myLooper()).postDelayed(new Runnable() {
            @Override
            public void run() {
              <!--后台杀死-->
                ActivityManager am = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
                am.killBackgroundProcesses(getPackageName());

            }
        }, 3000);
    }

这样App再次从最近的任务列表中唤醒的时候，其实会回到上次被杀死的状态下。

#### 原理 Activity永远是在ActivityRecord之后创建。我们可以保留ActivityRecord，在需要的时候，创建Activity

分析入口：最近的任务列表 -》RecentsActivity.java-》RecentsPanelView.java,唤出最近的任务列表，并唤醒App的时候，其实是进行下面的操作。找到当前要启动App的TaskDescription，并判断该App是否有效，或者说是否被ActivityManagerService清空。如果清空了taskId=-1，就要全新启动APP，如果没有清空，说明App要么存活，要么被后台杀死，但是这两种状况下，APP的现场都是保留的，点击启动，AMS会恢复APP现场。
 
     public void handleOnClick(View view) {
        ViewHolder holder = (ViewHolder)view.getTag();
        TaskDescription ad = holder.taskDescription;
        final Context context = view.getContext();
        final ActivityManager am = (ActivityManager)
                context.getSystemService(Context.ACTIVITY_SERVICE);
        Bitmap bm = holder.thumbnailViewImageBitmap;
        boolean usingDrawingCache;
        if (bm.getWidth() == holder.thumbnailViewImage.getWidth() &&
                bm.getHeight() == holder.thumbnailViewImage.getHeight()) {
            usingDrawingCache = false;
        } else {
            holder.thumbnailViewImage.setDrawingCacheEnabled(true);
            bm = holder.thumbnailViewImage.getDrawingCache();
            usingDrawingCache = true;
        }
        Bundle opts = (bm == null) ?
                null :
                ActivityOptions.makeThumbnailScaleUpAnimation(
                        holder.thumbnailViewImage, bm, 0, 0, null).toBundle();

        show(false);
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
            } catch (SecurityException e) {
                Log.e(TAG, "Recents does not have the permission to launch " + intent, e);
            }
        }
        if (usingDrawingCache) {
            holder.thumbnailViewImage.setDrawingCacheEnabled(false);
        }
    }
    
####  那么问题就是，什么时候后台杀死，杀死的时候如何处理taskId，会处理吗？主动后台杀死与主动清理手动退出的任务。被动杀死只是被Lowmemorykiller杀死。
       
 后台杀死ActivityManager
 
     public void killBackgroundProcesses(String packageName) {
        try {
            ActivityManagerNative.getDefault().killBackgroundProcesses(packageName,
                    UserHandle.myUserId());
        } catch (RemoteException e) {
        }
    }
    
最终会调用ActivityManagerService中的函数  
            
                killPackageProcessesLocked(packageName, appId, userId,
                        ProcessList.SERVICE_ADJ, false, true, true, false, "kill background");
                        
        
        int N = procs.size();
        for (int i=0; i<N; i++) {
            removeProcessLocked(procs.get(i), callerWillRestart, allowRestart, reason);
        }
        
        
     private final void handleAppDiedLocked(ProcessRecord app,
            boolean restarting, boolean allowRestart) {
        cleanUpApplicationRecordLocked(app, restarting, allowRestart, -1);
 
 在清除Activity的时候，会根据是否保存了状态而清除，一般是执行类onsaveinstancestate，如果执行了，就不清除Activity，

	 // Remove this application's activities from active lists.
	        boolean hasVisibleActivities = mMainStack.removeHistoryRecordsForAppLocked(app);
	        
	    <!---->    
 
             if (r.app == app) {
                boolean remove;
                if ((!r.haveState && !r.stateNotNeeded) || r.finishing) {
                    // Don't currently have state for the activity, or
                    // it is finishing -- always remove it.
                    remove = true;
                } else if (r.launchCount > 2 &&
                        r.lastLaunchTime > (SystemClock.uptimeMillis()-60000)) {
                    // We have launched this activity too many times since it was
                    // able to run, so give up and remove it.
                    remove = true;
                } else {
                 // 其实这里说的很清楚，APP死了，但是Activity永生，呼呼呼呼
                    // The process may be gone, but the activity lives on!，
                    remove = false;
                }


这样就不会清除ActivityRecord，当用户获取最近的任务列表的时候: rti.id = tr.numActivities > 0 ? tr.taskId : -1; ，tr.numActivities是大于0的。

    public List<ActivityManager.RecentTaskInfo> getRecentTasks(int maxNum,
            int flags, int userId) {
        userId = handleIncomingUser(Binder.getCallingPid(), Binder.getCallingUid(), userId,
                false, true, "getRecentTasks", null);

        synchronized (this) {
            enforceCallingPermission(android.Manifest.permission.GET_TASKS,
                    "getRecentTasks()");
            final boolean detailed = checkCallingPermission(
                    android.Manifest.permission.GET_DETAILED_TASKS)
                    == PackageManager.PERMISSION_GRANTED;

            IPackageManager pm = AppGlobals.getPackageManager();

            final int N = mRecentTasks.size();
            ArrayList<ActivityManager.RecentTaskInfo> res
                    = new ArrayList<ActivityManager.RecentTaskInfo>(
                            maxNum < N ? maxNum : N);
            for (int i=0; i<N && maxNum > 0; i++) {
                TaskRecord tr = mRecentTasks.get(i);
                // Only add calling user's recent tasks
                if (tr.userId != userId) continue;
                // Return the entry if desired by the caller.  We always return
                // the first entry, because callers always expect this to be the
                // foreground app.  We may filter others if the caller has
                // not supplied RECENT_WITH_EXCLUDED and there is some reason
                // we should exclude the entry.

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
                
这样，启动最近任务就会走am.moveTaskToFront(ad.taskId, ActivityManager.MOVE_TASK_WITH_HOME, opts);分支，对于被后台杀死的APP，AMS会重建Activity及App。
 
    final boolean resumeTopActivityLocked(ActivityRecord prev, Bundle options) {
        // Find the first activity that is not finishing.
        ActivityRecord next = topRunningActivityLocked(null);
        
在移到前台显示的时候，如果没有Task，那就从新创建

	  final void moveTaskToFrontLocked(TaskRecord tr, ActivityRecord reason, Bundle options) {
        if (DEBUG_SWITCH) Slog.v(TAG, 

    final boolean resumeTopActivityLocked(ActivityRecord prev) {
        return resumeTopActivityLocked(prev, null);
    }

    final boolean resumeTopActivityLocked(ActivityRecord prev, Bundle options) {
        // Find the first activity that is not finishing.
        ActivityRecord next = topRunningActivityLocked(null);
        
#### onSaveInstanceState()的调用时机，都是在onPause或者onStop之前，Android Honeycomb之前之后，之前onPause，之后onStop，但是对于按返回键的怎么处理呢

	The reason why these slight inconsistencies exist stems from a significant change to the Activity lifecycle that was made in Honeycomb. Prior to Honeycomb, activities were not considered killable until after they had been paused, meaning that onSaveInstanceState() was called immediately before onPause(). Beginning with Honeycomb, however, Activities are considered to be killable only after they have been stopped, meaning that onSaveInstanceState() will now be called before onStop() instead of immediately before onPause(). These differences are summarized in the table below:


#### 但是如何判断是否被销毁，如何知道从oncreate还是从onresume开始 

其实这个交给AMS来完成，ActivityManagerService首先会去除ActivityRecord，然后去找Task或者说Process，如果找不到，就新建，新建之后就相当于恢复现场


#####   mService.startProcessLocked其实是ActivitymanagerService，后台杀死跟正常的清除不太一样，后台杀死，现场保留，但是清理的话，是完全清除

如果Activity所在的进程未启动，那么先启动进程，在进程起来后会调用attachApplicationLocked()，函数中会接着调用realStartActivityLocked()函数继续启动这个Activity；      
        
         private final void startSpecificActivityLocked(ActivityRecord r,
            boolean andResume, boolean checkConfig) {
        // Is this activity's application already running?
        ProcessRecord app = mService.getProcessRecordLocked(r.processName,
                r.info.applicationInfo.uid);

        if (r.launchTime == 0) {
            r.launchTime = SystemClock.uptimeMillis();
            if (mInitialStartTime == 0) {
                mInitialStartTime = r.launchTime;
            }
        } else if (mInitialStartTime == 0) {
            mInitialStartTime = SystemClock.uptimeMillis();
        }
        
        if (app != null && app.thread != null) {
            try {
                app.addPackage(r.info.packageName);
                realStartActivityLocked(r, app, andResume, checkConfig);
                return;
            } catch (RemoteException e) {
                Slog.w(TAG, "Exception when starting activity "
                        + r.intent.getComponent().flattenToShortString(), e);
            }

            // If a dead object exception was thrown -- fall through to
            // restart the application.
        }

        mService.startProcessLocked(r.processName, r.info.applicationInfo, true, 0,
                "activity", r.intent.getComponent(), false, false);
    }
        
### 	　　onStoreInstanceState()在onStart() 和 onPostCreate(Bundle)之间调用。

##### ActivityThread 自从2.3之后，就从pause到stop了

                ActivityManagerNative.getDefault().activityStopped(
                    activity.token, state, thumbnail, description);


##### Activitymanagerservice

    public final void activityStopped(IBinder token, Bundle icicle, Bitmap thumbnail,
            CharSequence description) {
        if (localLOGV) Slog.v(
            TAG, "Activity stopped: token=" + token);

        // Refuse possible leaked file descriptors
        if (icicle != null && icicle.hasFileDescriptors()) {
            throw new IllegalArgumentException("File descriptors passed in Bundle");
        }

        ActivityRecord r = null;

        final long origId = Binder.clearCallingIdentity();

        synchronized (this) {
            r = mMainStack.isInStackLocked(token);
            if (r != null) {
                r.stack.activityStoppedLocked(r, icicle, thumbnail, description);
            }
        }

        if (r != null) {
            sendPendingThumbnail(r, null, null, null, false);
        }

        trimApplications();
                            
                    
##### activitymanagerNative

	   public boolean onTransact(int code, Parcel data, Parcel reply, int flags)
	            throws RemoteException {
    。。。。。
        case ACTIVITY_STOPPED_TRANSACTION: {
            data.enforceInterface(IActivityManager.descriptor);
            IBinder token = data.readStrongBinder();
            Bundle map = data.readBundle();
            Bitmap thumbnail = data.readInt() != 0
                ? Bitmap.CREATOR.createFromParcel(data) : null;
            CharSequence description = TextUtils.CHAR_SEQUENCE_CREATOR.createFromParcel(data);
            activityStopped(token, map, thumbnail, description);
            reply.writeNoException();
            return true;
        }
        
##### activitymanagerservice

    public final void activityStopped(IBinder token, Bundle icicle, Bitmap thumbnail,
            CharSequence description) {
        if (localLOGV) Slog.v(
            TAG, "Activity stopped: token=" + token);

        // Refuse possible leaked file descriptors
        if (icicle != null && icicle.hasFileDescriptors()) {
            throw new IllegalArgumentException("File descriptors passed in Bundle");
        }

        ActivityRecord r = null;

        final long origId = Binder.clearCallingIdentity();

        synchronized (this) {
            r = mMainStack.isInStackLocked(token);
            if (r != null) {
            
            <!--icicle 其实就是保存的现场数据，但是Actvity栈信息其实是本来就有的。-->
                r.stack.activityStoppedLocked(r, icicle, thumbnail, description);
            }
        }
 
####  被动杀死Lowmemorykiller
    
Andorid用户层的Application，在各种Activity生命周期切换时都会触发AMS中的回收机制，比如启动新的apk，一直back 退出一个apk，除了android AMS中默认的回收机制外，还会去维护一个oom adj 变量，作为linux层 lowmemorykiller的参考依据，如果内存不够，就让底层决定杀死谁。

ActivityManagerService

        if (app.curAdj != app.setAdj) {
            if (Process.setOomAdj(app.pid, app.curAdj)) {
             
                app.setAdj = app.curAdj;
            } else {
                success = false;
                Slog.w(TAG, "Failed setting oom adj of " + app + " to " + app.curAdj);
            }
        }
               
#####  通过socket与Lowmemorykiller通信

    private final boolean applyOomAdjLocked(ProcessRecord app,
            ProcessRecord TOP_APP, boolean doingAll, long now) {
            
 
	   public static final void setOomAdj(int pid, int uid, int amt) {
	        if (amt == UNKNOWN_ADJ)
	            return;

    private static void writeLmkd(ByteBuffer buf) {

        for (int i = 0; i < 3; i++) {
            if (sLmkdSocket == null) {
                    if (openLmkdSocket() == false) {
                        try {
                            Thread.sleep(1000);
                        } catch (InterruptedException ie) {
                        }
                        continue;
                    }
            }

            try {
            

 可以看到try了3次 ，去打开对应的socket 然后写数据，openLmkdSocket 实现如下，android的 LocalSocket 机制，通过lmkd 这个socket通信

	sLmkdSocket = new LocalSocket(LocalSocket.SOCKET_SEQPACKET);
	            sLmkdSocket.connect(
	                new LocalSocketAddress("lmkd",
	                        LocalSocketAddress.Namespace.RESERVED));
	            sLmkdOutputStream = sLmkdSocket.getOutputStream();
 
这是作为client去请求connect ，而service端的处理在 \system\core\lmkd\lmkd.c ， 可以看下这个service的启动：

	service lmkd /system/bin/lmkd
	    class core
	    critical
	    socket lmkd seqpacket 0660 system system
 

#### 参考文档

[Android应用程序启动过程源代码分析](http://blog.csdn.net/luoshengyang/article/details/6689748)

[Android Framework架构浅析之【近期任务】](http://blog.csdn.net/lnb333666/article/details/7869465)

[Android Low Memory Killer介绍](http://mysuperbaby.iteye.com/blog/1397863)

[Android开发之InstanceState详解]( http://www.cnblogs.com/hanyonglu/archive/2012/03/28/2420515.html )

[对Android近期任务列表（Recent Applications）的简单分析](http://www.cnblogs.com/coding-way/archive/2013/06/05/3118732.html)

[ Android——内存管理-lowmemorykiller 机制](http://blog.csdn.net/jscese/article/details/47317765)  