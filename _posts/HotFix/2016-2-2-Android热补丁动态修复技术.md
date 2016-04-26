---
layout: blog_content
title: "Android热补丁动态修复技术"
description: "android"
category: android
tags: [android]

---


## Android热补丁动态修复技术

###使用场景


   App发布之后，如果出现了严重的线上BUG，传统的做法是重新打包、测试、上线，可能代码改动很小，但是每次付出的代价是巨大的，有没有办法以补丁的方式动态修复紧急Bug，不再需要重新发布App，不再需要用户重新下载，覆盖安装？向用户下发Patch，在用户无感知的情况下，修复了外网问题，取得非常好的效果。

### 原理

该方案基于android dex分包方案，原理是将编译好的class文件拆分打包成两个dex，绕过dex方法数量的限制以及安装时的检查，在运行时再动态加载第二个dex文件中。
#### AndFix 原理
 

#### AndFix 坑

Application的onCreate里面处理AndFix相关的逻辑，一定要区分进程，因为如果你的app是多进程的，每个进程都会创建Application对象，导致你的补丁逻辑被重复执行。
在内存层面看，补丁操作的影响只会局限在进程之内，似乎没有什么关系，但是如果你的补丁操作涉及到文件系统的操作，比如拷贝文件、删除文件、解压文件等等，那么进程之间就会相互影响了。
我们遇到的问题就是在主进程里面下载好的补丁包会莫名其妙地不见，主进程下载好补丁包后，信鸽进程被启动，创建Application对象，执行补丁逻辑，把刚刚主进程下载好的补丁包应用了，然后又把补丁包删除
 
 
 
####  如果使用ClassLoader来动态升级APP或者动态修复BUG，都需要重新启动APP才能生效。

ClassLoader特性
使用ClassLoader的一个特点就是，当ClassLoader在成功加载某个类之后，会把得到类的实例缓存起来。下次再请求加载该类的时候，ClassLoader会直接使用缓存的类的实例，而不会尝试再次加载。也就是说，如果程序不重新启动，加载过一次的类就无法重新加载。
 
 
####  nuwa

* Support both dalvik and art runtime.
* Support productFlavor and buildType.
* Support proguard and multidex.
* Pure java implementation.

./gradlew clean nuwaDebugPatch -P NuwaDir=/Users/Documents/nuwa
./gradlew clean nuwaReleasePatch -P NuwaDir=/Users/personal/Documents/nuwa

##### 引入建议采用com.android.tools.build:gradle:1.2.3'，否则gradle有异常

Get Gradle Plugin

add following to the build.gradle of your root project.

	classpath 'cn.jiajixin.nuwa:gradle:1.2.2'
	
build.gradle maybe look like this:

	buildscript {
	    repositories {
	        jcenter()
	    }
	    dependencies {
	        classpath 'com.android.tools.build:gradle:1.2.3'
	        classpath 'cn.jiajixin.nuwa:gradle:1.2.2'
	    }
	}
	
add following to your build.gradle:

apply plugin: "cn.jiajixin.nuwa"
Get Nuwa SDK

	gradle dependency:
	
	dependencies {
	    compile 'cn.jiajixin.nuwa:nuwa:1.0.0'
	}
	
Use Nuwa SDK

add following to your application class:

	@Override
	protected void attachBaseContext(Context base) {
	    super.attachBaseContext(base);
	    Nuwa.init(this);
	}
	load the patch file according to your needs:
	
	Nuwa.loadPatch(this,patchFile)

对于多Dex测试时支持，因为自动分包，一般把我们的类是放在里面的


##### 打包
首先打一个有问题的包将相应分支的东西拷贝出去比如output/nuwa/

	/Users/personal/Documents/nuwa

打包无措的就可以了

	./gradlew aR
	./gradlew aR nuwaReleasePatch -P NuwaDir=/Users/personal/Documents/nuwa

之后就会生成jar

#### Mutidex\

手动分包


		afterEvaluate {
		    tasks.matching {
		        it.name.startsWith('dex')
		    }.each { dx ->
		        if (dx.additionalParameters == null) {
		            dx.additionalParameters = []
		        }
		        dx.additionalParameters += '--multi-dex'
		        dx.additionalParameters += '--set-max-idx-number=10000'
		        println("dx param = "+dx.additionalParameters)
		//        dx.additionalParameters += "--main-dex-list=$projectDir/multidex.keep".toString()
		    }
		}



1．Dex 拆分
根据前面对官方方案的研究总结，我们可以很快梳理出下面几个dex拆分步骤：
1）自动扫描整个工程代码得到 main-dex-list；
2）根据 main-dex-list 对整个工程编译后的所有 class 进行拆分，将主、从 dex 的 class 文件分开；
3）用 dx 工具对主、从 dex 的 class 文件分别打包成 .dex 文件，并放在 apk 的合适目录。

怎么自动生成 main-dex-list？
Android SDK 从 build tools 21 开始提供了 mainDexClasses 脚本来生成主 dex 的文件列表。查看这个脚本的源码，可以看到它主要做了下面两件事情：
1）调用 proguard 的 shrink 操作来生成一个临时 jar 包；
2）将生成的临时 jar 包和输入的文件集合作为参数，然后调用com.android.multidex.MainDexListBuilder 来生成主 dex 文件列表。

#### 加固问题



#### Andfix 问题


	-keep class * extends java.lang.annotation.Annotation
	-keepclasseswithmembernames class * {
	    native <methods>;
	}
	-keep class com.alipay.euler.andfix.** {*;}

在合适的地方

                    patchManager.addPatch(path);//path of the patch file that was downloaded
                    patchManager.loadPatch();
                    
下载patch后修复OK，然后删除patch，重新打开app还是修复好的逻辑，下载成功后，addpatch()方法会把patch拷贝到/data/data/pkgname/file/下，就算删掉也无所谓了，

在Application内部

	oncreate{
	        patchManager = new PatchManager(this);
	        patchManager.init(getAppVersion());//current version
	        patchManager.loadPatch();
	        }自然每次都要加载
#### 参考文档：
[Android线上bug热修复分析 **精**](http://www.jianshu.com/p/9402bef0d905) 

[基于Nuwa实现Android自动化HotFix](http://www.jianshu.com/p/72c17fb76f21)http://blog.csdn.net/qxs965266509/article/details/49816007http://blog.csdn.net/qxs965266509/article/details/49821413
http://blog.zhaiyifan.cn/2015/11/20/HotPatchCompare/
http://my.oschina.net/853294317/blog/308583
安卓App热补丁动态修复技术介绍
http://blog.csdn.net/lmj623565791/article/details/49883661
https://github.com/dodola/HotFix
美团分包方案http://tech.meituan.com/mt-android-auto-split-dex.html
腾讯bugly博客http://bugly.qq.com/blog/?p=781
dex分包https://m.oschina.net/blog/308583
Android下的挂钩(hook)和代码注入(inject)