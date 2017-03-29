---
layout: post
title: "Android Service分析：Service意义及后台杀死重启"
category: Android


---

Service可能大家都能说上两句，比如四大组件之一，同Activity的区别是没前台交互界面，可以启动后台任务等等。不过Service到底什么意思，


	03-23 04:18:41.744  2444  2782 I ActivityManager: Killing 14008:com.snail.labaffinity/u0a61 (adj 16): remove task
	03-23 04:18:41.776  2444  2633 W ActivityManager: Scheduling restart of crashed service com.snail.labaffinity/.service.BackGroundService in 1000ms
	03-23 04:18:41.776  2444  2633 D GraphicsStats: Buffer count: 2
	03-23 04:18:42.781 14528 14528 I art     : Late-enabling -Xcheck:jni
	03-23 04:18:42.790  2444  2468 I ActivityManager: Start proc 14528:com.snail.labaffinity/u0a61 for service com.snail.labaffinity/.service.BackGroundService
	03-23 04:18:42.814 14528 14528 W System  : ClassLoader referenced unknown path: /data/app/com.snail.labaffinity-1/lib/x86
	03-23 04:18:42.824 14528 14528 V lishang :  service onCreate

# 双binder服务保活，无法从最近层任务列表杀死的原因


# 启动方式

startService

bindService

# 重启时机

### **onStartCommand的返回值：**


 private void cleanUpRemovedTaskLocked(TaskRecord tr, boolean killProcess) {
        mRecentTasks.remove(tr);
        tr.removedFromRecents();
        ComponentName component = tr.getBaseIntent().getComponent();
        if (component == null) {
            Slog.w(TAG, "No component for base intent of task: " + tr);
            return;
        }

        // Find any running services associated with this app and stop if needed.
        mServices.cleanUpRemovedTaskLocked(tr, component, new Intent(tr.getBaseIntent()));

        if (!killProcess) {
            return;
        }

        // Determine if the process(es) for this task should be killed.
        final String pkg = component.getPackageName();
        // 包名 相同的包名 可以有不同的任务名字，以及不同的pid
        ArrayList<ProcessRecord> procsToKill = new ArrayList<>();
        ArrayMap<String, SparseArray<ProcessRecord>> pmap = mProcessNames.getMap();
        for (int i = 0; i < pmap.size(); i++) {

            SparseArray<ProcessRecord> uids = pmap.valueAt(i);
            for (int j = 0; j < uids.size(); j++) {
                ProcessRecord proc = uids.valueAt(j);
                if (proc.userId != tr.userId) {
                    // Don't kill process for a different user.
                    continue;
                }
                // 不杀home进程
                if (proc == mHomeProcess) {
                    // Don't kill the home process along with tasks from the same package.
                    continue;
                }
                if (!proc.pkgList.containsKey(pkg)) {
                    // Don't kill process that is not associated with this task.
                    continue;
                }

                for (int k = 0; k < proc.activities.size(); k++) {
                    TaskRecord otherTask = proc.activities.get(k).task;
                    if (tr.taskId != otherTask.taskId && otherTask.inRecents) {
                        // Don't kill process(es) that has an activity in a different task that is
                        // also in recents.
                        return;
                    }
                }

				//不杀前台服务进程 就算是从最近任务列表删除
                if (proc.foregroundServices) {
                    // Don't kill process(es) with foreground service.
                    return;
                }

                // Add process to kill list.
                procsToKill.add(proc);
            }
        }

        // Kill the running processes.
        for (int i = 0; i < procsToKill.size(); i++) {
            ProcessRecord pr = procsToKill.get(i);
            if (pr.setSchedGroup == Process.THREAD_GROUP_BG_NONINTERACTIVE
                    && pr.curReceiver == null) {
                pr.kill("remove task", true);
            } else {
                // We delay killing processes that are not in the background or running a receiver.
                pr.waitingToKill = "remove task";
            }
        }
    }


常用的返回值有3种，START_STICKY、START_NOT_STICKY和START_REDELIVER_INTENT。其中START_STICKY和START_REDELIVER_INTENT在service没有执行完就被系统杀掉后的一段时间内会被系统重启，被系统杀掉的情形可能是在系统内存不足或者某些ROM定制了管理后台任务的策略，比如锁屏一段时间后，不在白名单中的应用会被杀掉以释放内存。如果是service本身的错误导致在没有执行完就crash退出，是不会被系统重启的。

	void serviceDoneExecutingLocked(ServiceRecord r, int type, int startId, int res) {
	        boolean inStopping = mStoppingServices.contains(r);
	        if (r != null) {
	            if (type == 1) {
	                // This is a call from a service start...  take care of
	                // book-keeping.
	                r.callStart = true;
	                switch (res) {
	                    case Service.START_STICKY_COMPATIBILITY:
	                    case Service.START_STICKY: {
	                        // We are done with the associated start arguments.
	                        r.findDeliveredStart(startId, true);
	                        // Don't stop if killed.
	                        r.stopIfKilled = false;
	                        break;
	                    }
	                    case Service.START_NOT_STICKY: {
	                        // We are done with the associated start arguments.
	                        r.findDeliveredStart(startId, true);
	                        if (r.getLastStartId() == startId) {
	                            // There is no more work, and this service
	                            // doesn't want to hang around if killed.
	                            r.stopIfKilled = true;
	                        }
	                        break;
	                    }
	                    case Service.START_REDELIVER_INTENT: {
	                        // We'll keep this item until they explicitly
	                        // call stop for it, but keep track of the fact
	                        // that it was delivered.
	                        ServiceRecord.StartItem si = r.findDeliveredStart(startId, false);
	                        if (si != null) {
	                            si.deliveryCount = 0;
	                            si.doneExecutingCount++;
	                            // Don't stop if killed.
	                            r.stopIfKilled = true;
	                        }
	                        break;
	                    }
	                    case Service.START_TASK_REMOVED_COMPLETE: {
	                        // Special processing for onTaskRemoved().  Don't
	                        // impact normal onStartCommand() processing.
	                        r.findDeliveredStart(startId, true);
	                        break;
	                    }
	                    default:
	                        throw new IllegalArgumentException(
	                                "Unknown service start result: " + res);
	                }
	                if (res == Service.START_STICKY_COMPATIBILITY) {
	                    r.callStart = false;
	                }
	            }
	            final long origId = Binder.clearCallingIdentity();
	            serviceDoneExecutingLocked(r, inStopping);
	            Binder.restoreCallingIdentity(origId);
	        } else {
	            
	        }
	    }
    



# 参考文档

[Android Service使用拾遺阿里工程師分享](http://blog.csdn.net/yueqian_scut/article/details/51174255)
