---
layout: post
title: "Activity及Fragment后台杀死处理机制"
description: "Java"
category: android开发

---
> [判断一个Activity的Application是否在运行](#anchor_activity_is_runing)

<a name="anchor_activity_is_runing"></a>

#### 判断一个Activity的Application是否在运行

由于ActivityManager.getRunningTasks在5.0 lolip之后废弃了，如何判断一个Activity的App是否在运行。

**参考文档**[ActivityManager.getRunningTasks is deprecated android](http://stackoverflow.com/questions/31156313/activitymanager-getrunningtasks-is-deprecated-android)

	/***
	 * Checking Whether any Activity of Application is running or not
	 * @param context
	 * @return
	 */
	 
	public static boolean isForeground(Context context) {
	
	    // Get the Activity Manager
	    ActivityManager manager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
	
	    // Get a list of running tasks, we are only interested in the last one,
	    // the top most so we give a 1 as parameter so we only get the topmost.
	    List<ActivityManager.RunningAppProcessInfo> task = manager.getRunningAppProcesses();
	
	    // Get the info we need for comparison.
	    ComponentName componentInfo = task.get(0).importanceReasonComponent;
	
	    // Check if it matches our package name.
	    if(componentInfo.getPackageName().equals(context.getPackageName()))
	        return true;
	
	    // If not then our app is not on the foreground.
	    return false;
	}
	
 		