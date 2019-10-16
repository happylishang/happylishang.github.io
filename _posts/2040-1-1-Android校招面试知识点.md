# 项目经验

如果有项目经验建议可以围绕项目让面试者谈自己项目，项目中自己的定位，有哪些突出表现，解决了什么难题，如何解决的，有什么收获，如果面试者项目经验丰富，面试完项目类的知识可以适当聊下Android框架类的东西，看面试者对于Android系统的学习跟理解。

# JAVA基础

* 不同类型单利写法
* 类加载机制（加载的时机、加载器等）
* Java GC （常见的标记清理模型）
* 并发编程及锁的知识（如synchronize关键字的用法）
* HashMap和Hashtable 、StringBuilder 与StringBuffer
* Java的强引用、软引用、弱引用 （LeakCary用那种实现的内存泄露检测）
* HashMap原理如何解决hash冲突的
*  等等

# Android知识

### 基础篇

* 常用Activity启动模式（不必过分深究，因为配合Flag千变万化）
* Android中数据存储的方式有哪些
* 常见的图片加载库的三级模型、 LruCache实现原理
* Activity之间的通信方式、多界面间通信方式（EventBus类、可以深入问下原理）
* Handler、Looper、MessegeQueue消息模型
* Fragment为何setArgument参数传递、保留无参构造方法的原因
* Fragment生命周期、add与replace区别等
* 不同Android版本间的一些大变化
* AOP 面向切面编程
* H5跟native通信的方式
* IntentService跟普通Service的区别
* startService不同onStartCommand返回值有什么区别
* wait和sleep 的区别
* 常见内存泄露、如何查看和解决
* 如何获取APP的TopActivity
* MVC MVP区别
* ANR发生的原因，如何避免
* View的绘制过程，讲讲draw/onDraw和drawChild这个可以口述。
* View Touch事件的分发
* Crash的监测与捕获
* 用过哪些studio常用的性能检测工具，解决什么问题
* 组件化与模块化的概念，自己用过哪些场景，解决了什么问题



### 提升篇

####  SharePreference、ContentProvider原理及问题

*   底层实现（文件）
*   为何不支持跨进程，如何实现一个跨进程的SharePreference
*   ContentProvider 如何保证跨进程数据访问的同步互斥

####  AMS服务

* Activity的管理
* 后台杀死及恢复（Activity的恢复顺序、Fragment恢复）
* Android O对于后台进程的定义



####   binder机制相关知识点（看面试官自身把握）
    
* 服务的注册及查询使用流程（系统服务、普通服务）
* 一次拷贝原理
* 双进程双服务保活及讣告机制

####  View绘制及窗口管理

* UI数据如何传输到SurfaceFlinger（匿名共享内存知识）
* WMS知识（如常见的窗口类型 ）
* 在UI线程更新视图是否是严格成立的（addview的线程即可）
* 硬件加速渲染流程

#### 打包签名

* Android签名及验证签名知识（V1\V2的区别）
* 流行的多渠道打包原理


# Linux操作系统知识

很多面试者可能是嵌入式转Android，天生有Android底层Linux的支持算是优势，可以面试下操作系统类的东西，比如

*  线程跟进程的区别
* linux中进程间通信的方式、Android中用到的又是那些 
* 虚拟地址空间（用户空间、内核空间）
* binder是一个什么设备
* 系统调用的流程



# 主观题

Android主观题

## Sp多进程

SharePreference是Android中的一种数据存储方式，请问SharePreference底层实现是什么，SharePreference支持多进程吗？如果支持请说明原理，如果不支持，如何实现SharePreference支持多进程。

大概回答方向

* sp底层xml文件存储+内存缓存实现
* 本身不支持多进程
* 可以借助文件锁、ContentProvider等实现跨进程


## 简述下Looper、MessageQueue、线程之间的关系，同时描述下Handler的post消息是如何被执行的

大概回答方向，回答出大概方向就行

* 每一个线程内最多只有一个Looper，以及一个与Looper对应的MessageQueue

*  Handler的post消息是如何被执行？
 
无论哪个线程通过Handler post的消息都会被加入到MessageQueue，loop线程不断从MessageQueue读取消息并执行，如果没有线程就睡眠，等到在新消息被加入的时候，线程被唤醒，并执行

 


## Android及linux实现，为什么选择binder作为最常用的进程间通讯方式，他有什么优点，背后实现又是什么，同时，存在什么限制，在开发中你曾遇到过什么binder问题吗？如何解决的。

大概方向

* binder优点：一次拷贝，快， 实现原理map，让内核空间与用户空间通过一个偏移直接映射到同一内存
* 限制：传输数据量不能太大，太大会造成crash，减少数据传输大小就可以



# 高级面试


* Android OOM  
* 内存泄露 结合场景分析  追踪用到的常用工具 ，最好结合实例
* binder一次拷贝原理
* 自己用过的性能分析工具，怎么用的
* 后台杀死问题，Activity恢复顺序 考察AMS
* 
* BitMap导致OOM原理，JVM堆还是native堆，8.0之后的系统有什么改进
* sleep() 和 wait()  Java中的锁：同步（获取锁，这个锁加在哪，归谁管理）
* 
* Handler、Looper、MessegeQueue消息模型
* 如何获取APP的TopActivity
* View Touch事件的分发
* Crash的监测与捕获
* 线程跟进程的区别
* Java的强引用、软引用、弱引用 （LeakCary用那种实现的内存泄露检测）