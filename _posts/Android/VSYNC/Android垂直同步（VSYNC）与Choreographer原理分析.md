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