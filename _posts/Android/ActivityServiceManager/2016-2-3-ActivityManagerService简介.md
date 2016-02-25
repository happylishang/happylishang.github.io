---
layout: post
title: "ActivityManagerService简介"
description: "Java"
category: android
tags: [ActivityManagerService]

---

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
		            
		            
           #### 参考文档：