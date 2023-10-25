> volatile - 用来修饰变量，告诉编译器不要对这个变量进行优化，保证变量的可见性和访问的有序性【禁止指令重排】，但它并不提供原子性操作，在多线程编程中还需要结合其他同步机制，如互斥锁或信号量。
 
### 可见性与Java的内存模型

可见性是volatile最突出的一个作用，它指的是：一个线程对**共享变量**的修改，另一个线程可以**立刻感知**到。为什么会有可见性问题，这跟高速缓存这个概念分不开，从硬件上来看，为了缓解存储跟CPU速度的鸿沟，CPU添加了一快高速缓存，数据使用不会频繁的读取主存，而是先将主存数据读取到高速缓存，然后读写的都是高速缓存，计算结束后，才会更新主存，目前的处理器一般是多核CPU+多个独立高速缓存，因此，同一时刻，共享变量在这几块高速缓存的值会存在不一致的可能性，那更新主存也可能存在不确定性，这就是缓存一致性问题。

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/59dc737dae3345aaa4178d587060c8fb~tplv-k3u1fbpfcp-watermark.image?)

比如，多任务同时处理一个共享变量，并加1，最终写入主存的值不一定加2，因为高速缓存的存在，导致最终更新到主存的值可能只会加1，对于这种问题，硬件厂商的做法是添加缓存一致性协议

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e8561379c17e473a87b7cf4b2a95b841~tplv-k3u1fbpfcp-watermark.image?)

这里不必过度关注硬件侧面的实现，只要认清协议最大的作用是：高速缓存对于共享变量的修改，会及时同步到其他高速缓存，防止使用旧数据。有一个与之相关的概念：内存模型，即在多核多线程环境下，CPU与内存交互的模型，如何对内存或高速缓存进行读写。

Java 内存模型是建立在CPU模型的基础上的，对各种平台做了抽象，屏蔽了各种硬件和操作系统的内存访问差异，让 Java 程序在各种平台都能达到一致的内存访问效果。定义了程序中各个变量的访问规则，如何对**共享变量**进行存取，属性字段、静态字段和构成数组对象的元素，但不包括局部变量与方法参数，因为后者是线程私有的。Run-Time Data Areas可分为6部分[参考](https://docs.oracle.com/javase/specs/jvms/se8/html/jvms-2.html#jvms-2.5)，有些随虚拟机创建而创建，有些随线程创建而创建：

* pc Register：程序计数器  存储JVM当前线程执行指令的地址，为线程所有
* Java Virtual Machine Stacks  虚拟机栈，随着线程的创建而创建，记录线程中方法调用的信息，每次调用生成一个栈帧，每个线程栈帧包含方法的局部变量、参数、方法返回地址等
* Native Method Stacks 本地(原生)方法栈
*  Heap 堆 ，所有线程共享，随虚拟机启动创建，用来存储对象
*  Method Area 方法区 ，所有线程共享，主要存放类结构、方法、类成员定义，static 静态成员等
*  Run-Time Constant Pool  运行时常量池，比如字符串

如果粗粒度的话，可以认为，只有两种，一种是堆一种是栈，按是否被线程共享，内存可分为

 ![](https://static001.infoq.cn/resource/image/b4/62/b4ff890142874a6cbef1ad7a80eb7462.png) 
 
 volatile关键字，所关注主要是线程间可以共享的数据，JVM内存模型有一些规则

*  Java所有变量都存储在主内存中
*  每个线程都有自己独立的工作内存，Java线程对变量的所有操作都必须在**本地内存中进行**，而不能直接读写主内存。
*  调用栈和方法的本地变量存放在线程栈上
*  本地变量如果是引用，引用本身存放在线程栈上，对象放在堆上。
*  对象的成员随对象存放在堆上 
*  静态成员存放在堆上
*  堆上的对象可以被多线程访问，线程访问对象的成员变量时，都在线程的本地内存中拥有这个成员变量的私有拷贝。
*  不同的线程之间无法直接访问对方本地内存中的变量，线程间变量值的传递需要通过主内存来完成

线程本地存储空间类似于CPU的高速缓存，因此也会存在缓存不一致问题，

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/afef663c05084bc0af2ed786834d2bb3~tplv-k3u1fbpfcp-watermark.image?)

volatile关键字可以解决这个问题，volatile的特殊规则能保证了新值能被立即同步到主内存，线程在使用volatile变量前都立即从主内存刷新，volatile可见性借助了CPU的lock指令。也可以从工作内存跟主内存这个方向去理解，

	read读取->load加载->use使用->assign赋值->store存储->write写入->lock锁定->unlock解锁

* read: 作用于主内存，将变量的值从主内存传输到工作内存，主内存到工作内存
* load: 作用于工作内存，将read从主内存传输的变量值放入工作内存变量副本中，即数据加载
* use: 作用于工作内存，将工作内存变量副本的值传递给执行引擎，每当JVM遇到需要该变量的字节码指令时会执行该操作
* assign: 作用于工作内存，将从执行引擎接收到的值赋值给工作内存变量，每当JVM遇到一个给变量赋值字节码指令时会执行该操作
* store: 作用于工作内存，将赋值完毕的工作变量的值写回给主内存
* write: 作用于主内存，将store传输过来的变量值赋值给主内存中的变量
* 由于上述只能保证单条指令的原子性，针对多条指令的组合性原子保证，没有大面积加锁，所以，JVM提供了另外两个原子指令：
* lock: 作用于主内存，将一个变量标记为一个线程独占的状态，只是写时候加锁，就只是锁了写变量的过程。
* unlock: 作用于主内存，把一个处于锁定状态的变量释放，然后才能被其他线程占用
 
 volatile有关禁止指令重排的行为


当第一个操作是 volatile 读时，不论第二个操作是什么，都不能重排序；这个操作保证了volatile读之后的操作不会被重排到volatile读之前
当第二个操作为 volatile 写时，不论第一个操作是什么，都不能重排序；这个操作保证了volatile写之前的操作不会被重排到volatile写之后
当第一个操作为 volatile 写时，第二个操作为 volatile 读时，不能重排序
 

 在每一个 volatile 写操作前面插入一个 storestore 屏障
在每一个 volatile 写操作后面插入一个 storeload 屏障
在每一个 volatile 读操作后面插入一个 loadload 屏障
在每一个 volatile 读操作后面插入一个 loadstore 屏障

 

### 有序性性【禁止指令重排】：主要是多线程场景
	
	public class Singleton {
	    private static Singleton uniqueSingleton;
	
	    private Singleton() {
	    }
	
	    public synchronized Singleton getInstance() {
	        if (null == uniqueSingleton) {
	            uniqueSingleton = new Singleton();
	        }
	        return uniqueSingleton;
	    }
	}

	 public class Singleton {
	    private volatile static Singleton uniqueSingleton;
	
	    private Singleton() {
	    }
	
	    public Singleton getInstance() {
	        if (null == uniqueSingleton) {
	            synchronized (Singleton.class) {
	                if (null == uniqueSingleton) {
	                    uniqueSingleton = new Singleton();
	                }
	            }
	        }
	        return uniqueSingleton;
	    }
	}
	
	
	memory =allocate();    //1：分配对象的内存空间 
	
	instance =memory;     //3：instance指向刚分配的内存地址，此时对象还未初始化
	
	ctorInstance(memory);  //2：初始化对象
	
	
	JMM内存屏障插入策略：


在每个volatile写操作的前面插入一个StoreStore屏障。

在每个volatile写操作的后面插入一个StoreLoad屏障。

在每个volatile读操作的后面插入一个LoadLoad屏障。

在每个volatile读操作的后面插入一个LoadStore屏障。
 

内存屏障


	
	
例子1：A线程指令重排导致B线程出错，对于在同一个线程内，这样的改变是不会对逻辑产生影响的，但是在多线程的情况下指令重排序会带来问题。看下面这个情景:
	
	在线程A中:
	
	context = loadContext();
	
	inited = true;
	

	在线程B中:
	
	while(!inited ){ //根据线程A中对inited变量的修改决定是否使用context变量
	
	   sleep(100);
	
	}
	
	doSomethingwithconfig(context);
	
	 
	假设线程A中发生了指令重排序:
	
	inited = true;
	
	context = loadContext();
	
	 
	
那么B中很可能就会拿到一个尚未初始化或尚未初始化完成的context,从而引发程序错误。
	
	 

## 	参考文档

https://www.cnblogs.com/dolphin0520/p/3920373.html

[Java中的双重检查锁（double checked locking）](https://www.cnblogs.com/xz816111/p/8470048.html)

[一文看懂 JVM 内存布局及 GC 原理](https://www.infoq.cn/article/3wyretkqrhivtw4frmr3)