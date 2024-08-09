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
	    } ?: println("null")  == (println("null")) 需要执行  
	}

但是 block写法就不行 ，如下，不会执行	
	
	        p?.let {
            println(it)
            null
        } ?: { println("null") }  == ({ println("null") }) 直接null 
  


而靠谱的执行写法是

	        p?.let {
            println(it)
            null
        } ?: let { println("null") }  == ({ println("null") }) 直接let 执行{},同时也是计算返回值 
        

比如上述函数 ，如果p非null，会打印p，也会打印后面的null，因为let返回的值是null，后面的elvis条件成立，就会计算后面的值，所有的函数都有返回值，println("null")返回值是Unit，kotlin中都是对象，所以？后面的不执行，也会返回null，null判断会不断的传递


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
	
更简单的做法
	
	val inputAsString = input.readTextAndClose()  // defaults to UTF-8


### kotlin try catch

	runCatching获取可能的异常

	  return kotlin.runCatching {
            JSON.parseObject(jsonStr).getJSONObject("data")
        }.getOrNull()



### foreach写法 return之类的写法


## 协程与suspend函数  

协程的目的是什么？表面上说，协程的目的是为了调用suspend函数，但是其实翻过来，suspend函数为了协程，其实两者是一体的，但是真说目的：那就是**异步转同步**，否则没必要协程，直接起任务就好了

	suspend fun test(): Int {
	    delay(1000)
	    delay(1000)
	    delay(1000)
	    println("end ")
	    return 1
	}
	
	fun main() {
	
	    runBlocking {
	        println("start")
	        test()
	        println("end")
	    }
	}


转换后

	public final class SusTestKt {
	
	<!--test函数被转换  var0作为后续调用的入口  -->
	   @Nullable
	   public static final Object test(@NotNull Continuation var0) {
	      Object $continuation;
	      label37: {
	         if (var0 instanceof <undefinedtype>) {
	            $continuation = (<undefinedtype>)var0;
	            if ((((<undefinedtype>)$continuation).label & Integer.MIN_VALUE) != 0) {
	               ((<undefinedtype>)$continuation).label -= Integer.MIN_VALUE;
	               break label37;
	            }
	         }
			<!--var0 作为ContinuationImpl 的构建参数传入， ContinuationImpl  这个是作为其上封传来的，后面ContinuationImpl 协程 执行完毕后，或者说不在挂起后  继续执行 Continuation var0，但是逻辑并不是在这里而是在 ContinuationImpl的resume中 -->
			
			<!--每个挂起函数，如果在在调用挂起函数之后还有处理，那么自己内部就会构建ContinuationImpl 作为 自身函数的 Continuation  每个挂起函数的的ContinuationImpl 必定是回调自身，因为它是感知不到其他函数的   -->
	         $continuation = new ContinuationImpl(var0) {
	            // $FF: synthetic field
	            Object result;
	            int label;
	
			<!--invokeSuspend 会被调用 同时 -invokeSuspend会有返回值 ->  
	            @Nullable
	            public final Object invokeSuspend(@NotNull Object $result) {
	               this.result = $result;
	               this.label |= Integer.MIN_VALUE;
	               return SusTestKt.test(this);
	            }
	         };
	      }
	
<!--	ContinuationImpl 被封装了，之后层层处理为 Task，或者runable，将来 唤起后 通过  continuation. invokeSuspend  可以获取返回值，或者说，如果再挂起，则继续处理挂起 ，等 continuation再次被调用 如此反复  返回是就是通过  ContinuationImpl的 invokeSuspend获取，因为 invokeSuspend  会继续调用原来的函数，只要原函数有返回值，那么一定会获取到，如果后续需要结果，那么一定有ContinuationImpl   complete 负责让后面传递 ，-->
	
	
	      label31: {
	         Object var4;
	         label30: {
	         <!--每一轮开始  $result 都会先被传递 同时   $continuation).label 会被更新 -->
	            Object $result = ((<undefinedtype>)$continuation).result;
	            var4 = IntrinsicsKt.getCOROUTINE_SUSPENDED();
	            switch (((<undefinedtype>)$continuation).label) {
	            <!-- case 0:直接调用，还用不到 continuation   -->
	               case 0:
	                  ResultKt.throwOnFailure($result);
	                  ((<undefinedtype>)$continuation).label = 1;
	                  if (DelayKt.delay(1000L, (Continuation)$continuation) == var4) {
	                     return var4;
	                  }
	                  break;
	               case 1:
	                  ResultKt.throwOnFailure($result);
	                  break;
	               case 2:
	                  ResultKt.throwOnFailure($result);
	                 <!-- 通过break 标签 调到对应的state与函数调用 -->
	                  break label30;
	               case 3:
	                  ResultKt.throwOnFailure($result);
	               <!-- 通过break 标签 调到对应的state与函数调用 -->
	                  break label31;
	               default:
	                  throw new IllegalStateException("call to 'resume' before 'invoke' with coroutine");
	            }
			<!--调用协程函数 可能挂起 ，这里的挂起其实就是  直接返回了，以内转Java后没什么挂起了，无视是要不要处理回调  比如Delay返回了 挂起，那么就要封装为delay runable，后面会被调用 ContinuationImpl ，而后 ContinuationImpl有complete回调，根据ContinuationImpl 的返回值 发给后续  -->
	            ((<undefinedtype>)$continuation).label = 2;
	            if (DelayKt.delay(1000L, (Continuation)$continuation) == var4) {
	               return var4;
	            }
	         }
		<!--调用协程函数 -->
	         ((<undefinedtype>)$continuation).label = 3;
	         if (DelayKt.delay(1000L, (Continuation)$continuation) == var4) {
	            return var4;
	         }
	      }
		<!--最终一次又返回值，返回值会交给complete使用，否则resume之后，接着挂起 -->
	      String var1 = "end ";
	      System.out.println(var1);
	      return Boxing.boxInt(1);
	   }
	
	   public static final void main() {
	   
	   	<!--(Function2)(new Function2((Continuation)null 这里其实可以单独玻璃一个类，构建一个对象  null 是因为这里没有来源，不需处理Continuation，是入口的意思 -->
	      BuildersKt.runBlocking$default((CoroutineContext)null, (Function2)(new Function2((Continuation)null) {
	         int label;
	
	         @Nullable
	         public final Object invokeSuspend(@NotNull Object $result) {
	            Object var3 = IntrinsicsKt.getCOROUTINE_SUSPENDED();
	            Object var10000;
	            switch (this.label) {
	               case 0:
	                  ResultKt.throwOnFailure($result);
	                  this.label = 1;
	                  <!--第一个挂起函数SusTestKt.test  的 Continuation  是  this ，也即是说，挂起后，回调 this -->
	                  var10000 = SusTestKt.test(this);
	                  if (var10000 == var3) {
	                     return var3;
	                  }
	                  break;
	               case 1:
	                  ResultKt.throwOnFailure($result);
	                  var10000 = $result;
	                  break;
	               default:
	                  throw new IllegalStateException("call to 'resume' before 'invoke' with coroutine");
	            }
	
	            int v = ((Number)var10000).intValue();
	            System.out.println(v);
	            return Unit.INSTANCE;
	         }
	
	         @NotNull
	         public final Continuation create(@Nullable Object value, @NotNull Continuation completion) {
	            Intrinsics.checkNotNullParameter(completion, "completion");
	            Function2 var3 = new <anonymous constructor>(completion);
	            return var3;
	         }
	
	         public final Object invoke(Object var1, Object var2) {
	            return ((<undefinedtype>)this.create(var1, (Continuation)var2)).invokeSuspend(Unit.INSTANCE);
	         }
	      }), 1, (Object)null);
	   }
	
 
	   public static void main(String[] var0) {
	      main();
	   }
	}
	
#### 协程核心：载体是ContinuationImpl 也就是回调体 

协程函数会被转换成普通的Java函数，只不过多了一个Continuation<? super Unit> continuation参数，一般而言，协程函数里面是要有suspend调用的，否则没必要，有协程调用的时候，continuation参数才有用而真正**调用suspend的地方会被封装成另一种回调形式**，从上面的就可以看出，协程会被转换成 SusTestKt$main$1  suspendlamda 函数体， 或kotlin编译工具会辅助生成这些类，如果直接通过反编译，看到的会是另一种匿名对象的方式，但是基本流程类似。

runBlocking是一个固定范式：

	public final /* synthetic */ class BuildersKt__BuildersKt {
	    public static /* synthetic */ Object runBlocking$default(CoroutineContext coroutineContext, Function2 function2, int i, Object obj) throws InterruptedException {
	        if ((i & 1) != 0) {
	            coroutineContext = EmptyCoroutineContext.INSTANCE;
	        }
	        return BuildersKt.runBlocking(coroutineContext, function2);
	    }
	
	<!--后面调用的是  EmptyCoroutineContext.INSTANCE  function2 -->
	
function2就是协程体换成 的SusTestKt$main$1 对象，该对象被runBlocking调用  runBlocking有自己的阻塞执行逻辑。而且里面也会启动新的协程之类的。

	
	    public static final <T> T runBlocking(CoroutineContext context, Function2<? super CoroutineScope, ? super Continuation<? super T>, ? extends Object> function2) throws InterruptedException {
	   	 <!--CoroutineContext-->
	        CoroutineContext newContext;
	        <!--LOOP -->
	        EventLoop eventLoop;
	        <!--当前线程-->
	        Thread currentThread = Thread.currentThread();
	        <!--EmptyCoroutineContext.INSTANCE get返回的是null-->
	        ContinuationInterceptor contextInterceptor = (ContinuationInterceptor) context.get(ContinuationInterceptor.Key);
	        <!---->
	        if (contextInterceptor == null) {
	        <!--开始时null 创建爱哪一个LOOP-->
	            eventLoop = ThreadLocalEventLoop.INSTANCE.getEventLoop$kotlinx_coroutines_core();
	            <!--创建 依托LOOP的 CoroutineContex  有点类似于handler机制  -->
	            newContext = CoroutineContextKt.newCoroutineContext(GlobalScope.INSTANCE, context.plus(eventLoop));
	        } else {
	            EventLoop eventLoop2 = null;
	            EventLoop it = contextInterceptor instanceof EventLoop ? (EventLoop) contextInterceptor : null;
	            if (it != null && it.shouldBeProcessedFromContext()) {
	                eventLoop2 = it;
	            }
	            if (eventLoop2 == null) {
	                eventLoop2 = ThreadLocalEventLoop.INSTANCE.currentOrNull$kotlinx_coroutines_core();
	            }
	            eventLoop = eventLoop2;
	            newContext = CoroutineContextKt.newCoroutineContext(GlobalScope.INSTANCE, context);
	        }
	        <!--BlockingCoroutine  -->
	        BlockingCoroutine coroutine = new BlockingCoroutine(newContext, currentThread, eventLoop);
	        coroutine.start(CoroutineStart.DEFAULT, coroutine, function2);
	        return (T) coroutine.joinBlocking();
	    }
	}
	
EventLoopKt构建 createEventLoop  EventLoopKt本事也是个 CoroutineDispatcher
	
	
	public final class EventLoopKt {
	    public static final EventLoop createEventLoop() {
	        return new BlockingEventLoop(Thread.currentThread());
	    }
	
	    public static final long processNextEventInCurrentThread() {
	        EventLoop currentOrNull$kotlinx_coroutines_core = ThreadLocalEventLoop.INSTANCE.currentOrNull$kotlinx_coroutines_core();
	        if (currentOrNull$kotlinx_coroutines_core == null) {
	            return Long.MAX_VALUE;
	        }
	        return currentOrNull$kotlinx_coroutines_core.processNextEvent();
	    }
	
	    public static final void platformAutoreleasePool(Function0<Unit> function0) {
	        function0.invoke();
	    }
	}
	
协程执行的最后会构建一个BlockingCoroutine，BlockingCoroutine是一个协程，具体里面还执行什么，然后交给coroutine.start来定，BlockingCoroutine其实本身封装了协程的context，当前线程 ，以及Loop对象，

    public BlockingCoroutine(CoroutineContext parentContext, Thread blockedThread, EventLoop eventLoop) {
        super(parentContext, true, true);
        this.blockedThread = blockedThread;
        this.eventLoop = eventLoop;
    }
    
 SusTestKt.test(this)会调用DelayKt.delay(1000, continuation) ，而DelayKt.delay，肯定会返回coroutine_suspended，并且根据不同的Context，选择不同的处理方式，如果是lifeCycleScope的context，则会睡眠唤醒 ，continuation其实就是后续的流程调用，协程的赋值，只在协程内部同步，转换后都是回调，后面传递了协程函数体进去，将来会继续执行，由于state已经发生了转变，会继续执行后续，而结果会通过invokeSuspend(Object $result) 传递进去。  根据是否有后续操作，其实continuation用法挺有区别的，如果，没返后续，则直接用上游传递的，自身有后续，则需要封装，先先执行自己的，然后执行后续的。
  
 **每个唤醒的 continuation 执行的都是调用自身所创建的函数 ，创建的，而不是所处的。**，并且携带返回值传递进去。BaseContinuationImpl实现的时候是可以传递一个 continue进去的， 类似于串联，自己是别人的后续，同样，自己也是别人的前驱，
 
	 internal abstract class BaseContinuationImpl(
	    // This is `public val` so that it is private on JVM and cannot be modified by untrusted code, yet
	    // it has a public getter (since even untrusted code is allowed to inspect its call stack).
	    public val completion: Continuation<Any?>?
	) : Continuation<Any?>, CoroutineStackFrame, Serializable {

 completion: Continuation，这个参数就是BaseContinuationImpl被调用完后，主动调用的一个，而且是结果传递的地方

	   public final override fun resumeWith(result: Result<Any?>) {
	        // This loop unrolls recursion in current.resumeWith(param) to make saner and shorter stack traces on resume
	        var current = this
	        var param = result
	        while (true) {
	            // Invoke "resume" debug probe on every resumed continuation, so that a debugging library infrastructure
	            // can precisely track what part of suspended callstack was already resumed
	            probeCoroutineResumed(current)
	            with(current) {
	                val completion = completion!! // fail fast when trying to resume continuation without completion
	                val outcome: Result<Any?> =
	                    try {
	                    <!--这里获取结果 -->
	                        val outcome = invokeSuspend(param) //会调用之前的挂起函数，获取返回值，如果是挂起就挂起了 ，不挂起，后面执行 回调唤起，其实赋值什么的转换成回调 
	                        if (outcome === COROUTINE_SUSPENDED) return
	                        Result.success(outcome)
	                    } catch (exception: Throwable) {
	                        Result.failure(exception)
	                    }
	                releaseIntercepted() // this state machine instance is terminating
	                if (completion is BaseContinuationImpl) {
	                    // unrolling recursion via loop
	                    current = completion
	                    param = outcome
	                } else {
	                    // top-level completion reached -- invoke and return
	                    <!--这里传递结果 -->
	                    completion.resumeWith(outcome)
	                    return
	                } }  }   }
	

##  挂起函数最后的continue 参数是负责承接与回调的  承接的是当前挂起函数

每个挂起函数的continue都是为了承接 continue本身，而函数内部的 continueimpli对象是为了执行函数，不是作为参数，不是作为结果承接  ，continueimpli自身作为承接，但是气内含complete参数作为启下的作用。



	    
### 从Delay的看处理模型  

一开始continuation会被封装成消息插入Loop，在Loop中处理Delay的调用，delay调用了suspendCancellableCoroutine，
	 
		 public suspend fun delay(timeMillis: Long) {
	    if (timeMillis <= 0) return // don't delay
	    return suspendCancellableCoroutine sc@ { cont: CancellableContinuation<Unit> ->
	        // if timeMillis == Long.MAX_VALUE then just wait forever like awaitCancellation, don't schedule.
	        if (timeMillis < Long.MAX_VALUE) {
	            cont.context.delay.scheduleResumeAfterDelay(timeMillis, cont)
	        }
	    }
	}

suspendCancellableCoroutine是个典型的协程范式：
  
	  public suspend inline fun <T> suspendCancellableCoroutine(
	    crossinline block: (CancellableContinuation<T>) -> Unit
	): T =
	    suspendCoroutineUninterceptedOrReturn { uCont ->
	        val cancellable = CancellableContinuationImpl(uCont.intercepted(), resumeMode = MODE_CANCELLABLE)
	        /*
	         * For non-atomic cancellation we setup parent-child relationship immediately
	         * in case when `block` blocks the current thread (e.g. Rx2 with trampoline scheduler), but
	         * properly supports cancellation.
	         */
	        cancellable.initCancellability()
	        block(cancellable)
	        cancellable.getResult()
	    }


转换Java。suspendCoroutineUninterceptedOrReturn之后被处理成什么？ 反正就是执行里面的task，然后挂起，等待其他来帮助唤醒 
  
		      public static final Object delay(long timeMillis, Continuation<? super Unit> continuation) {
		        if (timeMillis <= 0) {
		            return Unit.INSTANCE;
		        }
		        CancellableContinuationImpl cancellable$iv = new CancellableContinuationImpl(IntrinsicsKt.intercepted(continuation), 1);
		        cancellable$iv.initCancellability();
		        CancellableContinuationImpl cont = cancellable$iv;
		        if (timeMillis < Long.MAX_VALUE) {
		            getDelay(cont.getContext()).scheduleResumeAfterDelay(timeMillis, cont);
		        }
		        Object result = cancellable$iv.getResult();
		        if (result == IntrinsicsKt.getCOROUTINE_SUSPENDED()) {
		            DebugProbesKt.probeCoroutineSuspended(continuation);
		        }
		        return result == IntrinsicsKt.getCOROUTINE_SUSPENDED() ? result : Unit.INSTANCE;
		    }

 suspendCoroutineUninterceptedOrReturn本身没实现，是靠编译工具
 
	 
	 
	/**
	 * Obtains the current continuation instance inside suspend functions and either suspends
	 * currently running coroutine or returns result immediately without suspension.
	 *
	 * If the [block] returns the special [COROUTINE_SUSPENDED] value, it means that suspend function did suspend the execution and will
	 * not return any result immediately. In this case, the [Continuation] provided to the [block] shall be
	 * resumed by invoking [Continuation.resumeWith] at some moment in the
	 * future when the result becomes available to resume the computation.
	 *
	 * Otherwise, the return value of the [block] must have a type assignable to [T] and represents the result of this suspend function.
	 * It means that the execution was not suspended and the [Continuation] provided to the [block] shall not be invoked.
	 * As the result type of the [block] is declared as `Any?` and cannot be correctly type-checked,
	 * its proper return type remains on the conscience of the suspend function's author.
	 *
	 * Invocation of [Continuation.resumeWith] resumes coroutine directly in the invoker's thread without going through the
	 * [ContinuationInterceptor] that might be present in the coroutine's [CoroutineContext].
	 * It is the invoker's responsibility to ensure that a proper invocation context is established.
	 * [Continuation.intercepted] can be used to acquire the intercepted continuation.
	 *
	 * Note that it is not recommended to call either [Continuation.resume] nor [Continuation.resumeWithException] functions synchronously
	 * in the same stackframe where suspension function is run. Use [suspendCoroutine] as a safer way to obtain current
	 * continuation instance.
	 */
	@SinceKotlin("1.3")
	@InlineOnly
	@Suppress("UNUSED_PARAMETER", "RedundantSuspendModifier")
	public suspend inline fun <T> suspendCoroutineUninterceptedOrReturn(crossinline block: (Continuation<T>) -> Any?): T {
	    contract { callsInPlace(block, InvocationKind.EXACTLY_ONCE) }
	    throw NotImplementedError("Implementation of suspendCoroutineUninterceptedOrReturn is intrinsic")
}