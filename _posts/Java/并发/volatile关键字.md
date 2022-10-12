volatile - 保证可见性和有序性

### 可见性
	
### 有序性性
	
	public class Singleton {
	    private static Singleton uniqueSingleton;
	
	    private Singleton() {
	    }
	
	    public Singleton getInstance() {
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