---

layout: post
title: Android可见APP的不可见任务栈（TaskRecord）杀死分析
category: Android

---

Android依托Java型虚拟机，OOM是经常遇到的问题，那么在快达到OOM的时候，系统难道不能回收部分界面来达到缩减开支的目的码？在系统内存不足的情况下，可以通过AMS及LowMemoryKiller杀优先级低的进程，来回收进程资源。但是这点对于前台OOM问题并没有多大帮助，因为每个Android应用有一个Java内存上限，比如256或者512M，而系统内存可能有6G或者8G，也就是说，一个APP的进程达到OOM的时候，可能系统内存还是很充足的，这个时候，系统如何避免OOM的呢？ios是会将不可见界面都回收，之后再恢复，Android做的并没有那么彻底，简单说：**对于单栈（TaskRecord）应用，在前台的时候，所有界面都不会被回收，只有多栈情况下，系统才会回收不可见栈的Activity**。注意回收的目标是不可见**栈（TaskRecord）**的Activity。

![前台APP回收场景](https://upload-images.jianshu.io/upload_images/1460468-e1eb5580372793d9.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

如上图，在前台时，左边单栈APP跟进程生命周期绑定，多栈的，不可见栈TaskRecord1是有被干掉风险，TaskRecord2不会。下面简单分析下。


# Android原生提供内存回收入口

Google应该也是想到了这种情况，源码自身就给APP自身回收内存留有入口，在每个进程启动的时候，回同步启动个微小的内存监测工具，入口是ActivityThread的attach函数，Android应用进程启动后，都会调用该函数：

> ActivityThread

	 private void attach(boolean system) {
	        sCurrentActivityThread = this;
	        mSystemThread = system;
	        if (!system) {
	           ...
	            final IActivityManager mgr = ActivityManagerNative.getDefault();
				 ...
	            // Watch for getting close to heap limit.
	            <!--关键点1，添加监测工具-->
	            BinderInternal.addGcWatcher(new Runnable() {
	                @Override public void run() {
	                    if (!mSomeActivitiesChanged) {
	                        return;
	                    }
	                    Runtime runtime = Runtime.getRuntime();
	                    long dalvikMax = runtime.maxMemory();
	                    long dalvikUsed = runtime.totalMemory() - runtime.freeMemory();
	                     <!--关键点2 ：如果已经可用的内存不足1/4着手处理杀死Activity，并且这个时候，没有缓存进程-->
	                    if (dalvikUsed > ((3*dalvikMax)/4)) {
	                        mSomeActivitiesChanged = false;
	                        try {
	                            mgr.releaseSomeActivities(mAppThread);
	                        } catch (RemoteException e) {
	                    ...
					}
					
先关键点1，对于非系统进程，通过BinderInternal.addGcWatcher添加了一个内存监测工具，后面会发现，这个工具的检测时机是每个GC节点。而对于我们上文说的回收不可见Task的时机是在关键点2：Java使用内存超过3/4的时候，调用AMS的**releaseSomeActivities**，尝试释放不可见Activity，当然，并非所有不可见的Activity会被回收，当APP内存超过3/4的时候，调用栈如下：

![APP内存超过3/4就会尝试GC](https://upload-images.jianshu.io/upload_images/1460468-b7228230d4ad9487.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


# APP在GC节点的内存监测机制  
  
之前说过，通过BinderInternal.addGcWatcher就添加了一个内存监测工具，原理是什么？其实很简单，就是利用了Java的finalize那一套：JVM垃圾回收器准备释放内存前，会先调用该对象finalize（如果有的话）。
  
	  public class BinderInternal {
	  <!--关键点1 弱引用-->
	    static WeakReference<GcWatcher> sGcWatcher
	            = new WeakReference<GcWatcher>(new GcWatcher());
	    static ArrayList<Runnable> sGcWatchers = new ArrayList<>();
	    static Runnable[] sTmpWatchers = new Runnable[1];
	    static long sLastGcTime;
	
	    static final class GcWatcher {
	        @Override
	        protected void finalize() throws Throwable {
	            handleGc();
	            sLastGcTime = SystemClock.uptimeMillis();
	            synchronized (sGcWatchers) {
	                sTmpWatchers = sGcWatchers.toArray(sTmpWatchers);
	            }
	            <!--关键点2 执行之前添加的回调-->
	            for (int i=0; i<sTmpWatchers.length; i++) {
	                if (sTmpWatchers[i] != null) {
	                    sTmpWatchers[i].run();
	                }
	            }
	            <!--关键点3 下一次轮回-->
	            sGcWatcher = new WeakReference<GcWatcher>(new GcWatcher());
	        }
	    }
	
	    public static void addGcWatcher(Runnable watcher) {
	        synchronized (sGcWatchers) {
	        
	            sGcWatchers.add(watcher);
	        }	
	    }
     ...
    }
    
这里有几个关键点，关键点1是弱引用，GC的sGcWatcher引用的对象是要被回收的，这样回收前就会走关键点2，遍历执行之前通过BinderInternal.addGcWatcher添加的回调，执行完毕后，重新为sGcWatcher赋值新的弱引用，这样就会走下一个轮回，这就是为什么GC的时候，有机会触发releaseSomeActivities，其实，这里是个不错的内存监测点，用来扩展自身的需求。

# AMS的TaskRecord栈释放机制

如果GC的时候，APP的Java内存使用超过了3/4，就会触发AMS的releaseSomeActivities，尝试回收界面，增加可用内存，但是并非所有场景都会真的销毁Activity，比如单栈的APP就不会销毁，多栈的也要分场景，可能选择性销毁不可见Activity。

> ActivityManagerService

    @Override
    public void releaseSomeActivities(IApplicationThread appInt) {
        synchronized(this) {
            final long origId = Binder.clearCallingIdentity();
            try {
                ProcessRecord app = getRecordForAppLocked(appInt);
                mStackSupervisor.releaseSomeActivitiesLocked(app, "low-mem");
            } finally {
                Binder.restoreCallingIdentity(origId);
            }
        }
    }
    
	
    void releaseSomeActivitiesLocked(ProcessRecord app, String reason) {
        TaskRecord firstTask = null;
        ArraySet<TaskRecord> tasks = null;
        for (int i = 0; i < app.activities.size(); i++) {
            ActivityRecord r = app.activities.get(i);
            <!--如果已经有一个进行，则不再继续-->
            if (r.finishing || r.state == DESTROYING || r.state == DESTROYED) {
                return;
            }
            <!--过滤-->
            if (r.visible || !r.stopped || !r.haveState || r.state == RESUMED || r.state == PAUSING
                    || r.state == PAUSED || r.state == STOPPING) {
                continue;
            }
            if (r.task != null) {
                if (firstTask == null) {
                    firstTask = r.task;
             <!--关键点1 只要要多余一个TaskRecord才有机会走这一步，-->
                } else if (firstTask != r.task) {
                    if (tasks == null) {
                        tasks = new ArraySet<>();
                        tasks.add(firstTask);
                    }
                    tasks.add(r.task);
                }
            }
        }
        <!--注释很明显，-->
        if (tasks == null) {
            if (DEBUG_RELEASE) Slog.d(TAG_RELEASE, "Didn't find two or more tasks to release");
            return;
        }
 
        // If we have activities in multiple tasks that are in a position to be destroyed,
        // let's iterate through the tasks and release the oldest one.
        final int numDisplays = mActivityDisplays.size();
        for (int displayNdx = 0; displayNdx < numDisplays; ++displayNdx) {
            final ArrayList<ActivityStack> stacks = mActivityDisplays.valueAt(displayNdx).mStacks;
            // Step through all stacks starting from behind, to hit the oldest things first.
            for (int stackNdx = 0; stackNdx < stacks.size(); stackNdx++) {
                final ActivityStack stack = stacks.get(stackNdx);
                // Try to release activities in this stack; if we manage to, we are done.
                if (stack.releaseSomeActivitiesLocked(app, tasks, reason) > 0) {
                    return;
                }
            }
        }
    }

这里先看第一个关键点1：**如果想要tasks非空，则至少需要两个TaskRecord才行，不然，只有一个firstTask，永远无法满足firstTask != r.task这个条件**，也无法走

	 tasks = new ArraySet<>();

也就是说，APP当前进程中，至少两个TaskRecord才有必要走Activity的销毁逻辑，注释说明很清楚：Didn't find two or more tasks to release，如果能找到超过两个会怎么样呢？
    
     final int releaseSomeActivitiesLocked(ProcessRecord app, ArraySet<TaskRecord> tasks,
            String reason) {
        
        <!--maxTasks 保证最多清理- tasks.size() / 4有效个，最少清理一个 同时最少保留一个前台TaskRecord->
        int maxTasks = tasks.size() / 4;
        if (maxTasks < 1) {
        <!--至少清理一个-->
            maxTasks = 1;
        }
        int numReleased = 0;
        for (int taskNdx = 0; taskNdx < mTaskHistory.size() && maxTasks > 0; taskNdx++) {
            final TaskRecord task = mTaskHistory.get(taskNdx);
            if (!tasks.contains(task)) {
                continue;
            }
            int curNum = 0;
            final ArrayList<ActivityRecord> activities = task.mActivities;
            for (int actNdx = 0; actNdx < activities.size(); actNdx++) {
                final ActivityRecord activity = activities.get(actNdx);
                if (activity.app == app && activity.isDestroyable()) {
                    destroyActivityLocked(activity, true, reason);
                    if (activities.get(actNdx) != activity) {
                        actNdx--;
                    }
                    curNum++;
                }
            }
            if (curNum > 0) {
                numReleased += curNum;
                maxTasks--;
                if (mTaskHistory.get(taskNdx) != task) {
                    // The entire task got removed, back up so we don't miss the next one.
                    taskNdx--;
                }
            }
        }
        return numReleased;
    }

ActivityStack利用maxTasks 保证，最多清理tasks.size() / 4，最少清理1个TaskRecord，同时，至少要保证保留一个前台可见TaskRecord，比如如果有两个TaskRecord，则清理先前的一个，保留前台显示的这个，如果三个，则还要看看最老的是否被有效清理，也就是是否有Activity被清理，如果有则只清理一个，保留两个，如果没有，则继续清理次老的，保留一个前台展示的，如果有四个，类似，如果有5个，则至少两个清理，这里的规则如果有兴趣，可自己简单看下。一般APP中，很少有超过两个TaskRecord的。
    
# demo验证

模拟了两个Task的模型，先启动在一个栈里面启动多个Activity，然后在通过startActivity启动一个新TaskRecord，并且在新栈中不断分配java内存，当Java内存使用超过3/4的时候，就会看到前一个TaskRecord栈内Activity被销毁的Log，同时如果通过studio的layoutinspect查看，会发现APP只保留了新栈内的Activity，验证了之前的分析。

![image.png](https://upload-images.jianshu.io/upload_images/1460468-fe5479d91ea4ba37.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# 总结

* 单栈的进程，Activity跟进程声明周期一致
* 多栈的，只有不可见栈的Activity可能被销毁（Java内存超过3/4,不可见）
* 该回收机制利用了Java虚拟机的gc机finalize
* 至少两个TaskRecord占才有效，所以该机制并不激进，因为主流APP都是单栈。
