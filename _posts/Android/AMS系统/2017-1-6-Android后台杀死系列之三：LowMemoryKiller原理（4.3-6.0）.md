---
layout: post
title: "Android后台杀死系列之三：LowMemoryKiller原理（4.3-6.0）"
category: Android
image: http://upload-images.jianshu.io/upload_images/1460468-dec3e577ea74f0e8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---
  
本篇是Android后台杀死系列的第三篇，前面两篇已经对后台杀死注意事项，杀死恢复机制做了分析，本篇主要讲解的是Android后台杀死原理。相对于后台杀死恢复，LowMemoryKiller原理相对简单，并且在网上还是能找到不少资料的，不过，由于Android不同版本在框架层的实现有一些不同，网上的分析也多是针对一个Android版本，本文简单做了以下区分对比。LowMemoryKiller(低内存杀手)是Andorid基于oomKiller原理所扩展的一个多层次oomKiller，OOMkiller(Out Of Memory Killer)是在Linux系统无法分配新内存的时候，选择性杀掉进程，到oom的时候，系统可能已经不太稳定，而LowMemoryKiller是一种根据内存阈值级别触发的内存回收的机制，在系统可用内存较低时，就会选择性杀死进程的策略，相对OOMKiller，更加灵活。在详细分析其原理与运行机制之前，不妨自己想一下，假设让你设计一个LowMemoryKiller，你会如何做，这样一个系统需要什么功能模块呢？

* 进程优先级定义：只有有了优先级，才能决定先杀谁，后杀谁
* 进程优先级的动态管理：一个进程的优先级不应该是固定不变的，需要根据其变动而动态变化，比如前台进程切换到后台优先级肯定要降低
* 进程杀死的时机，什么时候需要挑一个，或者挑多个进程杀死
* 如何杀死

以上几个问题便是一个MemoryKiller模块需要的基本功能，Android底层采用的是Linux内核，其进程管理都是基于Linux内核，LowMemoryKiller也相应的放在内核模块，这也意味着用户空间对于后台杀死不可见，就像AMS完全不知道一个APP是否被后台杀死，只有在AMS唤醒APP的时候，才知道APP是否被LowMemoryKiller杀死过。其实LowmemoryKiller的原理是很清晰的，先看一下整体流程图，再分步分析：

![App操作影响进程优先级](http://upload-images.jianshu.io/upload_images/1460468-dec3e577ea74f0e8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
  
先记住两点 ：

1. LowMemoryKiller是被动杀死进程
2. Android应用通过AMS，利用proc文件系统更新进程信息

# 目录

[Android应用进程优先级及oomAdj](#oomAdj_declear)       
[Android应用的优先级是如何更新的 ](#update_oomAdj)         
[LomemoryKiller内核部分：杀死原理](#kill)         

<a name="kill"></a>
 

# Android应用进程优先级及oomAdj

Android会尽可能长时间地保持应用存活，但为了新建或运行更重要的进程，可能需要移除旧进程来回收内存，在选择要Kill的进程的时候，系统会根据进程的运行状态作出评估，权衡进程的“重要性“，其权衡的依据主要是四大组件。如果需要缩减内存，系统会首先消除重要性最低的进程，然后是重要性略逊的进程，依此类推，以回收系统资源。在Android中，应用进程划分5级（[摘自Google文档](https://developer.android.com/guide/components/processes-and-threads.html)）：Android中APP的重要性层次一共5级：

* 前台进程(Foreground process)
* 可见进程(Visible process)
* 服务进程(Service process)
* 后台进程(Background process)
* 空进程(Empty process)

> 前台进程

用户当前操作所必需的进程。如果一个进程满足以下任一条件，即视为前台进程：

* 包含正在交互的Activity（resumed
* 包含绑定到正在交互的Activity的Service
* 包含正在“前台”运行的Service（服务已调用startForeground()）
* 包含正执行一个生命周期回调的Service（onCreate()、onStart() 或 onDestroy()）
* 包含一个正执行其onReceive()方法的BroadcastReceiver

通常，在任意给定时间前台进程都为数不多。只有在内存不足以支持它们同时继续运行这一万不得已的情况下，系统才会终止它们。 此时，设备往往已达到内存分页状态，因此需要终止一些前台进程来确保用户界面正常响应。

> 可见进程

没有任何前台组件、但仍会影响用户在屏幕上所见内容的进程。 如果一个进程满足以下任一条件，即视为可见进程：

* 包含不在前台、但仍对用户可见的 Activity（已调用其 onPause() 方法）。例如，如果前台 Activity 启动了一个对话框，允许在其后显示上一Activity，则有可能会发生这种情况。
* 包含绑定到可见（或前台）Activity 的 Service。

可见进程被视为是极其重要的进程，除非为了维持所有前台进程同时运行而必须终止，否则系统不会终止这些进程。

> 服务进程

正在运行已使用 startService() 方法启动的服务且不属于上述两个更高类别进程的进程。尽管服务进程与用户所见内容没有直接关联，但是它们通常在执行一些用户关心的操作（例如，在后台播放音乐或从网络下载数据）。因此，除非内存不足以维持所有前台进程和可见进程同时运行，否则系统会让服务进程保持运行状态。

> 后台进程

包含目前对用户不可见的 Activity 的进程（已调用 Activity 的 onStop() 方法）。这些进程对用户体验没有直接影响，系统可能随时终止它们，以回收内存供前台进程、可见进程或服务进程使用。 通常会有很多后台进程在运行，因此它们会保存在 LRU （最近最少使用）列表中，以确保包含用户最近查看的 Activity 的进程最后一个被终止。如果某个 Activity 正确实现了生命周期方法，并保存了其当前状态，则终止其进程不会对用户体验产生明显影响，因为当用户导航回该 Activity 时，Activity会恢复其所有可见状态。 有关保存和恢复状态、或者异常杀死恢复可以参考前两篇 文章。

> 空进程

不含任何活动应用组件的进程。保留这种进程的的唯一目的是用作缓存，以缩短下次在其中运行组件所需的启动时间，这就是所谓**热启动 **。为了使系统资源在进程缓存和底层内核缓存之间保持平衡，系统往往会终止这些进程。

根据进程中当前活动组件的重要程度，Android会将进程评定为它可能达到的最高级别。例如，如果某进程托管着服务和可见 Activity，则会将此进程评定为可见进程，而不是服务进程。此外，一个进程的级别可能会因其他进程对它的依赖而有所提高，即服务于另一进程的进程其级别永远不会低于其所服务的进程。 例如，如果进程 A 中的内容提供程序为进程 B 中的客户端提供服务，或者如果进程 A 中的服务绑定到进程 B 中的组件，则进程 A 始终被视为至少与进程B同样重要。

通过Google文档，对不同进程的重要程度有了一个直观的认识，下面看一下量化到内存是什么样的呈现形式，这里针对不同的重要程度，做了进一步的细分，定义了重要级别ADJ，并将优先级存储到内核空间的进程结构体中去，供LowmemoryKiller参考：
 
| ADJ优先级     | 优先级          | 对应场景  |
| ------------- |:-------------:| :-----|
| UNKNOWN_ADJ     | 16 | 一般指将要会缓存进程，无法获取确定值 |
| CACHED_APP_MAX_ADJ     | 15      |  不可见进程的adj最大值（不可见进程可能在任何时候被杀死） |
|  CACHED_APP_MIN_ADJ|     9 |      不可见进程的adj最小值（不可见进程可能在任何时候被杀死）|
|  SERVICE_B_AD|    8  |    B List中的Service（较老的、使用可能性更小）  |
| PREVIOUS_APP_ADJ |  7    |     上一个App的进程(比如APP_A跳转APP_B,APP_A不可见的时候，A就是属于PREVIOUS_APP_ADJ) |
| HOME_APP_ADJ |    6  |     Home进程 |
| SERVICE_ADJ |   5   |    服务进程(Service process)  |
| HEAVY_WEIGHT_APP_ADJ |   4   |  后台的重量级进程，system/rootdir/init.rc文件中设置    |
| BACKUP_APP_ADJ |   3   |   备份进程（这个不太了解）   |
| PERCEPTIBLE_APP_ADJ |    2  |    可感知进程，比如后台音乐播放<  |
| >VISIBLE_APP_ADJ |  1    |   可见进程(可见，但是没能获取焦点，比如新进程仅有一个悬浮Activity，Visible process)   |
| FOREGROUND_APP_ADJ |   0   |     前台进程（正在展示是APP，存在交互界面，Foreground process）  |
| PERSISTENT_SERVICE_ADJ |   -11   |   关联着系统或persistent进程   |
| PERSISTENT_PROC_ADJ |  -12    |     系统persistent进程，比如电话 |
|SYSTEM_ADJ  |   -16   |  系统进程    |
| NATIVE_ADJ |   -17   |   native进程（不被系统管理   |

**以上介绍的目的只有一点：Android的应用进程是有优先级的，它的优先级跟当前是否存在展示界面，以及是否能被用户感知有关，越是被用户感知的的应用优先级越高（系统进程不考虑）。**

<a name="update_oomAdj"></a>

# Android应用的优先级是如何更新的 

APP中很多操作都可能会影响进程列表的优先级，比如退到后台、移到前台等，都会潜在的影响进程的优先级，我们知道Lowmemorykiller是通过遍历内核的进程结构体队列，选择优先级低的杀死，那么APP操作是如何写入到内核空间的呢？Linxu有用户间跟内核空间的区分，无论是APP还是系统服务，都是运行在用户空间，严格说用户控件的操作是无法直接影响内核空间的，更不用说更改进程的优先级。其实这里是通过了Linux中的一个proc文件体统，proc文件系统可以简单的看多是内核空间映射成用户可以操作的文件系统，当然不是所有进程都有权利操作，通过proc文件系统，用户空间的进程就能够修改内核空间的数据，比如修改进程的优先级，在Android家族，5.0之前的系统是AMS进程直接修改的，5.0之后，是修改优先级的操作被封装成了一个独立的服务-lmkd，lmkd服务位于用户空间，其作用层次同AMS、WMS类似，就是一个普通的系统服务。我们先看一下5.0之前的代码，这里仍然用4.3的源码看一下，模拟一个场景，APP只有一个Activity，我们主动finish掉这个Activity，APP就回到了后台，这里要记住，虽然没有可用的Activity，但是APP本身是没哟死掉的，这就是所谓的热启动，先看下大体的流程：

![App操作影响进程优先级](http://upload-images.jianshu.io/upload_images/1460468-dec3e577ea74f0e8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

现在直接去AMS看源码：

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
    
一开始的流程跟startActivity类似，首先是先暂停当前resume的Activity，其实也就是自己，
    
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

pause掉当前Activity之后，还需要唤醒上一个Activity，如果当前APP的Activity栈里应经空了，就回退到上一个应用或者桌面程序，唤醒流程就不在讲解了，因为在AMS恢复异常杀死APP的那篇已经说过，这里要说的是唤醒之后对这个即将退回后台的APP的操作，这里注意与startActivity不同的地方，看下面代码：
    
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
        
看一下上面的两个关键点1跟2，1是同startActivity的completePauseLocked不同的地方，主动finish的prev.finishing是为true的，因此会执行finishCurrentActivityLocked分支，将当前pause的Activity加到mStoppingActivities队列中去，并且唤醒下一个需要走到到前台的Activity，唤醒后，会继续执行stop：
		   
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
	
可以看到，在native代码里，就是通过proc文件系统修改内核信息，这里就是动态更新进程的优先级oomAdj，以上是针对Android4.3系统的分析，之后会看一下5.0之后的系统是如何实现的。下面是4.3更新oomAdj的流程图，注意红色的执行点：

![LowMemoryKiller更新进程oomAdj](http://upload-images.jianshu.io/upload_images/1460468-942f1601d0e6fbdc.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
	

# Android5.0之后框架层的实现：LMKD服务

Android5.0将设置进程优先级的入口封装成了一个独立的服务lmkd服务，AMS不再直接访问proc文件系统，而是通过lmkd服务来进行设置，从init.rc文件中看到服务的配置。

	service lmkd /system/bin/lmkd
	    class core
	    critical
	    socket lmkd seqpacket 0660 system system
	    
从配置中可以看出，该服务是通过socket与其他进行进程进行通信，其实就是AMS通过socket向lmkd服务发送请求，让lmkd去更新进程的优先级，lmkd收到请求后，会通过/proc文件系统去更新内核中的进程优先级。首先看一下5.0中这一块AMS有什么改变，其实大部分流程跟之前4.3源码类似，我们只看一下不同地方

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
        
从上面的不同点1可以看出，5.0之后是通过ProcessList类去设置oomAdj，其实这里就是通过socket与LMKD服务进行通信，向lmkd服务传递给LMK_PROCPRIO命令去更新进程优先级：
       
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

其实就是openLmkdSocket打开本地socket端口，并将优先级信息发送过去，那么lmkd服务端如何处理的呢，init.rc里配置的服务是在开机时启动的，来看看lmkd服务的入口：main函数

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

很简单，打开一个端口，并通过mainloop监听socket，如果有请求到来，就解析命令并执行，刚才传入的LMK_PROCPRIO命令对应的操作就是cmd_procprio，用来更新oomAdj，其更新新机制还是通过proc文件系统，不信？看下面代码：

	static void cmd_procprio(int pid, int uid, int oomadj) {
	    struct proc *procp;
	    。。。
	    还是利用/proc文件系统进行更新
	    snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
	    snprintf(val, sizeof(val), "%d", lowmem_oom_adj_to_oom_score_adj(oomadj));
	    writefilestring(path, val);
	   。。。
	}

简单的流程图如下，同4.3不同的地方
	
![Android5.0之后的LMKD服务](http://upload-images.jianshu.io/upload_images/1460468-75fb0b227f12aeb4.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

以上就分析完了用户空间的操作如何影响到进程的优先级，并且将新的优先级写到内核中。最后看一下LomemoryKiller在什么时候、如何根据优先级杀死进程的：

<a name="kill"></a>

# LomemoryKiller内核部分：如何杀死

LomemoryKiller属于一个内核驱动模块，主要功能是：在系统内存不足的时候扫描进程队列，找到低优先级（也许说性价比低更合适）的进程并杀死，以达到释放内存的目的。对于驱动程序，入口是__init函数，先看一下这个驱动模块的入口：

	static int __init lowmem_init(void)
	{
		register_shrinker(&lowmem_shrinker);
		return 0;
	}
	
可以看到在init的时候，LomemoryKiller将自己的lowmem_shrinker入口注册到系统的内存检测模块去，作用就是在内存不足的时候可以被回调，register_shrinker函数是一属于另一个内存管理模块的函数，如果一定要根下去的话，可以看一下它的定义，其实就是加到一个回调函数队列中去:

	void register_shrinker(struct shrinker *shrinker)
	{
		shrinker->nr = 0;
		down_write(&shrinker_rwsem);
		list_add_tail(&shrinker->list, &shrinker_list);
		up_write(&shrinker_rwsem);
	}

最后，看一下，当内存不足触发回调的时候，LomemoryKiller是如何找到低优先级进程，并杀死的：入口函数就是init时候注册的lowmem_shrink函数（4.3源码，后面的都有微调但原理大概类似）：
	
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

先看关键点1：其实就是确定当前低内存对应的阈值；关键点2 ：找到比该阈值优先级低或者相等，并且内存占用较多的进程（tasksize = get_mm_rss(p->mm)其实就是获取内存占用）），将其杀死。如何杀死的呢？很直接，通过Linux的中的信号量，发送SIGKILL信号直接将进程杀死。到这就分析完了LomemoryKiller内核部分如何工作的。其实很简单，**一句话：被动扫描，找到低优先级的进程，杀死。**
	
 
# 总结

通过本篇文章，希望大家能有以下几点认知：

* Android APP进程是有优先级的的，与进程是否被用户感知有直接关系
* APP切换等活动都可能造成进程优先级的变化，都是利用AMS，并通过proc文件设置到内核的
* LowmemoryKiller运行在内核，在内存需要缩减的时候，会选择低优先级的进程杀死

至于更加细节的内存的缩减、优先级的计算也许将来会放到单独的文章中说明，本文的目的是：能让大家对LowmemoryKiller的概念以及运行机制有个简单了解。

    
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
[Google文档--进程和线程](https://developer.android.com/guide/components/processes-and-threads.html) 
