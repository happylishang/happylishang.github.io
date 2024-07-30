Retrofit是一个比较流行的网络请求框架，底层封装了Okhttp。准确来说，网络请求由okhttp来完成，Retrofit负责网络请求接口的封装及回调处理。对于Android开发而言，Retrofit后期加入了对kotlin协程的支持，使用起来更加丝滑。

## 基本用法

	interface API {
	    @GET("react-native/movies.json")
	   suspend fun getMovies(): JsonObject
	}


### 301处理 

>  协程会挂起，线程一般不必，因为协程的本质是回调而非线程【优化线程使用模型，简化耗时任务的回调写法】

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
	  

扩展属性，让协程用起来确实非常简单， ViewModel.viewModelScope其实就算是给了一个CoroutineScope，+ Dispatchers.Main.immediate，让其具备派发协程任务多能力，你甚至可以给字符串扩展一个

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
	
### 	责任链

同步执行

	  public Response execute() throws IOException {
	        synchronized(this) {
	            if (this.executed) {
	                throw new IllegalStateException("Already Executed");
	            }
	
	            this.executed = true;
	        }
	
	        this.transmitter.timeoutEnter();
	        this.transmitter.callStart();
	
	        Response var1;
	        try {
	            this.client.dispatcher().executed(this);
	            <!--真正的执行在这里，-->
	            var1 = this.getResponseWithInterceptorChain();
	        } finally {
	        <!--注意这里 这里已经执行完毕 派发结果-->
	            this.client.dispatcher().finished(this);
	        }
	
	        return var1;
	    }
	    
为什么说真正的执行在getResponseWithInterceptorChain 
	
	    Response getResponseWithInterceptorChain() throws IOException {
        List<Interceptor> interceptors = new ArrayList();
        interceptors.addAll(this.client.interceptors());
        interceptors.add(new RetryAndFollowUpInterceptor(this.client));
        interceptors.add(new BridgeInterceptor(this.client.cookieJar()));
        interceptors.add(new CacheInterceptor(this.client.internalCache()));
        interceptors.add(new ConnectInterceptor(this.client));
        if (!this.forWebSocket) {
            interceptors.addAll(this.client.networkInterceptors());
        }
		<!--CallServerInterceptor是最后一个Interceptor-->
        interceptors.add(new CallServerInterceptor(this.forWebSocket));
        Interceptor.Chain chain = new RealInterceptorChain(interceptors, this.transmitter, (Exchange)null, 0, this.originalRequest, this, this.client.connectTimeoutMillis(), this.client.readTimeoutMillis(), this.client.writeTimeoutMillis());
        boolean calledNoMoreExchanges = false;

        Response var5;
        try {
            Response response = chain.proceed(this.originalRequest);
            if (this.transmitter.isCanceled()) {
                Util.closeQuietly(response);
                throw new IOException("Canceled");
            }

            var5 = response;
        } catch (IOException var9) {
            calledNoMoreExchanges = true;
            throw this.transmitter.noMoreExchanges(var9);
        } finally {
            if (!calledNoMoreExchanges) {
                this.transmitter.noMoreExchanges((IOException)null);
            }

        }

        return var5;
    }
    
CallServerInterceptor是最后一个Interceptor ，这个CallServerInterceptor会真正的执行请求的任务，获取到请求结果之后通过his.client.dispatcher().finished(this);将结果派发出去，如此就完成了同步请求。
	
	    public Response intercept(Interceptor.Chain chain) throws IOException {
        RealInterceptorChain realChain = (RealInterceptorChain)chain;
        Exchange exchange = realChain.exchange();
        Request request = realChain.request();
        long sentRequestMillis = System.currentTimeMillis();
        exchange.writeRequestHeaders(request);
        boolean responseHeadersStarted = false;
        Response.Builder responseBuilder = null;
        if (HttpMethod.permitsRequestBody(request.method()) && request.body() != null) {
            if ("100-continue".equalsIgnoreCase(request.header("Expect"))) {
                exchange.flushRequest();
                responseHeadersStarted = true;
                exchange.responseHeadersStart();
                responseBuilder = exchange.readResponseHeaders(true);
            }

            if (responseBuilder == null) {
                BufferedSink bufferedRequestBody;
                if (request.body().isDuplex()) {
                    exchange.flushRequest();
                    bufferedRequestBody = Okio.buffer(exchange.createRequestBody(request, true));
                    request.body().writeTo(bufferedRequestBody);
                } else {
                    bufferedRequestBody = Okio.buffer(exchange.createRequestBody(request, false));
                    request.body().writeTo(bufferedRequestBody);
                    bufferedRequestBody.close();
                }
            } else {
                exchange.noRequestBody();
                if (!exchange.connection().isMultiplexed()) {
                    exchange.noNewExchangesOnConnection();
                }
            }
        } else {
            exchange.noRequestBody();
        }

        if (request.body() == null || !request.body().isDuplex()) {
            exchange.finishRequest();
        }

        if (!responseHeadersStarted) {
            exchange.responseHeadersStart();
        }

        if (responseBuilder == null) {
            responseBuilder = exchange.readResponseHeaders(false);
        }

        Response response = responseBuilder.request(request).handshake(exchange.connection().handshake()).sentRequestAtMillis(sentRequestMillis).receivedResponseAtMillis(System.currentTimeMillis()).build();
        int code = response.code();
        if (code == 100) {
            response = exchange.readResponseHeaders(false).request(request).handshake(exchange.connection().handshake()).sentRequestAtMillis(sentRequestMillis).receivedResponseAtMillis(System.currentTimeMillis()).build();
            code = response.code();
        }

        exchange.responseHeadersEnd(response);
        if (this.forWebSocket && code == 101) {
            response = response.newBuilder().body(Util.EMPTY_RESPONSE).build();
        } else {
            response = response.newBuilder().body(exchange.openResponseBody(response)).build();
        }

        if ("close".equalsIgnoreCase(response.request().header("Connection")) || "close".equalsIgnoreCase(response.header("Connection"))) {
            exchange.noNewExchangesOnConnection();
        }

        if ((code == 204 || code == 205) && response.body().contentLength() > 0L) {
            throw new ProtocolException("HTTP " + code + " had non-zero Content-Length: " + response.body().contentLength());
        } else {
            return response;
        }
    }

如何构建RealCall的呢

	retrofit.create(API::class.java)

创建代理

    public <T> T create(final Class<T> service) {
        this.validateServiceInterface(service);
        return Proxy.newProxyInstance(service.getClassLoader(), new Class[]{service}, new InvocationHandler() {
            private final Platform platform = Platform.get();
            private final Object[] emptyArgs = new Object[0];

            @Nullable
            public Object invoke(Object proxy, Method method, @Nullable Object[] args) throws Throwable {
                if (method.getDeclaringClass() == Object.class) {
                    return method.invoke(this, args);
                } else {
                    args = args != null ? args : this.emptyArgs;
                    return this.platform.isDefaultMethod(method) ? this.platform.invokeDefaultMethod(method, service, proxy, args) : Retrofit.this.loadServiceMethod(method).invoke(args);
                }
            }
        });
    }
    
得到一个API代理实例，在其调用方法的时候，会调用	invoke， 这里其实会调用

	Retrofit.this.loadServiceMethod(method).invoke(args);

method包含着方法的签名，

	 ServiceMethod<?> loadServiceMethod(Method method) {
	 <!--缓存-->
	        ServiceMethod<?> result = (ServiceMethod)this.serviceMethodCache.get(method);
	        if (result != null) {
	            return result;
	        } else {
	            synchronized(this.serviceMethodCache) {
	                result = (ServiceMethod)this.serviceMethodCache.get(method);
	                if (result == null) {
	                    result = ServiceMethod.parseAnnotations(this, method);
	                    this.serviceMethodCache.put(method, result);
	                }
	
	                return result;
	            }
	        }
	    }
	    	
开始缓存为null，会调用parseAnnotations构建ServiceMethod

    static <T> ServiceMethod<T> parseAnnotations(Retrofit retrofit, Method method) {
        RequestFactory requestFactory = RequestFactory.parseAnnotations(retrofit, method);
        Type returnType = method.getGenericReturnType();
        if (Utils.hasUnresolvableType(returnType)) {
            throw Utils.methodError(method, "Method return type must not include a type variable or wildcard: %s", new Object[]{returnType});
        } else if (returnType == Void.TYPE) {
            throw Utils.methodError(method, "Service methods cannot return void.", new Object[0]);
        } else {
        <!--返回数据走这里-->
            return HttpServiceMethod.parseAnnotations(retrofit, method, requestFactory);
        }
    }
    
根据返回数据构建

            return HttpServiceMethod.parseAnnotations(retrofit, method, requestFactory);

这里的构造比较复杂 

    static <ResponseT, ReturnT> HttpServiceMethod<ResponseT, ReturnT> parseAnnotations(Retrofit retrofit, Method method, RequestFactory requestFactory) {
        boolean isKotlinSuspendFunction = requestFactory.isKotlinSuspendFunction;
        boolean continuationWantsResponse = false;
        boolean continuationBodyNullable = false;
        Annotation[] annotations = method.getAnnotations();
        Object adapterType;
        Type responseType;
        <!--是不是kotlin协程函数，如果是处理协程-->
        if (isKotlinSuspendFunction) {
            Type[] parameterTypes = method.getGenericParameterTypes();
            responseType = Utils.getParameterLowerBound(0, (ParameterizedType)parameterTypes[parameterTypes.length - 1]);
            if (Utils.getRawType(responseType) == Response.class && responseType instanceof ParameterizedType) {
                responseType = Utils.getParameterUpperBound(0, (ParameterizedType)responseType);
                continuationWantsResponse = true;
            }

            adapterType = new Utils.ParameterizedTypeImpl((Type)null, retrofit2.Call.class, new Type[]{responseType});
            annotations = SkipCallbackExecutorImpl.ensurePresent(annotations);
        } else {
            adapterType = method.getGenericReturnType();
        }
	<!--构造 CallAdapter 可以自定义-->
        CallAdapter<ResponseT, ReturnT> callAdapter = createCallAdapter(retrofit, method, (Type)adapterType, annotations);
        
        responseType = callAdapter.responseType();
        if (responseType == okhttp3.Response.class) {
            throw Utils.methodError(method, "'" + Utils.getRawType(responseType).getName() + "' is not a valid response body type. Did you mean ResponseBody?", new Object[0]);
        } else if (responseType == Response.class) {
            throw Utils.methodError(method, "Response must include generic type (e.g., Response<String>)", new Object[0]);
        } else if (requestFactory.httpMethod.equals("HEAD") && !Void.class.equals(responseType)) {
            throw Utils.methodError(method, "HEAD method must use Void as response type.", new Object[0]);
        } else {
            Converter<ResponseBody, ResponseT> responseConverter = createResponseConverter(retrofit, method, responseType);
            Call.Factory callFactory = retrofit.callFactory;
            if (!isKotlinSuspendFunction) {
            <!--嵌套callAdapter-->
                return new CallAdapted(requestFactory, callFactory, responseConverter, callAdapter);
            } else {
            <!--协程类-->
                return (HttpServiceMethod)(continuationWantsResponse ? new SuspendForResponse(requestFactory, callFactory, responseConverter, callAdapter) : new SuspendForBody(requestFactory, callFactory, responseConverter, callAdapter, continuationBodyNullable));
            }
        }
    }

会创不同的Converter这里其实支持自定义添加，根据返回动态推断用哪个比如与  GsonConverterFactory对应的GsonResponseBodyConverter、GsonRequestBodyConverter

    private static <ResponseT> Converter<ResponseBody, ResponseT> createResponseConverter(Retrofit retrofit, Method method, Type responseType) {
        Annotation[] annotations = method.getAnnotations();

        try {
            return retrofit.responseBodyConverter(responseType, annotations);
        } catch (RuntimeException var5) {
            throw Utils.methodError(method, var5, "Unable to create converter for %s", new Object[]{responseType});
        }
    }

最后返回一个CallAdapter


            if (!isKotlinSuspendFunction) {
                return new CallAdapted(requestFactory, callFactory, responseConverter, callAdapter);
            } else {
                return (HttpServiceMethod)(continuationWantsResponse ? new SuspendForResponse(requestFactory, callFactory, responseConverter, callAdapter) : new SuspendForBody(requestFactory, callFactory, responseConverter, callAdapter, continuationBodyNullable));
            }    
    
在不是协程的情况直接返回CallAdapted，里面有callFactory负责构建后面的执行Call，如果用的是OkHttpClient，那么构建Call就是

    public Call newCall(Request request) {
        return RealCall.newRealCall(this, request, false);
    }
     
    @Nullable
    final ReturnT invoke(Object[] args) {
        retrofit2.Call<ResponseT> call = new OkHttpCall(this.requestFactory, args, this.callFactory, this.responseConverter);
        return this.adapt(call, args);
    }
    
各种参数被传递进去

### addCallAdapterFactory
	
 Retrofit 的addCallAdapterFactory与 client其实类似，
 
         public Builder client(OkHttpClient client) {
            return this.callFactory((Call.Factory)Objects.requireNonNull(client, "client == null"));
        }
 
     public Call newCall(Request request) {
        return RealCall.newRealCall(this, request, false);
    }

    private RealCall(OkHttpClient client, Request originalRequest, boolean forWebSocket) {
        this.client = client;
        this.originalRequest = originalRequest;
        this.forWebSocket = forWebSocket;
    }'
   
 RealCall引用了派发器等工具， 这样整个流程就串联起来了。我们API声明的方法，其实会在HttpServiceMethod.parseAnnotations全面利用，之后后面包装成RealCall。如此就完成了调用。
 
 
##  协程如何处理的

协程在编译后，会转换成Java的回调，kotlin定制的API 方法接口转java后是什么样呢，如下

原函数

	interface API {
	    @GET("topic/5433d5e4e737cbe96dcef312")
	    suspend fun getMovies(): JsonObject
	
	    @GET("topic/5433d5e4e737cbe96dcef312")
	    suspend fun getMovies(@Query("uid") userId: String): JsonObject
	
	    //    @GET("react-native/movies.json")
	//    fun getMovies2(): JsonObject //失败
	    @GET("topic/5433d5e4e737cbe96dcef312")
	    fun getMVINfo(): Call<JsonObject>
	}

编译后，接口还是都在的，函数模型签名都在的

	public interface API {
	    @GET("topic/5433d5e4e737cbe96dcef312")
	    Call<JsonObject> getMVINfo();
	
	    @GET("topic/5433d5e4e737cbe96dcef312")
	    Object getMovies(@Query("uid") String str, Continuation<? super JsonObject> continuation);
	
	    @GET("topic/5433d5e4e737cbe96dcef312")
	    Object getMovies(Continuation<? super JsonObject> continuation);
	}

可以看到转换后，两者的有明显差距，函数多了一个Continuation参数在最后，有这个就说明是个suspend函数，参数解析的时候，如果函数有Continuation.class参数，就可以看做是suspend函数，
          
            private ParameterHandler<?> parseParameter(int p, Type parameterType, @Nullable Annotation[] annotations, boolean allowContinuation) {
            ParameterHandler<?> result = null;
            if (annotations != null) {
                Annotation[] var6 = annotations;
                int var7 = annotations.length;

                for(int var8 = 0; var8 < var7; ++var8) {
                    Annotation annotation = var6[var8];
                    ParameterHandler<?> annotationAction = this.parseParameterAnnotation(p, parameterType, annotations, annotation);
                    if (annotationAction != null) {
                        if (result != null) {
                            throw Utils.parameterError(this.method, p, "Multiple Retrofit annotations found, only one allowed.", new Object[0]);
                        }
                        result = annotationAction;
                    }
                }
            }

            if (result == null) {
                if (allowContinuation) {
                    try {
                        if (Utils.getRawType(parameterType) == Continuation.class) {
                            this.isKotlinSuspendFunction = true;
                            return null;
                        }
                    } catch (NoClassDefFoundError var11) {
                    }
                }

                throw Utils.parameterError(this.method, p, "No Retrofit annotation found.", new Object[0]);
            } else {
                return result;
            }
        }
回头再看之前的构建HttpServiceMethod parseAnnotations

	  static <ResponseT, ReturnT> HttpServiceMethod<ResponseT, ReturnT> parseAnnotations(Retrofit retrofit, Method method, RequestFactory requestFactory) {
	        boolean isKotlinSuspendFunction = requestFactory.isKotlinSuspendFunction;
	        boolean continuationWantsResponse = false;
	        boolean continuationBodyNullable = false;
	        Annotation[] annotations = method.getAnnotations();
	        Object adapterType;
	        Type responseType;
	        if (isKotlinSuspendFunction) {
	        <!--如果是kotlin挂起函数如下处理-->
	            Type[] parameterTypes = method.getGenericParameterTypes();
	            <!--返回类型在parameterTypes.length - 1的参数给出 -->
	            responseType = Utils.getParameterLowerBound(0, (ParameterizedType)parameterTypes[parameterTypes.length - 1]);
	            if (Utils.getRawType(responseType) == Response.class && responseType instanceof ParameterizedType) {
	                responseType = Utils.getParameterUpperBound(0, (ParameterizedType)responseType);
	                continuationWantsResponse = true;
	            }
	
	            adapterType = new Utils.ParameterizedTypeImpl((Type)null, retrofit2.Call.class, new Type[]{responseType});
	            annotations = SkipCallbackExecutorImpl.ensurePresent(annotations);
	        } else {
	            adapterType = method.getGenericReturnType();
	        }
		<!--解析后封装为callAdapter-->
	        CallAdapter<ResponseT, ReturnT> callAdapter = createCallAdapter(retrofit, method, (Type)adapterType, annotations);
	        responseType = callAdapter.responseType();
	        if (responseType == okhttp3.Response.class) {
	            throw Utils.methodError(method, "'" + Utils.getRawType(responseType).getName() + "' is not a valid response body type. Did you mean ResponseBody?", new Object[0]);
	        } else if (responseType == Response.class) {
	            throw Utils.methodError(method, "Response must include generic type (e.g., Response<String>)", new Object[0]);
	        } else if (requestFactory.httpMethod.equals("HEAD") && !Void.class.equals(responseType)) {
	            throw Utils.methodError(method, "HEAD method must use Void as response type.", new Object[0]);
	        } else {
	            Converter<ResponseBody, ResponseT> responseConverter = createResponseConverter(retrofit, method, responseType);
	            Call.Factory callFactory = retrofit.callFactory;
	            if (!isKotlinSuspendFunction) {
	                return new CallAdapted(requestFactory, callFactory, responseConverter, callAdapter);
	            } else {
	            <!--关键点 回到处理都在这里-->
	                return (HttpServiceMethod)(continuationWantsResponse ? new SuspendForResponse(requestFactory, callFactory, responseConverter, callAdapter) : new SuspendForBody(requestFactory, callFactory, responseConverter, callAdapter, continuationBodyNullable));
	            }
	        }
	    }
	    
如果设定了返回值这里返回SuspendForBody

    static final class SuspendForBody<ResponseT> extends HttpServiceMethod<ResponseT, Object> {
        private final CallAdapter<ResponseT, retrofit2.Call<ResponseT>> callAdapter;
        private final boolean isNullable;

        SuspendForBody(RequestFactory requestFactory, Call.Factory callFactory, Converter<ResponseBody, ResponseT> responseConverter, CallAdapter<ResponseT, retrofit2.Call<ResponseT>> callAdapter, boolean isNullable) {
            super(requestFactory, callFactory, responseConverter);
            this.callAdapter = callAdapter;
            this.isNullable = isNullable;
        }

        protected Object adapt(retrofit2.Call<ResponseT> call, Object[] args) {
            call = (retrofit2.Call)this.callAdapter.adapt(call);
            Continuation<ResponseT> continuation = (Continuation)args[args.length - 1];

            try {
                return this.isNullable ? KotlinExtensions.awaitNullable(call, continuation) : KotlinExtensions.await(call, continuation);
            } catch (Exception var5) {
                return KotlinExtensions.suspendAndThrow(var5, continuation);
            }
        }
    }

适配器adapt调用的时候， KotlinExtensions.await(call, continuation)，等待请求结束，然后处理回调



	  public static final <T> Object await(Call<T> call, Continuation<? super T> continuation) {
	        CancellableContinuationImpl cancellable$iv = new CancellableContinuationImpl(IntrinsicsKt.intercepted(continuation), 1);
	        CancellableContinuationImpl continuation2 = cancellable$iv;
	        continuation2.invokeOnCancellation(new Function1<Throwable, Unit>(call) { // from class: retrofit2.KotlinExtensions$await$$inlined$suspendCancellableCoroutine$lambda$1
	            final /* synthetic */ Call $this_await$inlined;
	
	            /* access modifiers changed from: package-private */
	            {
	                this.$this_await$inlined = r1;
	            }
	
	            /* Return type fixed from 'java.lang.Object' to match base method */
	            /* JADX DEBUG: Method arguments types fixed to match base method, original types: [java.lang.Object] */
	            @Override // kotlin.jvm.functions.Function1
	            public /* bridge */ /* synthetic */ Unit invoke(Throwable th) {
	                invoke(th);
	                return Unit.INSTANCE;
	            }
	
	            public final void invoke(Throwable it) {
	                this.$this_await$inlined.cancel();
	            }
	        });
	        call.enqueue(new Callback<T>(continuation2) { // from class: retrofit2.KotlinExtensions$await$2$2
	            final /* synthetic */ CancellableContinuation $continuation;
	
	            /* access modifiers changed from: package-private */
	            {
	                this.$continuation = $captured_local_variable$0;
	            }
	
	            @Override // retrofit2.Callback
	            public void onResponse(Call<T> call2, Response<T> response) {
	                Intrinsics.checkParameterIsNotNull(call2, NotificationCompat.CATEGORY_CALL);
	                Intrinsics.checkParameterIsNotNull(response, "response");
	                if (response.isSuccessful()) {
	                    Object body = response.body();
	            		 ...
	                    Result.Companion companion2 = Result.Companion;
	                         <!--成功了，这里处理回调-->
	                    this.$continuation.resumeWith(Result.m93constructorimpl(body));
	                    return;
	                }
	                Result.Companion companion3 = Result.Companion;
	                this.$continuation.resumeWith(Result.m93constructorimpl(ResultKt.createFailure(new HttpException(response))));
	            }
	
	            @Override // retrofit2.Callback
	            public void onFailure(Call<T> call2, Throwable t) {
	                Intrinsics.checkParameterIsNotNull(call2, NotificationCompat.CATEGORY_CALL);
	                Intrinsics.checkParameterIsNotNull(t, "t");
	                Result.Companion companion = Result.Companion;
	                this.$continuation.resumeWith(Result.m93constructorimpl(ResultKt.createFailure(t)));
	            }
	        });
	        Object result = cancellable$iv.getResult();
	        if (result == IntrinsicsKt.getCOROUTINE_SUSPENDED()) {
	            DebugProbesKt.probeCoroutineSuspended(continuation);
	        }
	        return result;
	    }

也就说，retrofit内部，将suspend回调的处理给实现了。	 
 
#### 参考文档

[又看一遍Retrofit源码，这次写了篇笔记 https://juejin.cn/post/6955856064969441317#heading-15] (https://juejin.cn/post/6955856064969441317#heading-15)  


