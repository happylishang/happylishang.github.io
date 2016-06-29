---
layout: post
title: "Android App启动分析"
description: "Java"
category: android开发

---


#### 创建ActivityRecord
#### pause stop上一个Activiyt，并保存 onsaveinstancestate 也许是跟上一个进程的交互
#### 创建新进程启动



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
		
		

对应关系

* ApplicationThreadProxy（BinderProxy） -- ApplicationThreadNative --ApplicationThread（Binder）
* ActivityManagerProxy--ActivityManagerNative--ActivityManagerService（Binder）
* ActivityRecord（    final IApplicationToken.Stub appToken; // window manager token WMS 交互）

根据是否进程被创建，有时候，就算有Activityrecord 但是没有创建进程

	    final boolean resumeTopActivityLocked(ActivityRecord prev, Bundle options) {
    
   
 aPp端区分
 
       final HashMap<IBinder, ActivityClientRecord> mActivities
            = new HashMap<IBinder, ActivityClientRecord>();
            
     mActivities.put(r.token, r);  
     
         public final Activity getActivity(IBinder token) {
        return mActivities.get(token).activity;
    }
    
    
    Activity 内部启动Activity 看看启动模式，如果是同一个Task，那么就设置为同一个
    
    
#### 参考文档

[Android应用程序启动过程源代码分析](http://blog.csdn.net/luoshengyang/article/details/6689748)

[Android Framework架构浅析之【近期任务】](http://blog.csdn.net/lnb333666/article/details/7869465)

[Android Low Memory Killer介绍](http://mysuperbaby.iteye.com/blog/1397863)

[Android开发之InstanceState详解]( http://www.cnblogs.com/hanyonglu/archive/2012/03/28/2420515.html )

[对Android近期任务列表（Recent Applications）的简单分析](http://www.cnblogs.com/coding-way/archive/2013/06/05/3118732.html)

[ Android——内存管理-lowmemorykiller 机制](http://blog.csdn.net/jscese/article/details/47317765)  