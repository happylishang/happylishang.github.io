## 背景：

自 2020 年 3 月 12 日起，Galaxy Store 中的服务要求应用必须能支持 64 位架构。注册新应用程序或更新现有应用程序时，您必须提供支持 64 位架构的 apk。目前严选仅支持32位，64位均运行在兼容模式下，这也是目前主流的做法。但是随着市场的强制要求，不得不添加对64位的支持。


## 任务

* 补齐所有32位对应的64位so
* 兼容性测试回归

## 具体涉及模块

![image.png](https://upload-images.jianshu.io/upload_images/1460468-45b137ecb21078e0.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

主要需要补全64so的地方

![image.png](https://upload-images.jianshu.io/upload_images/1460468-9bfb39e5eafb38fe.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

需要用ndk工具编译出64位so库，或者去第三方sdk中去寻找。

## 重点注意问题
 
* 可信ID模拟器侦测so库

这个库是用ARM汇编编写的SMC代码，32位的跟64位的指令不同，需要重新开发，编译，反编译出可执行代码，生成64位so库，具体效果需要线上验证。

* Weex so库

目前版本的Weex不提供64位的so库，需要升级weex sdk版本，但是很多API都已改变，需要适配。

weex render只能在UI线程

[https://weex.apache.org/zh/download/major_change.html#_0-28](https://weex.apache.org/zh/download/major_change.html#_0-28)

需要在它们的 App 中内置 JavaScript 引擎，否则 Weex 将无法运行。对于不知道如何选择 JavaScript 用户的引擎，可以在 App 的 build.gradle 中引入下述脚本：

     apply from: 'https://raw.githubusercontent.com/apache/incubator-weex/release/0.28/android/sdk/buildSrc/download_jsc.gradle'


## 测试流程

理论上所有用到so库的功能都应该测试，尤其是之前64位so库没有提供的地方

* libyx_tracepath.so  ：网络诊断* libweibosdkcore.so  微博分享登录* libuptsmaddonmi.so  * libursandroidcore.so* libstidinteractive_liveness.so* libstidocr_stream_jni.so* libstidocr_stream.so* libuptsmaddon.so* libnative-filters.so* libnetsecsdk-3.3.1.so* libproperty_get.so* libstatic-webp.so* libgifimage.so* libimagepipeline.so* libjni_liveness_interactive.so* libmmkv.so* libCryptoSeed.so* libCtaApiLib.so* libemulator_check.so* libentryexpro.so* libfacial_action.so* libalicomphonenumberauthsdk-log-release_alijtca_plus.so* libApkPatchUtil.so* libc++_shared.so* libcore.so* libcpp-share-id.so* libcrashlytics.so* libA3AEECD8.so

增加64位so之后，包会变大。有如下两种方案

* 同时包含32及64位，包会增大
* 分开打32及64，上传市场分开，会增加上传的工作量，64的包是禁止32位安装的，需要市场支持

## 打包上架流程

 目前只有三星需要提供64的市场包，为了稳定性，目前只提供这一家，稳定性测试通过之后，可以扩展到其他市场。
 
*  一期：分开打包，分开上传，利用三星市场跑线上测试
*  二期：统一打包上传，后期扩展，避免其他市场产生同样问题

## 人力成本

* so库补全 ：一人3到4天（ndk编译其他sdk so补全）
* weex升级：一人3到4天
* 可信ID arm汇编so库支持64位：一周
* 测试回归：功能+稳定性（测试平台跑稳定性、兼容性）
* 上线评估：4月份中到下旬


