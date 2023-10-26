> volatile[**多线程场景、共享变量必加**] - 用来修饰变量，告诉编译器**每次都从主存中读取**，不要对这个变量进行优化，从而保证变量的可见性和访问的有序性【禁止指令重排】，但它并不提供原子性操作，在多线程编程中还需要结合其他同步机制，如互斥锁或信号量。
 
### 可见性与Java内存模型

可见性是volatile最突出的一个作用，即：一个线程对**共享变量**的修改，另一个线程可以**立刻感知**到。可见性问题跟高速缓存这个概念分不开，从硬件上来看，为了缓解存储跟CPU速度的鸿沟，CPU添加了一快高速缓存，数据使用不会频繁的读取主存，而是先将主存数据读取到高速缓存，然后读写的都是高速缓存，计算结束后，才会更新主存，目前的处理器一般是多核CPU+多个独立高速缓存，因此，同一时刻，共享变量在这几块高速缓存的值会存在不一致的可能性，更新主存也可能存在不确定性，这就是缓存一致性问题。

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/59dc737dae3345aaa4178d587060c8fb~tplv-k3u1fbpfcp-watermark.image?)

如下，在多线程场景，共用一个boolean变量：

	    private static boolean flag = false;
	
	    public static void main(String[] args) throws InterruptedException {
	
	        new Thread(() -> {
	            while (!flag) {
	
	            }
	            System.out.println("end thread 1");
	        }).start();
	        TimeUnit.SECONDS.sleep(1);
	        new Thread(() -> {
	         <!--原子操作，防止跟可见性混不易理解-->
	            flag = true;
	        }).start();
	        TimeUnit.SECONDS.sleep(1);
       	System.out.println("end main flag =" +flag);
	    }
	    
		输出
		
		----	end main flag =true
		----	end main flag =true
		----	end main flag =true
		----	end main flag =true
	
可以看到，并不是预料中的输出，有一个线程始终没有结束，这是因为处理器并不是直接操作主内存，每个线程都有自己的工作内存【或者理解成高速缓存】，处理器直接处理的是高速缓存，即使一个线程改变了工作内存的值，甚至同步到主内存，另一个线程也并不一定能够及时感知到，这就导致了上述结果的产生，为了解决这个问题就有了：**缓存一致性协议**。缓存一致性协议即：**对于高速缓存中共享变量的修改，会及时同步到其他缓存，防止使用旧数据**。


![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e8561379c17e473a87b7cf4b2a95b841~tplv-k3u1fbpfcp-watermark.image?)


Java 内存模型是建立在CPU模型的基础上的，对各种平台做了抽象，屏蔽了各种硬件和操作系统的内存访问差异，让 Java 程序在各种平台都能达到一致的内存访问效果。定义了程序中各个变量的访问规则，如何对**共享变量**进行存取，属性字段、静态字段和构成数组对象的元素，但不包括局部变量与方法参数，因为后者是线程私有的。Run-Time Data Areas可分为6部分[参考](https://docs.oracle.com/javase/specs/jvms/se8/html/jvms-2.html#jvms-2.5)，有些随虚拟机创建而创建，有些随线程创建而创建：

* pc Register：程序计数器  存储JVM当前线程执行指令的地址，为**线程**所有
* Java Virtual Machine Stacks  虚拟机栈，随着**线程的创建**而创建，记录线程中方法调用的信息，每次调用生成一个栈帧，每个线程栈帧包含方法的局部变量、参数、方法返回地址等
* Native Method Stacks 本地(原生)方法栈
*  Heap 堆 ，所有**线程共享**，随虚拟机启动创建，用来存储对象
*  Method Area 方法区 ，所有**线程共享**，主要存放类结构、方法、类成员定义，static 静态成员等
*  Run-Time Constant Pool  运行时常量池，比如字符串

如果粗粒度的话，可以认为，只有两种，一种是堆一种是栈，按是否被线程共享，内存可分为

 ![](https://static001.infoq.cn/resource/image/b4/62/b4ff890142874a6cbef1ad7a80eb7462.png) 
 
volatile关键字主要关注线程间可以共享的数据，JVM内存模型有一些规则

*  Java所有**变量**都存储在主内存中
*  每个线程都有自己独立的工作内存，Java线程**对变量的所有操作**都必须在**本地内存中进行**，而不能直接读写主内存。
*  调用栈和方法的本地变量存放在线程栈上
*  本地变量如果是引用，引用本身存放在线程栈上，对象放在堆上。
*  对象的成员随对象存放在堆上 
*  静态成员存放在堆上
*  堆上的对象可以被多线程访问，线程访问对象的成员变量时，都在线程的本地内存中拥有这个成员变量的私有拷贝。
*  不同的线程之间无法直接访问对方本地内存中的变量，**线程间变量值的传递需要通过主内存来完成**

线程本地存储空间类似于CPU的高速缓存，因此也会存在缓存不一致问题，在共享变量前加上volatile关键字即可，volatile可以强制写主内存，触发缓存一致性协议，将高速缓存【工作缓存】无效化，任何操作都是在主内存，而且，每次使用都会先判断是变量已经失效。

* 将当前处理器的缓存行的数据写回到系统内存，同时使其他CPU里缓存了该内存地址的数据置为无效。
* 没有volatile修饰的变量在工作内存操作完成后，并不知道处理器何时将缓存数据写回到内存。
* 加了volatile修饰的变量进行写操作，会直接写回到主存，启动缓存一致性协议。
* 处理器就会通过嗅探在总线上传播的数据来检查自己缓存的数据是否已过期，过期的话会将自己缓存行缓存的数据设置为无效，再次使用，则重新读取。
 


改进后的代码，注意这里只有简单的赋值操作，不需要考虑原子操作之类的问题，只考虑可见性：

    	private static volatile boolean flag = false;

输出：
	    
	 		end thread 1
			end main flag =true
		
volatile借助CPU的lock指令，每次写都会强制写入主内存，只要强制写主内存，触发缓存一直协议，就可以一致了。
	
### volatile不保证原子性[i++]

Java只能保证最基本赋值操作是原子性的，复杂操作的原子性需要通过加锁来解决，看如下例子：

	x = 10; 		//原子操作
	x++; 		//非原子操作
	y = x ; 	//非原子操作

以x++为例，它包括读取、加1、写入三个操作，所以存在执行两次x++，但是只加了1的情况， 

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e51a454535684033b452737b5150b93e~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=1162&h=644&s=62422&e=png&b=ffffff)

反过来想，如果volatile解决了++ --这些自增自减的原子性操作问题，那就不会再有原子类的必要了，验证代码如下，

	  private volatile  static int count = 0;
		    public static void main(String[] args) throws InterruptedException {
		
		        new Thread(() -> {
		            for (int i = 0; i < 10000; i++) {
		                count++;
		            }
		        }).start();
		
		        new Thread(() -> {
		            for (int i = 0; i < 10000; i++) {
		                count++;
		            }
		        }).start();
		
		        TimeUnit.SECONDS.sleep(1);
		        System.out.println(count);
		    }

	输出
	
	----	20000
	----	20000
	----	20000
	----	18351
	----	20000
	
添加volatile之后，虽然输出20000的概率蛮大的，但是还是会出现预料外的结果。所以，即使用volatile修饰变量，也无法保证变量++操作的原子性，也就是：多个线程并发执行了10000次++，实际上并不一定得到+10000的效果，所以volatile还是要配合同步锁的操作， 锁记得锁的是操作，是针对共享数据的操作，那么针对某个操作如果想要获得原子性，那么就要对这个这操作用加一个锁，读写都要，防止用的时候，不经意被更新了。
 
	 		  private volatile  static int count = 0;
			    public static void main(String[] args) throws InterruptedException {
			
			        new Thread(() -> {
			            for (int i = 0; i < 10000; i++) {
			                count++;
			            }
			        }).start();
			
			        new Thread(() -> {
			            for (int i = 0; i < 10000; i++) {
			                count++;
			            }
			        }).start();
			
			        TimeUnit.SECONDS.sleep(1);
			        System.out.println(count);
			    }

### 有序性性【禁止指令重排】：内存屏障

先看一个经典的有问题的单利写法：双重检查锁模式非安全
 
	 public class Singleton {
	    private   static Singleton uniqueSingleton;     private Singleton() {  }
	   
	    public Singleton getInstance() {
	        if (null == uniqueSingleton) { // 解决上来就 synchronized的低效率问题
	            synchronized (Singleton.class) {
	                if (null == uniqueSingleton) {//解决创建过个实例的问题
	                    uniqueSingleton = new Singleton(); //存在有序性问题
	                }
	            }
	        }
	        return uniqueSingleton;
	    }
	}

虽然上述的写法解决了直接用synchronized的效率问题，创建多个实例的BUG问题，但是仍旧存在有序性问题，导致其他线程可能使用未完成初始化的单利对象。new Singleton();是个复杂的过程，栈上分配内存空间->堆上分配并初始化对象->将堆内存地址赋值栈上引用，正常情况下是要对象初始完成后才赋值给uniqueSingleton，但是由于指令执行的顺序可以优化重排，可以先赋值，再初始化， 虽然对于整个函数的执行没影响，但是多线程情况下会有bug，先赋值之后，uniqueSingleton就不是null，其他线程可以直接用，很可能这个时候初始化，还没完成。这个时候就需要利用volatile的有序性来解决。
 
	 public class Singleton {
	    private   static  volatile Singleton uniqueSingleton;     private Singleton() {  }
	    public Singleton getInstance() {
	        if (null == uniqueSingleton) { // 解决上来就 synchronized的低效率问题
	            synchronized (Singleton.class) {
	                if (null == uniqueSingleton) {//解决创建过个实例的问题
	                    uniqueSingleton = new Singleton(); //存在有序性问题
	                }
	            }
	        }
	        return uniqueSingleton;
	    }
	}

volatile可以保证uniqueSingleton赋值前的指令不能重排，就避免了先赋值，后初始化的问题。那么volatile是通过什么技术来解决这个问题的？

* read: 作用于主内存，将变量的值从主内存传输到工作内存，主内存到工作内存
* load: 作用于工作内存，将read从主内存传输的变量值放入工作内存变量副本中，即数据加载
* use: 作用于工作内存，将工作内存变量副本的值传递给执行引擎，每当JVM遇到需要该变量的字节码指令时会执行该操作
* assign: 作用于工作内存，将从执行引擎接收到的值赋值给工作内存变量，每当JVM遇到一个给变量赋值字节码指令时会执行该操作
* store: 作用于工作内存，将赋值完毕的工作变量的值写回给主内存
* write: 作用于主内存，将store传输过来的变量值赋值给主内存中的变量
* 由于上述只能保证单条指令的原子性，针对多条指令的组合性原子保证，没有大面积加锁，所以，JVM提供了另外两个原子指令：
* lock: 作用于主内存，将一个变量标记为一个线程独占的状态，只是写时候加锁，就只是锁了写变量的过程。
* unlock: 作用于主内存，把一个处于锁定状态的变量释放，然后才能被其他线程占用
 
 
 读操作时在读指令use之前插入读屏障，重新从主存加载最新值进来，让工作内存中的数据失效，强制从新从主内存加载数据。（读屏障保证在该屏障之后，对共享变量的读取，加载的是主存中最新数据 ）**读屏障会确保指令重排序时，不会将读屏障之后的代码排在读屏障之前**

写操作时在写指令assign之后插入写屏障，能让写入工作内存中的最新数据更新写入主内存，让其他线程可见。（写屏障保证在该屏障之前的，对共享变量的改动，都同步到主存当中，其他线程就可以读到最新的结果了 ），**写屏障会确保指令重排序时，不会将写屏障之前的代码排在写屏障之后**，因此，写屏障会保证在对象new对象、初始化等操作再写之前，不会跑到写操作之后。

## 	参考文档

https://www.cnblogs.com/dolphin0520/p/3920373.html

[Java中的双重检查锁（double checked locking）](https://www.cnblogs.com/xz816111/p/8470048.html)

[一文看懂 JVM 内存布局及 GC 原理](https://www.infoq.cn/article/3wyretkqrhivtw4frmr3)

[volatile有序性和可见性底层原理](https://blog.csdn.net/qq_42764468/article/details/106898608)