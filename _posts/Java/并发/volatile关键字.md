volatile - 保证可见性和有序性

### 可见性与Java的内存模型

可见性是volatile最突出的一个作用，什么是可见性，可见性是多核CPU+多线程编程才会遇到的一个问题，它指的是：一个线程对**共享变量**的修改，另一个线程可以**立刻感知**到。为什么多核CPU+多线程会遇到可见性问题呢？这跟多核CPU的高速缓存有关系，为了缓解存储跟CPU速度的鸿沟，CPU添加了一快高速缓存，数据的使用不会频繁的读取主存，而是先将主存数据读取到高速缓存，然后读写的都是高速缓存，计算结束后，才会更新主存，在多核的场景中，有多块相互独立的高速缓存，同一时刻，共享变量在这几块缓存的值会存在不一致的可能性，更新主存也可能存在不确定性，即：缓存一致性。

	        int value=0;
	        new Thread(new Runnable() {
	            @Override
	            public void run() {
	            println("start")
	                if(value==0){
	                    value+=1;
	             println("value ="+value)     
	                }

	            }
	        }).start();
	        
	        new Thread(new Runnable() {
	            @Override
	            public void run() {
	            println("start")
	                if(value==0){
	                    value+=1;
	             println("value ="+value)     
	                }
	            }
	        }).start();

比如上述的代码，它的输出可能是

	start
	value =1
	start
	value =1

在即使第二个线程的代码在第一个线程执行完value+=1之后执行，第二个线程也不一定人为此时的value=1，因为第一个线程的结果不一定能及时同步给第二个线程，这就是缓存一致性带来的问题，如下图：

![](https://images0.cnblogs.com/blog/288799/201408/212219343783699.jpg)





一个线程对共享变量的修改，另一个线程可以感知到，我们称其为可见性。
	
### 有序性性
	
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
	
## 	参考文档

https://www.cnblogs.com/dolphin0520/p/3920373.html

[Java中的双重检查锁（double checked locking）](https://www.cnblogs.com/xz816111/p/8470048.html)