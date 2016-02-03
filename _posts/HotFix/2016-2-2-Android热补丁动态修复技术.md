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
