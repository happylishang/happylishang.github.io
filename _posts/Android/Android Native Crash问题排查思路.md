## 背景

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
	09-14 10:14:36.608  5944  5944 F DEBUG   : pid: 5687, tid: 5908, name: nioEventLoopGro  >>> com.netease.yanxuan <<<
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

对于Crash而言，精确的定位等于成功的一半，如何通过工具定位到native crash呢，如果是自己实现的so库，并且是native 发生crash一般而言，还是会有相应的日志打印出来的，本文主要针对一些特殊的so，尤其是不存在源码的so，对于这种场景如何定位，最重要当然还是复现：匹配对应的机型、环境、不断重试复现线上问题，一旦发生Crash后就些蛛丝马迹可查，本文以线上偶发的一个ARM64升级为例子，分析下定位流程：通过大量重试，复现场景后，便可以去查找问题日志，这个时候有一个挺好用的方法：bugreport命令：

	$ adb bugreport  ~\  

该命令会导出最近的crash相关信息，











 [ 一个关于Android支持64位CPU架构升级的“锅” ](https://www.jianshu.com/p/841c18c6e18d)


## Android10 ARM64 BUG

 
       // If base_reg differs from holder_reg, the offset was too large and we must have emitted
      // an explicit null check before the load. Otherwise, for implicit null checks, we need to
      // null-check the holder as we do not necessarily do that check before going to the thunk.
      vixl::aarch64::Label throw_npe_label;
      vixl::aarch64::Label* throw_npe = nullptr;
      if (GetCompilerOptions().GetImplicitNullChecks() && holder_reg.Is(base_reg)) {
        throw_npe = &throw_npe_label;
        __ Cbz(holder_reg.W(), throw_npe);
      }
      
## Android11 ARM64  BUG修复

      // In the case of a field load (with relaxed semantic), if `base_reg` differs from
      // `holder_reg`, the offset was too large and we must have emitted (during the construction
      // of the HIR graph, see `art::HInstructionBuilder::BuildInstanceFieldAccess`) and preserved
      // (see `art::PrepareForRegisterAllocation::VisitNullCheck`) an explicit null check before
      // the load. Otherwise, for implicit null checks, we need to null-check the holder as we do
      // not necessarily do that check before going to the thunk.
      //
      // In the case of a field load with load-acquire semantics (where `base_reg` always differs
      // from `holder_reg`), we also need an explicit null check when implicit null checks are
      // allowed, as we do not emit one before going to the thunk.
      vixl::aarch64::Label throw_npe_label;
      vixl::aarch64::Label* throw_npe = nullptr;
      if (GetCompilerOptions().GetImplicitNullChecks() &&
          (holder_reg.Is(base_reg) || (kind == BakerReadBarrierKind::kAcquire))) {
        throw_npe = &throw_npe_label;
        __ Cbz(holder_reg.W(), throw_npe);
      }
      // Check if the holder is gray and, if not, add fake dependency to the base register
      // and return to the LDR instruction to load the reference. Otherwise, use introspection
      // to load the reference and call the entrypoint that performs further checks on the
      // reference and marks it if needed.
      
     
     
![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8946f14bcef2420baf5f8a4f10b12ca0~tplv-k3u1fbpfcp-watermark.image?)

 
 数据
 
 ![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7568520088914dd281fa993eb1dfb2bd~tplv-k3u1fbpfcp-watermark.image?)
 
## lib.so问题排查：

https://gerrit.aospa.co/c/AOSPA/android_art/+/9174

null point check 

找对应设备，看看场景付下，找日志时间，搜日志找眉目

	-14 10:14:38.034  2875  3534 W TelephonyPermissions: reportAccessDeniedToReadIdentifiers:com.netease.yanxuan:getDeviceId:isPreinstalled=false:isPrivApp=false
	09-14 10:14:38.036  6082  6082 W System.err: java.io.FileNotFoundException: /system/build.prop: open failed: EACCES (Permission denied)
	09-14 10:14:38.036  6082  6082 W System.err: 	at libcore.io.IoBridge.open(IoBridge.java:496)
	09-14 10:14:38.036  6082  6082 W System.err: 	at java.io.FileInputStream.<init>(FileInputStream.java:159)
	09-14 10:14:38.036  6082  6082 W System.err: 	at com.netease.yanxuan.push.helper.c.isMiui(SourceFile:106)
	09-14 10:14:38.036  6082  6082 W System.err: 	at com.netease.yanxuan.push.thirdpart.miui.a.isEnabled(SourceFile:63)
	09-14 10:14:38.036  6082  6082 W System.err: 	at com.netease.yanxuan.push.thirdpart.c.a(SourceFile:57)
	09-14 10:14:38.036  6082  6082 W System.err: 	at com.netease.yanxuan.push.thirdpart.c.p(SourceFile:73)
	09-14 10:14:38.036  6082  6082 W System.err: 	at com.netease.yanxuan.application.h.l(SourceFile:436)
	09-14 10:14:38.036  6082  6082 W System.err: 	at com.netease.yanxuan.application.h.b(SourceFile:211)
	09-14 10:14:38.036  6082  6082 W System.err: 	at com.netease.yanxuan.application.h.a(SourceFile:168)
	09-14 10:14:38.036  6082  6082 W System.err: 	at com.netease.yanxuan.application.YXApplicationLike.onCreate(SourceFile:52)
	09-14 10:14:38.036  6082  6082 W System.err: 	at com.tencent.tinker.entry.TinkerApplicationInlineFence.handleMessageImpl(SourceFile:76)
	09-14 10:14:38.037  6082  6082 W System.err: 	at com.tencent.tinker.entry.TinkerApplicationInlineFence.handleMessage_$noinline$(SourceFile:60)
	09-14 10:14:38.037  6082  6082 W System.err: 	at com.tencent.tinker.entry.TinkerApplicationInlineFence.handleMessage(SourceFile:53)
	09-14 10:14:38.037  6082  6082 W System.err: 	at com.tencent.tinker.loader.app.TinkerInlineFenceAction.callOnCreate(SourceFile:57)
	09-14 10:14:38.037  6082  6082 W System.err: 	at com.tencent.tinker.loader.app.TinkerApplication.onCreate(SourceFile:189)
	09-14 10:14:38.037  6082  6082 W System.err: 	at com.netease.yanxuan.application.YXApplication.onCreate(SourceFile:30)
	09-14 10:14:38.037  6082  6082 W System.err: 	at android.app.Instrumentation.callApplicationOnCreate(Instrumentation.java:1190)
	09-14 10:14:38.037  6082  6082 W System.err: 	at android.app.ActivityThread.handleBindApplication(ActivityThread.java:6604)
	09-14 10:14:38.037  6082  6082 W System.err: 	at android.app.ActivityThread.access$1400(ActivityThread.java:227)
	09-14 10:14:38.037  6082  6082 W System.err: 	at android.app.ActivityThread$H.handleMessage(ActivityThread.java:1890)
	09-14 10:14:38.037  6082  6082 W System.err: 	at android.os.Handler.dispatchMessage(Handler.java:107)
	09-14 10:14:38.037  6082  6082 W System.err: 	at android.os.Looper.loop(Looper.java:224)
	09-14 10:14:38.037  6082  6082 W System.err: 	at android.app.ActivityThread.main(ActivityThread.java:7584)
	09-14 10:14:38.037  6082  6082 W System.err: 	at java.lang.reflect.Method.invoke(Native Method)
	09-14 10:14:38.037  6082  6082 W System.err: 	at com.android.internal.os.RuntimeInit$MethodAndArgsCaller.run(RuntimeInit.java:539)
	09-14 10:14:38.037  6082  6082 W System.err: 	at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:950)
	09-14 10:14:38.037  6082  6082 W System.err: Caused by: android.system.ErrnoException: open failed: EACCES (Permission denied)
	09-14 10:14:38.037  6082  6082 W System.err: 	at libcore.io.Linux.open(Native Method)
	09-14 10:14:38.037  6082  6082 W System.err: 	at libcore.io.ForwardingOs.open(ForwardingOs.java:167)
	09-14 10:14:38.037  6082  6082 W System.err: 	at libcore.io.BlockGuardOs.open(BlockGuardOs.java:252)
	09-14 10:14:38.037  6082  6082 W System.err: 	at libcore.io.ForwardingOs.open(ForwardingOs.java:167)
	09-14 10:14:38.037  6082  6082 W System.err: 	at android.app.ActivityThread$AndroidOs.open(ActivityThread.java:7483)
	09-14 10:14:38.037  6082  6082 W System.err: 	at libcore.io.IoBridge.open(IoBridge.java:482)
	09-14 10:14:38.037  6082  6082 W System.err: 	... 25 more
	
	
	
### 可疑点2：

	09-14 10:14:36.710  5687  5968 I NetworkUtil: network is available.
	09-14 10:14:36.711  5961  5961 E e.yanxuan:cach: Not starting debugger since process cannot load the jdwp agent.
	09-14 10:14:36.711  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.715  5687  5780 W AddressConnectTracer: [Thread-17] New channel is created for address: AddressUnit{cacheId=':12:137', list=[/59.111.182.48:9801], address=null}
	09-14 10:14:36.715  5944  5944 F DEBUG   : 
	09-14 10:14:36.715  5944  5944 F DEBUG   : backtrace:
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #00 pc 00000000008ee260  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (BakerReadBarrierThunkAcquire_r15_r0_2)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #01 pc 00000000009c9808  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (com.netease.mail.profiler.handler.BaseHandler.stopTrace+360)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #02 pc 00000000009b3cc4  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (com.netease.mail.profiler.handler.TailHandler$1.operationComplete+212)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #03 pc 00000000009b3b8c  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (com.netease.mail.android.wzp.util.Util$1.operationComplete [DEDUPED]+108)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #04 pc 0000000000b93180  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.util.concurrent.DefaultPromise.notifyListener0+80)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #05 pc 0000000000b9370c  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.util.concurrent.DefaultPromise.notifyListeners+988)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #06 pc 0000000000b94e3c  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.util.concurrent.DefaultPromise.trySuccess+92)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #07 pc 0000000000ba499c  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.DefaultChannelPromise.trySuccess+44)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #08 pc 0000000000b90ef4  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.nio.AbstractNioChannel$AbstractNioUnsafe.fulfillConnectPromise+84)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #09 pc 0000000000b91850  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.nio.AbstractNioChannel$AbstractNioUnsafe.finishConnect+192)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #10 pc 0000000000bb390c  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.nio.NioEventLoop.processSelectedKey+444)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #11 pc 0000000000bb3bf8  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.nio.NioEventLoop.processSelectedKeysOptimized+312)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #12 pc 0000000000bb55b8  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.nio.NioEventLoop.run+824)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #13 pc 0000000000ae1580  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.util.concurrent.SingleThreadEventExecutor$2.run+128)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #14 pc 0000000000adf068  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.util.concurrent.DefaultThreadFactory$DefaultRunnableDecorator.run+72)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #15 pc 00000000004afbb8  /system/framework/arm64/boot.oat (java.lang.Thread.run+72) (BuildId: 65cd48ea51183eb3b4cdfeb64ca2b90a9de89ffe)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #16 pc 0000000000137334  /apex/com.android.runtime/lib64/libart.so (art_quick_invoke_stub+548) (BuildId: fc24b8afa1bd5f1872cc1a38bcfa1cdc)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #17 pc 0000000000145fec  /apex/com.android.runtime/lib64/libart.so (art::ArtMethod::Invoke(art::Thread*, unsigned int*, unsigned int, art::JValue*, char const*)+244) (BuildId: fc24b8afa1bd5f1872cc1a38bcfa1cdc)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #18 pc 00000000004b0d98  /apex/com.android.runtime/lib64/libart.so (art::(anonymous namespace)::InvokeWithArgArray(art::ScopedObjectAccessAlreadyRunnable const&, art::ArtMethod*, art::(anonymous namespace)::ArgArray*, art::JValue*, char const*)+104) (BuildId: fc24b8afa1bd5f1872cc1a38bcfa1cdc)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #19 pc 00000000004b1eac  /apex/com.android.runtime/lib64/libart.so (art::InvokeVirtualOrInterfaceWithJValues(art::ScopedObjectAccessAlreadyRunnable const&, _jobject*, _jmethodID*, jvalue const*)+416) (BuildId: fc24b8afa1bd5f1872cc1a38bcfa1cdc)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #20 pc 00000000004f2868  /apex/com.android.runtime/lib64/libart.so (art::Thread::CreateCallback(void*)+1176) (BuildId: fc24b8afa1bd5f1872cc1a38bcfa1cdc)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #21 pc 00000000000e69e0  /apex/com.android.runtime/lib64/bionic/libc.so (__pthread_start(void*)+36) (BuildId: 1eb18e444251dc07dff5ebd93fce105c)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #22 pc 0000000000084b6c  /apex/com.android.runtime/lib64/bionic/libc.so (__start_thread+64) (BuildId: 1eb18e444251dc07dff5ebd93fce105c)
	09-14 10:14:36.716  5687  5780 V WZP     : [Thread-17] Start init channel
	09-14 10:14:36.717  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-1] Receive EncryptHandshakeRequestEvent
	09-14 10:14:36.717  5687  5780 V WZP     : [Thread-17] Init channel finish



### 可疑点3：

	543  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.544  5687  5931 E MicroMsg.SDK.WXApiImplV10: register app failed for wechat app signature check failed
	09-14 10:14:36.589  5944  5944 I crash_dump64: obtaining output fd from tombstoned, type: kDebuggerdTombstone
	09-14 10:14:36.589  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.590  1361  1361 I /system/bin/tombstoned: received crash request for pid 5908
	09-14 10:14:36.591  5944  5944 I crash_dump64: performing dump of process 5687 (target tid = 5908)
	09-14 10:14:36.607  5944  5944 F DEBUG   : *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***
	09-14 10:14:36.608  5944  5944 F DEBUG   : Build fingerprint: 'Xiaomi/vangogh/vangogh:10/QKQ1.191222.002/V12.0.6.0.QJVCNXM:user/release-keys'
	09-14 10:14:36.608  5944  5944 F DEBUG   : Revision: '0'
	09-14 10:14:36.608  5944  5944 F DEBUG   : ABI: 'arm64'
	09-14 10:14:36.608  5944  5944 F DEBUG   : Timestamp: 2021-09-14 10:14:36+0800
	09-14 10:14:36.608  5944  5944 F DEBUG   : pid: 5687, tid: 5908, name: nioEventLoopGro  >>> com.netease.yanxuan <<<
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
	09-14 10:14:36.611  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.612  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.628  5687  5784 W netease.yanxua: Long monitor contention with owner Thread-10 (5783) at void java.lang.System.arraycopy(java.lang.Object, int, java.lang.Object, int, int)(System.java:-2) waiters=0 in void com.netease.volley.toolbox.DiskBasedCache.initialize() for 978ms
	09-14 10:14:36.630  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.632  5687  5785 D YanXuan : wzp request begin...
	09-14 10:14:36.632  5687  5786 D YanXuan : wzp request begin...
	09-14 10:14:36.632  5687  5788 D YanXuan : wzp request begin...
	09-14 10:14:36.633  5687  5780 V WZP     : [Thread-13] Start to find connection for appId:12, serviceId:137, user:, timeout:5000, security: true
	09-14 10:14:36.633  5687  5790 D YanXuan : wzp request begin...
	09-14 10:14:36.633  5687  5780 V Locate  : [Thread-13] Start locate for appId:12, serviceId:137, user:, timeout:5000
	09
	
## 第四部分可疑	 

* https://github.com/facebook/SoLoader/issues/13 
* 
* https://github.com/facebook/SoLoader
	
		t', timeSpent=0}, Record{time=1631585675974, info='channelRead finish', timeSpent=0}, Record{time=1631585675974, info='stopTrace', timeSpent=0}], locate='', appId=12}
	09-14 10:14:35.978  5687  5780 V RecordStore: [wzp-profiler-save-3-1] Ignored by filter. 5280fb7386ac6c0171b4b13732b3f4f2
	09-14 10:14:35.982  5687  5780 V RecordStore: [wzp-profiler-save-3-1] Ignored by filter. 5280fb7386ac6c0171b4b13732b3f4f2
	09-14 10:14:35.982  5687  5780 V DefaultProfiler: [wzp-profiler-save-3-1] Handled by instantStore, traceId 5280fb7386ac6c0171b4b13732b3f4f2
	09-14 10:14:35.985   649  5879 E ResolverController: No valid NAT64 prefix (116, <unspecified>/0)
	09-14 10:14:35.987  5831  5831 D SoLoader: init start
	09-14 10:14:35.987  5831  5831 D SoLoader: adding system library source: /vendor/lib
	09-14 10:14:35.987  5831  5831 D SoLoader: adding system library source: /system/lib
	09-14 10:14:35.987  5831  5831 D SoLoader: adding application source: com.facebook.soloader.DirectorySoSource[root = /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/lib/arm64 flags = 0]
	09-14 10:14:35.987  5831  5831 D SoLoader: adding backup  source: com.facebook.soloader.ApkSoSource[root = /data/data/com.netease.yanxuan/lib-main flags = 1]
	09-14 10:14:35.987  5831  5831 D SoLoader: Preparing SO source: com.facebook.soloader.DirectorySoSource[root = /system/lib flags = 2]
	09-14 10:14:35.987  5831  5831 D SoLoader: Preparing SO source: com.facebook.soloader.DirectorySoSource[root = /vendor/lib flags = 2]
	09-14 10:14:35.987  5831  5831 D SoLoader: Preparing SO source: com.facebook.soloader.DirectorySoSource[root = /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/lib/arm64 flags = 0]
	09-14 10:14:35.987  5831  5831 D SoLoader: Preparing SO source: com.facebook.soloader.ApkSoSource[root = /data/data/com.netease.yanxuan/lib-main flags = 1]
	09-14 10:14:35.987  5831  5831 V fb-UnpackingSoSource: locked dso store /data/user/0/com.netease.yanxuan/lib-main
	09-14 10:14:35.988  5831  5831 I fb-UnpackingSoSource: dso store is up-to-date: /data/user/0/com.netease.yanxuan/lib-
	
	
	
	
	
	
	
	
	
	
	
	
# 全量日志

	09-14 10:14:36.156  5831  5880 W FA      : Failed to retrieve Firebase Instance Id
	09-14 10:14:36.159  1862  2093 I ActivityTaskManager: Displayed com.netease.yanxuan/.module.mainpage.activity.MainPageActivity: +886ms
	09-14 10:14:36.159  1862  1919 I ActivityTaskManager: The Process com.netease.yanxuan Already Exists in BG. So sending its PID: 5687
	09-14 10:14:36.160  1862  1919 D CompatibilityInfo: mCompatibilityFlags - 0
	09-14 10:14:36.160  1862  1919 D CompatibilityInfo: applicationDensity - 440
	09-14 10:14:36.160  1862  1919 D CompatibilityInfo: applicationScale - 1.0
	09-14 10:14:36.162  5831  5880 I FA      : Tag Manager is not found and thus will not be used
	09-14 10:14:36.166  5831  5880 W GooglePlayServicesUtil: Google Play services is missing.
	09-14 10:14:36.167  1862  1919 I Timeline: Timeline: App_transition_ready time:55002839
	09-14 10:14:36.167  2512  2512 D EventBus: [2512, u0] send(AppTransitionFinishedEvent)
	09-14 10:14:36.168  2512  2512 D EventBus: [2512, u0]  -> ForcedResizableInfoActivityController [0xd91f659, P1] onBusEvent(AppTransitionFinishedEvent)
	09-14 10:14:36.168  2512  2512 D EventBus: [2512, u0] onBusEvent(AppTransitionFinishedEvent) duration: 31 microseconds, avg: 79
	09-14 10:14:36.169  5687  5829 D b       : [Thread:6791] receive an intent from server, action=com.xiaomi.mipush.RECEIVE_MESSAGE
	09-14 10:14:36.172  5687  5687 W netease.yanxua: Accessing hidden field Landroid/view/Choreographer;->mLock:Ljava/lang/Object; (greylist-max-p, reflection, denied)
	09-14 10:14:36.172  5687  5687 W System.err: java.lang.NoSuchFieldException: No field mLock in class Landroid/view/Choreographer; (declaration of 'android.view.Choreographer' appears in /system/framework/framework.jar!classes3.dex)
	09-14 10:14:36.172  5687  5687 W System.err: 	at java.lang.Class.getDeclaredField(Native Method)
	09-14 10:14:36.172  5687  5687 W System.err: 	at com.snail.collie.b.d.j(SourceFile:224)
	09-14 10:14:36.172  5687  5687 W System.err: 	at com.snail.collie.b.d.ahQ(SourceFile:278)
	09-14 10:14:36.172  5687  5687 W System.err: 	at com.snail.collie.b.d.a(SourceFile:25)
	09-14 10:14:36.172  5687  5687 W System.err: 	at com.snail.collie.b.d$1$1.onWindowFocusChanged(SourceFile:94)
	09-14 10:14:36.172  5687  5687 W System.err: 	at android.view.ViewTreeObserver.dispatchOnWindowFocusChange(ViewTreeObserver.java:1018)
	09-14 10:14:36.172  5687  5687 W System.err: 	at android.view.ViewRootImpl.handleWindowFocusChanged(ViewRootImpl.java:2980)
	09-14 10:14:36.172  5687  5687 W System.err: 	at android.view.ViewRootImpl.access$1100(ViewRootImpl.java:151)
	09-14 10:14:36.172  5687  5687 W System.err: 	at android.view.ViewRootImpl$ViewRootHandler.handleMessage(ViewRootImpl.java:4674)
	09-14 10:14:36.172  5687  5687 W System.err: 	at android.os.Handler.dispatchMessage(Handler.java:107)
	09-14 10:14:36.172  5687  5687 W System.err: 	at android.os.Looper.loop(Looper.java:224)
	09-14 10:14:36.172  5687  5687 W System.err: 	at android.app.ActivityThread.main(ActivityThread.java:7584)
	09-14 10:14:36.172  5687  5687 W System.err: 	at java.lang.reflect.Method.invoke(Native Method)
	09-14 10:14:36.172  5687  5687 W System.err: 	at com.android.internal.os.RuntimeInit$MethodAndArgsCaller.run(RuntimeInit.java:539)
	09-14 10:14:36.172  5687  5687 W System.err: 	at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:950)
	09-14 10:14:36.174  5687  5829 D b       : [Thread:6791] processing a message, action=Command
	09-14 10:14:36.175  5687  5687 W ActivityThread: handleWindowVisibility: no activity for token android.os.BinderProxy@c7953f9
	09-14 10:14:36.176  5687  5829 D b       : [Thread:6791] begin execute onCommandResult, command=accept-time, resultCode=0, reason=Accept time unchanged.
	09-14 10:14:36.182  5831  5880 W FA      : Failed to retrieve Firebase Instance Id
	09-14 10:14:36.188  5687  5687 W EventBus: Please donot register with same enum priority
	09-14 10:14:36.188  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 13 lines
	09-14 10:14:36.188  5687  5687 W EventBus: Please donot register with same enum priority
	09-14 10:14:36.197  1862  5737 W DevicePolicyManager: Package com.netease.yanxuan (uid=10312, pid=5687) cannot access Device IDs
	09-14 10:14:36.197  2875  3534 W TelephonyPermissions: reportAccessDeniedToReadIdentifiers:com.netease.yanxuan:getDeviceId:isPreinstalled=false:isPrivApp=false
	09-14 10:14:36.204  1862  5737 W DevicePolicyManager: Package com.netease.yanxuan (uid=10312, pid=5687) cannot access Device IDs
	09-14 10:14:36.205  2875  3534 W TelephonyPermissions: reportAccessDeniedToReadIdentifiers:com.netease.yanxuan:getDeviceId:isPreinstalled=false:isPrivApp=false
	09-14 10:14:36.205  5687  5931 W System.err: java.lang.NoSuchMethodException: com.android.internal.telephony.IPhoneSubInfo$Stub$Proxy.getDeviceId []
	09-14 10:14:36.205  5687  5931 W System.err: 	at java.lang.Class.getMethod(Class.java:2072)
	09-14 10:14:36.205  5687  5931 W System.err: 	at java.lang.Class.getDeclaredMethod(Class.java:2050)
	09-14 10:14:36.205  5687  5931 W System.err: 	at com.netease.deviceid.g.aU(SourceFile:74)
	09-14 10:14:36.205  5687  5931 W System.err: 	at com.netease.deviceid.c.getDeviceId(SourceFile:27)
	09-14 10:14:36.205  5687  5931 W System.err: 	at com.netease.deviceid.a.getDeviceId(SourceFile:39)
	09-14 10:14:36.205  5687  5931 W System.err: 	at com.netease.yanxuan.common.util.h.pp(SourceFile:397)
	09-14 10:14:36.205  5687  5931 W System.err: 	at com.netease.yanxuan.module.trustid.a.<init>(SourceFile:38)
	09-14 10:14:36.205  5687  5931 W System.err: 	at com.netease.yanxuan.module.trustid.b$4.run(SourceFile:149)
	09-14 10:14:36.205  5687  5931 W System.err: 	at android.os.Handler.handleCallback(Handler.java:883)
	09-14 10:14:36.205  5687  5931 W System.err: 	at android.os.Handler.dispatchMessage(Handler.java:100)
	09-14 10:14:36.205  5687  5931 W System.err: 	at android.os.Looper.loop(Looper.java:224)
	09-14 10:14:36.205  5687  5931 W System.err: 	at android.os.HandlerThread.run(HandlerThread.java:67)
	09-14 10:14:36.208  1862  5737 W DevicePolicyManager: Package com.netease.yanxuan (uid=10312, pid=5687) cannot access Device IDs
	09-14 10:14:36.209  2875  3534 W TelephonyPermissions: reportAccessDeniedToReadIdentifiers:com.netease.yanxuan:getDeviceId:isPreinstalled=false:isPrivApp=false
	09-14 10:14:36.210  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.210  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.211  5687  5931 W netease.yanxua: Accessing hidden method Landroid/telephony/TelephonyManager;->getITelephony()Lcom/android/internal/telephony/ITelephony; (greylist-max-p, reflection, denied)
	09-14 10:14:36.212  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.219  1862  5737 W DevicePolicyManager: Package com.netease.yanxuan (uid=10312, pid=5687) cannot access Device IDs
	09-14 10:14:36.219  2875  3534 W TelephonyPermissions: reportAccessDeniedToReadIdentifiers:com.netease.yanxuan:getDeviceId:isPreinstalled=false:isPrivApp=false
	09-14 10:14:36.227  1862  5737 D ActivityManager: report kill process: killerPid is:5831, killedPid is:5831
	09-14 10:14:36.230  2512  2512 D NavBarTintController: onSampleCollected 1.0
	09-14 10:14:36.228  5831  5889 I Process : Sending signal. PID: 5831 SIG: 9
	09-14 10:14:36.272  1862  3249 I ActivityManager: Process com.netease.yanxuan:hotfix (pid 5831) has died: vis BTOP
	09-14 10:14:36.273   650   650 I Zygote  : Process 5831 exited due to signal 9 (Killed)
	09-14 10:14:36.273   794  2706 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: killProcess is called for pid : 5831
	09-14 10:14:36.273  1862  3249 I AutoStartManagerService: MIUILOG- Reject RestartService packageName :com.netease.yanxuan uid : 10312
	09-14 10:14:36.273   794  2706 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: process with pid 5831 is service
	09-14 10:14:36.273   794  2706 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: size of client connections for client: systemafter removal is 19
	09-14 10:14:36.274   794  2706 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: process with pid 5831 is service
	09-14 10:14:36.274   794  2706 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: size of client connections for client: com.netease.yanxuanafter removal is 1
	09-14 10:14:36.274   794  2706 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: process with pid 5831 is client
	09-14 10:14:36.274   794  2706 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: size of service connections for service: com.netease.yanxuan/com.google.android.gms.measurement.AppMeasurementServiceafter removal is 1
	09-14 10:14:36.274   794  2706 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: removing client with pid 5831process namecom.netease.yanxuan:hotfix
	09-14 10:14:36.274   794  2706 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: unbindService is called for service : com.netease.yanxuan/.common.util.tinker.TinkerDownloader and for client system
	09-14 10:14:36.274   794  2706 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: size of service connections for service: com.netease.yanxuan/.common.util.tinker.TinkerDownloaderafter removal is 0
	09-14 10:14:36.274   794  2706 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: destroyService is called for service : com.netease.yanxuan/.common.util.tinker.TinkerDownloader
	09-14 10:14:36.277  1862  2097 I libprocessgroup: Successfully killed process cgroup uid 10312 pid 5831 in 5ms
	09-14 10:14:36.333  2512  2512 D NavBarTintController: onSampleCollected 1.0
	09-14 10:14:36.434  2512  2512 D NavBarTintController: onSampleCollected 1.0
	09-14 10:14:36.445  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.478  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 42 lines
	09-14 10:14:36.479  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.479  5687  5746 W FA      : Callable skipped the worker queue.
	09-14 10:14:36.477  5687  5687 W nioEventLoopGro: type=1400 audit(0.0:52814): avc: denied { search } for name="battery" dev="sysfs" ino=67740 scontext=u:r:untrusted_app:s0:c56,c257,c512,c768 tcontext=u:object_r:sysfs_battery_supply:s0 tclass=dir permissive=0
	09-14 10:14:36.484  1862  3249 W DevicePolicyManager: Package com.netease.yanxuan (uid=10312, pid=5687) cannot access Device IDs
	09-14 10:14:36.484  1862  3249 W TelephonyPermissions: reportAccessDeniedToReadIdentifiers:com.netease.yanxuan:getSerial:isPreinstalled=false:isPrivApp=false
	09-14 10:14:36.489  2907  2907 D RecentsImpl: mActivityStateObserver com.netease.yanxuan.module.mainpage.activity.MainPageActivity
	09-14 10:14:36.273  1862  3249 I AutoStartManagerService: MIUILOG- Reject RestartService packageName :com.netease.yanxuan uid : 10312
	09-14 10:14:36.489  5687  5687 W Looper  : Slow Looper main: Activity com.netease.yanxuan/.module.mainpage.activity.MainPageActivity is 320ms late (wall=0ms running=0ms ClientTransaction{ callbacks=[android.app.servertransaction.TopResumedActivityChangeItem] }) because of 10 msg, msg 10 took 312ms (seq=52 late=8ms h=android.app.ActivityThread$H w=159)
	09-14 10:14:36.490  2907  3097 W GestureStubView: adaptRotation   currentRotation=0   mRotation=0
	09-14 10:14:36.490  2907  3097 D GestureStubView: resetRenderProperty: showGestureStub
	09-14 10:14:36.490  2907  3097 D GestureStubView: showGestureStub
	09-14 10:14:36.490  2907  3097 W GestureStubView: adaptRotation   currentRotation=0   mRotation=0
	09-14 10:14:36.490  2907  3097 D GestureStubView: resetRenderProperty: showGestureStub
	09-14 10:14:36.490  2907  3097 D GestureStubView: showGestureStub
	09-14 10:14:36.492  5687  5908 F libc    : Fatal signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0x4 in tid 5908 (nioEventLoopGro), pid 5687 (netease.yanxuan)
	09-14 10:14:36.499  2907  3097 D GestureStubView: gatherTransparentRegion: need render w:54  h:1440
	09-14 10:14:36.505  2907  3097 D GestureStubView: gatherTransparentRegion: need render w:54  h:1440
	09-14 10:14:36.511  1862  2409 I MiuiNetworkPolicy: updateUidState uid = 10061, uidState = 16
	09-14 10:14:36.511   649  2549 D OemNetd : whiteListUid: uid=10061, wmm=del
	09-14 10:14:36.512  5687  5687 W EventBus: Please donot register with same enum priority
	09-14 10:14:36.512  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 14 lines
	09-14 10:14:36.512  5687  5687 W EventBus: Please donot register with same enum priority
	09-14 10:14:36.512  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.527  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 23 lines
	09-14 10:14:36.529  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.535  2512  2512 D NavBarTintController: onSampleCollected 1.0
	09-14 10:14:36.536  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.536  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 1 line
	09-14 10:14:36.537  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.542  5687  5931 D MicroMsg.PaySdk.WXFactory: createWXAPI, appId = wx41ad0463c63edb69, checkSignature = true
	09-14 10:14:36.542  5687  5931 D MicroMsg.SDK.WXApiImplV10: <init>, appId = wx41ad0463c63edb69, checkSignature = true
	09-14 10:14:36.543  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.544  5687  5931 E MicroMsg.SDK.WXApiImplV10: register app failed for wechat app signature check failed
	09-14 10:14:36.589  5944  5944 I crash_dump64: obtaining output fd from tombstoned, type: kDebuggerdTombstone
	09-14 10:14:36.589  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.590  1361  1361 I /system/bin/tombstoned: received crash request for pid 5908
	09-14 10:14:36.591  5944  5944 I crash_dump64: performing dump of process 5687 (target tid = 5908)
	09-14 10:14:36.607  5944  5944 F DEBUG   : *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***
	09-14 10:14:36.608  5944  5944 F DEBUG   : Build fingerprint: 'Xiaomi/vangogh/vangogh:10/QKQ1.191222.002/V12.0.6.0.QJVCNXM:user/release-keys'
	09-14 10:14:36.608  5944  5944 F DEBUG   : Revision: '0'
	09-14 10:14:36.608  5944  5944 F DEBUG   : ABI: 'arm64'
	09-14 10:14:36.608  5944  5944 F DEBUG   : Timestamp: 2021-09-14 10:14:36+0800
	09-14 10:14:36.608  5944  5944 F DEBUG   : pid: 5687, tid: 5908, name: nioEventLoopGro  >>> com.netease.yanxuan <<<
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
	09-14 10:14:36.611  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.612  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.628  5687  5784 W netease.yanxua: Long monitor contention with owner Thread-10 (5783) at void java.lang.System.arraycopy(java.lang.Object, int, java.lang.Object, int, int)(System.java:-2) waiters=0 in void com.netease.volley.toolbox.DiskBasedCache.initialize() for 978ms
	09-14 10:14:36.630  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.632  5687  5785 D YanXuan : wzp request begin...
	09-14 10:14:36.632  5687  5786 D YanXuan : wzp request begin...
	09-14 10:14:36.632  5687  5788 D YanXuan : wzp request begin...
	09-14 10:14:36.633  5687  5780 V WZP     : [Thread-13] Start to find connection for appId:12, serviceId:137, user:, timeout:5000, security: true
	09-14 10:14:36.633  5687  5790 D YanXuan : wzp request begin...
	09-14 10:14:36.633  5687  5780 V Locate  : [Thread-13] Start locate for appId:12, serviceId:137, user:, timeout:5000
	09-14 10:14:36.633  5687  5780 V WZP     : [Thread-15] Start to find connection for appId:12, serviceId:137, user:, timeout:5000, security: true
	09-14 10:14:36.633  5687  5780 V Locate  : [Thread-13] Find locate result in cache
	09-14 10:14:36.633  5687  5780 V Locate  : [Thread-15] Start locate for appId:12, serviceId:137, user:, timeout:5000
	09-14 10:14:36.633  5687  5780 V Locate  : [Thread-13] Recover wzpAddress
	09-14 10:14:36.633  5687  5789 D YanXuan : wzp request begin...
	09-14 10:14:36.633  5687  5780 V Locate  : [Thread-15] Find locate result in cache
	09-14 10:14:36.634  5687  5780 V WZP     : [Thread-13] Start to create new connection
	09-14 10:14:36.634  5687  5780 W AddressConnectTracer: [Thread-13] Connect start [/59.111.182.48:9801]
	09-14 10:14:36.636  2512  2512 D NavBarTintController: onSampleCollected 1.0
	09-14 10:14:36.642  5687  5780 V WZP     : [Thread-17] Start to find connection for appId:12, serviceId:137, user:, timeout:5000, security: true
	09-14 10:14:36.643  5687  5780 V Locate  : [Thread-17] Start locate for appId:12, serviceId:137, user:, timeout:5000
	09-14 10:14:36.643  5687  5780 V Locate  : [Thread-17] Find locate result in cache
	09-14 10:14:36.643  5687  5780 V WZP     : [Thread-17] Start to create new connection
	09-14 10:14:36.643  5687  5780 W AddressConnectTracer: [Thread-17] Connect start [/59.111.182.48:9801]
	09-14 10:14:36.645  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.653  5687  5780 V WZP     : [Thread-16] Start to find connection for appId:12, serviceId:137, user:, timeout:5000, security: true
	09-14 10:14:36.653  5687  5780 V Locate  : [Thread-16] Start locate for appId:12, serviceId:137, user:, timeout:5000
	09-14 10:14:36.654  5687  5780 V Locate  : [Thread-16] Find locate result in cache
	09-14 10:14:36.654  5687  5780 V WZP     : [Thread-16] Start to create new connection
	09-14 10:14:36.654  5687  5780 W AddressConnectTracer: [Thread-16] Connect start [/59.111.182.48:9801]
	09-14 10:14:36.656   794 12432 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: bindService is called for service : com.netease.yanxuan/com.netease.deviceid.jni.EmulatorCheckService and for client com.netease.yanxuan
	09-14 10:14:36.656   794 12432 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: total connections for service : com.netease.yanxuan/com.netease.deviceid.jni.EmulatorCheckServiceare :1
	09-14 10:14:36.656   794 12432 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: total connections for client : com.netease.yanxuanare :2
	09-14 10:14:36.663  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.667  5687  5780 V WZP     : [Thread-15] Start to create new connection
	09-14 10:14:36.667  5687  5780 W AddressConnectTracer: [Thread-15] Connect start [/59.111.182.48:9801]
	09-14 10:14:36.675  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.676  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 2 lines
	09-14 10:14:36.676  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.677  5687  5780 V WZP     : [Thread-12] Start to find connection for appId:12, serviceId:137, user:, timeout:5000, security: true
	09-14 10:14:36.678  5687  5780 V Locate  : [Thread-12] Start locate for appId:12, serviceId:137, user:, timeout:5000
	09-14 10:14:36.678  5687  5780 V Locate  : [Thread-12] Find locate result in cache
	09-14 10:14:36.678  5687  5780 V WZP     : [Thread-12] Start to create new connection
	09-14 10:14:36.678  5687  5780 W AddressConnectTracer: [Thread-12] Connect start [/59.111.182.48:9801]
	09-14 10:14:36.683  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.685  5687  5780 W AddressConnectTracer: [Thread-13] Connect success /59.111.182.48:9801
	09-14 10:14:36.685   650   650 D Zygote  : Forked child process 5961
	09-14 10:14:36.687  5687  5780 W AddressConnectTracer: [Thread-13] Channel initialize start /59.111.182.48:9801
	09-14 10:14:36.687  1862  2096 D Boost   : hostingType=service, hostingName={com.netease.yanxuan/com.netease.deviceid.jni.EmulatorCheckService}, callerPackage=com.netease.yanxuan, isSystem=false, isBoostNeeded=false.
	09-14 10:14:36.688   649  2549 D OemNetd : setPidForPackage: packageName=com.netease.yanxuan, pid=5961, pid=10312
	09-14 10:14:36.688  1862  2096 I ActivityManager: Start proc 5961:com.netease.yanxuan:cache/u0a312 for service {com.netease.yanxuan/com.netease.deviceid.jni.EmulatorCheckService} caller=com.netease.yanxuan
	09-14 10:14:36.690  5687  5780 W AddressConnectTracer: [Thread-13] Channel initialize finish /59.111.182.48:9801
	09-14 10:14:36.692  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.693  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.694  5687  5780 W AddressConnectTracer: [Thread-17] Connect success /59.111.182.48:9801
	09-14 10:14:36.704  5687  5780 W AddressConnectTracer: [Thread-13] New channel is created for address: AddressUnit{cacheId=':12:137', list=[/59.111.182.48:9801], address=null}
	09-14 10:14:36.706  5687  5786 D YanXuan : wzp request end...
	09-14 10:14:36.706  5687  5780 V WZP     : [Thread-13] Start init channel
	09-14 10:14:36.706  5687  5780 W AddressConnectTracer: [Thread-17] Channel initialize start /59.111.182.48:9801
	09-14 10:14:36.708  5687  5780 W AddressConnectTracer: [Thread-17] Channel initialize finish /59.111.182.48:9801
	09-14 10:14:36.710  5687  5968 I NetworkUtil: network is available.
	09-14 10:14:36.711  5961  5961 E e.yanxuan:cach: Not starting debugger since process cannot load the jdwp agent.
	09-14 10:14:36.711  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.715  5687  5780 W AddressConnectTracer: [Thread-17] New channel is created for address: AddressUnit{cacheId=':12:137', list=[/59.111.182.48:9801], address=null}
	09-14 10:14:36.715  5944  5944 F DEBUG   : 
	09-14 10:14:36.715  5944  5944 F DEBUG   : backtrace:
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #00 pc 00000000008ee260  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (BakerReadBarrierThunkAcquire_r15_r0_2)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #01 pc 00000000009c9808  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (com.netease.mail.profiler.handler.BaseHandler.stopTrace+360)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #02 pc 00000000009b3cc4  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (com.netease.mail.profiler.handler.TailHandler$1.operationComplete+212)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #03 pc 00000000009b3b8c  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (com.netease.mail.android.wzp.util.Util$1.operationComplete [DEDUPED]+108)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #04 pc 0000000000b93180  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.util.concurrent.DefaultPromise.notifyListener0+80)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #05 pc 0000000000b9370c  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.util.concurrent.DefaultPromise.notifyListeners+988)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #06 pc 0000000000b94e3c  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.util.concurrent.DefaultPromise.trySuccess+92)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #07 pc 0000000000ba499c  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.DefaultChannelPromise.trySuccess+44)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #08 pc 0000000000b90ef4  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.nio.AbstractNioChannel$AbstractNioUnsafe.fulfillConnectPromise+84)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #09 pc 0000000000b91850  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.nio.AbstractNioChannel$AbstractNioUnsafe.finishConnect+192)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #10 pc 0000000000bb390c  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.nio.NioEventLoop.processSelectedKey+444)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #11 pc 0000000000bb3bf8  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.nio.NioEventLoop.processSelectedKeysOptimized+312)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #12 pc 0000000000bb55b8  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.channel.nio.NioEventLoop.run+824)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #13 pc 0000000000ae1580  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.util.concurrent.SingleThreadEventExecutor$2.run+128)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #14 pc 0000000000adf068  /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/oat/arm64/base.odex (io.netty.util.concurrent.DefaultThreadFactory$DefaultRunnableDecorator.run+72)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #15 pc 00000000004afbb8  /system/framework/arm64/boot.oat (java.lang.Thread.run+72) (BuildId: 65cd48ea51183eb3b4cdfeb64ca2b90a9de89ffe)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #16 pc 0000000000137334  /apex/com.android.runtime/lib64/libart.so (art_quick_invoke_stub+548) (BuildId: fc24b8afa1bd5f1872cc1a38bcfa1cdc)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #17 pc 0000000000145fec  /apex/com.android.runtime/lib64/libart.so (art::ArtMethod::Invoke(art::Thread*, unsigned int*, unsigned int, art::JValue*, char const*)+244) (BuildId: fc24b8afa1bd5f1872cc1a38bcfa1cdc)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #18 pc 00000000004b0d98  /apex/com.android.runtime/lib64/libart.so (art::(anonymous namespace)::InvokeWithArgArray(art::ScopedObjectAccessAlreadyRunnable const&, art::ArtMethod*, art::(anonymous namespace)::ArgArray*, art::JValue*, char const*)+104) (BuildId: fc24b8afa1bd5f1872cc1a38bcfa1cdc)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #19 pc 00000000004b1eac  /apex/com.android.runtime/lib64/libart.so (art::InvokeVirtualOrInterfaceWithJValues(art::ScopedObjectAccessAlreadyRunnable const&, _jobject*, _jmethodID*, jvalue const*)+416) (BuildId: fc24b8afa1bd5f1872cc1a38bcfa1cdc)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #20 pc 00000000004f2868  /apex/com.android.runtime/lib64/libart.so (art::Thread::CreateCallback(void*)+1176) (BuildId: fc24b8afa1bd5f1872cc1a38bcfa1cdc)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #21 pc 00000000000e69e0  /apex/com.android.runtime/lib64/bionic/libc.so (__pthread_start(void*)+36) (BuildId: 1eb18e444251dc07dff5ebd93fce105c)
	09-14 10:14:36.715  5944  5944 F DEBUG   :       #22 pc 0000000000084b6c  /apex/com.android.runtime/lib64/bionic/libc.so (__start_thread+64) (BuildId: 1eb18e444251dc07dff5ebd93fce105c)
	09-14 10:14:36.716  5687  5780 V WZP     : [Thread-17] Start init channel
	09-14 10:14:36.717  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-1] Receive EncryptHandshakeRequestEvent
	09-14 10:14:36.717  5687  5780 V WZP     : [Thread-17] Init channel finish
	09-14 10:14:36.717  5687  5780 V WZP     : [Thread-13] Init channel finish
	09-14 10:14:36.717  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-1] Write handshake unit
	09-14 10:14:36.717  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-1] Write finish
	09-14 10:14:36.717  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-2] Receive EncryptHandshakeRequestEvent
	09-14 10:14:36.717  5687  5780 W AddressConnectTracer: [Thread-12] Connect success /59.111.182.48:9801
	09-14 10:14:36.718  5687  5785 D YanXuan : wzp request end...
	09-14 10:14:36.718  5687  5788 D YanXuan : wzp request end...
	09-14 10:14:36.718  5687  5780 W AddressConnectTracer: [Thread-12] Channel initialize start /59.111.182.48:9801
	09-14 10:14:36.720  5687  5780 W AddressConnectTracer: [Thread-12] Channel initialize finish /59.111.182.48:9801
	09-14 10:14:36.721  5687  5780 W AddressConnectTracer: [Thread-12] New channel is created for address: AddressUnit{cacheId=':12:137', list=[/59.111.182.48:9801], address=null}
	09-14 10:14:36.724  5687  5780 V WZP     : [Thread-12] Start init channel
	09-14 10:14:36.724  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-1] Handshake result: continue
	09-14 10:14:36.724  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-2] Write handshake unit
	09-14 10:14:36.724  5687  5780 V WZP     : [Thread-12] Init channel finish
	09-14 10:14:36.724  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-2] Write finish
	09-14 10:14:36.724  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-2] Receive EncryptHandshakeRequestEvent
	09-14 10:14:36.724  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-2] Write handshake unit
	09-14 10:14:36.724  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-2] Write finish
	09-14 10:14:36.724  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-2] Handshake result: continue
	09-14 10:14:36.724  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-2] Handshake result: continue
	09-14 10:14:36.725  5687  5780 W AddressConnectTracer: [Thread-15] Connect success /59.111.182.48:9801
	09-14 10:14:36.727  5687  5780 W AddressConnectTracer: [Thread-15] Channel initialize start /59.111.182.48:9801
	09-14 10:14:36.729  5687  5780 W AddressConnectTracer: [Thread-15] Channel initialize finish 59.111.182.48/59.111.182.48:9801
	09-14 10:14:36.739  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.739  2512  2512 D NavBarTintController: onSampleCollected 1.0
	09-14 10:14:36.740  1862  5738 D CompatibilityInfo: mCompatibilityFlags - 0
	09-14 10:14:36.740  1862  5738 D CompatibilityInfo: applicationDensity - 440
	09-14 10:14:36.740  1862  5738 D CompatibilityInfo: applicationScale - 1.0
	09-14 10:14:36.743  5687  5780 V WZPChannel: [Thread-13] Start sending request #8
	09-14 10:14:36.743  5687  5780 W AddressConnectTracer: [Thread-15] New channel is created for address: AddressUnit{cacheId=':12:137', list=[59.111.182.48/59.111.182.48:9801], address=59.111.182.48/59.111.182.48:9801}
	09-14 10:14:36.745  5687  5780 V WZP     : [Thread-15] Start init channel
	09-14 10:14:36.745  5687  5780 V WZP     : [Thread-15] Init channel finish
	09-14 10:14:36.745  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-4] Receive EncryptHandshakeRequestEvent
	09-14 10:14:36.745  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-4] Write handshake unit
	09-14 10:14:36.745  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-4] Write finish
	09-14 10:14:36.745  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-4] Handshake result: continue
	09-14 10:14:36.745  5687  5780 W AddressConnectTracer: [Thread-16] Connect success 59.111.182.48/59.111.182.48:9801
	09-14 10:14:36.746  1862  5738 D CompatibilityInfo: mCompatibilityFlags - 0
	09-14 10:14:36.746  1862  5738 D CompatibilityInfo: applicationDensity - 440
	09-14 10:14:36.746  1862  5738 D CompatibilityInfo: applicationScale - 1.0
	09-14 10:14:36.748   794 12432 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: startService() is called for servicecom.netease.yanxuan/com.netease.deviceid.jni.EmulatorCheckService
	09-14 10:14:36.755  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.757  5687  5780 W AddressConnectTracer: [Thread-16] Channel initialize start 59.111.182.48/59.111.182.48:9801
	09-14 10:14:36.759  5687  5789 D YanXuan : wzp request end...
	09-14 10:14:36.760  5687  5780 W AddressConnectTracer: [Thread-16] Channel initialize finish 59.111.182.48/59.111.182.48:9801
	09-14 10:14:36.765  5687  5780 W AddressConnectTracer: [Thread-16] New channel is created for address: AddressUnit{cacheId=':12:137', list=[59.111.182.48/59.111.182.48:9801], address=59.111.182.48/59.111.182.48:9801}
	09-14 10:14:36.773  5687  5780 V WZP     : [Thread-16] Start init channel
	09-14 10:14:36.774  5687  5780 V WZP     : [Thread-16] Init channel finish
	09-14 10:14:36.774  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-3] Receive EncryptHandshakeRequestEvent
	09-14 10:14:36.774  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-3] Write handshake unit
	09-14 10:14:36.775  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-3] Write finish
	09-14 10:14:36.775  5687  5780 V WZPChannel: [Thread-12] Start sending request #11
	09-14 10:14:36.775  5687  5780 V WZPEncryptableCodecHandler: [nioEventLoopGroup-4-3] Handshake result: continue
	09-14 10:14:36.776  5687  5780 V WZPChannel: [Thread-17] Start sending request #12
	09-14 10:14:36.776  5687  5780 V WZPChannel: [Thread-15] Start sending request #13
	09-14 10:14:36.776  5687  5780 V WZPChannel: [Thread-13] Get response of unit #8
	09-14 10:14:36.776  5687  5780 V DefaultProfiler: [nioEventLoopGroup-4-2] Save trace: RecordItem{traceId='checkConfig-269fba9aa6d252f501169f808d9e77b5', address='59.111.182.48/59.111.182.48:9801', name='wzp', records=[Record{time=1631585676666, info='startTrace', timeSpent=0}, Record{time=1631585676666, info='write start', timeSpent=0}, Record{time=1631585676674, info='write finish', timeSpent=8}, Record{time=1631585676697, info='continueTrace', timeSpent=23}, Record{time=1631585676697, info='channelRead start', timeSpent=0}, Record{time=1631585676697, info='channelRead finish', timeSpent=0}, Record{time=1631585676697, info='stopTrace', timeSpent=0}], locate='', appId=12}
	09-14 10:14:36.776  5687  5780 V RecordStore: [wzp-profiler-save-3-1] Ignored by filter. checkConfig-269fba9aa6d252f501169f808d9e77b5
	09-14 10:14:36.777  5687  5780 V RecordStore: [wzp-profiler-save-3-1] Ignored by filter. checkConfig-269fba9aa6d252f501169f808d9e77b5
	09-14 10:14:36.777  5687  5780 V DefaultProfiler: [wzp-profiler-save-3-1] Handled by instantStore, traceId checkConfig-269fba9aa6d252f501169f808d9e77b5
	09-14 10:14:36.778  5687  5780 V WZPChannel: [Thread-16] Start sending request #14
	09-14 10:14:36.778  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.778  5687  5780 V WZPChannel: [Thread-12] Get response of unit #11
	09-14 10:14:36.778  5687  5780 V DefaultProfiler: [nioEventLoopGroup-4-4] Save trace: RecordItem{traceId='1#yanxuan-android#1631585676667#bMPryRfS', address='59.111.182.48/59.111.182.48:9801', name='wzp', records=[Record{time=1631585676688, info='startTrace', timeSpent=0}, Record{time=1631585676688, info='write start', timeSpent=0}, Record{time=1631585676688, info='write finish', timeSpent=0}, Record{time=1631585676717, info='continueTrace', timeSpent=29}, Record{time=1631585676717, info='channelRead start', timeSpent=0}, Record{time=1631585676717, info='channelRead finish', timeSpent=0}, Record{time=1631585676717, info='stopTrace', timeSpent=0}], locate='', appId=12}
	09-14 10:14:36.779  5687  5780 V DefaultProfiler: [nioEventLoopGroup-4-2] Save trace: RecordItem{traceId='checkConfig-24fb10632e88e6fc89c02c8b9d890b93', address='59.111.182.48/59.111.182.48:9801', name='wzp', records=[Record{time=1631585676677, info='startTrace', timeSpent=0}, Record{time=1631585676677, info='write start', timeSpent=0}, Record{time=1631585676679, info='write finish', timeSpent=2}, Record{time=1631585676717, info='continueTrace', timeSpent=38}, Record{time=1631585676717, info='channelRead start', timeSpent=0}, Record{time=1631585676717, info='channelRead finish', timeSpent=0}, Record{time=1631585676717, info='stopTrace', timeSpent=0}], locate='', appId=12}
	09-14 10:14:36.779  5687  5780 V RecordStore: [wzp-profiler-save-3-1] Ignored by filter. 1#yanxuan-android#1631585676667#bMPryRfS
	09-14 10:14:36.780  5687  5780 V RecordStore: [wzp-profiler-save-3-1] Ignored by filter. 1#yanxuan-android#1631585676667#bMPryRfS
	09-14 10:14:36.780  5687  5780 V DefaultProfiler: [wzp-profiler-save-3-1] Handled by instantStore, traceId 1#yanxuan-android#1631585676667#bMPryRfS
	09-14 10:14:36.780  5687  5780 V RecordStore: [wzp-profiler-save-3-1] Ignored by filter. checkConfig-24fb10632e88e6fc89c02c8b9d890b93
	09-14 10:14:36.780  5687  5780 V RecordStore: [wzp-profiler-save-3-1] Ignored by filter. checkConfig-24fb10632e88e6fc89c02c8b9d890b93
	09-14 10:14:36.780  5687  5780 V DefaultProfiler: [wzp-profiler-save-3-1] Handled by instantStore, traceId checkConfig-24fb10632e88e6fc89c02c8b9d890b93
	09-14 10:14:36.780  5687  5780 V WZPChannel: [Thread-15] Get response of unit #13
	09-14 10:14:36.781  5687  5780 V DefaultProfiler: [nioEventLoopGroup-4-3] Save trace: RecordItem{traceId='getSimple-269fba9aa6d252f501169f808d9e77b5', address='59.111.182.48/59.111.182.48:9801', name='wzp', records=[Record{time=1631585676702, info='startTrace', timeSpent=0}, Record{time=1631585676702, info='write start', timeSpent=0}, Record{time=1631585676702, info='write finish', timeSpent=0}, Record{time=1631585676757, info='continueTrace', timeSpent=55}, Record{time=1631585676757, info='channelRead start', timeSpent=0}, Record{time=1631585676757, info='channelRead finish', timeSpent=0}, Record{time=1631585676757, info='stopTrace', timeSpent=0}], locate='', appId=12}
	09-14 10:14:36.781  5687  5780 V WZPChannel: [Thread-16] Get response of unit #14
	09-14 10:14:36.781  5687  5780 V RecordStore: [wzp-profiler-save-3-1] Ignored by filter. getSimple-269fba9aa6d252f501169f808d9e77b5
	09-14 10:14:36.781  5687  5780 V RecordStore: [wzp-profiler-save-3-1] Ignored by filter. getSimple-269fba9aa6d252f501169f808d9e77b5
	09-14 10:14:36.781  5687  5780 V DefaultProfiler: [wzp-profiler-save-3-1] Handled by instantStore, traceId getSimple-269fba9aa6d252f501169f808d9e77b5
	09-14 10:14:36.784  5687  5780 V DefaultProfiler: [nioEventLoopGroup-4-1] Save trace: RecordItem{traceId='checkConfig-8fda5ea23ca2323209b93db0b3c2a5b7', address='59.111.182.48/59.111.182.48:9801', name='wzp', records=[Record{time=1631585676686, info='startTrace', timeSpent=0}, Record{time=1631585676686, info='write start', timeSpent=0}, Record{time=1631585676687, info='write finish', timeSpent=1}, Record{time=1631585676783, info='continueTrace', timeSpent=96}, Record{time=1631585676783, info='channelRead start', timeSpent=0}, Record{time=1631585676783, info='channelRead finish', timeSpent=0}, Record{time=1631585676783, info='stopTrace', timeSpent=0}], locate='', appId=12}
	09-14 10:14:36.785  5687  5780 V WZPChannel: [Thread-17] Get response of unit #12
	09-14 10:14:36.785  5687  5780 V RecordStore: [wzp-profiler-save-3-1] Ignored by filter. checkConfig-8fda5ea23ca2323209b93db0b3c2a5b7
	09-14 10:14:36.785  5687  5780 V RecordStore: [wzp-profiler-save-3-1] Ignored by filter. checkConfig-8fda5ea23ca2323209b93db0b3c2a5b7
	09-14 10:14:36.785  5687  5780 V DefaultProfiler: [wzp-profiler-save-3-1] Handled by instantStore, traceId checkConfig-8fda5ea23ca2323209b93db0b3c2a5b7
	09-14 10:14:36.788  5687  5790 D YanXuan : wzp request end...
	09-14 10:14:36.788  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.829  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 6 lines
	09-14 10:14:36.843  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.844  5961  5961 I e.yanxuan:cach: The ClassLoaderContext is a special shared library.
	09-14 10:14:36.854  2512  2512 D NavBarTintController: onSampleCollected 1.0
	09-14 10:14:36.855  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.872  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 2 lines
	09-14 10:14:36.873  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.878  5961  5961 I Perf    : Connecting to perf service.
	09-14 10:14:36.886  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.888  5961  5961 D Tinker.TinkerLoader: tryLoad test test
	09-14 10:14:36.891  5961  5961 W Tinker.TinkerLoader: tryLoadPatchFiles:patch dir not exist:/data/user/0/com.netease.yanxuan/tinker
	09-14 10:14:36.892  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.896  5961  5961 W Tinker.Tinker: tinker patch directory: /data/user/0/com.netease.yanxuan/tinker
	09-14 10:14:36.896  5961  5961 I Tinker.Tinker: try to install tinker, isEnable: true, version: 1.9.14.17
	09-14 10:14:36.897  5961  5961 I Tinker.TinkerLoadResult: parseTinkerResult loadCode:-2, process name:com.netease.yanxuan:cache, main process:false, systemOTA:false, fingerPrint:Xiaomi/vangogh/vangogh:10/QKQ1.191222.002/V12.0.6.0.QJVCNXM:user/release-keys, oatDir:null, useInterpretMode:false
	09-14 10:14:36.897  5961  5961 W Tinker.TinkerLoadResult: can't find patch file, is ok, just return
	09-14 10:14:36.897  5961  5961 I Tinker.DefaultLoadReporter: patch loadReporter onLoadResult: patch load result, path:/data/user/0/com.netease.yanxuan/tinker, code: -2, cost: 3ms
	09-14 10:14:36.897  5961  5961 W Tinker.Tinker: tinker load fail!
	09-14 10:14:36.904  5961  5961 I FeatureParser: can't find vangogh.xml in assets/device_features/,it may be in /vendor/etc/device_features
	09-14 10:14:36.907  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.907  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.905  5961  5961 W e.yanxuan:cache: type=1400 audit(0.0:52815): avc: denied { read } for name="u:object_r:vendor_displayfeature_prop:s0" dev="tmpfs" ino=27557 scontext=u:r:untrusted_app:s0:c56,c257,c512,c768 tcontext=u:object_r:vendor_displayfeature_prop:s0 tclass=file permissive=0
	09-14 10:14:36.908  5961  5961 E libc    : Access denied finding property "ro.vendor.df.effect.conflict"
	09-14 10:14:36.914  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.919  5961  5961 I MMKV    : page size:4096
	09-14 10:14:36.919  5961  5961 I MMKV    : root dir: /data/user/0/com.netease.yanxuan/files/mmkv
	09-14 10:14:36.920  5961  5961 I MMKV    : loading [YanXuan] with 7278 size in total, file size is 8192
	09-14 10:14:36.920  5961  5961 I MMKV    : loading [YanXuan] with crc 4070653268 sequence 39
	09-14 10:14:36.920  5961  5961 I MMKV    : loaded [YanXuan] with 61 values
	09-14 10:14:36.926  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.928  5961  5961 V Collie  : mIsColdStarUp false
	09-14 10:14:36.939  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.946  5961  6025 I DynamiteModule: Considering local module com.google.android.gms.measurement.dynamite:17 and remote module com.google.android.gms.measurement.dynamite:0
	09-14 10:14:36.946  5961  6025 I DynamiteModule: Selected local version of com.google.android.gms.measurement.dynamite
	09-14 10:14:36.946  5961  5961 D NetworkSecurityConfig: No Network Security Config specified, using platform default
	09-14 10:14:36.947  5961  6025 E ActivityThread: Failed to find provider info for com.google.android.gms.chimera
	09-14 10:14:36.947  5961  6025 W DynamiteModule: Failed to retrieve remote module version.
	09-14 10:14:36.948  5961  6025 W GooglePlayServicesUtil: Google Play Store is missing.
	09-14 10:14:36.952  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.959  5961  5961 D SoLoader: init start
	09-14 10:14:36.959  5961  5961 D SoLoader: adding system library source: /vendor/lib
	09-14 10:14:36.959  5961  5961 D SoLoader: adding system library source: /system/lib
	09-14 10:14:36.960  5961  5961 D SoLoader: adding application source: com.facebook.soloader.DirectorySoSource[root = /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/lib/arm64 flags = 0]
	09-14 10:14:36.960  5961  5961 D SoLoader: adding backup  source: com.facebook.soloader.ApkSoSource[root = /data/data/com.netease.yanxuan/lib-main flags = 1]
	09-14 10:14:36.960  5961  5961 D SoLoader: Preparing SO source: com.facebook.soloader.DirectorySoSource[root = /system/lib flags = 2]
	09-14 10:14:36.960  5961  5961 D SoLoader: Preparing SO source: com.facebook.soloader.DirectorySoSource[root = /vendor/lib flags = 2]
	09-14 10:14:36.960  5961  5961 D SoLoader: Preparing SO source: com.facebook.soloader.DirectorySoSource[root = /data/app/com.netease.yanxuan-YLeR3gwwgd3DyIUBNJZ8cA==/lib/arm64 flags = 0]
	09-14 10:14:36.960  5961  5961 D SoLoader: Preparing SO source: com.facebook.soloader.ApkSoSource[root = /data/data/com.netease.yanxuan/lib-main flags = 1]
	09-14 10:14:36.960  5961  5961 V fb-UnpackingSoSource: locked dso store /data/user/0/com.netease.yanxuan/lib-main
	09-14 10:14:36.961  5961  5961 I fb-UnpackingSoSource: dso store is up-to-date: /data/user/0/com.netease.yanxuan/lib-main
	09-14 10:14:36.961  5961  5961 V fb-UnpackingSoSource: releasing dso store lock for /data/user/0/com.netease.yanxuan/lib-main
	09-14 10:14:36.961  5961  5961 D SoLoader: init finish: 4 SO sources prepared
	09-14 10:14:36.961  5961  5961 D SoLoader: init exiting
	09-14 10:14:36.965  5961  6028 I FA      : App measurement initialized, version: 31049
	09-14 10:14:36.965  5961  6028 I FA      : To enable debug logging run: adb shell setprop log.tag.FA VERBOSE
	09-14 10:14:36.965  5961  6028 I FA      : To enable faster debug mode event logging run:
	09-14 10:14:36.965  5961  6028 I FA      :   adb shell setprop debug.firebase.analytics.app com.netease.yanxuan
	09-14 10:14:36.965  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.969  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 12 lines
	09-14 10:14:36.969  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.969  2512  2512 D NavBarTintController: onSampleCollected 1.0
	09-14 10:14:36.974  5961  5961 I MMKV    : loading [SplashActivityModule] with 834 size in total, file size is 4096
	09-14 10:14:36.974  5961  5961 I MMKV    : loading [SplashActivityModule] with crc 2551599884 sequence 8
	09-14 10:14:36.974  5961  5961 I MMKV    : loaded [SplashActivityModule] with 2 values
	09-14 10:14:36.983  5961  5961 D Tinker.TinkerLoader: [PendingLog @ 2021-09-14 10:14:36.888] tryLoad test test
	09-14 10:14:36.983  5961  5961 W Tinker.TinkerLoader: [PendingLog @ 2021-09-14 10:14:36.891] tryLoadPatchFiles:patch dir not exist:/data/user/0/com.netease.yanxuan/tinker
	09-14 10:14:36.984  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:36.986  5961  5961 W Tinker.UpgradePatchRetry: onPatchRetryLoad retry disabled, just return
	09-14 10:14:36.988   649  6043 E ResolverController: No valid NAT64 prefix (116, <unspecified>/0)
	09-14 10:14:36.990  5961  6028 W FA      : Failed to retrieve Firebase Instance Id
	09-14 10:14:36.999  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.000  1862  3249 W DevicePolicyManager: Package com.netease.yanxuan (uid=10312, pid=5961) cannot access Device IDs
	09-14 10:14:37.001  2875  3534 W TelephonyPermissions: reportAccessDeniedToReadIdentifiers:com.netease.yanxuan:getDeviceId:isPreinstalled=false:isPrivApp=false
	09-14 10:14:37.008  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.008  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 1 line
	09-14 10:14:37.020  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.021  5961  6028 W GooglePlayServicesUtil: Google Play Store is missing.
	09-14 10:14:37.021  5961  6028 W FA      : Service invalid
	09-14 10:14:37.023   794 12432 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: bindService is called for service : com.netease.yanxuan/com.google.android.gms.measurement.AppMeasurementService and for client com.netease.yanxuan:cache
	09-14 10:14:37.023   794 12432 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: total connections for service : com.netease.yanxuan/com.google.android.gms.measurement.AppMeasurementServiceare :2
	09-14 10:14:37.023   794 12432 I /vendor/bin/hw/vendor.qti.hardware.servicetracker@1.1-service: total connections for client : com.netease.yanxuan:cacheare :1
	09-14 10:14:37.026  5961  6028 W FA      : Failed to retrieve Firebase Instance Id
	09-14 10:14:37.033  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.036  5961  6028 W FA      : Failed to retrieve Firebase Instance Id
	09-14 10:14:37.049  5961  6028 W FA      : Failed to retrieve Firebase Instance Id
	09-14 10:14:37.050  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.063  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 24 lines
	09-14 10:14:37.063  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.067  5961  6028 W FA      : Failed to retrieve Firebase Instance Id
	09-14 10:14:37.071  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.074  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 3 lines
	09-14 10:14:37.075  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.076  5687  5687 E libEGL  : call to OpenGL ES API with no current context (logged once per thread)
	09-14 10:14:37.077  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.081  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 9 lines
	09-14 10:14:37.082  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.083  5961  6028 W FA      : Failed to retrieve Firebase Instance Id
	09-14 10:14:37.085  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.087  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 4 lines
	09-14 10:14:37.087  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.087  2512  2512 D NavBarTintController: onSampleCollected 1.0
	09-14 10:14:37.088  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.090  5687  5687 W EventBus: Please donot register with same enum priority
	09-14 10:14:37.091  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 19 lines
	09-14 10:14:37.091  5687  5687 W EventBus: Please donot register with same enum priority
	09-14 10:14:37.093  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.105  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 6 lines
	09-14 10:14:37.105  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.106  5961  6028 W FA      : Failed to retrieve Firebase Instance Id
	09-14 10:14:37.108  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.110  5687  5687 I chatty  : uid=10312(com.netease.yanxuan) identical 6 lines
	09-14 10:14:37.110  5687  5687 D ForceDarkHelper: updateByCheckExcludeList: pkg: com.netease.yanxuan activity: com.netease.yanxuan.module.mainpage.activity.MainPageActivity@c645f1c
	09-14 10:14:37.118  5687  5801 I NetworkUtil: network is available.
	09-14 10:14:37.121  1862  1920 W DevicePolicyManager: Package com.netease.yanxuan (uid=10312, pid=5687) cannot access Device IDs
	09-14 10:14:37.122  2875  3534 W TelephonyPermissions: reportAccessDeniedToReadIdentifiers:com.netease.yanxuan:getDeviceId:isPreinstalled=false:isPrivApp=false
	09-14 10:14:37.124  5687  5780 V WZPChannel: [yxs-upload] Start sending request #15
	09-14 10:14:37.125  5961  6028 W FA      : Failed to retrieve Firebase Instance Id
	09-14 10:14:37.129  5687  5780 V WZPChannel: [yxs-upload] Get response of unit #15



# /data/tombstones



如果手机没有 Root，那么需要借助 adb bugreport 命令进行抓取（官网 - 使用 adb 获取错误报告（需翻墙））
如果是安卓7.0以下，那么不支持将其打包成zip

	  $ adb bugreport ./
	Failed to get bugreportz version: 'bugreportz -v' returned '/system/bin/sh: bugreportz: not found' (code 0).
	If the device does not run Android 7.0 or above, try 'adb bugreport' instead.


直接运行 adb bugreport 会将全部报告输出到控制台（内容很多，不便查看）
可以将其输出到指定文件中，方便查阅（报告内容比较多，可能要等一会才执行完）

	  $ adb bugreport > bugreport.txt
	Failed to get bugreportz version, which is only available on devices running Android 7.0 or later.
	Trying a plain-text bug report instead.
 

 
 
###  https://wufengxue.github.io/2020/06/22/wechat-voice-codec-SEGV_MAPERR.html  有效参考分析工具 

###  https://developer.android.com/ndk/guides/ndk-stack
