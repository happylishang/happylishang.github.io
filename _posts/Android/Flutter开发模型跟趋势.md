 
## Flutter开发模型跟趋势

### Flutter现状
Flutter由Chrome团队孵化，自带跨端开发的影子，不过目前能力有限

* 支持Android iOS native统一开发，能实现APP端统一。
* 支持热重载（仅仅本地开发使用，还未扩展到动态化）

###  Flutter UI开发模型

Flutter界面开发更像iOS的风格，一个Window中实现各种View的嵌套，没有Android Activity多窗口的概念，页面间跳转采用iOS的navigationcontroler控制方式。UI组织上采用的是一种嵌套的方式，Android、iOS、前端都能比较快的上手，代码风格如下：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-17917fa3c42f99bf.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


### Flutter 缺点

*  目前不支持线上动态化（仅本地热重载）
* 目前还不支持web端 

### Flutter进化路径

* 2019支持全平台（来自Flutter团队声明）
* 2019支持动态化（来自Flutter团队声明）

### Flutter预演结论

目前还不适合直接上，等支持动态化能力之后再上，否则跟目前发版节奏没多大区别