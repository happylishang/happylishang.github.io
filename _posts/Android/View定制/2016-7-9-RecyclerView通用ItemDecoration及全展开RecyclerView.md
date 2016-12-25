---
layout: post
title: "RecyclerView通用ItemDecoration及全展开RecyclerView"
description: "View定制"
category: android开发

---

# RecyclerView的几种常用场景

Android L面世之后，Google就推荐用RecyclerView来取代ListView，在开发项目中，RecyclerView确实用的也越来越多。但是，虽然RecyclerView使用灵活，也有不少问题，比如：列表分割线都要开发者自己控制，再者，RecyclerView将测量与布局之类的逻辑都委托给了自己LayoutManager来处理，相应的也要对其LayoutManager进行定制。本文主要就以以下场景给出RecyclerView使用参考：

* 如何实现带分割线的线性RecyclerView
* 如何实现带分割线九宫格式RecyclerView
* 如何实现全展开的线性RecyclerView(比如：嵌套到ScrollView中使用)
* 如何实现全展开的九宫格式RecyclerView(比如：嵌套到ScrollView中使用)

先看一下实现样式，为了方便控制，边界的均不设置分割线，方便定制，如果需要可以采用Padding或者Margin来实现。

![九宫格列表样式](http://upload-images.jianshu.io/upload_images/1460468-2ecbed8e5d3076e0.gif?imageMogr2/auto-orient/strip)

![全展开的九宫格列表](http://upload-images.jianshu.io/upload_images/1460468-a663f26677c53449.gif?imageMogr2/auto-orient/strip)

![全展开的线性列表](http://upload-images.jianshu.io/upload_images/1460468-8e9ab06297bdbe21.gif?imageMogr2/auto-orient/strip)
