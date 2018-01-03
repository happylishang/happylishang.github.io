---
layout: post
title: "换个角度理解Java虚拟机"
categories: [Java]

---

Java虚拟机（JVM）到底是什么呢？很多时候的理解可能有些偏颇，首先JVM是个静态的概念，JVM自己是不会动的，甚至可以将其看做是一个对象。平时说什么Java虚拟机，它运行在哪里呢？有个虚拟机一直在运行吗？java.exe是做什么的，是将.class文件交给已经运行的虚拟机吗？还是说java.exe会自己启动一个虚拟机，解释运行.class文件，理解这个概念，再看JVM就会清楚很多。

首先JVM确实是个静态的概念，可以看做是一个虚拟机类，所谓运行，就是创建一个进程，并在其中new一个虚拟机对象，不断地用该对象解释处理.class文件，只是该对象功能负责，能够类装载、翻译、执行、GC等，java.exe的是一个bin文件，作用就是启动一个进程，新建并启动该虚拟机，可以说一个Java进程内含一个JVM。

![JVM模型.jpg](http://upload-images.jianshu.io/upload_images/1460468-82024a28e365c665.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


jre全称Java Runtime Environment，是运行Java程序需要的辅助环境，比如它可以提供标准库的支持等，

* 1.创建JVM装载环境和配置
* 2.装载JVM.dll 
* 3.初始化JVM.dll并挂界到JNIENV(JNI调用接口)实例 
* 4.调用JNIEnv实例装载并处理class类。


# 参考文档
[JVM启动过程——JVM之一](https://www.cnblogs.com/muffe/p/3540001.html)      
[JVM的生命周期——JVM之二](http://www.cnblogs.com/muffe/p/3541175.html)       
[将java程序导成.exe，在没有装jvm的机器上运行](http://2277259257.iteye.com/blog/2062341)