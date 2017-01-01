---
layout: post
title: "Android WebKit及Chromium入门"
description: "Java"
category: EventBus
tags: [Binder]

---



#### WebView差异

WebView是Android系统提供能显示网页的系统控件，它是一个特殊的View，同时它也是一个ViewGroup可以有很多其他子View。

* Android 4.4以下(不包含4.4)系统WebView底层实现是采用WebKit(http://www.webkit.org/)内核
* Android 4.4及其以上Google 采用了chromium(http://www.chromium.org/)作为系统WebView的底层内核支持。

#### 参考文档
 
 [Android 各个版本WebView](http://blog.csdn.net/typename/article/details/40425275)
 
 [Android Chromium WebView学习启动篇](http://blog.csdn.net/Luoshengyang/article/details/46569161)