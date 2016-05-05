---
layout: post
title: "Android App启动分析"
description: "Java"
category: android开发

---


#### 创建ActivityRecord
#### pause stop上一个Activiyt，并保存 onsaveinstancestate 也许是跟上一个进程的交互
#### 创建新进程启动


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