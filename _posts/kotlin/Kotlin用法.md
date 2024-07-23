# 最关键：不会先Google，一定会提供简洁的写法


### kotlin中的同步互斥

kotlin不提供wait、notify、notifyAll，但是可以通过RetreentLock来处理，

<!--也必须在lock的里面-->
		class KotlinBlockQueue<T> constructor(private val capacity: Int) {
	
		    private val lock = ReentrantLock()
		    private val notEmpty = lock.newCondition()
		    private val notFull = lock.newCondition()
		    private val items = mutableListOf<T>()
		
		    fun put(item: T) {
		        lock.withLock {
		            while (items.size == capacity) {
		                notFull.await()
		            }
		            items.add(item)
		            notEmpty.signal()
		        }
		    }
		
		    fun take(): T {
		        lock.withLock {
		            while (items.isEmpty()) {
		                notEmpty.await()
		            }
		            val item = items.removeAt(0)
		            notFull.signal()
		            return item
		        }
		    }
		}

如此可以实现生产者跟消费者模式，ReentrantLock的Condition是个不错的选择。如果只是处理互斥，当然也可以使用 synchronized 关键字，不过kotlin自己也提供了Mutex对象，不过kotlin自身没有现成的概念，转而是协程，Mutex主要是用在协程，多协程同步.

## kotlin下的单利synchronized用法

* 双重锁校验：懒汉模式

	    companion object {
	        @Volatile
	        private var instance: KotlinMutex? = null
	
	        fun getInstance(): KotlinMutex {
	            return instance ?: synchronized(this) {
	                instance ?: KotlinMutex().also { instance = it }
	            }
	        }
	    }
	    
或者	    

	       companion object {
	        val instance: KotlinMutex by lazy(mode = LazyThreadSafetyMode.SYNCHRONIZED) {
	            KotlinMutex()
	        }
	    } 
   
* 饿汉模式

		object KotlinMutex

kotlin没直接synchronized与volatile关键字，取而代之的是Synchronized与Volatile注解，Java的一些并发类都可以用，kotlin最主要的还是用协程。

####   by lazy 修饰的val类型的数据

by 是kotlin的一个关键字，实现了一种委托模式，by 就是这个变量的get，set都是委托给了另外一个类来去操作

	class DelegateExample {
	    var name:String by Delegate()
	}
	
	class Delegate{
	
	    private var _name:String = "default value provide by Delegate"
	
	    operator fun getValue(example: DelegateExample, property: KProperty<*>): String {
	        println("Delegate : get Value")
	        return _name
	    }
	
	    operator fun setValue(example: DelegateExample, property: KProperty<*>, s: String) {
	        println("Delegate : set Value: $s")
	        _name = s
	    }
	}
  
	  fun main() {
	    val example = DelegateExample();
	    println(example.name)
	    example.name = "tom"
	    println(example.name)
	}

在使用name的时候，其实会通过 Delegate的getValue来处理，设置值也会通过其setValue来设置。上面的输出 

	Delegate : get Value
	default value provide by Delegate
	Delegate : set Value: tom
	Delegate : get Value
	tom


而lazy 其实是kotlin定义的一个函数，SynchronizedLazyImpl(initializer)  by lazy就是依靠 SynchronizedLazyImpl来设置、获取值
  
	public actual fun <T> lazy(initializer: () -> T): Lazy<T> = SynchronizedLazyImpl(initializer)

	private class SynchronizedLazyImpl<out T>(initializer: () -> T, lock: Any? = null) : Lazy<T>, Serializable {
	    private var initializer: (() -> T)? = initializer
	    @Volatile private var _value: Any? = UNINITIALIZED_VALUE
	    // final field is required to enable safe publication of constructed instance
	    private val lock = lock ?: this
	
	    override val value: T
	        get() {
	            val _v1 = _value
	            if (_v1 !== UNINITIALIZED_VALUE) {
	                @Suppress("UNCHECKED_CAST")
	                return _v1 as T
	            }
	
	            return synchronized(lock) {
	                val _v2 = _value
	                if (_v2 !== UNINITIALIZED_VALUE) {
	                    @Suppress("UNCHECKED_CAST") (_v2 as T)
	                } else {
	                    val typedValue = initializer!!()
	                    _value = typedValue
	                    initializer = null
	                    typedValue
	                }
	            }
	        }
	
	    override fun isInitialized(): Boolean = _value !== UNINITIALIZED_VALUE
	
	    override fun toString(): String = if (isInitialized()) value.toString() else "Lazy value not initialized yet."
	
	    private fun writeReplace(): Any = InitializedLazyImpl(value)
	}
	
SynchronizedLazyImpl保证value只会被设置一次，并且synchronized保证了同步。他的getValue由接口实现 Lazy

	public interface Lazy<out T> {
	    /**
	     * Gets the lazily initialized value of the current Lazy instance.
	     * Once the value was initialized it must not change during the rest of lifetime of this Lazy instance.
	     */
	    public val value: T
	
	    /**
	     * Returns `true` if a value for this Lazy instance has been already initialized, and `false` otherwise.
	     * Once this function has returned `true` it stays `true` for the rest of lifetime of this Lazy instance.
	     */
	    public fun isInitialized(): Boolean
	}
	
	@kotlin.internal.InlineOnly
	public inline operator fun <T> Lazy<T>.getValue(thisRef: Any?, property: KProperty<*>): T = value

getValue会直接返回value的值。

## by lazy与    lateinit  

lateinit修饰的是var，可变的变量，by lazy是val 

###  kotlin 扩展函数 与接口实现函数

扩展函数最终的实现，是静态函数，而接口函数是要被实现为类的成员函数，两者不同，扩展函数无法替代内部函数，扩展函数会被直接覆盖。

> Extension is shadowed by a member: public open fun 

	interface InterfaceTest {
	    fun <T> process(param: T)
	
	    var value: String
	
	}
	
	//接口的实现不需要（），因为没有构造函数
	class InterfaceTestClass : InterfaceTest {
	    override fun <T> process(param: T) {
	        println("process 内部")    }
	
	    override var value: String = ""
	        get() = " hello"
	        set(value) {
	            field = value
	        }
	}
	fun <T> InterfaceTestClass.process(param: T){
	    println("process 扩展")
	}
	fun main() {
	    val interfaceTestClass = InterfaceTestClass()
	    interfaceTestClass.process("hello")
	    interfaceTestClass.value = "world"
	    println(interfaceTestClass.value)
	}

 

在类的构造方法里，用 var ,val 等修饰的都是属性，否则就是参数

## kotlin成员变量,初始化顺序  **都是构造函数的一部分**

参考文档 [参考文档](https://blog.csdn.net/devnn/article/details/121991390)


在类的构造方法里，用 var ,val 等修饰的都是属性，否则就是参数

	class ClassFeildTest(val name: String = "default value", var age: Int = 0,param: String = "param") {

执行顺序：

	class InitOrderDemo(name: String) {
	    val firstProperty = "First property: $name".also(::println)
	    
	    init {
	        println("First initializer block that prints ${name}")
	    }
	    
	    val secondProperty = "Second property: ${name.length}".also(::println)
	    
	    init {
	        println("Second initializer block that prints ${name.length}")
	    }
	}
 
转java代码，kotlin只是个编译框架

	public final class InitOrderDemo {
	   @NotNull
	   private final String firstProperty;
	   @NotNull
	   private final String secondProperty;
	
	   @NotNull
	   public final String getFirstProperty() {
	      return this.firstProperty;
	   }
	
	   @NotNull
	   public final String getSecondProperty() {
	      return this.secondProperty;
	   }
	
	   public InitOrderDemo(@NotNull String name) {
	      Intrinsics.checkNotNullParameter(name, "name");
	      super();
	      String var2 = "First property: " + name;
	      boolean var3 = false;
	      boolean var4 = false;
	      int var6 = false;
	      boolean var7 = false;
	      System.out.println(var2);
	      Unit var9 = Unit.INSTANCE;
	      this.firstProperty = var2;
	      var2 = "First initializer block that prints " + name;
	      var3 = false;
	      System.out.println(var2);
	      var2 = "Second property: " + name.length();
	      var3 = false;
	      var4 = false;
	      var6 = false;
	      var7 = false;
	      System.out.println(var2);
	      var9 = Unit.INSTANCE;
	      this.secondProperty = var2;
	      var2 = "Second initializer block that prints " + name.length();
	      var3 = false;
	      System.out.println(var2);
	   }
	}
	
Kotlin的成员变量初始化是放在构造函数当中的，init代码块也是"拷贝"到了构造函数当中，并且是按照声明顺序"拷贝"的，所以它们**都是构造函数的一部分**。

* 先执行主构造函数(初始化成员变量和执行init代码块)，
* 再执行次级构造函数代码。kotlin规定次级构造函数先要委托给主构造函数。
* 成员变量初始化和init代码块谁先执行是按它们的声明顺序来的。
* 如果有默认构造函数，那么也是要调用主构造函数之后，才能调用次构造函数后面的值，
* 属性、init，都只能用主构造函数的传入的值，因为他们只会在主构造函数有意义。代码不会出现在次构造函数中
* 主构造函数中是否是val var要看是只用作参数，或者说是否只用初始化
* 都是Java

###  Kotlin的成员变量必须在构造函数中初始化 ：必须保证在**所有构造函数中**有赋值操作，注意是**所有**，主构造函数是一种保证方式

	class NoMainClass {
	
	    var name: String
	
	    constructor(name: String) {
	        this.name = name
	    }	
	    constructor(name: String, age: Int): this(name) {
	    }
	}

主构造构造函数是根基，编译工具会计算出最长构造函数，然后其他函数会调用最长的，除了主构造函数，其他的函数都是函数，参数不可以加val var，他们都是参数。**无论是显性的，还是隐性的，必须有赋值给他的操作**。成员变量在编译成Java代码后，都是在构造函数中。存在主构造函数的话，主构造函数中的属性会直接被赋值的，无需考虑，相当于写了一个main构造函数，其他的都要调用它。如果初始化用了by lazy其实是会有不同表现的

	class ConstructorTest(val age: Int, var name: String = "default", param: String = "param2") {
	
	
	    //    初始化的时候，不会赋值，而是延迟，但是参数默认是final的，所有前后调用值其实没啥区别
	     val p: String by lazy {
	        param
	    }
	
	    //    初始化的时候，不会赋值，而是延迟，第一次调用的时候是什么之就给什么值，是可变的
	    val p2: String by lazy {
	        name
	    }
    
转换后的java代码，by委托模式，lazy，真正用的时候调用get
	
	 public ConstructorTest(int age, @NotNull String name, @NotNull final String param) {
	      Intrinsics.checkNotNullParameter(name, "name");
	      Intrinsics.checkNotNullParameter(param, "param");
	      super();
	      this.age = age;
	      this.name = name;
	      this.p$delegate = LazyKt.lazy((Function0)(new Function0() {
	         // $FF: synthetic method
	         // $FF: bridge method
	         public Object invoke() {
	            return this.invoke();
	         }
	
	         @NotNull
	         public final String invoke() {
	            return param;
	         }
	      }));
	      this.p2$delegate = LazyKt.lazy((Function0)(new Function0() {
	         // $FF: synthetic method
	         // $FF: bridge method
	         public Object invoke() {
	            return this.invoke();
	         }
	
	         @NotNull
	         public final String invoke() {
	            return ConstructorTest.this.getName();
	         }
	      }));
	   }
	   
### also let apply with run

> **run对应let   also对应apply**

* 	with//返回任意想要的 默认是scope是this

	    val withP = with(pa) {
	        println(this)
	        name = "tom"
	        "ddd"
	    }

 with本身是独立函数不是扩展函数
 
	 @kotlin.internal.InlineOnly
	public inline fun <T, R> with(receiver: T, block: T.() -> R): R {
	    contract {
	        callsInPlace(block, InvocationKind.EXACTLY_ONCE)
	    }
	    return receiver.block()
	}
	
第一个参数是T类型对象自身，第二个参数是T的扩展函数，最后是调用T的扩展函数，并返回要求的返回值
	
* 	    run //返回任意想要的 默认是scope是this

		    val ret = pa.run {
		        println(this)
		//        返回值随意
		        "afd"
		    }
		    
run是作为扩展函数来实现的， 调用**T的另一个扩展函数**，所以block内部才能拿到this，没有参数传入，也就没有it，返回值随意  不过该扩展函数没有入参

	@kotlin.internal.InlineOnly
	public inline fun <T, R> T.run(block: T.() -> R): R {
	    contract {
	        callsInPlace(block, InvocationKind.EXACTLY_ONCE)
	    }
	    return block()
	}

	    
* 	   let //返回任意想要的 默认是scope是it，主要是非空
 
	    val retLet = pa.let {
	        println(it)
	        it.name = "tom"
	        it
	    }
	    
let虽然也是扩展函数，但是参数block不是T的扩展函数，所以是拿不到this指针的，只能用参数，主动传递了this参数，由于是唯一的参数，可以用it代替

		@kotlin.internal.InlineOnly
		public inline fun <T, R> T.let(block: (T) -> R): R {
		    contract {
		        callsInPlace(block, InvocationKind.EXACTLY_ONCE)
		    }
		    return block(this)
		}

* also	// 默认返回自己，作用域是it

	    val ret2 = pa.also {
	        println(it)
	        it.name = "tom"
	    }
	    
* apply this //返回自己作用域是this

	    val ret3 = pa.apply {
	        name = "tom"
	    }
 翻译成Java
 
	    public static final void main() {
	      Parent pa = new Parent();
	      int var3 = false;
	      System.out.println(pa);
	      pa.setName("tom");
	      String withP = "ddd";
	      int var5 = false;
	      System.out.println(pa);
	      String ret = "afd";
	      int var6 = false;
	      System.out.println(pa);
	      pa.setName("tom");
	      int var7 = false;
	      System.out.println(pa);
	      pa.setName("tom");
	      int var8 = false;
	      pa.setName("tom");
	   }

为什么有些函数用it，因为有些函数有唯一参数，唯一参数其实就是it，如果 匿名函数 只有 1 个函数参数 , 在 匿名函数 的 函数体 中 , 可以 省略 函数名 声明 , 使用 it 关键字 代替 ;


![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ce4537beb8d045cfbf10f368697f1525~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=1486&h=1080&s=214553&e=png&b=1e1f22)


### 扩展函数 扩展函数的本质 

外部定义的扩展函数，会转换成静态函数，同时传递一自身this进去变量进去

	   public static final void printTKI2(@NotNull KuoFunTest $this$printTKI2) {
	      Intrinsics.checkNotNullParameter($this$printTKI2, "$this$printTKI2");
	      String var1 = "printT";
	      System.out.println(var1);
	      var1 = $this$printTKI2.getName();
	      System.out.println(var1);
	   }

内部自然也可以定义扩展函数，但是哪个傻子这么做呢，内部就是内部函数而已，当然如果要限制某些函数在某个类内部使用，可以在类内部定义，这样其他类就不能用了。

	class SquareScope {
	    fun Int.square(): Int = this * this
		
		@Test
	    fun test() {
	        println(22.square())
	    }
	}
	
那么square只能在SquareScope内部用
	
### 扩展属性:本质上就是扩展函数，只是省的定义一些东西 

扩展属性不能存储，只能操作,
	
	val KuoFunTest.myName
	    get() = name


	   @NotNull
	   public static final String getMyName(@NotNull KuoFunTest $this$myName) {
	      Intrinsics.checkNotNullParameter($this$myName, "$this$myName");
	      return $this$myName.getName();
	   }
	   
类似于在类定义之外，扩展一个getMyName函数。get需要给返回值

扩展函数的本质是定义静态函数，在类之外定义的，传递了对象的引用进去，但是打破内部访问的逻辑，所以private protect的成员变量跟方法都无法访问。

### 匿名函数与函数  Lambda表达式的本质是匿名函数

Lambda 表达式，也可称为闭包，Lambda 表达式（lambda expression）是一个匿名函数，可以作为参数，可以赋值给变量，之后被invoke，表达式本质上是 「可以作为值传递的代码块」，在老版本 Java 中，传递代码块需要使用匿名内部类实现，而使用 lambda 表达式甚至连函数声明都不需要，可以直接传递代码块作为函数值。当 lambda 表达式只有一个参数，可以用it关键字来引用唯一的实参。

* lambda 表达式与匿名函数是“函数字面值”，即未声明的函数， 但立即做为表达式传递。 未声明的函数，
* lambda表达式是函数的实现，不是函数的声明
* 双冒号：：相当于定义一个函数变量

两个函数变量，如下，等效

	fun main() {
	    val p: (Int, Int) -> Int = { x, y -> x + y }
	    val q = { x: Int, y: Int -> x + y }
	    val f = fun(x: Int, y: Int): Int {
	        return x + y
	    }
	    
	    println(p(1, 2))
	    println(q(1, 2))
	    println(f(1, 2))
	}

具名函数不能作为参数传递，但是匿名函数可以， 本质是一个匿名函数对象，具名函数本身不行 

## ？let  之类的写法

* 变量后加问号?  当变量的值是null ，那么Kotlin只是返回null ，而不执行属性或方法调用,否则调用，并用后面的返回值
* elvis操作符(?:)   ，如果前面的是null，用后面的返回值

?:与 ？是独立的，elvis表示一个 如果前面不是null 用前面的值，如果前面是null用后面的值 ,不可以简单的理解为三元操作

	fun test(p: String?) {
		
	    p?.let {
	        println(it)
	        null
	    } ?: println("null")
	}

比如上述函数 ，如果p非null，会打印p，也会打印后面的null，因为let返回的值是null，后面的elvis条件成立，就会计算后面的值，所有的函数都有返回值，println("null")返回值是Unit，kotlin中都是对象，所以？后面的不执行，也会返回null，null判断会不断的传递


### foreach写法 return之类的写法



### when

替换swith：表达式是一个返回值的条件表达式

	fun main(args: Array<String>){  
	    var number = 4  
	    var numberProvided = when(number) {  
	        1 -> "One"  
	        2 -> "Two"  
	        3 -> "Three"  
	        4 -> "Four"  
	        5 -> "Five"  
	        else -> "invalid number"  
	    }  
	    println("You provide $numberProvided")  
	}
 
###  inputSteam 读取 String

	val inputAsString = input.bufferedReader().use { it.readText() } 
	
	val inputAsString = input.readTextAndClose()  // defaults to UTF-8


### kotlin try catch

	runCatching获取可能的异常

	  return kotlin.runCatching {
            JSON.parseObject(jsonStr).getJSONObject("data")
        }.getOrNull()

