---
layout: post
title: "LowMemoryKiller机制及原理（4.3-6.0）"
description: "Java"
category: android开发

---

# Android后台杀死原理分析


Android系统以Linux内核为基础，所以对于进程的管理自然离不开Linux本身提供的机制。例如：

通过fork来创建进行
通过信号量来管理进程
通过proc文件系统来查询和调整进程状态 等


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

# 进程优先级 更新
# LMKD
# 进程保活
# 冷热启动
# 内核部分 
# oomKILLer(Linux自带)与Lowmemeorykiller（android定制）

# Lowmemeorykiller流程

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

