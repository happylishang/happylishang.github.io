	ThreadLocal ：This class provides thread-local variables.


一般跟静态变量一起，多通过静态方法获取，为了绑定特定线程属性，每个线程特有，最终回归到特定线程，比如Looper，本线程创建，其他线程通过引用，影响本线程。

	private static void prepare(boolean quitAllowed) {
	 if (sThreadLocal.get() != null) {
	 throw new RuntimeException("Only one Looper may be created per thread");
	 }
	 //将创建的 Looper 对象保存到 sThreadLocal 中。
	 sThreadLocal.set(new Looper(quitAllowed));
	}
	​
	​
	//从 ThreadLocal 取出 Looper 对象
	public static @Nullable Looper myLooper() {
	 return sThreadLocal.get();
	}
	
利用Handler发送消息时候，可能在其他线程，但是处理只能在Handler所绑定的线程。
	
通过类来访问，如果不利用	ThreadLocal，可能一个进程只能一个Looper，这不合理，ThreadLocal就是为了每个Thread自己使用而创建的。