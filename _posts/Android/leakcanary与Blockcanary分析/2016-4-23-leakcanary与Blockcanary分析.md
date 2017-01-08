---
layout: post
title: "Leakcanary与Blockcanary分析"
description: "android"
categories: [android]
tags: [android]

---


#### Blockcanary 原理

Looper给日志预留了接口，预留了UI主线程监控接口


#### LeakCanary Activity 监测内存泄露原理

* 添加弱引用
* GC
* 是否GC
* 分析dump文件

具体实现


		public class ExampleApplication extends Application {
		
		    private RefWatcher mRefWatcher;
		
		    @Override
		    public void onCreate() {
		        super.onCreate();
		        mRefWatcher = LeakCanary.install(this);
		    }
		
		}


	    public void watchActivities() {
	        this.stopWatchingActivities();
	        this.application.registerActivityLifecycleCallbacks(this.lifecycleCallbacks);
	    }


    private final ActivityLifecycleCallbacks lifecycleCallbacks = new ActivityLifecycleCallbacks() {
        public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
        }

        public void onActivityStarted(Activity activity) {
        }

        public void onActivityResumed(Activity activity) {
        }

        public void onActivityPaused(Activity activity) {
        }

        public void onActivityStopped(Activity activity) {
        }

        public void onActivitySaveInstanceState(Activity activity, Bundle outState) {
        }

        public void onActivityDestroyed(Activity activity) {
            ActivityRefWatcher.this.onActivityDestroyed(activity);
        }
    };
    
  在何时的时机，添加弱引用，并强制GC，如果回收了，或者说进入到自己一个被回收监控队列，说明没泄露，否则泄露
  
#### 参考文档

[**开源项目之LeakCanary源码分析**](http://www.easyread.cc/p/5032c52c6b0a)
