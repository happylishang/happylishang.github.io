---
layout: post
title: "备份 Android后台杀死系列之五：实践篇 进程保活-自“裁”或者耍流氓"
category: Android
image: http://upload-images.jianshu.io/upload_images/1460468-dec3e577ea74f0e8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---
 
研究后台杀究竟有什么用呢，没用你研究它干嘛，既然有杀死，就有保活。本篇文章主要探讨一下进程的保活，Android本身设计的时候是非常善良的，它希望进程在不可见或者其他一些场景下APP要懂得主动释放，可是Android低估了”贪婪“，尤其是很多国产APP，只希望索取来提高自己的性能，不管其他APP或者系统的死活，导致了很严重的资源浪费，这也是Android被iOS诟病的最大原因。本文的保活手段也分两种：遵纪守法的进程保活与流氓手段换来的进程保活。

# 针对LowmemoryKiller所做的进程保活

LowmemoryKiller会在内存不足的时候扫描所有的用户进程，找到不是太重要的进程杀死，至于LowmemoryKiller杀进程够不够狠，要看当前的内存使用情况，内存越少，下手越狠。在内核中，LowmemoryKiller.c定义了几种内存回收等级如下：（也许不同的版本会有些不同）

	static short lowmem_adj[6] = {
		0,
		1,
		6,
		12,
	};
	static int lowmem_adj_size = 4;
	
	static int lowmem_minfree[6] = {
		3 * 512,	/* 6MB */
		2 * 1024,	/* 8MB */
		4 * 1024,	/* 16MB */
		16 * 1024,	/* 64MB */
	};
	static int lowmem_minfree_size = 4;
		
lowmem_adj中各项数值代表阈值的警戒级数，lowmem_minfree代表对应级数的剩余内存，两者一一对应，比如当系统的可用内存小于6MB时，警戒级数为0；当系统可用内存小于8M而大于6M时，警戒级数为1；当可用内存小于64M大于16MB时，警戒级数为12。LowmemoryKiller就是根据当前系统的可用内存多少来获取当前的警戒级数，如果进程的oom_adj大于警戒级数并且占内存最大，将会被优先杀死， **具有相同omm_adj的进程，则杀死占用内存较多的**。omm_adj越小，代表进程越重要。一些前台的进程，oom_adj会比较小，而后台的服务，omm_adj会比较大，所以当内存不足的时候，Lowmemorykiller先杀掉的是后台服务而不是前台的进程。对于LowmemoryKiller的杀死，这里有一句话很重要，就是： **具有相同omm_adj的进程，则杀死占用内存较多的**，因此，如果我们的APP进入后台，就尽量释放不必要的资源，以降低自己被杀的风险。那么如何释放呢？onTrimeMemory是个不错的时机，而onLowmemory可能是最后的稻草，下面复习一下，LowmemoryKiller如何杀进程的，简单看一下实现源码（4.3）：（其他版本原理大同小异）

	static int lowmem_shrink(int nr_to_scan, gfp_t gfp_mask)
	{
		...		
		<!--关键点1 获取free内存状况-->
		int other_free = global_page_state(NR_FREE_PAGES);
		int other_file = global_page_state(NR_FILE_PAGES);
		<!--关键点2 找到min_adj -->
		for(i = 0; i < array_size; i++) {
			if (other_free < lowmem_minfree[i] &&
			    other_file < lowmem_minfree[i]) {
				min_adj = lowmem_adj[i];
				break;
			}
		}
	  <!--关键点3 找到p->oomkilladj>min_adj并且oomkilladj最大，内存最大的进程-->
		for_each_process(p) {
			// 找到第一个大于等于min_adj的，也就是优先级比阈值低的
			if (p->oomkilladj < min_adj || !p->mm)
				continue;
			// 找到tasksize这个是什么呢
			tasksize = get_mm_rss(p->mm);
			if (tasksize <= 0)
				continue;
			if (selected) {
			// 找到优先级最低，并且内存占用大的
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
		if(selected != NULL) {...
			force_sig(SIGKILL, selected);
		}
		return rem;
	}

这里先看一下关键点1，这里是内核获取当前的free内存状况，并且根据当前空闲内存计算出当前后台杀死的等级（关键点2），之后LowmemoryKiller会遍历所有的进程，找到优先级低并且内存占用较大的进程，如果这个进程的p->oomkilladj>min_adj，就表示这个进程可以杀死，LowmemoryKiller就会送过发送SIGKILL信号杀死就进程，注意，**lmkd会先找优先级低的进程，如果多个进程优先级相同，就优先杀死内存占用高的进程，这样就为我们提供了两种进程包活手段**：

* 1、提高进程的优先级，其实就是减小进程的p->oomkilladj（越小越重要）
* 2、降低APP的内存占用量，在oom_adj相同的时候，会优先干掉内存消耗大的进程

不过大多数情况下，Android对于进程优先级的管理都是比较合理，即使某些场景需要特殊手段提高优先级，Android也是给了参考方案的，比如音频播放，UI隐藏的时候，需要将Sevice进程设置成特定的优先级防止被后台杀死，比如一些备份的进程也需要一些特殊处理，但是这些都是在Android允许的范围内的，所以绝大多数情况下，Android是不建议APP自己提高优先级的，因为这会与Android自身的的进程管理相悖，**换句话说就是耍流氓**。这里先讨论第二种情况，通过合理的释放内存降低被杀的风险，地主不想被杀，只能交公粮，自裁保身，不过这里也要看自裁的时机，什么时候瘦身比较划算，O(∩_∩)O哈哈~！这里就牵扯到有一个onTrimeMemory函数，该函数是一个系统回调函数，主要是Android系统经过综合评估，给APP一个内存裁剪的等级，比如当内存还算充足，APP退回后台时候，会收到TRIM_MEMORY_UI_HIDDEN等级的裁剪，就是告诉APP，释放一些UI资源，比如大量图片内存，一些引入图片浏览缓存的场景，可能就更加需要释放UI资源，下面来看下onTrimeMemory的回调时机及APP应该做出相应处理。

## onTrimeMemory的回调时机及内存裁剪等级

OnTrimMemory是在Android 4.0引入的一个回调接口，其主要作用就是通知应用程序在不同的场景下进行自我瘦身，释放内存，降低被后台杀死的风险，提高用户体验，由于目前APP的适配基本是在14之上，所以不必考虑兼容问题。onTrimeMemory支持不同裁剪等级，比如，APP通过HOME建进入后台时，其优先级（oom_adj）就发生变化，从未触发onTrimeMemory回调，这个时候系统给出的裁剪等级一般是TRIM_MEMORY_UI_HIDDEN，意思是，UI已经隐藏，UI相关的、占用内存大的资源就可以释放了，比如大量的图片缓存等，当然，还会有其他很多场景对应不同的裁剪等级。因此，需要弄清楚两个问题：

* 1、不同的裁剪等级是如何生成的，其意义是什么
* 2、APP如何根据不同的裁剪等级释放内存资源，（自裁的程度）

先看下ComponentCallbacks2中定义的不同裁剪等级的意义：这里一共定义了4+3共7个裁剪等级，为什么说是4+3呢？因为有4个是针对后台进程的，还有3个是针对前台（RUNNING）进程的，目标对象不同，具体看下分析

|裁剪等级|数值|目标进程|
| ------------- |:-------------:| -----:|
| TRIM_MEMORY_COMPLETE | 80 |后台进程 |
| TRIM_MEMORY_MODERATE | 60 |后台进程 |
|  TRIM_MEMORY_BACKGROUND | 40 |后台进程 |
| TRIM_MEMORY_UI_HIDDEN | 20 | 后台进程|
|  TRIM_MEMORY_RUNNING_CRITICAL | 15 | 前台RUNNING进程|
| TRIM_MEMORY_RUNNING_LOW | 10 | 前台RUNNING进程|
| TRIM_MEMORY_RUNNING_MODERATE |5 | 前台RUNNING进程|

其意义如下：

* TRIM_MEMORY_UI_HIDDEN 当前应用程序的所有UI界面不可见，一般是用户点击了Home键或者Back键，导致应用的UI界面不可见，这时应该释放一些UI相关资源，TRIM_MEMORY_UI_HIDDEN是使用频率最高的裁剪等级。官方文档：the process had been showing a user interface, and is no longer doing so.  Large allocations with the UI should be released at this point to allow memory to be better managed

* TRIM_MEMORY_BACKGROUND 当前手机目前内存吃紧（**后台进程数量少**），系统开始根据LRU缓存来清理进程，而该程序位于LRU缓存列表的头部位置，不太可能被清理掉的，但释放掉一些比较容易恢复的资源能够提高手机运行效率，同时也能保证恢复速度。官方文档：the process has gone on to the LRU list.  This is a good opportunity to clean up resources that can efficiently and quickly be re-built if the user returns to the app

* TRIM_MEMORY_MODERATE  当前手机目前内存吃紧（**后台进程数量少**），系统开始根据LRU缓存来清理进程，而该程序位于LRU缓存列表的中间位置，应该多释放一些内存提高运行效率。官方文档：the process is around the middle of the background LRU list; freeing memory can help the system keep other processes running later in the list for better overall performance.

* TRIM_MEMORY_COMPLETE  当前手机目前内存吃紧 （**后台进程数量少**），系统开始根据LRU缓存来清理进程，而该程序位于LRU缓存列表的最边缘位置，系统会先杀掉该进程，应尽释放一切可以释放的内存。官方文档：the process is nearing the end  of the background LRU list, and if more memory isn't found soon it will be killed.

以下三个等级针对前台运行应用

* TRIM_MEMORY_RUNNING_MODERATE 表示该进程是前台或可见进程，正常运行，一般不会被杀掉，但是目前手机有些吃紧（**后台及空进程存量不多**），系统已经开始清理内存，有必要的话，可以释放一些内存。官方文档：the process is not an expendable background process, but the device is running moderately low on memory. Your running process may want to release some unneeded resources for use elsewhere。

* TRIM_MEMORY_RUNNING_LOW 表示该进程是前台或可见进程，正常运行，一般不会被杀掉，但是目前手机比较吃紧（**后台及空进程被全干掉了一大波**），应该去释放掉一些不必要的资源以提升系统性能。 官方文档：the process is not an expendable background process, but the device is running low on memory.  Your running process should free up unneeded resources to allow that  memory to be used elsewhere.

* TRIM_MEMORY_RUNNING_CRITICAL 表示该进程是前台或可见进程，但是目前手机比较内存十分吃紧（**后台及空进程基本被全干掉了**），这时应当尽可能地去释放任何不必要的资源，否则，系统可能会杀掉所有缓存中的进程，并且杀一些本来应当保持运行的进程。官方文档：the process is not an expendable background process, but the device is running extremely low on memory   and is about to not be able to keep any background processes running.  Your running process should free up as many non-critical resources as it  can to allow that memory to be used elsewhere.  The next thing that will happen after this is called to report that  nothing at all can be kept in the background, a situation that can start to notably impact the user.

以上只是抽象的说明了一下Android既定参数的意义，下面看一下onTrimeMemory回调的时机及原理，这里采用6.0的代码分析，因为6.0这块的代码经过了重构，比之前4.3的代码清晰很多：当用户的操作导致APP优先级发生变化，就会调用updateOomAdjLocked去更新进程的优先级，在更新优先级的时候，会扫描一遍LRU进程列表， 重新计算进程的oom_adj，并且参考当前系统状况去通知进程裁剪内存（这里只是针对Android Java层APP），这次操作一般发生在打开新的Activity界面、退回后台、应用跳转切换等等，updateOomAdjLocked代码大概600多行，比较长，尽量精简后如下：

    final void updateOomAdjLocked() {
        final ActivityRecord TOP_ACT = resumedAppLocked();
        <!--关键点1 找到TOP——APP，最顶层显示的APP-->
        final ProcessRecord TOP_APP = TOP_ACT != null ? TOP_ACT.app : null;
        final long oldTime = SystemClock.uptimeMillis() - ProcessList.MAX_EMPTY_TIME;
        mAdjSeq++;
        mNewNumServiceProcs = 0;
        final int emptyProcessLimit;
        final int hiddenProcessLimit;
        <!--关键点2 找到TOP——APP，最顶层显示的APP-->
        // 初始化一些进程数量的限制：
        if (mProcessLimit <= 0) {
            emptyProcessLimit = hiddenProcessLimit = 0;
        } else if (mProcessLimit == 1) {
            emptyProcessLimit = 1;
            hiddenProcessLimit = 0;
        } else {
        	// 空进程跟后台非空缓存继承的比例
            emptyProcessLimit = ProcessList.computeEmptyProcessLimit(mProcessLimit);
            cachedProcessLimit = mProcessLimit - emptyProcessLimit;
        }
 
        
        <!--关键点3 确定下进程槽 3个槽->
        int numSlots = (ProcessList.HIDDEN_APP_MAX_ADJ - ProcessList.HIDDEN_APP_MIN_ADJ + 1) / 2;
        // 后台进程/前台进程/空进程
        int numEmptyProcs = N - mNumNonCachedProcs - mNumCachedHiddenProcs;
        
        int emptyFactor = numEmptyProcs/numSlots;
        if (emptyFactor < 1) emptyFactor = 1;
        int hiddenFactor = (mNumHiddenProcs > 0 ? mNumHiddenProcs : 1)/numSlots;
        if (hiddenFactor < 1) hiddenFactor = 1;
        int stepHidden = 0;
        int stepEmpty = 0;
        int numHidden = 0;
        int numEmpty = 0;
        int numTrimming = 0;
        mNumNonHiddenProcs = 0;
        mNumHiddenProcs = 0;
        int i = mLruProcesses.size();
        // 优先级
        int curHiddenAdj = ProcessList.HIDDEN_APP_MIN_ADJ;
        // 初始化的一些值
        int nextHiddenAdj = curHiddenAdj+1;
        // 优先级
        int curEmptyAdj = ProcessList.HIDDEN_APP_MIN_ADJ;
        // 有意思
        int nextEmptyAdj = curEmptyAdj+2;
     

	    for (int i=N-1; i>=0; i--) {
	            ProcessRecord app = mLruProcesses.get(i);
	            if (!app.killedByAm && app.thread != null) {
	                app.procStateChanged = false;
	                <!--关键点4 计算进程的优先级或者缓存进程的优先级->   
	                // computeOomAdjLocked计算进程优先级，但是对于后台进程和empty进程computeOomAdjLocked无效，这部分优先级是AMS自己根据LRU原则分配的
	                computeOomAdjLocked(app, ProcessList.UNKNOWN_ADJ, TOP_APP, true, now);
	                //还未最终确认，有些进程的优先级，比如只有后台activity或者没有activity的进程，
	              <!--关键点5 计算进程的优先级或者缓存进程的优先级->   
	                if (app.curAdj >= ProcessList.UNKNOWN_ADJ) {
	                    switch (app.curProcState) {
	                        case ActivityManager.PROCESS_STATE_CACHED_ACTIVITY:
	                        case ActivityManager.PROCESS_STATE_CACHED_ACTIVITY_CLIENT:
	                            app.curRawAdj = curCachedAdj;
										<!--关键点6 根据LRU为后台进程分配优先级-->
	                            if (curCachedAdj != nextCachedAdj) {
	                                stepCached++;
	                                if (stepCached >= cachedFactor) {
	                                    stepCached = 0;
	                                    curCachedAdj = nextCachedAdj;
	                                    nextCachedAdj += 2;
	                                    if (nextCachedAdj > ProcessList.CACHED_APP_MAX_ADJ) {
	                                        nextCachedAdj = ProcessList.CACHED_APP_MAX_ADJ;
	                                    }
	                                }
	                            }
	                            break;
	                        default:
                        									     	<!--关键点7 根据LRU为后台进程分配优先级-->
	                            app.curRawAdj = curEmptyAdj;
	                            app.curAdj = app.modifyRawOomAdj(curEmptyAdj);
	                            if (curEmptyAdj != nextEmptyAdj) {
	                                stepEmpty++;
	                                if (stepEmpty >= emptyFactor) {
	                                    stepEmpty = 0;
	                                    curEmptyAdj = nextEmptyAdj;
	                                    nextEmptyAdj += 2;
	                                    if (nextEmptyAdj > ProcessList.CACHED_APP_MAX_ADJ) {
	                                        nextEmptyAdj = ProcessList.CACHED_APP_MAX_ADJ;
	                                    }
	                                }
	                            }
	                            break;
	                    }
	                }
				    <!--关键点8 设置优先级-->
	                applyOomAdjLocked(app, true, now, nowElapsed);

					 <!--关键点9 根据缓存进程的数由AMS选择性杀进程，后台进程太多-->
	                switch (app.curProcState) {
	                    case ActivityManager.PROCESS_STATE_CACHED_ACTIVITY:
	                    case ActivityManager.PROCESS_STATE_CACHED_ACTIVITY_CLIENT:
	                        mNumCachedHiddenProcs++;
	                        numCached++;
	                        if (numCached > cachedProcessLimit) {
	                            app.kill("cached #" + numCached, true);
	                        }
	                        break;
	                    case ActivityManager.PROCESS_STATE_CACHED_EMPTY:
	                        if (numEmpty > ProcessList.TRIM_EMPTY_APPS
	                                && app.lastActivityTime < oldTime) {
	                            app.kill("empty for "
	                                    + ((oldTime + ProcessList.MAX_EMPTY_TIME - app.lastActivityTime)
	                                    / 1000) + "s", true);
	                        } else {
	                            numEmpty++;
	                            if (numEmpty > emptyProcessLimit) {
	                                app.kill("empty #" + numEmpty, true);
	                            }
	                        }
	                        break;
	                    default:
	                        mNumNonCachedProcs++;
	                        break;
	                }
					 <!--关键点10 计算需要裁剪进程的数目-->
	                if (app.curProcState >= ActivityManager.PROCESS_STATE_HOME
	                        && !app.killedByAm) {
	                		// 比home高的都需要裁剪，不包括那些等级高的进程
	                    numTrimming++;
	                }
	            }
	        }

 
	        final int numCachedAndEmpty = numCached + numEmpty;
	        int memFactor;
			 <!--关键点11 根据后台进程数目确定当前系统的内存使用状况 ，确立内存裁剪等级（内存因子）memFactor，android的理念是准许存在一定数量的后台进程，并且只有内存不够的时候，才会缩减后台进程-->
	        if (numCached <= ProcessList.TRIM_CACHED_APPS
	                && numEmpty <= ProcessList.TRIM_EMPTY_APPS) {
	 	     	// 等级高低 ，杀的越厉害，越少，需要约紧急的时候才杀
	            if (numCachedAndEmpty <= ProcessList.TRIM_CRITICAL_THRESHOLD) {//3
	                memFactor = ProcessStats.ADJ_MEM_FACTOR_CRITICAL;
	            } else if (numCachedAndEmpty <= ProcessList.TRIM_LOW_THRESHOLD) { //5
	                memFactor = ProcessStats.ADJ_MEM_FACTOR_LOW;
	            } else {
	                memFactor = ProcessStats.ADJ_MEM_FACTOR_MODERATE;
	            }
	        } else {
	        	// 后台进程数量足够说明内存充足
	            memFactor = ProcessStats.ADJ_MEM_FACTOR_NORMAL;
	        }
	
	        <!--关键点12 根据内存裁剪等级裁剪内存 Android认为后台进程不足的时候，内存也不足-->
	        if (memFactor != ProcessStats.ADJ_MEM_FACTOR_NORMAL) {
	            if (mLowRamStartTime == 0) {
	                mLowRamStartTime = now;
	            }
	            int step = 0;
	            int fgTrimLevel;
	         // 内存不足的时候，也要通知前台或可见进程进行缩减
	            switch (memFactor) {
	                case ProcessStats.ADJ_MEM_FACTOR_CRITICAL:
	                    fgTrimLevel = ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL;
	                    break;
	                case ProcessStats.ADJ_MEM_FACTOR_LOW:
	                    fgTrimLevel = ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW;
	                    break;
	                default:
	                    fgTrimLevel = ComponentCallbacks2.TRIM_MEMORY_RUNNING_MODERATE;
	                    break;
	            }
	            int factor = numTrimming/3;
	            int minFactor = 2;
	            if (mHomeProcess != null) minFactor++;
	            if (mPreviousProcess != null) minFactor++;
	            if (factor < minFactor) factor = minFactor;
	            int curLevel = ComponentCallbacks2.TRIM_MEMORY_COMPLETE;
	            <!--裁剪后台进程-->
	            for (int i=N-1; i>=0; i--) {
	                ProcessRecord app = mLruProcesses.get(i);
	                if (allChanged || app.procStateChanged) {
	                    setProcessTrackerStateLocked(app, trackerMemFactor, now);
	                    app.procStateChanged = false;
	                }
	                
	       			//  PROCESS_STATE_HOME = 12;  
	       			//PROCESS_STATE_LAST_ACTIVITY = 13; 退到后台的就会用
	                // 优先级比较低，回收等级比较高ComponentCallbacks2.TRIM_MEMORY_COMPLETE
	                //  当curProcState > 12且没有被am杀掉的情况；上面的update的时候，在kill的时候，是会设置app.killedByAm的
	                //裁剪的话，如果 >= ActivityManager.PROCESS_STATE_HOME，老的裁剪等级较高，不重要，越新鲜的进程，裁剪等级越低
	
	                if (app.curProcState >= ActivityManager.PROCESS_STATE_HOME
	                        && !app.killedByAm) {
	               		 // 先清理最陈旧的 ，最陈旧的那个遭殃
	                    if (app.trimMemoryLevel < curLevel && app.thread != null) {
	                        try {
	                            app.thread.scheduleTrimMemory(curLevel);
	                        } catch (RemoteException e) {
	                        }
	                    }
	                    app.trimMemoryLevel = curLevel;
	                    step++; 
	                    // 反正一共就三个槽，将来再次刷新的 时候，要看看是不是从一个槽里面移动到另一个槽，
	                    // 没有移动，就不需要再次裁剪，等级没变
	                    if (step >= factor) {
	                        step = 0;
	                        switch (curLevel) {
	                            case ComponentCallbacks2.TRIM_MEMORY_COMPLETE:
	                                curLevel = ComponentCallbacks2.TRIM_MEMORY_MODERATE;
	                                break;
	                            case ComponentCallbacks2.TRIM_MEMORY_MODERATE:
	                                curLevel = ComponentCallbacks2.TRIM_MEMORY_BACKGROUND;
	                                break;
	                        }
	                    }
	                } else if (app.curProcState == ActivityManager.PROCESS_STATE_HEAVY_WEIGHT) {
	                    if (app.trimMemoryLevel < ComponentCallbacks2.TRIM_MEMORY_BACKGROUND
	                            && app.thread != null) {
	                        try {
	                            app.thread.scheduleTrimMemory(
	                                    ComponentCallbacks2.TRIM_MEMORY_BACKGROUND);
	                        } catch (RemoteException e) {
	                        }
	                    }
	                    app.trimMemoryLevel = ComponentCallbacks2.TRIM_MEMORY_BACKGROUND;
	                } else {
	                    if ((app.curProcState >= ActivityManager.PROCESS_STATE_IMPORTANT_BACKGROUND
	                            || app.systemNoUi) && app.pendingUiClean) {
	                        // 释放UI
	                        final int level = ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN;
	                        if (app.trimMemoryLevel < level && app.thread != null) {
	                            try {
	                                app.thread.scheduleTrimMemory(level);
	                            } catch (RemoteException e) {
	                            }
	                        }
	                        app.pendingUiClean = false;
	                    }
						// 启动的时候会回调一遍，如果有必要，启动APP的时候，app.trimMemoryLevel=0
	                    if (app.trimMemoryLevel < fgTrimLevel && app.thread != null) {
	                        try {
	                            app.thread.scheduleTrimMemory(fgTrimLevel);
	                        } catch (RemoteException e) {
	                        }
	                    }
	                    app.trimMemoryLevel = fgTrimLevel;
	                }
	            }
	        } else {
	        	  <!--关键点13 内存充足的时候，进程的裁剪-->
	             ... 
	            for (int i=N-1; i>=0; i--) {
	                ProcessRecord app = mLruProcesses.get(i);
	                // 在resume的时候，都是设置成true，所以退回后台的时候app.pendingUiClean==true是满足的，
	                // 因此缩减一次，但是不会再次走这里的分支缩减即使优先级变化，但是已经缩减过
	                // 除非走上面的后台流程，那个时候这个进程的等级已经很低了，
	                if ((app.curProcState >= ActivityManager.PROCESS_STATE_IMPORTANT_BACKGROUND
	                        || app.systemNoUi) && app.pendingUiClean) {
	                    if (app.trimMemoryLevel < ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN
	                            && app.thread != null) {
	                        try {
	                            app.thread.scheduleTrimMemory(
	                                    ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN);
	                        } catch (RemoteException e) {
	                        }
	                    }
	                    // clean一次就弄成false
	                    app.pendingUiClean = false;
	                }
	                // 基本算没怎么裁剪
	                app.trimMemoryLevel = 0;
	            }
	        }
	 }

 
	  
 app.thread.scheduleTrimMemory(curLevel);
 
   
OnLowMemory()和OnTrimMemory()的比较

* OnLowMemory被回调时，已经没有后台进程；而onTrimMemory被回调时，还有后台进程。
* OnLowMemory是在最后一个后台进程被杀时调用，一般情况是low memory killer 杀进程后触发；而OnTrimMemory的触发更频繁， 每次计算进程优先级时，只要满足条件，都会触发。
* 通过一键清理后，OnLowMemory不会被触发，而OnTrimMemory会被触发一次。



# 通过“流氓”手段提高oom_adj，降低被杀风险，化身流氓进程

进程优先级的计算Android是有自己的一条准则的，某些特殊场景的需要额外处理进程的oom_adj Android也是给了参考方案的。但是，那对于流氓来说，并没有任何约束效力。 "流氓"仍然能够参照oom_adj（优先级）的计算规则，利用其漏洞，提高进程的oom_adj，以降低被杀的风险。如果单单降低被杀风险还好，就怕那种即不想死，又想占用资源的APP，累积下去就会导致系统内存不足，导致整个系统卡顿。

优先级的计算逻辑比较复杂，这里只简述非缓存进程，因为一旦沦为缓存进程，其优先级就只能依靠LRU来计算，不可控。而流氓是不会让自己沦为缓存进程的，非缓存进程是以下进程中的一种，并且，优先级越高（数值越小），越不易被杀死：

| ADJ优先级     | 优先级          | 进程类型 |
| ------------- |:-------------:| :-----|
| SERVICE_ADJ |   5   |    服务进程(Service process)  |
| HEAVY_WEIGHT_APP_ADJ |   4   |  后台的重量级进程，system/rootdir/init.rc文件中设置    |
| BACKUP_APP_ADJ |   3   |   备份进程（这个不太了解）   |
| PERCEPTIBLE_APP_ADJ |    2  |    可感知进程，比如后台音乐播放  |
| VISIBLE_APP_ADJ |  1    |   可见进程(可见，但是没能获取焦点，比如新进程仅有一个悬浮Activity，Visible process)   |
| FOREGROUND_APP_ADJ |   0   |     前台进程（正在展示是APP，存在交互界面，Foreground process）  |


	private final int computeOomAdjLocked(ProcessRecord app, int cachedAdj, ProcessRecord TOP_APP,
	        boolean doingAll, long now) {
	    //之前的博客中提到过，updateOomAdjLocked函数每次更新oom_adj时，都会分配一个序号
	    //此处就是根据序号判断是否已经处理过命令
	    if (mAdjSeq == app.adjSeq) {
	        // This adjustment has already been computed.
	        return app.curRawAdj;
	    }
	
	    //ProcessRecord对应的ActivityThread不存在了
	    //修改其中的一些变量，此时的oom_adj为CACHED_APP_MAX_ADJ，
	    //其意义我们在前一篇博客中已经提到过
	    if (app.thread == null) {
	        app.adjSeq = mAdjSeq;
	        app.curSchedGroup = ProcessList.SCHED_GROUP_BACKGROUND;
	        app.curProcState = ActivityManager.PROCESS_STATE_CACHED_EMPTY;
	        return (app.curAdj=app.curRawAdj=ProcessList.CACHED_APP_MAX_ADJ);
	    }
	
	    //初始化一些变量
	    //这些变量的具体用途，在篇博客中我们不关注
	    //大家只用留意一下ProcessRecord的schedGroup、procState和oom_adj即可
	    app.adjTypeCode = ActivityManager.RunningAppProcessInfo.REASON_UNKNOWN;
	    app.adjSource = null;
	    app.adjTarget = null;
	    app.empty = false;
	    app.cached = false;
	
	    final int activitiesSize = app.activities.size();
	
	    //这个判断没啥意义，ProcessRecord中只有初始化时为maxAdj赋值
	    //maxAdj取值为UNKNOWN_ADJ，即最大的1001
	    if (app.maxAdj <= ProcessList.FOREGROUND_APP_ADJ) {
	        //这部分代码就是修改app的curSchedGroup，并将oom_adj设置为maxAdj
	        //实际过程中，应该是不会执行的的
	        ......................
	    }
	
	    //保存当前TOP Activity的状态
	    final int PROCESS_STATE_CUR_TOP = mTopProcessState;
	    ......................
	}
	
	.................
	// Determine the importance of the process, starting with most
	// important to least, and assign an appropriate OOM adjustment.
	// 上面的这段注释为整个computeOomAdjLocked函数“代言”
	
	int adj;
	int schedGroup;
	int procState;
	boolean foregroundActivities = false;
	BroadcastQueue queue;
	
	//若进程包含正在前台显示的Activity
	if (app == TOP_APP) {
	    // The last app on the list is the foreground app.
	    adj = ProcessList.FOREGROUND_APP_ADJ;
	
	    //单独的一种schedGroup
	    schedGroup = ProcessList.SCHED_GROUP_TOP_APP;
	    app.adjType = "top-activity";
	
	    //当前处理的是包含前台Activity的进程时，才会将该值置为true
	    foregroundActivities = true;
	    procState = PROCESS_STATE_CUR_TOP;
	} else if (app.instrumentationClass != null) {
	    //处理正在进行测试的进程
	
	    // Don't want to kill running instrumentation.
	    adj = ProcessList.FOREGROUND_APP_ADJ;
	    schedGroup = ProcessList.SCHED_GROUP_DEFAULT;
	
	    app.adjType = "instrumentation";
	    procState = ActivityManager.PROCESS_STATE_FOREGROUND_SERVICE;
	} else if ((queue = isReceivingBroadcast(app)) != null) {
	    //处理正在处理广播的进程
	
	    // An app that is currently receiving a broadcast also
	    // counts as being in the foreground for OOM killer purposes.
	    // It's placed in a sched group based on the nature of the
	    // broadcast as reflected by which queue it's active in.
	    adj = ProcessList.FOREGROUND_APP_ADJ;
	
	    //根据处理广播的Queue，决定调度策略
	    schedGroup = (queue == mFgBroadcastQueue)
	            ? ProcessList.SCHED_GROUP_DEFAULT : ProcessList.SCHED_GROUP_BACKGROUND;
	
	    app.adjType = "broadcast";
	    procState = ActivityManager.PROCESS_STATE_RECEIVER;
	} else if (app.executingServices.size() > 0) {
	    //处理Service正在运行的进程
	
	    // An app that is currently executing a service callback also
	    // counts as being in the foreground.
	    adj = ProcessList.FOREGROUND_APP_ADJ;
	
	    schedGroup = app.execServicesFg ?
	            ProcessList.SCHED_GROUP_DEFAULT : ProcessList.SCHED_GROUP_BACKGROUND;
	
	    procState = ActivityManager.PROCESS_STATE_SERVICE;
	} else {
	    //其它进程，在后续过程中再进一步处理
	    // As far as we know the process is empty.  We may change our mind later.
	    schedGroup = ProcessList.SCHED_GROUP_BACKGROUND;
	
	    // At this point we don't actually know the adjustment.  Use the cached adj
	    // value that the caller wants us to.
	    // 先将adj临时赋值为cachedAdj，即参数传入的UNKNOW_ADJ
	    adj = cachedAdj;
	    procState = ActivityManager.PROCESS_STATE_CACHED_EMPTY;
	
	    app.cached = true;
	    app.empty = true;
	    app.adjType = "cch-empty";
	}
	..................
	
	....................
	    //如果该Service还被客户端Bounded，即是Bounded Service时
	    for (int conni = s.connections.size()-1;
	            conni >= 0 && (adj > ProcessList.FOREGROUND_APP_ADJ
	                    || schedGroup == ProcessList.SCHED_GROUP_BACKGROUND
	                    || procState > ActivityManager.PROCESS_STATE_TOP);
	            conni--) {
	        ArrayList<ConnectionRecord> clist = s.connections.valueAt(conni);
	
	        //客户端可以通过一个Connection以不同的参数绑定Service
	        //因此，一个Service可以对应多个Connection，一个Connection又对应多个ConnectionRecord
	        //这里依次处理每一个ConnectionRecord
	        for (int i = 0;
	                i < clist.size() && (adj > ProcessList.FOREGROUND_APP_ADJ
	                        || schedGroup == ProcessList.SCHED_GROUP_BACKGROUND
	                        || procState > ActivityManager.PROCESS_STATE_TOP);
	                i++) {
	            ConnectionRecord cr = clist.get(i);
	
	            if (cr.binding.client == app) {
	                // Binding to ourself is not interesting.
	                continue;
	            }
	
	            //当BIND_WAIVE_PRIORITY为1时，客户端就不会影响服务端
	            //if中的流程就可以略去；否则，客户端就会影响服务端
	            if ((cr.flags&Context.BIND_WAIVE_PRIORITY) == 0) {
	                ProcessRecord client = cr.binding.client;
	
	                //计算出客户端进程的oom_adj
	                //由此可看出Android oom_adj的计算多么麻烦
	                //要是客户端进程中，又有个服务进程被绑定，那么将再计算其客户端进程的oom_adj？！
	                int clientAdj = computeOomAdjLocked(client, cachedAdj,
	                        TOP_APP, doingAll, now);
	
	                int clientProcState = client.curProcState;
	                if (clientProcState >= ActivityManager.PROCESS_STATE_CACHED_ACTIVITY) {
	                    // If the other app is cached for any reason, for purposes here
	                    // we are going to consider it empty.  The specific cached state
	                    // doesn't propagate except under certain conditions.
	                    clientProcState = ActivityManager.PROCESS_STATE_CACHED_EMPTY;
	                }
	
	                String adjType = null;
	
	                //BIND_ALLOW_OOM_MANAGEMENT置为1时，先按照通常的处理方式，调整服务端进程的adjType
	                if ((cr.flags&Context.BIND_ALLOW_OOM_MANAGEMENT) != 0) {
	                    //与前面分析Unbounded Service基本一致，若进程显示过UI或Service超时
	                    //会将clientAdj修改为当前进程的adj，即不需要考虑客户端进程了
	                    if (app.hasShownUi && app != mHomeProcess) {
	                        if (adj > clientAdj) {
	                            adjType = "cch-bound-ui-services";
	                        }
	                        app.cached = false;
	                        clientAdj = adj;
	                        clientProcState = procState;
	                    } else {
	                        if (now >= (s.lastActivity
	                                + ActiveServices.MAX_SERVICE_INACTIVITY)) {
	                            if (adj > clientAdj) {
	                                adjType = "cch-bound-services";
	                            }
	                            clientAdj = adj;
	                        }
	                    }
	                }
	
	                //根据情况，按照clientAdj调整当前进程的adj
	                if (adj > clientAdj) {
	                    // If this process has recently shown UI, and
	                    // the process that is binding to it is less
	                    // important than being visible, then we don't
	                    // care about the binding as much as we care
	                    // about letting this process get into the LRU
	                    // list to be killed and restarted if needed for
	                    // memory.
	                    // 上面的注释很清楚
	                    if (app.hasShownUi && app != mHomeProcess
	                            && clientAdj > ProcessList.PERCEPTIBLE_APP_ADJ) {
	                        adjType = "cch-bound-ui-services";
	                    } else {
	                        //以下的流程表明，client和flag将同时影响Service进程的adj
	
	                        if ((cr.flags&(Context.BIND_ABOVE_CLIENT
	                                |Context.BIND_IMPORTANT)) != 0) {
	                            //从这里再次可以看出，Service重要性小于等于Client
	                            adj = clientAdj >= ProcessList.PERSISTENT_SERVICE_ADJ
	                                    ? clientAdj : ProcessList.PERSISTENT_SERVICE_ADJ;
	
	                        //BIND_NOT_VISIBLE表示不将服务端当作visible进程看待
	                        //于是，即使客户端的adj小于PERCEPTIBLE_APP_ADJ，service也只能取到PERCEPTIBLE_APP_ADJ
	                        } else if ((cr.flags&Context.BIND_NOT_VISIBLE) != 0
	                                && clientAdj < ProcessList.PERCEPTIBLE_APP_ADJ
	                                && adj > ProcessList.PERCEPTIBLE_APP_ADJ) {
	                            adj = ProcessList.PERCEPTIBLE_APP_ADJ;
	                        } else if (clientAdj >= ProcessList.PERCEPTIBLE_APP_ADJ) {
	                            adj = clientAdj;
	                        } else {
	                            if (adj > ProcessList.VISIBLE_APP_ADJ) {
	                                adj = Math.max(clientAdj, ProcessList.VISIBLE_APP_ADJ);
	                            }
	                        }
	
	                        if (!client.cached) {
	                            app.cached = false;
	                        }
	                        adjType = "service";
	                    }
	                }
	
	                if ((cr.flags&Context.BIND_NOT_FOREGROUND) == 0) {
	                    //进一步更具client调整当前进程的procState、schedGroup等
	                    ...................
	                } else {
	                    ...................
	                }
	                .................
	                if (procState > clientProcState) {
	                    procState = clientProcState;
	                }
	                //其它参数的赋值
	                .................
	            } 
	
	            if ((cr.flags&Context.BIND_TREAT_LIKE_ACTIVITY) != 0) {
	                app.treatLikeActivity = true;
	            }
	
	            //取出ConnectionRecord所在的Activity
	            final ActivityRecord a = cr.activity;
	
	            //BIND_ADJUST_WITH_ACTIVITY值为1时，表示服务端可以根据客户端Activity的oom_adj作出相应的调整
	            if ((cr.flags&Context.BIND_ADJUST_WITH_ACTIVITY) != 0) {
	                if (a != null && adj > ProcessList.FOREGROUND_APP_ADJ &&
	                        (a.visible || a.state == ActivityState.RESUMED ||
	                                a.state == ActivityState.PAUSING)) {
	                //BIND_ADJUST_WITH_ACTIVITY置为1，且绑定的activity可见或在前台时，
	                //Service进程的oom_adj可以变为FOREGROUND_APP_ADJ
	                adj = ProcessList.FOREGROUND_APP_ADJ;
	
	                //BIND_NOT_FOREGROUND为0时，才准许调整Service进程的调度优先级
	                if ((cr.flags&Context.BIND_NOT_FOREGROUND) == 0) {
	                    if ((cr.flags&Context.BIND_IMPORTANT) != 0) {
	                        schedGroup = ProcessList.SCHED_GROUP_TOP_APP;
	                    } else {
	                        schedGroup = ProcessList.SCHED_GROUP_DEFAULT;
	                    }
	                }
	
	                //改变其它参数
	                app.cached = false;
	                app.adjType = "service";
	                app.adjTypeCode = ActivityManager.RunningAppProcessInfo
	                        .REASON_SERVICE_IN_USE;
	                app.adjSource = a;
	                app.adjSourceProcState = procState;
	                app.adjTarget = s.name;
	            }
	        }
	    }
	}
	....................
		....................
	//依次处理进程中的ContentProvider
	for (int provi = app.pubProviders.size()-1;
	                provi >= 0 && (adj > ProcessList.FOREGROUND_APP_ADJ
	                        || schedGroup == ProcessList.SCHED_GROUP_BACKGROUND
	                        || procState > ActivityManager.PROCESS_STATE_TOP);
	                provi--) {
	    ContentProviderRecord cpr = app.pubProviders.valueAt(provi);
	
	    //依次处理ContentProvider的客户端
	    for (int i = cpr.connections.size()-1;
	            i >= 0 && (adj > ProcessList.FOREGROUND_APP_ADJ
	                    || schedGroup == ProcessList.SCHED_GROUP_BACKGROUND
	                    || procState > ActivityManager.PROCESS_STATE_TOP);
	            i--) {
	        ContentProviderConnection conn = cpr.connections.get(i);
	
	        ProcessRecord client = conn.client;
	        if (client == app) {
	            // Being our own client is not interesting.
	            continue;
	        }
	
	        //计算客户端的oom_adj
	        int clientAdj = computeOomAdjLocked(client, cachedAdj, TOP_APP, doingAll, now);
	        int clientProcState = client.curProcState;
	        if (clientProcState >= ActivityManager.PROCESS_STATE_CACHED_ACTIVITY) {
	            // If the other app is cached for any reason, for purposes here
	            // we are going to consider it empty.
	            clientProcState = ActivityManager.PROCESS_STATE_CACHED_EMPTY;
	        }
	
	        //与Unbounded Service的处理基本类似
	        if (adj > clientAdj) {
	            if (app.hasShownUi && app != mHomeProcess
	                    && clientAdj > ProcessList.PERCEPTIBLE_APP_ADJ) {
	                app.adjType = "cch-ui-provider";
	            } else {
	                //根据clientAdj，调整当前进程的adj
	                adj = clientAdj > ProcessList.FOREGROUND_APP_ADJ
	                        ? clientAdj : ProcessList.FOREGROUND_APP_ADJ;
	                        app.adjType = "provider";
	            }
	
	            //调整其它变量
	            app.cached &= client.cached;
	            app.adjTypeCode = ActivityManager.RunningAppProcessInfo
	                    .REASON_PROVIDER_IN_USE;
	            app.adjSource = client;
	            app.adjSourceProcState = clientProcState;
	            app.adjTarget = cpr.name;
	        }
	
	        //进一步调整调度策略和procState
	        ....................
	
	        //特殊情况的处理
	        // If the provider has external (non-framework) process
	        // dependencies, ensure that its adjustment is at least
	        // FOREGROUND_APP_ADJ.
	        if (cpr.hasExternalProcessHandles()) {
	            if (adj > ProcessList.FOREGROUND_APP_ADJ) {
	                adj = ProcessList.FOREGROUND_APP_ADJ;
	                schedGroup = ProcessList.SCHED_GROUP_DEFAULT;
	                app.cached = false;
	                app.adjType = "provider";
	                app.adjTarget = cpr.name;
	            }
	            if (procState > ActivityManager.PROCESS_STATE_IMPORTANT_FOREGROUND) {
	                procState = ActivityManager.PROCESS_STATE_IMPORTANT_FOREGROUND;
	            }
	        }
	    }
	}
	
	//如果进程之前运行过ContentProvider，同时ContentProvider的存活时间没有超时
	//那么进程的adj可以变为PREVIOUS_APP_ADJ
	if (app.lastProviderTime > 0 && (app.lastProviderTime+CONTENT_PROVIDER_RETAIN_TIME) > now) {
	    if (adj > ProcessList.PREVIOUS_APP_ADJ) {
	        adj = ProcessList.PREVIOUS_APP_ADJ;
	        schedGroup = ProcessList.SCHED_GROUP_BACKGROUND;
	
	        app.cached = false;
	        app.adjType = "provider";
	    }
	    if (procState > ActivityManager.PROCESS_STATE_LAST_ACTIVITY) {
	        procState = ActivityManager.PROCESS_STATE_LAST_ACTIVITY;
	    }
	}
	....................
	
	//根据进程信息，进一步调整procState
	...................
	
	//对Service进程做一些特殊处理
	if (adj == ProcessList.SERVICE_ADJ) {
	    if (doingAll) {
	        //每次updateOomAdj时，将mNewNumAServiceProcs置为0
	        //然后LRU list中，从后往前数，前1/3的service进程就是AService
	        //其余的就是bService
	        //mNumServiceProcs为上一次update时，service进程的数量
	        app.serviceb = mNewNumAServiceProcs > (mNumServiceProcs/3);
	
	        //记录这一次update后，service进程的数量
	        //update完毕后，该值将赋给mNumServiceProcs
	        mNewNumServiceProcs++;
	        .............
	        if (!app.serviceb) {
	            // This service isn't far enough down on the LRU list to
	            // normally be a B service, but if we are low on RAM and it
	            // is large we want to force it down since we would prefer to
	            // keep launcher over it.
	            // 如果不是bService，但内存回收等级过高，也被视为bService
	            if (mLastMemoryLevel > ProcessStats.ADJ_MEM_FACTOR_NORMAL
	                    && app.lastPss >= mProcessList.getCachedRestoreThresholdKb()) {
	                app.serviceHighRam = true;
	                app.serviceb = true;
	                ................
	            } else {
	                //LRU中后1/3的Service，都是AService
	                mNewNumAServiceProcs++;
	                ............
	            }
	        } else {
	            app.serviceHighRam = false;
	        }
	    }
	    //将bService的oom_adj调整为SERVICE_B_ADJ
	    if (app.serviceb) {
	        adj = ProcessList.SERVICE_B_ADJ;
	    }
	}
	
	//计算完毕
	app.curRawAdj = adj;
	
	.............
	//if基本没有用，maxAdj已经是最大的UNKNOW_ADJ
	if (adj > app.maxAdj) {
	    adj = app.maxAdj;
	    if (app.maxAdj <= ProcessList.PERCEPTIBLE_APP_ADJ) {
	        schedGroup = ProcessList.SCHED_GROUP_DEFAULT;
	    }
	}
	
	//最后做一些记录和调整
	.............
	
	return app.curRawAdj;


# 对于多进程的APP

        <activity
            android:name=".activity.MainActivity"
            android:label="@string/app_name"
            android:theme="@style/AppTheme.NoActionBar">
            <intent-filter>
                <action android:name="android.intent.action.MAIN"/>

                <category android:name="android.intent.category.LAUNCHER"/>
            </intent-filter>
        </activity>

        <service android:name=".service.RogueBackGroundService"
            />
        <service android:name=".service.RogueBackGroundService$RogueIntentService"/>

        <activity
            android:name=".activity.ViewPagerFragmentAdapterActivity"
            android:process=":ViewPagerFragmentAdapterActivity"/>
            
只有当前显示Activity的进程优先级是0

# AMS怎么杀 AMS会杀死进程吗？

* APP在后台
* APP在前台
* APP在杀死的时候，怎么回调
* 进程过多杀

AMS如果后台进程的数量过多，AMS会杀死一些后台进程。


# 设定优先级，是否成功失败


# 如何处理保活

1、是否只是改变自身的优先级
2、是否引起其他有限级的改变
3、后台的优先级变化，退回后台引起整个列表的变化吗？，还是只是几个


# 进程保活

全集中在一个函数中  final void updateOomAdjLocked() ，这个函数先计算优先级，再清理，再瘦身

# 优先级改变杀
 

# 进程保活 

前面讲到，后台杀死的原理，假如进程进入后台，系统就不管了了？知道内存不够才去回收，当然不是，总要提前警告一次，才能抄家伙，上来就杀，太不讲人情，Android也是如此，先给App一个悔过的机会，让APP瘦身。

两个入口：要根据自己的场景判断

    @Override
    public void onTerminate() {
        super.onTerminate();
     }

    @Override
    public void onLowMemory() {
        super.onLowMemory();
     }



    
 
# onLowMemory的执行时机，杀干净了后台进程，通知前台

void onLowMemory ()

This is called when the overall system is running low on memory, and actively running processes should trim their memory usage. While the exact point at which this will be called is not defined, generally it will happen when all background process have been killed. That is, before reaching the point of killing processes hosting service and foreground UI that we would like to avoid killing.

You should implement this method to release any caches or other unnecessary resources you may be holding on to. The system will perform a garbage collection for you after returning from this method.

Preferably, you should implement onTrimMemory(int) from ComponentCallbacks2 to incrementally unload your resources based on various levels of memory demands. That API is available for API level 14 and higher, so you should only use this onLowMemory() method as a fallback for older versions, which can be treated the same as onTrimMemory(int) with the TRIM_MEMORY_COMPLETE level.


是否给一个自我瘦身的机会，杀鸡儆猴，如果你是那只鸡，那就没办法了！onLowMemory是在杀死所有后台进程的时候，给前台进程回调用的，该杀的都杀了，如果你再不释放资源，并且内存还是不够的话，就别怪连前台进程也杀掉。 

	scheduleAppGcsLocked
	
	performAppGcsIfAppropriateLocked
	
	performAppGcsLocked

    /**
     * Ask a given process to GC right now.
     */
    final void performAppGcLocked(ProcessRecord app) {
        try {
            app.lastRequestedGc = SystemClock.uptimeMillis();
            if (app.thread != null) {
                if (app.reportLowMemory) {
                    app.reportLowMemory = false;
                    app.thread.scheduleLowMemory();
                } else {
                    app.thread.processInBackground();
                }
            }
        } catch (Exception e) {
            // whatever.
        }
    }
    
这时候会回调APP的scheduleLowMemory，提供一个瘦身的机会减少内存

Runtime.getRuntime().gc();
	        
	        

# 什么时候回到onLowmemory    -app.reportLowMemory==true，所有的后台进程，都被干掉的情况下


	
	 final void appDiedLocked(ProcessRecord app, int pid,
	            IApplicationThread thread) {
	
	        mProcDeaths[0]++;
	        
	        BatteryStatsImpl stats = mBatteryStatsService.getActiveStatistics();
	        synchronized (stats) {
	            stats.noteProcessDiedLocked(app.info.uid, pid);
	        }
	
	        // Clean up already done if the process has been re-started.
	        // 重启？
	        if (app.pid == pid && app.thread != null &&
	                app.thread.asBinder() == thread.asBinder()) {
	            if (!app.killedBackground) {
	                Slog.i(TAG, "Process " + app.processName + " (pid " + pid
	                        + ") has died.");
	            }
	            EventLog.writeEvent(EventLogTags.AM_PROC_DIED, app.userId, app.pid, app.processName);
	            if (DEBUG_CLEANUP) Slog.v(
	                TAG, "Dying app: " + app + ", pid: " + pid
	                + ", thread: " + thread.asBinder());
	            boolean doLowMem = app.instrumentationClass == null;
	            handleAppDiedLocked(app, false, true);
	            // 是不是因为内存紧张导致的LowmemoryKiller机制生效，杀死的进程
	            if (doLowMem) {
	                // If there are no longer any background processes running,
	                // and the app that died was not running instrumentation,
	                // then tell everyone we are now low on memory.
	
	                // 通知低内存，大家注意，已经有富农被杀了，打土豪分田地的那哥们出来了，自觉的破财消灾吧
	
	                boolean haveBg = false;
	                for (int i=mLruProcesses.size()-1; i>=0; i--) {
	                    ProcessRecord rec = mLruProcesses.get(i);
	                    if (rec.thread != null && rec.setAdj >= ProcessList.HIDDEN_APP_MIN_ADJ) {
	                        haveBg = true;
	                        break;
	                    }
	                }
	                // 是不是所有的后台都被杀死了，如果都被杀死了，通知前台的app，快向组织交钱，包名要紧
	                if (!haveBg) {
	                    EventLog.writeEvent(EventLogTags.AM_LOW_MEMORY, mLruProcesses.size());
	                    long now = SystemClock.uptimeMillis();
	                    for (int i=mLruProcesses.size()-1; i>=0; i--) {
	                        ProcessRecord rec = mLruProcesses.get(i);
	                        if (rec != app && rec.thread != null &&
	                                (rec.lastLowMemory+GC_MIN_INTERVAL) <= now) {
	                            // The low memory report is overriding any current
	                            // state for a GC request.  Make sure to do
	                            // heavy/important/visible/foreground processes first.
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
	                    // gc 
	                    mHandler.sendEmptyMessage(REPORT_MEM_USAGE);
	                    scheduleAppGcsLocked();
	                }
	            }
	        } else if (app.pid != pid) {
	            // A new process has already been started.
	            Slog.i(TAG, "Process " + app.processName + " (pid " + pid
	                    + ") has died and restarted (pid " + app.pid + ").");
	            EventLog.writeEvent(EventLogTags.AM_PROC_DIED, app.userId, app.pid, app.processName);
	        } else if (DEBUG_PROCESSES) {
	            Slog.d(TAG, "Received spurious death notification for thread "
	                    + thread.asBinder());
	        }
	    }
	


    final void performAppGcLocked(ProcessRecord app) {
        try {
            app.lastRequestedGc = SystemClock.uptimeMillis();
            if (app.thread != null) {
                // app.reportLowMemory app虚拟机的内存是不是不够了？？
                if (app.reportLowMemory) {
                    app.reportLowMemory = false;
                    // 这里才真正的调用APP 的onLowmemory
                    app.thread.scheduleLowMemory();
                } else {
                    app.thread.processInBackground();
                }
            }
        } catch (Exception e) {
            // whatever.
        }
    }
    

答案是肯定的AMS，也会有选择的杀死进程，不过跟当前的内存没太大关系，而是根据当前启动APP的数量，比如空的APP过多，或者后台APP过多，都可能引起后台杀死，比如在4.3上，如果后台的APP超过24个一般就会触发AMS杀进程，要么杀空进程，要么杀靠后的隐藏进程。


OnTrimMemory:Android系统从4.0开始还提供了onTrimMemory()的回调，当系统内存达到某些条件的时候，所有正在运行的应用都会收到这个回调， 同时在这个回调里面会传递以下的参数，代表不同的内存使用情况，收到onTrimMemory()回调的时候，需要根据传递的参数类型进行判断， 合理的选择释放自身的一些内存占用，一方面可以提高系统的整体运行流畅度，另外也可以避免自己被系统判断为优先需要杀掉的应用。


# 流氓的进程保活手段

## 双Service，强制前台进程保活

原理：Android 的前台service机制。但该机制的缺陷是通知栏保留了图标。

调用startForeground(ID， new Notification())，发送空的Notification ，一般而言图标则不会显示，不过测试发现，在Lollipop上会显示。可以通过在需要提优先级的service A启动一个Service(可以是InnerService)，两个服务同时startForeground，且绑定同样的 ID。Stop 掉InnerService ，这样通知栏图标即被移除。这方案实际利用了Android前台service的漏洞。微信在评估了国内不少app已经使用后，才进行了部署。其实目标是让大家站同一起跑线上，哪天google 把漏洞堵了，效果也是一样的。注意Service都要在Manifest中注册。优先级提高后，通过AMS的killBackgroundProcesses已经不能把进程杀死了，因为killBackgroundProcesses只会杀死oom_adj大于ProcessList.SERVICE_ADJ的进程，但是通过这种方式我们APP的优先级已经提高到了ProcessList.VISIBLE_APP_ADJ，可谓流氓至极，如果再占据着内存不释放，那就是泼皮无赖了。

	public class RogueBackGroundService extends Service {
	
	    private static int ROGUE_ID = 1;
	
	    @Nullable
	    @Override
	    public IBinder onBind(Intent intent) {
	        return null;
	    }
	
	    @Override
	    public int onStartCommand(Intent intent, int flags, int startId) {
	        return START_STICKY;
	    }
	
	    @Override
	    public void onCreate() {
	        super.onCreate();
	        Intent intent = new Intent(this, RogueIntentService.class);
	        startService(intent);
	        startForeground(ROGUE_ID, new Notification());
	    }
	
	    public static class RogueIntentService extends IntentService {
	
	        //流氓相互唤醒Service
	        public RogueIntentService(String name) {
	            super(name);
	        }
	
	        public RogueIntentService() {
	            super("RogueIntentService");
	        }
	
	        @Override
	        protected void onHandleIntent(Intent intent) {
	
	        }
	
	        @Override
	        public void onCreate() {
	            super.onCreate();
	            startForeground(ROGUE_ID, new Notification());
	        }
	
	        @Override
	        public void onDestroy() {
	            stopForeground(true);//这里不写也没问题，好像会自动停止
	            super.onDestroy();
	            LogUtils.v("onDestroy");
	        }
	    }
	}

## QQ通过添加一个像素

试验了下，并未成功，优先级并未改变，该被杀还是被杀


## 进程独立，（不同等级的进程可以干掉不重要的）
	        
# 退回后后台的时候为何用的裁剪等级是UITRIM_MEMORY_UI_HIDDEN

# 单击HOME键，其实还是应用的切换

# 通知前台Runing的app，通知后台app

# 有Service的时候，看看Service是什么Service，未停止的Service

# 正常的情况下，一般都是UI_HIDEN，如何裁剪呢，一般回到后台，我们可以将UI释放掉，减少内存的占用，因为同样大小的Oom_adj，LMK先杀内存占用大的。

# 如何合理的保活，其实除了根据TRIM参数，还要根据当前的内存占用情况，没达到限制，不一定会杀后台，设定的阈值

 
 
# 这里针对异常杀死的一些需求

1、**异常杀死后，再次打开完全重启（有个网友问的）** 如何判定是后台杀死
2、进程保活


![App操作影响进程优先级](http://upload-images.jianshu.io/upload_images/1460468-dec3e577ea74f0e8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 	        
###  参考文档

[谷歌文档Application ](https://developer.android.com/reference/android/app/Application.html#onLowMemory%28%29)                 
[Android四大组件与进程启动的关系](http://gityuan.com/2016/10/09/app-process-create-2/)     
[Android 7.0 ActivityManagerService(8) 进程管理相关流程分析(2) updateOomAdjLocked](http://blog.csdn.net/gaugamela/article/details/53927724)           
[Android 7.0 ActivityManagerService(9) 进程管理相关流程分析(3) computeOomAdjLocked 精](http://blog.csdn.net/gaugamela/article/details/54176460)      
[Android代码内存优化建议-OnTrimMemory优化 精](http://androidperformance.com/2015/07/20/Android-Performance-Memory-onTrimMemory.html)       
[微信Android客户端后台保活经验分享](http://www.infoq.com/cn/articles/wechat-android-background-keep-alive)      
[按"Home"键回到桌面的过程](http://book.51cto.com/art/201109/291309.htm)       
[Android low memory killer 机制](https://my.oschina.net/wolfcs/blog/288259)           
[应用内存优化之OnLowMemory&OnTrimMemory](http://www.cnblogs.com/xiajf/p/3993599.html)         