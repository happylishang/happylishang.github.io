## Native Crash的统计有两部分：

####  信号量统计时机

####  堆栈捕获


so文件，里面包含的信息并不多，仅仅有符号表，其他的基本都是地址，指令，可供查直接查看的信息不多，所以真的要看线上问题，只能通过错误利用原来的so进行处理。


借助 ndk-stack 工具，可以使用符号来表示来自 adb logcat 的堆栈轨迹或 /data/tombstones/ 中的 Tombstone。该工具会将共享库内的任何地址替换为源代码中对应的 <source-file>:<line-number>，从而简化调试流程

参考文档：https://developer.android.com/ndk/guides/ndk-stack


>  adb logcat |  /Users/hzlishang/Library/Android/sdk/ndk/21.4.7075529/ndk-stack  -sym /Users/hzlishang/prj/demo/mtsec/mtsc/build/intermediates/merged_native_libs/debug/out/lib/arm64-v8a

		
	********** Crash dump: **********
	Build fingerprint: 'google/flame/flame:12/SP2A.220405.003/8210211:user/release-keys'
	#00 0x0000000000079518 /apex/com.android.runtime/lib64/bionic/libc.so (__strcat_chk+40) (BuildId: cd7952cb40d1a2deca6420c2da7910be)
	#01 0x000000000000175c /data/app/~~b_l42wjZcLhvOSDDpWEE5g==/com.netease.mtsece-u3d8mEZTmNmw_qg6EqEdEA==/base.apk!librand.so (MIXM+712) (BuildId: 1f1d16717fb0d8e2f845c76fa77b8dfc4af01df9)
	                                                                                                                             strcat(char*, char const* pass_object_size1)
	                                                                                                                             /Users/hzlishang/Library/Android/sdk/ndk/21.1.6352462/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/include/bits/fortify/string.h:128:12
	                                                                                                                             MIXM
	                                                                                                                             /Users/hzlishang/prj/demo/mtsec/mtsc/.cxx/cmake/debug/arm64-v8a/../../../../src/main/jni/sec.c:164:0
	#02 0x00000000002d4044 /apex/com.android.art/lib64/libart.so (art_quick_generic_jni_trampoline+148) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#03 0x00000000002ca764 /apex/com.android.art/lib64/libart.so (art_quick_invoke_stub+548) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#04 0x00000000002ee6b0 /apex/com.android.art/lib64/libart.so (art::interpreter::ArtInterpreterToCompiledCodeBridge(art::Thread*, art::ArtMethod*, art::ShadowFrame*, unsigned short, art::JValue*)+312) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#05 0x000000000040ade4 /apex/com.android.art/lib64/libart.so (bool art::interpreter::DoCall<false, false>(art::ArtMethod*, art::Thread*, art::ShadowFrame&, art::Instruction const*, unsigned short, art::JValue*)+820) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#06 0x0000000000409ce0 /apex/com.android.art/lib64/libart.so (MterpInvokeDirect+1580) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#07 0x00000000002c4f94 /apex/com.android.art/lib64/libart.so (mterp_op_invoke_direct+20) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#08 0x0000000000000c90 [anon:dalvik-classes4.dex extracted in memory from /data/app/~~b_l42wjZcLhvOSDDpWEE5g==/com.netease.mtsece-u3d8mEZTmNmw_qg6EqEdEA==/base.apk!classes4.dex] (com.moutai.mtsc.RandK.mCB+0)
	#09 0x000000000027d840 /apex/com.android.art/lib64/libart.so (art::interpreter::Execute(art::Thread*, art::CodeItemDataAccessor const&, art::ShadowFrame&, art::JValue, bool, bool) (.llvm.3351068054637636664)+644) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#10 0x000000000035a9e4 /apex/com.android.art/lib64/libart.so (art::interpreter::ArtInterpreterToInterpreterBridge(art::Thread*, art::CodeItemDataAccessor const&, art::ShadowFrame*, art::JValue*)+148) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#11 0x000000000040b05c /apex/com.android.art/lib64/libart.so (bool art::interpreter::DoCall<false, false>(art::ArtMethod*, art::Thread*, art::ShadowFrame&, art::Instruction const*, unsigned short, art::JValue*)+1452) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#12 0x00000000002c0ea4 /apex/com.android.art/lib64/libart.so (MterpInvokeVirtual+5380) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#13 0x00000000002c4e94 /apex/com.android.art/lib64/libart.so (mterp_op_invoke_virtual+20) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#14 0x000000000004850e [anon:dalvik-classes2.dex extracted in memory from /data/app/~~b_l42wjZcLhvOSDDpWEE5g==/com.netease.mtsece-u3d8mEZTmNmw_qg6EqEdEA==/base.apk!classes2.dex] (com.netease.mtsece.MainActivity$1.onClick+62)
	#15 0x000000000027d840 /apex/com.android.art/lib64/libart.so (art::interpreter::Execute(art::Thread*, art::CodeItemDataAccessor const&, art::ShadowFrame&, art::JValue, bool, bool) (.llvm.3351068054637636664)+644) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#16 0x000000000035a9e4 /apex/com.android.art/lib64/libart.so (art::interpreter::ArtInterpreterToInterpreterBridge(art::Thread*, art::CodeItemDataAccessor const&, art::ShadowFrame*, art::JValue*)+148) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#17 0x000000000040b05c /apex/com.android.art/lib64/libart.so (bool art::interpreter::DoCall<false, false>(art::ArtMethod*, art::Thread*, art::ShadowFrame&, art::Instruction const*, unsigned short, art::JValue*)+1452) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#18 0x00000000003d537c /apex/com.android.art/lib64/libart.so (MterpInvokeInterface+4912) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#19 0x00000000002c5094 /apex/com.android.art/lib64/libart.so (mterp_op_invoke_interface+20) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#20 0x000000000034e126 /system/framework/framework.jar (android.view.View.performClick+34)
	#21 0x00000000002ec094 /apex/com.android.art/lib64/libart.so (MterpInvokeSuper+2748) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#22 0x00000000002c4f14 /apex/com.android.art/lib64/libart.so (mterp_op_invoke_super+20) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#23 0x00000000002a3712 [anon:dalvik-classes.dex extracted in memory from /data/app/~~b_l42wjZcLhvOSDDpWEE5g==/com.netease.mtsece-u3d8mEZTmNmw_qg6EqEdEA==/base.apk] (com.google.android.material.button.MaterialButton.performClick+6)
	#24 0x000000000027d840 /apex/com.android.art/lib64/libart.so (art::interpreter::Execute(art::Thread*, art::CodeItemDataAccessor const&, art::ShadowFrame&, art::JValue, bool, bool) (.llvm.3351068054637636664)+644) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#25 0x000000000035a9e4 /apex/com.android.art/lib64/libart.so (art::interpreter::ArtInterpreterToInterpreterBridge(art::Thread*, art::CodeItemDataAccessor const&, art::ShadowFrame*, art::JValue*)+148) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#26 0x000000000040b05c /apex/com.android.art/lib64/libart.so (bool art::interpreter::DoCall<false, false>(art::ArtMethod*, art::Thread*, art::ShadowFrame&, art::Instruction const*, unsigned short, art::JValue*)+1452) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#27 0x00000000002c0ea4 /apex/com.android.art/lib64/libart.so (MterpInvokeVirtual+5380) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#28 0x00000000002c4e94 /apex/com.android.art/lib64/libart.so (mterp_op_invoke_virtual+20) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#29 0x000000000034e15a /system/framework/framework.jar (android.view.View.performClickInternal+6)
	#30 0x000000000027d840 /apex/com.android.art/lib64/libart.so (art::interpreter::Execute(art::Thread*, art::CodeItemDataAccessor const&, art::ShadowFrame&, art::JValue, bool, bool) (.llvm.3351068054637636664)+644) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#31 0x000000000035a9e4 /apex/com.android.art/lib64/libart.so (art::interpreter::ArtInterpreterToInterpreterBridge(art::Thread*, art::CodeItemDataAccessor const&, art::ShadowFrame*, art::JValue*)+148) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#32 0x000000000040b05c /apex/com.android.art/lib64/libart.so (bool art::interpreter::DoCall<false, false>(art::ArtMethod*, art::Thread*, art::ShadowFrame&, art::Instruction const*, unsigned short, art::JValue*)+1452) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#33 0x0000000000409ce0 /apex/com.android.art/lib64/libart.so (MterpInvokeDirect+1580) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#34 0x00000000002c4f94 /apex/com.android.art/lib64/libart.so (mterp_op_invoke_direct+20) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#35 0x0000000000349598 /system/framework/framework.jar (android.view.View.access$3700+0)
	#36 0x000000000027d840 /apex/com.android.art/lib64/libart.so (art::interpreter::Execute(art::Thread*, art::CodeItemDataAccessor const&, art::ShadowFrame&, art::JValue, bool, bool) (.llvm.3351068054637636664)+644) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#37 0x000000000035a9e4 /apex/com.android.art/lib64/libart.so (art::interpreter::ArtInterpreterToInterpreterBridge(art::Thread*, art::CodeItemDataAccessor const&, art::ShadowFrame*, art::JValue*)+148) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#38 0x000000000040b05c /apex/com.android.art/lib64/libart.so (bool art::interpreter::DoCall<false, false>(art::ArtMethod*, art::Thread*, art::ShadowFrame&, art::Instruction const*, unsigned short, art::JValue*)+1452) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#39 0x000000000076d4b8 /apex/com.android.art/lib64/libart.so (MterpInvokeStatic+3812) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#40 0x00000000002c5014 /apex/com.android.art/lib64/libart.so (mterp_op_invoke_static+20) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#41 0x0000000000326418 /system/framework/framework.jar (android.view.View$PerformClick.run+16)
	#42 0x00000000003d49e8 /apex/com.android.art/lib64/libart.so (MterpInvokeInterface+2460) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#43 0x00000000002c5094 /apex/com.android.art/lib64/libart.so (mterp_op_invoke_interface+20) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#44 0x000000000042fc34 /system/framework/framework.jar (android.os.Handler.handleCallback+4)
	#45 0x000000000076ce1c /apex/com.android.art/lib64/libart.so (MterpInvokeStatic+2120) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#46 0x00000000002c5014 /apex/com.android.art/lib64/libart.so (mterp_op_invoke_static+20) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#47 0x000000000042faa8 /system/framework/framework.jar (android.os.Handler.dispatchMessage+8)
	#48 0x00000000002c0294 /apex/com.android.art/lib64/libart.so (MterpInvokeVirtual+2292) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#49 0x00000000002c4e94 /apex/com.android.art/lib64/libart.so (mterp_op_invoke_virtual+20) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#50 0x00000000004594ba /system/framework/framework.jar (android.os.Looper.loopOnce+334)
	#51 0x000000000076ce1c /apex/com.android.art/lib64/libart.so (MterpInvokeStatic+2120) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#52 0x00000000002c5014 /apex/com.android.art/lib64/libart.so (mterp_op_invoke_static+20) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#53 0x0000000000459afc /system/framework/framework.jar (android.os.Looper.loop+152)
	#54 0x000000000076c840 /apex/com.android.art/lib64/libart.so (MterpInvokeStatic+620) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#55 0x00000000002c5014 /apex/com.android.art/lib64/libart.so (mterp_op_invoke_static+20) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#56 0x00000000001c8996 /system/framework/framework.jar (android.app.ActivityThread.main+202)
	#57 0x000000000027d840 /apex/com.android.art/lib64/libart.so (art::interpreter::Execute(art::Thread*, art::CodeItemDataAccessor const&, art::ShadowFrame&, art::JValue, bool, bool) (.llvm.3351068054637636664)+644) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#58 0x000000000027c9e8 /apex/com.android.art/lib64/libart.so (artQuickToInterpreterBridge+1176) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#59 0x00000000002d4178 /apex/com.android.art/lib64/libart.so (art_quick_to_interpreter_bridge+88) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#60 0x00000000002ca9e8 /apex/com.android.art/lib64/libart.so (art_quick_invoke_static_stub+568) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#61 0x000000000035b5d0 /apex/com.android.art/lib64/libart.so (_jobject* art::InvokeMethod<(art::PointerSize)8>(art::ScopedObjectAccessAlreadyRunnable const&, _jobject*, _jobject*, _jobject*, unsigned long)+608) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#62 0x000000000035b348 /apex/com.android.art/lib64/libart.so (art::Method_invoke(_JNIEnv*, _jobject*, _jobject*, _jobjectArray*)+52) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#63 0x00000000000b2f74 /apex/com.android.art/javalib/arm64/boot.oat (art_jni_trampoline+132) (BuildId: ad9ee401645a5135206a62ff86fc2ef5cdc29120)
	#64 0x00000000002ca764 /apex/com.android.art/lib64/libart.so (art_quick_invoke_stub+548) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#65 0x00000000002ee6b0 /apex/com.android.art/lib64/libart.so (art::interpreter::ArtInterpreterToCompiledCodeBridge(art::Thread*, art::ArtMethod*, art::ShadowFrame*, unsigned short, art::JValue*)+312) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#66 0x000000000040ade4 /apex/com.android.art/lib64/libart.so (bool art::interpreter::DoCall<false, false>(art::ArtMethod*, art::Thread*, art::ShadowFrame&, art::Instruction const*, unsigned short, art::JValue*)+820) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#67 0x00000000002c0ea4 /apex/com.android.art/lib64/libart.so (MterpInvokeVirtual+5380) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#68 0x00000000002c4e94 /apex/com.android.art/lib64/libart.so (mterp_op_invoke_virtual+20) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#69 0x000000000024a562 /system/framework/framework.jar (com.android.internal.os.RuntimeInit$MethodAndArgsCaller.run+22)
	#70 0x000000000027d840 /apex/com.android.art/lib64/libart.so (art::interpreter::Execute(art::Thread*, art::CodeItemDataAccessor const&, art::ShadowFrame&, art::JValue, bool, bool) (.llvm.3351068054637636664)+644) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#71 0x000000000027c9e8 /apex/com.android.art/lib64/libart.so (artQuickToInterpreterBridge+1176) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#72 0x00000000002d4178 /apex/com.android.art/lib64/libart.so (art_quick_to_interpreter_bridge+88) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#73 0x0000000000858d28 /data/misc/apexdata/com.android.art/dalvik-cache/arm64/boot-framework.oat (com.android.internal.os.ZygoteInit.main+2232)
	#74 0x00000000002ca9e8 /apex/com.android.art/lib64/libart.so (art_quick_invoke_static_stub+568) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#75 0x000000000044ca04 /apex/com.android.art/lib64/libart.so (art::JValue art::InvokeWithVarArgs<_jmethodID*>(art::ScopedObjectAccessAlreadyRunnable const&, _jobject*, _jmethodID*, std::__va_list)+464) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#76 0x000000000062cf30 /apex/com.android.art/lib64/libart.so (art::JNI<true>::CallStaticVoidMethodV(_JNIEnv*, _jclass*, _jmethodID*, std::__va_list)+268) (BuildId: 34e3dd028e2e682b63a512d6a4f1b5eb)
	#77 0x00000000000b5ac4 /system/lib64/libandroid_runtime.so (_JNIEnv::CallStaticVoidMethod(_jclass*, _jmethodID*, ...)+120) (BuildId: 4412672799aae7dbc3e6195f95eaed8d)
	#78 0x00000000000c0fb4 /system/lib64/libandroid_runtime.so (android::AndroidRuntime::start(char const*, android::Vector<android::String8> const&, bool)+836) (BuildId: 4412672799aae7dbc3e6195f95eaed8d)
	#79 0x000000000000258c /system/bin/app_process64 (main+1336) (BuildId: b19398086311144b1336801485a886c8)
	#80 0x00000000000487dc /apex/com.android.runtime/lib64/bionic/libc.so (__libc_init+96) (BuildId: cd7952cb40d1a2deca6420c2da7910be)
	Crash dump is completed
	
	java -jar buglyqq-upload-symbol.jar -appid  0c20628c17  -appkey  412f7724-37fe-41b7-90d6-5456aca04a14  -bundleid com.moutai.mall  -version 1.2.1   -platform  Android -inputSymbol      /Users/hzlishang/prj/demo/mtsec/mtsc/build/intermediates/merged_native_libs/debug/out/lib/arm64-v8a
	
	
#### 	根据线上日志/符号表导出原来堆栈

	
	/Users/hzlishang/Library/Android/sdk/ndk/21.4.7075529/ndk-stack  -sym /Users/hzlishang/prj/demo/xCrash/xcrash_sample/build/intermediates/merged_native_libs/debug/out/lib/arm64-v8a  --dump   crash.log
	
	
	
	
	