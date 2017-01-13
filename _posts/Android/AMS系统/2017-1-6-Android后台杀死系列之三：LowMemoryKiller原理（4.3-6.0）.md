---
layout: post
title: "Android后台杀死系列之三：LowMemoryKiller原理（4.3-6.0）"
category: Android
image: http://upload-images.jianshu.io/upload_images/1460468-dec3e577ea74f0e8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---


 
 Android中对于内存的回收，主要依靠Lowmemorykiller来完成，是一种根据阈值级别触发相应力度的内存回收的机制。


本篇是Android后台杀死系列的第三篇，前面两篇已经对后台杀死注意事项，杀死恢复机制做了分析，本篇主要讲解的是Android后台杀死原理。相对于后台杀死恢复，LowMemoryKiller在网上还是能找到不少资料的，但是由于Android不同版本在框架层的实现有一些不同，本文引导区分一下，其实最底层都是类似的。

LowMemoryKiller(低内存杀手)是Andorid基于oomKiller原理所扩展的一个多层次oomKiller，属于内核模块，运行在内核空间，OOMkiller(Out Of Memory Killer)是在无法分配新内存的时候，选择性杀掉进程，到oom的时候，系统可能已经处于亚健康状态。LowMemoryKiller是系统可用内存较低时，选择性杀死进程的策略，相对OOMKiller，更加灵活。在详细分析原理之前不妨自己想一下，假设让你设计一个LowMemoryKiller，你会如何做，这样一个系统需要什么功能模块呢？

* 进程优先级定义：先杀谁，后杀谁
* 进程优先级的动态管理：一个进程的优先级不应该是固定不变的，需要根据其变动而动态变化
* 杀死的时机，什么时候需要挑一个，或者挑多个进程杀死
* 如何杀

以上几个问题便是一个MemoryKiller模块需要的基本功能，Android底层采用的是Linux内核，所以其进程管理都是基于Linux内核，所以LowMemoryKiller放在内核也比较合理，不过这也意味着用户空间对于后台杀死不可见，就像AMS完全不知道一个APP是否被后台杀死，只有在AMS唤醒APP的时候，才知道APP是否被LowMemoryKiller杀死过。其实LowmemoryKiller的原理是很清晰的，先看一下整体流程图，再分步分析：

![App操作影响进程优先级](http://upload-images.jianshu.io/upload_images/1460468-dec3e577ea74f0e8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
  
影响进程被杀的点是AMS去更新内核的进程优先级，并且这个操作是单向的，所以先记住两点 

1. LowMemoryKiller是被动杀死进程
2. Android应用通过AMS，利用proc文件系统更新进程信息

# oomAdj
# 进程保活
# 冷热启动

# Android应用进程优先级的概念 

要杀死低优先级的进程首先得有进程优先级的概念跟定义，Android中是如何定义应用程序的优先级的呢？Android中都是以组件的方式呈献给用户的，其进程的优先级正是由这些组件 及其运行状态决定的。在Android中应用进程划分5级（[Google文档](https://developer.android.com/guide/components/processes-and-threads.html)）：

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

# Android应用进程的优先级是如何更新的

![LowMemorykiller更新进程优先级](http://upload-images.jianshu.io/upload_images/1460468-ff1cdc46734ac3e2.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![5.0更新](http://upload-images.jianshu.io/upload_images/1460468-97a3a5e8a2a9555f.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

APP中很多操作都可能会影响本身进程的优先级，比如退到后台、移到前台、因为加载图片分配了很多内存等，都会潜在的影响进程的优先级，我们知道Lowmemorykiller是通过遍历内核的进程结构体队列，选择优先级低的杀死，那么APP操作是如何写入到内核空间的呢，Linxu有用户间跟内核空间的区分，无论是APP还是系统服务，都是运行在用户空间，严格说用户控件的操作是无法直接影响内核空间的，更不用说更改进程的优先级，其实这里是通过了Linux中的一个proc文件体统，proc文件系统可以简单的看多是内核空间映射成用户可以操作的文件系统，当然不是所有进程都有权利操作，通过proc文件系统，用户空间的进程就能够修改内核空间的数据，比如进程的优先级，而在Android中，在Android5.0之前是AMS进程，5.0之后，是一个独立的LMKD服务，LMKD服务位于用户空间，其作用层次同AMS、WMS类似，就是一个普通的系统服务。我们先看一下5.0之前的代码，这里仍然用4.3的源码看一下，模拟一个场景，APP只有一个Activity，我们主动finish掉这个Activity，这个APP就回到了后台，这里要记住，虽然没有可用的Activity，但是APP本身是没哟死掉的，这个也是所谓的热启动，直接去AMS看源码：

> ActivityManagerService
 
    public final boolean finishActivity(IBinder token, int resultCode, Intent resultData) {
         ...
        synchronized(this) {
           
            final long origId = Binder.clearCallingIdentity();
            boolean res = mMainStack.requestFinishActivityLocked(token, resultCode,
                    resultData, "app-request", true);
         ...
        }
    }
    
在这种场景下首先是先暂停当前resume的Activity，其实也就是自己，
    
	  final boolean finishActivityLocked(ActivityRecord r, int index, int resultCode,
	            Intent resultData, String reason, boolean immediate, boolean oomAdj) {
	         ...
	            if (mPausingActivity == null) {
	                if (DEBUG_PAUSE) Slog.v(TAG, "Finish needs to pause: " + r);
	                if (DEBUG_USER_LEAVING) Slog.v(TAG, "finish() => pause with userLeaving=false");
	                startPausingLocked(false, false);
	            }
				...
	    }

pause当前resume的Activity之后，还需要唤醒上一个Activity，如果当前APP没有回退的Activity，就会退到上一个应用或者桌面程序，唤醒流程就不在讲解了，因为在AMS恢复异常杀死APP的那篇已经说过，这里要说的是唤醒之后对这个即将退回后台的APP的操作，这里注意与startActivity不同的地方，看下面代码：
    
> ActivityStack    
 
     private final void completePauseLocked() {
        ActivityRecord prev = mPausingActivity;
         
        if (prev != null) {
            if (prev.finishing) {
            1、 不同点
           <!--主动finish的时候，走的是这个分支，状态变换的细节请自己查询代码-->
                prev = finishCurrentActivityLocked(prev, FINISH_AFTER_VISIBLE, false);
            } 
            ...
			2、相同点 		
         if (!mService.isSleeping()) {
            resumeTopActivityLocked(prev);
        }
看一下上面的两个关键点1跟2 ，1是同startActivity的completePauseLocked不同的地方，主动finish的prev.finishing是为true的，因此会执行finishCurrentActivityLocked分支，将当前pause的Activity加到mStoppingActivities队列中去，并且唤醒下一个需要到前台的Activity，唤醒后，会继续执行stop：
		   
		private final ActivityRecord finishCurrentActivityLocked(ActivityRecord r,
		        int index, int mode, boolean oomAdj) {
		    if (mode == FINISH_AFTER_VISIBLE && r.nowVisible) {
		        if (!mStoppingActivities.contains(r)) {
		            mStoppingActivities.add(r);
		 			...
		        }
				   ....
		        return r;
		    }
			...
		}

让我们再回到resumeTopActivityLocked继续看，resume之后会回调completeResumeLocked函数，继续执行stop，这个函数通过向Handler发送IDLE_TIMEOUT_MSG消息来回调activityIdleInternal函数，最终执行destroyActivityLocked销毁ActivityRecord，

	final boolean resumeTopActivityLocked(ActivityRecord prev, Bundle options) {
			...
	   if (next.app != null && next.app.thread != null) {					...
	            try {
	            	。。。
	                next.app.thread.scheduleResumeActivity(next.appToken,
	                        mService.isNextTransitionForward());
	      			..。
	            try {
	                next.visible = true;
	                completeResumeLocked(next);
	            }  
	            ....
	         } 
	         
在销毁Activity的时候，如果当前APP的Activity堆栈为空了，就说明当前Activity没有可见界面了，这个时候就需要动态更新这个APP的优先级，详细代码如下：

	 final boolean destroyActivityLocked(ActivityRecord r,
	            boolean removeFromApp, boolean oomAdj, String reason) {
	    		...
	       if (hadApp) {
	            if (removeFromApp) {
	                // 这里动ProcessRecord里面删除，但是没从history删除
	                int idx = r.app.activities.indexOf(r);
	                if (idx >= 0) {
	                    r.app.activities.remove(idx);
	                }
	                ...
	                if (r.app.activities.size() == 0) {
	                    // No longer have activities, so update oom adj.
	                    mService.updateOomAdjLocked();
	             	...
	       }
最终会调用AMS的updateOomAdjLocked函数去更新进程优先级，在4.3的源码里面，主要是通过Process类的setOomAdj函数来设置优先级：
           	                  
> ActivityManagerService
        
    private final boolean updateOomAdjLocked(ProcessRecord app, int hiddenAdj,
            int clientHiddenAdj, int emptyAdj, ProcessRecord TOP_APP, boolean doingAll) {
        ...
        计算优先级
        computeOomAdjLocked(app, hiddenAdj, clientHiddenAdj, emptyAdj, TOP_APP, false, doingAll);
		 。。。
 		 <!--如果不相同，设置新的OomAdj-->
  		 
        if (app.curAdj != app.setAdj) {
            if (Process.setOomAdj(app.pid, app.curAdj)) {
            ...
    }
    
Process中setOomAdj是一个native方法，原型在android_util_Process.cpp中

> android_util_Process.cpp

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
	
可以看到，就是通过proc文件系统修改内核信息，这里就是动态更新进程的优先级oomAdj，以上是针对Android4.3系统的分析，那么再来看一下5.0之后的系统是如何实现的。

# Android5.0之后的LowMemoryKiller框架层实现：LMKD服务

Android5.0将设置进程优先级的入口封装成了一个独立的服务LMKD服务，AMS不再直接访问proc文件系统，而是通过LMKD服务来进行设置，从init.rc文件中看到服务的配置。

	service lmkd /system/bin/lmkd
	    class core
	    critical
	    socket lmkd seqpacket 0660 system system
	    
从配置中可以看出，该服务是通过socket与其他进行进程进行通信，再来看一下AMS有什么改变，大部分流程跟之前4.3源码类似，看一下不同地方


> ActivityManagerService

    private final boolean updateOomAdjLocked(ProcessRecord app, int cachedAdj,
            ProcessRecord TOP_APP, boolean doingAll, long now) {
        ...
        computeOomAdjLocked(app, cachedAdj, TOP_APP, doingAll, now);
        ...
        applyOomAdjLocked(app, doingAll, now, SystemClock.elapsedRealtime());
    }

    private final boolean applyOomAdjLocked(ProcessRecord app, boolean doingAll, long now,
            long nowElapsed) {
        boolean success = true;

        if (app.curRawAdj != app.setRawAdj) {
            app.setRawAdj = app.curRawAdj;
        }

        int changes = 0;
		  不同点1
        if (app.curAdj != app.setAdj) {
            ProcessList.setOomAdj(app.pid, app.info.uid, app.curAdj);
            if (DEBUG_SWITCH || DEBUG_OOM_ADJ) Slog.v(TAG_OOM_ADJ,
                    "Set " + app.pid + " " + app.processName + " adj " + app.curAdj + ": "
                    + app.adjType);
            app.setAdj = app.curAdj;
            app.verifiedAdj = ProcessList.INVALID_ADJ;
        }
        
从上面的不同点1可以看出，5.0之后是通过一个ProcessList类去设置oomAdj，其实这里就是通过socket与LMKD服务进行通信，这里传递给lmkd服务的命令是LMK_PROCPRIO

        
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
	      }    
	    
	private static void writeLmkd(ByteBuffer buf) {
		 	for (int i = 0; i < 3; i++) {
		    if (sLmkdSocket == null) {
		      if (openLmkdSocket() == false) {
				...
		    try {
		        sLmkdOutputStream.write(buf.array(), 0, buf.position());
		        return;
		        ...
		}

其实就是openLmkdSocket打开本地socket端口，并将优先级信息发送过去，那么lmkd服务端如何处理的呢，init.rc里配置的服务是在开机时启动的，来看看lmkd服务的main入口

> lmkd.c函数

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

其实就是打开一个端口，并不通过mainloop，监听socket，有需求到来，就解析命令并执行，刚才传入的LMK_PROCPRIO命令对应的操作就是cmd_procprio，用来更新oomAdj，其实其更新新机制还是通过proc文件系统，看下面代码：

	static void cmd_procprio(int pid, int uid, int oomadj) {
	    struct proc *procp;
	    。。。
	    还是利用/proc文件系统进行更新
	    snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
	    snprintf(val, sizeof(val), "%d", lowmem_oom_adj_to_oom_score_adj(oomadj));
	    writefilestring(path, val);
	   。。。
	}

以上就分析完了用户空间的操作如何影响到进程的优先级，并且将新的优先级写到内核中。下面看一下LomemoryKiller在什么时候根据优先级杀死进程的：

# LomemoryKiller内核部分：如何杀死

LomemoryKiller属于一个内核驱动呢模块，主要是在系统内存不足的时候扫描进程队列，找到优先级低（也许说性价比更合适）的进程并杀死，以达到释放内存的目的，看一下这个驱动模块的注册入口：

	static int __init lowmem_init(void)
	{
		register_shrinker(&lowmem_shrinker);
		return 0;
	}
	
可以看到在注册驱动的时候，LomemoryKiller其实就是将自己的lowmem_shrinker入口注册到系统的内存检测模块去，作用就是在内存不足的时候可以被回调，register_shrinker函数是一属于另一个内存管理模块的函数，如果一定要根下去的话，可以看一下它的定义:

void register_shrinker(struct shrinker *shrinker)
{
	shrinker->nr = 0;
	down_write(&shrinker_rwsem);
	list_add_tail(&shrinker->list, &shrinker_list);
	up_write(&shrinker_rwsem);
}

现在来看一下lowmem_shrinker是如何找到低优先级进程，并杀死的，
	
	static int lowmem_shrink(int nr_to_scan, gfp_t gfp_mask)
	{
		struct task_struct *p;
		。。。
		关键点1 找到当前的内存对应的阈值
		for(i = 0; i < array_size; i++) {
			if (other_free < lowmem_minfree[i] &&
			    other_file < lowmem_minfree[i]) {
				min_adj = lowmem_adj[i];
				break;
			}
		}
		。。。
		关键点2 找到优先级低于这个阈值的进程，并杀死
		
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
 
		}
		if(selected != NULL) {
			force_sig(SIGKILL, selected);
			rem -= selected_tasksize;
		}
		lowmem_print(4, "lowmem_shrink %d, %x, return %d\n", nr_to_scan, gfp_mask, rem);
		read_unlock(&tasklist_lock);
		return rem;
	}

先看关键点1、其实就是确定当前低内存对应的阈值，关键点2 ，找到该阈值下优先级低，切内存占用高的的进程，将其杀死，其杀死方式很直接，就是通过Linux的中的信号量，发送SIGKILL信号直接将进程杀死。到这就分析完了LomemoryKiller内核部分如何工作，其实很简单，就是被动扫描，找打低优先级的进程，杀死。
	
 
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

