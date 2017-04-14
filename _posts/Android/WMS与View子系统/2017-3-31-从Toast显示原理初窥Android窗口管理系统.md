---
layout: post
title: 从Toast显示原理初窥Android窗口管理系统 
category: Android
image: http://upload-images.jianshu.io/upload_images/1460468-a11e1f92cfa98c7c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---
 
Android窗口管理系统是非常大的一块，涉及AMS、InputManagerService、输入法管理等，这么复杂的一个系统，如果直接扎进入分析看源码可能会比较混乱，所以，本文以Toast显示原理作为切入点，希望能简单点初窥一下WMS。首先，简单看下Toast用法：

	Context context = getApplicationContext();
	CharSequence text = "Hello toast!";
	int duration = Toast.LENGTH_SHORT;
	Toast toast = Toast.makeText(context, text, duration);
	toast.show(); 
#  Toast的显示原理

下面跟一下源码：

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
 可以看到makeText仅仅是新建了一个Toast实例，并为其创建了一个无主TextView，并没多少特殊逻辑。那么看下关键的show代码：

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
	        }
	    } 
这里首先通过getService获取通知管理服务，

    static private INotificationManager getService() {
        if (sService != null) {
            return sService;
        }
        sService = INotificationManager.Stub.asInterface(ServiceManager.getService("notification"));
        return sService;
    }  

之后再将Toast的显示请求发送给该服务，在发送的过程中传递一个Binder实体，提供给NotificationManagerService回调使用，不过如果看下NotificationManagerService就会发现，该类并不是Binder实体，所以本身不是服务逻辑的承载体，在NotificationManagerService中，真正的服务对象是INotificationManager.Stub，因此到Service端，真正请求的服务是INotificationManager.Stub的enqueueToast：

    private final IBinder mService = new INotificationManager.Stub() {
	
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
		
		...    }
	}
	
从上面的synchronized (mToastQueue)可以知道，这是个支持多线程的操作的对象，其实很好立即，既然上面牵扯到插入节点的操作，那么就一定在某个地方有摘除节点的操作。接着看下showNextToastLocked，如果当前没有Toast在显示，就会执行showNextToastLocked，当然如果有正在显示的Toast，这里就只执行插入操作，其实这里有点小计俩，那就是下一个Toast的执行是依赖超时进行处理的，也就是必须等到生一个Toast超时，显示完毕，才显示下一个Toast，具体让下看：

    void showNextToastLocked() {
        ToastRecord record = mToastQueue.get(0);
        while (record != null) {
            try {
            <!--关键点1-->
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

看一下关键点1，这里虽然是while循环，但是只取到一个有效的ToastRecord就返回了，也就是队列上的后续TaskRecord要依赖其他手段来显示了。这里并没看到WindowManagerService的身影，其实View添加到窗口显示的时机都是在APP端，而不是在服务端，对这里而言，就是通过CallBack回调，前面不是传递过来一个Binder实体么，这个实体在NotificationManagerService端就是作为Proxy，以回调APP端，其实Android里面的系统服务都是采用这种处理模式APP与Service互为C/S，record.callback就是APP端TN的代理，这里简单看一下其实现：


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
 
	        @Override
	        public void show() {
	            if (localLOGV) Log.v(TAG, "SHOW: " + this);
	            mHandler.post(mShow);
	        }
            ...
            
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
	 	            <!--关键点1-->
	                    mWM.removeView(mView);
	                }
	                if (localLOGV) Log.v(TAG, "ADD! " + mView + " in " + this);
	                mWM.addView(mView, mParams);
	                trySendAccessibilityEvent();
	            }
	        }
	

	        public void handleHide() {
	            if (localLOGV) Log.v(TAG, "HANDLE HIDE: " + this + " mView=" + mView);
	            if (mView != null) {
	            <!--关键点2-->
	                if (mView.getParent() != null) {
	                    mWM.removeView(mView);
	                }
	
	                mView = null;
	            }
	        }
	    }
   
 其show函数，归根到底就是通过WindowManagerService，将View添加到Window， mWM.addView(mView, mParams);这样Toast就显示出来了。那么怎么隐藏呢？不能一个Toast总是占据屏幕吧。
 
#  Toast的隐藏原理
 
 接着看NotificationManagerService端的showNextToastLocked函数，在callback后，会继续通过scheduleTimeoutLocked为Toast添加一个TimeOut监听，并利用该监听将过期的Toast从系统移出,看下实现：
 
     void showNextToastLocked() {
        ToastRecord record = mToastQueue.get(0);
        while (record != null) {
            try {
            <!--关键点1-->
                record.callback.show();
             <!--关键点2-->
                scheduleTimeoutLocked(record);
                return;
            } catch (RemoteException e) {
			...
        }
    }

scheduleTimeoutLocked其实就是通过Handler添加一个延时执行的Action，

    private void scheduleTimeoutLocked(ToastRecord r)
    {
        mHandler.removeCallbacksAndMessages(r);
        Message m = Message.obtain(mHandler, MESSAGE_TIMEOUT, r);
        long delay = r.duration == Toast.LENGTH_LONG ? LONG_DELAY : SHORT_DELAY;
        mHandler.sendMessageDelayed(m, delay);
    }
 
等到 Timeout的时候，Handler处理该事件，
 
     private void handleTimeout(ToastRecord record)
    {
        synchronized (mToastQueue) {
            int index = indexOfToastLocked(record.pkg, record.callback);
            if (index >= 0) {
                cancelToastLocked(index);
            }
        }
    }

可以看到就是通过cancelToastLocked来隐藏当前显示的Toast，当然，如果队列中还有Toast要显示，就继续showNextToastLocked显示下一个，这里将显示放在cancle里完成Loop监听也挺奇葩的。

    void cancelToastLocked(int index) {
        ToastRecord record = mToastQueue.get(index);
        try {
            record.callback.hide();
        } catch (RemoteException e) {
        }
        mToastQueue.remove(index);
        keepProcessAliveLocked(record.pid);
        if (mToastQueue.size() > 0) {
            showNextToastLocked();
        }
    }
 
callback.hide()其实就是通过WindowManager移除当前View，

        public void handleHide() {
            if (mView != null) {
                if (mView.getParent() != null) {
                    if (localLOGV) Log.v(TAG, "REMOVE! " + mView + " in " + this);
                    mWM.removeView(mView);
                }

                mView = null;
            }
        }

可以看到Toast的显示跟隐藏还是APP端自己处理的，就是通过WindowManager，添加或者移除View，不过这两个时机是通过NotificationManagerService进行管理的，其实就是保证Toast按照顺序一个个显示，防止Toast覆盖， 以上就是Toast的显示与有隐藏原理 ，可以看到这里并未涉及任何的Activity或者其他组件的信息，也就是说View的显示其实可以完全不必依赖Activity，那么是不是子线程也能添加显示View或者更新UI呢，答案是肯定的，有兴趣可以自己看下。
 
# 一个小问题：Toast一定要在主线程？
 
 答案是：并不一定在主线程，但是要在Hanlder可用线程
 
>  方案一：可行
> 
        new Thread() {
            @Override
            public void run() {
                super.run();
                Looper.prepare();
					Context context = getApplicationContext();
					CharSequence text = "Hello toast!";
					int duration = Toast.LENGTH_SHORT;
					Toast toast = Toast.makeText(context, text, duration);
					toast.show();
                Looper.loop();
            }
        }.start();
>  方案二：出错崩溃
 
         new Thread() {
            @Override
            public void run() {
                super.run();
					Context context = getApplicationContext();
					CharSequence text = "Hello toast!";
					int duration = Toast.LENGTH_SHORT;
					Toast toast = Toast.makeText(context, text, duration);
					toast.show();
            }
        }.start();
        
为什么方案一可以，而方案二不行，其实很简单因为方案一提供了Toast运行所需要的Looper环境，在分析Toast显示的时候，APP端是通过Handler执行的，这样做的好处是不阻塞Binder线程，因为在这个点APP端Service端。另外，如果addView的线程不是Loop线程，执行完就结束了，当然就没机会执行后续的请求，这个是由Hanlder的构造函数保证的

    public Handler(Callback callback, boolean async) {
        if (FIND_POTENTIAL_LEAKS) {
            final Class<? extends Handler> klass = getClass();
            if ((klass.isAnonymousClass() || klass.isMemberClass() || klass.isLocalClass()) &&
                    (klass.getModifiers() & Modifier.STATIC) == 0) {
                Log.w(TAG, "The following Handler class should be static or leaks might occur: " +
                    klass.getCanonicalName());
            }
        }

        mLooper = Looper.myLooper();
        if (mLooper == null) {
            throw new RuntimeException(
                "Can't create handler inside thread that has not called Looper.prepare()");
        }
        mQueue = mLooper.mQueue;
        mCallback = callback;
        mAsynchronous = async;
    }
    
 如果Looper==null ，就会报错，而Toast对象在实例化的时候，也会为自己实例化一个Hanlder，这就是为什么说“一定要在主线程”，其实准确的说应该是 “一定要在Looper非空的线程”。
 
    
![Toast显示原理.png](http://upload-images.jianshu.io/upload_images/1460468-a11e1f92cfa98c7c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)