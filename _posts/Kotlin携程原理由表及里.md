	
	   public final void startCoroutine() {
	      Function2 funTest = (Function2)(new Function2((Continuation)null) {
	         int label;
	
	         @Nullable
	         public final Object invokeSuspend(@NotNull Object $result) {
	            Object var3 = IntrinsicsKt.getCOROUTINE_SUSPENDED();
	            MainActivity var10000;
	            switch(this.label) {
	            case 0:
	               ResultKt.throwOnFailure($result);
	               String var2 = "funTest";
	               System.out.println(var2);
	               var10000 = MainActivity.this;
	               this.label = 1;
	               if (var10000.suspendFun1(this) == var3) {
	                  return var3;
	               }
	               break;
	            case 1:
	               ResultKt.throwOnFailure($result);
	               break;
	            case 2:
	               ResultKt.throwOnFailure($result);
	               return Unit.INSTANCE;
	            default:
	               throw new IllegalStateException("call to 'resume' before 'invoke' with coroutine");
	            }
	
	            var10000 = MainActivity.this;
	            this.label = 2;
	            if (var10000.suspendFun2(this) == var3) {
	               return var3;
	            } else {
	               return Unit.INSTANCE;
	            }
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
	      });
	      BuildersKt.launch$default((CoroutineScope)GlobalScope.INSTANCE, (CoroutineContext)Dispatchers.getDefault(), (CoroutineStart)null, funTest, 2, (Object)null);
	   }
	
	   @Nullable
	   public final Object suspendFun1(@NotNull Continuation $completion) {
	      String var2 = "suspendFun1";
	      System.out.println(var2);
	      return Unit.INSTANCE;
	   }
	
	   @Nullable
	   public final Object suspendFun2(@NotNull Continuation $completion) {
	      String var2 = "suspendFun2";
	      System.out.println(var2);
	      return Unit.INSTANCE;
	   }