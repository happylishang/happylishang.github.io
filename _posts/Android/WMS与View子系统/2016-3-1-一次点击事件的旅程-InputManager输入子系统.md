---
layout: default
title: 一次点击事件的旅程 - InputManager输入子系统 
category: [android]

---


### Linux内核输入子系统

触摸事件是由Linux内核的一个Input子系统来管理的(InputManager)，Linux子系统会在 /dev/input/ 这个路径下创建硬件输入设备节点(这里的硬件设备就是我们的触摸屏了)。当手指触动触摸屏时，硬件设备通过设备节点像内核(其实是InputManager管理)报告事件，InputManager 经过处理将此事件传给 Android系统的一个系统Service： WindowManagerService 。

 
 <img src="http://stackvoid.com/album/2014-09-30-details-dispatch-onTouch-Event-in-Android-01.gif" width="800"/>
 
 WindowManagerService调用dispatchPointer()从存放WindowState的z-order顺序列表中找到能接收当前touch事件的 WindowState，通过IWindow代理将此消息发送到IWindow服务端(IWindow.Stub子类)，这个IWindow.Stub属于ViewRoot(这个类继承Handler，主要用于连接PhoneWindow和WindowManagerService)，所以事件就传到了ViewRoot.dispatchPointer()中.
 
 我们来看一下ViewRoot的dispatchPointer方法：、
 
	  public void dispatchPointer(MotionEvent event, long eventTime,
	              boolean callWhenDone) {
	         Message msg = obtainMessage(DISPATCH_POINTER);
	         msg.obj = event;
	         msg.arg1 = callWhenDone ? 1 : 0;
	         sendMessageAtTime(msg, eventTime);
	      }
	
dispatchPointer方法就是把这个事件封装成Message发送出去，在ViewRoot Handler的handleMessage中被处理，其调用了mView.dispatchTouchEvent方法(mView是一个PhoneWindow.DecorView对象)，PhoneWindow.DecorView继承FrameLayout(FrameLayout继承ViewGroup，ViewGroup继承自View),DecorView里的dispatchTouchEvent方法如下. 这里的Callback的cb其实就是Activity的attach()方法里的设置回调。

        @Override
        public boolean dispatchTouchEvent(MotionEvent ev) {
            final Callback cb = getCallback();
            return cb != null && mFeatureId < 0 ? cb.dispatchTouchEvent(ev) : super
                    .dispatchTouchEvent(ev);
        }
	
也就是说，正常情形下，当前的Activity就是这里的cb，即调用了Activity的dispatchTouchEvent方法。

下面来分析一下从Activity到各个子View的事件传递和处理过程。
首先先分析Activity的dispatchTouchEvent方法。

    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (ev.getAction() == MotionEvent.ACTION_DOWN) {
            onUserInteraction();
        }
        if (getWindow().superDispatchTouchEvent(ev)) {
            return true;
        }
        return onTouchEvent(ev);
    }
	
onUserInteraction() 是一个空方法，开发者可以根据自己的需求覆写这个方法(这个方法在一个Touch事件的周期肯定会调用到的)。如果判断成立返回True，当前事件就不在传播下去了。 superDispatchTouchEvent(ev) 这个方法做了什么呢？ getWindow().superDispatchTouchEvent(ev) 也就是调用了 PhoneWindow.superDispatchTouchEvent 方法，而这个方法返回的是 mDecor.superDispatchTouchEvent(event)，在内部类 DecorView(上文中的mDecor) 的superDispatchTouchEvent 中调用super.dispatchTouchEvent(event)，而DecorView继承自ViewGroup(通过FrameLayout，FrameLayout没有dispatchTouchEvent)，最终调用的是ViewGroup的dispatchTouchEvent方法。

	               --> performLaunchActivity(ActivityRecord, Intent) : Activity - android.app.ActivityThread
	               
performLaunchActivity我们很熟识，因为我前面在讲Activity启动过程详解时候讲过，在启动一个新的Activity会执行该方法，在该方法里面会执行attach方法，找到attach方法对应代码可以看到：

	        mWindow = PolicyManager.makeNewWindow(this);
	        mWindow.setCallback(this);

mWindow就是一个PhoneWindow，它是Activity的一个内部成员，通过调用mWindow的setCallback(this)，把新建立的Activity设置为PhoneWindow一个mCallback成员，这样我们就清楚了，前面的cb就是拥有这个PhoneWindow的Activity,cb.dispatchTouchEvent(ev)也就是执行：Activity.dispatchTouchEvent


Event事件是首先到了 PhoneWindow 的 DecorView 的 dispatchTouchEvent 方法，此方法通过 CallBack 调用了 Activity 的 dispatchTouchEvent 方法，在 Activity 这里，我们可以重写 Activity 的dispatchTouchEvent 方法阻断 touch事件的传播。接着在Activity里的dispatchTouchEvent 方法里，事件又再次传递到DecorView，DecorView通过调用父类(ViewGroup)的dispatchTouchEvent 将事件传给父类处理，也就是我们下面要分析的方法，这才进入网上大部分文章讲解的touch事件传递流程。

为什么要从 PhoneWindow.DecorView 中 传到 Activity，然后在传回 PhoneWindow.DecorView 中呢？ 主要是为了方便在Activity中通过控制dispatchTouchEvent 来控制当前Activity 事件的分发， 下一篇关于数据埋点文章就应用了这个机制，我们要重点分析的就是ViewGroup中的dispatchTouchEvent方法。 

 ViewGroup 的 dispatchTouchEvent 的调用过程。
 
* 首先判断此 MotionEvent 能否被拦截，如果是的话，能调用我们覆写 onInterceptTouchEvent来处理拦截到的事件；如果此方法返回TRUE，表示需要拦截，那么事件到此为止，就不会传递到子View中去。这里要注意，onInterceptTouchEvent 方法默认是返回FALSE。
  
* 若没有拦截此Event，首先找到此ViewGroup中所有的子View，通过方法 canViewReceivePointerEvents和isTransformedTouchPointInView，对每个子View通过坐标(Event事件坐标和子View坐标比对)计算，找到坐标匹配的View。
 
* 调用dispatchTransformedTouchEvent方法，处理Event事件。


> **用户点击屏幕产生Touch(包括DOWN、UP、MOVE，本文分析的是DOWN)事件 
> -> InputManager
> -> WindowManagerService.dispatchPointer() 
> -> IWindow.Stub 
> -> ViewRoot.dispatchPointer() 
> -> PhoneWindow.DecorView.dispatchTouchEvent() 
> -> Activity.dispatchTouchEvent() 
> -> PhoneWindow.superDispatchTouchEvent 
> -> PhoneWindow.DecorView.superDispatchTouchEvent 
> -> ViewGroup.dispatchTouchEvent() 
> -> ViewGroup.dispatchTransformedTouchEvent() 
> -> 子View.dispatchTouchEvent() 
> -> 子View.onTouch() 
> -> 子View.onTouchEvent() 
> -> 事件被消费结束。(这个过程是由上往下传导)
> -> 如果事件没有被子View消费，也就是说子View的dispatchTouchEvent返回false，此时事件由其父类处理(由下往上传导)，最后到达系统边界也没处理，就将此事件抛弃了。**

###  参考文档

Android 事件分发机制详解 <http://stackvoid.com/details-dispatch-onTouch-Event-in-Android/>
