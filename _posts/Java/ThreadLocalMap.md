ThreadLocal->ThreadLocalMap->Thread

一个线程不仅仅持有一个ThreadLocal<T>的弱引用，如果你有两个ThreadLocal<>，怎么确定取出来的是哪个

4.ThreadLocal是为了让线程能够保存属于自己的私有对象，需要的时候能够通过get取出来。

	ThreadLocal tl1= new ThreadLocal();
	ThreadLocal tl2= new ThreadLocal();
	tl1.set("value1");  // 第一个Entry就是<tl1,"value1">
	tl2.set("value2"); // 第二个Entry就是<tl2,"value2">

