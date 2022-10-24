升级顺序为 自旋锁->偏向锁->轻量级锁->重量级锁。 

* 偏向锁主要解决在没有对象抢占的情况下
* 一旦出现了两个或多个线程抢占对象操作时，偏向锁就会升级为轻量级锁  轻量级锁同样使用CAS技术进行实现

乐观锁

乐观锁是一种乐观思想，即认为读多写少，遇到并发写的可能性低，每次去拿数据的时候都认为别人不会修改，所以不会上锁，但是在更新的时候会判断一下在此期间别人有没有去更新这个数据，采取在写时先读出当前版本号，然后加锁操作（比较跟上一次的版本号，如果一样则更新），如果失败则要重复读-比较-写的操作。

java中的乐观锁基本都是通过CAS操作实现的，CAS是一种更新的原子操作，比较当前值跟传入值是否一样，一样则更新，否则失败。

悲观锁
悲观锁是就是悲观思想，即认为写多，遇到并发写的可能性高，每次去拿数据的时候都认为别人会修改，所以每次在读写数据的时候都会上锁，这样别人想读写这个数据就会block直到拿到锁。java中的悲观锁就是Synchronized,AQS框架下的锁则是先尝试cas乐观锁去获取锁，获取不到，才会转换为悲观锁，如RetreenLock。
 


## synchronized锁的用法

对象锁

	  public synchronized  void method(){}
	  
	   public void method() {
        	synchronized (object) {}
        }

类锁

	  public static synchronized  void method(){}
	  public void method() {
        	synchronized (XX.class) {}
        }

## synchronized锁的实现


synchronized内置锁是一种对象锁,作用粒度是对象，作用在普通方法上产生的变化

	  public  void test2(){
	  }
  
	public  void  test3(){
		synchronized(this){}
	}
	  public static synchronized void test(){
	  }
	  
	  public synchronized void test(){
	  }

反编译后得到的代码 javap -verbose

<!--普通方法-->
	  public void test2();
	    descriptor: ()V
	    flags: ACC_PUBLIC
	    Code:
	      stack=0, locals=1, args_size=1
	         0: return
	      LineNumberTable:
	        line 6: 0

对于加在代码快的synchronized

	<!--代码快-->
	  public void test3();
	    descriptor: ()V
	    flags: ACC_PUBLIC
	    Code:
	      stack=2, locals=3, args_size=1
	         0: aload_0
	         1: dup
	         2: astore_1
	         3: monitorenter
	         4: aload_1
	         5: monitorexit
	         6: goto          14
	         9: astore_2
	        10: aload_1
	        11: monitorexit
	        12: aload_2
	        13: athrow
	        14: return
 
多了一些代码，尤其是monitorenter，monitorexit比较显眼，


每个对象都与一个monitor 相关联。当且仅当拥有所有者时（被拥有），monitor才会被锁定。执行到monitorenter指令的线程，会尝试去获得对应的monitor，如下：
每个对象维护着一个记录着被锁次数的计数器, 对象未被锁定时，该计数器为0。线程进入monitor（执行monitorenter指令）时，会把计数器设置为1.
当同一个线程再次获得该对象的锁的时候，计数器再次自增.
当其他线程想获得该monitor的时候，就会阻塞，直到计数器为0才能成功。

monitor的拥有者线程才能执行 monitorexit指令。
线程执行monitorexit指令，就会让monitor的计数器减一。如果计数器为0，表明该线程不再拥有monitor。其他线程就允许尝试去获得该monitor了。


这两条会让对象在执行，使其锁计数器加1或者减1。每一个对象在同一时间只与一个monitor(锁)相关联，而一个monitor在同一时间只能被一个线程获得，一个对象在尝试获得与这个对象相关联的Monitor锁的所有权的时候，monitorenter指令会发生如下3中情况之一： monitor计数器为0，意味着目前还没有被获得，那这个线程就会立刻获得然后把锁计数器+1，一旦+1，别的线程再想获取，就需要等待 如果这个monitor已经拿到了这个锁的所有权，又重入了这把锁，那锁计数器就会累加，变成2，并且随着重入的次数，会一直累加 这把锁已经被别的线程获取了，等待锁释放。
 
领已中欧冠是通过方法访问标识符	        实现的
	        
	  public static synchronized void tests();
	    descriptor: ()V
	    flags: ACC_PUBLIC, ACC_STATIC, ACC_SYNCHRONIZED
	    Code:
	      stack=0, locals=0, args_size=0
	         0: return
	      LineNumberTable:
	        line 3: 0
	        
	  public synchronized void test();
	    descriptor: ()V
	    flags: ACC_PUBLIC, ACC_SYNCHRONIZED
	    Code:
	      stack=0, locals=1, args_size=1
	         0: return
	      LineNumberTable:
	        line 4: 0

	        
可以看到跟普通方法的区别 在     flags: ACC_PUBLIC, ACC_SYNCHRONIZED，多了一个ACC_SYNCHRONIZED标志。方法级别的同步是隐式的，作为方法调用的一部分，当调用一个ACC_SYNCHRONIZED标志的方法，线程也需要先获得monitor锁，然后开始执行方法，方法执行之后再释放monitor锁。如果在方法执行过程中，发生了异常，那么在异常被抛到方法外之前，监视器锁会被自动释放。

**同步方法和同步代码块都是通过monitor来实现的**，对象与monito一对一，线程可以占有或者释放monitor

### 锁升级

synchronized早期完全属于悲观锁，而且完全是重量级锁，一旦牵扯锁竞争，就必定走线程的睡眠与唤醒，这里势必会走内核态与用户态的状态切换，开销非常大，可能睡眠唤醒的代价比代码执行的代价还要高，后期的JDK版本对synchronized进行了优化，有了一个 无锁-->偏向锁-->轻量级锁-->重量级锁的升级过程，除了重量级锁，其他的都不牵扯线程的睡眠唤醒，甚至都可以看做是无锁状态，简单看下各个阶段的表现跟原理。

无锁 -> 偏向锁 -> 轻量级锁 （自旋锁）-> 重量级锁,锁可以升级但是不可以降级

#### 无锁跟偏向锁

其实个人感觉无锁跟偏向锁基本算是一个意思，作用也基本类似，默认偏向锁的开关是开启的，一个对象被创建后，MarkWord字段应该是无锁状态还是偏向锁状态，跟其创建的时机有一些关系，虚拟机启动前几秒创建的都是non-biasable的，
	
* 1  虚拟机启动就创建对象的MarkWord值


		OFF  SZ   TYPE DESCRIPTION               VALUE
		  0   8        (object header: mark)     0x0000000000000001 (non-biasable; age: 0)
		  8   4        (object header: class)    0xf80001e5
		 12   4        (object alignment gap)   
	 
	 
* 2 虚拟机启动1s后创建对象的MarkWord值

		OFF  SZ   TYPE DESCRIPTION               VALUE
		  0   8        (object header: mark)     0x0000000000000001 (non-biasable; age: 0)
		  8   4        (object header: class)    0xf80001e5
		 12   4        (object alignment gap)    
 
	
* 3 虚拟机启动3s后创建对象的MarkWord值

		OFF  SZ   TYPE DESCRIPTION               VALUE
		  0   8        (object header: mark)     0x0000000000000005 (biasable; age: 0)
		  8   4        (object header: class)    0xf80001e5
		 12   4        (object alignment gap)    
		 
* 4 虚拟机启动4s后创建对象的MarkWord值

		OFF  SZ   TYPE DESCRIPTION               VALUE
		  0   8        (object header: mark)     0x0000000000000005 (biasable; age: 0)
		  8   4        (object header: class)    0xf80001e5
		 12   4        (object alignment gap)    		 

可以看到挺神奇的表现，对象初始化的状态跟虚拟机的启动时间有关系，前几秒的是non-biasable，后面的都是biasable不过这个并没有太大的影响，无锁跟偏向锁的作用基本是一样，两个状态个人人为基本可以等同不。过在用synchronized获取对象锁后，两者的表现是不一样的，non-biasable对象的锁会升级为轻量级锁，而biasable的会成为偏向锁状态biasable，biasable状态的MarkWord前面会填充线程ID，只要填充色上线程ID，无锁与偏向锁的区别才能体现，没有填充线程ID的biasable与non-biasable是没啥区别。但是non-biasable的会直接升级轻量级锁
	
* 5  synchronized获取对象锁之后，对象的MarkWord值
 
		OFF  SZ   TYPE DESCRIPTION               VALUE
		  0   8        (object header: mark)     0x00007fdf6910d005 (biased: 0x0000001ff7da4434; epoch: 0; age: 0)
		  8   4        (object header: class)    0xf80001e5
		 12   4        (object alignment gap)    



偏向锁就是在运行过程中，对象的锁偏向某个线程。即在开启偏向锁机制的情况下，某个线程获得锁，当该线程下次再想要获得锁时，**同一个线程再次请求该锁的时候，无需做任何同步 [比如CAS自旋]**，直接就可以执行同步代码，比较适合竞争较少的情况。

偏向锁的目标是，减少无竞争且只有一个线程使用锁的情况下，使用轻量级锁而产生的性能消耗。轻量级锁每次申请、释放锁都至少需要一次CAS，但偏向锁只有初始化时需要一次CAS。

1.1 偏向锁获取过程

* 查看Mark Word中偏向锁的标识以及锁标志位，若是否为偏向锁为1，并且锁标志位为01，则该锁为可偏向状态。
* 若该锁为可偏向状态，判断Mark Word中的**线程ID与当前线程ID是否相等，如果相同，则直接执行同步代码**，否则通过CAS操作竞争锁，先对比，再竞争
* 如果竞争成功，将Mark Word中线程ID设置为当前线程ID，然后执行同步代码。
* 如果竞争失败，说明有其他线程竞争。持有偏向锁状态的线程在没有字节码正在执行的情况下释放锁，然后恢复到未锁定状态或者膨胀为轻量级锁。

1.2 偏向锁释放过程
持有偏向锁的线程不会主动释放锁，只有遇到其他线程尝试竞争偏向锁时，持有偏向锁状态的线程才会释放锁。持有持有偏向锁的线程需要等到所有的同步任务执行完成之后（即没有字节码正在执行），才会暂停持有偏向锁的线程，然后恢复到未锁定状态或者膨胀为轻量级锁。


### monitor监视器

montor到底是什么呢？我们接下来剥开Synchronized的第三层，monitor是什么？ 它可以理解为一种同步工具，或者说是同步机制，它通常被描述成一个对象。操作系统的管程是概念原理，ObjectMonitor是它的原理实现。



Mark Word : 用于存储对象自身的运行时数据，它是实现轻量级锁和偏向锁的关键。



可重入锁：又名递归锁，是指在同一个线程在外层方法获取锁的时候，再进入该线程的内层方法会自动获取锁（前提锁对象得是同一个对象或者class），不会因为之前已经获取过还没释放而阻塞。

 
简单来说在JVM中monitorenter和monitorexit字节码依赖于底层的操作系统的Mutex Lock来实现的，但是由于使用Mutex Lock需要将当前线程挂起并从用户态切换到内核态来执行

优化，只需要依靠一条CAS原子指令就可以完成锁的获取及释放。当存在锁竞争的情况下，执行CAS指令失败的线程将调用操作系统互斥锁进入到阻塞状态，当锁被释放的时候被唤醒(具体处理步骤下面详细讨论)。
 

轻量级锁(Lightweight Locking)：这种锁实现的背后基于这样一种假设，即在真实的情况下我们程序中的大部分同步代码一般都处于无锁竞争状态(即单线程执行环境)，在无锁竞争的情况下完全可以避免调用操作系统层面的重量级互斥锁，取而代之的是在monitorenter和monitorexit中只需要依靠一条CAS原子指令就可以完成锁的获取及释放。当存在锁竞争的情况下，执行CAS指令失败的线程将调用操作系统互斥锁进入到阻塞状态，当锁被释放的时候被唤醒(具体处理步骤下面详细讨论)。 偏向锁(Biased Locking)：是为了在无锁竞争的情况下避免在锁获取过程中执行不必要的CAS原子指令，因为CAS原子指令虽然相对于重量级锁来说开销比较小但还是存在非常可观的本地延迟。 适应性自旋(Adaptive Spinning)：当线程在获取轻量级锁的过程中执行CAS操作失败时，在进入与monitor相关联的操作系统重量级锁(mutex semaphore)前会进入忙等待(Spinning)然后再次尝试，当尝试一定的次数后如果仍然没有成功则调用与该monitor关联的semaphore(即互斥锁)进入到阻塞状态。



​自旋锁早在JDK1.4 中就引入了，只是当时默认时关闭的。在JDK 1.6后默认为开启状态。自旋锁本质上与阻塞并不相同，先不考虑其对多处理器的要求，如果锁占用的时间非常的短，那么自旋锁的性能会非常的好，相反，其会带来更多的性能开销(因为在线程自旋时，始终会占用CPU的时间片，如果锁占用的时间太长，那么自旋的线程会白白消耗掉CPU资源)。因此自旋等待的时间必须要有一定的限度，如果自旋超过了限定的次数仍然没有成功获取到锁，就应该使用传统的方式去挂起线程了，在JDK定义中，自旋锁默认的自旋次数为10次，用户可以使用参数-XX:PreBlockSpin来更改。

## synchronized与Lock的区别

1）Lock是一个接口，而synchronized是Java中的关键字，synchronized是内置的语言实现；
2）当synchronized块结束时，会自动释放锁，lock一般需要在finally中自己释放。synchronized在发生异常时，会自动释放线程占有的锁，因此不会导致死锁现象发生；而Lock在发生异常时，如果没有主动通过unLock()去释放锁，则很可能造成死锁现象，因此使用Lock时需要在finally块中释放锁；
3）lock等待锁过程中可以用interrupt来终端等待，而synchronized只能等待锁的释放，不能响应中断。
4）lock可以通过trylock来知道有没有获取锁，而synchronized不能； 
5）当synchronized块执行时，只能使用非公平锁，无法实现公平锁，而lock可以通过new ReentrantLock(true)设置为公平锁，从而在某些场景下提高效率。
6）LLock可以提高多个线程进行读操作的效率。（可以通过readwritelock实现读写分离）
7）synchronized 锁类型可重入 不可中断 非公平 而 lock 是： 可重入 可判断 可公平（两者皆可） 
在性能上来说，如果竞争资源不激烈，两者的性能是差不多的，而当竞争资源非常激烈时（即有大量线程同时竞争），此时Lock的性能要远远优于synchronized。所以说，在具体使用时要根据适当情况选择。 

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

### 自旋

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
 
 
 https://blog.csdn.net/yinwenjie/article/details/84922958
 
 
###  自旋

如果对于那些需要同步的简单的代码块，获取锁挂起操作消耗的时间比用户代码执行的时间还要长，这种同步策略显然非常糟糕的


自旋锁（spinlock）：是指当一个线程在获取锁的时候，如果锁已经被其它线程获取，那么该线程将循环等待，然后不断的判断锁是否能够被成功获取，直到获取到锁才会退出循环，获取锁的线程一直处于活跃状态，但是并没有执行任何有效的任务，使用这种锁会造成busy-waiting