# 最主要的一点：VSYNC同步信号的接受要用户主动去注册，才会接受，而且是单次有效



有几个触发要区分清楚

* Input输入
* VSYNC输入
* INVALID消息输入
* Chorgrapher自己的几个MessageQueue

流程：

* 1 invalide需要重绘或者Input输入存在
* 2 去异步（oneway）请求VSYNC同步信号
* 3 VSYNC信号到来，重绘

也就是说垂直同步信号 是需要Client主动去请求的，否则VSYNC不会被通知到Client

垂直同步跟UI更新，跟消息处理、动画更新是两个完全不同的东西，前者属于引擎，后者属于业务

# 对此requestLayout跟invalid都不会重复调用布局测绘

    <ProgressBar
        android:id="@+id/progress_bar"
        android:layout_width="30dp"
        android:layout_height="30dp"
        android:layout_centerInParent="true"/>
        
        

输入DisplayEventReceiver       WindowInputEventReceiver    ConsumeBatchedInputRunnable 

![image.png](https://upload-images.jianshu.io/upload_images/1460468-e6173e52c5e28102.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![image.png](https://upload-images.jianshu.io/upload_images/1460468-59db43c5821639d6.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

https://digitalassetlinks.googleapis.com/v1/statements:list?
   source.web.site=https://you.163.com
   relation=delegate_permission/common.handle_all_urls

 