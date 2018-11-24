targetsdk 之前是22 ，

适应华为市场升级26 

# 一：权限 6.0

￼￼￼￼![image.png](https://upload-images.jianshu.io/upload_images/1460468-cee573d9f6937c51.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

*  定位：登陆页面+统计埋点
* IMEI+序列号 ：风控需求  启动的时候获取 ，是否还需要在其他地方获取，每次启动提示
*  存储：按道理说，只有照片
  相机：拍摄的地方，很明显

现在，需要 Build.serial 标识符的应用必须请求 READ_PHONE_STATE 权限，然后使用 Android P 中新增的新 Build.getSerial() 函数



二 ：touch id

三：AndroidP

fresco升级问题 

getEncodedCacheKey(imageRequest)

getEncodedCacheKey(imageRequest, AppProfile.getContext())



## sdk升级问题   (升级没问题，但是如果出现问题，降级需要删除APP重新安装)

Canvas save

Canvas.CLIP_SAVE_FLAG

ImgPermissionUtil.getInstance().start

/data/user/0/com.netease.yanxuan/files/.appSkinAndroid/yxskin

外部存储权限的问题？？

视频播放问题

第三方aptch包问题  

java.lang.RuntimeException: Stub!
	at org.apache.http.impl.client.AbstractHttpClient.<init>(AbstractHttpClient.java:6)
	at org.apache.http.impl.client.DefaultHttpClient.<init>(DefaultHttpClient.java:8)
	at com.sina.weibo.sdk.net.HttpManager.getNewHttpClient(HttpManager.java:505)
	at com.sina.weibo.sdk.net.HttpManager.requestHttpExecute(HttpManager.java:134)
	at com.sina.weibo.sdk.net.HttpManager.openUrl(HttpManager.java:112)
	at com.sina.weibo.sdk.net.NetUtils.internalHttpRequest(NetUtils.java:46)
	at com.sina.weibo.sdk.utils.AidTask.loadAidFromNet(AidTask.java:344)
	at com.sina.weibo.sdk.utils.AidTask.access$3(AidTask.java:331)
	at com.sina.weibo.sdk.utils.AidTask$2.run(AidTask.java:203)
	at java.lang.Thread.run(Thread.java:764)
	
	
## 	关于外部存储的问题


* lrwxrwxrwx root     root              1971-01-13 01:49 sdcard -> /storage/self/primary
* lrwxrwxrwx root     root              1971-01-13 01:49 primary -> /mnt/user/0/primary
* lrwxrwxrwx root     root              2018-11-18 15:05 primary -> /storage/emulated/0


Also starting in API level 19, this permission is not required to read/write files in your application-specific directories returned by Context.getExternalFilesDir(String) and Context.getExternalCacheDir().

## 二维码分享问题


# 7.0 视频播放器问题 Textuview

相机拍照适配，权限Fileprovider的回收，其实是7.0引入的
参考文档[使用FileProvider解决file:// URI引起的FileUriExposedException

https://inthecheesefactory.com/blog/how-to-share-access-to-file-with-fileprovider-on-android-nougat](http://gelitenight.github.io/android/2017/01/29/solve-FileUriExposedException-caused-by-file-uri-with-FileProvider.html#section-3)

https://blog.csdn.net/Zz110753/article/details/60877594

[Android7.0调用系统相机拍照、访问相册问题。](https://blog.csdn.net/Zz110753/article/details/60877594)


### webview多进程共享目录

	11-24 15:12:34.203 30351-30351/com.netease.yanxuan E/AndroidRuntime: FATAL EXCEPTION: main
	    Process: com.netease.yanxuan, PID: 30351
	    java.lang.RuntimeException: Using WebView from more than one process at once with the same data directory is not supported. https://crbug.com/558377
	        at xk.b(SourceFile:96)
	        at xm.run(SourceFile:3)
	        at android.os.Handler.handleCallback(Handler.java:873)
	        at android.os.Handler.dispatchMessage(Handler.java:99)
	        at android.os.Looper.loop(Looper.java:193)
	        at android.app.ActivityThread.main(ActivityThread.java:6669)
	        at java.lang.reflect.Method.invoke(Native Method)
	        at com.android.internal.os.RuntimeInit$MethodAndArgsCaller.run(RuntimeInit.java:493)
	        at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:858)
	11-24 15:12:34.226 30351-30351/com.netease.yanxuan E/Crash: java.lang.RuntimeException: Using WebView from more than one process at once with the same data directory is not supported. https://crbug.com/558377
	        at xk.b(SourceFile:96)
	        at xm.run(SourceFile:3)
	        at android.os.Handler.handleCallback(Handler.java:873)
	        at android.os.Handler.dispatchMessage(Handler.java:99)
	        at android.os.Looper.loop(Looper.java:193)
	        at android.app.ActivityThread.main(ActivityThread.java:6669)
	        at java.lang.reflect.Method.invoke(Native Method)
	        at com.android.internal.os.RuntimeInit$MethodAndArgsCaller.run(RuntimeInit.java:493)
	        at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:858)
	11-24 15:12:34.235 30351-30351/com.netease.yanxuan E/CrashHandler: an error occured while writing file...
	    java.io.FileNotFoundException: /storage/emulated/0/crash/yanxuan/crash-2018-11-24-15-12-34-1543043554226.log (Permission denied)
	        at java.io.FileOutputStream.open0(Native Method)
	        at java.io.FileOutputStream.open(FileOutputStream.java:308)
	        at java.io.FileOutputStream.<init>(FileOutputStream.java:238)
	        at java.io.FileOutputStream.<init>(FileOutputStream.java:119)
	        at com.netease.yanxuan.crash.CrashHandler.saveCrashInfo2File(CrashHandler.java:157)
	        at com.netease.yanxuan.crash.CrashHandler.handleException(CrashHandler.java:115)
	        at com.netease.yanxuan.crash.CrashHandler.uncaughtException(CrashHandler.java:79)
	        at java.lang.ThreadGroup.uncaughtException(ThreadGroup.java:1068)
	        at java.lang.ThreadGroup.uncaughtException(ThreadGroup.java:1063)
	        at java.lang.Thread.dispatchUncaughtException(Thread.java:1955)


### 问题二 不支持http请求 CLERRTEXT


	11-24 16:00:32.377 6456-7088/com.netease.yanxuan W/WebView: java.lang.Throwable: A WebView method was called on thread 'JavaBridge'. All WebView methods must be called on the same thread. (Expected Looper Looper (main, tid 2) {e452b38} called on Looper (JavaBridge, tid 6177) {fece0a7}, FYI main Looper is Looper (main, tid 2) {e452b38})
	        at android.webkit.WebView.checkThread(WebView.java:2695)
	        at android.webkit.WebView.getUrl(WebView.java:1488)
	        at com.netease.yanxuan.common.yanxuan.view.yxwebview.YXWebView$InJavaScriptLocalObj.showSource(YXWebView.java:404)
	        at android.os.MessageQueue.nativePollOnce(Native Method)
	        at android.os.MessageQueue.next(MessageQueue.java:326)
	        at android.os.Looper.loop(Looper.java:160)
	        at android.os.HandlerThread.run(HandlerThread.java:65)
	11-24 16:00:32.378 6456-7088/com.netease.yanxuan D/StrictMode: StrictMode policy violation: android.os.strictmode.WebViewMethodCalledOnWrongThreadViolation
	        at android.webkit.WebView.checkThread(WebView.java:2695)
	        at android.webkit.WebView.getUrl(WebView.java:1488)
	        at com.netease.yanxuan.common.yanxuan.view.yxwebview.YXWebView$InJavaScriptLocalObj.showSource(YXWebView.java:404)
	        at android.os.MessageQueue.nativePollOnce(Native Method)
	        at android.os.MessageQueue.next(MessageQueue.java:326)
	        at android.os.Looper.loop(Looper.java:160)
	        at android.os.HandlerThread.run(HandlerThread.java:65)
	11-24 16:00:32.378 6456-7088/com.netease.yanxuan W/System.err: java.lang.RuntimeException: java.lang.Throwable: A WebView method was called on thread 'JavaBridge'. All WebView methods must be called on the same thread. (Expected Looper Looper (main, tid 2) {e452b38} called on Looper (JavaBridge, tid 6177) {fece0a7}, FYI main Looper is Looper (main, tid 2) {e452b38})
	        at android.webkit.WebView.checkThread(WebView.java:2700)
	        at android.webkit.WebView.getUrl(WebView.java:1488)
	        at com.netease.yanxuan.common.yanxuan.view.yxwebview.YXWebView$InJavaScriptLocalObj.showSource(YXWebView.java:404)
	        at android.os.MessageQueue.nativePollOnce(Native Method)
	11-24 16:00:32.379 6456-7088/com.netease.yanxuan W/System.err:     at android.os.MessageQueue.next(MessageQueue.java:326)
	        at android.os.Looper.loop(Looper.java:160)
	        at android.os.HandlerThread.run(HandlerThread.java:65)
	    Caused by: java.lang.Throwable: A WebView method was called on thread 'JavaBridge'. All WebView methods must be called on the same thread. (Expected Looper Looper (main, tid 2) {e452b38} called on Looper (JavaBridge, tid 6177) {fece0a7}, FYI main Looper is Looper (main, tid 2) {e452b38})
	        at android.webkit.WebView.checkThread(WebView.java:2695)
	    	... 6 more
	    	
 解决方式
	
在资源文件新建xml目录，新建文件、

	network_security_config.xml
	<?xml version="1.0" encoding="utf-8"?>
	<network-security-config>
	    <domain-config cleartextTrafficPermitted="true">
	        <domain includeSubdomains="true">android.bugly.qq.com</domain>
	    </domain-config>
	</network-security-config>

复制代码然后在清单文件中application下加入android:networkSecurityConfig="@xml/network_security_config"即可

 	    	
	    		        
# 8.0  序列号问题 

* Build.getSerail
* Android 8.0: java.lang.IllegalStateException: Not allowed to start service Intent 后台服务
* Fresco跟weex的冲突，so没有 从apk解压拷出来  缺少就考呗到主工程，反正要合并
 
###  优化点

可以适当将Bitmap缓存调整大

# 9.0 API问题 电池问题 非公开API SERVICE问题
	
 
## 换肤部分API

	11-24 10:37:01.852 30241 30241 W netease.yanxua: Accessing hidden field Landroid/view/LayoutInflater;->mFactory:Landroid/view/LayoutInflater$Factory; (light greylist, reflection)
	11-24 10:37:01.852 30241 30241 W netease.yanxua: Accessing hidden field Landroid/view/LayoutInflater;->mFactory2:Landroid/view/LayoutInflater$Factory2; (light greylist, reflection)
	
## 可信ID部分API ，只有这部分有两个处于深灰区域
	
	11-24 10:37:01.882 30241 30442 W netease.yanxua: Accessing hidden method Landroid/os/ServiceManager;->getService(Ljava/lang/String;)Landroid/os/IBinder; (light greylist, reflection)
	11-24 10:37:01.883 30241 30442 W netease.yanxua: Accessing hidden method Lcom/android/internal/telephony/ITelephony$Stub;->asInterface(Landroid/os/IBinder;)Lcom/android/internal/telephony/ITelephony; (light greylist, reflection)
	11-24 10:37:01.883 30241 30442 W netease.yanxua: Accessing hidden method Lcom/android/internal/telephony/ITelephony$Stub$Proxy;->getDeviceId(Ljava/lang/String;)Ljava/lang/String; (light greylist, reflection)
	11-24 10:37:01.884 30241 30442 W netease.yanxua: Accessing hidden method Lcom/android/internal/telephony/ITelephony$Stub$Proxy;->getInterfaceDescriptor()Ljava/lang/String; (dark greylist, reflection)
	11-24 10:37:01.886 30241 30442 W netease.yanxua: Accessing hidden method Lcom/android/internal/telephony/IPhoneSubInfo$Stub;->asInterface(Landroid/os/IBinder;)Lcom/android/internal/telephony/IPhoneSubInfo; (light greylist, reflection)
	11-24 10:37:01.886 30241 30442 W netease.yanxua: Accessing hidden method Lcom/android/internal/telephony/IPhoneSubInfo$Stub$Proxy;->getDeviceId(Ljava/lang/String;)Ljava/lang/String; (light greylist, reflection)
	11-24 10:37:01.886 30241 30442 W netease.yanxua: Accessing hidden method Lcom/android/internal/telephony/IPhoneSubInfo$Stub$Proxy;->getInterfaceDescriptor()Ljava/lang/String; (dark greylist, reflection)
	11-24 10:37:01.890 30241 30442 W netease.yanxua: Accessing hidden method Landroid/os/UserHandle;->getUserId(I)I (light greylist, reflection)
	11-24 10:37:01.890 30241 30442 W netease.yanxua: Accessing hidden field Landroid/provider/Settings$Global;->MOVED_TO_SECURE:Ljava/util/HashSet; (light greylist, reflection)
	11-24 10:37:01.890 30241 30442 W netease.yanxua: Accessing hidden field Landroid/provider/Settings$Secure;->MOVED_TO_LOCK_SETTINGS:Ljava/util/HashSet; (light greylist, reflection)
	11-24 10:37:01.890 30241 30442 W netease.yanxua: Accessing hidden field Landroid/provider/Settings$Secure;->MOVED_TO_GLOBAL:Ljava/util/HashSet; (light greylist, reflection)
	11-24 10:37:01.890 30241 30442 W netease.yanxua: Accessing hidden field Landroid/provider/Settings$Secure;->sNameValueCache:Landroid/provider/Settings$NameValueCache; (light greylist, reflection)
	11-24 10:37:01.890 30241 30442 W netease.yanxua: Accessing hidden method Landroid/provider/Settings$NameValueCache;->getStringForUser(Landroid/content/ContentResolver;Ljava/lang/String;I)Ljava/lang/String; (light greylist, reflection)


# 使用veridex扫描出来


>snaildeMac-mini:veridex-mac personal$ ./appcompat.sh  --dex-file=/Users/personal/soft/runtime-master-appcompat/app-original-debug.apk 

NOTE: appcompat.sh is still under development. It can report
API uses that do not execute at runtime, and reflection uses
that do not exist. It can also miss on reflection uses.

	#1: Linking light greylist Landroid/app/Notification;->setLatestEventInfo(Landroid/content/Context;Ljava/lang/CharSequence;Ljava/lang/CharSequence;Landroid/app/PendingIntent;)V use(s):
	       Lcom/xiaomi/push/service/XMPushService;->a(Landroid/content/Context;)Landroid/app/Notification;
	
	#2: Linking light greylist Landroid/net/SSLCertificateSocketFactory;->getHttpSocketFactory(ILandroid/net/SSLSessionCache;)Lorg/apache/http/conn/ssl/SSLSocketFactory; use(s):
	       Lcom/alipay/android/phone/mrpc/core/b;->a(Ljava/lang/String;)Lcom/alipay/android/phone/mrpc/core/b;
	
	#3: Linking dark greylist Landroid/os/SystemProperties;->get(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String; use(s):
	       Lcom/huawei/updatesdk/framework/bean/startup/StartupRequest;->getSysteBit()I
	       Lcom/huawei/updatesdk/support/a/a;->b()Ljava/lang/String;
	       Lcom/huawei/updatesdk/support/f/c;->e()I
	
	#4: Linking dark greylist Landroid/os/SystemProperties;->getInt(Ljava/lang/String;I)I use(s):
	       Lcom/huawei/updatesdk/support/f/c;->d()I
	
	#5: Linking light greylist Landroid/util/FloatMath;->sqrt(F)F use(s):
	       Lcom/tencent/connect/avatar/c;->a(Landroid/view/MotionEvent;)F
	
	#6: Linking dark greylist Lcom/android/internal/util/Predicate;->apply(Ljava/lang/Object;)Z use(s):
	       Lcom/facebook/imagepipeline/cache/CountingLruMap;->getMatchingEntries(Lcom/android/internal/util/Predicate;)Ljava/util/ArrayList;
	       Lcom/facebook/imagepipeline/cache/CountingLruMap;->removeAll(Lcom/android/internal/util/Predicate;)Ljava/util/ArrayList;
	
	#7: Reflection light greylist Landroid/app/ActivityManager;->INTENT_SENDER_ACTIVITY use(s):
	       Lcom/tencent/tinker/loader/hotplug/handler/AMSInterceptHandler;-><clinit>()V
	
	#8: Reflection light greylist Landroid/app/ActivityThread;->currentActivityThread use(s):
	       Lcom/qiyukf/unicorn/d;->a(Lcom/qiyukf/unicorn/d;Landroid/content/Context;)V
	       Lcom/tencent/tinker/loader/shareutil/ShareReflectUtil;->getActivityThread(Landroid/content/Context;Ljava/lang/Class;)Ljava/lang/Object;
	       Lcom/tencent/tinker/loader/shareutil/ShareReflectUtil;->getActivityThread(Landroid/content/Context;Ljava/lang/Class;)Ljava/lang/Object;
	
	#9: Reflection light greylist Landroid/app/ActivityThread;->mInstrumentation use(s):
	       Lcom/qiyukf/unicorn/d;->a(Lcom/qiyukf/unicorn/d;Landroid/content/Context;)V
	
	#10: Reflection light greylist Landroid/app/ActivityThread;->mPackages use(s):
	       Lcom/tencent/tinker/loader/TinkerResourcePatcher;->isResourceCanPatch(Landroid/content/Context;)V
	
	#11: Reflection light greylist Landroid/app/ActivityThread;->mResourcePackages use(s):
	       Lcom/tencent/tinker/loader/TinkerResourcePatcher;->isResourceCanPatch(Landroid/content/Context;)V
	
	#12: Reflection light greylist Landroid/app/AppOpsManager;->OP_POST_NOTIFICATION use(s):
	       Lcom/xiaomi/channel/commonutils/android/a;->c(Landroid/content/Context;Ljava/lang/String;)Lcom/xiaomi/channel/commonutils/android/a$a;
	
	#13: Reflection light greylist Landroid/app/AppOpsManager;->checkOp use(s):
	       Lcom/netease/libs/permissioncompat/SettingsCompat;->checkOp(Landroid/content/Context;I)I
	
	#14: Reflection light greylist Landroid/app/LoadedApk;->mResDir use(s):
	       Lcom/tencent/tinker/loader/TinkerResourcePatcher;->isResourceCanPatch(Landroid/content/Context;)V
	
	#15: Reflection light greylist Landroid/app/ResourcesManager;->getInstance use(s):
	       Lcom/tencent/tinker/loader/TinkerResourcePatcher;->isResourceCanPatch(Landroid/content/Context;)V
	
	#16: Reflection light greylist Landroid/bluetooth/BluetoothAdapter;->mService use(s):
	       Lcom/alipay/b/a/a/b/b;->a(Landroid/bluetooth/BluetoothAdapter;)Ljava/lang/String;
	
	#17: Reflection light greylist Landroid/content/Intent;->mExtras use(s):
	       Lcom/tencent/tinker/loader/shareutil/ShareIntentUtil;->fixIntentClassLoader(Landroid/content/Intent;Ljava/lang/ClassLoader;)V
	
	#18: Reflection dark greylist Landroid/content/IntentFilter;->setAutoVerify use(s):
	       Lcom/tencent/tinker/loader/hotplug/IncrementComponentManager;->parseIntentFilter(Landroid/content/Context;Ljava/lang/String;Lorg/xmlpull/v1/XmlPullParser;)V
	       Lcom/tencent/tinker/loader/hotplug/IncrementComponentManager;->parseIntentFilter(Landroid/content/Context;Ljava/lang/String;Lorg/xmlpull/v1/XmlPullParser;)V
	
	#19: Reflection dark greylist Landroid/content/pm/ActivityInfo;->FLAG_ALLOW_EMBEDDED use(s):
	       Lcom/tencent/tinker/loader/hotplug/IncrementComponentManager$1;->onTranslate(Landroid/content/Context;ILjava/lang/String;Ljava/lang/String;Landroid/content/pm/ActivityInfo;)V
	
	#20: Reflection dark greylist Landroid/content/pm/ActivityInfo;->FLAG_SHOW_FOR_ALL_USERS use(s):
	       Lcom/tencent/tinker/loader/hotplug/IncrementComponentManager$1;->onTranslate(Landroid/content/Context;ILjava/lang/String;Ljava/lang/String;Landroid/content/pm/ActivityInfo;)V
	       Lcom/tencent/tinker/loader/hotplug/IncrementComponentManager$1;->onTranslate(Landroid/content/Context;ILjava/lang/String;Ljava/lang/String;Landroid/content/pm/ActivityInfo;)V
	
	#21: Reflection dark greylist Landroid/content/pm/PackageParser;->collectCertificates use(s):
	       Lcom/netease/yanxuan/common/util/install/SignUtil;->getUnInstalledApkSignature(Ljava/lang/String;)Ljava/lang/String;
	
	#22: Reflection light greylist Landroid/content/pm/PackageParser;->parsePackage use(s):
	       Lcom/netease/yanxuan/common/util/install/SignUtil;->getUnInstalledApkSignature(Ljava/lang/String;)Ljava/lang/String;
	
	#23: Reflection light greylist Landroid/content/res/AssetManager;->addAssetPath use(s):
	       Lcom/netease/yanxuan/yxskin/res/SkinCompatResources;->getSkinResources(Ljava/lang/String;)Landroid/content/res/Resources;
	       Lcom/tencent/tinker/loader/TinkerResourcePatcher;->isResourceCanPatch(Landroid/content/Context;)V
	
	#24: Reflection dark greylist Landroid/content/res/AssetManager;->ensureStringBlocks use(s):
	       Lcom/tencent/tinker/loader/TinkerResourcePatcher;->isResourceCanPatch(Landroid/content/Context;)V
	
	#25: Reflection light greylist Landroid/content/res/Resources;->mResourcesImpl use(s):
	       Lcom/tencent/tinker/loader/TinkerResourcePatcher;->isResourceCanPatch(Landroid/content/Context;)V
	
	#26: Reflection light greylist Landroid/content/res/Resources;->mTypedArrayPool use(s):
	       Lcom/tencent/tinker/loader/TinkerResourcePatcher;->clearPreloadTypedArrayIssue(Landroid/content/res/Resources;)V
	
	#27: Reflection light greylist Landroid/graphics/FontFamily;->abortCreation use(s):
	       Landroid/support/v4/graphics/TypefaceCompatApi26Impl;-><clinit>()V
	
	#28: Reflection light greylist Landroid/graphics/FontFamily;->addFontFromAssetManager use(s):
	       Landroid/support/v4/graphics/TypefaceCompatApi26Impl;-><clinit>()V
	
	#29: Reflection light greylist Landroid/graphics/FontFamily;->addFontFromBuffer use(s):
	       Landroid/support/v4/graphics/TypefaceCompatApi26Impl;-><clinit>()V
	
	#30: Reflection light greylist Landroid/graphics/FontFamily;->freeze use(s):
	       Landroid/support/v4/graphics/TypefaceCompatApi26Impl;-><clinit>()V
	
	#31: Reflection light greylist Landroid/graphics/Typeface;->createFromFamiliesWithDefault use(s):
	       Landroid/support/v4/graphics/TypefaceCompatApi24Impl;-><clinit>()V
	       Landroid/support/v4/graphics/TypefaceCompatApi26Impl;-><clinit>()V
	
	#32: Reflection dark greylist Landroid/graphics/drawable/Drawable;->getOpticalInsets use(s):
	       Landroid/support/v7/internal/widget/DrawableUtils;->getOpticalBounds(Landroid/graphics/drawable/Drawable;)Landroid/graphics/Rect;
	
	#33: Reflection light greylist Landroid/graphics/drawable/Drawable;->isProjected use(s):
	       Landroid/support/v4/graphics/drawable/DrawableWrapperApi21;->findAndCacheIsProjectedDrawableMethod()V
	
	#34: Reflection light greylist Landroid/graphics/drawable/NinePatchDrawable$NinePatchState;->mNinePatch use(s):
	       Lcom/unionpay/tsmservice/mi/widget/UPSaftyKeyboard;->d(Landroid/graphics/drawable/Drawable;)Lcom/unionpay/tsmservice/mi/data/NinePatchInfo;
	       Lcom/unionpay/tsmservice/widget/UPSaftyKeyboard;->d(Landroid/graphics/drawable/Drawable;)Lcom/unionpay/tsmservice/data/NinePatchInfo;
	
	#35: Reflection light greylist Landroid/media/AudioAttributes;->toLegacyStreamType use(s):
	       Landroid/support/v4/media/AudioAttributesCompatApi21;->toLegacyStreamType(Landroid/support/v4/media/AudioAttributesCompatApi21$Wrapper;)I
	
	#36: Reflection light greylist Landroid/media/session/MediaSession;->getCallingPackage use(s):
	       Landroid/support/v4/media/session/MediaSessionCompatApi24;->getCallingPackage(Ljava/lang/Object;)Ljava/lang/String;
	
	#37: Reflection light greylist Landroid/os/Bundle;->getIBinder use(s):
	       Landroid/support/v4/app/BundleCompat$BundleCompatBaseImpl;->getBinder(Landroid/os/Bundle;Ljava/lang/String;)Landroid/os/IBinder;
	
	#38: Reflection light greylist Landroid/os/Bundle;->putIBinder use(s):
	       Landroid/support/v4/app/BundleCompat$BundleCompatBaseImpl;->putBinder(Landroid/os/Bundle;Ljava/lang/String;Landroid/os/IBinder;)V
	
	#39: Reflection light greylist Landroid/os/Handler;->mCallback use(s):
	       Lcom/tencent/tinker/loader/hotplug/interceptor/HandlerMessageInterceptor;-><clinit>()V
	
	#40: Reflection light greylist Landroid/os/MemoryFile;->getFileDescriptor use(s):
	       Lcom/facebook/imagepipeline/platform/GingerbreadPurgeableDecoder;->getFileDescriptorMethod()Ljava/lang/reflect/Method;
	
	#41: Reflection light greylist Landroid/os/ServiceManager;->getService use(s):
	       Lcom/netease/deviceid/IPhoneSubInfoUtil;->getDeviceIdLevel1(Landroid/content/Context;)Ljava/lang/String;
	       Lcom/netease/deviceid/IPhoneSubInfoUtil;->getDeviceIdLevel2(Landroid/content/Context;)Ljava/lang/String;
	       Lcom/netease/deviceid/ISettingUtils;->getAndroidPropertyLevel1(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;
	       Lcom/netease/deviceid/ITelephonyUtil;->getDeviceIdLevel1(Landroid/content/Context;)Ljava/lang/String;
	       Lcom/netease/deviceid/ITelephonyUtil;->getDeviceIdLevel2(Landroid/content/Context;)Ljava/lang/String;
	
	#42: Reflection dark greylist Landroid/os/SystemProperties;->get use(s):
	
	       Lcom/alipay/b/a/a/a/a;->b(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
	       Lcom/alipay/b/a/a/b/b;->k()Ljava/lang/String;
	       Lcom/alipay/b/a/a/b/d;->a(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
	       Lcom/qiyukf/unicorn/ui/activity/a;->a(Landroid/content/Context;)Z
	       Lcom/ta/utdid2/a/a/g;->get(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
	       Lcom/netease/yanxuan/common/util/rom/EmuiRom;->getEmuiVersion()Ljava/lang/String;
	       Lcom/huawei/hms/a/a;->a(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
	       Lcom/huawei/hms/update/f/a;->a(Ljava/lang/String;)Ljava/lang/String;
	       Lcom/netease/epay/basic/bar/OSUtils;->getSystemProperty(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
	       Lcom/netease/epay/sdk/base/util/SystemBarTintManager;-><clinit>()V
	       Lcom/netease/epay/sdk/creditpay/c;->a(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
	       Lcom/netease/epay/verifysdk/f/o;->a(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
	       Lcom/netease/libs/permissioncompat/rom/RomBase;->getProp(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
	       Lcom/netease/libs/yxcommonbase/base/SystemUtil;->getSystemProperty(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
	       Lcom/sina/weibo/sdk/utils/AidTask;->getSerialNo()Ljava/lang/String;
	       Lcom/tencent/tinker/loader/shareutil/ShareTinkerInternals;->isVmJitInternal()Z
	       Lcom/unionpay/mobile/android/utils/f;->e()Ljava/lang/String;
	       Lcom/unionpay/sdk/r;->a()Lcom/unionpay/sdk/m$g;
	       Lcom/unionpay/utils/e;->a()Ljava/lang/String;
	
	#43: Reflection dark greylist Landroid/os/SystemProperties;->getInt use(s):
	       Lcom/huawei/hms/a/a;->a(Ljava/lang/String;I)I
	
	#44: Reflection light greylist Landroid/os/UserHandle;->getUserId use(s):
	       Lcom/netease/deviceid/ISettingUtils;->getAndroidProperty(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;
	       Lcom/netease/deviceid/ISettingUtils;->getAndroidPropertyLevel1(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;
	
	#45: Reflection light greylist Landroid/os/storage/StorageManager;->getVolumeList use(s):
	       Lcom/huawei/updatesdk/support/b/c;->e(Landroid/content/Context;)Ljava/lang/String;
	
	#46: Reflection light greylist Landroid/os/storage/StorageVolume;->getPath use(s):
	       Lcom/huawei/updatesdk/support/b/c;->d()Ljava/lang/reflect/Method;
	
	#47: Reflection dark greylist Landroid/provider/Settings$Global;->MOVED_TO_SECURE use(s):
	       Lcom/netease/deviceid/ISettingUtils;->getAndroidPropertyLevel1(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;
	
	#48: Reflection dark greylist Landroid/provider/Settings$Secure;->MOVED_TO_GLOBAL use(s):
	       Lcom/netease/deviceid/ISettingUtils;->getAndroidPropertyLevel1(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;
	
	#49: Reflection dark greylist Landroid/provider/Settings$Secure;->MOVED_TO_LOCK_SETTINGS use(s):
	       Lcom/netease/deviceid/ISettingUtils;->getAndroidPropertyLevel1(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;
	
	#50: Reflection light greylist Landroid/provider/Settings$Secure;->getStringForUser use(s):
	       Lcom/netease/deviceid/ISettingUtils;->getAndroidProperty(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;
	
	#51: Reflection light greylist Landroid/provider/Settings$Secure;->sNameValueCache use(s):
	       Lcom/netease/deviceid/ISettingUtils;->getAndroidPropertyLevel1(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;
	
	#52: Reflection light greylist Landroid/service/media/MediaBrowserService$Result;->mFlags use(s):
	       Landroid/support/v4/media/MediaBrowserServiceCompatApi24;-><clinit>()V
	
	#53: Reflection light greylist Landroid/telephony/TelephonyManager;->getDefault use(s):
	       Lcom/huawei/hms/update/f/a/c;->e()I
	       Lcom/unionpay/sdk/f;->B(Landroid/content/Context;)Lorg/json/JSONArray;
	
	#54: Reflection light greylist Landroid/telephony/TelephonyManager;->getITelephony use(s):
	       Lcom/netease/deviceid/ITelephonyUtil;->getDeviceIdLevel0(Landroid/content/Context;)Ljava/lang/String;
	
	#55: Reflection light greylist Landroid/telephony/TelephonyManager;->getSubscriberInfo use(s):
	       Lcom/netease/deviceid/IPhoneSubInfoUtil;->getDeviceIdLevel0(Landroid/content/Context;)Ljava/lang/String;
	
	#56: Reflection light greylist Landroid/view/LayoutInflater;->mFactory use(s):
	       Lcom/netease/yanxuan/yxskin/hooker/SkinActivityDelegate;->hookFactory(Landroid/app/Activity;Lcom/netease/yanxuan/yxskin/hooker/BaseSkinHooker$IViewAddedCallback;)V
	       Lcom/netease/yanxuan/yxskin/hooker/SkinActivityDelegate;->hookFactory(Landroid/app/Activity;Lcom/netease/yanxuan/yxskin/hooker/BaseSkinHooker$IViewAddedCallback;)V
	
	#57: Reflection light greylist Landroid/view/LayoutInflater;->mFactory2 use(s):
	       Lcom/netease/yanxuan/yxskin/hooker/SkinActivityDelegate;->hookFactory(Landroid/app/Activity;Lcom/netease/yanxuan/yxskin/hooker/BaseSkinHooker$IViewAddedCallback;)V
	       Lcom/netease/yanxuan/yxskin/hooker/SkinActivityDelegate;->hookFactory(Landroid/app/Activity;Lcom/netease/yanxuan/yxskin/hooker/BaseSkinHooker$IViewAddedCallback;)V
	       Landroid/support/v4/view/LayoutInflaterCompat;->forceSetFactory2(Landroid/view/LayoutInflater;Landroid/view/LayoutInflater$Factory2;)V
	
	#58: Reflection light greylist Landroid/view/View;->computeFitSystemWindows use(s):
	       Landroid/support/v7/internal/widget/ViewUtils;-><clinit>()V
	
	#59: Reflection light greylist Landroid/view/View;->mAccessibilityDelegate use(s):
	       Landroid/support/v4/view/ViewCompat$ViewCompatBaseImpl;->hasAccessibilityDelegate(Landroid/view/View;)Z
	
	#60: Reflection light greylist Landroid/view/View;->mMinHeight use(s):
	       Landroid/support/v4/view/ViewCompat$ViewCompatBaseImpl;->getMinimumHeight(Landroid/view/View;)I
	
	#61: Reflection light greylist Landroid/view/View;->mMinWidth use(s):
	       Landroid/support/v4/view/ViewCompat$ViewCompatBaseImpl;->getMinimumWidth(Landroid/view/View;)I
	
	#62: Reflection light greylist Landroid/view/View;->mRecreateDisplayList use(s):
	       Landroid/support/v4/widget/SlidingPaneLayout$SlidingPanelLayoutImplJB;-><init>()V
	
	#63: Reflection light greylist Landroid/view/animation/Animation;->mListener use(s):
	       Landroid/support/v4/app/FragmentManagerImpl;->getAnimationListener(Landroid/view/animation/Animation;)Landroid/view/animation/Animation$AnimationListener;
	
	#64: Reflection light greylist Landroid/view/inputmethod/InputMethodManager;->showSoftInputUnchecked use(s):
	       Landroid/support/v7/widget/SearchView$AutoCompleteTextViewReflector;-><init>()V
	
	#65: Reflection light greylist Landroid/widget/AbsListView;->mIsChildViewEnabled use(s):
	       Landroid/support/v7/internal/widget/ListViewCompat;-><init>(Landroid/content/Context;Landroid/util/AttributeSet;I)V
	
	#66: Reflection light greylist Landroid/widget/AbsListView;->performLongPress use(s):
	       Lcom/netease/hearttouch/htrecycleview/bga/BGASwipeItemLayout;->performAdapterViewItemLongClick()Z
	
	#67: Reflection light greylist Landroid/widget/AutoCompleteTextView;->doAfterTextChanged use(s):
	       Landroid/support/v7/widget/SearchView$AutoCompleteTextViewReflector;-><init>()V
	
	#68: Reflection light greylist Landroid/widget/AutoCompleteTextView;->doBeforeTextChanged use(s):
	       Landroid/support/v7/widget/SearchView$AutoCompleteTextViewReflector;-><init>()V
	
	#69: Reflection light greylist Landroid/widget/AutoCompleteTextView;->ensureImeVisible use(s):
	       Landroid/support/v7/widget/SearchView$AutoCompleteTextViewReflector;-><init>()V
	
	#70: Reflection light greylist Landroid/widget/CompoundButton;->mButtonDrawable use(s):
	       Landroid/support/v4/widget/CompoundButtonCompat$CompoundButtonCompatBaseImpl;->getButtonDrawable(Landroid/widget/CompoundButton;)Landroid/graphics/drawable/Drawable;
	
	#71: Reflection light greylist Landroid/widget/PopupWindow;->mAnchor use(s):
	       Landroid/support/v7/internal/widget/AppCompatPopupWindow;->wrapOnScrollChangedListener(Landroid/widget/PopupWindow;)V
	
	#72: Reflection light greylist Landroid/widget/PopupWindow;->mOnScrollChangedListener use(s):
	       Landroid/support/v7/internal/widget/AppCompatPopupWindow;->wrapOnScrollChangedListener(Landroid/widget/PopupWindow;)V
	
	#73: Reflection light greylist Landroid/widget/PopupWindow;->mOverlapAnchor use(s):
	       Landroid/support/v4/widget/PopupWindowCompat$PopupWindowCompatApi21Impl;-><clinit>()V
	
	#74: Reflection light greylist Landroid/widget/PopupWindow;->setClipToScreenEnabled use(s):
	       Landroid/support/v7/widget/ListPopupWindow;-><clinit>()V
	
	#75: Reflection light greylist Landroid/widget/TextView;->mMaxMode use(s):
	       Landroid/support/v4/widget/TextViewCompat$TextViewCompatBaseImpl;->getMaxLines(Landroid/widget/TextView;)I
	
	#76: Reflection light greylist Landroid/widget/TextView;->mMaximum use(s):
	       Landroid/support/v4/widget/TextViewCompat$TextViewCompatBaseImpl;->getMaxLines(Landroid/widget/TextView;)I
	       Landroid/support/v4/widget/TextViewCompat$TextViewCompatBaseImpl;->getMaxLines(Landroid/widget/TextView;)I
	
	#77: Reflection dark greylist Landroid/widget/TextView;->mMinMode use(s):
	       Landroid/support/v4/widget/TextViewCompat$TextViewCompatBaseImpl;->getMinLines(Landroid/widget/TextView;)I
	
	#78: Reflection light greylist Landroid/widget/TextView;->mMinimum use(s):
	       Landroid/support/v4/widget/TextViewCompat$TextViewCompatBaseImpl;->getMinLines(Landroid/widget/TextView;)I
	       Landroid/support/v4/widget/TextViewCompat$TextViewCompatBaseImpl;->getMinLines(Landroid/widget/TextView;)I
	
	#79: Reflection light greylist Lcom/android/internal/R$dimen;->status_bar_height use(s):
	       Lcom/netease/yanxuan/common/util/ScreenUtil;->getStatusBarHeight(Landroid/content/Context;)I
	
	#80: Reflection light greylist Lcom/android/internal/telephony/IPhoneSubInfo$Stub;->asInterface use(s):
	       Lcom/netease/deviceid/IPhoneSubInfoUtil;->getDeviceIdLevel1(Landroid/content/Context;)Ljava/lang/String;
	       Lcom/netease/deviceid/IPhoneSubInfoUtil;->getDeviceIdLevel2(Landroid/content/Context;)Ljava/lang/String;
	
	#81: Reflection light greylist Lcom/android/internal/telephony/ITelephony$Stub;->asInterface use(s):
	       Lcom/netease/deviceid/ITelephonyUtil;->getDeviceIdLevel1(Landroid/content/Context;)Ljava/lang/String;
	       Lcom/netease/deviceid/ITelephonyUtil;->getDeviceIdLevel2(Landroid/content/Context;)Ljava/lang/String;
	
	#82: Reflection light greylist Lcom/android/internal/widget/ILockSettings$Stub;->asInterface use(s):
	       Lcom/netease/deviceid/ISettingUtils;->getAndroidPropertyLevel1(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;
	
	#83: Reflection light greylist Ldalvik/system/CloseGuard;->get use(s):
	       Lokhttp3/internal/platform/AndroidPlatform$CloseGuard;->get()Lokhttp3/internal/platform/AndroidPlatform$CloseGuard;
	
	#84: Reflection light greylist Ldalvik/system/CloseGuard;->open use(s):
	       Lokhttp3/internal/platform/AndroidPlatform$CloseGuard;->get()Lokhttp3/internal/platform/AndroidPlatform$CloseGuard;
	
	#85: Reflection light greylist Ldalvik/system/CloseGuard;->warnIfOpen use(s):
	       Lokhttp3/internal/platform/AndroidPlatform$CloseGuard;->get()Lokhttp3/internal/platform/AndroidPlatform$CloseGuard;
	
	#86: Reflection light greylist Ldalvik/system/VMRuntime;->getCurrentInstructionSet use(s):
	       Lcom/tencent/tinker/loader/shareutil/ShareTinkerInternals;->getCurrentInstructionSet()Ljava/lang/String;
	
	#87: Reflection light greylist Ljava/lang/Thread;->parkBlocker use(s):
	       Lio/netty/util/internal/chmv8/ForkJoinPool;-><clinit>()V
	
	#88: Reflection light greylist Ljava/nio/Buffer;->address use(s):
	       Lio/netty/util/internal/PlatformDependent0;-><clinit>()V
	
	#89: Reflection light greylist Lsun/misc/Unsafe;->theUnsafe use(s):
	       Lcom/google/gson/internal/UnsafeAllocator;->create()Lcom/google/gson/internal/UnsafeAllocator;
	
	89 hidden API(s) used: 6 linked against, 83 through reflection
	


