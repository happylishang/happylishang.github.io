---
layout: post
title: Android窗口管理分析（1）：View如何绘制到屏幕上的主观理解
category: Android
image: http://upload-images.jianshu.io/upload_images/1460468-76a055cbca80ba44.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---

窗口管理可以说是Android系统中最复杂的一部分，主要是它涉及的模块比较对，虽然说是窗口管理，但除了WindowManagerService还包括SurfaceFlinger服务、Linux的共享内存及tmpfs文件系统、Binder通信、InputManagerService、动画、VSYNC同步技术等，一篇文章不可能分析完全，但是可以首先对于窗口的显示与管理有一个大概的轮廓，再分块分解，涉及的知识点大概如下：

![窗口管理知识图谱.png](http://upload-images.jianshu.io/upload_images/1460468-76a055cbca80ba44.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

## WMS的作用是窗口管理 不负责View绘制

既然是概述，我们不妨直观的思考一个问题，Activity是如何呈现到屏幕上的，或者说View是如何被绘制到屏幕上来的？或多或少，开发者都知道WindowManagerService是负责Android的窗口管理，但是它其实只负责管理，比如窗口的添加、移除、调整顺序等，至于图像的绘制与合成之类的都不是WMS管理的范畴，WMS更像在更高的层面对于Android窗口的一个抽象，真正完成图像绘制的是APP端，而完成图层合成的是SurfaceFlinger服务。这里通过一个简单的悬浮窗口来探索一下大概流程：
		
		TextView mview=new TextView(context);
		...<!--设置颜色 样式-->
		WindowManager mWindowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
		WindowManager.LayoutParams wmParams = new WindowManager.LayoutParams();
		wmParams.type = WindowManager.LayoutParams.TYPE_TOAST;
		wmParams.format = PixelFormat.RGBA_8888;
		wmParams.width = 800;
		wmParams.height = 800;
		mWindowManager.addView(mview, wmParams);
		
以上代码可以在主屏幕上添加一个TextView并展示，并且这个TextView独占一个窗口。在利用WindowManager.addView添加窗口之前，TextView的onDraw不会被调用，也就说View必须被添加到窗口中，才会被绘制，或者可以这样理解，只有**申请了依附窗口，View才会有可以绘制的目标内存**。当APP通过WindowManagerService的代理向其添加窗口的时候，WindowManagerService除了自己进行登记整理，还需要向SurfaceFlinger服务申请一块Surface画布，其实主要是画布背后所对应的一块内存，只有这一块内存申请成功之后，APP端才有绘图的目标，并且这块内存是APP端同SurfaceFlinger服务端共享的，这就省去了绘图资源的拷贝，示意图如下：

![绘图原理.jpg](http://upload-images.jianshu.io/upload_images/1460468-3cddb5d035046beb.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

以上是抽象的图层对应关系，可以看到，APP端是可以通过unLockCanvasAndPost直接同SurfaceFlinger通信进行重绘的，就是说图形的绘制同WMS没有关系，WMS只是负责窗口的管理，并不负责窗口的绘制，这一点其实也可以从IWindowSession的binder通信接口看出来：

	interface IWindowSession {
	
	    int add(IWindow window, int seq, in WindowManager.LayoutParams attrs,
	            in int viewVisibility, out Rect outContentInsets,
	            out InputChannel outInputChannel);
	            
	    int addToDisplay(IWindow window, int seq, in WindowManager.LayoutParams attrs,
	            in int viewVisibility, in int layerStackId, out Rect outContentInsets,
	            out InputChannel outInputChannel);
	            
		int relayout(IWindow window, int seq, in WindowManager.LayoutParams attrs,
	            int requestedWidth, int requestedHeight, int viewVisibility,
	            int flags, out Rect outFrame, out Rect outOverscanInsets,
	            out Rect outContentInsets, out Rect outVisibleInsets,
	            out Configuration outConfig, out Surface outSurface);
	            
	    void remove(IWindow window);
	...
	}

从参数就可以看出，APP与WindowManagerService通信的时候没有任何View相关的信息，更不会说将视图的数据传递给WMS，基本都是以IWindow为基本单位进行通信的，所以涉及的操作也都是针对窗口的，比如整个窗口的添加、移除、大小调整、分组等，**单单从窗口显示来看**，WMS的作用确实很明确，就是在服务端登记当前存活窗口，后面还会看到，这会影响SurfaceFlinger的图层混合，可以说是为SurfaceFlinger服务的。

在对于日常开发来说，WMS的窗口分组有时候会对开发带来影响，如果不知道窗口分组管理，可能有点忙迷惑，比如Dialog必须使用Activity的Context，PopupWindow不能作为父窗口，尤其要避免作为Webview的容器等，这些都跟WMS窗口的组织有关系。PopupWindow、Dialog、Activity三者都有窗口的概念，但又各有不同，Activity属于应用窗口、PopupWindow属于子窗口，而Dialog位于两者之间，从性质上说属于应用窗口，但是从直观理解上，比较像子窗口（其实不是）。Android中的窗口主要分为三种：系统窗口、应用窗口、子窗口，Toast就属于系统窗口，而Dialog、Activity属于应用窗口，不过Dialog必须依附Activity才能存在。PopupWindow算是子窗口，必须依附到其他窗口，依附的窗口可以使应用窗口也可以是系统窗口，但是不能是子窗口。

![窗口组织形式.jpg](http://upload-images.jianshu.io/upload_images/1460468-14737360edacc3b3.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

当然，WMS的作用不仅只是管理窗口，它还负责窗口动画、Touch事件等，后面会逐个模块分析。

## View绘制与数据传递

既然WMS的作用只是窗口管理，那么图形是怎么绘制的呢？并且这些绘制信息是如何传递给SurfaceFlinger服务的呢？每个View都有自己的onDraw回调，开发者可以在onDraw里绘制自己想要绘制的图像，很明显View的绘制是在APP端，直观上理解，View的绘制也不会交给服务端，不然也太不独立了，可是View绘制的内存是什么时候分配的呢？是谁分配的呢？我们知道每个Activity可以看做是一个图层，其对应一块绘图表面其实就是Surface，Surface绘图表面对应的内存其实是由SurfaceFlinger申请的，并且，内存是APP与SurfaceFlinger间进程共享的。实现机制是基于Linux的共享内存，其实就是MAP+tmpfs文件系统，你可以理解成SF为APP申请一块内存，然后通过binder将这块内存相关的信息传递APP端，APP端往这块内存中绘制内容，绘制完毕，通知SF图层混排，之后，SF再将数据渲染到屏幕。其实这样做也很合理，因为图像内存比较大，普通的binder与socket都无法满足需求，内存共享的示意图如下：

![View绘制与共享内存.jpg](http://upload-images.jianshu.io/upload_images/1460468-38952a23b15f700e.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

## 总结

其实整个Android窗口管理简化的话可以分为以下三部分

* WindowManagerService：WMS控制着Surface画布的添加与次序，动画还有触摸事件
* SurfaceFlinger：SF负责图层的混合，并且将结果传输给硬件显示
* APP端：每个APP负责相应图层的绘制，
* APP与SurfaceFlinger通信：APP与SF图层之间数据的共享是通过匿名内存来实现的。
