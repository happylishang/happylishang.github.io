---
layout: post
title: "Android View绘制的几个关键节点及概念"
description: "View定制"
category: android开发

---

# setview

# canvas的创建

> ViewRootImpl.java中的performDraw()会调用draw函数，进而调用如下函数，通过mSurface.lockCanvas(dirty)获取canvas。可以看出canvas是由Surface创建的，

    private boolean drawSoftware(Surface surface, AttachInfo attachInfo, int yoff,
            boolean scalingRequired, Rect dirty) {

        // Draw with software renderer.
        Canvas canvas;
        try {
            int left = dirty.left;
            int top = dirty.top;
            int right = dirty.right;
            int bottom = dirty.bottom;

            canvas = mSurface.lockCanvas(dirty);
> Surface.java

    private final Canvas mCanvas = new CompatibleCanvas();
    
# 绘图的关键是获取到canvas
#canvas及bitmap的关系

Surface本身的作用类似一个句柄，得到了这个句柄就可以得到其中的Canvas、原生缓冲器以及其它方面的内容。
SurfaceView提供了一个专门用于绘制的surface，这个surface内嵌于。你可以控制这个Surface的格式和尺寸。Surfaceview控制这个Surface在屏幕的正确绘制位置,SurfaceView与Surface的联系就是，Surface是管理显示内容的数据（implementsParcelable），包括存储于数据的交换。而SurfaceView就是把这些数据显示出来到屏幕上面
SurfaceHolder是控制surface的一个抽象接口，你可以通过SurfaceHolder来控制surface的尺寸和格式，或者修改surface的像素，监视surface的变化等等，SurfaceHolder是SurfaceView的典型接口。
       
       

# 参考文档

[Android应用层View绘制流程与源码分析](http://blog.csdn.net/yanbober/article/details/46128379)