## CAS概念：compare-and-swap原子操作

CAS到底是什么，为什么出现？很多文章都说的稀里糊涂，Wiki百科的解释反而是最清晰的

> In computer science, compare-and-swap (CAS) is an atomic instruction used in multithreading to achieve synchronization. It compares the contents of a memory location with a given value and, only if they are the same, modifies the contents of that memory location to a new given value. This is done as a single atomic operation. The atomicity guarantees that the new value is calculated based on up-to-date information; if the value had been updated by another thread in the meantime, the write would fail.

**CAS是一条原子操作指令，用于在多线程编程中实现同步**，CAS本身就是为了多线程同步而出现的，对于单线程编程没有任何意义。在同步上，CAS的立足点是**共享变量**，依赖硬件指令，保证了比较+修改的原子性，每次利用CAS写的时候，都要确保之前GET值未被修改，否则更新失败，该操作可以保证写操作都是基于最新的值计算而来，避免了GET->SET之间线程被被打断而引发的风险。

## 利用CAS实现无锁synchronization 

CAS本身的定位是原子操作，不具备锁或者synchronization的能力，但是它是构建锁的一种非常好用的工具，compare-and-swap的原子性为实现synchronization提供了契机：多个线程更新一个时，只有一个会成功，成功代表已获取锁，失败的代表竞争失败，失败的线程可以选择自旋等待或者睡眠等待，**这就是锁的概念**。可以参考利用AtomicBoolean实现的一个无锁并发编程：先看下在没有锁的情况下会有什么表现：

	 <!--无锁编程：CPU忙等待-->
	static boolean condition=false;
	  @Override
	    public void run() {
	        if (!condition) {
	         <!--这里有漏洞可以钻-->
	           condition = true;
	         <!--临界操作-->
	         condition = false;
	        } }
	        
多线程执行如上逻辑时，会存在问题，如果线程A恰好在执行condition = true这条操作前之前被切走了，还没来得及更新condition，那么其他线程也同样可以满足 if (!condition) ，从而进入临界区，这时会出现多个线程访问临界区从情况，究其原因就是get、compare、set是分开的，如若换成实现了CAS 的AtomicBoolean就可避免上述问题：

    static AtomicBoolean  condition = new AtomicBoolean(false);
	  @Override
    public void run() {
        if (!condition.getAndSet(false, true)) {
           <!--临界资源操作-->
        }  
    }
    
AtomicBoolean的getAndSet保证了GET->SET的原子性，中间没有中断，因而不会存在多个线程同时满足  if (!condition.getAndSet(false, true))的情况，而AtomicBoolean的getAndSet其实就是通过CAS+自旋完成的

    //AtomicBoolean.java
    public final boolean getAndSet(boolean var1) {
        boolean var2;
        do {
            var2 = this.get();
        } while(!this.compareAndSet(var2, var1));
       return var2;
    }

JAVA里的compareAndSet通过Unsafe类实现的，

    public final boolean compareAndSet(boolean var1, boolean var2) {
        int var3 = var1 ? 1 : 0;
        int var4 = var2 ? 1 : 0;
        return unsafe.compareAndSwapInt(this, valueOffset, var3, var4);
    }
    
Unsafe底层在不同平台实现各不相同，不需要过多关心。综上所述，**CAS只能提供原子操作能力，配合CAS+自旋能达到类似synchronization 的目的**，不过这种锁是忙等待，如果临近区执行比较耗时，会造成CPU负担过重。


## 利用CAS实现AbstractQueuedSynchronizer[AQS队列同步器]框架

AbstractQueuedSynchronizer（队列同步器）是并发包的核心，ReentrantLock, Semaphore等都是借助AQS模板实现的，而AQS由是借助CAS与同步队列实现的，AQS会把请求获取锁失败的线程放入一个队列的尾部，然后睡眠。CAS的使用的时机一定是在操作临界资源的时候，请求锁的操作就是一个CAS操作，CAS保证只会有一个线程获取锁成功，失败的就进入睡眠，ReentrantLock是借助AQS实现一个常用锁，支持公平与非公平两种模式，可以通过其用法看CAS在锁上的作用。

# 非公平锁，上来就抢，不关心是不是有其他线程在等待

   static final class NonfairSync extends Sync {
        private static final long serialVersionUID = 7316153563782823691L;

        /**
         * Performs lock.  Try immediate barge, backing up to normal
         * acquire on failure.
         */
        final void lock() {
            if (compareAndSetState(0, 1))
                setExclusiveOwnerThread(Thread.currentThread());
            else
                acquire(1);
        }

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
        
    /**
     * Sync object for fair locks
     */
    static final class FairSync extends Sync {
        private static final long serialVersionUID = -3000897897090466540L;

        final void lock() {
            acquire(1);
        }

        /**
         * Fair version of tryAcquire.  Don't grant access unless
         * recursive call or no waiters or is first.
         */
        protected final boolean tryAcquire(int acquires) {
            final Thread current = Thread.currentThread();
            int c = getState();
            if (c == 0) {
            <!--判断前面是不是有等待的节点-->
                if (!hasQueuedPredecessors() &&
                    compareAndSetState(0, acquires)) {
                    setExclusiveOwnerThread(current);
                    return true;
                }
            }
            else if (current == getExclusiveOwnerThread()) {
                int nextc = c + acquires;
                if (nextc < 0)
                    throw new Error("Maximum lock count exceeded");
                setState(nextc);
                return true;
            }
            return false;
        }
    }

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
 