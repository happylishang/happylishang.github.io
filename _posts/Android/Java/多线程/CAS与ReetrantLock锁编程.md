## CAS概念：compare-and-swap原子操作

CAS到底是什么，为什么出现？很多文章都说的稀里糊涂，Wiki百科的解释反而是最清晰的

> In computer science, compare-and-swap (CAS) is an atomic instruction used in multithreading to achieve synchronization. It compares the contents of a memory location with a given value and, only if they are the same, modifies the contents of that memory location to a new given value. This is done as a single atomic operation. The atomicity guarantees that the new value is calculated based on up-to-date information; if the value had been updated by another thread in the meantime, the write would fail.

**CAS是一条原子操作指令，用于在多线程编程中实现同步**，CAS本身就是为了多线程同步而出现的，对于单线程编程没有任何意义。在同步上，CAS的立足点是**共享变量**，依赖硬件指令，保证了比较+修改的原子性，每次利用CAS写的时候，都要确保之前GET值未被修改，否则更新失败，该操作可以保证写操作都是基于最新的值计算而来，避免了GET->SET之间线程被被打断而引发的风险。

## AtomicBoolean如何利用CAS实现无锁同步

CAS本身的定位是**原子操作**，不具备锁或者synchronization的能力，但它是构建锁的一种非常好的工具，compare-and-swap的原子性为实现synchronization提供了契机：多个线程利用CAS更新一个值时，只有一个会成功，成功代表已获取锁，失败的代表竞争失败，失败的线程可以选择自旋等待或者睡眠等待，**这就是锁的概念**。可以参考利用AtomicBoolean实现的一个无锁并发编程：先看下在没有锁的情况下会有什么表现：

	 <!--无锁编程：多线程 CPU忙等待-->
	static volatile boolean condition=false;
	  @Override
	    public void run() {
	        if (!condition) {
	         <!--这里有漏洞可以钻，切走-->
	           condition = true;
	         <!--临界操作-->
	         condition = false;
	        } }
	        
多线程执行如上逻辑时，会存在问题，如果线程A恰好在执行condition = true这条操作前之前被切走了，还没来得及更新condition，那么其他线程也同样可以满足 if (!condition) ，从而进入临界区，出现多个线程访问临界区从情况。究其原因就是对于condition 的get、compare、set是分开的，如若借助CAS就可以避免上述问题，原子类就是利用CAS实现的类，通过这种类可以修复上述风险：

    static  AtomicBoolean  condition = new AtomicBoolean(false);
	  @Override
    public void run() {
    <!--默认false，谁设置为true谁获得成功-->
        if (!condition.getAndSet(true)) {
           <!--临界资源操作-->
           ...           
           <!--恢复-->
           condition.set(false)
        }  
    }
    
AtomicBoolean的getAndSet保证了GET->SET的原子性，中间没有中断，因而不会存在多个线程同时满足 **if (!condition.getAndSet(false, true))**的情况，一定存在且只存在一个线程在某个时间点满足条件。而AtomicBoolean如何实现的呢？其实就是利用CAS+自旋完成的

    //AtomicBoolean.java
    private static final long VALUE;
    public final boolean getAndSet(boolean var1) {
        boolean var2;
        do {
            var2 = this.get();
        } while(!this.compareAndSet(var2, var1));
       return var2;
    }

compareAndSet通过Java中的Unsafe类实现，核心原则就是只有一个线程能够成功！
   
	    public final boolean compareAndSet(boolean expect, boolean update) {
	        return U.compareAndSwapInt(this, VALUE,
	                                   (expect ? 1 : 0),
	                                   (update ? 1 : 0));
	    }

    
Unsafe底层在不同平台实现各不相同，不需要过多关心。综上所述，**CAS其实只能提供原子操作能力，需要CAS配合自旋能才能达到类似synchronization同步锁的目的**，不过这种锁是忙等待，**如果临界区执行比较耗时，其他任务会一直轮训等待，可能会造成CPU负担过重**。


## Java框中的AbstractQueuedSynchronizer[AQS队列同步器]框架：有锁同步[睡眠与唤起]

AbstractQueuedSynchronizer[**队列同步器**]是并发包的核心，ReentrantLock、Semaphore等都是借助AQS模板实现的，而AQS也是借助CAS与同步队列实现的，AQS会把请求获取锁失败的线程放入一个队列的尾部，然后**睡眠**。CAS的使用的时机一定是在操作临界资源的时候，请求锁的操作就是一个CAS操作，CAS保证只会有一个线程获取锁成功，失败的就进入睡眠，下面借助ReentrantLock【**可重入的互斥锁**】来看下AbstractQueuedSynchronizer框架的实现，ReentrantLock是一种常用的Java锁，支持公平与非公平两种模式，公平锁与非公平锁的区别是是否一直遵守先来后到，公平锁：直接进入等待队列，先进入先唤醒，而非公平锁支持先来一次抢断，抢断不成功，再退化成排队。实现如下：

> 非公平锁类似于强制加塞+交警执法，加塞成功，直接抢断，加塞失败，被罚到后面排队


首先看下ReentrantLock的构造函数，很明显可以看出，ReentrantLock默认无参构造方法实现的是非公平锁

    public ReentrantLock() {
        sync = new NonfairSync();
    }

如果需要使用公平锁，则需要使用有参构造函数

    public ReentrantLock(boolean fair) {
        sync = fair ? new FairSync() : new NonfairSync();
    }

NonfairSync与FairSync均继承自ReentrantLock中的静态内部类Sync类，结构

![结构图](https://img-blog.csdnimg.cn/186fd44b30564f82ad1cb043dcc15ef8.png)

### 加锁流程 -公平锁

ReentrantLock在使用时候，一般是

	reentrantLock.lock();
	<!--临界代码-->
	...
	reentrantLock.unLock();

先看下公平锁lock的实现

	    static final class FairSync extends Sync {
	        private static final long serialVersionUID = -3000897897090466540L;
	
	        final void lock() {
	            acquire(1);
	        }
	        
acquire调用的其实是父类AbstractQueuedSynchronizer的acquire方法，acquire进一步调用子类tryAcquire以及自身的acquireQueued，如果无法获取锁，并且满足某些条件则进入睡眠

    public final void acquire(int arg) {
        if (!tryAcquire(arg) &&
            acquireQueued(addWaiter(Node.EXCLUSIVE), arg))
            selfInterrupt();
    }

如果tryAcquire直接就获取成功的话，是无需睡眠的，tryAcquire逻辑如下

	        protected final boolean tryAcquire(int acquires) {
	            final Thread current = Thread.currentThread();
	            
			<!--  获取用于同步的state   private volatile int state; 在AbstractQueuedSynchronizer定义-->
	            int c = getState();
	            if (c == 0) {
	            	<!--判断前面是不是有等待的节点，第一次进来肯定没有-->
	                if (!hasQueuedPredecessors() &&
	                    compareAndSetState(0, acquires)) {
	                    setExclusiveOwnerThread(current);
	                    return true;
	                }
	            } 
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
            if (oldTail != null) {
            	//给定对象的指定偏移地址的地方设值，与此类似操作还有：putInt，putDouble，putLong，putChar等
            	<!--为什么不用赋值？防止切换么？  设置node的前一个是oldTail，可能吧，不让队列设置被中断-->
                U.putObject(node, Node.PREV, oldTail);
                if (compareAndSetTail(oldTail, node)) { //   U.compareAndSwapObject(this, TAIL, expect, update); tail 变量赋值，保证尾部正确性，尾巴是oldTail，新尾巴是node，有且仅有一个线程符合要求
               <!-- 进入锁，有保证，可以赋值-->  
                    oldTail.next = node;
                    return node;
                }
            } else {
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
    
之后回到for(;;)，将该线程新建的Node加入到该队列，这里核心的同步全部依靠Unsafe类的compareAndSet来实现。将其加入到队列之后，即可返回，其他线程可进入addWaiter流程，本线程acquireQueued流程继续，
    
    final boolean acquireQueued(final Node node, int arg) {
        try {
            boolean interrupted = false;
            for (;;) {
                final Node p = node.predecessor();
                <!--判断当前加入的是不是第一个，如果是，并且获取tryAcquire成功，说明正好锁被释放了，可以直接获取，不用睡眠-->
                if (p == head && tryAcquire(arg)) {
                    setHead(node);
                    p.next = null; // help GC
                    return interrupted;
                }
                <!--如果node不是第一个节点，那自己基本上要睡眠的  shouldParkAfterFailedAcquire 找到一个可以唤起自己的前驱节点这里其实就是之前第一个 -->
                if (shouldParkAfterFailedAcquire(p, node) &&
                    parkAndCheckInterrupt())
                    interrupted = true;
            }
        } catch (Throwable t) {
            cancelAcquire(node);
            throw t;
        }
    }
    
其中parkAndCheckInterrupt会利用 LockSupport.park执行真正的挂起，最终其实是Unsafe的park函数，LockSupport(提供park/unpark操作，睡眠与唤起)，是AQS框架中另一个重要类，跟提供CAS的Unsafa共同构建了该体系。

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

Thread.interrupted()是用来判断是否被打断过，如果被打断过，返回true，结果就是不用处理该任务了，已经终结，否则还是要处理的。

### 加锁流程 -非公平锁
       
       

	   static final class NonfairSync extends Sync {
	        private static final long serialVersionUID = 7316153563782823691L;

	        final void lock() {
	        <!--不同点-->
	            if (compareAndSetState(0, 1))
	                setExclusiveOwnerThread(Thread.currentThread());
	            else
	                acquire(1);
	        }
可以看到非公平锁首先会调用CAS尝试将state从0改为1，如果能改成功则表示能够直接获取到锁，那就可以将exclusiveOwnerThread设置为当前线程，不需要公平锁的acquire操作，如果获取不到，则走acquire，走后续流程，不过acquire也跟公平锁有些不同。
	
	        protected final boolean tryAcquire(int acquires) {
	            return nonfairTryAcquire(acquires);
	        }
	    }
		   final boolean nonfairTryAcquire(int acquires) {
		            final Thread current = Thread.currentThread();
		            int c = getState();
		            if (c == 0) {
		            <!--再抢一次-->
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
		        
可以看到非公平锁会调用 nonfairTryAcquire将自己加入等待队列，同公平锁相比，它还是比较**野蛮**的处理方式，直接通过compareAndSetState判断这个时间点是否可以获取锁，如果成功就算获取成功，而公平锁则要看自己是不是第一个，如果是才会去获取，这里之后的流程两者一致都是要睡眠等待唤起。

####   唤起流程

唤起其实是调用的unlock，无论是否是公平锁，这里的处理逻辑都一样
   
     public void unlock() {
        sync.release(1);
    }
        public final boolean release(int arg) {
        if (tryRelease(arg)) {
            Node h = head;
            if (h != null && h.waitStatus != 0)
                unparkSuccessor(h);
            return true;
        }
        return false;
    }

tryRelease的实现在ReetrantLock中，因为ReetrantLock是可重入锁，所以要看看是不是已经到了最外层的锁，只有到了最外层，才算真正的release，

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
 
 之后选择Head之后第一个线程Node进行唤起即可
 
	 
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
	    
可以看到最终调用的是 LockSupport.unpark(s.thread)将线程唤起。到这里ReentrantLock基本用法的分析就结束了，可以看到它基本是**依靠Unsafe的CAS操作+LockSupport的park/unpark实现了锁同步**。从上述分析也可以窥探AbstractQueuedSynchronizer框架的一部分，**AbstractQueuedSynchronizer实现了线程队列与唤起的基本框架**，将lock/unlock的能力交给外部进行定制，只需要实现AbstractQueuedSynchronizer定制的模板，就可以获得不同的锁，但是核心的阻塞/唤起框架已经定了：**靠Node队列+CAS更新操作+Unsafe的睡眠/唤起能力实现**。
  
## CAS的ABA问题

## AtomicInteger中volatile value作用

### volatile 可以保证可见性

：启动两个线程，一个线程修改static 变量，另一个线程读取该变量，看看volatile变量的作用

	public class VolatileTest {
	    final static int COUNT = 5;
	    static int value = 0;
	    public static void main(String[] args) {
	        new Thread(() -> {
	            int tmp = value;
	            while (tmp < COUNT) {
	            <!--读取并使用-->
	                if (value != tmp) {
	                    tmp = value;
	                    System.out.print("\n R "+value + " "+tmp);
	                }
	            }
	        }, "R").start();
	        new Thread(() -> {
	            while (value < COUNT) {
	                value ++;
	                System.out.print("\n W "+value);
	                try {
	                    TimeUnit.SECONDS.sleep(1);
	                } catch (Exception e) {
	                }
	            }
	        }, "W").start();
	    }
	}
	
输出可能是如下情况：	

	 W 1
	 R 1 1
	 W 2
	 W 3
	 W 4
	 W 5
	 结束
	 
可以看到写线程已经将静态变量value更新成了5，但是R线程中看到的value依旧是1，所以R线程可能就在那个地方死循环了，为value加上volatile之后呢？

	    static volatile  int value = 0;

之后输出会变成理想情况：

	 W 1
	 R 1 1
	 W 2
	 R 2 2
	 W 3
	 R 3 3
	 W 4
	 R 4 4
	 W 5
	 R 5 5
	Process finished with exit code 0

所以volatile修饰的变量其可见性会很时时，一个线程修改后，另一个线程会再次用的时候会立即可见。

### 防止指令重排

	public class NoVisibility {
	    private static boolean ready = false;
	    private static int number = 0;
	
	    private static class ReaderThread extends Thread {
	        @Override
	        public void run() {
	            while (!ready) {
	                Thread.yield(); //交出CPU让其它线程工作
	            }
	            System.out.println(number);
	        }
	    }
	
	    public static void main(String[] args) {
	        new ReaderThread().start();
	        number = 42;
	        ready = true;
	    }
	}

在单一线程中，只要重排序不会影响到程序的执行结果，那么就不能保证其中的操作一定按照程序写定的顺序执行，即使重排序可能会对其它线程产生明显的影响。


### synchroniz关键字也能保证可见性

即当ThreadA释放锁M时，它所写过的变量（比如，x和y，存在它工作内存中的）都会同步到主存中，而当ThreadB在申请同一个锁M时，ThreadB的工作内存会被设置为无效，然后ThreadB会重新从主存中加载它要访问的变量到它的工作内存中（这时x=1，y=1，是ThreadA中修改过的最新的值）。通过这样的方式来实现ThreadA到ThreadB的线程间的通信。

 


## 无锁编程与乐观锁

CAS：Compare and Swap，比较再交换，从硬件上说，是一条指令，执行两个动作，为什么要这样做：**因为有时候需要这样来达到及时更新，从而用于无锁编程。**，所以CAS背后其实透露的是一种无锁算法。



CAS到底是什么？是一种操作还是一种思想，还是一个指令

CAS算法


，。

无锁编程，即不使用锁的情况下实现多线程之间的变量同步，也就是在没有线程被阻塞的情况下实现变量的同步，所以也叫非阻塞同步（Non-blocking Synchronization）。



compareAndSwapInt本身是原子操作，不阻塞，本身没有自旋属性，需要外部添加do while才能达到自旋的作用










AtomicBoolean这些类只是提供原子操作的类，本身不算锁，而且本身的初衷是无锁编程，不存在GET，



	// Unsafe.java
	public final int getAndAddInt(Object o, long offset, int delta) {
	   int v;
	   do {
	       v = getIntVolatile(o, offset);
	   } while (!compareAndSwapInt(o, offset, v, v + delta));
	   return v;
	}
	 
	private static final Unsafe unsafe = Unsafe.getUnsafe();
    private static final long valueOffset;

    static {
      try {
        valueOffset = unsafe.objectFieldOffset
            (AtomicReference.class.getDeclaredField("value"));
      } catch (Exception ex) { throw new Error(ex); }
    }

    private volatile V value;

    /**
     * Creates a new AtomicReference with the given initial value.
     *
     * @param initialValue the initial value
     */
    public AtomicReference(V initialValue) {
        value = initialValue;
    }

    /**
     * Creates a new AtomicReference with null initial value.
     */
    public AtomicReference() {
    }
    
     
getAndAddInt()循环获取给定对象o中的偏移量处的值v，然后判断内存值是否等于v。如果相等则将内存值设置为 v + delta，否则返回false，继续循环进行重试，直到设置成功才能退出循环，并且将旧值返回。 整个“比较+更新”操作封装在compareAndSwapInt()中，在JNI里是借助于一个CPU指令完成的，属于原子操作，可以保证多个线程都能够看到同一个变量的修改值。CPU操作成功会里用MESI内存一致模型，让其同步，

后续JDK通过CPU的cmpxchg指令，去比较寄存器中的 A 和 内存中的值 V。如果相等，就把要写入的新值 B 存入内存中。如果不相等，就将内存值 V 赋值给寄存器中的值 A。然后通过Java代码中的while循环再次调用cmpxchg指令进行重试，直到设置成功为止。

 
    System.out.print("普通原子类无法解决ABA问题： ");
            System.out.println(atomicReference.compareAndSet("A", "C") + "\t" + atomicReference.get());
            System.out.print("版本号的原子类解决ABA问题： ");
 
#  synchronized锁原理
 
 Java早期版本中，synchronized属于重量级锁，效率低下，因为监视器锁（monitor）是依赖于底层的操作系统的Mutex Lock来实现的，而操作系统实现线程之间的切换时需要从用户态转换到核心态，这个状态之间的转换需要相对比较长的时间，时间成本相对较高，这也是为什么早期的synchronized效率低的原因。庆幸的是在Java 6之后Java官方对从JVM层面对synchronized较大优化，所以现在的synchronized锁效率也优化得很不错了，Java 6之后，为了减少获得锁和释放锁所带来的性能消耗，引入了轻量级锁和偏向锁，
 
 
 