Hilt依赖注入

Hilt 是 Google 提供的一个现代化的依赖注入（DI）框架，基于 Dagger，用于简化 Android 应用的依赖注入。它减少了 Dagger 中一些样板代码和配置的复杂性，并且与 Android 的生命周期和组件（如 Activity, Fragment, ViewModel 等）进行了紧密的集成。

Hilt 的设计使得依赖注入变得更加易于使用，并且和 Android 官方的架构组件（如 ViewModel, LiveData, WorkManager 等）兼容。

* @Inject：用来标记构造函数、字段或方法，表示需要进行依赖注入。
* @Module 和 @InstallIn：通过模块提供依赖并指定依赖的作用域（例如 ActivityComponent、SingletonComponent 等）。
* @Component：在 Hilt 中，组件的创建和管理由 Hilt 自动完成，不需要显式地定义。


使用 @Inject 注解来标记需要注入的构造函数、字段或方法。
 

	public class UserRepository {
	    private final ApiService apiService;
	
	    @Inject //这里需要依赖注入 ApiService。
	    public UserRepository(ApiService apiService) {
	        this.apiService = apiService;
	    }
	
	    public void getUserData() {
	        // 使用 apiService 获取数据
	    }
	}
	
 @Inject标记构造方法，标识这里需要依赖注入一个ApiService，注意是由外部注入，Inject是个动词，外面Inject，不是自己被注入到别人。
 
	 @Module
	@InstallIn(SingletonComponent.class)  // 指定作用域为 Singleton
	public class NetworkModule {
	    @Provides
	    @Singleton //明确标记某个依赖为单例，通常和 @InstallIn(SingletonComponent.class) 配合使用。
	    public ApiService provideApiService() {
	        return new ApiServiceImpl();
	    }
	}
	
Module提供需要的依赖，	ApiServiceImpl在需要注入的地方法，提供注入，SingletonComponent 是 Hilt 中的一个作用域，它用于指定依赖的生命周期范围，通常用于 单例 级别的依赖注入。通过使用 SingletonComponent，Hilt 可以确保在整个应用程序的生命周期内，只会创建和使用一个该组件所提供的实例，它与 @Singleton 注解一起使用来确保唯一性。Singleton实例会在应用程序启动时创建，并且在整个应用生命周期内只会创建一次。

## 作用域：主要用在作用的范畴以及生命周期

主要用在作用的范畴，比如Application声明周期，Activity生命周期。

Singleton 和 SingletonComponent 都与单例模式相关，但它们在 Hilt 中的作用和使用方式有所不同。让我们深入了解这两者的作用、区别以及它们的使用场景。**@Singleton 是 Dagger 和 Hilt 提供的一个注解，用于标记一个类或一个依赖提供方法**，表示该类或依赖实例应该在整个应用程序中共享。它保证该依赖在应用程序生命周期内只有一个实例，并且在所有需要注入该依赖的地方使用同一个实例。

作用：@Singleton 注解是用来确保所标记的类或方法返回的依赖实例是单例的，也就是说，这个实例会在应用程序的生命周期内保持唯一。
使用方式：通常结合 Hilt 模块中的 @Provides 注解或直接标记类上的构造函数来实现。

**SingletonComponent 是 Hilt 的一个 作用域**，用于指定模块提供的依赖的生命周期范围。它表示该模块提供的依赖是应用级的单例，依赖实例在应用程序的生命周期内只会被创建一次。

**正确使用：@Singleton + SingletonComponent**

* @Singleton 注解和 SingletonComponent 作用域是紧密相连的，必须一起使用才能确保正确的单例行为。
* 如果你没有一起使用，可能会导致依赖生命周期不一致，违反单例模式，或者多个实例的问题。
* 正确的做法是：将模块使用 @InstallIn(SingletonComponent.class) 指定作用域，并且使用 @Singleton 来标记依赖为单例。

@InstallIn(ActivityComponent.class)  标识是Activity级别的，如果ActivityComponent + Singleton  表示该依赖的生命周期仅限于某个 Activity，即依赖只在 Activity 的生命周期内存在。ActivityComponent 配合使用，那么依赖将只能在一个 Activity 生命周期内共享实例。也就是说，它不是应用级别的单例，**而只是 在每个 Activity 中都有一个单例实例**。 ActivityComponent 会覆盖 @Singleton 的效果，如果只需要 在 Activity 内共享 单例，可以使用 @ActivityScoped，而不是直接使用 @Singleton：，


## 通过EntryPoint注入，

@AndroidEntryPoint 是 Hilt 依赖注入框架中的一个注解，它用于标记 Android 组件（如 Activity、Fragment、Service、BroadcastReceiver 和 View）使其能够支持 Hilt 自动注入依赖。不过在场景上是有选择的，并非所有的类都支持直接注入。注入的场景们如下

* 让 Android 组件（Activity、Fragment、Service、BroadcastReceiver）支持 Hilt 依赖注入。
* 在 Fragment 中使用 Hilt 时，宿主 Activity 也必须有 @AndroidEntryPoint。
* ViewModel 需要使用 @HiltViewModel，而不需要 @AndroidEntryPoint。

		@AndroidEntryPoint  // 必须在 Activity/Fragment 上加上此注解
		public class MainActivity extends AppCompatActivity {
		    @Inject
		    UserRepository userRepository;
		    
		    <!--注入ViewModel-->
		    
	      @AndroidEntryPoint
		public class MainActivity extends AppCompatActivity {
		
		    @Inject
		    MainViewModel mainViewModel;
	    
 Activity 或 Fragment 中，可以使用 @Inject 来注入依赖。哪里需要就注入哪里, 如果想在普通类中使用
 
##  自己注入：通过interface 、EntryPoint主动注入到普通类

有时候，普通类并不会被 Activity 直接注入，而是 自己创建的，例如：

	class MyRepository @Inject constructor() {
	    fun fetchData(): String {
	        return "数据获取成功"
	    }
	}


	@Module
	@InstallIn(SingletonComponent::class)  // 作用域：整个应用
	object AppModule {
	    @Provides
	    fun provideMyRepository(): MyRepository {
	        return MyRepository()
	    }
	
	    @Provides
	    fun provideMyServiceHelper(myRepository: MyRepository): MyServiceHelper {
	        return MyServiceHelper(myRepository)
	    }
	}

 
	 @EntryPoint
	@InstallIn(SingletonComponent::class)
	interface MyServiceHelperEntryPoint {
	    fun getMyRepository(): MyRepository
	}
	

然后在普通类中：
  
		class MyServiceHelper(context: Context) {
	    private val myRepository: MyRepository by lazy {
	        EntryPointAccessors.fromApplication(
	            context.applicationContext,
	            MyServiceHelperEntryPoint::class.java
	        ).getMyRepository()
	    }
	
	    fun execute() {
	        println(myRepository.fetchData())
	    }
	}
	
普通类要通过 EntryPoint这样的做法来处理，自己处理如何提供依赖 ，其实就是通过interface
