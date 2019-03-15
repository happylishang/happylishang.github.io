# 场景：探究下什么是后台启动Service

	public class LabApplication extends Application {
	    @Override
	    public void onCreate() {
	        super.onCreate();
	          Intent intent = new Intent( this, BackGroundService.class);
	        startService(intent);
	    }
	 }
  
	  public class BackGroundService extends Service {
	    @Nullable
	    @Override
	    public IBinder onBind(Intent intent) {
	        return null;
	    }
	
	    @Override
	    public int onStartCommand(Intent intent, int flags, int startId) {
	        LogUtils.v("onStartCommand");
	        return START_STICKY;
	    }
	}
	
 第一次冷启动没问题，如果我们杀死APP后，应用再回复的时候就会出现如下Crash（禁止后台启动Service的Crash Log）：
  
![image.png](https://upload-images.jianshu.io/upload_images/1460468-c5c9ad3821e02d49.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

    java.lang.RuntimeException: Unable to create application com.snail.labaffinity.app.LabApplication: java.lang.IllegalStateException: Not allowed to start service Intent { cmp=com.snail.labaffinity/.service.BackGroundService }: app is in background uid UidRecord{72bb30d u0a238 SVC  idle change:idle|uncached procs:1 seq(0,0,0)}
        at android.app.ActivityThread.handleBindApplication(ActivityThread.java:5925)
        at android.app.ActivityThread.access$1100(ActivityThread.java:200)
        at android.app.ActivityThread$H.handleMessage(ActivityThread.java:1656)
        at android.os.Handler.dispatchMessage(Handler.java:106)
        at android.os.Looper.loop(Looper.java:193)
        at android.app.ActivityThread.main(ActivityThread.java:6718)
        at java.lang.reflect.Method.invoke(Native Method)
        at com.android.internal.os.RuntimeInit$MethodAndArgsCaller.run(RuntimeInit.java:493)
        at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:858)

这个是为什么呢？为什么冷启动没问题，恢复就有问题

          
>  ContextImpl.java 
  
    private ComponentName startServiceCommon(Intent service, boolean requireForeground,
            UserHandle user) {
        try {
            validateServiceIntent(service);
            service.prepareToLeaveProcess(this);
            ComponentName cn = ActivityManager.getService().startService(
                mMainThread.getApplicationThread(), service, service.resolveTypeIfNeeded(
                            getContentResolver()), requireForeground,
                            getOpPackageName(), user.getIdentifier());
            if (cn != null) {
                if (cn.getPackageName().equals("!")) {
                    throw new SecurityException(
                            "Not allowed to start service " + service
                            + " without permission " + cn.getClassName());
                } else if (cn.getPackageName().equals("!!")) {
                    throw new SecurityException(
                            "Unable to start service " + service
                            + ": " + cn.getClassName());
                } else if (cn.getPackageName().equals("?")) {
                    throw new IllegalStateException(
                            "Not allowed to start service " + service + ": " + cn.getClassName());
                }
            }
            return cn;
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }
    
> ActiveServices.java
 
         // If this isn't a direct-to-foreground start, check our ability to kick off an
        // arbitrary service
        if (forcedStandby || (!r.startRequested && !fgRequired)) {
            // Before going further -- if this app is not allowed to start services in the
            // background, then at this point we aren't going to let it period.
            final int allowed = mAm.getAppStartModeLocked(r.appInfo.uid, r.packageName,
                    r.appInfo.targetSdkVersion, callingPid, false, false, forcedStandby);
            if (allowed != ActivityManager.APP_START_MODE_NORMAL) {
                Slog.w(TAG, "Background start not allowed: service "
                        + service + " to " + r.name.flattenToShortString()
                        + " from pid=" + callingPid + " uid=" + callingUid
                        + " pkg=" + callingPackage + " startFg?=" + fgRequired);
                if (allowed == ActivityManager.APP_START_MODE_DELAYED || forceSilentAbort) {
                    // In this case we are silently disabling the app, to disrupt as
                    // little as possible existing apps.
                    return null;
                }
                if (forcedStandby) {
                    // This is an O+ app, but we might be here because the user has placed
                    // it under strict background restrictions.  Don't punish the app if it's
                    // trying to do the right thing but we're denying it for that reason.
                    if (fgRequired) {
                        if (DEBUG_BACKGROUND_CHECK) {
                            Slog.v(TAG, "Silently dropping foreground service launch due to FAS");
                        }
                        return null;
                    }
                }
                // This app knows it is in the new model where this operation is not
                // allowed, so tell it what has happened.
                UidRecord uidRec = mAm.mActiveUids.get(r.appInfo.uid);
                return new ComponentName("?", "app is in background uid " + uidRec);
            }
        }

> ActivityManagerService.java

	  int getAppStartModeLocked(int uid, String packageName, int packageTargetSdk,
	            int callingPid, boolean alwaysRestrict, boolean disabledOnly, boolean forcedStandby) {
	        UidRecord uidRec = mActiveUids.get(uid);
	        if (DEBUG_BACKGROUND_CHECK) Slog.d(TAG, "checkAllowBackground: uid=" + uid + " pkg="
	                + packageName + " rec=" + uidRec + " always=" + alwaysRestrict + " idle="
	                + (uidRec != null ? uidRec.idle : false));
	        if (uidRec == null || alwaysRestrict || forcedStandby || uidRec.idle) {
	            boolean ephemeral;
	            if (uidRec == null) {
	                ephemeral = getPackageManagerInternalLocked().isPackageEphemeral(
	                        UserHandle.getUserId(uid), packageName);
	            } else {
	                ephemeral = uidRec.ephemeral;
	            }
	
	            if (ephemeral) {
	                // We are hard-core about ephemeral apps not running in the background.
	                return ActivityManager.APP_START_MODE_DISABLED;
	            } else {
	                if (disabledOnly) {
	                    // The caller is only interested in whether app starts are completely
	                    // disabled for the given package (that is, it is an instant app).  So
	                    // we don't need to go further, which is all just seeing if we should
	                    // apply a "delayed" mode for a regular app.
	                    return ActivityManager.APP_START_MODE_NORMAL;
	                }
	                final int startMode = (alwaysRestrict)
	                        ? appRestrictedInBackgroundLocked(uid, packageName, packageTargetSdk)
	                        : appServicesRestrictedInBackgroundLocked(uid, packageName,
	                                packageTargetSdk);
	                if (DEBUG_BACKGROUND_CHECK) {
	                    Slog.d(TAG, "checkAllowBackground: uid=" + uid
	                            + " pkg=" + packageName + " startMode=" + startMode
	                            + " onwhitelist=" + isOnDeviceIdleWhitelistLocked(uid, false)
	                            + " onwhitelist(ei)=" + isOnDeviceIdleWhitelistLocked(uid, true));
	                }
	                if (startMode == ActivityManager.APP_START_MODE_DELAYED) {
	                    // This is an old app that has been forced into a "compatible as possible"
	                    // mode of background check.  To increase compatibility, we will allow other
	                    // foreground apps to cause its services to start.
	                    if (callingPid >= 0) {
	                        ProcessRecord proc;
	                        synchronized (mPidsSelfLocked) {
	                            proc = mPidsSelfLocked.get(callingPid);
	                        }
	                        if (proc != null &&
	                                !ActivityManager.isProcStateBackground(proc.curProcState)) {
	                            // Whoever is instigating this is in the foreground, so we will allow it
	                            // to go through.
	                            return ActivityManager.APP_START_MODE_NORMAL;
	                        }
	                    }
	                }
	                return startMode;
	            }
	        }
	        return ActivityManager.APP_START_MODE_NORMAL;
	    }
	
	
 
        
# 什么时候是后台启动？

    boolean isUidActiveLocked(int uid) {
        final UidRecord uidRecord = mActiveUids.get(uid);
        return uidRecord != null && !uidRecord.setIdle;
    }
        