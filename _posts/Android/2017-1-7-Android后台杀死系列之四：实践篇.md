---
layout: post
title: "Android后台杀死系列之四：实践篇"
category: Android

---
 
![App操作影响进程优先级](http://upload-images.jianshu.io/upload_images/1460468-dec3e577ea74f0e8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

前面写了三篇分析，那么究竟要怎么用呢，没用你研究它干嘛，在第一篇注意事项的时候，只是为了防止一些异常之类的做了一些处理

这里针对异常杀死的一些需求

1、异常杀死后，再次打开完全重启（有个网友问的）
2、进程保活
3、
  
# 进程保活 

进程包活可能是APP开发比较关心的地方，  
	        
###  参考文档

[Fragment Transactions & Activity State Loss](http://www.androiddesignpatterns.com/2013/08/fragment-transaction-commit-state-loss.html)精          
[Lowmemorykiller笔记](http://blog.csdn.net/guoqifa29/article/details/45370561) **精**       
[Fragment实例化，Fragment生命周期源码分析](http://johnnyyin.com/2015/05/19/android-fragment-life-cycle.html)      
[ android.app.Fragment$InstantiationException的原因分析](http://blog.csdn.net/sun927/article/details/46629919)      
[Android Framework架构浅析之【近期任务】](http://blog.csdn.net/lnb333666/article/details/7869465)      
[Android Low Memory Killer介绍](http://mysuperbaby.iteye.com/blog/1397863)      
[Android开发之InstanceState详解]( http://www.cnblogs.com/hanyonglu/archive/2012/03/28/2420515.html )      
[Square：从今天开始抛弃Fragment吧！](http://www.jcodecraeer.com/a/anzhuokaifa/androidkaifa/2015/0605/2996.html)      
[对Android近期任务列表（Recent Applications）的简单分析](http://www.cnblogs.com/coding-way/archive/2013/06/05/3118732.html)      
[ Android——内存管理-lowmemorykiller 机制](http://blog.csdn.net/jscese/article/details/47317765)          
[ ActivityStackSupervisor分析](http://blog.csdn.net/guoqifa29/article/details/40015127)      
[A Deeper Look of ViewPager and FragmentStatePagerAdaper](http://billynyh.github.io/blog/2014/03/02/fragment-state-pager-adapter/)      
[View的onSaveInstanceState和onRestoreInstanceState过程分析](http://www.cnblogs.com/xiaoweiz/p/3813914.html)      