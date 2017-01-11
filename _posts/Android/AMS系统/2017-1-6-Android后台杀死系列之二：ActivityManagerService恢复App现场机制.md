---
layout: post
title: "Android后台杀死系列之二：ActivityManagerService恢复App现场机制"
category: Android
image: http://upload-images.jianshu.io/upload_images/1460468-00df66d0bf4dec82.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---


本篇是Android后台杀死系列的第二篇，主要讲解ActivityMangerService是如何恢复被后台杀死的进程的（基于4.3 ），在开篇 [FragmentActivity及PhoneWindow后台杀死处理机制](http://www.jianshu.com/p/00fef8872b68) 中，简述了后台杀死所引起的一些常见问题，还有Android系统控件对后台杀死所做的一些兼容，以及onSaveInstance跟onRestoreInstance的作用于执行时机，最后说了如何应对后台杀死，但是对于被后台杀死的进程如何恢复的并没有讲解，本篇不涉及后台杀死，比如LowmemoryKiller机制，只讲述被杀死的进程如何恢复的。假设，一个应用被后台杀死，再次从最近的任务列表唤起App时候，系统是如何处理的呢？有这么几个问题可能需要解决：

* App被杀前的场景是如何保存的
* **系统（AMS）如何知道App被杀死了**
* 系统（AMS）如何恢复被杀的App
* 被后台杀死的App的启动流程跟普通的启动有什么区别
* Activity的恢复顺序为什么是倒序恢复


# 系统（AMS）如何知道App被杀死了

首先来看第一个问题，系统如何知道Application被杀死了，Android使用了Linux的oomKiller机制，只是简单的做了个变种，采用分等级的LowmemoryKiller，但是这个其实是内核层面，LowmemoryKiller杀死进程后，不会像用户空间发送通知，也就是说即使是框架层的ActivityMangerService也无法知道App是否被杀死，但是，只有知道App或者Activity是否被杀死，AMS（ActivityMangerService）才能正确的走唤起流程，那么AMS究竟是在什么时候知道App或者Activity被后台杀死了呢？我们先看一下从最近的任务列表进行唤起的时候，究竟发生了什么。

## 最近的任务列表或者Icon再次唤起App

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

## 在唤起App的时候AMS侦测App或者Activity是否被异常杀死

接着往下看moveTaskToFrontLocked，这个函数在ActivityStack中，主要管理ActivityRecord栈的，所有start的Activity都在ActivityStack中保留一个ActivityRecord，这个也是AMS管理Activiyt的一个依据，最终moveTaskToFrontLocked会调用resumeTopActivityLocked来唤起Activity，AMS获取即将resume的Activity信息的方式主要是通过ActivityRecord，它并不知道Activity本身是否存活，获取之后，AMS在唤醒Activity的环节才知道App或者Activity被杀死，而这个过程是通过异常来处理的，具体看一下resumeTopActivityLocked源码：
 
    final boolean resumeTopActivityLocked(ActivityRecord prev, Bundle options) {
      
         // This activity is now becoming visible.
            mService.mWindowManager.setAppVisibility(next.appToken, true);
                   
		 ....    恢复逻辑  
        if (next.app != null && next.app.thread != null) {
          // 正常恢复
            try {
                // Deliver all pending results.
                ArrayList a = next.results;
                if (a != null) {
                    final int N = a.size();
                    if (!next.finishing && N > 0) {
                        next.app.thread.scheduleSendResult(next.appToken, a);
                    }
                }
                ...
                next.app.thread.scheduleResumeActivity(next.appToken,
                        mService.isNextTransitionForward()); 
                ...
            } catch (Exception e) {
                // Whoops, need to restart this activity!
                // 这里需要重启，难道被后台杀死，走的是异常分支吗？？？？ 异常杀死
                if (DEBUG_STATES) Slog.v(TAG, "Resume failed; resetting state to "
                        + lastState + ": " + next);
                next.state = lastState;
                mResumedActivity = lastResumedActivity;
                <!--确实这里是因为进程挂掉了-->
                Slog.i(TAG, "Restarting because process died: " + next);
                 。。。
                startSpecificActivityLocked(next, true, false);
                return true;
            }
            ...
            }

由于没有主动调用finish的，所以AMS并不会清理掉ActivitRecord与TaskRecord ，因此resume的时候走的就是上面的分支，可以这里会调用next.app.thread.scheduleSendResult或者next.app.thread.scheduleResumeActivity进行唤起上一个Activity，但是如果APP或者Activity被异常杀死，那么唤起的操作一定是失败，会抛出异常，首先假设APP整个被杀死，那么APP端同AMS通信的Binder线程也不复存在，这个时候通过Binder进行通信就会抛出RemoteException，如此，就会走下面的catch部分，通过startSpecificActivityLocked再次将APP重建，并且将最后的Activity重建，其实你可以本地利用AIDL写一个C/S通信，在将一端关闭，然后用另一端访问，就会抛出RemoteException异常，如下图：

![Binder访问已经被杀死的进程](http://upload-images.jianshu.io/upload_images/1460468-00df66d0bf4dec82.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

还有一种可能，APP没有被kill，但是Activity被Kill掉了，这个时候会怎么样。首先，Activity的管理是一定通过AMS的，Activity的kill一定是是AMS操刀的，是有记录的，严格来说，这种情况并不属于后台杀死，因为这属于AMS正常的管理，在可控范围，比如打开了开发者模式中的“不保留活动”,这个时候，虽然会杀死Activity，但是仍然保留了ActivitRecord，所以再唤醒，或者回退的的时候仍然有迹可循,看一下ActivityStack的Destroy回调代码，
 
` final boolean destroyActivityLocked(ActivityRecord r,
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
    } `
    
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

# App被杀前的场景是如何保存的

## 新Activity启动跟旧Activity的保存

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

到这里ActivityManagerService恢复APP场景的逻辑就应该讲完了，再碎碎念一些问题，可能是一些面试的点。

 

# 主动清楚最近任务跟异常杀死的区别

# 冷热启动

![删除最近的任务.png](http://upload-images.jianshu.io/upload_images/1460468-436339b7fc278e2d.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


# 一句话概括后台杀死恢复原理：Application进程被Kill，但现场被AMS保存，AMS能根据保存恢复Application现场

# ActivityStack

# 几种场景分析

## ViewPager跟FragmentTabHost恢复View恢复

## FragemntDialog

# Lowmemorykiller不同版本在Framework层表现不同(LoLipop之后 单独封装成了服务LMKS)

## android5.0之前
## android5.0之后，采用了socket通信去service更新 

[参考](http://gityuan.com/2016/09/17/android-lowmemorykiller/)

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

# 原理：Activity永远是在AMS创建ActivityRecord之后创建的。异常场景AMS仍然可以保留ActivityRecord，在恢复的时候，AMS重建Activity

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
    
#  那么问题就是，什么时候后台杀死，杀死的时候如何处理taskId，会处理吗？主动后台杀死与主动清理手动退出的任务。被动杀死只是被Lowmemorykiller杀死。
       
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
        
 

# 但是如何判断是否被销毁，如何知道从oncreate还是从onresume开始 
 


##   mService.startProcessLocked其实是ActivitymanagerService，后台杀死跟正常的清除不太一样，后台杀死，现场保留，但是清理的话，是完全清除

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

## ActivityThread 自从2.3之后，就从pause到stop了

                ActivityManagerNative.getDefault().activityStopped(
                    activity.token, state, thumbnail, description);


## Activitymanagerservice

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
                            
                    
## activitymanagerNative

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
        
## activitymanagerservice

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
  
 
 

# 正常退出的处理机制

按返回键调用onBackPressed，finish自己。

    public void finishActivity(int requestCode) {
        if (mParent == null) {
            try {
                ActivityManagerNative.getDefault()
                    .finishSubActivity(mToken, mEmbeddedID, requestCode);
            } catch (RemoteException e) {
                // Empty
            }
        } else {
            mParent.finishActivityFromChild(this, requestCode);
        }
    }
    
AMS端


	   public final void finishSubActivity(IBinder token, String resultWho,
	            int requestCode) {
	        synchronized(this) {
	            final long origId = Binder.clearCallingIdentity();
	            mMainStack.finishSubActivityLocked(token, resultWho, requestCode);
	            Binder.restoreCallingIdentity(origId);
	        }
	    }
   
        
       void makeFinishing() {
        if (!finishing) {
            finishing = true;
            if (task != null && inHistory) {
            <!-- -- 可以跟上对应-->
                task.numActivities--;
            }
            if (stopped) {
                clearOptionsLocked();
            }
        }
    }
        
AMS PAUSE之后调用stop，APP端都是被动相应，其实APP端，都是被动响应，看着像主动，但是全是委派。
 
  
                    mStoppingActivities.add(prev);
                    if (mStoppingActivities.size() > 3) {
                        // If we already have a few activities waiting to stop,
                        // then give up on things going idle and start clearing
                        // them out.
            
                        scheduleIdleLocked();
                         
 
之后调用的时候，r.activity.mFinished就是true，不会再次保存现场

       if (finished) {
            r.activity.mFinished = true;
        }

                if (!r.activity.mFinished && saveState) {
                state = new Bundle();
                state.setAllowFds(false);
                mInstrumentation.callActivityOnSaveInstanceState(r.activity, state);
                r.state = state;
            }
            
# 强制杀死问题

	? I/ActivityManager: Process com.ls.tools (pid 12284) has died
	05-05 15:26:13.124 762-10606/? W/ActivityManager: Force removing ActivityRecord{1a378c0 u0 com.ls.tools/.activity.KillBackGroundActivity t759}: app died, no saved state
	05-05 15:26:13.135 12803-12803/? I/art: Late-enabling -Xcheck:jni


# Activity的恢复顺序，严格按照AMS中ActivityStack的顺序

#  滑动删除Task

removeTask函数是隐藏的，只能killBackGround


ActivityManager removeTask

    /**
     * Completely remove the given task.
     *
     * @param taskId Identifier of the task to be removed.
     * @return Returns true if the given task was found and removed.
     *
     * @hide
     */
    public boolean removeTask(int taskId) throws SecurityException {
        try {
            return ActivityManagerNative.getDefault().removeTask(taskId);
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }
    	
	
	
	// 处理滑动删除
	    public boolean removeTask(int taskId, int flags) {
	        synchronized (this) {
	            enforceCallingPermission(android.Manifest.permission.REMOVE_TASKS,
	                    "removeTask()");
	            long ident = Binder.clearCallingIdentity();
	            try {
	                ActivityRecord r = mMainStack.removeTaskActivitiesLocked(taskId, -1,
	                        false);
	                if (r != null) {
	                    mRecentTasks.remove(r.task);
	                    cleanUpRemovedTaskLocked(r.task, flags);
	                    return true;
	                } else {
	                    TaskRecord tr = null;
	                    int i=0;
	                    while (i < mRecentTasks.size()) {
	                        TaskRecord t = mRecentTasks.get(i);
	                        if (t.taskId == taskId) {
	                            tr = t;
	                            break;
	                        }
	                        i++;
	                    }
	                    if (tr != null) {
	                        if (tr.numActivities <= 0) {
	                            // Caller is just removing a recent task that is
	                            // not actively running.  That is easy!
	                            mRecentTasks.remove(i);
	                            cleanUpRemovedTaskLocked(tr, flags);
	                            return true;
	                        } else {
	                            Slog.w(TAG, "removeTask: task " + taskId
	                                    + " does not have activities to remove, "
	                                    + " but numActivities=" + tr.numActivities
	                                    + ": " + tr);
	                        }
	                    }
	                }
	            } finally {
	                Binder.restoreCallingIdentity(ident);
	            }
	        }
	        return false;
	    }
    
    
    
    private void cleanUpRemovedTaskLocked(TaskRecord tr, int flags) {
        final boolean killProcesses = (flags&ActivityManager.REMOVE_TASK_KILL_PROCESS) != 0;
        Intent baseIntent = new Intent(
                tr.intent != null ? tr.intent : tr.affinityIntent);
        ComponentName component = baseIntent.getComponent();
        if (component == null) {
            Slog.w(TAG, "Now component for base intent of task: " + tr);
            return;
        }

        // Find any running services associated with this app.
        mServices.cleanUpRemovedTaskLocked(tr, component, baseIntent);

        if (killProcesses) {
            // Find any running processes associated with this app.
            final String pkg = component.getPackageName();
            ArrayList<ProcessRecord> procs = new ArrayList<ProcessRecord>();
            HashMap<String, SparseArray<ProcessRecord>> pmap = mProcessNames.getMap();
            for (SparseArray<ProcessRecord> uids : pmap.values()) {
                for (int i=0; i<uids.size(); i++) {
                    ProcessRecord proc = uids.valueAt(i);
                    if (proc.userId != tr.userId) {
                        continue;
                    }
                    if (!proc.pkgList.contains(pkg)) {
                        continue;
                    }
                    procs.add(proc);
                }
            }

            // Kill the running processes.
            for (int i=0; i<procs.size(); i++) {
                ProcessRecord pr = procs.get(i);
                if (pr.setSchedGroup == Process.THREAD_GROUP_BG_NONINTERACTIVE) {
                    Slog.i(TAG, "Killing " + pr.toShortString() + ": remove task");
                    EventLog.writeEvent(EventLogTags.AM_KILL, pr.userId, pr.pid,
                            pr.processName, pr.setAdj, "remove task");
                    pr.killedBackground = true;
                    Process.killProcessQuiet(pr.pid);
                } else {
                    pr.waitingToKill = "remove task";
                }
            }
        }
    }
    
	    
# 参考文档

[Android应用程序启动过程源代码分析](http://blog.csdn.net/luoshengyang/article/details/6689748)

[Android Framework架构浅析之【近期任务】](http://blog.csdn.net/lnb333666/article/details/7869465)

[Android Low Memory Killer介绍](http://mysuperbaby.iteye.com/blog/1397863)

[Android开发之InstanceState详解]( http://www.cnblogs.com/hanyonglu/archive/2012/03/28/2420515.html )

[对Android近期任务列表（Recent Applications）的简单分析](http://www.cnblogs.com/coding-way/archive/2013/06/05/3118732.html)

[ Android——内存管理-lowmemorykiller 机制](http://blog.csdn.net/jscese/article/details/47317765)  

[Android 操作系统的内存回收机制](https://www.ibm.com/developerworks/cn/opensource/os-cn-android-mmry-rcycl/) 
  
[Android LowMemoryKiller原理分析](http://gityuan.com/2016/09/17/android-lowmemorykiller/)

[Android进程生命周期与ADJ](http://gityuan.com/2015/10/01/process-lifecycle/)

[Linux下/proc目录简介](http://blog.csdn.net/zdwzzu2006/article/details/7747977)


[startActivity启动过程分析 精](http://gityuan.com/2016/03/12/start-activity/)

[Activity销毁流程](http://blog.csdn.net/qq_23547831/article/details/51232309)
