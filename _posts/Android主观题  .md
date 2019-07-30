Android主观题

# Sp多进程

SharePreference是Android中的一种数据存储方式，请问SharePreference底层实现是什么，SharePreference支持多进程吗？如果支持请说明原理，如果不支持，如何实现SharePreference支持多进程。

大概回答方向

* sp底层xml文件存储+内存缓存实现
* 本身不支持多进程
* 可以借助文件锁、ContentProvider等实现跨进程


# 简述下Looper、MessageQueue、线程之间的关系，同时描述下Handler的post消息是如何被执行的

大概回答方向，回答出大概方向就行

* 每一个线程内最多只有一个Looper，以及一个与Looper对应的MessageQueue

*  Handler的post消息是如何被执行？
 
无论哪个线程通过Handler post的消息都会被加入到MessageQueue，loop线程不断从MessageQueue读取消息并执行，如果没有线程就睡眠，等到在新消息被加入的时候，线程被唤醒，并执行


# Android及linux实现，为什么选择binder作为最常用的进程间通讯方式，他有什么优点，背后实现又是什么，同时，存在什么限制，在开发中你曾遇到过什么binder问题吗？如何解决的。

大概方向

* binder优点：一次拷贝，快， 实现原理map，让内核空间与用户空间通过一个偏移直接映射到同一内存
* 限制：传输数据量不能太大，太大会造成crash，减少数据传输大小就可以

