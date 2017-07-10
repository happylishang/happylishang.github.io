---
layout: post
title: "Android后台杀死系列之五：实践篇 进程保活-自“裁”或者耍流氓"
category: Android
image: http://upload-images.jianshu.io/upload_images/1460468-dec3e577ea74f0e8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---

本篇文章是后台杀死系列的最后一篇，主要探讨一下进程的保活，Android本身设计的时候是非常善良的，它希望进程在不可见或者其他一些场景下APP要懂得主动释放，可是Android低估了”贪婪“，尤其是很多国产APP，只希望索取来提高自己的性能，不管其他APP或者系统的死活，导致了很严重的资源浪费，这也是Android被iOS诟病的最大原因。本文的保活手段也分两种：遵纪守法的进程保活与流氓手段换来的进程保活。

**声明：坚决反对流氓手段实现进程保活 坚决反对流氓进程保活 坚决反对流氓进程保活 “请告诉产品：无法进入白名单”**

* 正常守法的进程保活：内存裁剪（好学生APP要使用）
* 流氓的进程保活，提高优先级（好学生APP别用）
* 流氓的进程保活，双Service进程相互唤醒（binder讣告原理）（好学生APP别用）

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

OnTrimMemory是在Android 4.0引入的一个回调接口，其主要作用就是通知应用程序在不同的场景下进行自我瘦身，释放内存，降低被后台杀死的风险，提高用户体验，由于目前APP的适配基本是在14之上，所以不必考虑兼容问题。在APP中可以在Application或者Activity中直接覆盖OnTrimMemory函数以响应系统号召：

      public class LabApplication extends Application {
         @Override
           public void onTrimMemory(int level) {
             super.onTrimMemory(level);
             //根据level裁减内存
              }
        }


onTrimeMemory支持不同裁剪等级，比如，APP通过HOME建进入后台时，其优先级（oom_adj）就发生变化，从未触发onTrimeMemory回调，这个时候系统给出的裁剪等级一般是TRIM_MEMORY_UI_HIDDEN，意思是，UI已经隐藏，UI相关的、占用内存大的资源就可以释放了，比如大量的图片缓存等，当然，还会有其他很多场景对应不同的裁剪等级。因此，需要弄清楚两个问题：

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

以上抽象的说明了一下Android既定参数的意义，下面看一下onTrimeMemory回调的时机及原理，这里采用6.0的代码分析，因为6.0比之前4.3的代码清晰很多：当用户的操作导致APP优先级发生变化，就会调用updateOomAdjLocked去更新进程的优先级，在更新优先级的时候，会扫描一遍LRU进程列表， 重新计算进程的oom_adj，并且参考当前系统状况去通知进程裁剪内存（这里只是针对Android Java层APP），这次操作一般发生在打开新的Activity界面、退回后台、应用跳转切换等等，updateOomAdjLocked代码大概600多行，比较长，尽量精简后如下，还是比较长，这里拆分成一段段梳理：

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
    
这前三个关键点主要是做了一些准备工作，关键点1 是单独抽离出TOP_APP，因为它比较特殊，系统只有一个前天进程，关键点2主要是根据当前的配置获取后台缓存进程与空进程的数目限制，而关键点3是将后台进程分为三备份，无论是后台进程还是空进程，会间插的均分6个优先级，一个优先级是可以有多个进程的，而且并不一定空进程的优先级小于HIDDEN进程优先级。
         
         
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
	                
上面的这几个关键点主要是为所有进程计算出其优先级oom_adj之类的值，对于非后台进程，比如HOME进程 服务进程，备份进程等都有自己的独特的计算方式，而剩余的后台进程就根据LRU三等分配优先级。

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
	        
上面的两个关键点是看当前后台进程是否过多或者过老，如果存在过多或者过老的后台进程，AMS是有权利杀死他们的。**之后才是我们比较关心的存活进程的裁剪：**
 
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
关键点11这里不太好理解：**Android系统根据后台进程的数目来确定当前系统内存的状况，后台进程越多，越说明内存并不紧张，越少，说明越紧张，回收等级也就越高**，如果后台进程的数目较多，内存裁剪就比较宽松是ProcessStats.ADJ_MEM_FACTOR_NORMAL，如果不足，则再根据缓存数目划分等级。以6.0源码来说：

* 如果后台进程数量（包含空进程）< 3 ，就说明内存非常紧张，内存裁剪因子就是ProcessStats.ADJ_MEM_FACTOR_CRITICAL
* 如果后台进程数量（包含空进程）< 5 ，就说明内存非常紧张，内存裁剪因子就是ProcessStats.ADJ_MEM_FACTOR_LOW
* 如果比上面两个多，但是仍然不足正常的后台数目 ，内存裁剪因子就是ProcessStats.ADJ_MEM_FACTOR_MODERATE

与之对应的关键点12，是确立前台RUNNING进程（也不一定是前台显示）的裁剪等级。  

* ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL; 
*  ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW;  
*  ComponentCallbacks2.TRIM_MEMORY_RUNNING_MODERATE;

之后就真正开始裁剪APP，这里先看后台进程不足的情况的裁剪，这部分相对复杂一些：
	            
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
	                }
	                
上面的这部分是负责  app.curProcState >= ActivityManager.PROCESS_STATE_HOME这部分进程裁剪，这部分主要是后台缓存进程，一般是oom_adj在9-11之间的进程，这部门主要根据LRU确定不同的裁减等级。
              
	                else {
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
	        } 
	        
而这里的裁剪主要是一些优先级较高的进程，其裁剪一般是 ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN ，由于这部分进程比较重要，裁剪等级较低，至于前台进程的裁剪，一般是在启动的时候，这个时候app.pendingUiClean==false，只会裁剪当前进程：
	        
	        else {
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
最后这部分是后台进程数量充足的时候，系统只会针对app.curProcState >= ActivityManager.PROCESS_STATE_IMPORTANT_BACKGROUND的进程进行裁剪，而裁剪等级也较低：ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN，因此根据裁剪等级APP可以大概知道系统当前的内存状况，同时也能知道系统希望自己如何裁剪，之后APP做出相应的瘦身即可。不过，上面的进程裁剪的优先级是完全根据后台进程数量来判断的，但是，不同的ROM可能进行了改造，所以裁剪等级不一定完全准确，比如在开发者模式打开**限制后台进程数量**的选项，限制后台进程数目不超过2个，那么这个时候的裁剪等级就是不太合理的，因为内存可能很充足，但是由于限制了后台进程的数量，导致裁剪等级过高。因此在使用的时候，最好结合裁剪等级与当前内存数量来综合考量。

# 通过“流氓”手段提高oom_adj，降低被杀风险，化身流氓进程

关于进程优先级的计算，Android是有自己的一条准则的，就算某些特殊场景的需要额外处理进程的oom_adj Android也是给了参考方案的。但是，那对于流氓来说，并没有任何约束效力。 "流氓"仍然能够参照oom_adj（优先级）的计算规则，利用其漏洞，提高进程的oom_adj，以降低被杀的风险。如果单单降低被杀风险还好，就怕那种即不想死，又想占用资源的APP，累积下去就会导致系统内存不足，导致整个系统卡顿。

优先级的计算逻辑比较复杂，这里只简述非缓存进程，因为一旦沦为缓存进程，其优先级就只能依靠LRU来计算，不可控。而流氓是不会让自己沦为缓存进程的，非缓存进程是以下进程中的一种，并且，优先级越高（数值越小），越不易被杀死：

| ADJ优先级     | 优先级          | 进程类型 |
| ------------- |:-------------:| :-----|
| SERVICE_ADJ |   5   |    服务进程(Service process)  |
| HEAVY_WEIGHT_APP_ADJ |   4   |  后台的重量级进程，system/rootdir/init.rc文件中设置    |
| BACKUP_APP_ADJ |   3   |   备份进程（这个不太了解）   |
| PERCEPTIBLE_APP_ADJ |    2  |    可感知进程，比如后台音乐播放 ，通过startForeground设置的进程 |
| VISIBLE_APP_ADJ |  1    |   可见进程(可见，但是没能获取焦点，比如新进程仅有一个悬浮Activity，其后面的进程就是Visible process)   |
| FOREGROUND_APP_ADJ |   0   |     前台进程（正在展示是APP，存在交互界面，Foreground process）  |

*  第一种提高到FOREGROUND_APP_ADJ

我们从低到高看：如何让进程编程FOREGROUND_APP_ADJ进程，也就是前台进程，这个没有别的办法，只有TOP activity进程才能是算前台进程。正常的交互逻辑下，这个是无法实现的，锁屏的时候倒是可以启动一个Activity，但是需要屏幕点亮的时候再隐藏，容易被用户感知，得不偿失，所以基本是无解,所以之前传说的QQ通过一个像素来保活的应该不是这种方案，而通过WindowManager往主屏幕添加View的方式也并未阻止进程被杀，到底是否通过一像素实现进程包活，个人还未得到解答，希望能有人解惑。
 
* 第二种，提高到VISIBLE_APP_ADJ或者PERCEPTIBLE_APP_ADJ（不同版本等级可能不同 “4.3 = PERCEPTIBLE_APP_ADJ” 而 “> 5.0 = VISIBLE_APP_ADJ”），就表现形式上看，微博，微等信都可能用到了，而且这种手段的APP一般很难杀死，就算从最近的任务列表删除，其实进程还是没有被杀死，只是杀死了Activity等组件。

先看一下源码中对两种优先级的定义，VISIBLE_APP_ADJ是含有可见但是非交互Activity的进程，PERCEPTIBLE_APP_ADJ是用户可感知的进程，如后台音乐播放等
 
	    // This is a process only hosting components that are perceptible to the
	    // user, and we really want to avoid killing them, but they are not
	    // immediately visible. An example is background music playback.
	    static final int PERCEPTIBLE_APP_ADJ = 2;
	
	    // This is a process only hosting activities that are visible to the
	    // user, so we'd prefer they don't disappear.
	    static final int VISIBLE_APP_ADJ = 1;
    
这种做法是相对温和点的，Android官方曾给过类似的方案，比如音乐播放时后，通过设置前台服务的方式来保活，这里就为流氓进程提供了入口，不过显示一个常住服务会在通知栏上有个运行状态的图标，会被用户感知到。但是Android恰恰还有个漏洞可以把该图标移除，真不知道是不是Google故意的。这里可以参考微信的保活方案：**双Service强制前台进程保活**。

startForeground(ID， new Notification())，可以将Service变成前台服务，所在进程就算退到后台，优先级只会降到PERCEPTIBLE_APP_ADJ或者VISIBLE_APP_ADJ，一般不会被杀掉,Android的有个漏洞，如果两个Service通过同样的ID设置为前台进程，而其一通过stopForeground取消了前台显示，结果是保留一个前台服务，但不在状态栏显示通知，这样就不会被用户感知到耍流氓，这种手段是比较常用的流氓手段。优先级提高后，AMS的killBackgroundProcesses已经不能把进程杀死了，它只会杀死oom_adj大于ProcessList.SERVICE_ADJ的进程，而最近的任务列表也只会清空Activity，无法杀掉进程。 因为后台APP的优先级已经提高到了PERCEPTIBLE_APP_ADJ或ProcessList.VISIBLE_APP_ADJ，可谓流氓至极，如果再占据着内存不释放，那就是泼皮无赖了，**这里有个遗留疑问：startForeground看源码只会提升到PERCEPTIBLE_APP_ADJ，但是在5.0之后的版本提升到了VISIBLE_APP_ADJ，这里看源码，没找到原因，希望有人能解惑**。具体做法如下：

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
	        }
	    }
	}
	
不过这个漏洞在Android7.1之后失效了，因为Google加了一个校验：如果还有Service通过setForeground绑定相同id的Notification，就不能cancelNotification，也就是说还是会显示通知（在通知列表）。

	 private void cancelForegroudNotificationLocked(ServiceRecord r) {
	        if (r.foregroundId != 0) {
	            // First check to see if this app has any other active foreground services
	            // with the same notification ID.  If so, we shouldn't actually cancel it,
	            // because that would wipe away the notification that still needs to be shown
	            // due the other service.
	            ServiceMap sm = getServiceMap(r.userId);
	            if (sm != null) {
	            <!--查看是不是与该ID 通知绑定的Service取消了了前台显示-->
	                for (int i = sm.mServicesByName.size()-1; i >= 0; i--) {
	                    ServiceRecord other = sm.mServicesByName.valueAt(i);
	                    if (other != r && other.foregroundId == r.foregroundId
	                            && other.packageName.equals(r.packageName)) {
	                        // Found one!  Abort the cancel.
	                        <!--如果找到还有显示的Service，直接返回-->
	                        return;
	                    }
	                }
	            }
	            r.cancelNotification();
	        }
	    }
	
# 双Service守护进程保活（这个也很流氓，不过如果不提高优先级（允许被杀），也算稍微良心）

前文我们分析过**Android Binder的讣告机制**：如果Service Binder实体的进程挂掉，系统会向Client发送讣告，而这个讣告系统就给进程保活一个可钻的空子。可以通过两个进程中启动两个binder服务，并且互为C/S，一旦一个进程挂掉，另一个进程就会收到讣告，在收到讣告的时候，唤起被杀进程。逻辑如下下：

![双服务保活.jpg](https://user-gold-cdn.xitu.io/2017/3/20/ff017d20d9dcabcab14f7d0790712061)

首先编写两个binder实体服务PairServiceA ，PairServiceB，并且在onCreate的时候相互绑定，并在onServiceDisconnected收到讣告的时候再次绑定。

	public class PairServiceA extends Service {
	
	    @Nullable
	    @Override
	    public IBinder onBind(Intent intent) {
	        return new AliveBinder();
	    }
	
	    @Override
	    public void onCreate() {
	        super.onCreate();
	        bindService(new Intent(PairServiceA.this, PairServiceB.class), mServiceConnection, BIND_AUTO_CREATE);
	    }
	
	    private ServiceConnection mServiceConnection = new ServiceConnection() {
	        @Override
	        public void onServiceConnected(ComponentName name, IBinder service) {
	
	        }
	
	        @Override
	        public void onServiceDisconnected(ComponentName name) {
	            bindService(new Intent(PairServiceA.this, PairServiceB.class), mServiceConnection, BIND_AUTO_CREATE);
	            ToastUtil.show("bind A");
	        }
	    };

与之配对的B

	public class PairServiceB extends Service {
	
	    @Nullable
	    @Override
	    public IBinder onBind(Intent intent) {
	        return new AliveBinder();
	    }
	
	    @Override
	    public void onCreate() {
	        super.onCreate();
	        bindService(new Intent(PairServiceB.this, PairServiceA.class), mServiceConnection, BIND_AUTO_CREATE);
	    }
	
	    private ServiceConnection mServiceConnection = new ServiceConnection() {
	        @Override
	        public void onServiceConnected(ComponentName name, IBinder service) {
	
	        }
	
	        @Override
	        public void onServiceDisconnected(ComponentName name) {
	            bindService(new Intent(PairServiceB.this, PairServiceA.class), mServiceConnection, BIND_AUTO_CREATE);
	        }
	    };
	}

之后再Manifest中注册，注意要进程分离

        <service android:name=".service.alive.PairServiceA"/>
        <service
            android:name=".service.alive.PairServiceB"
            android:process=":alive"/>
            
之后再Application或者Activity中启动一个Service即可。

    startService(new Intent(MainActivity.this, PairServiceA.class));

这个方案一般都没问题，因为Binder讣告是系统中Binder框架自带的，除非一次性全部杀死所有父子进程，这个没测试过。这种方案虽然无法改变优先级，但是**从最近的任务列表删除的时候，仍然无法杀死该进程**，原因如下：

此时APP内至少两个进程A\B ,并且AB相互通过bindService绑定，此时就是互为客户端，在oom_adj中有这么一种计算逻辑，如果进程A的Service被B通过bind绑定，那么A的优先级可能会受到B的影响，因为在计算A的时候需要先计算B，但是B同样是A的Service，反过来有需要计算A，如果不加额外的判断，就会出现死循环，AMS是通过一个计数来标识的：**mAdjSeq == app.adjSeq**。于是流程就是这样 

* 计算A：发现依赖B，去计算B
* 计算B：发现依赖A，回头计算A
* 计算A：发现A正在计算，直接返回已经计算到一半的A优先级

上面的流程能保证不出现死循环，并且由于A只计算了一半，所以A的很多东西没有更新，所以B拿到的A就是之前的数值，比如 curProcState、curSchedGroup:

    private final int computeOomAdjLocked(ProcessRecord app, int cachedAdj, ProcessRecord TOP_APP,
            boolean doingAll, long now) {
        if (mAdjSeq == app.adjSeq) {
            // This adjustment has already been computed.
            return app.curRawAdj;
        }
        ....
          for (int is = app.services.size()-1;
                is >= 0 && (adj > ProcessList.FOREGROUND_APP_ADJ
                        || schedGroup == Process.THREAD_GROUP_BG_NONINTERACTIVE
                        || procState > ActivityManager.PROCESS_STATE_TOP);
                is--) {
            ServiceRecord s = app.services.valueAt(is);
           ...
            for (int conni = s.connections.size()-1;
                    conni >= 0 && (adj > ProcessList.FOREGROUND_APP_ADJ
                            || schedGroup == Process.THREAD_GROUP_BG_NONINTERACTIVE
                            || procState > ActivityManager.PROCESS_STATE_TOP);
                    conni--) {
                ArrayList<ConnectionRecord> clist = s.connections.valueAt(conni);
                for (int i = 0;
                        i < clist.size() && (adj > ProcessList.FOREGROUND_APP_ADJ
                                || schedGroup == Process.THREAD_GROUP_BG_NONINTERACTIVE
                                || procState > ActivityManager.PROCESS_STATE_TOP);
                        i++) {
                    ConnectionRecord cr = clist.get(i);

                    if (cr.binding.client == app) {
                        // Binding to ourself is not interesting.
                        continue;
                    }
                    if ((cr.flags&Context.BIND_WAIVE_PRIORITY) == 0) {
                        ProcessRecord client = cr.binding.client;
                        // 这里会不会出现死循环的问题呢？ A需要B的计算、B需要A的计算，这个圆环也许就是为什么
                        // 无法左滑删除的原因 循环的，
                        <!--关键点1 -->
                        int clientAdj = computeOomAdjLocked(client, cachedAdj,
                                TOP_APP, doingAll, now);

                        int clientProcState = client.curProcState;
                        if (clientProcState >= ActivityManager.PROCESS_STATE_CACHED_ACTIVITY) {
                            clientProcState = ActivityManager.PROCESS_STATE_CACHED_EMPTY;
                        }
                       <!--关键点2-->
								...
                        if ((cr.flags&Context.BIND_NOT_FOREGROUND) == 0) {
                            if (client.curSchedGroup == Process.THREAD_GROUP_DEFAULT) {
                                schedGroup = Process.THREAD_GROUP_DEFAULT;
                            }
                    ...        
			}

上面的代码中：关键点1是循环计算的入口，关键点2是无法删除的原因所在，由于A没及时更新，导致schedGroup = Process.THREAD_GROUP_DEFAULT，反过来也让A保持schedGroup = Process.THREAD_GROUP_DEFAULT。A B 都无法左滑删除。
 
# 广播或者Service原地复活的进程保活

还有一些比较常见的进程保活手段是通过注册BroadcastReceiver来实现的比如：

* 开机
* 网络状态切换
* 相机
* 一些国内推送SDK（内含一些）

另外也能依靠Service的自启动特性，通过onStartCommand的START_STICKY来实现，相比上面的不死，这些算比较柔和的启动了，毕竟这两种都是允许后台杀死的前提下启动的：

	public class BackGroundService extends Service {
	    @Nullable
	    @Override
	    public IBinder onBind(Intent intent) {
	        return null;
	    }
	
	    @Override
	    public int onStartCommand(Intent intent, int flags, int startId) {
	        return START_STICKY;
	    }
	}



# 通过START_STICKY与START_REDELIVER_INTENT实现被杀唤醒

通过startService启动的Service，如果没用呗stopService结束掉，在进程被杀掉之后，是有可能重新启动的，实现方式：


    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_STICKY;//或者START_REDELIVER_INTENT
    }

当然，前提是该进程可以被杀掉（无论被AMS还是LMDK），用户主动杀死（最近任务列表或者退出应用），都一定会通过Binder讣告机制回调:

   
    private final void handleAppDiedLocked(ProcessRecord app,
            boolean restarting, boolean allowRestart) {
        int pid = app.pid;
        boolean kept = cleanUpApplicationRecordLocked(app, restarting, allowRestart, -1);
        ...
       }

进而调用cleanUpApplicationRecordLocked函数进行一系列清理及通知工作，这里先看Service相关的工作：

	  private final boolean cleanUpApplicationRecordLocked(ProcessRecord app,
	            boolean restarting, boolean allowRestart, int index) {
	        ...
			 // 这里先出处理service
	        mServices.killServicesLocked(app, allowRestart);
	        ...
      }   

这里传入的allowRestart==true，也就说：允许重新启动Service：

    final void killServicesLocked(ProcessRecord app, boolean allowRestart) {
 
        ...
        ServiceMap smap = getServiceMap(app.userId);
       // Now do remaining service cleanup.
        for (int i=app.services.size()-1; i>=0; i--) {
            ServiceRecord sr = app.services.valueAt(i);
            if (!app.persistent) {
                app.services.removeAt(i);
            }
            ...
            if (allowRestart && sr.crashCount >= 2 && (sr.serviceInfo.applicationInfo.flags
                    &ApplicationInfo.FLAG_PERSISTENT) == 0) {
                bringDownServiceLocked(sr);
            } else if (!allowRestart
                    || !mAm.mUserController.isUserRunningLocked(sr.userId, 0)) {
                bringDownServiceLocked(sr);
            } else {
               <!--关键点1 先进行判定，如果有需要将重启的消息发送到消息队列等待执行-->
                boolean canceled = scheduleServiceRestartLocked(sr, true);
               // 受时间跟次数的限制 sr.stopIfKilled  
              <!--关键点2 二次确认，如果不应该启动Service，就将重启Service的消息移除-->
               if (sr.startRequested && (sr.stopIfKilled || canceled)) {
                    if (sr.pendingStarts.size() == 0) {
                        sr.startRequested = false;
                        if (!sr.hasAutoCreateConnections()) {
                            bringDownServiceLocked(sr);
                        }
               ...
     }
 
先看关键点1：如果允许重新启动，并且APP Crash的次数小于两次，就视图将为结束的Service重新唤起，其实就是调用scheduleServiceRestartLocked，发送消息，等待唤醒，关键点2是二次确认下，是不是需要被唤醒，如果不需要就将上面的消息移除，并进行一定的清理工作，这里的sr.stopIfKilled，其实主要跟onStartCommand返回值有关系：

	 void serviceDoneExecutingLocked(ServiceRecord r, int type, int startId, int res) {
	        boolean inDestroying = mDestroyingServices.contains(r);
	        if (r != null) {
	            if (type == ActivityThread.SERVICE_DONE_EXECUTING_START) {
	                r.callStart = true;
	                switch (res) {
	                    case Service.START_STICKY_COMPATIBILITY:
	                    case Service.START_STICKY: {
	                        r.findDeliveredStart(startId, true);
	                        r.stopIfKilled = false;
	                        break;
	                    }
	                    case Service.START_NOT_STICKY: {
	                        r.findDeliveredStart(startId, true);
	                        if (r.getLastStartId() == startId) {
	                            r.stopIfKilled = true;
	                        }
	                        break;
	                    }
	                    case Service.START_REDELIVER_INTENT: {
	                        ServiceRecord.StartItem si = r.findDeliveredStart(startId, false);
	                        if (si != null) {
	                            si.deliveryCount = 0;
	                            si.doneExecutingCount++;
	                            r.stopIfKilled = true;
	                        }
	                        break;
	                    }
	                    
所以，如果onStartCommand返回的是Service.START_STICKY，在被杀死后是会重新启动的，有必要的话，还会重启进程：

    private final boolean scheduleServiceRestartLocked(ServiceRecord r,
            boolean allowCancel) {
        boolean canceled = false;
		 ...
		 <!--关键点1-->
        mAm.mHandler.removeCallbacks(r.restarter);
        mAm.mHandler.postAtTime(r.restarter, r.nextRestartTime);
        r.nextRestartTime = SystemClock.uptimeMillis() + r.restartDelay;
        return canceled;
    }

看关键点1，其实就是发送一个重新启动Service的消息，之后就会重新启动Service。

    private class ServiceRestarter implements Runnable {
        private ServiceRecord mService;

        void setService(ServiceRecord service) {
            mService = service;
        }

        public void run() {
            synchronized(mAm) {
                performServiceRestartLocked(mService);
            }
        }
    }
    
再看下几个标志的意义：
             
1、  START_STICKY

在运行onStartCommand后service进程被kill后，那将保留在开始状态，但是不保留那些传入的intent。不久后service就会再次尝试重新创建，因为保留在开始状态，在创建     service后将保证调用onstartCommand。如果没有传递任何开始命令给service，那将获取到null的intent

2、  START_NOT_STICKY

在运行onStartCommand后service进程被kill后，并且没有新的intent传递给它。Service将移出开始状态，并且直到新的明显的方法（startService）调用才重新创建。因为如果没有传递任何未决定的intent那么service是不会启动，也就是期间onstartCommand不会接收到任何null的intent。

3、  START_REDELIVER_INTENT

在运行onStartCommand后service进程被kill后，系统将会再次启动service，并传入最后一个intent给onstartCommand。直到调用stopSelf(int)才停止传递intent。如果在被kill后还有未处理好的intent，那被kill后服务还是会自动启动。因此onstartCommand不会接收到任何null的intent。       
         

#  ProcessRecord中一些参数的意义的意义
       
*     int maxAdj;                 // Maximum OOM adjustment for this process
*     int curRawAdj;              // Current OOM unlimited adjustment for this process
*     int setRawAdj;              // Last set OOM unlimited adjustment for this process  
*     int curAdj;                 // Current OOM adjustment for this process
*     int setAdj;                 // Last set OOM adjustment for this process

adj主要用来给LMKD服务，让内核曾选择性的处理后台杀死，curRawAdj是本地updateOomAdj计算出的临时值，setRawAdj是上一次计算出兵设定好的oom值，两者都是未经过二次调整的数值，curAdj与setAdj是经过调整之后的adj，这里有个小问题，为什么前台服务进程的oom_adj打印出来是1，但是在AMS登记的curAdj却是2呢？


	 oom: max=16 curRaw=2 setRaw=2 cur=2 set=2
    curSchedGroup=-1 setSchedGroup=-1 systemNoUi=false trimMemoryLevel=0
    curProcState=4 repProcState=4 pssProcState=-1 setProcState=4 lastStateTime=-37s554ms

AMS传递给LMKD服务的adj确实是2，LMKD用2计算出的oom_score_adj=117 （1000*oom_adj/17） 也是准确的数值 ,那为什么proc/pid/oom_adj中的数值是1呢？应该是**反向取整**导致的，高版本的内核都不在使用oom_adj，而是用oom_score_adj，oom_adj是一个向前兼容。

<!--为何只记录了oom_score_adj-->
	static void cmd_procprio(int pid, int uid, int oomadj) {
	    struct proc *procp;
	    char path[80];
	    char val[20];
	
	    if (oomadj < OOM_DISABLE || oomadj > OOM_ADJUST_MAX) {
	        ALOGE("Invalid PROCPRIO oomadj argument %d", oomadj);
	        return;
	    }
	
		// 这里只记录oom_score_adj
	    snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
	    snprintf(val, sizeof(val), "%d", lowmem_oom_adj_to_oom_score_adj(oomadj));
	    writefilestring(path, val);
	    <!--use_inkernel_interface = 1-->
	     if (use_inkernel_interface)
        return;
        ....
     }
 
 use_inkernel_interface标识其他几个oom_adj，oom_score跟随 oom_score_adj变化。oom_adj=（oom_score_adj*17/1000）,取整的话，正好小了1；看如下解释：
 
 

	The value of /proc/<pid>/oom_score_adj is added to the badness score before oom_adj；
	
	For backwards compatibility with previous kernels, /proc/<pid>/oom_adj may also
	be used to tune the badness score.  Its acceptable values range from -16
	(OOM_ADJUST_MIN) to +15 (OOM_ADJUST_MAX) and a special value of -17
	(OOM_DISABLE) to disable oom killing entirely for that task.  Its value is
	scaled linearly with /proc/<pid>/oom_score_adj.
	
oom_adj的存在是为了和旧版本的内核兼容，并且随着oom_score_adj线性变化，如果更改其中一个，另一个会自动跟着变化，在内核中变化方式为：

* 写oom_score_adj时，内核里都记录在变量 task->signal->oom_score_adj 中；
* 读oom_score_adj时，从内核的变量 task->signal->oom_score_adj 中读取；
* 写oom_adj时，也是记录到变量 task->signal->oom_score_adj 中，会根据oom_adj值按比例换算成oom_score_adj。
* 读oom_adj时，也是从内核变量 task->signal->oom_score_adj 中读取，只不过显示时又按比例换成oom_adj的范围。


所以，就会产生如下精度丢失的情况：

	# echo 9 > /proc/556/oom_adj
	# cat /proc/556/oom_score_adj
	  529
	# cat /proc/556/oom_adj
	  8

这也是为什么Android中明明算出来的oom_adj=1（2），在proc/pid/oom_adj总记录的确实0（1）。
  
   
*     int curSchedGroup;          // Currently desired scheduling class
*     int setSchedGroup;          // Last set to background scheduling class

curSchedGroup与setSchedGroup是AMS管理进程的一个参考，定义在ProcessList.java中，从名字上看与任务调度有关系，答案也确实如此，取值有如下三种，不同版本略有不同，这里是7.0，

    // Activity manager's version of Process.THREAD_GROUP_BG_NONINTERACTIVE
    static final int SCHED_GROUP_BACKGROUND = 0;
    // Activity manager's version of Process.THREAD_GROUP_DEFAULT
    static final int SCHED_GROUP_DEFAULT = 1;
    // Activity manager's version of Process.THREAD_GROUP_TOP_APP
    static final int SCHED_GROUP_TOP_APP = 2;
    
AMS只能杀死后台进程，只有setSchedGroup==ProcessList.SCHED_GROUP_BACKGROUND的进程才被AMS看做后台进程，才可以被杀死，否则AMS无权杀死。

		 <!--参考代码1-->
	  if (app.waitingToKill != null && app.curReceivers.isEmpty()
	                    && app.setSchedGroup == ProcessList.SCHED_GROUP_BACKGROUND) {
	                app.kill(app.waitingToKill, true);
	                success = false;
	            } 
        
       <!--参考代码2-->    
        // Kill the running processes.
        for (int i = 0; i < procsToKill.size(); i++) {
            ProcessRecord pr = procsToKill.get(i);
            if (pr.setSchedGroup == ProcessList.SCHED_GROUP_BACKGROUND
                    && pr.curReceivers.isEmpty()) {
                pr.kill("remove task", true);
            } else {
                // We delay killing processes that are not in the background or running a receiver.
                pr.waitingToKill = "remove task";
            }
        }
        
以上两个场景：场景一是AMS计算oomAdj并清理进程 ，场景二的代表：从最近的任务列表删除进程。   
         
*     int curProcState = PROCESS_STATE_NONEXISTENT; // Currently computed process state
*     int repProcState = PROCESS_STATE_NONEXISTENT; // Last reported process state
*     int setProcState = PROCESS_STATE_NONEXISTENT; // Last set process state in process tracker
*     int pssProcState = PROCESS_STATE_NONEXISTENT; // Currently requesting pss for

ProcState 主要是为AMS服务，AMS依据procState判断进程当前的状态以及重要程度，对应的值位于ActivityManager.java中，主要作用是：决定进程的缓存等级以及缓存进程的生死。

	<!--参考代码1-->
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
                
	<!--参考代码2-->
	if ((app.curProcState >= ActivityManager.PROCESS_STATE_IMPORTANT_BACKGROUND
	                            || app.systemNoUi) && app.pendingUiClean) {
	                        final int level = ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN;
	                        if (app.trimMemoryLevel < level && app.thread != null) {
	                            try {
	                                app.thread.scheduleTrimMemory(level);
	                            } catch (RemoteException e) {
	                            }
	                        }
	                        app.pendingUiClean = false;
	                    }
                    
                    
#  添加Manifest文件属性值为android:persistent=“true” 

**这种做法需要系统签名，一般是在定制ROM的时候，手机厂家自身的APP才能获取的权限。
**              

# 总结 

**所有流氓手段的进程保活，都是下策**，建议不要使用，本文只是分析实验用。当APP退回后台，优先级变低，就应该适时释放内存，以提高系统流畅度，依赖流氓手段提高优先级，还不释放内存，保持不死的，都是作死。

[Android 后台杀死系列之一：FragmentActivity 及 PhoneWindow 后台杀死处理机制](https://juejin.im/post/5878dc578d6d810058b884a9)
[Android后台杀死系列之二：ActivityManagerService与App现场恢复机制](https://juejin.im/post/5878c1ce8d6d810058769b75)  	        	[Android后台杀死系列之三：后台杀死原理LowMemoryKiller（4.3-6.0）](https://juejin.im/post/5878bf99570c35006211dc7a)   
[Android后台杀死系列之四：Binder讣告原理](https://juejin.im/post/58cf7eef128fe1006c99ba1c)

**仅供参考，欢迎指正 	**        

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
[2.1. linux OOM 机制分析 oom_adj不一致](http://learning-kernel.readthedocs.io/en/latest/mem-management.html)

