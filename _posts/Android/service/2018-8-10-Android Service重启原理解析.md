
---
layout: post
title: Android Service重启原理解析
category: Android

---

Android系统中，APP进程被杀后，等一会经常发现进程又起来了，这个现象同APP中Service的使用有很大关系，这里指的Service是通过startService启动的，而不是通binderSertvice启动的，binderSertvice是通Activity显示界面相关的，如果两者统一进程，binderSertvice的影响可以忽略，如果不是同一进程，Service会被重启。显示都没了那么这种情况情况下的Service也没必要启动了，但是对于通过startService启动的服务，很可能需要继续处理自己需要处理的问题，因此，可能需要重启。

相信不少人之前多少都了解过，如果想要Service在进程结束后重新唤醒，那么可能需要用到将Service的onStartCommand返回值设置成START_REDELIVER_INTENT或者START_STICKY，这样被杀后Service就可以被唤醒，那么为什么？

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
    
        return START_REDELIVER_INTENT（或者START_STICKY）;
    }

先看下Google文档对于Service的onStartCommand常用的几个返回值的解释（不完全正确）：

* START_REDELIVER_INTENT

Constant to return from onStartCommand(Intent, int, int): if this service's process is killed while it is started (after returning from onStartCommand(Intent, int, int)), then it will be scheduled for a restart and the last delivered Intent re-delivered to it again via onStartCommand(Intent, int, int).

*  START_STICKY

Constant to return from onStartCommand(Intent, int, int): if this service's process is killed while it is started (after returning from onStartCommand(Intent, int, int)), then leave it in the started state but don't retain this delivered intent.

* 	START_NOT_STICKY

Constant to return from onStartCommand(Intent, int, int): if this service's process is killed while it is started (after returning from onStartCommand(Intent, int, int)), and there are no new start intents to deliver to it, then take the service out of the started state and don't recreate until a future explicit call to Context.startService(Intent).

简单说就是：进程被杀后，START_NOT_STICKY 不会重新唤起Service，除非重新调用startService，才会调用onStartCommand，而START_REDELIVER_INTENT跟START_STICKY都会重启Service，并且START_REDELIVER_INTENT会将最后的一个Intent传递给onStartCommand。**不过，看源码这个解释并不准确，START_REDELIVER_INTENT不仅仅会发送最后一个Intent，它会将之前所有的startService的Intent都重发给onStartCommand**，在AMS中会保存START_REDELIVER_INTENT的所有Intent信息：

![AMS存储所有杀死后需要重发的Intent](https://upload-images.jianshu.io/upload_images/1460468-a150baea69020c6c.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

而START_NOT_STICKY跟START_STICKY都不需要AMS存储Intent，如下图：

![AMS不存储Intent](https://upload-images.jianshu.io/upload_images/1460468-9f64548d9d543cbb.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

从测试来看，所有的Intent都会被重发，而不仅仅是最后一个。为什么设置了某些选项就会重启，设置会重新发送之前Intent呢？下面就来分析下原理，首先简单跟踪下启动，因为恢复所需要的所有信息都是在启动的时候构建好的。

# Service首次启动简述（Android6.0）

为了简化流程，我们假设Service所在的进程已经启动。直接从AMS调用ActiveService 的startServiceLocked开始，看看启动的时候是如何为恢复做准备的

	  ComponentName startServiceLocked(IApplicationThread caller, Intent service, String resolvedType,
	            int callingPid, int callingUid, String callingPackage, int userId)
	            throws TransactionTooLargeException {
	         <!--构建ServiceRecord-->
	        ServiceLookupResult res =
	            retrieveServiceLocked(service, resolvedType, callingPackage,
	                    callingPid, callingUid, userId, true, callerFg);
	        ..
	        ServiceRecord r = res.record;				 ..
	        <!--为调用onStartCommand添加ServiceRecord.StartItem-->
	        r.pendingStarts.add(new ServiceRecord.StartItem(r, false, r.makeNextStartId(),
	                service, neededGrants));
	         ...
	      <!--继续启动Service路程-->   
        return startServiceInnerLocked(smap, service, r, callerFg, addToStarting);
        }

启动Service的时候，AMS端先为其构建一个ServiceRecord，算是Service在AMS端的映像，然后**添加一个ServiceRecord.StartItem到pendingStarts列表**，这个是回调onStartCommand的依据，之后调用startServiceInnerLocked 再调用bringUpServiceLocked进一步启动Service:

       <!--函数1-->
       ComponentName startServiceInnerLocked(ServiceMap smap, Intent service, ServiceRecord r,
            boolean callerFg, boolean addToStarting) throws TransactionTooLargeException {
        <!--还没有处理onStart-->
        r.callStart = false;
        ...
        String error = bringUpServiceLocked(r, service.getFlags(), callerFg, false);
        ...
         
       <!--函数2-->          
	    private final String bringUpServiceLocked(ServiceRecord r, int intentFlags, boolean execInFg,
	            boolean whileRestarting) throws TransactionTooLargeException {
	         //第一次调用的时候，r.app=null，第二次可以直接调用sendServiceArgsLocked触发onStartCommand的执行
	        if (r.app != null && r.app.thread != null) {
	            // 启动的时候也会调用
	            sendServiceArgsLocked(r, execInFg, false);
	            return null;
	        }
	        ...
	        
	       if (!isolated) {
	        app = mAm.getProcessRecordLocked(procName, r.appInfo.uid, false);
	        if (app != null && app.thread != null) {
	            try {
	                app.addPackage(r.appInfo.packageName, r.appInfo.versionCode, mAm.mProcessStats);
	               // 调用realStartServiceLocked真正开始启动Servie
	                realStartServiceLocked(r, app, execInFg);
	          ...           

第一次启动service的时候，为了表示APP端Service还没启动，r.app是没有赋值的，r.app要一直到realStartServiceLocked的执行才被赋值，如果已经启动了，再次调用startService，这里就会走sendServiceArgsLocked，直接回调到APP端onstartCommand：

	  private final void realStartServiceLocked(ServiceRecord r,
	            ProcessRecord app, boolean execInFg) throws RemoteException {
	       
	        r.app = app;
	        r.restartTime = r.lastActivity = SystemClock.uptimeMillis();
			 ..
	        boolean created = false;
	        try {
	           <!--通知APP启动Service-->
	            app.thread.scheduleCreateService(r, r.serviceInfo,
	                    mAm.compatibilityInfoForPackageLocked(r.serviceInfo.applicationInfo),
	                    app.repProcState);
	            r.postNotification();
	            created = true;
	        } ...
	     // If the service is in the started state, and there are no
        // pending arguments, then fake up one so its onStartCommand() will
        // be called.
        <!--恢复：这里应该主要是给start_sticky用的，恢复的时候触发调用onStartCommand-->
        if (r.startRequested && r.callStart && r.pendingStarts.size() == 0) {
            r.pendingStarts.add(new ServiceRecord.StartItem(r, false, r.makeNextStartId(),
                    null, null));
        }
        <!--处理onstartComand-->
        sendServiceArgsLocked(r, execInFg, true);
        ...
 
realStartServiceLocked会通过Binder通知APP创建Service：app.thread.scheduleCreateService，然后接着通过通知APP回调onStartCommand，由于AMS是通过向APP的UI线程插入消息来处理的，等到sendServiceArgsLocked的请求被执行的时候，Service一定会被创建完成，创建流程没什么可说的，这里主要说的是sendServiceArgsLocked。之前在startServiceLocked的时候，我们向pendingStarts塞入了一个ServiceRecord.StartItem，这个在下面的sendServiceArgsLocked会被用到：
 
    private final void sendServiceArgsLocked(ServiceRecord r, boolean execInFg,
            boolean oomAdjusted) throws TransactionTooLargeException {
        final int N = r.pendingStarts.size();
        if (N == 0) {
            return;
        }
        // 这里只处理pendingStarts>0 这里处理的是浅杀
        while (r.pendingStarts.size() > 0) {
            Exception caughtException = null;
            ServiceRecord.StartItem si;
            try {
                si = r.pendingStarts.remove(0);
                <!--这里主要是给START_STICKY恢复用的，在START_STICKY触发onStartCommand的时候其intent为null，pendingStarts size为1-->
                if (si.intent == null && N > 1) {
                    // If somehow we got a dummy null intent in the middle,
                    // then skip it.  DO NOT skip a null intent when it is
                    // the only one in the list -- this is to support the
                    // onStartCommand(null) case.
                    continue;
                }
                <!--更新deliveredTime  恢复延时计算的一个因子-->
                si.deliveredTime = SystemClock.uptimeMillis();
                <!--将pendingStarts中的ServiceRecord.StartItem转移到deliveredStarts 恢复的一个判断条件-->
                r.deliveredStarts.add(si);
                <!--deliveryCount++ 是恢复的一个判断条件-->
                si.deliveryCount++;
                ...
                r.app.thread.scheduleServiceArgs(r, si.taskRemoved, si.id, flags, si.intent);
           	 ... 
             }

sendServiceArgsLocked主要用来向APP端发送消息，主要有两个作用：要么是让APP端触发onStartCommand，要么是在删除最近任务的时候触发onTaskRemoved。这里先关心触发onStartCommand，sendServiceArgsLocked会根据pendingStarts来看看需要发送哪些给APP端，之前被塞入的ServiceRecord.StartItem在这里就用到了，由于是第一次，这了传过来的Intent一定是非空的，所以执行后面的。这里有几点比较重要的：

* 将pendingStarts中的记录转移到deliveredStarts，也就是从未执行onStartCommand转移到已执行
* 更新deliveredTime，对于START_REDELIVER_INTENT，这个是将来恢复延时的一个计算因子
* 更新deliveryCount，如果onStartCommand执行失败的次数超过两次，后面就不会为这个Intent重发（仅限START_REDELIVER_INTENT）
* 通过scheduleServiceArgs回调APP
  
之后通过scheduleServiceArgs回调APP端，ActivityThread中相应处理如下：

    private void handleServiceArgs(ServiceArgsData data) {
        Service s = mServices.get(data.token);
        if (s != null) {
            ...
                int res;
                // 如果没有 taskRemoved，如果taskRemoved 则回调onTaskRemoved
                if (!data.taskRemoved) {
                <!--普通的触发onStartCommand-->
                    res = s.onStartCommand(data.args, data.flags, data.startId);
                } else {
                <!--删除最近任务回调-->
                    s.onTaskRemoved(data.args);
                    res = Service.START_TASK_REMOVED_COMPLETE;
                }                
                try {
                 <!-- 通知AMS处理完毕-->
                    ActivityManagerNative.getDefault().serviceDoneExecuting(
                            data.token, SERVICE_DONE_EXECUTING_START, data.startId, res);
                } ...
           }

APP端触发onStartCommand回调后，会通知服务端Service启动完毕，在服务端ActiveServices继续执行serviceDoneExecuting，这个时候也是Service恢复的一个关键点，这里onStartCommand的返回值就会真正被用，用来生成Service恢复的一个关键指标

    void serviceDoneExecutingLocked(ServiceRecord r, int type, int startId, int res) {
        boolean inDestroying = mDestroyingServices.contains(r);
        if (r != null) {
            if (type == ActivityThread.SERVICE_DONE_EXECUTING_START) {
                // This is a call from a service start...  take care of
                // book-keeping.
                r.callStart = true;
                switch (res) {
                <!--对于 START_STICKY_COMPATIBILITY跟START_STICKY的Service，一定会被重启 但是START_STICKY_COMPATIBILITY不一定回调onStartCommand-->
                    case Service.START_STICKY_COMPATIBILITY:
                    case Service.START_STICKY: {
                    <!--清理deliveredStarts-->
                        r.findDeliveredStart(startId, true);
                     <!--标记 被杀后需要重启-->
                        r.stopIfKilled = false;
                        break;
                    }
                    case Service.START_NOT_STICKY: {
                        <!--清理-->
                        r.findDeliveredStart(startId, true);
                        <!--不需要重启-->
                        if (r.getLastStartId() == startId) {
                            r.stopIfKilled = true;
                        }
                        break;
                    }
                    case Service.START_REDELIVER_INTENT: {
                     
                        ServiceRecord.StartItem si = r.findDeliveredStart(startId, false);
                        // 不过这个时候 r.stopIfKilled = true
                        if (si != null) {
                            si.deliveryCount = 0;
                            si.doneExecutingCount++;
                            // Don't stop if killed.  这个解释有些奇葩 
                           <!--不需要立即重启 START_REDELIVER_INTENT的时候，依靠的是deliveredStarts触发重启-->
                            r.stopIfKilled = true;
                        }
                        break;
                    }
                    ...
                }
                if (res == Service.START_STICKY_COMPATIBILITY) {
                <!--如果是Service.START_STICKY_COMPATIBILITY，会重启，但是不会触发onStartCommand，不同版本可能不同-->
                    r.callStart = false;
                }
            } ...
    }
  
serviceDoneExecutingLocked主要做了以下两件事

* 对于不需要重新发送Intent的Service，清理deliveredStarts
* 对于需要立刻重启的Service将其stopIfKilled设置为false

对于 Service.START_STICKY比较好理解，需要重启，并且不发送Intent，但是对于Service.START_REDELIVER_INTENT有些迷惑，这个也需要重启，只是重启的不是那么迅速（后面会分析），不过Google这里将其stopIfKilled设置为了true，其实Service.START_REDELIVER_INTENT类型的Service重启依靠的不是这个标志位,对比下两种情况的ProcessRecord：

![START_STICKY](https://upload-images.jianshu.io/upload_images/1460468-6b75d857414e0e98.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![Service.START_REDELIVER_INTENT](https://upload-images.jianshu.io/upload_images/1460468-0cea9f681d0d2cf0.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
  
findDeliveredStart是用来清理deliveredStarts的，第二个参数如果是true，就说明需要清除，否则，就是保留，可以看到对于Service.START_REDELIVER_INTENT是保留从，其余全部清除。
 
    public StartItem findDeliveredStart(int id, boolean remove) {
        final int N = deliveredStarts.size();
        for (int i=0; i<N; i++) {
            StartItem si = deliveredStarts.get(i);
            if (si.id == id) {
                if (remove) deliveredStarts.remove(i);
                return si;
            }
        }
        return null;
    }


执行到这里，Service启动完毕，为重启构建的数据也都准备好了，主要包括两个

* ProcessRecord的stopIfKilled字段，如果是false，需要立即重启
* ProcessRecord 的deliveredStarts，如果非空，则需要重启，并重发之前的Intent（重启可能比较慢）

除了上面的情况，基本都不重启，启动分析完成，场景构建完毕，下面看看如何恢复的，假设APP被后台杀死了，Service（以及进程）如何重启的呢？
     
# APP被杀后Service如何重启

Binder有个讣告机制，Server死后，会向Client发送一份通知，在这里，其实就是APP死掉后，会想ActivityManagerService发送一份讣告通知，AMS后面负责清理APP的场景，并看是否需要回复Service，进一步处理后续流程，ActivityManagerService会调用handleAppDiedLocked处理死去的进程：

	<!--函数1-->
    private final void handleAppDiedLocked(ProcessRecord app,
            boolean restarting, boolean allowRestart) {
        int pid = app.pid;
        boolean kept = cleanUpApplicationRecordLocked(app, restarting, allowRestart, -1);
        ..,
    
    <!--函数2-->    
    private final boolean cleanUpApplicationRecordLocked(ProcessRecord app,
        boolean restarting, boolean allowRestart, int index) {
       ...
       mServices.killServicesLocked(app, allowRestart);
    
进一步调用ActiveServcies的killServicesLocked，killServicesLocked负责清理已死进程的Service，如果有必要，还需要根据之前启动时的设置重启Service：

    final void killServicesLocked(ProcessRecord app, boolean allowRestart) {
    	<!--先清理bindService，如果仅仅是bind先清理掉-->
       for (int i = app.connections.size() - 1; i >= 0; i--) {
            ConnectionRecord r = app.connections.valueAt(i);
            removeConnectionLocked(r, app, null);
        }
          ...     
        ServiceMap smap = getServiceMap(app.userId);
        <!--处理未正常stop的Service-->
        for (int i=app.services.size()-1; i>=0; i--) {
            ServiceRecord sr = app.services.valueAt(i);
			  ...
            <!--  超过两次的要避免再次重启Service，但是进程还是会被唤醒 如果是系统应用则无视，仍旧重启-->
            if (allowRestart && sr.crashCount >= 2 && (sr.serviceInfo.applicationInfo.flags
                    &ApplicationInfo.FLAG_PERSISTENT) == 0) {
                bringDownServiceLocked(sr);
            } else if (!allowRestart || !mAm.isUserRunningLocked(sr.userId, false)) {
               <!--不准重启的-->
                bringDownServiceLocked(sr);
            } else {
            		<!--准备重启-->
                boolean canceled = scheduleServiceRestartLocked(sr, true);
                <!--看看是否终止一些极端的情况-->
                // Should the service remain running?  Note that in the
                // extreme case of so many attempts to deliver a command
                // that it failed we also will stop it here.
                <!-重启次数过多的话canceled=true（主要针对重发intent的）-->
                if (sr.startRequested && (sr.stopIfKilled || canceled)) {
                    if (sr.pendingStarts.size() == 0) {
                        sr.startRequested = false;...
                        if (!sr.hasAutoCreateConnections()) {
                            bringDownServiceLocked(sr);
                 } }  }
            }
        }

这里有些限制，比如**重启两次都失败，那就不再重启Service，但是系统APP不受限制**，bindService的那种先不考虑，其他的为被正常stop的都会调用scheduleServiceRestartLocked进行重启登记，不过对于**像START_NOT_STICKY这种，登记会再次被取消**，sr.stopIfKilled就是在这里被用到。先看下    scheduleServiceRestartLocked，它的返回值也会影响是否需要重启：

    private final boolean scheduleServiceRestartLocked(ServiceRecord r,
            boolean allowCancel) {
        boolean canceled = false;

        ServiceMap smap = getServiceMap(r.userId);
        if (smap.mServicesByName.get(r.name) != r) {
            ServiceRecord cur = smap.mServicesByName.get(r.name);
            Slog.wtf(TAG, "Attempting to schedule restart of " + r
                    + " when found in map: " + cur);
            return false;
        }

        final long now = SystemClock.uptimeMillis();

        if ((r.serviceInfo.applicationInfo.flags
                &ApplicationInfo.FLAG_PERSISTENT) == 0) {
            long minDuration = SERVICE_RESTART_DURATION;
            long resetTime = SERVICE_RESET_RUN_DURATION;

            // Any delivered but not yet finished starts should be put back
            // on the pending list.
            // 在clean的时候会处理
            // 这里仅仅是要处理的需要deliveredStarts intent
            // remove task的被清理吗
            final int N = r.deliveredStarts.size();
            // deliveredStarts的耗时需要重新计算
            if (N > 0) {
                for (int i=N-1; i>=0; i--) {
                    ServiceRecord.StartItem si = r.deliveredStarts.get(i);
                    si.removeUriPermissionsLocked();
                    if (si.intent == null) {
                        // We'll generate this again if needed.
                    } else if (!allowCancel || (si.deliveryCount < ServiceRecord.MAX_DELIVERY_COUNT
                            && si.doneExecutingCount < ServiceRecord.MAX_DONE_EXECUTING_COUNT)) {
                        // 重启的时候
                        // 重启的时候，deliveredStarts被pendingStarts替换掉了
                        // 也就说，这个时候由死转到活
                        r.pendingStarts.add(0, si);
                        // 当前时间距离上次的deliveredTime，一般耗时比较长
                        long dur = SystemClock.uptimeMillis() - si.deliveredTime;
                        dur *= 2;
                        if (minDuration < dur) minDuration = dur;
                        if (resetTime < dur) resetTime = dur;
                    } else {
                        canceled = true;
                    }
                }
                r.deliveredStarts.clear();
            }
           r.totalRestartCount++;
            // r.restartDelay第一次重启
            if (r.restartDelay == 0) {
                r.restartCount++;
                r.restartDelay = minDuration;
            } else {
                // If it has been a "reasonably long time" since the service
                // was started, then reset our restart duration back to
                // the beginning, so we don't infinitely increase the duration
                // on a service that just occasionally gets killed (which is
                // a normal case, due to process being killed to reclaim memory).
                <!--如果被杀后，运行时间较短又被杀了，那么增加重启延时，否则重置为minDuration，（比如内存不足，经常重杀，那么不能无限重启，增大延时）-->
                if (now > (r.restartTime+resetTime)) {
                    r.restartCount = 1;
                    r.restartDelay = minDuration;
                } else {
                    r.restartDelay *= SERVICE_RESTART_DURATION_FACTOR;
                    if (r.restartDelay < minDuration) {
                        r.restartDelay = minDuration;
                    }
                }
            }
           <!--计算下次重启的时间-->
            r.nextRestartTime = now + r.restartDelay;
            <!--两个Service启动至少间隔10秒，这里的意义其实不是很大，主要是为了Service启动失败的情况，如果启动成功，其他要启动的Service会被一并直接重新唤起，-->
            boolean repeat;
            do {
                repeat = false;
                for (int i=mRestartingServices.size()-1; i>=0; i--) {
                    ServiceRecord r2 = mRestartingServices.get(i);
                    if (r2 != r && r.nextRestartTime
                            >= (r2.nextRestartTime-SERVICE_MIN_RESTART_TIME_BETWEEN)
                            && r.nextRestartTime
                            < (r2.nextRestartTime+SERVICE_MIN_RESTART_TIME_BETWEEN)) {
                        r.nextRestartTime = r2.nextRestartTime + SERVICE_MIN_RESTART_TIME_BETWEEN;
                        r.restartDelay = r.nextRestartTime - now;
                        repeat = true;
                        break;
                    }
                }
            } while (repeat);
        } else {
        <!--系统服务，即可重启-->
            // Persistent processes are immediately restarted, so there is no
            // reason to hold of on restarting their services.
            r.totalRestartCount++;
            r.restartCount = 0;
            r.restartDelay = 0;
            r.nextRestartTime = now;
        }
        
         if (!mRestartingServices.contains(r)) {
            <!--添加一个Service-->
            mRestartingServices.add(r);
            ...
        }
        
        mAm.mHandler.removeCallbacks(r.restarter);
        // postAtTime
        mAm.mHandler.postAtTime(r.restarter, r.nextRestartTime);
        <!--校准一下真实的nextRestartTime，dump时候可以看到-->
        r.nextRestartTime = SystemClock.uptimeMillis() + r.restartDelay;
        ...
        return canceled;
    }
    
scheduleServiceRestartLocked主要作用是计算重启延时，并发送重启的消息到Handler对应的MessageQueue，对于需要发送Intent的Service，他们之前的Intent被暂存在delivered， 在恢复阶段，原来的deliveredStarts会被清理，转换到pendingStart列表中，后面重新启动时候会根据pending重发Intent给Service，调用其onStartCommand。不过对于这种Service，其启动恢复的时间跟其运行时间有关系，距离startService时间越长，其需要恢复的延时时间就越多，后面会单独解释。

![341534476234_.pic_hd.jpg](https://upload-images.jianshu.io/upload_images/1460468-5155f719b3c51aca.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

其次，什么时候Cancle重新启动呢？只有deliveredStarts非空（START_DELIVER_INTENT）,并且回调onStartCommand失败的次数>=2，或者成功的次数>=6的时候重启失败的次数小于2doneExecutingCount，也就是对于START_DELIVER_INTENT，如果被杀超过6次，AMS会清理该Service，不会再重启了。另外如果重启的Service有很多个，多个Service开始预置的重启间隔最少是10S，不过，并不是说Service需要间隔10才能重启，而是说，如果前一个Service重启失败，要等10s才重启下一个，如果第一个Service就重启成功，同时进程也启动成功，那么所有的Service都会被唤起，而不需要等到真正的10秒延时间隔

![321534474231_.pic_hd.jpg](https://upload-images.jianshu.io/upload_images/1460468-98def679dee7cc5a.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![331534474536_.pic_hd.jpg](https://upload-images.jianshu.io/upload_images/1460468-8088c0a60779ba6c.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到，虽然pendingStart中Service重启的间隔是至少相隔10秒，但是一个Service启动成功后，所有的Service都被唤起了，虽然还没有到之前预置的启动时机。这是为什么？因为，如果在进程未启动的时候启动Service，那么需要先启动进程，之后attach Application ，在attatch的时候，除了启动自己Service，还要将其余等待唤醒的Service一并唤起，源码如下：

    boolean attachApplicationLocked(ProcessRecord proc, String processName)
            throws RemoteException {
        boolean didSomething = false;
        ...
        // 只要是一个起来了，就立刻重新启动所有Service，进程已经活了，就无须等待
        if (mRestartingServices.size() > 0) {
            ServiceRecord sr = null;
            for (int i=0; i<mRestartingServices.size(); i++) {
                sr = mRestartingServices.get(i);
                if (proc != sr.isolatedProc && (proc.uid != sr.appInfo.uid
                        || !processName.equals(sr.processName))) {
                    continue;
                }
                <!--清除旧的，-->
                mAm.mHandler.removeCallbacks(sr.restarter);
                <!--添加新的-->
                mAm.mHandler.post(sr.restarter);
            }
        }
        return didSomething;
    }

可以看到，attachApplicationLocked的时候，会将之前旧的含有10秒延迟间隔的restarter清理掉，并重新添加无需延时的重启命令，这样那些需要重启的Service就不用等到之前设定的延时就可以重新启动了。还有什么情况，需要考虑呢，看下面的：

            <!-重启次数过多的话canceled=true（主要针对重发intent的）-->
                if (sr.startRequested && (sr.stopIfKilled || canceled)) {
                    if (sr.pendingStarts.size() == 0) {
                        sr.startRequested = false;...
                        if (!sr.hasAutoCreateConnections()) {
                            bringDownServiceLocked(sr);
                 } }  }
            }
            
* 对于START_STICKY，scheduleServiceRestartLocked返回值一定是false，delay的时间是1S，并且由于其stopIfKilled是false，所以一定会被快速重启，不会走bringDownServiceLocked流程
* 对于STAR_NO_STICKY，scheduleServiceRestartLocked返回值是flase，但是stopIfKilled是true，另外其pendingStarts列表为空，**如果没有被其他存活的Activity绑定**，那么需要走bringDownServiceLocked流程，也就是说，不会被重启。

处理完上述逻辑后，ServiceRestarter就会被插入到MessegeQueue等待执行，之后调用performServiceRestartLocked-> bringUpServiceLocked-> realStartServiceLocked进一步处理Service的重启。

    private class ServiceRestarter implements Runnable {
        private ServiceRecord mService;

        void setService(ServiceRecord service) {
            mService = service;
        }

        public void run() {
            synchronized(mAm) {
                performServiceRestartLocked(mService);
            }
        }
    }
    
    final void performServiceRestartLocked(ServiceRecord r) {
    // 如果被置空了，也是不用重启
    if (!mRestartingServices.contains(r)) {
        return;
    }
    try {
        bringUpServiceLocked(r, r.intent.getIntent().getFlags(), r.createdFromFg, true);
    } catch (TransactionTooLargeException e) {
        // Ignore, it's been logged and nothing upstack cares.
    }
}

之前第一次启动的时候看过了，这里再来看复习一下，主要看不同的点，其实主要是针对START_STICKY的处理：

	  private final void realStartServiceLocked(ServiceRecord r,
	            ProcessRecord app, boolean execInFg) throws RemoteException {
	       ...
        <!--恢复：这里应该主要是给start_sticky用的，恢复的时候触发调用onStartCommand-->
        if (r.startRequested && r.callStart && r.pendingStarts.size() == 0) {
            r.pendingStarts.add(new ServiceRecord.StartItem(r, false, r.makeNextStartId(),
                    null, null));
        }
        <!--处理onstartComand-->
        sendServiceArgsLocked(r, execInFg, true);
        ... 

 对于START_STICKY需要重启，之前说过了，但是怎么标记需要重新调用onStartCommand呢？上面的realStartServiceLocked会主动添加一个ServiceRecord.StartItem到pendingStarts，因为这个时候，对于START_STICKY满足如下条件。
 
	 r.startRequested && r.callStart && r.pendingStarts.size() == 0
 
不过，这个Item没有Intent，也就说，回调onStartCommand的时候，没有Intent传递给APP端，接下来的sendServiceArgsLocked跟之前的逻辑没太大区别，不再分析，下面再说下为什么START_REDELIVER_INTENT比较耗时。
        
# 被杀重启时候，为什么	START_REDELIVER_INTENT通常比START_STICK延时更多

之前说过，在onStartCommand返回值是START_REDELIVER_INTENT的时候，其重启恢复的延时时间跟Service的启动时间有关系。具体算法是：从start到now的时间*2，距离启动时间越长，restart的延时越多。

	 private final boolean scheduleServiceRestartLocked(ServiceRecord r,
	            boolean allowCancel) {
	        boolean canceled = false;
		...
		final int N = r.deliveredStarts.size();
            // deliveredStarts的耗时需要重新计算
            if (N > 0) {
            ...
				if (!allowCancel || (si.deliveryCount < ServiceRecord.MAX_DELIVERY_COUNT
		                            && si.doneExecutingCount < ServiceRecord.MAX_DONE_EXECUTING_COUNT)) {
		                        r.pendingStarts.add(0, si);
		                        // 当前时间距离上次的deliveredTime，一般耗时比较长
		                        long dur = SystemClock.uptimeMillis() - si.deliveredTime;
		                        dur *= 2;
		                        if (minDuration < dur) minDuration = dur;
		                        if (resetTime < dur) resetTime = dur;
		                    }
		                    
如果设置了START_REDELIVER_INTENT，这里的deliveredStarts就一定非空，因为它持有startService的Intent列表，在这种情况下，重启延时是需要重新计算的，一般是是2*（距离上次sendServiceArgsLocked的时间（比如由startService触发））

 long dur = SystemClock.uptimeMillis() - si.deliveredTime;
 
比如距离上次startService的是3分，那么它就在6分钟后重启，如果是1小时，那么它就在一小时后启动，

![启动后运行时间](https://upload-images.jianshu.io/upload_images/1460468-74d78b6c16cac048.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![再次启动延时](https://upload-images.jianshu.io/upload_images/1460468-5b43eb16f9cb171e.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

而对于START_STICK，它的启动延时基本上是系统设置的Service最小重启延时单位，一般是一秒：

    static final int SERVICE_RESTART_DURATION = 1*1000;

![START_STICK重启延时](https://upload-images.jianshu.io/upload_images/1460468-861a448746e8a1fb.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
    
所以**如果你需要快速重启Service，那么就使用START_STICK，不过START_STICK不会传递之前Intent信息**，上面分析都是假设进程被意外杀死，那么用户主动从最近的任务列表删除的时候，也会重启，有什么不同吗？

 
# 从最近任务列表删除，如何处理Service的重启

左滑删除有时候会导致进程被杀死，这个时候，未被stop的Service也是可能需要重新启动的，这个时候跟之前的有什么不同吗？在这种情况下Service的onTaskRemoved会被回调。
    
    @Override
    public void onTaskRemoved(Intent rootIntent) {
        super.onTaskRemoved(rootIntent);
    }

左滑删除TASK会调用AMS的cleanUpRemovedTaskLocked，这个函数会先处理Service的，并回调其onTaskRemoved，之后杀进程，杀进程之后的逻辑同样走binder讣告机制，跟之前的恢复没什么区别，这里主要看看onTaskRemoved，如果不需要重启，可以在这里做下处理：

    private void cleanUpRemovedTaskLocked(TaskRecord tr, boolean killProcess) {
        ...
        // Find any running services associated with this app and stop if needed.
        <!--先处理Service，如果有必要清理Service-->
        mServices.cleanUpRemovedTaskLocked(tr, component, new Intent(tr.getBaseIntent()));

        if (!killProcess) {
            return;
        }

        <!--找到跟改Task相关的进程，并决定是否需要kill-->
        ArrayList<ProcessRecord> procsToKill = new ArrayList<>();
        ArrayMap<String, SparseArray<ProcessRecord>> pmap = mProcessNames.getMap();
        for (int i = 0; i < pmap.size(); i++) {
           SparseArray<ProcessRecord> uids = pmap.valueAt(i);
            for (int j = 0; j < uids.size(); j++) {
                ProcessRecord proc = uids.valueAt(j);
                ...<!--满足条件的等待被杀 （不是Home，）-->
                procsToKill.add(proc);
            }
        }

       // 如果可以即可杀，就立刻杀，否则等待下一次评估oomadj的时候杀，不过，总归是要杀的 
        for (int i = 0; i < procsToKill.size(); i++) {
            ProcessRecord pr = procsToKill.get(i);
            if (pr.setSchedGroup == Process.THREAD_GROUP_BG_NONINTERACTIVE
                    && pr.curReceiver == null) {
                pr.kill("remove task", true);
            } else {
                pr.waitingToKill = "remove task";
            }
        }
    }

ActiveServices的cleanUpRemovedTaskLocked

    void cleanUpRemovedTaskLocked(TaskRecord tr, ComponentName component, Intent baseIntent) {
    
        ArrayList<ServiceRecord> services = new ArrayList<>();
        ArrayMap<ComponentName, ServiceRecord> alls = getServices(tr.userId);
        for (int i = alls.size() - 1; i >= 0; i--) {
            ServiceRecord sr = alls.valueAt(i);
            if (sr.packageName.equals(component.getPackageName())) {
                services.add(sr);
            }
        }
        // Take care of any running services associated with the app.
        for (int i = services.size() - 1; i >= 0; i--) {
            ServiceRecord sr = services.get(i);
       	 // 如果是通过startRequested启动
            if (sr.startRequested) {
                if ((sr.serviceInfo.flags&ServiceInfo.FLAG_STOP_WITH_TASK) != 0) {
                    stopServiceLocked(sr);
                } else {
                <!--作为remove的一部分，这里pendingStarts的add主要是为了回到onStartCommand，而且这个时候，进程还没死呢，否则通知个屁啊-->
                    sr.pendingStarts.add(new ServiceRecord.StartItem(sr,  taskremover =true,
                            sr.makeNextStartId(), baseIntent, null));
                    if (sr.app != null && sr.app.thread != null) {
                        try {
                            sendServiceArgsLocked(sr, true, false);
                        } ...
                    }
 
其实从最近任务列表删除最近任务的时候，处理很简单，如果Service设置了ServiceInfo.FLAG_STOP_WITH_TASK，那么左滑删除后，Service不用重启，也不会处理 onTaskRemoved，直接干掉，否则，是需要往pendingStarts填充ServiceRecord.StartItem，这样在sendServiceArgsLocked才能发送onTaskRemoved请求，为了跟启动onStartCommand分开，ServiceRecord.StartItem的taskremover被设置成true，这样在回调ActiviyThread的handleServiceArgs就会走onTaskRemoved分支如下：

    private void handleServiceArgs(ServiceArgsData data) {
        Service s = mServices.get(data.token);
        if (s != null) {
            try {
                if (data.args != null) {
                    data.args.setExtrasClassLoader(s.getClassLoader());
                    data.args.prepareToEnterProcess();
                }
                int res;
                // 如果没有 taskRemoved，如果taskRemoved 则回调onTaskRemoved
                if (!data.taskRemoved) {
                    res = s.onStartCommand(data.args, data.flags, data.startId);
                } else {
                    s.onTaskRemoved(data.args);
                    res = Service.START_TASK_REMOVED_COMPLETE;
                }

				....
				

因此，从最近任务列表删除，可以看做是仅仅多了个一个onTaskRemoved在这个会调中，用户可以自己处理一些事情，比如中断一些Service处理的事情，保存现场等。


# 总结

* 通过startService启动，但是却没有通过stopService结束的Service并不一定触发重新启动，需要设置相应的onStartCommand返回值，比如START_REDELIVER_INTENT、比START_STICK
* START_REDELIVER_INTENT并不是重发最后一个Intent，看源码是所有Intent
* START_REDELIVER_INTENT同START_STICK重启的延时不一样，START_STICK一般固定1s，而START_REDELIVER_INTENT较长，基本是距离startService的2倍。
* 可以用来做包活，但是不推荐，而且国内也不怎么好用（MIUI、华为等都对AMS做了定制，限制较多）