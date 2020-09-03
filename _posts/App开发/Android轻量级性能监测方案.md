
## App性能如何量化

如何衡量一个APP性能好坏？直观感受就是：启动快、流畅、不闪退、耗电少等感官指标，反应到技术层面包装下就是：FPS（帧率）、界面渲染速度、Crash率、网络、CPU使用率、电量损耗速度等，一般挑其中几个关键指标作为APP质量的标尺。目前也有多种开源APM监控方案，但大部分偏向离线检测，对于线上监测而言显得太重，可能会适得其反，方案简单对比如下：


   SDK     		|  现状与问题       | 是否推荐直接线上使用     |
--------------------|------------------|-----------------------|
腾讯matrix       | 功能全，但是重，而且运行测试期间经常Crash      | 否   |
腾讯GT       	   | 2018年之后没更新，关注度低，本身功能挺多，也挺重性价比还不如matrix  |否   |
网易Emmagee      | 2018年之后没更新，几乎没有关注度，重 |否        |
听云App          |  适合监测网络跟启动，场景受限  | 否   |

还有其他多种APM检测工具，功能复杂多样，但其实很多指标并不是特别重要，实现越复杂，线上风险越大，因此，并不建议直接使用。而且，分析多家APP的实现原理，其核心思路基本相同，且门槛也并不是特别高，建议自研一套，在灵活性、安全性上更有保障，更容易做到轻量级。本文主旨就是**围绕几个关键指标**：FPS、内存（内存泄漏）、界面启动、流量等，实现**轻量级**的线上监测。




## 核心性能指标拆解

* 稳定性：Crash统计

 Crash统计与聚合有比较通用的策略，比如Firebase、Bugly等，不在本文讨论范围
 
*  网络请求

每个APP的网络请求一般都存在统一的Hook点，门槛很低，且各家请求协议与SDK有别，很难实现统一的网络请求监测，其次，想要真正定位网络请求问题，可能牵扯整个请求的链路，更适合做一套网络全链路监控APM，也不在讨论范围。

* 冷启动时间及各个Activity页面启动时间 (存在统一方案)
* 页面FPS、卡顿、ANR    （存在统一方案）
* 内存统计及内存泄露侦测 （存在统一方案）
* 流量消耗   （存在统一方案）
* 电量   （存在统一方案）
* CPU使用率（CPU）：还没想好咋么用，7.0之后实现机制也变了，先不考虑

线上监测的重点就聚焦后面几个，下面逐个拆解如何实现。

## 启动耗时

界面启动从直观上说就是：从点击一个图标到看到下一个界面首帧，如果这个过程耗时较长，用户会会感受到顿挫，影响体验。从场景上说，启动耗时间简单分两种：

* 冷启动耗时：在APP未启动的情况从，从点击桌面icon 到看到闪屏Activity的首帧（非默认背景） 
* 界面启动耗：APP启动后，从上一个界面pause，到下一个界面首帧可见， 

本文粒度较粗，主要聚焦Activity，这里有个比较核心的时机：Activity首帧可见点，这个点究竟在什么时候？经分析测试发现，不同版本表现不一，在Android 10 之前这个点与onWindowFocusChanged回调点基本吻合，在Android  10 之后，系统做了优化，将首帧可见的时机提前到onWindowFocusChanged之前，可以简单看做onResume（或者onAttachedToWindow）之后，对于一开始点击icon的点，可以约等于APP进程启动的点，拿到了上面两个时间点，就可以得到冷启动耗时。

APP进程启动的点可以通过加载一个空的ContentProvider来记录，因为ContentProvider的加载时机比较靠前，早于Application的onCreate之前，相对更准确一点，很多SDK的初始也采用这种方式，实现如下：
	
	public class LauncherHelpProvider extends ContentProvider {
	
	    // 用来记录启动时间
	    public static long sStartUpTimeStamp = SystemClock.uptimeMillis();
	    ...
	    
	    }

这样就得到了冷启动的开始时间，如何得到第一个Activity界面可见的时间呢？比较简单的做法是在SplashActivity中进行打点，对于Android 10 以前的，可以在onWindowFocusChanged中打点，在Android 10以后，可以在onResume之后进行打点。不过，做SDK需要减少对业务的入侵，可以借助Applicattion监听Activity Lifecycle无入侵获取这个时间点。对于Android 10之前系统， 可以利用ViewTreeObserve监听nWindowFocusChange回调，达到无入侵获取onWindowFocusChanged调用点，示意代码如下

       application.registerActivityLifecycleCallbacks(new Application.ActivityLifecycleCallbacks() {
           ....
           @Override
        public void onActivityResumed(@NonNull final Activity activity) {
            super.onActivityResumed(activity);
            launcherFlag |= resumeFlag;
            
              <!--添加onWindowFocusChanged 监听-->
            	activity.getWindow().getDecorView().getViewTreeObserver().addOnWindowFocusChangeListener(new ViewTreeObserver.OnWindowFocusChangeListener() {
            	<!--onWindowFocusChanged回调-->
                @Override
                public void onWindowFocusChanged(boolean b) {
                    if (b && (launcherFlag ^ startFlag) == 0) {
                       <!--判断是不是首个Activity-->
                        final boolean isColdStarUp = ActivityStack.getInstance().getBottomActivity() == activity;
                        <!--获取首帧可见距离启动的时间-->
                        final long coldLauncherTime = SystemClock.uptimeMillis() - LauncherHelpProvider.sStartUpTimeStamp;
                        final long activityLauncherTime = SystemClock.uptimeMillis() - mActivityLauncherTimeStamp;
                        activity.getWindow().getDecorView().getViewTreeObserver().removeOnWindowFocusChangeListener(this);
                        <!--异步线程处理回调，减少UI线程负担-->
                        mHandler.post(new Runnable() {
                            @Override
                            public void run() {
                                if (isColdStarUp) {
                                //todo 监听到冷启动耗时
                                ...


对于Android 10以后的系统，可以在onActivityResumed回调时添加一UI线程Message来达到监听目的，代码如下

        @Override
        public void onActivityResumed(@NonNull final Activity activity) {
            super.onActivityResumed(activity);
            if (launcherFlag != 0 && (launcherFlag & resumeFlag) == 0) {
                launcherFlag |= resumeFlag;
                if (Build.VERSION.SDK_INT > Build.VERSION_CODES.P) {
                    //  10 之后有改动，第一帧可见提前了 可认为onActivityResumed之后
                    mUIHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            <!--获取第一帧可见时间点-->                        }
                    });
                }

如此就可以检测到冷启动耗时。APP启动后，各Activity启动耗时计算逻辑类似，首帧可见点沿用上面方案即可，不过这里还缺少上一个界面暂停的点，经分析测试，锚在上一个Actiivty pause的时候比较合理，因此Activity启动耗时定义如下：

	Activity启动耗时 = 当前Activity 首帧可见 - 上一个Activity onPause被调用

同样为了减轻对业务入侵，也依赖registerActivityLifecycleCallbacks来实现：补全上方缺失

       application.registerActivityLifecycleCallbacks(new Application.ActivityLifecycleCallbacks() {
	
		   @Override
	        public void onActivityPaused(@NonNull Activity activity) {
	            super.onActivityPaused(activity);
	            <!--记录上一个Activity pause节点-->
	            mActivityLauncherTimeStamp = SystemClock.uptimeMillis();
	            launcherFlag = 0;
	        }
	        ...
        @Override
        public void onActivityResumed(@NonNull final Activity activity) {
            super.onActivityResumed(activity);
            launcherFlag |= resumeFlag;
           <!--参考上面获取首帧的点-->
                 ...
 
到这里就获取了两个比较关键的启动耗时，不过，时机使用中可能存在各种异常场景：比如闪屏页在onCreate或者onResume中调用了finish跳转首页，对于这种场景就需要额外处理，比如在onCreate中调用了finish，onResume可能不会被调用，这个时候就要在 onCreate之后进行统计，同时利用用Activity.isFinishing()标识这种场景。其次，启动耗时对于不同配置也是不一样的，不能用绝对时间衡量，只能横向对比。

## FPS 

其实并非如此，举个例子，游戏玩家通常追求更流畅的游戏画面体验一般要达到 60FPS 以上，但我们平时看到的大部分电影或视频 FPS 其实不高，一般只有 25FPS ~ 30FPS，而实际上我们也没有觉得卡顿。 在人眼结构上看，当一组动作在 1 秒内有 12 次变化（即 12FPS），我们会认为这组动作是连贯的；而当大于 60FPS 时，人眼很难区分出来明显的变化，所以 60FPS 也一直作为业界衡量一个界面流畅程度的重要指标。一个稳定在 30FPS 的动画，我们不会认为是卡顿的，但一旦 FPS 很不稳定，人眼往往容易感知到。

FPS 低并不意味着卡顿发生，而卡顿发生 FPS 一定不高。 FPS 可以衡量一个界面的流程性，但往往不能很直观的衡量卡顿的发生，这里有另一个指标（掉帧程度）可以更直观地衡量卡顿。

什么是掉帧（跳帧）？ 按照理想帧率 60FPS 这个指标，计算出平均每一帧的准备时间有 1000ms/60 = 16.6667ms，如果一帧的准备时间超出这个值，则认为发生掉帧，超出的时间越长，掉帧程度越严重。假设每帧准备时间约 32ms，每次只掉一帧，那么 1 秒内实际只刷新 30 帧，即平均帧率只有 30FPS，但这时往往不会觉得是卡顿。反而如果出现某次严重掉帧（>300ms），那么这一次的变化，通常很容易感知到。所以界面的掉帧程度，往往可以更直观的反映出卡顿。

怎么衡量流程性
我们将掉帧数划分出几个区间进行定级，掉帧数小于 3 帧的情况属于最佳，依次类推，见下表：

Best	Normal	Middle	High	Frozen
[0:3)	[3:9)	[9:24)	[24:42)	[42:∞)

	
### FPS基线(每秒传输帧数Frames Per Second）

从腾讯、百度各方的报告来看，用FPS来衡量是否卡顿并不科学，理想情况下FPS是60，在硬件没有特殊订制的情况，上限也是60，每16ms都能完成一帧的刷新，达到60肯定是不卡的，但是50的帧率卡顿吗？答案是不一定。平时看到的大部分电影或视频 FPS并不高，30FPS即可满足，一个稳定在 30FPS 的动画，并不卡顿，但如果FPS 很不稳定，却更容易感知到，这里有个词叫**稳定**，50的FPS如果是均分到各个节点，那么用户是感知不到掉帧的，但是，如果剩余的10帧是一次回执掉的，那用户的感知就很明显，也就是**瞬时帧率**的意义更大。

> 掉帧/跳帧/卡顿

理想帧率 60FPS 的情况下，每一帧 16.6667ms，如果一帧准备超出这个值，则认为发生掉帧，超出的时间越长，掉帧程度越严重。假设每帧准备时间约 32ms，每次只掉一帧，那么 1 秒内实际只刷新 30 帧，即平均帧率只有 30FPS，但这时往往不会觉得是卡顿。反而如果出现某次严重掉帧（>300ms），那么这一次的变化，通常很容易感知到。所以界面的掉帧程度，往往可以更直观的反映出卡顿。Matrix给的卡顿建议

![](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/3e52ff90fe76495fa656d02e96a123fc~tplv-k3u1fbpfcp-zoom-1.image)

相比单看平均帧率，掉帧程度的分布更能反应界面流畅程度。百度的流畅定义跟Martrix类似，区分了小卡顿与大卡顿，同样认为**瞬时大卡顿**是造成界面流畅度降低的核心原因。

结论：瞬时帧率能真实反应界面流畅度，一般瞬时帧率平局小于3的情况下，都可以认为界面流畅。





## 启动耗时

只需要以startActivity执行为起始点，以第一帧渲染为结束点，就能得出一个较为准确的耗时。

Launch耗时可以通过onCreate、onRestoreInstanceState、onStart、onResume四个函数的耗时相加得出。在这四个方法中，onCreate一般是最重的那个方法，因为很多变量的初始化都会放在这里进行。另外，onCreate方法中还有个耗时大户是LayoutInfalter.infalte方法，调用setContentView会执行到这个方法，对于一些复杂布局的第一次解析，会消耗大量时间。由于这四个方法是同步顺序执行的，单独把某些操作从onCreate移到onResume之类的并没有什么意义，Launch耗时只关心这几个方法的总耗时。
 
*  onWindowFocusChanged

 onWindowFocusChanged是测量第一次可见的时机 onresume跟onwindowforceChange一般不做耗时操作
 
、
 UI线程所有Msg执行都算掉帧 ，
 掉帧检测
 FPS计算逻辑：平均值意义不大，瞬时fps更有参考价值，或者说其稳定性，意义更大
 卡顿掉帧指标计算

掉帧/跳帧/抖动：帧率不稳
卡顿：卡在那不懂，也可以看做更严重的掉帧


## 数据整合

其次，目前缺少一套基线，即：什么样的页面是符合性能要求，这个衡量的基本标准目前缺失，咨询了下云音乐，他们目前也没有线上性能监测能力，只是本地跑跑数据，目前业内给出的开源项目，大部分都是离线下本地数据采集，线上生产环境能直接跑的轻量级监测SDK还没有，所以这部分能力需要我们自己补全。

## 基线的制定（参考业界或者先线上跑跑）


