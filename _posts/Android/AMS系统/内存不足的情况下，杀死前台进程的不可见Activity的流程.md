内存不足的情况下，杀死前台进程的不可见Activity的流程，其次难道这还是个双向的吗？A应用内存不需，B系统内存不足

	 private void attach(boolean system) {
	        sCurrentActivityThread = this;
	        mSystemThread = system;
	        if (!system) {
	           ...
	            final IActivityManager mgr = ActivityManagerNative.getDefault();
	            try {
	                mgr.attachApplication(mAppThread);
	            } catch (RemoteException ex) {
	            }
	            // Watch for getting close to heap limit.
	            <!--关键点1-->
	            BinderInternal.addGcWatcher(new Runnable() {
	                @Override public void run() {
	                    if (!mSomeActivitiesChanged) {
	                        return;
	                    }
	                    Runtime runtime = Runtime.getRuntime();
	                    long dalvikMax = runtime.maxMemory();
	                    long dalvikUsed = runtime.totalMemory() - runtime.freeMemory();
	                    // 如果已经使用的内存不足1/4着手处理杀死Activity，并且这个时候，没有缓存进程
	                    if (dalvikUsed > ((3*dalvikMax)/4)) {
	                        mSomeActivitiesChanged = false;
	                        try {
	                            mgr.releaseSomeActivities(mAppThread);
	                        } catch (RemoteException e) {
	                        }

  看关键点1，这里声明了GC的时机，就是内存超过3/4的时候，是有一定概率回收不可见的Activity的，但，只是有可能，并不是说，只要不可见就会回收，超过3/4，等到下一次GC，触发      
  
  
	  public class BinderInternal {
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
	            for (int i=0; i<sTmpWatchers.length; i++) {
	                if (sTmpWatchers[i] != null) {
	                    sTmpWatchers[i].run();
	                }
	            }
	            sGcWatcher = new WeakReference<GcWatcher>(new GcWatcher());
	        }
	    }
	
	    public static void addGcWatcher(Runnable watcher) {
	        synchronized (sGcWatchers) {
	            sGcWatchers.add(watcher);
	        }
	    }

可以手动模拟