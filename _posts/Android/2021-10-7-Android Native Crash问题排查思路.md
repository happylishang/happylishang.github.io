## 背景:定位难

对于Android APP而言，native层Crash相比于Java层更难捕获与定位，因为so的代码通常不可见，而且，一些第三方so的crash或者系统的更难定位，堆栈信息非常少：参考下面的几个native crash实例

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ea708a1d1e2846c082480ac157fbd238~tplv-k3u1fbpfcp-watermark.image?)

甚至即时全量打印Log信息，也只能得到一些不太方便定位的日志，无法直接定位问题

	09-14 10:14:36.590  1361  1361 I /system/bin/tombstoned: received crash request for pid 5908
	09-14 10:14:36.591  5944  5944 I crash_dump64: performing dump of process 5687 (target tid = 5908)
	09-14 10:14:36.607  5944  5944 F DEBUG   : *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***
	09-14 10:14:36.608  5944  5944 F DEBUG   : Build fingerprint: 'Xiaomi/vangogh/vangogh:10/QKQ1.191222.002/V12.0.6.0.QJVCNXM:user/release-keys'
	09-14 10:14:36.608  5944  5944 F DEBUG   : Revision: '0'
	09-14 10:14:36.608  5944  5944 F DEBUG   : ABI: 'arm64'
	09-14 10:14:36.608  5944  5944 F DEBUG   : Timestamp: 2021-09-14 10:14:36+0800
	09-14 10:14:36.608  5944  5944 F DEBUG   : pid: 5687, tid: 5908, name: nioEventLoopGro  >>> com.netxx.xaxxxn <<<
	09-14 10:14:36.608  5944  5944 F DEBUG   : uid: 10312
	09-14 10:14:36.608  5944  5944 F DEBUG   : signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0x4
	09-14 10:14:36.608  5944  5944 F DEBUG   : Cause: null pointer dereference
	09-14 10:14:36.608  5944  5944 F DEBUG   :     x0  0000000000000000  x1  0000000014d85fb0  x2  0000000015100bf8  x3  0000000000000000
	09-14 10:14:36.608  5944  5944 F DEBUG   :     x4  0000000015100c18  x5  000000000000005a  x6  0000000015100c30  x7  0000000000000018
	09-14 10:14:36.608  5944  5944 F DEBUG   :     x8  0000000000000000  x9  20454cc47a8eade3  x10 00000000005c0000  x11 000000000000004b
	09-14 10:14:36.608  5944  5944 F DEBUG   :     x12 000000000000001f  x13 0000000000000000  x14 00000000a2018668  x15 0000000000000010
	09-14 10:14:36.608  5944  5944 F DEBUG   :     x16 0000000000000000  x17 0000000000054402  x18 00000077328bc000  x19 00000077616e0c00
	09-14 10:14:36.608  5944  5944 F DEBUG   :     x20 0000000000000001  x21 00000000151004a0  x22 0000000014d85fb0  x23 00000000a1f03180
	09-14 10:14:36.608  5944  5944 F DEBUG   :     x24 0000000000000001  x25 0000000000000000  x26 0000000000000003  x27 00000000151000b8
	09-14 10:14:36.608  5944  5944 F DEBUG   :     x28 0000000000000000  x29 00000000151009b0
	09-14 10:14:36.608  5944  5944 F DEBUG   :     sp  000000773536e4f0  lr  000000779431b80c  pc  0000007794240260

如上，虽然能看到 Cause: null pointer dereference，但是到底是什么代码导致的，没有非常明确的消息，不像Java层Crash有非常清晰堆栈，这就让Native的crash定位非常头痛。


## 如何定位native crash

对于Crash而言，精确的定位等于成功的一半。如何通过工具定位到native crash呢，如果是自己实现的so库，一般而言还是会有相应的日志打印出来的，本文主要针对一些特殊的so，尤其是不存在源码的so，对于这种场景如何定位，最重要当然还是复现：匹配对应的机型、环境、不断重试复现线上问题，一旦发生Crash后就些蛛丝马迹可查，本文以线上偶发的一个ARM64升级为例子，分析下定位流程：**通过归纳，重试，复现场景后，便可以去查找问题日志**，这个时候有一个挺好用的方法：bugreport命令：

	$ adb bugreport  ~\  

app crash 的时候，系统会保存一个tombstone文件到/data/tombstones目录，该命令会导出最近的crash相关信息，我们可以通过bugreport导出，导出后它是一个zip包的形式，解压后如下


![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c28840e929ec40d9bf5448918b1eaa54~tplv-k3u1fbpfcp-watermark.image?)


对于每个tombstone，如果是native crash，打开后大概会看到如下日志：

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/07740494bee84e5ab3a293d99f0bec2a~tplv-k3u1fbpfcp-watermark.image?)


最上面的这些日志是最重要的，它包含了发生crash的线程是哪个，发的日志调用帧是哪个，到这里基本能很大程度上帮助我们实现问题的定位了，也就是基于bugreport + tombstone。


## 问题分析


线上ARM64升级的Crash只发生在Android10的系统上，具体到我们这个BUG，最终归宿到

	arm64/base.odex (BakerReadBarrierThunkAcquire_r15_r0_2)   
	
	Cause: null pointer dereference

但是上述的问题看起来跟如下帧似乎没有任何关系

	arm64/base.odex (com.netease.mail.profiler.handler.BaseHandler.stopTrace+360)

Java层的代码，怎么忽然就跑到arm64/base.odex (BakerReadBarrierThunk中去了呢？不防分析一下完整的调用帧：

	
	backtrace:
	      #00 pc 00000000008ee260  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (BakerReadBarrierThunkAcquire_r15_r0_2)
	      #01 pc 00000000009c9808  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (com.netease.mail.profiler.handler.BaseHandler.stopTrace+360)
	      #02 pc 00000000009b3cc4  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (com.netease.mail.profiler.handler.TailHandler$1.operationComplete+212)
	      #03 pc 00000000009b3b8c  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (com.netease.mail.android.wzp.util.Util$1.operationComplete [DEDUPED]+108)
	      #04 pc 0000000000b93180  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.util.concurrent.DefaultPromise.notifyListener0+80)
	      #05 pc 0000000000b9370c  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.util.concurrent.DefaultPromise.notifyListeners+988)
	      #06 pc 0000000000b94e3c  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.util.concurrent.DefaultPromise.trySuccess+92)
	      #07 pc 0000000000ba499c  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.DefaultChannelPromise.trySuccess+44)
	      #08 pc 0000000000b90ef4  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.nio.AbstractNioChannel$AbstractNioUnsafe.fulfillConnectPromise+84)
	      #09 pc 0000000000b91850  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.nio.AbstractNioChannel$AbstractNioUnsafe.finishConnect+192)
	      #10 pc 0000000000bb390c  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.nio.NioEventLoop.processSelectedKey+444)
	      #11 pc 0000000000bb3bf8  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.nio.NioEventLoop.processSelectedKeysOptimized+312)
	      #12 pc 0000000000bb55b8  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.nio.NioEventLoop.run+824)
	      #13 pc 0000000000ae1580  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.util.concurrent.SingleThreadEventExecutor$2.run+128)
	      #14 pc 0000000000adf068  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.util.concurrent.DefaultThreadFactory$DefaultRunnableDecorator.run+72)
	      #15 pc 00000000004afbb8  /system/framework/arm64/boot.oat (java.lang.Thread.run+72) (BuildId: 65cd48ea51183eb3b4cdfeb64ca2b90a9de89ffe)
	      #16 pc 0000000000137334  /apex/com.android.runtime/lib64/libart.so (art_quick_invoke_stub+548) (BuildId: fc24b8afa1bd5f1872cc1a38bcfa1cdc)
	      #17 pc 0000000000145fec  /apex/com.android.runtime/lib64/libart.so (art::ArtMethod::Invoke(art::Thread*, unsigned int*, unsigned int, art::JValue*, char const*)+244) (BuildId: fc24b8afa1bd5f1872cc1a38bcfa1cdc)
	      #18 pc 00000000004b0d98  /apex/com.android.runtime/lib64/libart.so (art::(anonymous namespace)::InvokeWithArgArray(art::ScopedObjectAccessAlreadyRunnable const&, art::ArtMethod*, art::(anonymous namespace)::ArgArray*, art::JValue*, char const*)+104) (BuildId: fc24b8afa1bd5f1872cc1a38bcfa1cdc)
	      #19 pc 00000000004b1eac  /apex/com.android.runtime/lib64/libart.so (art::InvokeVirtualOrInterfaceWithJValues(art::ScopedObjectAccessAlreadyRunnable const&, _jobject*, _jmethodID*, jvalue const*)+416) (BuildId: fc24b8afa1bd5f1872cc1a38bcfa1cdc)
	      #20 pc 00000000004f2868  /apex/com.android.runtime/lib64/libart.so (art::Thread::CreateCallback(void*)+1176) (BuildId: fc24b8afa1bd5f1872cc1a38bcfa1cdc)
	      #21 pc 00000000000e69e0  /apex/com.android.runtime/lib64/bionic/libc.so (__pthread_start(void*)+36) (BuildId: 1eb18e444251dc07dff5ebd93fce105c)
	      #22 pc 0000000000084b6c  /apex/com.android.runtime/lib64/bionic/libc.so (__start_thread+64) (BuildId: 1eb18e444251dc07dff5ebd93fce105c)
      

从#22帧开始看出这个是一个ART解释执行的过程，Android中基本所有线程栈都是这种形式，那么最终就可以认为是解释BaseHandler.stopTrace这句的时候，出现了null pointer dereference这样一个异常，为甚会这样呢？由于在系统上有共性：只有Android10系统的ARM64设备上出现，所以有理由怀疑Android10的源码在BakerReadBarrierThunkAcquire_r15_r0_2这里的处理上有什么不对劲,通过检索akerReadBarrierThunkAcquire_r15_r0_2字符串，发现code_generator_arm64.cc源码CompileBakerReadBarrierThunk函数最终输出了这段日志：

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/5c45bb0730514d9b90930320f1961678~tplv-k3u1fbpfcp-watermark.image?)

对比Android10与Android 11源码发现有一处很明确的不同，在Field Load使用之前，多加了一个空检查的Case:

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/6e07a18fd39046e7ad99ce558ee7ab81~tplv-k3u1fbpfcp-watermark.image?)

解释执行代码实在是看不懂：摘录了下这条记录的log Fix null checks on volatile reference field loads on ARM64.如下：

	Fix null checks on volatile reference field loads on ARM64.
	
	ART's compiler adds a null check HIR instruction before each field
	load HIR instruction created in the instruction builder phase. When
	implicit null checks are allowed, the compiler elides the null check
	if it can be turned into an implicit one (i.e. if the offset is within
	a system page range).
	
	On ARM64, the Baker read barrier thunk built for field reference loads
	needs to check the lock word of the holder of the field, and thus
	includes an explicit null check if no null check has been done before.
	However, this was not done for volatile loads (implemented with a
	load-acquire instruction on ARM64). This change adds this missing null
	check.
	
意思就是：对于volatile修饰的变量（映射为load-acquire instruction），加上空检查，避免运行时空指针。Android 10没有做这个空检查，该commit就是为修复该BUG，回到业务中发现，确实有地方用了多线程及volatile，处理掉这段逻辑即可。

## 总结
 
 最主要的是结合bugreport及tombstone文件做好定位，定位问题后，才方便解决。
 
#### 参考文档 	   
 
 https://wufengxue.github.io/2020/06/22/wechat-voice-codec-SEGV_MAPERR.html  有效参考分析工具 

 https://developer.android.com/ndk/guides/ndk-stack
