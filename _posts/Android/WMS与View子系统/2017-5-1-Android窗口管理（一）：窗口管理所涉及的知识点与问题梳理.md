---
layout: post
title: Android 窗口管理（一）：窗口管理涉及的知识点与直观问题
category: Android
image: 

---

# Android 窗口管理（一）：窗口管理涉及的知识点与直观问题

Android的窗口管理是该系统最复杂的一块，涉及了WindowManagerService（Window管理服务）、SurfaceFlingerService（Surface混合服务）、InputManagerService（输入系统）、VSYNC（垂直同步）与Choreogrpher、窗口动效管理等。
* WindowManagerService主要是管理窗口的Z顺序及归属属性
* SurfaceFlingerService主要用于混合当前可见窗口，输出FramBuffer数据
* InputManagerService主要用来处理触摸及按键事件
* VSYNC（垂直同步）与Choreogrpher是为了UI有更好的体验而引入的垂直同步机制

牵扯到的问题

* View绘制内存如何共享给SurfaceFlingerService
* View绘制的时机及重绘的时机
* ViewrootImpl的作用及新建时机
* Activity与Dialog默认能相应返回事件而Popwindow不可以为什么
* InputManagerService实现中C|S通信的方式是Pipe或者Socket，原理是什么？
* Surface、Layer 、Client（单一）与WindowToken与WindowState的关系，谁与谁一一对应
* Surface利用Parcel传递的原理，一起最主要传递的什么 IGraphicBufferProducer的传递预作用
* View贡献内存的申请时机与fd传递机制，（牵扯到Binder通信中fd传递）
* Input与View绘制与动画执行的先后顺序（Looper驱动？）
* Choreogrpher为Looper的MessageQueue添加塞子的阻塞原理
* 输入事件如何与当前窗口意义对应 add的时候，重新获取焦点的时候WMS更新告诉SF与InputManagerService，当前那个窗口处于最上面
* Java与Native层Surface的关系与作用
* 绘图都是在Surface上绘制的是什么意思（应该是在Surface申请的内存上绘制的时机lockCanvas）


![窗口管理知识图谱.png](http://upload-images.jianshu.io/upload_images/1460468-b58b52373128decc.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


直观的理解窗口管理：View如何从新建到显示到窗口（不要考虑Activity，跟它没啥关系，Actvity不是View，只是更好显示View的一个辅助组件），先简述下View的新建与添加显示：首先Client新建View对象，当然这里只是简单的Java类，并没有开始绘制真正的图形，之后Client向WindowManagerService登记，其实就是添加显示，并标记自己的位于的图层，WindowManagerService接着想SurfaceFlinger服务申请抽象的图层，获取抽象Surface绘图表面，之后Client利用Surface向SurfaceFlinger申请绘图内存，这部分内存是C/S共享的，获取到内存后Client绘制图形到内存，绘制完成，通知SurfaceFlinger显示到屏幕上。

# 动效及触摸事件的处理都在UI线程

所以，尽量不要在UI线程做太复杂的处理，否则会卡顿或者掉帧，VSYNC信号到来的时候，如果UI没准备好，就会掉帧。


