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

wait，notify需要再Syncronize的方法或者代码块中调用，Thread.sleep()和Object.wait()的区别是wait会释放锁，但是Thread.sleep()不会释放锁。Object.wait()和Condition.await()就基本一致的，不同的是Condition.await()底层是调用**LockSupport.park()**来实现阻塞当前线程的，并且在阻塞当前线程之前还干了两件事，一是把当前线程添加到条件队列中，二是“完全”释放锁，也就是让state状态变量变为0，然后才是调用LockSupport.park()阻塞当前线程。猜测object的wait差不多流程类似。
 
 
 react-native bundle --platform android --dev false --entry-file  /Users/hzlishang/Documents/GitHub/LabAffinity/app/src/main/assets/index.android.js  --bundle-output  index.android.bundle

