---
layout: post
title: "Activity及Fragment后台杀死处理机制"
description: "Java"
category: android开发

---

#### 场景与问题

* 什么时候会有这个问题
* 为什么会有，已经会有什么后果
* 怎么处理

#### 应用何时会被后台杀死

在近期的任务列表里面，有些不是主动结束掉的任务，会因为内存紧张等原因被后台杀死。

PhoneWindowManager 

	 List<ActivityManager.RecentTaskInfo> recentTasks = am  
	                .getRecentTasks(MAX_RECENT_TASKS,  
	                        ActivityManager.RECENT_IGNORE_UNAVAILABLE);  
	                        
	                        。。。
	  /** 
     * 切换应用 
     */  
    private void switchTo(RecentTag tag) {  
        if (tag.info.id >= 0) {  
            // 这是一个活跃的任务，所以把它移动到最近任务的前面  
            final ActivityManager am = (ActivityManager) getContext()  
                    .getSystemService(Context.ACTIVITY_SERVICE);  
            am.moveTaskToFront(tag.info.id, ActivityManager.MOVE_TASK_WITH_HOME);  
        } else if (tag.intent != null) {  
            tag.intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY  
                    | Intent.FLAG_ACTIVITY_TASK_ON_HOME);  
            try {  
                getContext().startActivity(tag.intent);  
            } catch (ActivityNotFoundException e) {  
                Log.w("Recent", "Unable to launch recent task", e);  
            }  
        }  
    }                       
                        
后台杀死如何处理RecentTaskInfo

#### 后台杀死的后果


### Activity内部的Fragment后台杀死后重建，不是ViewPager的，由DialogFragment 得到的处理

每次重新创建DialogFragment，不要让系统恢复

    @Override
    protected void onSaveInstanceState(Bundle outState) {

        if (outState != null) {
            outState.putParcelable("android:support:fragments", null);
        }
        super.onSaveInstanceState(outState);
    }
   
   那里具体原因是什么？多个？ 
#### 如何应对

###  参考文档
[Lowmemorykiller笔记](http://blog.csdn.net/guoqifa29/article/details/45370561) **精** 

[Fragment实例化，Fragment生命周期源码分析](http://johnnyyin.com/2015/05/19/android-fragment-life-cycle.html)

[ android.app.Fragment$InstantiationException的原因分析](http://blog.csdn.net/sun927/article/details/46629919)

[Android Framework架构浅析之【近期任务】](http://blog.csdn.net/lnb333666/article/details/7869465)

[Android Low Memory Killer介绍](http://mysuperbaby.iteye.com/blog/1397863)

 
[Android开发之InstanceState详解]( http://www.cnblogs.com/hanyonglu/archive/2012/03/28/2420515.html )