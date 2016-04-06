---
layout: post
title: "Android一些开发的小技巧"
description: "Java"
category: android开发

---

> [判断一个Activity的Application是否在运行](#anchor_activity_is_runing)   
> [获取当前运行的顶层Activity](#anchor_top_activity_runing)   
> [Android 6.0 Apache HTTP Client Removal](#COMPILE_SDK_VERSION_23)  
> [Android 使用android-support-multidex解决Dex超出方法数的限制问题,让你的应用不再爆棚](#android-support-multidex_65535)    
> [加速Android Studio的Gradle构建速度](#gradle_speeding)    
> [Window Leaked窗体泄漏了Activity has leaked window](#leaked_window)     
> [MAC下显示隐藏代码](#mac_file_show_orhide)     
> [Android Webview回调单引号跟双引号的问题](#webview_js_callback)    




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

<a name="mac_file_show_orhide"></a>

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


<a name="anchor_top_activity_runing"></a>
 
#### Android获取当前Activity对象

背景：（当你不能使用this获取Activity对象）如何方便地当前Activity对象

思路：

* 维护一个Activity的対象栈，在每个Activity的生命手气方法执行的时候，控制add和remove，栈顶元素就是当前的Activity对象。为了代码的复用，这个操作可以
写在BaseActivity中.



* 使用反射来获取当前Activity对象。（个人认为是相对优雅和解耦的方式）

查看源码发现 Activity Thread 这个类管理着所有的Activity对象，也就持有所有的Activity对象，使用反射获得当前ActivityThread对象

，然后就能拿到当前的Activity对象

示例：
		
		public static Activity getCurrentActivity () {
		
		Class activityThreadClass = Class.forName("android.app.ActivityThread");
		
		Object activityThread = activityThreadClass.getMethod("currentActivityThread").invoke(null);
		
		Field activitiesField = activityThreadClass.getDeclaredField("mActivities");
		
		activitiesField.setAccessible(true);
		
		Map activities = (Map) activitiesField.get(activityThread);
		
		for (Object activityRecord : activities.values()) {
		
			Class activityRecordClass = activityRecord.getClass();
		
			Field pausedField = activityRecordClass.getDeclaredField("paused");
		
			pausedField.setAccessible(true);
		
		if (!pausedField.getBoolean(activityRecord)) {
		
			Field activityField = activityRecordClass.getDeclaredField("activity");
		
			activityField.setAccessible(true);
		
			Activity activity = (Activity) activityField.get(activityRecord);
		
			return activity;
		
			 }
		 
		  }
		
		return null;
		
		}

 
Having access to the current Activity is very handy. Wouldn’t it be nice to have a static getActivity method returning the current Activity with no unnecessary questions?

The Activity class is very useful. It gives access to the application’s UI thread, views, resources, and many more. Numerous methods require a Context, but how to get the pointer? Here are some ways:

Tracking the application’s state using overridden lifecycle methods. You have to store the current Activity in a static variable and you need access to the code of all Activities.
Tracking the application’s state using Instrumentation. Declare Instrumentation in the manifest, implement it and use its methods to track Activity changes. Passing an Activity pointer to methods and classes used in your Activities. Injecting the pointer using one of the code injection libraries. All of these approaches are rather inconvenient; fortunately, there is a much easier way to get the current Activity.
Seems like the system needs access to all Activities without the issues mentioned above. So, most likely there is a way to get Activities using only static calls. I spent a lot of time digging through the Android sources on grepcode.com, and I found what I was looking for. There is a class called ActivityThread. This class has access to all Activities and, what’s even better, has a static method for getting the current ActivityThread. There is only one little problem – the Activity list has package access.
Easy to solve using reflection:

	public static Activity getActivity() {
	    Class activityThreadClass = Class.forName("android.app.ActivityThread");
	    Object activityThread = activityThreadClass.getMethod("currentActivityThread").invoke(null);
	    Field activitiesField = activityThreadClass.getDeclaredField("mActivities");
	    activitiesField.setAccessible(true);
	    HashMap activities = (HashMap) activitiesField.get(activityThread);
	    for (Object activityRecord : activities.values()) {
	        Class activityRecordClass = activityRecord.getClass();
	        Field pausedField = activityRecordClass.getDeclaredField("paused");
	        pausedField.setAccessible(true);
	        if (!pausedField.getBoolean(activityRecord)) {
	            Field activityField = activityRecordClass.getDeclaredField("activity");
	            activityField.setAccessible(true);
	            Activity activity = (Activity) activityField.get(activityRecord);
	            return activity;
	        }
	    }
	}
Such a method can be used anywhere in the app and it’s much more convenient than all of the mentioned approaches. Moreover, it seems like it’s not as unsafe as it looks. It doesn’t introduce any new potential leaks or null pointers.

The above code snippet lacks exception handling and naively assumes that the first running Activity is the one we’re looking for. You might want to add some additional checks.

 
	

<a name="leaked_window"></a>

#### Window Leaked大概就是说一个窗体泄漏了 Activity com.photos.MainActivity has leaked window  
 		 
Android的每一个Activity都有个WindowManager窗体管理器，同样，构建在某个Activity之上的对话框、PopupWindow也有相应的WindowManager窗体管理器。因为对话框、PopupWindown不能脱离Activity而单独存在着，所以当某个Dialog或者某个PopupWindow正在显示的时候我们去finish()了承载该Dialog(或PopupWindow)的Activity时，就会抛Window Leaked异常了，因为这个Dialog(或PopupWindow)的WindowManager已经没有谁可以附属了，所以它的窗体管理器已经泄漏了。

**WMS会管理所有的Window，而Activity内部创建的Window属于Activity的子Window，如果不自己释放，就会导致窗口泄露。会一直被WMS保持着**

<a name="webview_js_callback"></a>

#### Android Webview回调单引号跟双引号的问题

**
对于字符串形式的参数，一定要记住使用单引号 ' 将其包裹，否则 JavaScript （可能）会无法解析这个字符串，提示未定义。 因为js会把未加单引号的字符串当做变量，而不是String常量'号，但是对于字符串要加，当然也比较灵活，只要字符串是'adfadfa',或者“adfa”的格式传递过去就可以！
**

#### ViewPager 设置自动循环的View的时候，初始postion不能太大，不然会滑动混乱

#### SparseArray代替HashMap<int object>

	private static final SparseArray<String> FROM_INFOS = new SparseArray<String>() 	{
	        {
	            put(R.id.xxx, "_stat_from=web_in_wxfriend");
	            put(R.id.xxx, "_stat_from=web_in_wxmoments");
	            put(R.id.xxx, "_stat_from=web_in_weibo");
	     
	        }
	    };
	    
#### onNewIntent的使用场景 

launchMode为singleTask的时候，通过Intent启到一个Activity,如果系统已经存在一个实例，系统就会将请求发送到这个实例上，但这个时候，系统就不会再调用通常情况下我们处理请求数据的onCreate方法，而是调用onNewIntent方法，这个只是作为一个增补，有的话没坏处，或者说只有好处如下所示:


	public void onCreate(Bundle savedInstanceState) {
	
	   super.onCreate(savedInstanceState);
	
	   setContentView(R.layout.main);
	
	   processExtraData();
	
	 }
	
	 protected void onNewIntent(Intent intent) {
	
	   super.onNewIntent(intent);
	
	   setIntent(intent);//must store the new intent unless getIntent() will return the old one
	
	   processExtraData()
	
	 }
	
	 private void processExtraData(){
	
	   Intent intent = getIntent();
	
	   //use the data received here
	
	 }	    