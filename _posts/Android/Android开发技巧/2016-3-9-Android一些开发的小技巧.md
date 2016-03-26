---
layout: post
title: "Android一些开发的小技巧"
description: "Java"
category: android开发

---

> [判断一个Activity的Application是否在运行](#anchor_activity_is_runing)   
> [Android 6.0 Apache HTTP Client Removal](#COMPILE_SDK_VERSION_23)  
> [Android 使用android-support-multidex解决Dex超出方法数的限制问题,让你的应用不再爆棚](#android-support-multidex_65535)    
> [加速Android Studio的Gradle构建速度](#gradle_speeding)
 <a name="gradle_speeding"></>
####加速Android Studio的Gradle构建速度
 
	org.gradle.daemon=true
	org.gradle.jvmargs=-Xmx2048m -XX:MaxPermSize=512m -XX:+HeapDumpOnOutOfMemoryError -Dfile.encoding=UTF-8
	org.gradle.parallel=true
	org.gradle.configureondemand=true
 
 <img src="http://139.129.6.122/wp-content/uploads/2015/09/speedup_gradle_2.png" width="800">
 

	# Project-wide Gradle settings.
	
	# IDE (e.g. Android Studio) users:
	# Settings specified in this file will override any Gradle settings
	# configured through the IDE.
	
	# For more details on how to configure your build environment visit
	# http://www.gradle.org/docs/current/userguide/build_environment.html
	
	# The Gradle daemon aims to improve the startup and execution time of Gradle.
	# When set to true the Gradle daemon is to run the build.
	# TODO: disable daemon on CI, since builds should be clean and reliable on servers
	org.gradle.daemon=true
	
	# Specifies the JVM arguments used for the daemon process.
	# The setting is particularly useful for tweaking memory settings.
	# Default value: -Xmx10248m -XX:MaxPermSize=256m
	org.gradle.jvmargs=-Xmx2048m -XX:MaxPermSize=512m -XX:+HeapDumpOnOutOfMemoryError -Dfile.encoding=UTF-8
	
	# When configured, Gradle will run in incubating parallel mode.
	# This option should only be used with decoupled projects. More details, visit
	# http://www.gradle.org/docs/current/userguide/multi_project_builds.html#sec:decoupled_projects
	org.gradle.parallel=true
	
	# Enables new incubating mode that makes Gradle selective when configuring projects. 
	# Only relevant projects are configured which results in faster builds for large multi-projects.
	# http://www.gradle.org/docs/current/userguide/multi_project_builds.html#sec:configuration_on_demand
	org.gradle.configureondemand=true

#### Looper.loop()会创建一个常驻线程除非自己主动结束

 Looper.loop(); 让Looper开始工作，从消息队列里取消息，处理消息。 注意：写在Looper.loop()之后的代码不会被执行，这个函数内部应该是一个循环，当调用mHandler.getLooper().quit()后，loop才会中止，其后的代码才能得以运行。


 <a name="android-support-multidex_65535"></a>
#### Android 使用android-support-multidex解决Dex超出方法数的限制问题,让你的应用不再爆棚

[参考文档](http://blog.csdn.net/t12x3456/article/details/40837287) 

[使用方法](http://www.infoq.com/cn/news/2014/11/android-multidex)

 1.修改Gradle配置文件，启用MultiDex并包含MultiDex支持：

android { compileSdkVersion 21 buildToolsVersion "21.1.0"

defaultConfig {
    ...
    minSdkVersion 14
    targetSdkVersion 21
    ...

    // Enabling multidex support.
    multiDexEnabled true
}
...
}

dependencies { compile 'com.android.support:multidex:1.0.0' } 
 让应用支持多DEX文件。在MultiDexApplication JavaDoc中描述了三种可选方法：

* 在AndroidManifest.xml的application中声明android.support.multidex.MultiDexApplication；

* 如果你已经有自己的Application类，让其继承MultiDexApplication；

* 如果你的Application类已经继承自其它类，你不想/能修改它，那么可以重写attachBaseContext()方法：

		@Override 
		protected void attachBaseContext(Context base) {
		    super.attachBaseContext(base); MultiDex.install(this);
		}





 
 <a name="COMPILE_SDK_VERSION_23"></a>
 
#### Android 6.0 Apache HTTP Client Removal

COMPILE_SDK_VERSION=23引入的问题

Android 6.0 release removes support for the Apache HTTP client. If your app is using this client and targets Android 2.3 (API level 9) or higher, use the HttpURLConnection class instead. This API is more efficient because it reduces network use through transparent compression and response caching, and minimizes power consumption. To continue using the Apache HTTP APIs, you must first declare the following compile-time dependency in your build.gradle file:

	android {
	    useLibrary 'org.apache.http.legacy'
	}

<img  src="../../../images/android/kits/apache_removal.png" width="800"/>


####  minSdkVersion targetSdkVersion maxSdkVersion 

build target并不存在于manifest文件中，而是存在于项目根目录中的project.properties文件中。如果使用Eclipse构建项目的话，那么每个项目的根目录下都会有一个project.properties文件，这个文件中的内容用于告诉构建系统，怎样构建这个项目。打开这个文件，除了注释之外，还有以下一行：
target=android-18 这句话就是指明build target，也就是根据哪个android平台构架这个项目。指明build target为android-18，就是使用sdk中platforms目录下android-18目录中的android.jar这个jar包编译项目。同样，这个android.jar会被加入到本项目的build path中。如下图：

<img src="http://img.blog.csdn.net/20140122174131609?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQvemhhbmdqZ19ibG9n/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast" />

每当修改了build target，就会将另一个android.jar加入到build path中替换原来的jar。将build target改成android-17后的效果如下图：

<img src="http://img.blog.csdn.net/20140122174424468?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQvemhhbmdqZ19ibG9n/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast"/>

一般情况下，应该使用最新的API level作为build target,不过不太好吧

#### Android gradle: buildtoolsVersion vs compileSdkVersion ...

At IO 2014, we release API 20 and build-tools 20.0.0 to go with it.You can use a higher version of the build-tools than your compileSdkVersion, in order to pick up new/better compiler while not changing what you build your app against.

* CompileSdkVersion是你SDK的版本号，也就是API Level，例如API-19、API-20、API-21等等。
 
* buildeToolVersion是你构建工具的版本，其中包括了打包工具aapt、dx等等。这个工具的目录位于..your_sdk_path/build-tools/XX.XX.XX这个版本号一般是API-LEVEL.0.0。 例如I/O2014大会上发布了API20对应的build-tool的版本就是20.0.0在这之间可能有小版本，例如20.0.1等等。
 
*  build-tools 里面是不同版本(例如21.1.1)的build工具，这些工具包括了aapt打包工具、dx.bat、aidl.exe等等
 
*  platform 是存放不同API-level版本SDK目录的地方
 
* platform-tools 是一些android平台相关的工具，adb、fastboot等
 
* tools是指的安卓开发相关的工具，例如android.bat、ddms.bat(Dalvik debug Monitor Service)、draw9patch.bat等等

#### MAC下显示隐藏代码

	defaults write ~/Library/Preferences/com.apple.finder AppleShowAllFiles -bool true  显示代码

	defaults write ~/Library/Preferences/com.apple.finder AppleShowAllFiles -bool false 隐藏代码


<a name="anchor_activity_is_runing"></a>

#### 判断一个Activity的Application是否在运行

由于ActivityManager.getRunningTasks在5.0 lolip之后废弃了，如何判断一个Activity的App是否在运行。

**参考文档**[ActivityManager.getRunningTasks is deprecated android](http://stackoverflow.com/questions/31156313/activitymanager-getrunningtasks-is-deprecated-android)

	/***
	 * Checking Whether any Activity of Application is running or not
	 * @param context
	 * @return
	 */
	 
	public static boolean isForeground(Context context) {
	
	    // Get the Activity Manager
	    ActivityManager manager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
	
	    // Get a list of running tasks, we are only interested in the last one,
	    // the top most so we give a 1 as parameter so we only get the topmost.
	    List<ActivityManager.RunningAppProcessInfo> task = manager.getRunningAppProcesses();
	
	    // Get the info we need for comparison.
	    ComponentName componentInfo = task.get(0).importanceReasonComponent;
	
	    // Check if it matches our package name.
	    if(componentInfo.getPackageName().equals(context.getPackageName()))
	        return true;
	
	    // If not then our app is not on the foreground.
	    return false;
	}
	
 		
