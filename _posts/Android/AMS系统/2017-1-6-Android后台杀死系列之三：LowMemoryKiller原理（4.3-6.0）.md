---
layout: post
title: "Android后台杀死系列之三：LowMemoryKiller原理（4.3-6.0）"
description: "Java"
category: android开发

---


本篇是Android后台杀死系列的第三篇，主要讲解的是Android后台杀死原理：LowMemoryKiller，

LowMemoryKiller是Andorid基于Linux的oomKiller原理所扩展的一个多层次oomKiller，假设让你设计一个LowMemoryKiller，你会如何做，这样一个系统需要什么功能模块呢？

* 进程优先级：先杀谁，后杀谁
* 进程优先级的动态管理：一个进程的优先级不应该是固定不变的，需要根据其变动而动态变化
* 杀死的时机，什么时候需要挑一个，或者挑多个进程杀死

以上三个问题可能是一个MemoryKiller模块需要的基本功能，


Android系统以Linux内核为基础，所以对于进程的管理自然离不开Linux本身提供的机制。例如：

* 通过fork来创建进行
* 通过信号量来管理进程
* 通过proc文件系统来查询和调整进程状态 等


Android中对于内存的回收，主要依靠Lowmemorykiller来完成，是一种根据阈值级别触发相应力度的内存回收的机制。
Android开发经常会遇到这样的问题，App在后台久置之后，再次点击图标或从最近的任务列表打开时，App可能会崩溃。这种情况往往是App在后台被系统杀死，在恢复的时候遇到了问题，这种问题经常出现在FragmentActivity中，尤其是里面添加了Fragment的时候。开发时一直遵守谷歌的Android开发文档，创建Fragment尽量采用推荐的参数传递方式，并且保留默认的Fragment无参构造方法，避免绝大部分后台杀死-恢复崩溃的问题，但是对于原理的了解紧限于恢复时的重建机制，采用反射机制，并使用了默认的构造参数，直到使用FragmentDialog，示例代码如下：



一、 进程生命周期
Android系统将尽量长时间地保持应用进程，但为了新建进程或运行更重要的进程，最终需要清除旧进程来回收内存。 为了确定保留或终止哪些进程，系统会根据进程中正在运行的组件以及这些组件的状态，将每个进程放入“重要性层次结构”中。 必要时，系统会首先消除重要性最低的进程，然后是清除重要性稍低一级的进程，依此类推，以回收系统资源。

进程的重要性，划分5级：

前台进程(Foreground process)
可见进程(Visible process)
服务进程(Service process)
后台进程(Background process)
空进程(Empty process)




* ADJ级别	取值	解释
* UNKNOWN_ADJ	16	一般指将要会缓存进程，无法获取确定值
* CACHED_APP_MAX_ADJ	15	不可见进程的adj最大值 1
* CACHED_APP_MIN_ADJ	9	不可见进程的adj最小值 2
* SERVICE_B_AD	8	B List中的Service（较老的、使用可能性更小）
* PREVIOUS_APP_ADJ	7	上一个App的进程(往往通过按返回键)
* HOME_APP_ADJ	6	Home进程
* SERVICE_ADJ	5	服务进程(Service process)
* HEAVY_WEIGHT_APP_ADJ	4	后台的重量级进程，system/rootdir/init.rc文件中设置
* BACKUP_APP_ADJ	3	备份进程 3
* PERCEPTIBLE_APP_ADJ	2	可感知进程，比如后台音乐播放 4
* VISIBLE_APP_ADJ	1	可见进程(Visible process) 5
* FOREGROUND_APP_ADJ	0	前台进程（Foreground process） 6
* PERSISTENT_SERVICE_ADJ	-11	关联着系统或persistent进程
* PERSISTENT_PROC_ADJ	-12	系统persistent进程，比如telephony
* SYSTEM_ADJ	-16	系统进程
* NATIVE_ADJ	-17	

# oomAdj


# LMKD
# 进程保活
# 冷热启动
# 内核部分 
# oomKILLer(Linux自带)与Lowmemeorykiller（android定制）

# Lowmemeorykiller流程

# 进程优先级更新逻辑

![LowMemorykiller更新进程优先级](http://upload-images.jianshu.io/upload_images/1460468-ff1cdc46734ac3e2.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![5.0更新](http://upload-images.jianshu.io/upload_images/1460468-97a3a5e8a2a9555f.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

以finish为例

Activity

    private void finish(int finishTask) {
		...
                if (ActivityManagerNative.getDefault()
                        .finishActivity(mToken, resultCode, resultData, finishTask)) {
                    mFinished = true;

ActivityManagerService

    @Override
    public boolean finishActivityAffinity(IBinder token) {
     			...
                return task.stack.finishActivityAffinityLocked(r);
        
    
ActivityStack    

    private final ActivityRecord finishCurrentActivityLocked(ActivityRecord r,
            int index, int mode, boolean oomAdj) {
        // First things first: if this activity is currently visible,
        // and the resumed activity is not yet visible, then hold off on
        // finishing until the resumed one becomes visible.
        if (mode == FINISH_AFTER_VISIBLE && r.nowVisible) {
            if (!mStoppingActivities.contains(r)) {
                mStoppingActivities.add(r);
                if (mStoppingActivities.size() > 3) {
                    // If we already have a few activities waiting to stop,
                    // then give up on things going idle and start clearing
                    // them out.
                    scheduleIdleLocked();
                } else {
                    checkReadyForSleepLocked();
                }
            }
            if (DEBUG_STATES) Slog.v(TAG, "Moving to STOPPING: " + r
                    + " (finish requested)");
            r.state = ActivityState.STOPPING;
            if (oomAdj) {
            
            <!--更新oomAdj-->
            
                mService.updateOomAdjLocked();
            }
            return r;
        }
        
ActivityManagerService
        
    private final boolean updateOomAdjLocked(ProcessRecord app, int hiddenAdj,
            int clientHiddenAdj, int emptyAdj, ProcessRecord TOP_APP, boolean doingAll) {
        app.hiddenAdj = hiddenAdj;
        app.clientHiddenAdj = clientHiddenAdj;
        app.emptyAdj = emptyAdj;

        if (app.thread == null) {
            return false;
        }

        final boolean wasKeeping = app.keeping;

        boolean success = true;

        computeOomAdjLocked(app, hiddenAdj, clientHiddenAdj, emptyAdj, TOP_APP, false, doingAll);

        if (app.curRawAdj != app.setRawAdj) {
            if (wasKeeping && !app.keeping) {
                // This app is no longer something we want to keep.  Note
                // its current wake lock time to later know to kill it if
                // it is not behaving well.
                BatteryStatsImpl stats = mBatteryStatsService.getActiveStatistics();
                synchronized (stats) {
                    app.lastWakeTime = stats.getProcessWakeTime(app.info.uid,
                            app.pid, SystemClock.elapsedRealtime());
                }
                app.lastCpuTime = app.curCpuTime;
            }

            app.setRawAdj = app.curRawAdj;
        }

  		 <!--设置OomAdj-->
            
        if (app.curAdj != app.setAdj) {
            if (Process.setOomAdj(app.pid, app.curAdj)) {
                if (DEBUG_SWITCH || DEBUG_OOM_ADJ) Slog.v(
                    TAG, "Set " + app.pid + " " + app.processName +
                    " adj " + app.curAdj + ": " + app.adjType);
                app.setAdj = app.curAdj;
            } else {
                success = false;
                Slog.w(TAG, "Failed setting oom adj of " + app + " to " + app.curAdj);
            }
        }
        if (app.setSchedGroup != app.curSchedGroup) {
            app.setSchedGroup = app.curSchedGroup;
            if (DEBUG_SWITCH || DEBUG_OOM_ADJ) Slog.v(TAG,
                    "Setting process group of " + app.processName
                    + " to " + app.curSchedGroup);
            if (app.waitingToKill != null &&
                    app.setSchedGroup == Process.THREAD_GROUP_BG_NONINTERACTIVE) {
                Slog.i(TAG, "Killing " + app.toShortString() + ": " + app.waitingToKill);
                EventLog.writeEvent(EventLogTags.AM_KILL, app.userId, app.pid,
                        app.processName, app.setAdj, app.waitingToKill);
                app.killedBackground = true;
                
                <!--可以杀死-->
                
                Process.killProcessQuiet(app.pid);
                success = false;
            } else {
                if (true) {
                    long oldId = Binder.clearCallingIdentity();
                    try {
                        Process.setProcessGroup(app.pid, app.curSchedGroup);
                    } catch (Exception e) {
                        Slog.w(TAG, "Failed setting process group of " + app.pid
                                + " to " + app.curSchedGroup);
                        e.printStackTrace();
                    } finally {
                        Binder.restoreCallingIdentity(oldId);
                    }
                } else {
                    if (app.thread != null) {
                        try {
                            app.thread.setSchedulingGroup(app.curSchedGroup);
                        } catch (RemoteException e) {
                        }
                    }
                }
            }
        }
        return success;
    }
    
Process

    public static final native boolean setOomAdj(int pid, int amt);


android_util_Process.cpp

	jboolean android_os_Process_setOomAdj(JNIEnv* env, jobject clazz,
	                                      jint pid, jint adj)
	{
	#ifdef HAVE_OOM_ADJ
	    char text[64];
	    sprintf(text, "/proc/%d/oom_adj", pid);
	    int fd = open(text, O_WRONLY);
	    if (fd >= 0) {
	        sprintf(text, "%d", adj);
	        write(fd, text, strlen(text));
	        close(fd);
	    }
	    return true;
	#endif
	    return false;
	}
	
在Linux系统中，可以通过proc文件系统修改内核信息，这里就是动态更新进程的优先级oomAdj。

以上是4.3系统，

Android5.0之后的系统，AMS不再直接更新，而是通过LMKD服务来进行

	service lmkd /system/bin/lmkd
	    class core
	    critical
	    socket lmkd seqpacket 0660 system system
	    
该服务是直接通过socket进行进程通信。


AMS


    private final boolean updateOomAdjLocked(ProcessRecord app, int cachedAdj,
            ProcessRecord TOP_APP, boolean doingAll, long now) {
        if (app.thread == null) {
            return false;
        }

        computeOomAdjLocked(app, cachedAdj, TOP_APP, doingAll, now);

        return applyOomAdjLocked(app, doingAll, now, SystemClock.elapsedRealtime());
    }

    private final boolean applyOomAdjLocked(ProcessRecord app, boolean doingAll, long now,
            long nowElapsed) {
        boolean success = true;

        if (app.curRawAdj != app.setRawAdj) {
            app.setRawAdj = app.curRawAdj;
        }

        int changes = 0;

        if (app.curAdj != app.setAdj) {
            ProcessList.setOomAdj(app.pid, app.info.uid, app.curAdj);
            if (DEBUG_SWITCH || DEBUG_OOM_ADJ) Slog.v(TAG_OOM_ADJ,
                    "Set " + app.pid + " " + app.processName + " adj " + app.curAdj + ": "
                    + app.adjType);
            app.setAdj = app.curAdj;
            app.verifiedAdj = ProcessList.INVALID_ADJ;
        }
        
这里就是通过socket进行通信了

        
    public static final void setOomAdj(int pid, int uid, int amt) {
        if (amt == UNKNOWN_ADJ)
            return;

        long start = SystemClock.elapsedRealtime();
        ByteBuffer buf = ByteBuffer.allocate(4 * 4);
        buf.putInt(LMK_PROCPRIO);
        buf.putInt(pid);
        buf.putInt(uid);
        buf.putInt(amt);
        writeLmkd(buf);
        long now = SystemClock.elapsedRealtime();
        if ((now-start) > 250) {
            Slog.w("ActivityManager", "SLOW OOM ADJ: " + (now-start) + "ms for pid " + pid
                    + " = " + amt);
        }
    }    
    
来看看lmkd服务于socket

lmkd.c函数

	int main(int argc __unused, char **argv __unused) {
	    struct sched_param param = {
	            .sched_priority = 1,
	    };
	
	    mlockall(MCL_FUTURE);
	    sched_setscheduler(0, SCHED_FIFO, &param);
	    if (!init())
	        mainloop();
	
	    ALOGI("exiting");
	    return 0;
	}

这里就是通过mainloop，监听socket，有需求，就更新oomAdj
其更新机制还是通过proc文件系统

	static void cmd_procprio(int pid, int uid, int oomadj) {
	    struct proc *procp;
	    char path[80];
	    char val[20];
	
	    if (oomadj < OOM_DISABLE || oomadj > OOM_ADJUST_MAX) {
	        ALOGE("Invalid PROCPRIO oomadj argument %d", oomadj);
	        return;
	    }
	
	    snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
	    snprintf(val, sizeof(val), "%d", lowmem_oom_adj_to_oom_score_adj(oomadj));
	    writefilestring(path, val);
	
	    if (use_inkernel_interface)
	        return;
	
	    procp = pid_lookup(pid);
	    if (!procp) {
	            procp = malloc(sizeof(struct proc));
	            if (!procp) {
	                // Oh, the irony.  May need to rebuild our state.
	                return;
	            }
	
	            procp->pid = pid;
	            procp->uid = uid;
	            procp->oomadj = oomadj;
	            proc_insert(procp);
	    } else {
	        proc_unslot(procp);
	        procp->oomadj = oomadj;
	        proc_slot(procp);
	    }
	}


# LomemoryKiller内核部分

LomemoryKiller属于被动相应的一个模块，系统内存不足的时候，会调用LomemoryKiller将一些优先级较低的进程杀死。

	static int __init lowmem_init(void)
	{
		register_shrinker(&lowmem_shrinker);
		return 0;
	}
	
	static void __exit lowmem_exit(void)
	{
		unregister_shrinker(&lowmem_shrinker);
	}
	
一下是扫描，上面其实通过proc就已经改变了oomadj的值
	
	static int lowmem_shrink(int nr_to_scan, gfp_t gfp_mask)
	{
		struct task_struct *p;
		struct task_struct *selected = NULL;
		int rem = 0;
		int tasksize;
		int i;
		int min_adj = OOM_ADJUST_MAX + 1;
		int selected_tasksize = 0;
		int array_size = ARRAY_SIZE(lowmem_adj);
		int other_free = global_page_state(NR_FREE_PAGES);
		int other_file = global_page_state(NR_FILE_PAGES);
		if(lowmem_adj_size < array_size)
			array_size = lowmem_adj_size;
		if(lowmem_minfree_size < array_size)
			array_size = lowmem_minfree_size;
		for(i = 0; i < array_size; i++) {
			if (other_free < lowmem_minfree[i] &&
			    other_file < lowmem_minfree[i]) {
				min_adj = lowmem_adj[i];
				break;
			}
		}
		if(nr_to_scan > 0)
			lowmem_print(3, "lowmem_shrink %d, %x, ofree %d %d, ma %d\n", nr_to_scan, gfp_mask, other_free, other_file, min_adj);
		rem = global_page_state(NR_ACTIVE_ANON) +
			global_page_state(NR_ACTIVE_FILE) +
			global_page_state(NR_INACTIVE_ANON) +
			global_page_state(NR_INACTIVE_FILE);
		if (nr_to_scan <= 0 || min_adj == OOM_ADJUST_MAX + 1) {
			lowmem_print(5, "lowmem_shrink %d, %x, return %d\n", nr_to_scan, gfp_mask, rem);
			return rem;
		}
	
		read_lock(&tasklist_lock);
		for_each_process(p) {
			if (p->oomkilladj < min_adj || !p->mm)
				continue;
			tasksize = get_mm_rss(p->mm);
			if (tasksize <= 0)
				continue;
			if (selected) {
				if (p->oomkilladj < selected->oomkilladj)
					continue;
				if (p->oomkilladj == selected->oomkilladj &&
				    tasksize <= selected_tasksize)
					continue;
			}
			selected = p;
			selected_tasksize = tasksize;
			lowmem_print(2, "select %d (%s), adj %d, size %d, to kill\n",
			             p->pid, p->comm, p->oomkilladj, tasksize);
		}
		if(selected != NULL) {
			lowmem_print(1, "send sigkill to %d (%s), adj %d, size %d\n",
			             selected->pid, selected->comm,
			             selected->oomkilladj, selected_tasksize);
			force_sig(SIGKILL, selected);
			rem -= selected_tasksize;
		}
		lowmem_print(4, "lowmem_shrink %d, %x, return %d\n", nr_to_scan, gfp_mask, rem);
		read_unlock(&tasklist_lock);
		return rem;
	}

	
	
 

本篇是Android后台杀死系列的第二篇，主要讲解ActivityMangerService是如何恢复被后台杀死的进程的，在开篇[FragmentActivity及PhoneWindow后台杀死处理机制](http://www.jianshu.com/p/00fef8872b68)中，简述了后台杀死所引起的一些常见问题，还有Android系统控件对后台杀死所做的一些兼容，以及onSaveInstance跟onRestoreInstance的作用于执行时机，最后说了如何应对后台杀死，但是对于被后台杀死的进程如何恢复的并没有讲解，本篇不涉及后台杀死，比如LowmemoryKiller机制，只讲述被杀死的进程如何恢复的。假设，一个应用被后台杀死，再次从最近的任务列表唤起App时候，系统是如何处理的呢？有这么几个问题可能需要解决：

* 系统如何知道App被杀死了
* App被杀前的场景是如何保存的
* 系统如何恢复被杀的App
* 被后台杀死的App的启动流程跟普通的启动有什么区别
* Activity的恢复顺序为什么是倒序恢复


# 系统如何知道App被杀死了

首先来看第一个问题，系统如何知道Application被杀死了，Android使用了Linux的oomKiller机制，只是简单的做了个变种，采用分等级的LowmemoryKiller，但是这个其实是内核层面，LowmemoryKiller杀死进程后，不会像用户空间发送通知，也就是说即使是框架层的ActivityMangerService也无法知道App是否被杀死，但是，只有知道App或者Activity是否被杀死，AMS（ActivityMangerService）才能正确的走唤起流程，那么AMS究竟是在什么时候知道App或者Activity被后台杀死了呢？我们先看一下从最近的任务列表进行唤起的时候，究竟发生了什么。走到系统源码的systemUi包，里面有个RecentActivity，这个其实就是最近的任务列表的入口：      




# Application保存流程

* App被杀前的场景是如何保存的


## 新Activity启动跟旧Activity的保存
 

# 对于APP，所有的处理都是被动响应，Android是基于操作系统的被动式开发。

# 主动清楚最近的任务

# Activity的恢复流程 顺序

## 不保留活动

# 内核层面的杀死，框架层AMS是不知道的，只有在恢复的时候，才自己查询得到，并主导恢复流程


# startactivity总是会走realStartActivityLocked，但是恢复，就不走，恢复的实收是直接走resumeTopActivity，如果被杀死，抛出异常

![Binder访问已经被杀死的进程](http://upload-images.jianshu.io/upload_images/1460468-00df66d0bf4dec82.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# Activitymanagerservice 如何知道应用背后太杀死,RemoteException 在resume的时候，如果无法启动，那就说明是被杀死了 ,对方进程已经死了，那就无法通信了，那就死了



    final boolean resumeTopActivityLocked(ActivityRecord prev, Bundle options) {
      
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

                if (next.newIntents != null) {
                    next.app.thread.scheduleNewIntent(next.newIntents, next.appToken);
                }

                EventLog.writeEvent(EventLogTags.AM_RESUME_ACTIVITY,
                        next.userId, System.identityHashCode(next),
                        next.task.taskId, next.shortComponentName);
                
                next.sleeping = false;
                showAskCompatModeDialogLocked(next);
                next.app.pendingUiClean = true;
                next.app.thread.scheduleResumeActivity(next.appToken,
                        mService.isNextTransitionForward());
                
                checkReadyForSleepLocked();

            } catch (Exception e) {
                // Whoops, need to restart this activity!
                // 这里需要重启，难道被后台杀死，走的是异常分支吗？？？？ 异常杀死
                if (DEBUG_STATES) Slog.v(TAG, "Resume failed; resetting state to "
                        + lastState + ": " + next);
                next.state = lastState;
                mResumedActivity = lastResumedActivity;
                <!--确实这里是因为进程挂掉了-->
                Slog.i(TAG, "Restarting because process died: " + next);
                if (!next.hasBeenLaunched) {
                    next.hasBeenLaunched = true;
                } else {
                    if (SHOW_APP_STARTING_PREVIEW && mMainStack) {
                        mService.mWindowManager.setAppStartingWindow(
                                next.appToken, next.packageName, next.theme,
                                mService.compatibilityInfoForPackageLocked(
                                        next.info.applicationInfo),
                                next.nonLocalizedLabel,
                                next.labelRes, next.icon, next.windowFlags,
                                null, true);
                    }
                }
                startSpecificActivityLocked(next, true, false);
                return true;
            }
            ...
            }
    
    
    
    

Android开发经常会遇到这样的问题，App在后台久置之后，再次点击图标或从最近的任务列表打开时，App可能会崩溃。这种情况往往是App在后台被系统杀死，在恢复的时候遇到了问题，这种问题经常出现在FragmentActivity中，尤其是里面添加了Fragment的时候。开发时一直遵守谷歌的Android开发文档，创建Fragment尽量采用推荐的参数传递方式，并且保留默认的Fragment无参构造方法，避免绝大部分后台杀死-恢复崩溃的问题，但是对于原理的了解紧限于恢复时的重建机制，采用反射机制，并使用了默认的构造参数，直到使用FragmentDialog，还要后天杀死后，Dialog不会显示，但是FragmentDialog就可以恢复。其实App在后台待久了很可能被Android的LowMemoryKiller机制给杀掉，但是其现场是被AMS保存的，再次启动是时候，会通过AMS进行恢复。LowMemoryKiller机制在另一篇文章讲述。本文就Activity的保存，及恢复探讨一下Android后台杀死及恢复的机制。

![Activity Launch流程图.png](http://upload-images.jianshu.io/upload_images/1460468-c91b004975ed70c4.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# 最近的任务列表展示原理

# 点击Icon再次唤起原理

# 最近的任务列表唤起App

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
            if (DEBUG) Log.v(TAG, "Starting activity " + intent);
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
    
非主动退出的最近进程列表一般是ad.taskId >0,否则就是-1，

    public List<ActivityManager.RecentTaskInfo> getRecentTasks(int maxNum,
            int flags, int userId) {
     		 。。。
            IPackageManager pm = AppGlobals.getPackageManager();

            final int N = mRecentTasks.size();
            ArrayList<ActivityManager.RecentTaskInfo> res
                    = new ArrayList<ActivityManager.RecentTaskInfo>(
                            maxNum < N ? maxNum : N);
            for (int i=0; i<N && maxNum > 0; i++) {
                TaskRecord tr = mRecentTasks.get(i);
                // Only add calling user's recent tasks
 
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
                    
如果不是主动退出，tr.numActivities就大于0.


moveTaskToFront
    
# Application保存及恢复流程

## 新Activity启动跟旧Activity的保存

在Activity A 启动另一个Activity B 的时候，A可能会因为系统内存不足等原因被回收掉，如果不做任何处理，在B回退到A到时候就会遇到问题，AMS在B启动之前，会先保存好A的现场，如果A被杀死了，B回退的时候，AMS会根据保存的现场恢复A，如果A没有被杀死，A就不会再次走新建流程，AMS唤起A，并将保存的A的现场丢弃。

具体看代码感受一下，onStoreInstanceState执行时机，在startActivity的时候，

Activity.java

    public void startActivityForResult(Intent intent, int requestCode, Bundle options) {
        if (mParent == null) {
            Instrumentation.ActivityResult ar =
                mInstrumentation.execStartActivity(
                    this, mMainThread.getApplicationThread(), mToken, this,
                    intent, requestCode, options);
            if (ar != null) {
                mMainThread.sendActivityResult(
                    mToken, mEmbeddedID, requestCode, ar.getResultCode(),
                    ar.getResultData());
            }
            if (requestCode >= 0) {
                // If this start is requesting a result, we can avoid making
                // the activity visible until the result is received.  Setting
                // this code during onCreate(Bundle savedInstanceState) or onResume() will keep the
                // activity hidden during this time, to avoid flickering.
                // This can only be done when a result is requested because
                // that guarantees we will get information back when the
                // activity is finished, no matter what happens to it.
                mStartedActivity = true;
            }
        } else {
            if (options != null) {
                mParent.startActivityFromChild(this, intent, requestCode, options);
            } else {
                // Note we want to go through this method for compatibility with
                // existing applications that may have overridden it.
                mParent.startActivityFromChild(this, intent, requestCode);
            }
        }
    }
    
Instrumention.java
    
        public ActivityResult execStartActivity(
            Context who, IBinder contextThread, IBinder token, Activity target,
            Intent intent, int requestCode, Bundle options) {
        IApplicationThread whoThread = (IApplicationThread) contextThread;
        if (mActivityMonitors != null) {
            synchronized (mSync) {
                final int N = mActivityMonitors.size();
                for (int i=0; i<N; i++) {
                    final ActivityMonitor am = mActivityMonitors.get(i);
                    if (am.match(who, null, intent)) {
                        am.mHits++;
                        if (am.isBlocking()) {
                            return requestCode >= 0 ? am.getResult() : null;
                        }
                        break;
                    }
                }
            }
        }
        try {
            intent.migrateExtraStreamToClipData();
            intent.prepareToLeaveProcess();
            int result = ActivityManagerNative.getDefault()
                .startActivity(whoThread, who.getBasePackageName(), intent,
                        intent.resolveTypeIfNeeded(who.getContentResolver()),
                        token, target != null ? target.mEmbeddedID : null,
                        requestCode, 0, null, null, options);
            checkStartActivityResult(result, intent);
        } catch (RemoteException e) {
        }
        return null;
    }
    
AcrtivityManagerNative
	
	  public int startActivity(IApplicationThread caller, String callingPackage, Intent intent,
	            String resolvedType, IBinder resultTo, String resultWho, int requestCode,
	            int startFlags, String profileFile,
	            ParcelFileDescriptor profileFd, Bundle options) throws RemoteException {
	        Parcel data = Parcel.obtain();
	        Parcel reply = Parcel.obtain();
	        data.writeInterfaceToken(IActivityManager.descriptor);
	        data.writeStrongBinder(caller != null ? caller.asBinder() : null);
	        data.writeString(callingPackage);
	        intent.writeToParcel(data, 0);
	        data.writeString(resolvedType);
	        data.writeStrongBinder(resultTo);
	        data.writeString(resultWho);
	        data.writeInt(requestCode);
	        data.writeInt(startFlags);
	        data.writeString(profileFile);
	        if (profileFd != null) {
	            data.writeInt(1);
	            profileFd.writeToParcel(data, Parcelable.PARCELABLE_WRITE_RETURN_VALUE);
	        } else {
	            data.writeInt(0);
	        }
	        if (options != null) {
	            data.writeInt(1);
	            options.writeToParcel(data, 0);
	        } else {
	            data.writeInt(0);
	        }
	        mRemote.transact(START_ACTIVITY_TRANSACTION, data, reply, 0);
	        reply.readException();
	        int result = reply.readInt();
	        reply.recycle();
	        data.recycle();
	        return result;
	    }
	        
ActivityManagerService    
    
        public final int startActivity(IApplicationThread caller, String callingPackage,
            Intent intent, String resolvedType, IBinder resultTo,
            String resultWho, int requestCode, int startFlags,
            String profileFile, ParcelFileDescriptor profileFd, Bundle options) {
        return startActivityAsUser(caller, callingPackage, intent, resolvedType, resultTo,
                resultWho, requestCode,
                startFlags, profileFile, profileFd, options, UserHandle.getCallingUserId());
    }
    

    
        public final int startActivityAsUser(IApplicationThread caller, String callingPackage,
            Intent intent, String resolvedType, IBinder resultTo,
            String resultWho, int requestCode, int startFlags,
            String profileFile, ParcelFileDescriptor profileFd, Bundle options, int userId) {
        enforceNotIsolatedCaller("startActivity");
        userId = handleIncomingUser(Binder.getCallingPid(), Binder.getCallingUid(), userId,
                false, true, "startActivity", null);
        return mMainStack.startActivityMayWait(caller, -1, callingPackage, intent, resolvedType,
                resultTo, resultWho, requestCode, startFlags, profileFile, profileFd,
                null, null, options, userId);
    }
    
ActivityStack

    final int startActivityMayWait(IApplicationThread caller, int callingUid,
         
            
            int res = startActivityLocked(caller, intent, resolvedType,
                    aInfo, resultTo, resultWho, requestCode, callingPid, callingUid,
                    callingPackage, startFlags, options, componentSpecified, null);
            
         。。。
    } 
    

<!--启动新的APP，或者新Activity，或者唤醒-->

    private final void startActivityLocked(ActivityRecord r, boolean newTask,
            boolean doResume, boolean keepCurTransition, Bundle options) {
        final int NH = mHistory.size();

        int addPos = -1;
        
        if (!newTask) {
            // If starting in an existing task, find where that is...
            boolean startIt = true;
            for (int i = NH-1; i >= 0; i--) {
                ActivityRecord p = mHistory.get(i);
                if (p.finishing) {
                    continue;
                }
                if (p.task == r.task) {
                    // Here it is!  Now, if this is not yet visible to the
                    // user, then just add it without starting; it will
                    // get started when the user navigates back to it.
                    addPos = i+1;
                    if (!startIt) {
                        if (DEBUG_ADD_REMOVE) {
                            RuntimeException here = new RuntimeException("here");
                            here.fillInStackTrace();
                            Slog.i(TAG, "Adding activity " + r + " to stack at " + addPos,
                                    here);
                        }
                        mHistory.add(addPos, r);
                        r.putInHistory();
                        mService.mWindowManager.addAppToken(addPos, r.appToken, r.task.taskId,
                                r.info.screenOrientation, r.fullscreen,
                                (r.info.flags & ActivityInfo.FLAG_SHOW_ON_LOCK_SCREEN) != 0);
                        if (VALIDATE_TOKENS) {
                            validateAppTokensLocked();
                        }
                        ActivityOptions.abort(options);
                        return;
                    }
                    break;
                }
                if (p.fullscreen) {
                    startIt = false;
                }
            }
        }

        // Place a new activity at top of stack, so it is next to interact
        // with the user.
        if (addPos < 0) {
            addPos = NH;
        }
        
        // If we are not placing the new activity frontmost, we do not want
        // to deliver the onUserLeaving callback to the actual frontmost
        // activity
        if (addPos < NH) {
            mUserLeaving = false;
            if (DEBUG_USER_LEAVING) Slog.v(TAG, "startActivity() behind front, mUserLeaving=false");
        }
        
        // Slot the activity into the history stack and proceed
        if (DEBUG_ADD_REMOVE) {
            RuntimeException here = new RuntimeException("here");
            here.fillInStackTrace();
            Slog.i(TAG, "Adding activity " + r + " to stack at " + addPos, here);
        }
        mHistory.add(addPos, r);
        r.putInHistory();
        r.frontOfTask = newTask;
        if (NH > 0) {
            // We want to show the starting preview window if we are
            // switching to a new task, or the next activity's process is
            // not currently running.
            boolean showStartingIcon = newTask;
            ProcessRecord proc = r.app;
            if (proc == null) {
                proc = mService.mProcessNames.get(r.processName, r.info.applicationInfo.uid);
            }
            if (proc == null || proc.thread == null) {
                showStartingIcon = true;
            }
            if (DEBUG_TRANSITION) Slog.v(TAG,
                    "Prepare open transition: starting " + r);
            if ((r.intent.getFlags()&Intent.FLAG_ACTIVITY_NO_ANIMATION) != 0) {
                mService.mWindowManager.prepareAppTransition(
                        AppTransition.TRANSIT_NONE, keepCurTransition);
                mNoAnimActivities.add(r);
            } else {
                mService.mWindowManager.prepareAppTransition(newTask
                        ? AppTransition.TRANSIT_TASK_OPEN
                        : AppTransition.TRANSIT_ACTIVITY_OPEN, keepCurTransition);
                mNoAnimActivities.remove(r);
            }
            r.updateOptionsLocked(options);
            mService.mWindowManager.addAppToken(
                    addPos, r.appToken, r.task.taskId, r.info.screenOrientation, r.fullscreen,
                    (r.info.flags & ActivityInfo.FLAG_SHOW_ON_LOCK_SCREEN) != 0);
            boolean doShow = true;
            if (newTask) {
                // Even though this activity is starting fresh, we still need
                // to reset it to make sure we apply affinities to move any
                // existing activities from other tasks in to it.
                // If the caller has requested that the target task be
                // reset, then do so.
                if ((r.intent.getFlags()
                        &Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED) != 0) {
                    resetTaskIfNeededLocked(r, r);
                    doShow = topRunningNonDelayedActivityLocked(null) == r;
                }
            }
            if (SHOW_APP_STARTING_PREVIEW && doShow) {
                // Figure out if we are transitioning from another activity that is
                // "has the same starting icon" as the next one.  This allows the
                // window manager to keep the previous window it had previously
                // created, if it still had one.
                ActivityRecord prev = mResumedActivity;
                if (prev != null) {
                    // We don't want to reuse the previous starting preview if:
                    // (1) The current activity is in a different task.
                    if (prev.task != r.task) prev = null;
                    // (2) The current activity is already displayed.
                    else if (prev.nowVisible) prev = null;
                }
                mService.mWindowManager.setAppStartingWindow(
                        r.appToken, r.packageName, r.theme,
                        mService.compatibilityInfoForPackageLocked(
                                r.info.applicationInfo), r.nonLocalizedLabel,
                        r.labelRes, r.icon, r.windowFlags,
                        prev != null ? prev.appToken : null, showStartingIcon);
            }
        } else {
            // If this is the first activity, don't do any fancy animations,
            // because there is nothing for it to animate on top of.
            mService.mWindowManager.addAppToken(addPos, r.appToken, r.task.taskId,
                    r.info.screenOrientation, r.fullscreen,
                    (r.info.flags & ActivityInfo.FLAG_SHOW_ON_LOCK_SCREEN) != 0);
            ActivityOptions.abort(options);
        }
        if (VALIDATE_TOKENS) {
            validateAppTokensLocked();
        }

        if (doResume) {
            resumeTopActivityLocked(null);
        }
    }

    final void validateAppTokensLocked() {
        mValidateAppTokens.clear();
        mValidateAppTokens.ensureCapacity(mHistory.size());
        for (int i=0; i<mHistory.size(); i++) {
            mValidateAppTokens.add(mHistory.get(i).appToken);
        }
        mService.mWindowManager.validateAppTokens(mValidateAppTokens);
    }
  
  ActivityStack  
    
      final boolean resumeTopActivityLocked(ActivityRecord prev, Bundle options) {
        // Find the first activity that is not finishing.
        ActivityRecord next = topRunningActivityLocked(null);

        // Remember how we'll process this pause/resume situation, and ensure
        // that the state is reset however we wind up proceeding.
        final boolean userLeaving = mUserLeaving;
        mUserLeaving = false;

        if (next == null) {
            // There are no more activities!  Let's just start up the
            // Launcher...
            if (mMainStack) {
                ActivityOptions.abort(options);
                return mService.startHomeActivityLocked(mCurrentUser);
            }
        }

        next.delayedResume = false;
        
        // If the top activity is the resumed one, nothing to do.
        if (mResumedActivity == next && next.state == ActivityState.RESUMED) {
            // Make sure we have executed any pending transitions, since there
            // should be nothing left to do at this point.
            mService.mWindowManager.executeAppTransition();
            mNoAnimActivities.clear();
            ActivityOptions.abort(options);
            return false;
        }

        // If we are sleeping, and there is no resumed activity, and the top
        // activity is paused, well that is the state we want.
        if ((mService.mSleeping || mService.mShuttingDown)
                && mLastPausedActivity == next
                && (next.state == ActivityState.PAUSED
                    || next.state == ActivityState.STOPPED
                    || next.state == ActivityState.STOPPING)) {
            // Make sure we have executed any pending transitions, since there
            // should be nothing left to do at this point.
            mService.mWindowManager.executeAppTransition();
            mNoAnimActivities.clear();
            ActivityOptions.abort(options);
            return false;
        }

        // Make sure that the user who owns this activity is started.  If not,
        // we will just leave it as is because someone should be bringing
        // another user's activities to the top of the stack.
        if (mService.mStartedUsers.get(next.userId) == null) {
            Slog.w(TAG, "Skipping resume of top activity " + next
                    + ": user " + next.userId + " is stopped");
            return false;
        }

        // The activity may be waiting for stop, but that is no longer
        // appropriate for it.
        mStoppingActivities.remove(next);
        mGoingToSleepActivities.remove(next);
        next.sleeping = false;
        mWaitingVisibleActivities.remove(next);

        next.updateOptionsLocked(options);

        if (DEBUG_SWITCH) Slog.v(TAG, "Resuming " + next);

        // If we are currently pausing an activity, then don't do anything
        // until that is done.
        if (mPausingActivity != null) {
            if (DEBUG_SWITCH || DEBUG_PAUSE) Slog.v(TAG,
                    "Skip resume: pausing=" + mPausingActivity);
            return false;
        }

        // Okay we are now going to start a switch, to 'next'.  We may first
        // have to pause the current activity, but this is an important point
        // where we have decided to go to 'next' so keep track of that.
        // XXX "App Redirected" dialog is getting too many false positives
        // at this point, so turn off for now.
        if (false) {
            if (mLastStartedActivity != null && !mLastStartedActivity.finishing) {
                long now = SystemClock.uptimeMillis();
                final boolean inTime = mLastStartedActivity.startTime != 0
                        && (mLastStartedActivity.startTime + START_WARN_TIME) >= now;
                final int lastUid = mLastStartedActivity.info.applicationInfo.uid;
                final int nextUid = next.info.applicationInfo.uid;
                if (inTime && lastUid != nextUid
                        && lastUid != next.launchedFromUid
                        && mService.checkPermission(
                                android.Manifest.permission.STOP_APP_SWITCHES,
                                -1, next.launchedFromUid)
                        != PackageManager.PERMISSION_GRANTED) {
                    mService.showLaunchWarningLocked(mLastStartedActivity, next);
                } else {
                    next.startTime = now;
                    mLastStartedActivity = next;
                }
            } else {
                next.startTime = SystemClock.uptimeMillis();
                mLastStartedActivity = next;
            }
        }
        
        <!--必须将当前Resume的Activity设置为pause 然后stop才能继续-->
        
        // We need to start pausing the current activity so the top one
        // can be resumed...
        if (mResumedActivity != null) {
            if (DEBUG_SWITCH) Slog.v(TAG, "Skip resume: need to start pausing");
            // At this point we want to put the upcoming activity's process
            // at the top of the LRU list, since we know we will be needing it
            // very soon and it would be a waste to let it get killed if it
            // happens to be sitting towards the end.
            if (next.app != null && next.app.thread != null) {
                // No reason to do full oom adj update here; we'll let that
                // happen whenever it needs to later.
                mService.updateLruProcessLocked(next.app, false);
            }
            startPausingLocked(userLeaving, false);
            return true;
        }
   
 暂停resume activity  
   
    private final void startPausingLocked(boolean userLeaving, boolean uiSleeping) {
        if (mPausingActivity != null) {
            RuntimeException e = new RuntimeException();
            Slog.e(TAG, "Trying to pause when pause is already pending for "
                  + mPausingActivity, e);
        }
        ActivityRecord prev = mResumedActivity;
        if (prev == null) {
            RuntimeException e = new RuntimeException();
            Slog.e(TAG, "Trying to pause when nothing is resumed", e);
            resumeTopActivityLocked(null);
            return;
        }
        if (DEBUG_STATES) Slog.v(TAG, "Moving to PAUSING: " + prev);
        else if (DEBUG_PAUSE) Slog.v(TAG, "Start pausing: " + prev);
        mResumedActivity = null;
        mPausingActivity = prev;
        mLastPausedActivity = prev;
        prev.state = ActivityState.PAUSING;
        prev.task.touchActiveTime();
        prev.updateThumbnail(screenshotActivities(prev), null);

        mService.updateCpuStats();
        
        if (prev.app != null && prev.app.thread != null) {
            if (DEBUG_PAUSE) Slog.v(TAG, "Enqueueing pending pause: " + prev);
            try {
                EventLog.writeEvent(EventLogTags.AM_PAUSE_ACTIVITY,
                        prev.userId, System.identityHashCode(prev),
                        prev.shortComponentName);
                prev.app.thread.schedulePauseActivity(prev.appToken, prev.finishing,
                        userLeaving, prev.configChangeFlags);
                if (mMainStack) {
                    mService.updateUsageStats(prev, false);
                }
            } catch (Exception e) {
                // Ignore exception, if process died other code will cleanup.
                Slog.w(TAG, "Exception thrown during pause", e);
                mPausingActivity = null;
                mLastPausedActivity = null;
            }
        } else {
            mPausingActivity = null;
            mLastPausedActivity = null;
        }

        // If we are not going to sleep, we want to ensure the device is
        // awake until the next activity is started.
        if (!mService.mSleeping && !mService.mShuttingDown) {
            mLaunchingActivity.acquire();
            if (!mHandler.hasMessages(LAUNCH_TIMEOUT_MSG)) {
                // To be safe, don't allow the wake lock to be held for too long.
                Message msg = mHandler.obtainMessage(LAUNCH_TIMEOUT_MSG);
                mHandler.sendMessageDelayed(msg, LAUNCH_TIMEOUT);
            }
        }


        if (mPausingActivity != null) {
            // Have the window manager pause its key dispatching until the new
            // activity has started.  If we're pausing the activity just because
            // the screen is being turned off and the UI is sleeping, don't interrupt
            // key dispatch; the same activity will pick it up again on wakeup.
            if (!uiSleeping) {
                prev.pauseKeyDispatchingLocked();
            } else {
                if (DEBUG_PAUSE) Slog.v(TAG, "Key dispatch not paused for screen off");
            }

            // Schedule a pause timeout in case the app doesn't respond.
            // We don't give it much time because this directly impacts the
            // responsiveness seen by the user.
            Message msg = mHandler.obtainMessage(PAUSE_TIMEOUT_MSG);
            msg.obj = prev;
            prev.pauseTime = SystemClock.uptimeMillis();
            mHandler.sendMessageDelayed(msg, PAUSE_TIMEOUT);
            if (DEBUG_PAUSE) Slog.v(TAG, "Waiting for pause to complete...");
        } else {
            // This activity failed to schedule the
            // pause, so just treat it as being paused now.
            if (DEBUG_PAUSE) Slog.v(TAG, "Activity not running, resuming next.");
            resumeTopActivityLocked(null);
        }
    }   
    
   ApplicationThreadProxy.schedulePauseActivity
 
 ActivityThread  
	   
	   private void handlePauseActivity(IBinder token, boolean finished,
	            boolean userLeaving, int configChanges) {
	        ActivityClientRecord r = mActivities.get(token);
	        if (r != null) {
	            //Slog.v(TAG, "userLeaving=" + userLeaving + " handling pause of " + r);
	            if (userLeaving) {
	                performUserLeavingActivity(r);
	            }
	
	            r.activity.mConfigChangeFlags |= configChanges;
	            
	            performPauseActivity(token, finished, r.isPreHoneycomb());
	
	            // Make sure any pending writes are now committed.
	            if (r.isPreHoneycomb()) {
	                QueuedWork.waitToFinish();
	            }
	
	            // Tell the activity manager we have paused.
	            try {
	                ActivityManagerNative.getDefault().activityPaused(token);
	            } catch (RemoteException ex) {
	            }
	        }
	    }
  
performPauseActivity(token, finished, r.isPreHoneycomb());之类其实是2.3之前的在执行pause的时候，是否保存村现场。  执行完毕，还要通知AMS，执行结束，

ActivityManagerService

    public final void activityPaused(IBinder token) {
        final long origId = Binder.clearCallingIdentity();
        mMainStack.activityPaused(token, false);
        Binder.restoreCallingIdentity(origId);
    }

ActivityStack
  
    final void activityPaused(IBinder token, boolean timeout) {
        if (DEBUG_PAUSE) Slog.v(
            TAG, "Activity paused: token=" + token + ", timeout=" + timeout);

        ActivityRecord r = null;

        synchronized (mService) {
            int index = indexOfTokenLocked(token);
            if (index >= 0) {
                r = mHistory.get(index);
                mHandler.removeMessages(PAUSE_TIMEOUT_MSG, r);
                if (mPausingActivity == r) {
                    if (DEBUG_STATES) Slog.v(TAG, "Moving to PAUSED: " + r
                            + (timeout ? " (due to timeout)" : " (pause complete)"));
                    r.state = ActivityState.PAUSED;
                    completePauseLocked();
                } else {
                    EventLog.writeEvent(EventLogTags.AM_FAILED_TO_PAUSE,
                            r.userId, System.identityHashCode(r), r.shortComponentName, 
                            mPausingActivity != null
                                ? mPausingActivity.shortComponentName : "(none)");
                }
            }
        }
    }
    
     private final void completePauseLocked() {
        ActivityRecord prev = mPausingActivity;
        if (DEBUG_PAUSE) Slog.v(TAG, "Complete pause: " + prev);
        
        if (prev != null) {
            if (prev.finishing) {
                if (DEBUG_PAUSE) Slog.v(TAG, "Executing finish of activity: " + prev);
                prev = finishCurrentActivityLocked(prev, FINISH_AFTER_VISIBLE, false);
            } else if (prev.app != null) {
                if (DEBUG_PAUSE) Slog.v(TAG, "Enqueueing pending stop: " + prev);
                if (prev.waitingVisible) {
                    prev.waitingVisible = false;
                    mWaitingVisibleActivities.remove(prev);
                    if (DEBUG_SWITCH || DEBUG_PAUSE) Slog.v(
                            TAG, "Complete pause, no longer waiting: " + prev);
                }
                if (prev.configDestroy) {
                    // The previous is being paused because the configuration
                    // is changing, which means it is actually stopping...
                    // To juggle the fact that we are also starting a new
                    // instance right now, we need to first completely stop
                    // the current instance before starting the new one.
                    if (DEBUG_PAUSE) Slog.v(TAG, "Destroying after pause: " + prev);
                    destroyActivityLocked(prev, true, false, "pause-config");
                } else {
                    mStoppingActivities.add(prev);
                    if (mStoppingActivities.size() > 3) {
                        // If we already have a few activities waiting to stop,
                        // then give up on things going idle and start clearing
                        // them out.
                        if (DEBUG_PAUSE) Slog.v(TAG, "To many pending stops, forcing idle");
                        scheduleIdleLocked();
                    } else {
                        checkReadyForSleepLocked();
                    }
                }
            } else {
                if (DEBUG_PAUSE) Slog.v(TAG, "App died during pause, not stopping: " + prev);
                prev = null;
            }
            mPausingActivity = null;
        }

        if (!mService.isSleeping()) {
            resumeTopActivityLocked(prev);
        } else {
            checkReadyForSleepLocked();
            ActivityRecord top = topRunningActivityLocked(null);
            if (top == null || (prev != null && top != prev)) {
                // If there are no more activities available to run,
                // do resume anyway to start something.  Also if the top
                // activity on the stack is not the just paused activity,
                // we need to go ahead and resume it to ensure we complete
                // an in-flight app switch.
                resumeTopActivityLocked(null);
            }
        }
        
        if (prev != null) {
            prev.resumeKeyDispatchingLocked();
        }

        if (prev.app != null && prev.cpuTimeAtResume > 0
                && mService.mBatteryStatsService.isOnBattery()) {
            long diff = 0;
            synchronized (mService.mProcessStatsThread) {
                diff = mService.mProcessStats.getCpuTimeForPid(prev.app.pid)
                        - prev.cpuTimeAtResume;
            }
            if (diff > 0) {
                BatteryStatsImpl bsi = mService.mBatteryStatsService.getActiveStatistics();
                synchronized (bsi) {
                    BatteryStatsImpl.Uid.Proc ps =
                            bsi.getProcessStatsLocked(prev.info.applicationInfo.uid,
                            prev.info.packageName);
                    if (ps != null) {
                        ps.addForegroundTimeLocked(diff);
                    }
                }
            }
        }
        prev.cpuTimeAtResume = 0; // reset it
    }   



    private final void stopActivityLocked(ActivityRecord r) {
        if (DEBUG_SWITCH) Slog.d(TAG, "Stopping: " + r);
        if ((r.intent.getFlags()&Intent.FLAG_ACTIVITY_NO_HISTORY) != 0
                || (r.info.flags&ActivityInfo.FLAG_NO_HISTORY) != 0) {
            if (!r.finishing) {
                if (!mService.mSleeping) {
                    if (DEBUG_STATES) {
                        Slog.d(TAG, "no-history finish of " + r);
                    }
                    requestFinishActivityLocked(r.appToken, Activity.RESULT_CANCELED, null,
                            "no-history", false);
                } else {
                    if (DEBUG_STATES) Slog.d(TAG, "Not finishing noHistory " + r
                            + " on stop because we're just sleeping");
                }
            }
        }

        if (r.app != null && r.app.thread != null) {
            if (mMainStack) {
                if (mService.mFocusedActivity == r) {
                    mService.setFocusedActivityLocked(topRunningActivityLocked(null));
                }
            }
            r.resumeKeyDispatchingLocked();
            try {
                r.stopped = false;
                if (DEBUG_STATES) Slog.v(TAG, "Moving to STOPPING: " + r
                        + " (stop requested)");
                r.state = ActivityState.STOPPING;
                if (DEBUG_VISBILITY) Slog.v(
                        TAG, "Stopping visible=" + r.visible + " for " + r);
                if (!r.visible) {
                    mService.mWindowManager.setAppVisibility(r.appToken, false);
                }
                r.app.thread.scheduleStopActivity(r.appToken, r.visible, r.configChangeFlags);
                if (mService.isSleeping()) {
                    r.setSleeping(true);
                }
                Message msg = mHandler.obtainMessage(STOP_TIMEOUT_MSG);
                msg.obj = r;
                mHandler.sendMessageDelayed(msg, STOP_TIMEOUT);
            } catch (Exception e) {
                // Maybe just ignore exceptions here...  if the process
                // has crashed, our death notification will clean things
                // up.
                Slog.w(TAG, "Exception thrown during pause", e);
                // Just in case, assume it to be stopped.
                r.stopped = true;
                if (DEBUG_STATES) Slog.v(TAG, "Stop failed; moving to STOPPED: " + r);
                r.state = ActivityState.STOPPED;
                if (r.configDestroy) {
                    destroyActivityLocked(r, true, false, "stop-except");
                }
            }
        }
    }

继续回到ActivityThread 调用

    private void handleStopActivity(IBinder token, boolean show, int configChanges) {
        ActivityClientRecord r = mActivities.get(token);
        r.activity.mConfigChangeFlags |= configChanges;

        StopInfo info = new StopInfo();
        performStopActivityInner(r, info, show, true);

        if (localLOGV) Slog.v(
            TAG, "Finishing stop of " + r + ": show=" + show
            + " win=" + r.window);

        updateVisibility(r, show);

        // Make sure any pending writes are now committed.
        if (!r.isPreHoneycomb()) {
            QueuedWork.waitToFinish();
        }

        // Schedule the call to tell the activity manager we have
        // stopped.  We don't do this immediately, because we want to
        // have a chance for any other pending work (in particular memory
        // trim requests) to complete before you tell the activity
        // manager to proceed and allow us to go fully into the background.
        info.activity = r;
        info.state = r.state;
        mH.post(info);
    }    
    
保存现场    
    
        private void performStopActivityInner(ActivityClientRecord r,
            StopInfo info, boolean keepShown, boolean saveState) {
        if (localLOGV) Slog.v(TAG, "Performing stop of " + r);
        Bundle state = null;
        if (r != null) {
            if (!keepShown && r.stopped) {
                if (r.activity.mFinished) {
                    // If we are finishing, we won't call onResume() in certain
                    // cases.  So here we likewise don't want to call onStop()
                    // if the activity isn't resumed.
                    return;
                }
                RuntimeException e = new RuntimeException(
                        "Performing stop of activity that is not resumed: "
                        + r.intent.getComponent().toShortString());
                Slog.e(TAG, e.getMessage(), e);
            }

            if (info != null) {
                try {
                    // First create a thumbnail for the activity...
                    // For now, don't create the thumbnail here; we are
                    // doing that by doing a screen snapshot.
                    info.thumbnail = null; //createThumbnailBitmap(r);
                    info.description = r.activity.onCreateDescription();
                } catch (Exception e) {
                    if (!mInstrumentation.onException(r.activity, e)) {
                        throw new RuntimeException(
                                "Unable to save state of activity "
                                + r.intent.getComponent().toShortString()
                                + ": " + e.toString(), e);
                    }
                }
            }

            // Next have the activity save its current state and managed dialogs...
            if (!r.activity.mFinished && saveState) {
                if (r.state == null) {
                    state = new Bundle();
                    state.setAllowFds(false);
                    mInstrumentation.callActivityOnSaveInstanceState(r.activity, state);
                    r.state = state;
                } else {
    
之后会实行 ActivityManagerNative.getDefault().activityStopped，通知AMS，还会将保存的现场数据带过去。
  
  
    private static class StopInfo implements Runnable {
        ActivityClientRecord activity;
        Bundle state;
        Bitmap thumbnail;
        CharSequence description;

        @Override public void run() {
            // Tell activity manager we have been stopped.
            try {
                if (DEBUG_MEMORY_TRIM) Slog.v(TAG, "Reporting activity stopped: " + activity);
                ActivityManagerNative.getDefault().activityStopped(
                    activity.token, state, thumbnail, description);
            } catch (RemoteException ex) {
            }
        }
    }
    
接下来就会启动新Activity，或者启动新的Application

以上就是startactivity，中save现场的逻辑下面来看下恢复的逻辑


# Activity或者Application恢复流程

## Application没有被后台杀死

返回键或者finish返回上一个Activity

### 上一个Activity没被后台杀死


AMS

    public final boolean finishActivity(IBinder token, int resultCode, Intent resultData) {
        // Refuse possible leaked file descriptors
        if (resultData != null && resultData.hasFileDescriptors() == true) {
            throw new IllegalArgumentException("File descriptors passed in Intent");
        }

        synchronized(this) {
            if (mController != null) {
                // Find the first activity that is not finishing.
                ActivityRecord next = mMainStack.topRunningActivityLocked(token, 0);
                if (next != null) {
                    // ask watcher if this is allowed
                    boolean resumeOK = true;
                    try {
                        resumeOK = mController.activityResuming(next.packageName);
                    } catch (RemoteException e) {
                        mController = null;
                        Watchdog.getInstance().setActivityController(null);
                    }
    
                    if (!resumeOK) {
                        return false;
                    }
                }
            }
            final long origId = Binder.clearCallingIdentity();
            boolean res = mMainStack.requestFinishActivityLocked(token, resultCode,
                    resultData, "app-request", true);
            Binder.restoreCallingIdentity(origId);
            return res;
        }
    }
    
ActivityStack

    final boolean finishActivityLocked(ActivityRecord r, int index, int resultCode,
            Intent resultData, String reason, boolean immediate, boolean oomAdj) {
        if (r.finishing) {
            Slog.w(TAG, "Duplicate finish request for " + r);
            return false;
        }

        r.makeFinishing();
        EventLog.writeEvent(EventLogTags.AM_FINISH_ACTIVITY,
                r.userId, System.identityHashCode(r),
                r.task.taskId, r.shortComponentName, reason);
        if (index < (mHistory.size()-1)) {
            ActivityRecord next = mHistory.get(index+1);
            if (next.task == r.task) {
                if (r.frontOfTask) {
                    // The next activity is now the front of the task.
                    next.frontOfTask = true;
                }
                if ((r.intent.getFlags()&Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET) != 0) {
                    // If the caller asked that this activity (and all above it)
                    // be cleared when the task is reset, don't lose that information,
                    // but propagate it up to the next activity.
                    next.intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET);
                }
            }
        }

        r.pauseKeyDispatchingLocked();
        if (mMainStack) {
            if (mService.mFocusedActivity == r) {
                mService.setFocusedActivityLocked(topRunningActivityLocked(null));
            }
        }

        finishActivityResultsLocked(r, resultCode, resultData);
        
        if (mService.mPendingThumbnails.size() > 0) {
            // There are clients waiting to receive thumbnails so, in case
            // this is an activity that someone is waiting for, add it
            // to the pending list so we can correctly update the clients.
            mService.mCancelledThumbnails.add(r);
        }

        if (immediate) {
            return finishCurrentActivityLocked(r, index,
                    FINISH_IMMEDIATELY, oomAdj) == null;
        } else if (mResumedActivity == r) {
            boolean endTask = index <= 0
                    || (mHistory.get(index-1)).task != r.task;
            if (DEBUG_TRANSITION) Slog.v(TAG,
                    "Prepare close transition: finishing " + r);
            mService.mWindowManager.prepareAppTransition(endTask
                    ? AppTransition.TRANSIT_TASK_CLOSE
                    : AppTransition.TRANSIT_ACTIVITY_CLOSE, false);
    
            // Tell window manager to prepare for this one to be removed.
            mService.mWindowManager.setAppVisibility(r.appToken, false);
                
            if (mPausingActivity == null) {
                if (DEBUG_PAUSE) Slog.v(TAG, "Finish needs to pause: " + r);
                if (DEBUG_USER_LEAVING) Slog.v(TAG, "finish() => pause with userLeaving=false");
                startPausingLocked(false, false);
            }

        } else if (r.state != ActivityState.PAUSING) {
            // If the activity is PAUSING, we will complete the finish once
            // it is done pausing; else we can just directly finish it here.
            if (DEBUG_PAUSE) Slog.v(TAG, "Finish not pausing: " + r);
            return finishCurrentActivityLocked(r, index,
                    FINISH_AFTER_PAUSE, oomAdj) == null;
        } else {
            if (DEBUG_PAUSE) Slog.v(TAG, "Finish waiting for pause of: " + r);
        }

        return false;
    }

ActivityStack    
    
        private final ActivityRecord finishCurrentActivityLocked(ActivityRecord r,
            int index, int mode, boolean oomAdj) {
        // First things first: if this activity is currently visible,
        // and the resumed activity is not yet visible, then hold off on
        // finishing until the resumed one becomes visible.
        if (mode == FINISH_AFTER_VISIBLE && r.nowVisible) {
            if (!mStoppingActivities.contains(r)) {
                mStoppingActivities.add(r);
                if (mStoppingActivities.size() > 3) {
                    // If we already have a few activities waiting to stop,
                    // then give up on things going idle and start clearing
                    // them out.
                    scheduleIdleLocked();
                } else {
                    checkReadyForSleepLocked();
                }
            }
            if (DEBUG_STATES) Slog.v(TAG, "Moving to STOPPING: " + r
                    + " (finish requested)");
            r.state = ActivityState.STOPPING;
            if (oomAdj) {
                mService.updateOomAdjLocked();
            }
            return r;
        }

        // make sure the record is cleaned out of other places.
        mStoppingActivities.remove(r);
        mGoingToSleepActivities.remove(r);
        mWaitingVisibleActivities.remove(r);
        if (mResumedActivity == r) {
            mResumedActivity = null;
        }
        final ActivityState prevState = r.state;
        if (DEBUG_STATES) Slog.v(TAG, "Moving to FINISHING: " + r);
        r.state = ActivityState.FINISHING;

        if (mode == FINISH_IMMEDIATELY
                || prevState == ActivityState.STOPPED
                || prevState == ActivityState.INITIALIZING) {
            // If this activity is already stopped, we can just finish
            // it right now.
            boolean activityRemoved = destroyActivityLocked(r, true,
                    oomAdj, "finish-imm");
            if (activityRemoved) {
                resumeTopActivityLocked(null);
            }
            return activityRemoved ? null : r;
        } else {
            // Need to go through the full pause cycle to get this
            // activity into the stopped state and then finish it.
            if (localLOGV) Slog.v(TAG, "Enqueueing pending finish: " + r);
            mFinishingActivities.add(r);
            resumeTopActivityLocked(null);
        }
        return r;
    }
        

### Activity被后台杀死，（比如在开发者模式打开不保留活动）


## Application被后台杀死

        
## Fragment无参构造函数的影响 

# 对于APP，所有的处理都是被动响应，Android是基于操作系统的被动式开发。

# 主动清楚最近的任务不恢复，异常杀死恢复，但是时间过长也不会恢复？？？存几天（手机重启）

# 进程保活
# 冷热启动

![删除最近的任务.png](http://upload-images.jianshu.io/upload_images/1460468-436339b7fc278e2d.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


# 后台杀死原理：Application进程被Kill，但现场被AMS保存，AMS根据保存现场恢复Application

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
        
# onSaveInstanceState()的调用时机，都是在onPause或者onStop之前，Android Honeycomb之前之后，之前onPause，之后onStop，但是对于按返回键的怎么处理呢

	The reason why these slight inconsistencies exist stems from a significant change to the Activity lifecycle that was made in Honeycomb. Prior to Honeycomb, activities were not considered killable until after they had been paused, meaning that onSaveInstanceState() was called immediately before onPause(). Beginning with Honeycomb, however, Activities are considered to be killable only after they have been stopped, meaning that onSaveInstanceState() will now be called before onStop() instead of immediately before onPause(). These differences are summarized in the table below:


# 但是如何判断是否被销毁，如何知道从oncreate还是从onresume开始 

其实这个交给AMS来完成，ActivityManagerService首先会去除ActivityRecord，然后去找Task或者说Process，如果找不到，就新建，新建之后就相当于恢复现场


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
 
#  被动杀死Lowmemorykiller
    
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
               
##  通过socket与Lowmemorykiller通信

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
 
# 注意事项 

一般需要注意的是Fragment的处理

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

[Android 操作系统的内存回收机制](https://www.ibm.com/developerworks/cn/opensource/os-cn-android-mmry-rcycl/) 
  
[Android LowMemoryKiller原理分析 精](http://gityuan.com/2016/09/17/android-lowmemorykiller/)

[Android进程生命周期与ADJ](http://gityuan.com/2015/10/01/process-lifecycle/)

[Linux下/proc目录简介](http://blog.csdn.net/zdwzzu2006/article/details/7747977)

[Android系统中的进程管理：进程的创建 精 ](http://qiangbo.space/2016-10-10/AndroidAnatomy_Process_Creation/)    

[Android系统中的进程管理：进程的优先级](Android系统中的进程管理：进程的优先级)

