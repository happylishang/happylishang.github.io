---
layout: post
title: "APP冷启动优化：如何使用好工具【Perfetto\ systrace \MethodTracing】"
category: Android


---


APP的性能提升无非就是围绕稳定、流畅之类的指标做文章，在推动性能提升的时候，什么才是关键，热情？能力 ？规范？，个人认为是工具，用好性能分析工具，性能提升就走完了一大半，就好比：”算数我比不过小王，但我找了个电子计算器“。以提升冷启动速度为例，看看整体的性能优化流程应该是什么样子，而在这其中性能工具能带来什么。


# 冷启动的定义与可优化的点

如何衡量当前的性能指标，个人感觉，性能的衡量分三步： **指标制->  指标采集 -> 性能基线与优劣评级**, 以上三块组成性能量化工具，有了量化工具，就可以说APP性能是好是坏，以冷启动为例，冷启动指标如何制定？单从技术上说感觉可以定义如下：

	冷启动耗时 = 从APP进程创建到第一个有效页面帧[闪屏]


具体到实现上，涉及哪些环节，会怎样影响冷启动速度呢？

	冷启动->系统会启动一个StartWindow占位-> 启动进程->创建Application->Application中初始化全局配置->启动第一个Activity->Create->Start->Resume->AddWindow->UI测量绘制[performTraversals]->首帧可见
	
![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/5bb1710811e040d697511dc3c697ac43~tplv-k3u1fbpfcp-watermark.image?)	

冷启动的时候，系统一般会先启动一个占位Window，默认是个白屏窗口，复用的是第一个启动Activity配置，在体感上，主要下面的Activity配置

        <item name="android:windowBackground">@drawable/xxx</item>

它一般是SplashActivity的配置，用品牌图做个中转，这个图最好要限制下尺寸，否则在解析上影响启动速度。随后系统会启动进程加载SplashActivity，启动进程主要是Application中可能有些APP全局初始化操作，尽量轻，或者延后处理，当然，也会有一些ContentProvider与Receiver影响启动这些都可以通过工具查看。

	public class LabApplication extends Application {
	    @Override
	    public void onCreate() {
		        super.onCreate()
		        <!--UI中不要处理耗时操作-->
	 }
	
之后便是Activity的创建与启动流程 ：

	    
![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/25c4129b2cf448b78aaf7ff021c1392c~tplv-k3u1fbpfcp-watermark.image?)

可以看到上图的Activity启动流程都是被动消息的处理，主要是受控AMS指挥，代码中设置View及显示的流程也就上图的几个点，比如onCreate中设置Layout并inflater，当然，这不是必须的，即使不主动setContentView，在后面的wm.addView中也会创建顶层DecorView。

    @Override
    public void setContentView(int resId) {
        ensureSubDecor();
        ViewGroup contentParent = mSubDecor.findViewById(android.R.id.content);
        contentParent.removeAllViews();
        <!--可能影响耗时-->
        LayoutInflater.from(mContext).inflate(resId, contentParent);
        mAppCompatWindowCallback.getWrapped().onContentChanged();
    }
	    
而setContentView的inflate可能是影响耗时的一个点。之后handleResumeActivity中，会想WMS添加窗口View
    
	@Override
	    public void handleResumeActivity(IBinder token, boolean finalStateRequest, boolean isForward,
	            String reason) {
	 				...
	        final ActivityClientRecord r = performResumeActivity(token, finalStateRequest, reason);
	         		...
	          	r.window = r.activity.getWindow();
            		View decor = r.window.getDecorView();
	                if (!a.mWindowAdded) {
	                    a.mWindowAdded = true;
	                    <!--添加Window-->
	                    wm.addView(decor, l);
	                } else {
	                   
	    @Override
	    public final View getDecorView() {
	        if (mDecor == null || mForceDecorInstall) {
	            installDecor();
	        }
	        return mDecor;
	    }
	    
可以看到 getDecorView会兜底处理Activity 的顶层窗口创建逻辑。addView会调用WindowManagerGlobal的addView，进而创建ViewRootImpl，利用ViewRootImpl进一步添加Window

	   public void addView(View view, ViewGroup.LayoutParams params,
	            Display display, Window parentWindow) {
	        	  <!--关键-->
	      	      root = new ViewRootImpl(view.getContext(), display);
	
	            view.setLayoutParams(wparams);
  					<!--添加到WMS，处理UI显示-->
	                
	                root.setView(view, wparams, panelParentView);
	     
	        }
	    }

而ViewRootImpl接管流程之后，所以View相关的操作都将在ViewRootImpl处理，而最终由其requestLayout触发测量、布局、绘制的动作

    public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
        synchronized (this) {
            if (mView == null) {

		  				 <!--申请下个Message绘制-->
		                requestLayout();
		                
                    <!--添加窗口-->      
                    mOrigWindowType = mWindowAttributes.type;
                    mAttachInfo.mRecomputeGlobalAttributes = true;
                    collectViewAttributes();
                    res = mWindowSession.addToDisplay(mWindow, mSeq, mWindowAttributes,
                            getHostVisibility(), mDisplay.getDisplayId(), mWinFrame,
                            mAttachInfo.mContentInsets, mAttachInfo.mStableInsets,
                            mAttachInfo.mOutsets, mAttachInfo.mDisplayCutout, mInputChannel);
                 
            }
        }
    }

requestLayout会scheduleTraversals预先占位一个异步消息，用于接收并doScheduleCallback触发的VSYNC信号，这样可以保证之后插入的消息都被延期处理，从而Window被添加后，UI绘制任务第一时间执行。

    void scheduleTraversals() {
        if (!mTraversalScheduled) {
            mTraversalScheduled = true;
            <!--异步消息-->
            mTraversalBarrier = mHandler.getLooper().getQueue().postSyncBarrier();
            <!--插入绘制请求，触发VSYNC-->
            mChoreographer.postCallback(
                    Choreographer.CALLBACK_TRAVERSAL, mTraversalRunnable, null);
 
    }
  
等待VSYNC消息回来后，撤离异步消息栅栏，第一时间处理UI绘制：
    
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f5e66d79ca4b4621addec689f02940c9~tplv-k3u1fbpfcp-watermark.image?)

所以首帧的渲染一定是在Resume之后，那么具体的时机怎么把控？到底在哪，如下图所示，插入的点在哪？

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/78902fe9f558459fa5988d3b82910079~tplv-k3u1fbpfcp-watermark.image?)

网上有一些其他的实现，认为可以监听onAttachedToWindow或者OnWindowFocusChange，onAttachedToWindow的问题是可能太过靠前，还没有Draw, OnWindowFocusChange的缺点可能是太过滞后，其实可以简单认为view会的draw以后，View的绘制就算完成，虽然到展示还可能相差一个VSYNC等待图层合成，但是对于性能监测的评定，误差一个固定值可以接受：

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ba357571e08640808e3193fd009c07ba~tplv-k3u1fbpfcp-watermark.image?)

在onResume函数中插入一条消息可以吗，理论上来说，太过靠前，这条消息在执行的时候，还没Draw，因为请求VSYNC的同步栅栏是在是在Onresume结束后才插入的，无法拦截之前的Message，但是由于VSYNC可能存在复用，Onresume中插入的消息也有可能会在绘制之后执行，这个不是完全一定的，比如点击MaterialButton启动一个Activity，第二个Activity的setView触发的VSYNC就可能复用MaterialButton的波纹触发的VSYNC，从而导致第二个Activity的performTraval复用第一个VSYNC执行，从而发生在onResume插入消息之前，如下

> 栅栏消息

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7f9b2a9052a2433586bfc2b0ff48f81a~tplv-k3u1fbpfcp-watermark.image?)

> 重绘CallBack包含多个Activity的重绘

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/441ed56238664d2c9d2443de540224de~tplv-k3u1fbpfcp-watermark.image?)	 

综上所述，**将指标定义在第一次View的Draw执行可能比较靠谱**。具体可以再DecorView上插入一个透明View，监听器onDraw回调即可，如果觉得不够优雅，就退一步，监听OnWindowFocusChange的回调，也勉强可以接受, OnWindowFocusChange一定是在Draw之后的。

有了指标，那是否达标？如何采集？基线呢？可以参考业界做法，采集方式可以无入侵打点，而优秀基线可以认为：

	优秀=秒开 

如果发现不达标，接下来要做的就是定位+优化，这个时候就体现分析工具的重要性，其实上述的原理分析就已经借助Studio自带的Profiler工具，在理解流程上事半功倍。


# 如何定位当前性能问题

冷启动每个阶段的耗时可以通过多种工具、方式来定位：可以用的有Debug.startMethodTracing跟踪，也可以利用perfetto/systrace来查看，甚至还可以用Studio自身的Profiler跟踪，每种方式都有自己的优势，可配合选择使用。


## Debug.startMethodTracing 适合查看UI线程的耗时函数

Debug.startMethodTracing是通过应用插桩来生成跟踪日志，做到对方法的跟踪。但是启用剖析功能后，应用的运行速度会减慢，所以，不应使用剖析数据确定绝对时间，最大的作用是用在对比上，可以对比之前，或者对比周围函数。具体用法：

    private void startTrace() {
        File file = new File(getApplication().getExternalFilesDir("android"), "methods.trace");
        Debug.startMethodTracing(file.getAbsolutePath(), 100 * 1024 * 1024);
    }
	<!-注意配对使用-->
    private void stopTrace() {
        Debug.stopMethodTracing();
    }
    
对于冷启动：进程启动时开启监听，在合适节点配对停止即可，之后导出.trace文件在Studio中分析，可以看到关键函数耗，Studio提供了多种模式，Flame Chart、Top Down、Bottom Up、Event，不同的模式侧重点不同。定向分析的时候，可以分段锁定范围，比如冷启动可分几个阶段排查，进程创建、Application初始化、Activity的创建、create、resume、draw等，先选定Main线程，然后将范围限制定Application阶段，如下下：

*  Flame Chart：更侧重直观反映函数耗时严重程度

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/96cf09d30b924f6bac2963e147f01e33~tplv-k3u1fbpfcp-watermark.image?)

比如上图，浅黄色部分其实就是需要重点关注的部分,耗时最多的函数，会最先展示，更加方便定位严重问题，大致定位问题后，就可以用Top Down 进一步看细节。


* Top Down： 更侧重自顶向下详细**排查**

利用Top-Down模式可以更精确观察函数耗时与调用堆栈，更加清晰，如下在Application初始化阶段，可以清醒看到函数调用顺序、耗时等，

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/138af79c4866421185ceb3e3faca94ce~tplv-k3u1fbpfcp-watermark.image?)

对于冷启动，重点排查耗时函数，尝试将非核心逻辑从UI线程中移除。同理对于闪屏Activity的onCreate跟onResume阶段所做的处理类似


![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/349265d2bc8f4fab9ee373f6284bc476~tplv-k3u1fbpfcp-watermark.image?)

从图中就很容下发现，有些Flutterboost、埋点Json解析类的耗时操作被不小心关联进了Activit的启动流程中，拖慢了冷启动速度，那就可以放到非UI线程中处理，或者延后处理。

* Bottom Up：一种平铺的模式，

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f88d69ac118542a592a77a9089dd1a98~tplv-k3u1fbpfcp-watermark.image?)

这个模式个人用的不多，罗列的函数太多，没有层次，可能单独看排名靠前的几个有些收益。

依赖profiler基本能定位哪些函数导致了冷启动速度慢，但是这些函数可能并非自己耗时严重，也许是会因为调度或者锁的原因导致慢，这个时候perfetto/systrace会提供更多帮助。

# perfetto/systrace：大局与调度

[perfetto地址及使用文档](https://ui.perfetto.dev/#!/record)

perfetto/systrace是官方提供另一种性能分析工具，其中perfetto可以看做是systrace的升级版。相比MethodTracing代码插桩，无法具体到每个方法，但可以提供全局性能概览，可以更快定位问题范围，而且perfetto/systrace在全局任务调度、系统调用上更具优势，MethodTracing多少对于性能有些影响，而perfetto/systrace借助系统本身lOG，可以降低自身带来的影响，用perfetto看一下冷启动的流程，如下：

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e1db83c48ed3482f82aba32ed31f84dd~tplv-k3u1fbpfcp-watermark.image?)

如图，首先你就能直观的看到那些阶段的耗时比较严重，然后定向分析即可，将时间段收缩，放大观察：

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/d28fef2e1cb540d18cee218594206041~tplv-k3u1fbpfcp-watermark.image?)

可以直观看出Activity启动时蓝色标记的资源解析耗时过长，定向排查后发现图大

	Name	res/BKC.xml
	Category	null
	Start time	1s 309ms 568us 459ns
	Duration	42ms 35us 682ns
	Slice ID	2465

适当将图缩小，降低加载成本，经过优化缩短到8ms

	Name	res/BKC.xml
	Category	null
	Start time	964ms 602us 749ns
	Duration	8ms 260us 261ns
	Slice ID	11851
	type	internal_slice
 
 再比如，对于有些阶段，UI线程莫名的睡眠，其实可以比较方便的查看是什么因素导致的，如下：
 
![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c673d7fc05fb4c92afadcb1151abe50e~tplv-k3u1fbpfcp-watermark.image?)

如上所示，在当前阶段，UI线程因为没有获取锁进入了睡眠，之后，被另一个线程唤起了，同样在渲染阶段有些异常的睡眠也是类似问题，基本都是异常、频繁调用些可能阻塞的耗时任务


![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/023be70f12d4405d935dd206de6f0209~tplv-k3u1fbpfcp-watermark.image?)



通过这样的方式，可以排查到底是哪个地方有问题，是否可以避免，大概的使用方式就是如此。


# 对于整体冷启动优化效果：用perfetto看比较直接

优化前：1261ms

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8cefd5ea2a124bd6a1b72ed8629a7b5f~tplv-k3u1fbpfcp-watermark.image?)

 
优化后：439ms

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/75861e508f7843f097599a35656612ee~tplv-k3u1fbpfcp-watermark.image?)


所用的优化除了上面的措施还有部分如下措施等：

* 延迟非必要receiver的注册
* 闪屏广告Layout布局按需加载
* 锁优化，进程线程间阻塞优化



# 总结

BUG是必然的，优化是持久的，如何用好工具是关键的。
