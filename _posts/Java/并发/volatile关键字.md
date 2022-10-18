volatile - 保证可见性和有序性

### 可见性与Java的内存模型

可见性是volatile最突出的一个作用，什么是可见性，可见性是多核CPU+多线程编程才会遇到的一个问题，它指的是：一个线程对**共享变量**的修改，另一个线程可以**立刻感知**到。为什么多核CPU+多线程会遇到可见性问题呢？这跟多核CPU的高速缓存有关系，为了缓解存储跟CPU速度的鸿沟，CPU添加了一快高速缓存，数据的使用不会频繁的读取主存，而是先将主存数据读取到高速缓存，然后读写的都是高速缓存，计算结束后，才会更新主存，在多核的场景中，有多块相互独立的高速缓存，同一时刻，共享变量在这几块缓存的值会存在不一致的可能性，更新主存也可能存在不确定性，称为：缓存一致性。

![](https://images0.cnblogs.com/blog/288799/201408/212219343783699.jpg)

为了加快运行速度，于是计算机的设计者在 CPU 中加了一个CPU 。这个 CPU 高速缓存的速度介于 CPU 与内存之间，每次需要读取数据的时候，先从内存读取到CPU缓存中，CPU再从CPU缓存中读取



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