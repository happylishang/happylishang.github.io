---
layout: post
title: 从Toast显示原理初窥Android窗口管理系统 
category: Android

---
 
Android窗口管理系统是非常大的一块，如果真要牵扯的话要设计AMS、InputManagerService、输入管理等，分析这么复杂的一个系统，如果直接扎进入看源码，可能会比较混论，所以这里找一个Toast显示原理作为切入点，希望能简单点。先看下最简单的Toast用法：

	Toast.makeText(context , msg, Toast.LENGTH_SHORT).show(); 
跟一下源码:

    public static Toast makeText(Context context, CharSequence text, int duration) {
        Toast result = new Toast(context);
        LayoutInflater inflate = (LayoutInflater)
                context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View v = inflate.inflate(com.android.internal.R.layout.transient_notification, null);
        TextView tv = (TextView)v.findViewById(com.android.internal.R.id.message);
        tv.setText(text);
        result.mNextView = v;
        result.mDuration = duration;
        return result;
    }
 最后看下show的关键代码

	    public void show() {
	        if (mNextView == null) {
	            throw new RuntimeException("setView must have been called");
	        }
	        INotificationManager service = getService();
	        String pkg = mContext.getPackageName();
	        TN tn = mTN;
	        tn.mNextView = mNextView;
	        try {
	            service.enqueueToast(pkg, tn, mDuration);
	        } catch (RemoteException e) {
	            // Empty
	        }
	    } 
首先获取通知管理服务

    static private INotificationManager getService() {
        if (sService != null) {
            return sService;
        }
        sService = INotificationManager.Stub.asInterface(ServiceManager.getService("notification"));
        return sService;
    }  

之后将Toast显示的请求加入到通知队列中去：继续跟下NotificationManagerService
	
	 public void enqueueToast(String pkg, ITransientNotification callback, int duration)
	    {
	
	        if (pkg == null || callback == null) {
	            Slog.e(TAG, "Not doing toast. pkg=" + pkg + " callback=" + callback);
	            return ;
	        }
	
	        final boolean isSystemToast = isCallerSystem() || ("android".equals(pkg));
	
	        if (ENABLE_BLOCKED_TOASTS && !noteNotificationOp(pkg, Binder.getCallingUid())) {
	            if (!isSystemToast) {
	                Slog.e(TAG, "Suppressing toast from package " + pkg + " by user request.");
	                return;
	            }
	        }
	
	        synchronized (mToastQueue) {
	            int callingPid = Binder.getCallingPid();
	            long callingId = Binder.clearCallingIdentity();
	            try {
	                ToastRecord record;
	                int index = indexOfToastLocked(pkg, callback);
	                // If it's already in the queue, we update it in place, we don't
	                // move it to the end of the queue.
	                if (index >= 0) {
	                    record = mToastQueue.get(index);
	                    record.update(duration);
	                } else {
	                    // Limit the number of toasts that any given package except the android
	                    // package can enqueue.  Prevents DOS attacks and deals with leaks.
	                    if (!isSystemToast) {
	                        int count = 0;
	                        final int N = mToastQueue.size();
	                        for (int i=0; i<N; i++) {
	                             final ToastRecord r = mToastQueue.get(i);
	                             if (r.pkg.equals(pkg)) {
	                                 count++;
	                                 if (count >= MAX_PACKAGE_NOTIFICATIONS) {
	                                     Slog.e(TAG, "Package has already posted " + count
	                                            + " toasts. Not showing more. Package=" + pkg);
	                                     return;
	                                 }
	                             }
	                        }
	                    }
	
	                    record = new ToastRecord(callingPid, pkg, callback, duration);
	                    mToastQueue.add(record);
	                    index = mToastQueue.size() - 1;
	                    keepProcessAliveLocked(callingPid);
	                }
	                // If it's at index 0, it's the current toast.  It doesn't matter if it's
	                // new or just been updated.  Call back and tell it to show itself.
	                // If the callback fails, this will remove it from the list, so don't
	                // assume that it's valid after this.
	                if (index == 0) {
	                    showNextToastLocked();
	                }
	            } finally {
	                Binder.restoreCallingIdentity(callingId);
	            }
	        }
	    }


