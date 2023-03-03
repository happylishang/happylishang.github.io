>  协程会挂起，线程一般不必，因为协程的本质是回调而非线程【优化线程使用模型，简化耗时任务的回调写法】
> 


	/**
	 * [CoroutineScope] tied to this [ViewModel].
	 * This scope will be canceled when ViewModel will be cleared, i.e [ViewModel.onCleared] is called
	 * This scope is bound to
	 * [Dispatchers.Main.immediate][kotlinx.coroutines.MainCoroutineDispatcher.immediate]
	 */
	public val ViewModel.viewModelScope: CoroutineScope
	    get() {
	        val scope: CoroutineScope? = this.getTag(JOB_KEY)
	        if (scope != null) {
	            return scope
	        }
	        return setTagIfAbsent(
	            JOB_KEY,
	            CloseableCoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
	        )
	    }
	  

扩展属性，让协程用起来确实非常简单，	    ViewModel.viewModelScope其实就算是给了一个CoroutineScope，+ Dispatchers.Main.immediate，让其具备派发协程任务多能力，你甚至可以给字符串扩展一个

	public val String.viewModelScope: CoroutineScope
	    get() {
	        val scope: CoroutineScope? = this.getTag(JOB_KEY)
	        if (scope != null) {
	            return scope
	        }
	        return setTagIfAbsent(
	            JOB_KEY,
	            CloseableCoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
	        )
	    }
	    
当然，这不是特别合理。	    

### retrofit调用

    fun testRetrofit() {
        lifecycleCoroutineScope.launch {
            val ret = RetrofitUnit.apiService.getUserInfo()
            println(ret.toString())
        }
    }
    
    
   代码翻译之后
   
       public final void testRetrofit() {
        Job unused = BuildersKt__Builders_commonKt.launch$default(this.lifecycleCoroutineScope, null, null, new CoroutinesTestModule$testRetrofit$1(null), 3, null);
    }
    
	 final class CoroutinesTestModule$testRetrofit$1 extends SuspendLambda implements Function2<CoroutineScope, Continuation<? super Unit>, Object> {
     
    @Override // kotlin.coroutines.jvm.internal.BaseContinuationImpl
    public final Object invokeSuspend(Object $result) {
        Object coroutine_suspended = IntrinsicsKt.getCOROUTINE_SUSPENDED();
        switch (this.label) {
            case 0:
                ResultKt.throwOnFailure($result);
                this.label = 1;
                <!--这里会有coroutine_suspended挂起-->
                Object userInfo = RetrofitUnit.INSTANCE.getApiService().getUserInfo(this);
                if (userInfo != coroutine_suspended) {
                    $result = userInfo;
                    break;
                } else {
                    return coroutine_suspended;
                }
            case 1:
                ResultKt.throwOnFailure($result);
                break;
            default:
                throw new IllegalStateException("call to 'resume' before 'invoke' with coroutine");
        }
        System.out.println((Object) ((ApiResult) $result).toString());
        return Unit.INSTANCE;
    }
} 

RetrofitUnit.INSTANCE.getApiService()同正常的Retrofit没什么区别，区别在getUserInfo调用，可以看到它传递了一个this 参数进去，它其实就是CoroutinesTestModule$testRetrofit$1，算是个回调的入口了、在方法调用时候，会在其他线程中启动一个协程，并且利用await回调，也就是收内部变相启动了协程asyn + await机制。

 RetrofitUnit.INSTANCE.getApiService().getUserInfo(this);

可以示意为如下流程调用：

	suspend getUserinfo(){
		var ret;
		CorotineScop(CurrentContext).asyn{
		
			ret=fetchinfo()
			
		}.await();
		
		return ret
	
	}
