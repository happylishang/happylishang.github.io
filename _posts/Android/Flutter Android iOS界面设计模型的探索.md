
Google推出Flutter有一段时间了，它是由谷歌开发的开源移动应用软件开发工具包，用于为Android和iOS开发应用，同时也将是Google Fuchsia下开发应用的主要工具，也代表了Google在跨平台上的方向，而且由Flutter及Android目前的开发模型也能看出Google想要发力的方向，本文结合ios，Flutter、Android的UI开发模型简单看下APP开发的大趋势。

# Android的UI开发模型趋势

2018年之前，Android开发基本是靠Activiyt承载UI显示及业务，虽然Fragment退出很久，但是仍是作为Activity的辅助来用，每个Activity对应一个Window，每个Window中又可以添加很多View，也就是说之前主流Android都是多Window+多View的开发模型，相比之下iOS更多的使用的是单Window+多View（VC）的开发模型。为了管理Window，Android提供了WMS，为了管理Activity，Android提供了AMS，但是AMS跟WMS在管理上其实并非完全独立的，两者关联很紧密，而且分工似乎也不是特别合理，个人觉得是有些混乱，Google可能也发现了这个问题，所以18年之后，Goolge退出的Android开发框架更偏向于iOS的开发模式，而Flutter就更像了，而Google18开发者大会似乎也有意向引导使用单Activity开发APP，这点就很像ios的单Window+View。

Google推出的Jetpack开发工具包，里面的Navigation组件就很像ios 的NavigationController，也可以说Android在逐渐尝试iOS的一些做法，因为Activity的启动、暂停等等业务转换都需要通AMS进行跨进程通信，这个多多少少会让效率打折扣。

![image.png](https://upload-images.jianshu.io/upload_images/1460468-e03bc1f8bee978d7.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

在Google的AndroidDevSummit官方人员的介绍，也一定程度上说明了他们的引导趋势：SingleActivity，在这种模型下Jetpack中的各种通信组件也能跟大程度的发挥他们的作用，比如其中LifeCircle+LiveData等都是以Activity为维度，这样的话APP内数据的共享跟通知就很方便，基本可以看做APP内共享数据

![image.png](https://upload-images.jianshu.io/upload_images/1460468-bbaaeda9dc15a8f7.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

从上面Google官方视频也能看出单Actvity+Fragment跳转也可能是未来的趋势。

## iOS UI开发模型 

虽然说ios采用的MVC，但iOS的M同V似乎很少直接通信，其实跟Android的MVP没什么区别，这点两端似乎是一致的。ios比较省事的地方是APP基本都是单Window，其他全是View，View之间的切换或者说VC之间的切换都在单个Window内部，很少有多个Window的概念，这点跟Android的多Acitivity差别很大，不过，在这点设计上似乎ios更清晰跟高明。不过Android的xml布局相比之下是Android开发者的福音，很多ios开发者用代码写布局，虽然性能上有提升，但是在硬件资源过剩的时代，Android xml 瑕不掩瑜，开发真的很方便。
 
 
## Flutter UI开发模型 跟趋势

Chrome是Google最厉害的工具。Flutter由Chrome团队孵化，因此带着浏览器的影子，不过期开发模型更偏向于ios的开发模型，单个window中各个view跳过来，跳过去。就单纯的界面显示而言，Activity完全是一个辅助工具，但是它参与了太多界面显示的东西，承担了太多非View界面的责任，导致WMS跟View自身功能的萎缩，AMS管理Activity，WMS管理窗口，但是WMS管理窗口的能力太低了，都被AMS占用了，并且就Activity而言，各种栈、恢复就比较麻烦，目前Google似乎有意弱化四大组件，转而推行自己的jetpack。
  
# JetPacket中的Navigation与Fragment的配合

非常像参考ios单window+ navigationcontoler配合。Google正在放弃对Activity、Service等组件的支持，而Flutter基本是完全摒弃了这一切，Fuscasa系统也许更彻底，没有Activity这样的概念。

![image.png](https://upload-images.jianshu.io/upload_images/1460468-16c559bae4417725.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

Android的navigation很大程度上借鉴（抄袭）了ios的故事版，不过这个总觉得有些鸡肋。

# Google Skia底层渲染库的逐步统一 

首先有一点要明确，那就是Skia是支持OpenGL的，Skia只是API的抽象，底层支持很多种实现方式，可能是CPU绘制，也可能是GPU。

skia如何使用GPU

	/*获取OpenGL上下文*/
	GrContextFactory contextFactory;
	GrContext* context = contextFactory.get(GrContextFactory::kNative_GLContextType);
	/*创建指定大小格式Surface，并由Surface中取出Canvas*/
	const SkImageInfo info = SkImageInfo::MakeN32Premul(720,1080);
	SkSurface* surface = SkSurface::NewRenderTarget(context, info);//实际上是创建一个纹理，并创建相应的fbo与之绑定，以作为渲染目标
	//或者用 NewScratchRenderTarget，这个会用缓存过的目标纹理
	SkCanvas* canvas = surface->getCanvas();
	/*执行绘制*/
	/*canvas->drawColor(0x0);*/
	/*..........*/
	/*..........*/
	/*..........*/
	/*绘制完成，取出像素*/
	SkBitmap output;
	output.setInfo(info);
	canvas->readPixels(&output);
	/*又或者读到GraphicBuffer上*/
	/*输入 ANativeWindow_Buffer outBuffer*/
	canvas->readPixels(info, outputBuffer.bits, outputBuffer.stride*4/*ARGB*/, 0, 0);
	
就Android而言，8.0时候系统提供了OpenGL（Skia）选项，而9.0之后，默认就是OpenGL（Skia），而在Q上，可能就是Vulkan了，也就是Google在逐步推行自家的渲染引擎，就目前而言Chrome，Flutter，Androd已经都实现了对Skia的支持，在将来的平台统一上Google已经走出了自己很关键的一步，可能将来的UI界面无论是在Web，还是Android都是通过Skia渲染来实现的，能够做到多端统一，也能做到一份代码，多端可用。Android早期通过skia库进行2d渲染，后来加入了hwui利用opengl替换skia进行大部分渲染工作，现在开始用skia opengl替换掉之前的opengl，从p开始skia库不再作为一个单独的动态库so，而是静态库a编译到hwui的动态库里，将skia整合进hwui，hwui调用skia opengl。

# 总结

社会在进化，浪费资源的做法会被淘汰，被社会的资源配置优化掉，共享经济会有问题，但是让闲置的资源利用起来的趋势不会停止，同样，如果能节约开发及重复开发，那么这个技术的前进也会成为趋势，虽然会受到阻碍，但是绝对不会倒退。

# 参考文档

[hwui opengl VS skia opengl VS skia vulkan?](https://segmentfault.com/a/1190000017099186)     
[Skia深入分析8——Skia的GPU绘图 原](https://my.oschina.net/jxt1234and2010/blog/517729)