---
layout: post
title: "Android后台杀死系列之三：LowMemoryKiller原理（4.3-6.0）"
category: Android

---


 
 Android中对于内存的回收，主要依靠Lowmemorykiller来完成，是一种根据阈值级别触发相应力度的内存回收的机制。


本篇是Android后台杀死系列的第三篇，前面两篇已经对后台杀死注意事项，杀死恢复机制做了分析，本篇主要讲解的是Android后台杀死原理：LowMemoryKiller。LowMemoryKiller(低内存杀手)是Andorid基于oomKiller原理所扩展的一个多层次oomKiller，属于内核模块，运行在内核空间，OOMkiller(Out Of Memory Killer)是在无法分配新内存的时候，选择性杀掉进程，到oom的时候，系统可能已经处于亚健康状态。LowMemoryKiller是系统可用内存较低时，选择性杀死进程的策略，相对OOMKiller，更加灵活。在详细分析原理之前不妨自己想一下，假设让你设计一个LowMemoryKiller，你会如何做，这样一个系统需要什么功能模块呢？

* 进程优先级定义：先杀谁，后杀谁
* 进程优先级的动态管理：一个进程的优先级不应该是固定不变的，需要根据其变动而动态变化
* 杀死的时机，什么时候需要挑一个，或者挑多个进程杀死
* 如何杀

以上几个问题便是一个MemoryKiller模块需要的基本功能，Android底层采用的是Linux内核，所以其进程管理都是基于Linux内核，所以LowMemoryKiller放在内核也比较合理，不过这也意味着用户空间对于后台杀死不可见，就像AMS完全不知道一个APP是否被后台杀死，只有在AMS唤醒APP的时候，才知道APP是否被LowMemoryKiller杀死过。其实LowmemoryKiller的原理是很清晰的，先看一下整体流程图，再分步分析：

![App操作影响进程优先级](http://upload-images.jianshu.io/upload_images/1460468-dec3e577ea74f0e8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
  
 可以看到，影响进程被杀的点是AMS去更新内核的进程优先级，并且这个操作是单向的，所以先记住两点 

1. LowMemoryKiller是被动杀死进程
2. Android应用通过AMS，利用proc文件系统更新进程信息


* 通过fork来创建进行
* 通过信号量来管理进程
* 通过proc文件系统来查询和调整进程状态等



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
# oomKILLer(Linux自带)与Lowmemorykiller（android定制）

# 进程的优先级是如何更新的

![LowMemorykiller更新进程优先级](http://upload-images.jianshu.io/upload_images/1460468-ff1cdc46734ac3e2.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![5.0更新](http://upload-images.jianshu.io/upload_images/1460468-97a3a5e8a2a9555f.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


由于牵扯到内核，所以这里还是有个用户间跟内核空间的概念， APP中很多操作都会影响本身进程的优先级，比如退到后台、移到前台、因为加载图片分配了很多内存等，这些都会潜在的影响进程的优先级，这里我们用主动finish一个Activity为例子梳理一遍，先看一下4.3的源码，因为5.0 Lolipop之后引入了一个LMKD服务，这个在之前是没有，不过不用担心，这个服务位于用户空间，其作用层次同AMS、WMS类似，属于系统服务，在调用finish关闭掉当前Activity的时候


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
 # 进程保活
# 冷热启动
 
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

