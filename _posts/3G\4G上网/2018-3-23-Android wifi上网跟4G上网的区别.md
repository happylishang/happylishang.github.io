---
layout: post
title: "Android wifi上网跟4G上网的区别"
category: android
 
---

手机上网可以用Wifi，也可以用4G，这两者究竟有什么区别，Wifi模块跟4G无限通信模块用的是同一种上网媒介吗，一个4G手机是否两块网卡呢？手机的MAC地址说的是谁的呢，比如，当你通过系统API获取MAC地址的时候，获取的是哪种MAC地址呢？本文由MAC地址（作为设备唯一标识）问题引出，简单分析下两种上网方式的区别，扫盲，高手勿拍砖：

* Wifi上网跟4G上网用的是同一块“网卡”吗
* Wifi上网跟4G上网的“MAC”地址是同一个吗
* 两者在实现方式上有什么不同呢（TCP/IP协议）

首先来看第一个问题，Wifi上网跟4G上网用的是同一块“网卡”吗，答案是否定的，一般而言，Wifi上网用的是以太网卡，拥有48位唯一的MAC地址，而4G上网则通过手机内部的基带模块来实现无线上网的目的。

# 手机Wifi上网跟4G上网硬件设施的区别

从硬件环境上来说，手机链接一个无线路由器，通过Wifi上网，走的还是以太网，在链路层，用的是以太网协议，也就是说，这种上网模式完全可以看做是手机连接了一根网线，所以其媒介仍可以看做传统意义上的网卡：

![手机wifi上网模型.png](https://upload-images.jianshu.io/upload_images/1460468-99d8d19275bbbdc8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

而4G上网用的是蜂窝网络，信号以电磁波的形式在空气中进行传播，发送到距离最近的基站，基站通过交换机转发到覆盖目标设备的基站，并通知目标设备，回传结果，这种上网模式在链路层，用的一般是PPP（Point-to-Point Protocol）协议，而其上网媒介用的则是无线通信专用的无线基带通信模块：

![手机4G上网模型.png](https://upload-images.jianshu.io/upload_images/1460468-b03a5be8526f11d6.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

硬件上的不同，决定了其在软件系统上必定采取不同的适配方式。


