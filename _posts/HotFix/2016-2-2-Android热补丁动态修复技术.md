---
layout: blog_content
title: "Android热补丁动态修复技术"
description: "android"
category: android
tags: [android]

---


## Android热补丁动态修复技术

###使用场景


   App发布之后，如果出现了严重的线上BUG，传统的做法是重新打包、测试、上线，可能代码改动很小，但是每次付出的代价是巨大的，有没有办法以补丁的方式动态修复紧急Bug，不再需要重新发布App，不再需要用户重新下载，覆盖安装？向用户下发Patch，在用户无感知的情况下，修复了外网问题，取得非常好的效果。

### 原理

该方案基于android dex分包方案，原理是将编译好的class文件拆分打包成两个dex，绕过dex方法数量的限制以及安装时的检查，在运行时再动态加载第二个dex文件中。
#### AndFix 原理

#### AndFix 坑

Application的onCreate里面处理AndFix相关的逻辑，一定要区分进程，因为如果你的app是多进程的，每个进程都会创建Application对象，导致你的补丁逻辑被重复执行。
在内存层面看，补丁操作的影响只会局限在进程之内，似乎没有什么关系，但是如果你的补丁操作涉及到文件系统的操作，比如拷贝文件、删除文件、解压文件等等，那么进程之间就会相互影响了。
我们遇到的问题就是在主进程里面下载好的补丁包会莫名其妙地不见，主进程下载好补丁包后，信鸽进程被启动，创建Application对象，执行补丁逻辑，把刚刚主进程下载好的补丁包应用了，然后又把补丁包删除
参考文档：http://blog.csdn.net/qxs965266509/article/details/49816007http://blog.csdn.net/qxs965266509/article/details/49821413