* Android：WMS窗口管理+Activity（辅助） 其实有两套 
* iOS：View+VC ios MVC=Android MVP 
* Flutter:View+VC(简)  更偏向iOS，

从Flutter看Google对于UI管理上的修改：Chrome是Google最厉害的工具。Flutter孵化与Chrome团队，

可能Google也感觉Android中Activity似乎有些多余，就单纯的界面显示而言，Activity完全是一个辅助工具，但是由于它参与了太多界面显示的东西，承担了太多非View界面的责任，导致WMS跟View自身功能的猥琐，AMS管理Activity，WMS管理窗口，但是WMS管理窗口的能力太低了，都AMS占用了，AMS管理四大组件，但就Activity而言，各种栈，各种恢复就比较麻烦。

Activity:窗口的管理与分组更加不好做，Activity是AMS管理，但是其代表的Token确实WMS中窗口分组的依据，Actiity的栈又存在多Task等类型，控制起来可能就更加不如意，导致AMS跟WMS合起来管理窗口，而且两者的分工也不是特别分明，弱化自大组件

## 布局的优缺点

Android布局性能越好，Android布局对于开发者的优势越明显，
iOS早起的自己计算高度的能力不错，但是硬件性能上来之后，这种优势会被缩小，虽然仍然有，但是没以前那么明显，Android越来越流畅就是个典型例子。

## ios MVC 其实是Android的MVP

# ios View模型 路由 等

# JetPacket中的Navigation与Fragment的配合参考ios单window

就更像iOS的navigationcontoler+VC的配合，可以看到Google似乎正在放弃对Activity、Service等自大组件的支持，Flutter基本是摒弃了这一切，Fuscasa系统也许更彻底的没有Activity这样的概念，Android中国AMS WMS的分工跟配合感觉是比价混乱的。

![image.png](https://upload-images.jianshu.io/upload_images/1460468-16c559bae4417725.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

Android的navigation很大程度上借鉴（抄袭）了ios的故事版，不过这个总觉得有些鸡肋，不过xml绝对是Android相对于ios开发的一大优势，尤其其中的wrap_content match_parent等属性，以前测量布局可能是Android性能浪费，卡顿的源头，但是硬件性能的提升填补了这一缺点，而开发的简便性反而成全了Android开发的优势，ios代码要敲很久，而Android的xml很容易就实现，而且效果也基本能直接看到。

#  Flutter渲染Skia OpenGl

首先有一点要明确，那就是Skia是支持OpenGL的，或者说它支持很多种实现方式，Skia只是API的抽象，至于底层的实现，可能是CPU绘制，也可能是GPU，就拿Android而言，8.0时候系统提供了OpenGL（Skia）选项，而9.0之后，默认就是OpenGL（Skia），而在Q上，可能就是Vulkan了。

从这里其实能看出Google的野心

Android早期通过skia库进行2d渲染，后来加入了hwui利用opengl替换skia进行大部分渲染工作，现在开始用skia opengl替换掉之前的opengl，从p的代码结构上也可以看出，p开始skia库不再作为一个单独的动态库so，而是静态库a编译到hwui的动态库里，将skia整合进hwui，hwui调用skia opengl，也为以后hwui使用skia vulkan做铺垫。

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

Skia创建GPU上下文时，其Surface并不关联Android里面的显示系统，因此是无法直接渲染上屏的，在绘制完成之后需要额外的一次readPixels，也即内存拷贝，这使其不适合做实时渲染。只是在做比较复杂的效果，如Bicubic插值、光照、模糊时，可以用一用。 
关于 Skia的特效，可以看 include/effects 和 src/effects 目录下面的代码，这里面是CPU方式实现的。由于很少见用到，之前并没有介绍。 
对应的gpu特效实现见 include/gpu 和 src/gpu/effects目录下的代码。


Flutter外部借用Surface，可能就是为了省去最后的那次拷贝，

# 参考文档

[hwui opengl VS skia opengl VS skia vulkan?](https://segmentfault.com/a/1190000017099186)     
[Skia深入分析8——Skia的GPU绘图 原](https://my.oschina.net/jxt1234and2010/blog/517729)