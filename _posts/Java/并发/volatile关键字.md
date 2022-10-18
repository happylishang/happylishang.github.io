volatile - 保证可见性和有序性/Users/personal/Downloads/Android_.fdbk0_hbyxtest52@163.com_2022-10-17_11_38_09_799.txt

### 可见性

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