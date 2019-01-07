Android依托Java型虚拟机，OOM是经常遇到的问题，那么在快达到OOM的时候，系统难道不能回收部分界面来达到缩减开支的目的码？在系统内存不足的情况下，可以通过AMS及LowMemoryKiller杀优先级低的进程，来回收进程资源。但是这点对于前台OOM问题并没有多大帮助，因为每个Android应用有一个Java内存上限，比如256或者512M，而系统内存可能有6G或者8G，也就是说，一个APP的进程达到OOM的时候，可能系统内存还是很充足的，这个时候，系统如何避免OOM的呢？ios是会将不可见界面都回收，之后再恢复，Android做的并没有那么彻底，简单说：**对于单栈（TaskRecord）应用，在前台的时候，所有界面都不会被回收，只有多栈情况下，系统才会回收不可见栈的Activity**。注意回收的目标是不可见**栈（TaskRecord）**。

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
					
先关键点1，对于非系统进程，通过BinderInternal.addGcWatcher添加了一个内存监测工具，后面会发现，这个工具的检测时机是每个GC节点。而对于我们上文说的回收不可见Task的时机是在关键点2：Java使用内存超过3/4的时候，调用AMS的**releaseSomeActivities**，尝试释放不可见Activity，当然，并非所有不可见的Activity会被回收。


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

如果再GC的时候，APP的Java内存使用超过了3/4，就会触发AMS的releaseSomeActivities，尝试回收界面，增加可用内存：

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
        // Examine all activities currently running in the process.
        TaskRecord firstTask = null;
        // Tasks is non-null only if two or more tasks are found.
        ArraySet<TaskRecord> tasks = null;
        if (DEBUG_RELEASE) Slog.d(TAG_RELEASE, "Trying to release some activities in " + app);
        for (int i = 0; i < app.activities.size(); i++) {
            ActivityRecord r = app.activities.get(i);
            // First, if we find an activity that is in the process of being destroyed,
            // then we just aren't going to do anything for now; we want things to settle
            // down before we try to prune more activities.
            if (r.finishing || r.state == DESTROYING || r.state == DESTROYED) {
                if (DEBUG_RELEASE) Slog.d(TAG_RELEASE, "Abort release; already destroying: " + r);
                return;
            }
            // Don't consider any activies that are currently not in a state where they
            // can be destroyed.
            if (r.visible || !r.stopped || !r.haveState || r.state == RESUMED || r.state == PAUSING
                    || r.state == PAUSED || r.state == STOPPING) {
                if (DEBUG_RELEASE) Slog.d(TAG_RELEASE, "Not releasing in-use activity: " + r);
                continue;
            }
            if (r.task != null) {
                if (DEBUG_RELEASE) Slog.d(TAG_RELEASE, "Collecting release task " + r.task
                        + " from " + r);
                if (firstTask == null) {
                    firstTask = r.task;
                    // 销毁第一个task之后的activity
                } else if (firstTask != r.task) {
                    if (tasks == null) {
                        tasks = new ArraySet<>();
                        tasks.add(firstTask);
                    }
                    tasks.add(r.task);
                }
            }
        }
        if (tasks == null) {
            if (DEBUG_RELEASE) Slog.d(TAG_RELEASE, "Didn't find two or more tasks to release");
            return;
        }
        <!--关键点 释放multiple tasks中不可见Task-->
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
    
可以手动模拟

![image.png](https://upload-images.jianshu.io/upload_images/1460468-fe5479d91ea4ba37.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
