
## Java框中的AbstractQueuedSynchronizer[AQS队列同步器]框架：有锁同步[睡眠与唤起]

ReentrantLock是一种常用的Java锁，支持公平与非公平两种模式，公平锁与非公平锁的区别是**是否一直遵守先来后到**，公平锁：直接进入等待队列，先进入先唤醒，而非公平锁支持**先来一次抢断【加塞】，抢断【加塞】不成功，再退化成排队**。ReentrantLock 是基于AbstractQueuedSynchronizer框架实现的，AbstractQueuedSynchronizer[**队列同步器**]是并发包的核心，或者说抽象队列同步器就是锁的本体。

除了ReentrantLock、Semaphore(做流量控制)等也是借助AQS模板实现的，而AQS也是借助CAS与同步队列实现的，**AQS会把请求获取锁失败的线程放入一个队列的尾部**，然后**睡眠**。加锁是借助CAS完成，CAS保证只会有一个线程获取锁成功，失败的就进入睡眠。

ReentrantLock本身只实现了Lock、Serializable接口，

	public class ReentrantLock implements Lock, Serializable {

它采用的是组合模式，而不是简单的继承，内部有个静态抽象内部类Sync，继承AbstractQueuedSynchronizer，负责AQS框架的部分：
	
      abstract static class Sync extends AbstractQueuedSynchronizer {
    
看下**ReentrantLock**的构造函数，ReentrantLock默认无参构造方法跟有参方法：

    public ReentrantLock() {
        sync = new NonfairSync();
    }
    public ReentrantLock(boolean fair) {
        sync = fair ? new FairSync() : new NonfairSync();
    }
    
默认非公平，如果需要使用公平锁，则需要使用有参构造函数，NonfairSync与FairSync均继承自ReentrantLock中的静态内部类Sync类 ，负责承担AQS的作用。

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/3a2b8ece52c043e2bb8ee2b5e3935340~tplv-k3u1fbpfcp-watermark.image?)


### ReetrantLock加锁流程--公平锁 **排队优先**

ReentrantLock在使用时候，一般是

	reentrantLock.lock();
	
	<!--临界代码-->
	...
	reentrantLock.unLock();

公平锁lock()函数的实现

	 static final class FairSync extends Sync {
	        private static final long serialVersionUID = -3000897897090466540L;
	
	        final void lock() {
	            acquire(1);
	        }
	        
acquire调用的其实是父类AbstractQueuedSynchronizer的acquire方法，acquire进一步调用子类tryAcquire以及自身的acquireQueued，如果无法获取锁，并且满足某些条件则进入睡眠

    public final void acquire(int arg) {
        if (!tryAcquire(arg) && acquireQueued(addWaiter(Node.EXCLUSIVE), arg))
            selfInterrupt();
    }

如果tryAcquire直接就获取成功的话，是无需睡眠的，tryAcquire的作用就像名字一样，试试能不能直接获取到，逻辑如下

	        protected final boolean tryAcquire(int acquires) {
	            final Thread current = Thread.currentThread();
			<!--  获取用于同步的state   private volatile int state; 在AbstractQueuedSynchronizer定义-->
	            int c = getState();
	            if (c == 0) {
	            	<!--判断前面是不是有等待的节点，第一次进来肯定没有，这里也是跟非公平锁相差最大的地方，不是唤醒的节点是抢占的节点-->
	                if (!hasQueuedPredecessors() &&
	                    compareAndSetState(0, acquires)) {  //  compareAndSetState(0, acquires)这句只会有一个现成成功
	                    setExclusiveOwnerThread(current);
	                    return true;
	            }} 
	          <!--另一半流程 可重入逻辑-->
	           else if (current == getExclusiveOwnerThread()) {
                int nextc = c + acquires;
                if (nextc < 0)
                    throw new Error("Maximum lock count exceeded");
                setState(nextc);
                return true;
            }
            <!--无法获取锁-->
            return false;
        }
    }
compareAndSetState将value的值从0更新成1，获得锁，同时将自己设置成占有锁的对象。这个时候，如果有另一个线程进来会怎样，hasQueuedPredecessors仍旧满足条件，但是compareAndSetState会无法满足，从而进入另一半流程，先判断是不是当前线程重新申请锁

	if (current == getExclusiveOwnerThread()) 

这部分是可重入锁的原理部分，即一个已经获取锁的线程，重新申请锁，如果是这种情况，则直接更新state即可，也看做锁获取成功，否则认为锁获取失败，假设不是同一个线程，后续如何处理呢？流程会重新回到父类的模板：

	   public final void acquire(int arg) {
	        if (!tryAcquire(arg) &&
	            acquireQueued(addWaiter(Node.EXCLUSIVE), arg))
	            selfInterrupt();
	    }
	    
继续走addWaiter 与acquireQueued流程，创建一个enqueues node，再加入队列，然后就可以中断

    private Node addWaiter(Node mode) {
        Node node = new Node(mode);
        for (;;) {
            Node oldTail = tail;
            if (oldTail != null) {//尾部插入，头部唤起，先进先出
            	//给定对象的指定偏移地址的地方设值，与此类似操作还有：putInt，putDouble，putLong，putChar等
            	<!--为什么不用赋值？防止切换么？  设置node的前一个是oldTail，可能吧，不让队列设置被中断-->
                U.putObject(node, Node.PREV, oldTail);
                if (compareAndSetTail(oldTail, node)) { //   U.compareAndSwapObject(this, TAIL, expect, update); tail 变量赋值，保证尾部正确性，尾巴是oldTail，新尾巴是node，有且仅有一个线程符合要求
               <!-- 进入锁，有保证，可以赋值-->  
                    oldTail.next = node;
                    return node;
                }
            } else {
            <! 构建队列，第一个H=T使用来辅助的吗?感觉无实质意义-->
                initializeSyncQueue();
            }
        }
    }
第一次进来调用initializeSyncQueue，初始化SyncQueue，利用U.compareAndSwapObject设置Head，并且tail = h，

    private final void initializeSyncQueue() {
        Node h;
        if (U.compareAndSwapObject(this, HEAD, null, (h = new Node())))
            tail = h;
    }
    
之后回到for(;;)，将该线程新建的Node加入到该队列，这里核心的同步全部依靠Unsafe类的compareAndSet来实现。 for + compareAndSetTail构成自旋，组要保证**只有compareAndSetTail【更新多线程共用的对象都要用CAS】**，这保证只有一个线程更新tail成功，这里重点是tail，而不是tail内部的值，将其加入到队列之后，即可返回，其他线程可进入addWaiter流程，本线程acquireQueued流程继续，
    
    final boolean acquireQueued(final Node node, int arg) {
        try {
            boolean interrupted = false;
            <!--注意这里有for (;;)，for (;;)的目的是防止临界状态的时候，线程未被唤醒-->
            for (;;) {
            		<!--此时node已经加入队列尾部-->
                final Node p = node.predecessor();
                <!--判断当前加入的是不是第一个，如果是，再次尝试获取一次，如果获取tryAcquire成功，说明正好锁被释放了，可以直接获取 这个时候还没睡眠 ，如果是一个等待线程都没有，那么另一个执行线程不会唤醒水，如果有等待线程才存在唤醒，这里有个标志就是shouldParkAfterFailedAcquire设置的signal-->
                if (p == head && tryAcquire(arg)) {
                    setHead(node);
                    p.next = null; // help GC
                    return interrupted;
                }
                <!--存在执行到这里，另一个释放的临界点-->
                <!--如果node不是第一个节点，那自己基本上要睡眠的  shouldParkAfterFailedAcquire 找到一个可以唤起自己的前驱节点这里其实就是之前第一个 ,第一次shouldParkAfterFailedAcquire设置了signal，返回了false，所以会从新进入for (;;)，如果tryAcquire成功，说明释放了，如果失败，也已经加入等待队列，不会被遗漏。-->
             
                if (shouldParkAfterFailedAcquire(p, node) &&
                    parkAndCheckInterrupt())
                    interrupted = true;
            }
        } catch (Throwable t) {
            cancelAcquire(node);
            throw t;
        }
    }

注意，**这里有个for循环，第一次shouldParkAfterFailedAcquire设置了signal，返回了false，所以会从新进入for (;;)，如果tryAcquire成功，说明释放了，如果失败，也已经加入等待队列，主要是防止临界点的遗漏。**如果加入队列后，还是没有获取到锁，那么parkAndCheckInterrupt会利用LockSupport.park挂起，最终其实是Unsafe的park函数，LockSupport(提供park/unpark操作，睡眠与唤起)，是AQS框架中另一个重要类，跟提供CAS的Unsafa共同构建了该体系。

	    private final boolean parkAndCheckInterrupt() {
	   	 <!--安全挂起-->
	        LockSupport.park(this);
	        <!--唤醒后判断是否是被中断过，正常不会被中断-->
	        return Thread.interrupted();
	    }

    public static void park(Object blocker) {
        Thread t = Thread.currentThread();
        setBlocker(t, blocker); //设置block，可能给外部看状态用的
        U.park(false, 0L);   //开始阻塞，等待该线程的unpark函数被调用
        setBlocker(t, null);//清理
    }

Thread.interrupted()是用来判断是否被设置中断运行，如果被打断过，返回true，结果就是不用处理该任务了，需要终结，否则还是要处理的。从上面的流程可以看出，公平锁是在AQS的基础上实现的，AQS定义了锁的基本框架与功能：

* **CAS的能力**
* **acquire ：获取锁**
* **tryAcquire ：尝试获取，试试能不能一次性成功**
* **release：释放锁**
* **tryRelease：尝试释放**
* **Node head、 Node tail队列 ：线程等待队列模型，这里的队列对应是Thread等待队列**

> 备注：**对于已经获取到锁的线程，后续的操作就不需要任何同步处理**，因为就它自己能操作其他的都无法通过CAS更新，那后续也就无需CAS更新，直接赋值即可。

### 加锁流程 -非公平锁**加塞优先，上来就抢**
             

	   static final class NonfairSync extends Sync {
	        private static final long serialVersionUID = 7316153563782823691L;


	 final boolean nonfairTryAcquire(int acquires) {
			            final Thread current = Thread.currentThread();
			            int c = getState();
			            if (c == 0) {
			            <!--如果可用，上来就抢，不等-->
			                if (compareAndSetState(0, acquires)) {
			                    setExclusiveOwnerThread(current);
			                    return true;
			                }
			            }
			            else if (current == getExclusiveOwnerThread()) {
			                int nextc = c + acquires;
			                if (nextc < 0) // overflow
			                    throw new Error("Maximum lock count exceeded");
			                setState(nextc);
			                return true;
			            }
			            return false;
			        }
			        
可以看到非公平锁在当下可用的时候，首先调用CAS尝试将state从0改为1，如果能改成功则表示能够直接获取到锁，那就可以将exclusiveOwnerThread设置为当前线程，不需要公平锁的判断是否有等待队列的操作，如果获取不到，则走后续acquire流程，而公平锁则要看自己是不是第一个，如果是才会去获取，这里之后的流程两者一致都是要睡眠等待唤起。

#### 唤起流程

唤起其实是调用的unlock，无论是否是公平锁，这里的处理逻辑都一样
   
    public void unlock() {
        sync.release(1);
    }
    
    public final boolean release(int arg) {
        if (tryRelease(arg)) {
            Node h = head;
             <!--   **这里在之前设置过等待信号量  pred.compareAndSetWaitStatus(ws, Node.SIGNAL)**;-->
            if (h != null && h.waitStatus != 0)
                unparkSuccessor(h);
            return true;
        }
        return false;
    }

tryRelease的实现在ReetrantLock中，因为ReetrantLock是可重入锁，所以要看看是不是已经到了最外层的锁，只有到了最外层，才算真正的release：

        protected final boolean tryRelease(int releases) {
            int c = getState() - releases;
            if (Thread.currentThread() != getExclusiveOwnerThread())
                throw new IllegalMonitorStateException();
            boolean free = false;
            if (c == 0) {
                free = true;
                setExclusiveOwnerThread(null);
            }
            setState(c);
            return free;
        }	
 
 之后选择Head之后**第一个线程Node进行唤起**即可
 
	   private void unparkSuccessor(Node node) {
	        /*
	         * If status is negative (i.e., possibly needing signal) try
	         * to clear in anticipation of signalling.  It is OK if this
	         * fails or if status is changed by waiting thread.
	         */
	        int ws = node.waitStatus;
	        if (ws < 0)
	            node.compareAndSetWaitStatus(ws, 0);
	
	        /*
	         * Thread to unpark is held in successor, which is normally
	         * just the next node.  But if cancelled or apparently null,
	         * traverse backwards from tail to find the actual
	         * non-cancelled successor.
	         */
	        Node s = node.next;
	        if (s == null || s.waitStatus > 0) {
	            s = null;
	            for (Node p = tail; p != node && p != null; p = p.prev)
	                if (p.waitStatus <= 0)
	                    s = p;
	        }
	        if (s != null)
	            LockSupport.unpark(s.thread);
	    }       
可以看到最终调用的是 LockSupport.unpark(s.thread)将线程唤起。到这里ReentrantLock基本用法的分析就结束了，可以看到它基本是依靠Unsafe的CAS操作+LockSupport的park/unpark实现了锁同步。从上述分析也可以窥探AbstractQueuedSynchronizer框架的一部分，AbstractQueuedSynchronizer实现了线程队列与唤起的基本框架，将lock/unlock的能力交给外部进行定制，只需要实现AbstractQueuedSynchronizer定制的模板，就可以获得不同的锁，但是核心的阻塞/唤起框架已经定了：靠Node队列+CAS更新操作+Unsafe的睡眠/唤起能力实现。	 
   
### ReentrantLock的Condition

ReentrantLock的Condition用来处理生产者-消费者（producer-consumer）问题，即有界缓冲区（bounded-buffer问题，一个是生产者，一个是消费者，除了临界资源的使用，还牵扯缓冲区空与满的处理，如果没有Condition，则只有临界区的概念，producer-consumer模型就不够清晰，这个模型会**根据身份与缓存的状态，选择性睡眠与唤醒**，而ReentrantLock是无差别的。

* 当缓冲区已经满了，生产者还想放入新的数据，生产者应该休眠，等待消费者从缓冲区中取走数据后再唤醒它。
* 当缓冲区已经空了，消费者还想去取消息，可以让消费者进行休眠，待生产者放数据再唤醒它。

很明显这个时候，单纯靠ReentrantLock处理是不友善的

	class BoundedBuffer {
	   final Lock lock = new ReentrantLock();//锁
	   final Condition notFull  = lock.newCondition();//写条件 
	   final Condition notEmpty = lock.newCondition();//读条件 
	
	   final Object[] items = new Object[100];//缓存队列
	   int putptr/*写索引*/, takeptr/*读索引*/, count/*队列中存在的数据个数*/;
	
	   public void put(Object x) throws InterruptedException {
	     lock.lock();
	     try {
	       while (count == items.length)//如果队列满了 
	         notFull.await();//阻塞写线程
	       items[putptr] = x;//赋值 
	       if (++putptr == items.length) putptr = 0;//如果写索引写到队列的最后一个位置了，那么置为0
	       ++count;//个数++
	       notEmpty.signal();//唤醒读线程
	     } finally {
	       lock.unlock();
	     }
	   }
	
	   public Object take() throws InterruptedException {
	     lock.lock();
	     try {
	       while (count == 0)//如果队列为空
	         notEmpty.await();//阻塞读线程
	       Object x = items[takeptr];//取值 
	       if (++takeptr == items.length) takeptr = 0;//如果读索引读到队列的最后一个位置了，那么置为0
	       --count;//个数--
	       notFull.signal();//唤醒写线程
	       return x;
	     } finally {
	       lock.unlock();
	     }
	   } 
	 }
	 
