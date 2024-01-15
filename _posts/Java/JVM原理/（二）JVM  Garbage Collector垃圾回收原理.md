JVM 运行时内存布局

![](https://static001.infoq.cn/resource/image/b4/62/b4ff890142874a6cbef1ad7a80eb7462.png)

分两类，Thread独享的内存

Thread独享的内存：线程创建时，相应的区域分配内存，线程销毁时，释放相应内存
Thread共享的内存： Heap： GC 垃圾回收的主站场、Method Area：方法区、Runtime Constant Pool

除了 PC Register 区不会抛出 StackOverflowError 或 OutOfMemoryError ，其它 5 个区域，当请求分配的内存不足时，均会抛出 OutOfMemoryError（即：OOM），其中 thread 独立的 JVM Stack 区及 Native Method Stack 区还会抛出 StackOverflowError

## **如何判断一个Java对象是可回收的? **

![](https://static001.infoq.cn/resource/image/e3/71/e36c624e8b4300775123f95a34b86571.png)

###  引用计数法  

无法解决循环引用的问题，A引用B，B同时引用A，AB都无用的时候，两者无法回收。

### 可达性分析法：对象是否可达

目前的虚拟机基本都是采用可达性分析算法来判断对象是否存活，这种算法以GC Root对象为起点，遍历出所有的引用子节点，再以子节点为起点，引出此节点指向的下一个结点，直到所有的结点都遍历完毕,任何在在这个引用链上的节点都可以认为是可达的，否则就是不可达的。那么GC ROOT是怎么定义呢：

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/aa9caf1956c34223b36b87f21ed8d6ef~tplv-k3u1fbpfcp-watermark.image?)

上图中的A 以及 AA的静态变量都可以作为GC root，它们引出的强引用链都是可达的对象。而GG GG2没有在GC Root的引用链上，就可以被回收。哪些对象可以作为GC ROOT。

### 常见的GC Root种类 ：两栈两方法

> GC Root：A garbage collection root is an object that is accessible from outside the heap. 

**GC管理的主要区域是Java堆**，方法区、栈和本地方法区不被GC所管理,也正是这些不被管理的对象可以作为GC roots,被GC roots引用的对象不被GC回收。

* 运行线程栈上引用的对象
* 运行线程上Native方法栈中JNI引用的对象
* 类**静态属性变量引用的对象**、或者**常量引用的对象**
* 虚拟机内部的引用，比如系统类加载器加载的对象等

## 如何回收

#### mark-sweep 标记清除法
#### mark-copy 标记复制法
#### mark-compact 标记-整理（也称标记-压缩）法
#### generation-collect 分代收集算法 


### 参考文档

[GC Roots 是什么？哪些对象可以作为 GC Root？看完秒懂！](https://blog.csdn.net/weixin_38007185/article/details/108093716)


[一文看懂 JVM 内存布局及 GC 原理](https://www.infoq.cn/article/3wyretkqrhivtw4frmr3)