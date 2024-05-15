### LockSupport使用：挂起与唤醒


     
        val threadList: MutableList<Thread> = mutableListOf()

LockSupport.park()会挂起当前运行的线程

		<!--在某个线程主动睡眠，但是睡眠之前需要将睡眠线程给标记出来-->
        threadPoolExecutor.submit(Runnable {
            threadList.add(Thread.currentThread())
            LockSupport.park()
            val name = "结果" + Thread.currentThread().name +" "+Thread.currentThread().id

LockSupport.unpark需要明确的指明哪个线程需要还清
        
		<!--另个一个现场负责唤醒Thread-->
        threadList.forEach {
            LockSupport.unpark(it)
        }
        threadList.clear()
   

LockSupport park函数主要是调用了Unsafe的park函数，所以LockSupport的底层还是Unsafe的实现

	    public static void park() {
	        U.park(false, 0L);
	    }
	    
只不过Unsafe不是外部能直接调用的，所以 LockSupport便算是一个辅助类。

 

### LockSupport与Object自身的wait，notify的关系

wait，notify需要再Syncronize的方法或者代码块中调用，
