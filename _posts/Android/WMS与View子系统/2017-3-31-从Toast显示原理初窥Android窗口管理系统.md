---
layout: post
title: 从Toast显示原理初窥Android窗口管理系统 
category: Android
image: http://upload-images.jianshu.io/upload_images/1460468-a11e1f92cfa98c7c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

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

之后将Toast显示的请求加入到通知队列中去：继续跟下NotificationManagerService，在NotificationManagerService中，真正的服务对象是INotificationManager.Stub

    private final IBinder mService = new INotificationManager.Stub() {
    }

因此代码如下

	
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
	 
	                if (index >= 0) {
	                    record = mToastQueue.get(index);
	                    record.update(duration);
	                } else {
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
	                if (index == 0) {
	                    showNextToastLocked();
	                }
	            } finally {
	                Binder.restoreCallingIdentity(callingId);
	            }
	        }
	    }

从  synchronized (mToastQueue)我们知道，这是个支持多线程的操作的对象，接着看下showNextToastLocked，如果当前没有Toast在显示，就会执行showNextToastLocked，

    void showNextToastLocked() {
        ToastRecord record = mToastQueue.get(0);
        while (record != null) {
            try {
                record.callback.show();
                scheduleTimeoutLocked(record);
                return;
            } catch (RemoteException e) {

                int index = mToastQueue.indexOf(record);
                if (index >= 0) {
                    mToastQueue.remove(index);
                }
                keepProcessAliveLocked(record.pid);
                if (mToastQueue.size() > 0) {
                    record = mToastQueue.get(0);
                } else {
                    record = null;
                }
            }
        }
    }

可见showNextToastLocked会显示所有队列中的Toast，一次调用CallBack，CallBack其实是Binder通信，这里就是TN的代理，将View添加到Window，之后再进行scheduleTimeoutLocked监听

    private void scheduleTimeoutLocked(ToastRecord r)
    {
        mHandler.removeCallbacksAndMessages(r);
        Message m = Message.obtain(mHandler, MESSAGE_TIMEOUT, r);
        long delay = r.duration == Toast.LENGTH_LONG ? LONG_DELAY : SHORT_DELAY;
        mHandler.sendMessageDelayed(m, delay);
    }
    
    
        private static class TN extends ITransientNotification.Stub {
        final Runnable mShow = new Runnable() {
            @Override
            public void run() {
                handleShow();
            }
        };

        final Runnable mHide = new Runnable() {
            @Override
            public void run() {
                handleHide();
                // Don't do this in handleHide() because it is also invoked by handleShow()
                mNextView = null;
            }
        };

        private final WindowManager.LayoutParams mParams = new WindowManager.LayoutParams();
        final Handler mHandler = new Handler();    

        int mGravity;
        int mX, mY;
        float mHorizontalMargin;
        float mVerticalMargin;


        View mView;
        View mNextView;

        WindowManager mWM;

        TN() {
            // XXX This should be changed to use a Dialog, with a Theme.Toast
            // defined that sets up the layout params appropriately.
            final WindowManager.LayoutParams params = mParams;
            params.height = WindowManager.LayoutParams.WRAP_CONTENT;
            params.width = WindowManager.LayoutParams.WRAP_CONTENT;
            params.format = PixelFormat.TRANSLUCENT;
            params.windowAnimations = com.android.internal.R.style.Animation_Toast;
            params.type = WindowManager.LayoutParams.TYPE_TOAST;
            params.setTitle("Toast");
            params.flags = WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
                    | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                    | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE;
        }

        /**
         * schedule handleShow into the right thread
         */
        @Override
        public void show() {
            if (localLOGV) Log.v(TAG, "SHOW: " + this);
            mHandler.post(mShow);
        }

        /**
         * schedule handleHide into the right thread
         */
        @Override
        public void hide() {
            if (localLOGV) Log.v(TAG, "HIDE: " + this);
            mHandler.post(mHide);
        }

        public void handleShow() {
            if (localLOGV) Log.v(TAG, "HANDLE SHOW: " + this + " mView=" + mView
                    + " mNextView=" + mNextView);
            if (mView != mNextView) {
                // remove the old view if necessary
                handleHide();
                mView = mNextView;
                Context context = mView.getContext().getApplicationContext();
                String packageName = mView.getContext().getOpPackageName();
                if (context == null) {
                    context = mView.getContext();
                }
                mWM = (WindowManager)context.getSystemService(Context.WINDOW_SERVICE);
                // We can resolve the Gravity here by using the Locale for getting
                // the layout direction
                final Configuration config = mView.getContext().getResources().getConfiguration();
                final int gravity = Gravity.getAbsoluteGravity(mGravity, config.getLayoutDirection());
                mParams.gravity = gravity;
                if ((gravity & Gravity.HORIZONTAL_GRAVITY_MASK) == Gravity.FILL_HORIZONTAL) {
                    mParams.horizontalWeight = 1.0f;
                }
                if ((gravity & Gravity.VERTICAL_GRAVITY_MASK) == Gravity.FILL_VERTICAL) {
                    mParams.verticalWeight = 1.0f;
                }
                mParams.x = mX;
                mParams.y = mY;
                mParams.verticalMargin = mVerticalMargin;
                mParams.horizontalMargin = mHorizontalMargin;
                mParams.packageName = packageName;
                if (mView.getParent() != null) {
                    if (localLOGV) Log.v(TAG, "REMOVE! " + mView + " in " + this);
                    mWM.removeView(mView);
                }
                if (localLOGV) Log.v(TAG, "ADD! " + mView + " in " + this);
                mWM.addView(mView, mParams);
                trySendAccessibilityEvent();
            }
        }

        private void trySendAccessibilityEvent() {
            AccessibilityManager accessibilityManager =
                    AccessibilityManager.getInstance(mView.getContext());
            if (!accessibilityManager.isEnabled()) {
                return;
            }
            // treat toasts as notifications since they are used to
            // announce a transient piece of information to the user
            AccessibilityEvent event = AccessibilityEvent.obtain(
                    AccessibilityEvent.TYPE_NOTIFICATION_STATE_CHANGED);
            event.setClassName(getClass().getName());
            event.setPackageName(mView.getContext().getPackageName());
            mView.dispatchPopulateAccessibilityEvent(event);
            accessibilityManager.sendAccessibilityEvent(event);
        }        

        public void handleHide() {
            if (localLOGV) Log.v(TAG, "HANDLE HIDE: " + this + " mView=" + mView);
            if (mView != null) {
                // note: checking parent() just to make sure the view has
                // been added...  i have seen cases where we get here when
                // the view isn't yet added, so let's try not to crash.
                if (mView.getParent() != null) {
                    if (localLOGV) Log.v(TAG, "REMOVE! " + mView + " in " + this);
                    mWM.removeView(mView);
                }

                mView = null;
            }
        }
    }

可以看到，就是通过WindowManager，添加或者移除View，而这两个的时机都是通过NotificationManagerService控制的

如果有多个，那么就在取消上一个之后，再显示下一个

    void cancelToastLocked(int index) {
        ToastRecord record = mToastQueue.get(index);
        try {
            record.callback.hide();
        } catch (RemoteException e) {
            Slog.w(TAG, "Object died trying to hide notification " + record.callback
                    + " in package " + record.pkg);
            // don't worry about this, we're about to remove it from
            // the list anyway
        }
        mToastQueue.remove(index);
        keepProcessAliveLocked(record.pid);
        if (mToastQueue.size() > 0) {
            // Show the next one. If the callback fails, this will remove
            // it from the list, so don't assume that the list hasn't changed
            // after this point.
            showNextToastLocked();
        }
    }
 如此衔接上
 
 
 以上就是Toast的显示与有隐藏原理   
 
 为什么一定要在主线程Toast？
 
 其实不一定非要主线程，因为APP端用到了Handler，Handler是依赖Looper的，必须是Looper线程才行，所以这里并指定到主线程。



![Toast显示原理.png](http://upload-images.jianshu.io/upload_images/1460468-a11e1f92cfa98c7c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)