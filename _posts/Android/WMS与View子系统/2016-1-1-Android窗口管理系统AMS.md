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



#### ActivityManagerService作用（管理四大组件）

ActivityManagerService（AMS）是Android中最核心的服务，主要负责系统中四大组件的启动、切换、调度及应用进程的管理和调度等工作，其职责与操作系统中的进程管理和调度模块相类似，因此它在Android中非常重要。ActivityManagerService这些Java类没有继承Service，为什么？，尼玛，系统都没起来，AMS本来就是管理这些的，都没起来，都没有Service的概念，还谈什么继承Service，Service组件是什么，为了什么，不过是为了Java更好的定义服务，但是这种服务不是唯一的实现方式。只要实现了服务+Binder通信，就已经满足了Android Server的要求。
#### ActivityManagerServices框架（实现与使用）

框架图

![](http://wiki.jikexueyuan.com/project/deep-android-v2/images/chapter6/image001.png)



#### ActivityManagerService启动

Android系统大部分的Server服务都是由SystemServer进行启动的。AMS由SystemServer的ServerThread线程创建，代码如下：

	[-->SystemServer.java::ServerThread的run函数]

	
	public class SystemServer    
	{    
	    ......    
	  
	    native public static void init1(String[] args);    
	  
	    ......    
	  
	    public static void main(String[] args) {    
	        ......    
	  
	        init1(args);    
	  
	        ......    
	    }   
	  
	    public static final void init2() {    
	        Slog.i(TAG, "Entered the Android system server!");    
	        Thread thr = new ServerThread();    
	        thr.setName("android.server.ServerThread");    
	        thr.start();    
	    }
	    
	    class ServerThread extends Thread {
 
		    @Override
		    public void run() {
		   
	 
		        try {

		            context = ActivityManagerService.main(factoryTest);
	 
		            pm = PackageManagerService.main(context,
		                    factoryTest != SystemServer.FACTORY_TEST_OFF);
		
		            ActivityManagerService.setSystemProcess();
		            
		            
           
### 参考文档

 图解Android - Android GUI 系统 (2) - 窗口管理 (View, Canvas, Window Manager) <http://www.cnblogs.com/samchen2009/p/3367496.html>
 Android 4.4(KitKat)窗口管理子系统 - 体系框架 <http://blog.csdn.net/jinzhuojun/article/details/37737439>