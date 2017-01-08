---
layout: post
title: "Android热补丁动态修复技术"
description: "android"
category: android
tags: [android]

---


## Android热补丁动态修复技术

#### 使用场景

App发布之后，如果出现了严重的线上BUG，传统的做法是重新打包、测试、上线，可能代码改动很小，但是每次付出的代价是巨大的，有没有办法以补丁的方式动态修复紧急Bug，不再需要重新发布App，不再需要用户重新下载，覆盖安装？向用户下发Patch，在用户无感知的情况下，修复BUG问题。

#### 解决方案

大概有两种实现原理，

* 一种是阿里Andfix为代表的方案，在方法级别修复，将存在bug的Java类的方法修改为Native方法，立即生效不用重启。

* 第二种是ClassLoader基于mutidex的实现方式，本文瞄准Nuwa，Android支持多个dex，但是在查找类的时候，有一个有限返回的说法，也就是如果某个类在前面的dex中找到就不会去后面的去寻找。可以把修复好的类放到前面的dex里面，这样就避免了调用后面的类。

#### AndFix 坑

Application的onCreate里面处理AndFix相关的逻辑，一定要区分进程，因为如果你的app是多进程的，每个进程都会创建Application对象，导致你的补丁逻辑被重复执行。在内存层面看，补丁操作的影响只会局限在进程之内，似乎没有什么关系，但是如果你的补丁操作涉及到文件系统的操作，比如拷贝文件、删除文件、解压文件等等，那么进程之间就会相互影响了。我们遇到的问题就是在主进程里面下载好的补丁包会莫名其妙地不见，主进程下载好补丁包后，信鸽进程被启动，创建Application对象，执行补丁逻辑，把刚刚主进程下载好的补丁包应用了，然后又把补丁包删除
 
####  如果使用ClassLoader来动态升级APP或者动态修复BUG，都需要重新启动APP才能生效。

ClassLoader特性：使用ClassLoader的一个特点就是，当ClassLoader在成功加载某个类之后，会把得到类的实例缓存起来。下次再请求加载该类的时候，ClassLoader会直接使用缓存的类的实例，而不会尝试再次加载。也就是说，如果程序不重新启动，加载过一次的类就无法重新加载。
 
 
####  NUWA 女娲

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



#### Andfix修复注意事项

* 代码混淆

	-keep class * extends java.lang.annotation.Annotation
	-keepclasseswithmembernames class * {
	    native <methods>;
	}
	-keep class com.alipay.euler.andfix.** {*;}

* 在合适的地方将下载的包加入并加载，加入为了意外删除也可以用

                    patchManager.addPatch(path);//path of the patch file that was downloaded
                    patchManager.loadPatch();
                    
下载patch后修复OK，然后删除patch，重新打开app还是修复好的逻辑，下载成功后，addpatch()方法会把patch拷贝到/data/data/pkgname/file/下，就算删掉也无所谓了，

在Application内部

	oncreate{
	        patchManager = new PatchManager(this);
	        patchManager.init(getAppVersion());//current version
	        patchManager.loadPatch();
	        }
	        
自然每次都要加载
打差分包

	 ./apkpatch.sh -f app-release2.apk -t app-release.apk -k /Users/personal/prj/TestApplication/keystore.jks -p lishang2011 -a keystore -e lishang2011 -o ./
 
AndFix的客户端使用流程 1.客户端在每次启动的时候，可以请求接口，判断是否需要去获取patch文件，如果需要获取，则直接下载patch文件到sd卡，下载好之后将文件名改为我们自己定义的文件名。（防止文件没有下载成功就进行patch加载会导致客户端崩溃）当patch加载成功后，程序会删除掉已经加载成功的patch文件。在sd卡上不会留下patch文件。

通过启动apk的时候，对是否下载哪些patch文件进行校验，然后再下载patch。
问题：（1)这里需要验证是否已经下载过改版本的patch，如果下载过，就不进行下载 （2)还需要考虑1、2同时下载时的线程安全。（如果不通过push进行更新，则可以不需要考虑，这里考虑到后续还有很多问题，所以暂时不建议做push更新）

patch的使用方式，经过测试，得出patch文件是增量更新的，更新的方式如下： 如果有3个补丁分别为ABC，那么APK如果想进行补丁升级，要先打补丁A，再打补丁B，最后打补丁C，以此类推。所以这里不能进行跨版本升级，因为在更新补丁的时候，没办法删除之前的补丁，删除补丁需要更随APK的升级而升级补丁的版本号，比如：APK 从1.0升级到1.1，那么在1.1这个版本会删除掉之前1.0所有的补丁。

 考虑到服务器拿patch的流程，这里提出2个方案去实施：
方案1、每次启动都去依次获取该版本的补丁，直至最新的补丁。比如1.0的版本，发了3个补丁包，那么用户有可能会在出现4种情况，没有补丁，已有补丁A，已有补丁AB，和已有补丁ABC。 （1）如果是没有补丁，就依次去获取补丁ABC。 （2）如果已有补丁A，就需要依次去下载补丁BC。 （3）如果已有补丁AB，就需要去下载补丁C。 （4）如果已经有补丁ABC，就不需要去下载。 在每个补丁内，都需要带版本号，每次去请求网络的时候，都带上版本号，让服务器去返回下载地址。（如果是1.0发了3个补丁，1.1发了2个补丁，那么服务器只能返回1.0的3个补丁，1.1的补丁不应该返回）

方案2、规定每个小版本都只发一个补丁，这样就不需要考虑判断版本号的问题。

AndFix使用注意事项 

（1）不支持YunOS
（2）无法添加新类和新的字段 
（3）使用加固平台可能会使热补丁功能失效 
（4）由于这里涉及到Android底层系统的替换，所以需要更多的机型进行测试，以防出现像YunOS（阿里云）这样的系统。 AndFix的Android客户端部署 Android客户端的部署可以参考，下面两个链接：
 （1）http://www.jianshu.com/p/479b8c7ec3e3 
 （2）http://blog.csdn.net/yaya_soft/article/details/50460102

测试的注意事项 （1）如果是选择方案1，那么需要关注多个patch包是否依次加载成功，是否会出现漏包的情况。 （2）需要考虑分版本加载的情况 （3）兼容性测试，看看在不同机型上，是否会出现问题（这里建议使用更多的机型进行测试） （4）需要考虑在多个patch包的情形下，已下载了前面几个，然后下载失败，当再次启动时，会不会继续下载新的patch包。


* 如何处理下载与删除并防止重复下载
* 如何处理多个热更新包
	
	
	    /**
	     * 初始化AndFix
	     */
	    private void initAndFix() {
	        // initialize
	        mPatchManager = new PatchManager(this);
	        String versionName = EgmUtil.getNumberVersion(this);
	        mPatchManager.init(versionName);
	        //获取本地的版本号
	        String version = EgmPrefHelper.getAndFixVersion(this);
	        String pathVersion[] = version.split("_");
	        int patchVer = 0;
	        if(pathVersion!=null&&pathVersion.length>1){
	            patchVer =  Integer.valueOf(pathVersion[1]);
	        }
	        //获取网络上的patch版本号
	        ArrayList<PatchInfo> mPatchInfosUrl = new ArrayList<PatchInfo>();
	        //自己创建的patchinfo的列表，用于存储需要下载的patch信息
	        ArrayList<PatchInfo> mPatchInfos = new ArrayList<PatchInfo>();
	        if(mPatchInfosUrl!=null){
	            for(int i=0;i<mPatchInfosUrl.size();i++){
	                PatchInfo patchInfo =  mPatchInfosUrl.get(i);
	                String name = patchInfo.name;
	                if(name!=null){
	                    int urlVersion = getPatchVersion(name);
	                    //比较本地的版本号和网络上的patch版本号，如果网络的版本号大于本地的版本号就添加
	                    if(patchVer<urlVersion){
	                        mPatchInfos.add(patchInfo);
	                    }
	                }
	            }
	        }
	
	        if(mPatchInfos!=null&&mPatchInfos.size()>0){
	            downloadFile(mPatchInfos);
	            }
	        }
	
	    private int getPatchVersion(String patchName){
	        int index = patchName.lastIndexOf("_");
	        String patchStr = patchName.substring(index, patchName.length());
	        int pathVer =  Integer.valueOf(patchStr);
	        return pathVer;
	    }
	
	    /**
	     * 下载完成之后加载补丁
	     */
	    private void addPatch(PatchInfo patchInfo) {
	        // load patch
	        mPatchManager.loadPatch();
	        // add patch at runtime
	        try {
	            // .apatch file path
	            String patchFileString = Environment.getExternalStorageDirectory()
	                    .getAbsolutePath() + APATCH_PATH;
	            mPatchManager.addPatch(patchFileString);
	
	            //复制且加载补丁成功后，删除下载的补丁
	            File f = new File(this.getFilesDir(), DIR + APATCH_PATH);
	            if (f.exists()) {
	                boolean result = new File(patchFileString).delete();
	                if (!result)
	                    LogUtil.i(TAG, patchFileString + " delete fail");
	            }
	            //获取版本号
	            String versionName = EgmUtil.getNumberVersion(this);
	            //保存AndFix的版本号，在version版本号后面+"_"+0,1,2,3
	            int verPatch = getPatchVersion(patchInfo.name);
	            EgmPrefHelper.setAndFixVersion(this, versionName + "_" + verPatch);
	        } catch (IOException e) {
	            LogUtil.i(TAG, "", e);
	        }
	    }
	    private  int count = 0;
	    /**
	     * 增加下载文件的逻辑
	     * @param urls
	     */
	    private void downloadFile(final ArrayList<PatchInfo> patchInfos){
	        count++;
	        FileDownloadManager.getInstance().downloadFile(patchInfos.get(count-1).url,  DIR, APATCH_PATH,new FileDownloadListener() {
	            @Override
	            public void onSuccess(String path) {
	                if(patchInfos==null){
	                    return;
	                }
	               PatchInfo mPatchInfo =  patchInfos.get(count-1);
	                if(count!=patchInfos.size()&&count!=0){
	                    addPatch(mPatchInfo);
	                    downloadFile(patchInfos);
	                }else{
	                    addPatch(mPatchInfo);
	                    count = 0;
	                }
	            }
	            @Override
	            public void onProgress(long current, long total, int percent, int speed) {
	
	            }
	            @Override
	            public void onFailed(String err, int errCode) {
	                count--;
	            }
	        });
	    }
	



#### 参考文档：
[Android线上bug热修复分析 **精**](http://www.jianshu.com/p/9402bef0d905) 

[基于Nuwa实现Android自动化HotFix](http://www.jianshu.com/p/72c17fb76f21)

http://blog.csdn.net/qxs965266509/article/details/49816007
http://blog.csdn.net/qxs965266509/article/details/49821413


http://blog.zhaiyifan.cn/2015/11/20/HotPatchCompare/
http://my.oschina.net/853294317/blog/308583
安卓App热补丁动态修复技术介绍
http://blog.csdn.net/lmj623565791/article/details/49883661
https://github.com/dodola/HotFix
美团分包方案http://tech.meituan.com/mt-android-auto-split-dex.html
腾讯bugly博客http://bugly.qq.com/blog/?p=781
dex分包https://m.oschina.net/blog/308583
Android下的挂钩(hook)和代码注入(inject)