---
layout: default
title: Android 窗口管理系统入门 
categories: [android]

---

> **分析Android框架的时候谨记：上层都是逻辑封装，包括Activity、View，所有的实现均有相应Servcie来处理，比如View的绘制等**

### 目录

* 窗口和图形系统 - Window and View Manager System.
* 显示合成系统 - Surface Flinger
* 用户输入系统 - InputManager System
* 应用框架系统 - Activity Manager System.

### 导读，问题引入原理

我们知道，启动一个Activity，之后setContentView之后，就可以显示界面了，那么具体的实现是怎么样子的，界面的绘制是在当前进程吗，还是由那个服务来完成的，set后的后续处理如何做到，view的布局如何解析并绘制的，
### 参考文档

 