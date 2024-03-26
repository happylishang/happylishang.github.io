模式：在某些场景下，针对某类问题的某种通用的解决方案。单例、工厂、观察者、代理模式、装饰者、使用的频率比较高，其他不是特别高。

* 创建型模式：对象实例化的模式，创建型模式用于解耦对象的实例化过程。
* 结构型模式：把类或对象结合在一起形成一个更大的结构。
* 行为型模式：类和对象如何交互，及划分责任和算法。

### 单例模式：性能与安全的考量

类加载是线程安全的，类加载时候，会初始化静态变量，执行静态代码块，单例的实现模式：静态内部类、双重锁校验、懒汉、饿汉，其中饿汉是靠类加载机制保证、懒汉是靠Syncronize关键字保证，双重锁校验依靠Syncronize限定构造+volatile限制指令重排保证、静态内部类也是靠类加载机制来保证线程安全。
	
	public class SingleToneClass {
	
	    //  懒汉，类加载的时候，可以先不用创建实例 Holder加载时候构建，线程安全不需要volatile
	    private static class Holder {
	        public static SingleToneClass sInstance = new SingleToneClass();
	    }
	
	    public static SingleToneClass getInstance() {
	        return Holder.sInstance;
	    }
	}
	
	//饿汉式
	// class 类在被加载的时候创建Singleton实例，如果对象创建后一直没有使用，则会浪费很大的内存空间，此方法不适合创建大对象。
	class BSingleTone {
	    private BSingleTone() {
	
	    }
	
	    //线程安全，不需要volatile
	    private static BSingleTone sInstance = new BSingleTone();
	
	    public static BSingleTone getInstance() {
	        return sInstance;
	    }
	}
	
	//线程安全的懒汉模式
	class BDoubleSingleTone {
	    private BDoubleSingleTone() {
	
	    }
	
	    //线程安全的懒汉模式
	    private static BDoubleSingleTone sInstance;
	    //  缺点是每次都要同步，因为同步锁在了静态方法上，降低效率，其实只要创建的时候加锁就可以了
		//   不会构造多个，而且，synchronized保证变量的可见性
	    public static synchronized BDoubleSingleTone getInstance() {
	        if (sInstance == null)
	            sInstance = new BDoubleSingleTone();
	        return sInstance;
	    }
	}
	
	//双重校验锁模式
	class BBDoubleSingleTone {
	    private BBDoubleSingleTone() {
	
	    }
	    //要用volatile，变量防止不可见
	    // 优点：支持多线程，并发量高，且以懒汉式加载，不浪费内存空间。
	    // 缺点：一时找不出缺点，非要说缺点的话，就是实现比较麻烦
	    private static volatile BBDoubleSingleTone sInstance;
	
	    // 锁不加方法上，加代码快上
	    public static BBDoubleSingleTone getInstance() {
	        if (sInstance == null) {
	                // 为何先判断为null，因为如果不这样，其实跟静态同步方法没啥区别
	            synchronized (BBDoubleSingleTone.class) {
	                //  一个线程可能正好在其sInstance == null被挂起 ，如果这里不二次判断，可能会构造多次，至于为什么
	                //  synchronized在修改了本地内存中的变量后，解锁前会将本地内存修改的内容刷新到主内存中，
	                // 确保了共享变量的值是最新的，也就保证了可见性。
	                if (sInstance == null)
	                    // 用volatile 是防止编译优化 顺序混淆
	                    // 加锁操作并不能保证同步区内的代码不会发生重排序 加锁保证顺序性，是外部跟内部，不是内部本身的顺序性
	                    sInstance = new BBDoubleSingleTone();
	            }
	        }
	        return sInstance;
	    }
	}
	
### 工厂模式	

## 设计原则：用抽象构建框架，用实现扩展细节
单一职责原则告诉我们实现类要职责单一；里氏替换原则告诉我们不要破坏继承体系；依赖倒置原则告诉我们要面向接口编程；接口隔离原则告诉我们在设计接口的时候要精简单一；迪米特法则告诉我们要降低耦合。而开闭原则是总纲，他告诉我们要对扩展开放，对修改关闭。

### 单一职责      

### 里氏替换原则 

### 依赖倒置：核心思想是面向接口编程【面相接口与抽象编程】

依赖倒置：其实是让底层依赖上层，或者说面向接口编程，具体的实现，交给具体的实现类，只针对接口编程。

### 接口隔离：建立单一接口，不要建立庞大臃肿的接口，尽量细化接口，接口中的方法尽量少

使用多个隔离的接口，比使用单个接口要好。本意降低类之间的耦合度，而设计模式就是一个软件的设计思想，从大型软件架构出发，为了升级和维护方便。所以上文中多次出现：降低依赖，降低耦合

### 迪米特法则（最少知道原则）

### 开闭原则（Open Close Principle）  

对扩展开放，对修改关闭在程序需要进行拓展的时候，不能去修改原有的代码，实现一个热插拔的效果。所以一句话概括就是：为了使程序的扩展性好，易于维护和升级。想要达到这样的效果，需要面向接口编程。


## 参考文档

参考文档：https://www.cnblogs.com/pony1223/p/7608955.html
设计模式——设计模式三大分类以及六大原则  https://blog.csdn.net/SEU_Calvin/article/details/66994321