# 核心  ViewModel处理数据[服务器]的获取、整理、[服务器]更新 +UI响应+[内聚]，ViewModel是变化的Presenter，不过Compose内涵了响应式编程，其实就是数据绑定

## MVVM框架

jetpack compose的 MVVM模式 其实就是用Viewmode代替了 presenter，同时配套提供了相应的数据驱动，响应式编程的一些工具类，比如MutableStateFlow，类似LiveData等。

![MVVM](https://i-blog.csdnimg.cn/direct/3f5188fed1ae46ee95740d0fcb0a8a4a.png)

基本使用

	class MainViewModel: ViewModel() {
	    private val _stateFlow= MutableStateFlow("Hello World")
	    val stateFlow = _stateFlow.asStateFlow()
	   
	   
	   <!--相应UI交互的相应函数--> 
	        fun onEvent(event: Event) {
	        
	    ...
	    
	    <!--定义事件-->
	    sealed interface Event {
        data object  Stopped : Event
        data object  Resume : Event
    }
    
    	    <!--定义数据-->
      data class UiState(
      
      		  val msgIndex:Int = 0,
 				
      )
    
	}

MutableStateFlow可以在初期看做是LiveData的替代，其实就是另一种为了感知数据变化的数据组件。_stateFlow与_stateFlow.asStateFlow()一个为了对内，一个对外，对内可修改，对外只读且响应式呈现UI，这样可以将数据修改全部内聚在ViewModel里面。源码里可以看到会被MutableStateFlow封装。

	public fun <T> MutableStateFlow<T>.asStateFlow(): StateFlow<T> =
	    ReadonlyStateFlow(this, null)

UI 交互如何影响ViewMode呢，其实是通过事件来完成，用发消息，替换掉了之前Presenter的直接处理

	@Composable
	fun MainScreen(
	    state:  MainViewModel .UiState,//这里传入的一般是被rember封装过的数据
	    sendEvent: (event: MainViewModel.Event) -> Unit,
	) 

如何使用

	// viewmode注入，类似presenter new
	val viewModel = hiltViewModel<MainViewModel>() 
	//获取remember 的 uiState，如果需要感知声明周期，可以用其他函数
	val uiState by viewModel.uiState.collectAsStateWithLifecycle()  
	//构造界面  注意这里传入 uiState，其实是已经被remember封装过的  uiState，具备响应式能力
	MainScreen(   state = uiState, sendEvent = viewModel::onEvent  ) 

**可以看出 MutableStateFlow并不具备记忆能力**，通过collectAsStateWithLifecycle才行，其实对于普通业务开发而言，两者都能完成需求，StateFlow更灵活而已。


## remember用法 ：compose函数，用来记录compose中变量的状态 

  **remember==缓存【甚至说局部单利】**

remember的记住什么？主要是告诉当前组件，会记住某个值，或者说会缓存某个值，防止View重绘每次都用初始的值，如果已经记过了，就可能会用缓存的值，remember不是为了监听变化，相反，是为了提醒用缓存，变化是 MutableState的作用, 记住是remember的作用这样在重绘的时候，可以用新的值，**MutableState会触发特定位置的重绘，remember会让重绘使用缓存值**：

	Remember the value produced by calculation. calculation will only be evaluated during the composition. Recomposition will always return the value produced by composition.

	@Composable
	inline fun <T> remember(
	    vararg keys: Any?,
	    crossinline calculation: @DisallowComposableCalls () -> T //用来计算值用的说通俗一些，一般来说key不变，只会调用一次，重绘之后，调用
 
	): T {
	    var invalid = false
	    for (key in keys) invalid = invalid or currentComposer.changed(key)
	    return currentComposer.cache(invalid, calculation)
	}

remember的lambda只是定义了一个如何去计算state值的算式，并没有执行，当这个函数组合且Compose框架判断需要依据lambda去获取这个state的值时，这时，这个lambda就会被执行，lambda的返回值就是计算结果，那么这个Composable函数后面访问到这个状态，访问的都是lambda的计算结果，

可变参数可以没有，也可以传递其他么rember的变量

	@Composable
	fun Greeting(name: String, age: Int) {
	    val message = remember(name, age) { "Hello $name, you are $age years old!" }
	 
	}

name跟age任何一个变了，都会重新计算。 mutableStateOf(0)是让compose感知到变回，以便重组，但是不负责记忆，remember会负责记忆。
 
	@Composable
	fun Component() {
	    val myText = mutableStateOf(0)
	
	    Column {
	        Text(text = myText.value.toString())
	        Button(onClick = { myText.value++ }) {}
	    }
	
	 }
	 
会重组，但是不会变化，mutableStateOf其实相当于监听，观察者模式mutable State。但是没记住，每次重新调用Component都会初始。为了让让状态能够跨越重组而持久存在，就要把它放在函数外，其实类似于我们说的全局变量，不过有时候可能是自己触发自己，不需要放外边，那就自己remember，正如上面说的**MutableStateFlow并不具备记忆能力**，只具备触发的能力，变化之后，与之关联的UI要刷新，MutableStateFlow出现在那个Compose函数中，就会触发哪个函数重绘 用例：
 
	class MainActivity : ComponentActivity() {
	    override fun onCreate(savedInstanceState: Bundle?) {
	        super.onCreate(savedInstanceState)
	        val model = GreetingViewModel()
	        setContent {
	            val value = model._stateFlow.collectAsState()
            //Greeting 状态提升 _stateFlow是MutableState ，并且被remember
	            Greeting(value.value, model.onEvent)
	        }   }}

	@Composable
	fun Greeting(model: GreetingViewModel.UiState, event: (() -> Unit)?) {
	
	//    这里只会计算一次 mutableStateOf(1)  否则直接返回缓存值
    		val count = remember { mutableStateOf(1) }
    //    另外，如果是外部使用 count本身也是个mutable变量 就在局部自己负责自己，

	    LazyColumn(
	    ) {
	        for (i in 0..100) {
	            item {
	                Button(onClick = {
	                    event?.invoke()
	                }) { Text(
                        text = "" + model.title + " " + model.content +   count.value,
	                        modifier = Modifier.fillMaxSize() ) } } }}}
	@Preview
	@Composable
	fun GreetingPreview() {
	    // 这里value是一个MutableState,同时它的值在这里被记录了，
	    //  并且记录的作用域应该也是可以调整的。毕竟model也牵扯到复用
	    Greeting(GreetingViewModel.UiState("title", "content"), null)
	}
	class GreetingViewModel : ViewModel() {
	    private val stateFlow = MutableStateFlow(UiState("title", "content"))  //内部
	    val _stateFlow: StateFlow<UiState> = stateFlow.asStateFlow()//外部 更新限制在内部，其实也挺烦
	    val onEvent = fun() {
	        // 值更新了，同步更新缓存，同时会触发UI重绘MutableState ，这个对象被外边remember了
	        stateFlow.update {
	            it.copy(
	                content = "" + System.currentTimeMillis()
	            ) } }
	    data class UiState(var title: String, var content: String)
	}
	
	 
 上面的 Greeting如果不全局刷新，就不会计算从缓存再次去除值给count，但是count本事也是个mutable变量，而且在LazyColumn之外，它的变化，会引起LazyColumn 内部Item更新 ，并且值对于item而言是外部变量，所以也会更新。跟定一个外部mutableStateOf一样


	 val outerMutable = mutableStateOf(1)
	
	@Composable
	fun Greeting(model: GreetingViewModel.UiState, event: (() -> Unit)?) {
	//    这里只会计算一次 mutableStateOf(1)  否则直接返回缓存值
	    val count = remember { mutableStateOf(1) }
	//    另外，如果是外部使用 count本身也是个mutable变量 就在局部自己负责自己
	
	
	    LazyColumn(
	    		、、、
	      Button(onClick = {
 
	                    outerMutable.value++
	                }) {
	                    Text(
	                        text = "" + model.title + " " + model.content +   count.value + outerMutable.value,
	                        modifier = Modifier.fillMaxSize()
	                    )
			

**注意区分观察者模式与缓存的区别，也就是mutable与remmeber的区别**


## @Composable  到底是什么

Composable是函数，函数，kotlin函数，所以函数的一切特性还是存在的，参数，返回值等，它只是等被调用的函数，用来Compose。

 触发 Compose 重绘的因素

| 触发原因 | 说明 | 示例 |
|---------|------|------|
| **可组合函数参数变化** | 任何 `@Composable` 函数的参数变化都会触发重组 | `MyComposable(text)` 传入的新值不同 |
| **`remember` 变量变化** | 变量由 `remember` 或 `mutableStateOf` 维护，值变化会触发重组 | `val count by remember { mutableStateOf(0) }` |
| **`State` 变化** | `mutableStateOf` 变量改变，会触发依赖它的 Composable 重新执行 | `count++` 会导致依赖 `count` 的 UI 重新绘制 |
| **`rememberUpdatedState` 变化** | `rememberUpdatedState` 用于在 `LaunchedEffect` 等中监听最新值，但不强制重组 | `rememberUpdatedState(text)` 只更新值，不触发 UI 重绘 |
| **Composition 结构变化** | `if/else` 控制的 UI 结构发生改变 | `if (isVisible) Text("显示") else Text("隐藏")` |
| **`LaunchedEffect` 重新执行** | 依赖值变化会重新执行 `LaunchedEffect` | `LaunchedEffect(count) { ... }` |
| **`derivedStateOf` 变化** | 监听多个 `State` 变化，触发合并后的 UI 变化 | `val total by derivedStateOf { count1 + count2 }` |

## snapshotFlow

将state转换为Flow进行监听。LaunchedEffect 会多次触发，而 snapshotFlow 仅会触发一次（跳过重复值）：


    snapshotFlow { sliderValue }
        .debounce(300) // 只在用户停顿后再发送
        .collect { newValue -> updateVolume(newValue) }
        
 使用 snapshotFlow 的最佳场景：

* 	监听 State，但不想触发 UI 重新组合。
* 	防抖 & 限流（如搜索输入框、滑动条）。
* 	监听 State 并执行异步任务（如网络请求）。
	

重绘就是函数重新调用

### Flow的解释 


*  asStateFlow() 适合 UI 状态：  UI中配合collectAsState使用 

持有最新数据，订阅时立即获取最新值。
适用于 ViewModel 存储 UI 状态（如 text、count）。

* 🔹 asSharedFlow() 适合事件通知：配合collect使用，不用考虑state更新UI，

不会存储数据，只推送新事件（如 Toast、Snackbar）。
适用于一次性事件，防止旧事件误触发。

* callbackFlow 将回调转换为 Flow。

		callbackFlow {
		    val listener = object : MyListener {
		        override fun onEvent(data: String) {
		            trySend(data).onFailure {
		                Log.e("callbackFlow", "数据丢失: $data")
		            }
		        }
		    }
		
		    MyApi.registerListener(listener)
		    awaitClose { MyApi.unregisterListener(listener) }
		}.buffer(Channel.CONFLATED) // 仅保留最新数据
 

## callbackFlow vs suspendCancellableCoroutine



callbackFlow vs suspendCancellableCoroutine 的区别
 

* callbackFlow：用于 将持续回调（如监听器）转换为 Flow，适用于多次回调的场景。
* suspendCancellableCoroutine：用于 将一次性回调转换为挂起函数，适用于单次回调的场景。

🔹 何时使用？

* ✅ 使用 callbackFlow

适用于：持续回调（监听 GPS、WebSocket、网络状态）。
示例：监听音量变化、监听传感器数据。

* ✅ 使用 suspendCancellableCoroutine

适用于：一次性回调（获取一次位置、请求一次权限）。
示例：获取用户当前位置、执行一次 API 调用。


### MutableSharedFlow MutableStateFlow

MutableStateFlow用于状态的同步，无论是先注册，还是后注册，collet回调一定会有，适用于状态保持一致，而MutableSharedFlow主要用于事件传递通知，而且，非常重要的一点：先 emit() 才会有数据。


可以认为MutableSharedFlow一定要主动触发，才有collect回调，而MutableStateFlow第一次必定有回调，为了保持同步。所以如果是要监听事件，就用MutableSharedFlow，如果UI状态一致，用MutableStateFlow




### ✅ `MutableStateFlow` vs `snapshotFlow` 使用场景对比

| 场景 | 推荐使用 |
|------|-----------|
| ViewModel 中的业务状态（例如用户输入、网络数据） | ✅ `MutableStateFlow` |
| Compose 中监听 UI 状态（如滑动、当前页、选择项等） | ✅ `snapshotFlow` |
| 想让多个组件共享状态 | ✅ `StateFlow` / `MutableStateFlow` |
| 想从某个 `@Composable` 变量派生出 Flow | ✅ `snapshotFlow` |





### 参考文档


[LiveData vs MutableStateFlow in Android Kotlin: A Comprehensive Comparison](https://medium.com/@rushabhprajapati20/livedata-vs-mutablestateflow-in-android-kotlin-a-comprehensive-comparison-a186848d410c#id_token=eyJhbGciOiJSUzI1NiIsImtpZCI6IjMxYjhmY2NiMmU1MjI1M2IxMzMxMzhhY2YwZTU2NjMyZjA5OTU3ZWUiLCJ0eXAiOiJKV1QifQ.eyJpc3MiOiJodHRwczovL2FjY291bnRzLmdvb2dsZS5jb20iLCJhenAiOiIyMTYyOTYwMzU4MzQtazFrNnFlMDYwczJ0cDJhMmphbTRsamRjbXMwMHN0dGcuYXBwcy5nb29nbGV1c2VyY29udGVudC5jb20iLCJhdWQiOiIyMTYyOTYwMzU4MzQtazFrNnFlMDYwczJ0cDJhMmphbTRsamRjbXMwMHN0dGcuYXBwcy5nb29nbGV1c2VyY29udGVudC5jb20iLCJzdWIiOiIxMTE1MjExOTEzNDY2MDE4OTQ2NjIiLCJlbWFpbCI6ImhhcHB5bGlzaGFuZzI5NTZAZ21haWwuY29tIiwiZW1haWxfdmVyaWZpZWQiOnRydWUsIm5iZiI6MTczNTE5Mjk1NywibmFtZSI6IlMgTCIsInBpY3R1cmUiOiJodHRwczovL2xoMy5nb29nbGV1c2VyY29udGVudC5jb20vYS9BQ2c4b2NKbDNReTFLZzFXMFVHbG5XSHV4ZzcwM1lLd0NoRTNYR1c2WUI2ZGg2NHpscHZZcEZBPXM5Ni1jIiwiZ2l2ZW5fbmFtZSI6IlMiLCJmYW1pbHlfbmFtZSI6IkwiLCJpYXQiOjE3MzUxOTMyNTcsImV4cCI6MTczNTE5Njg1NywianRpIjoiMzNkODExYTY3Zjg4NjRhNWQzZmY0ZWUzNzQzMTg4NDhlMjllZWQ3YSJ9.HMT5Wj4BqF_kP00wW1SdqNE35WDWidTMB1cbYAhX2S_oBiEs4ZmMnlWdHyy67IummOcMuDIzC8E4ytM95ZpzBMuyS_v_kYsEo48fURBTyQOjPxVHJiKkAH__rmErEp5jiizAwxgbTHY7xWdbm-o58qRXRtkw6hFI1DlB5lJfgo1U98rsr4yeqGNQnvNeAnalQhau_OeWYSdzpa9f9cQPPF0kfcpdN81XJaS_gCsaqSMNZcJoZBxpUC1d1L1BnXskIh71PEfpeuvTSzsTAaIZvwuwBiSNXmougzAp4PRvsdDBAiWVD75svHDRflYxxaWQ3i8VZDMszIy--wqhRHTFyg)
