# View显示逻辑

## onResume的时候显示原理

## App启动后如何Attach走后续流程--启动第一个Activity

## 显示与更新的逻辑

## Token，ActivityRecord   new Token  ActivityClientRecord对应 

## 几种关系 Window与PhoneWindow WindowSession WindowManager  WIndowManagerService

## Window有什么用


* 显示原理
* 更新原理
* 动画
* 背景
* 管理
* 通信

# ViewRootImpl作用

* A：链接WindowManager和DecorView的纽带，更广一点可以说是Window和View之间的纽带。
* B：完成View的绘制过程，包括measure、layout、draw过程。
* C：向DecorView分发收到的用户发起的event事件，如按键，触屏等事件。

# 参考文档

[Android中的ViewRootImpl类源码解析  ](http://blog.csdn.net/qianhaifeng2012/article/details/51737370)       
[View的事件分发机制源码解析](http://blog.csdn.net/qianhaifeng2012/article/details/51674022)