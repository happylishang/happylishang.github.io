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

模式：在某些场景下，针对某类问题的某种通用的解决方案。**单例、工厂、观察者、代理模式、装饰者使用的频率比较高**，其他不是特别高。

* 创建型模式：对象实例化的模式，创建型模式用于解耦对象的实例化过程。
* 结构型模式：把类或对象结合在一起形成一个更大的结构。
* 行为型模式：类和对象如何交互，及划分责任和算法。

### 单例模式：性能与安全的考量

某个类只能有一个实例，提供一个全局的访问点，就是单例模式。类加载是线程安全的，加载同时会初始化静态变量，执行静态代码块。单例的实现有多种：静态内部类、双重锁校验、懒汉、饿汉，其中饿汉是靠类加载机制保证、懒汉是靠Syncronize关键字保证，双重锁校验依靠Syncronize限定构造+volatile限制指令重排保证、静态内部类也是靠类加载机制来保证线程安全。
	
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
	

枚举方法可以保证安全，防止反射?
		
	class SingleTonEnumClass {
	    
	    //    不会上来就构建对象
	    
	    public static SingleTonEnumClass getInstance() {
	        return SingleTonEnum.INSTANCE.ob;
	    }
	
	    private SingleTonEnumClass() {
	    
	    }

	    //  枚举同静态内部类类似，可以实现加载线程安全
	    //  枚举类型是线程安全的，并且只会装载一次，只会装装载一次
	    
	    enum SingleTonEnum {
	        
	        INSTANCE;
	        <!--注意这里是属性值，不是静态的，是对象的，跟着唯一的枚举对象绑定-->
	        SingleTonEnumClass ob;
	
	        // jvm 保证enum 的SingleTonEnum的构造方法只执行一次
	        <!--但是能够保证SingleTonEnumClass一次吗？不能-->
	        SingleTonEnum() {
	        <!--构建的方式在构造方法里-->
	            ob = new SingleTonEnumClass();
	        }
	    }
	}


感觉上面的不对，应是枚举对象本身是单利，而不是枚举对象的一个属性，枚举的自己无法new，但是其他对象是可以的。所以直接用枚举 本身，由于是单利，其函数本身无所谓其他实例的对象，只要提供相应的方法即可，没有必要再转接一遍。单利，单利，一个进程中只能存在一个的对象。
	
	public enum Singleton {  
	    // jvm 保证enum 的SingleTonEnum的构造方法只执行一次
	    INSTANCE;  
	}  
	    


除枚举方式外, 其他方法都会通过反射的方式破坏单例,反射是通过调用构造方法生成新的对象，所以如果我们想要阻止单例破坏，可以在构造方法中进行判断，若已有实例, 则阻止生成新的实例。如果单例类实现了序列化接口Serializable, 就可以通过反序列化破坏单例，所以我们可以不实现序列化接口,如果非得实现序列化接口，可以重写反序列化方法readResolve(), 反序列化时直接返回相关单例对象


### 工厂设计模式

工厂设计模式是Java中最常用的设计模式之一。它是一种创建型设计模式，能够用于创建一个或多个类所需要的对象。有了这个工厂


* 简单工厂：主要负责根据传入的参数生产某个产品类，比较固定，而且对于扩展并不友好，每次都要根据type修改代码，case-when，不符合依赖倒置，所以一般不怎么用。

* 工厂方法模式，定义一个创建对象的接口，让子类工厂决定实例化什么对象，外界不关注内部怎么生成或者生成什么， 只要给了对应的工厂，返回需要商品即可。比如线程成ThreadPoolExecutor的一个参数，ThreadFactory threadFactory，就是采用工厂方法，工厂方法，一般只有一个create()方法，没有其他的了， 所以模式名特地指出了”方法“二字。
	
		 public ThreadPoolExecutor(int corePoolSize, int maximumPoolSize, long keepAliveTime, TimeUnit unit, BlockingQueue<Runnable> workQueue, ThreadFactory threadFactory, RejectedExecutionHandler handler) {
	   
		public interface ThreadFactory {
		    Thread newThread(Runnable var1);
		}

不关心什么样的ThreadFactory，只要能利用newThread返回Thread就可以，至于如何生成，如何定制，不需要关心。

* 抽象工厂模式：**核心是复杂**，创建相关或依赖对象的家族，对象们有了维度的划分，不再是同一维度，他与工厂方法模式的区别就在于：**抽象工厂方法里的create有好几个，是一个产品族** 或者你可以认为，工厂方法模式支持的对象，都已经全部内含在抽象工厂模式中，而具体的实现可以在此基础上，扩展其他维度。**可以认为工厂方法的流水线种类比较单一，但是抽象工厂的流水线比较多元，不过核心还是工厂方法的扩展**

> 参考文档：https://bbs.huaweicloud.com/blogs/338666

 	
### 建造者模式Builder：【私人灵活定制】自定义对话框、自定义类

构建与表示分离，建造者模式将复杂产品的构建过程封装分解在不同的方法中，使得创建过程非常清晰，能够让我们更加精确的控制复杂产品对象的创建过程，使得类似的构建过程可以创建不同的表示。封装一个复杂对象的构建过程，并可以按步骤构造，比如自己封装一些DialogBuilder，主要是自己定制一些属性，装饰，达到不同UI表现目的
 
###  原型模式：clone加改造
 
 
### 适配器模式：Adapter将一个类的方法接口转换成客户希望的另外一个接口。

接口的转换，感觉跟代理很像：使用现有的类，来处理不同的事情，比如你想新建一个有动态能力的 类，这个类有个接口是foward()，有两个对象有这个能力，鱼 用的是swim，蛇用的是crawl，这个时候，就可以根据自己需要的能力实现对应的适配器对象

	abstrace Adapter{
		abstrace void foward();
	}

	 setAdapter(new Adapter(){
	 		new fish.swim
	 })

Android 里ListView用适配器模式的目的就是让listview的每个item可以客户自己高度定制化，其实这里感觉挺像一个抽象工厂模式。适配器就是转换。

	抽象类来定义适配器，要实现的接口是Adapter


适配器一般是要自己来决定用哪种对象进行转换，非常像代理+抽象工厂。

### 桥接模式 

桥接（Bridge）是用于把抽象化与实现化解耦，使得二者可以独立变化。这种类型的设计模式属于结构型模式，它通过提供抽象化和实现化之间的桥接结构，来实现二者的解耦。一般构造函数中需要传入 xxxImpl，作为代理使用，桥接模式其实就是个代理么。
 
	
###  组合模式：将多种类的对象放一起

组合模式（Composite Pattern），又叫部分整体模式，是用于把一组相似的对象当作一个单一的对象。组合模式依据树形结构来组合对象，用来表示部分以及整体层次。这种类型的设计模式属于结构型模式，它创建了对象组的树形结构。java不支持多重继承，而且也麻烦，所有有了组合模式。

其实就将多种类的对象放到一起。

### 装饰模式：动态的给对象添加新的功能【加包装】

装饰器模式（Decorator Pattern）允许向一个现有的对象添加新的功能，同时又不改变其结构。这种类型的设计模式属于结构型模式，它是作为现有的类的一个包装。

简而言之，包装一下，复写被包装的方法，同时扩展。

### 代理模式：为其他对象提供一个代理以便控制这个对象的访问。

代理模式就是给一个对象提供一个代理，并由代理对象控制对原对象的引用。它使得客户不能直接与真正的目标对象通信。代理对象是目标对象的代表，其他需要与这个目标对象打交道的操作都是和这个代理对象在交涉
 
其实也蛮 组合、也蛮像桥接，只不过代理要求与被代理的对象功能基本一致，参考Android的Binder框架，那个远程代理更有代理的意义。

### 模板模式 ：行为型模式

有些时候我们做某几件事情的步骤都差不多，仅有那么一小点的不同，在软件开发的世界里同样如此，如果我们都将这些步骤都一一做的话，费时费力不讨好。所以我们可以将这些步骤分解、封装起来，然后利用继承的方式来继承即可，当然不同的可以自己重写实现嘛！这就是模板方法模式提供的解决方案。

* 第一步
* 第二步
* 第三步


### 策略模式：一个类的行为或其算法可以在运行时更改。这种类型的设计模式属于行为型模式

参考线程池的拒绝策略，其实就是面向接口编程的另一种应用：RejectedExecutionHandler

		 public ThreadPoolExecutor(int corePoolSize, int maximumPoolSize, long keepAliveTime, TimeUnit unit, BlockingQueue<Runnable> workQueue, ThreadFactory threadFactory, RejectedExecutionHandler handler) {
 
 
### 观察者模式：对象间的一对多的依赖关系 -行为型设计模式

它定义了一种一对多的依赖关系，当一个对象的状态发生改变时，其所有依赖者都会收到通知并自动更新。

	view.setOnClickListenerr
 
 或者各种自定义Listener
 
### 责任链模式：将请求的发送者和接收者解耦，使的多个对象都有处理这个请求的机会。 行为：传递
 
在这种模式中，通常每个接收者都包含对另一个接收者的引用。如果一个对象不能处理该请求，那么它会把相同的请求传给下一个接收者，依此类推。
 
 有个传递关系，一般而言，如果前面的处理了后面的可以不处理，当然也可以都处理，都处理就比较像一堆listener，或者说也可以做过滤，Error Info debug等输出不同。
 
###  迭代器模式：一种遍历访问聚合对象中各个元素的方法，不暴露该对象的内部结构。

迭代器模式（Iterator Pattern）是 Java 和 .Net 编程环境中非常常用的设计模式。这种模式用于顺序访问集合对象的元素，不需要知道集合对象的底层表示。

	public interface Iterator {
	   public boolean hasNext();
	   public Object next();
	}
		 
采用内部实现类：内部类可以直接用外部类的东西，但是不能是静态内部类，静态内部类是独立的，只是简单的标识一下归属性或者使用范围。**static nested classes.   inner classes.**
  
    public class NameRepository implements Container {
	   public String[] names = {"Robert" , "John" ,"Julie" , "Lora"};
	 
	   @Override
	   public Iterator getIterator() {
	      return new NameIterator();
	   }
	 
	   private class NameIterator implements Iterator {
	 
	      int index;
	      @Override
	      public boolean hasNext() {
	         if(index < names.length){
	            return true;
	         }
	         return false;
	      }
	 
	      @Override
	      public Object next() {
	         if(this.hasNext()){
	            return names[index++];
	         }
	         return null;
	      }     
	   }
	}

### 命令模式：将命令请求封装为一个对象，使得可以用不同的请求来进行参数化。

面向接口 、面向抽象编程，抽象Action指令，然后像需要的订阅者派发指令，根据指令类型处理。


## 参考文档

参考文档：https://www.cnblogs.com/pony1223/p/7608955.html

设计模式——设计模式三大分类以及六大原则  https://blog.csdn.net/SEU_Calvin/article/details/66994321

Android 23种设计模式 https://www.jianshu.com/p/fb558642823e