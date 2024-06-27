![](https://pic4.zhimg.com/80/v2-abefb713de46f1e6dd241246c0afe263_720w.webp)

JVM的内存结构大概分为：

* 堆（Heap）：线程共享。所有的对象实例以及数组都要在堆上分配。回收器主要管理的对象。
* 方法区（Method Area）：线程共享。存储**类信息、常量、静态变量**、**即时编译器编译后的代码**。
* 方法栈（JVM Stack）：线程私有。存储局部变量表、操作栈、动态链接、方法出口，对象指针。
* 本地方法栈（Native Method Stack）：线程私有。为虚拟机使用到的Native 方法服务。如Java使用c或者c++编写的接口服务时，代码在此区运行。
* 程序计数器（Program Counter Register）：线程私有。有些文章也翻译成PC寄存器（PC Register），同一个东西。它可以看作是当前线程所执行的字节码的行号指示器。指向下一条要执行的指令。为什么私有，考虑下多核CPU，可以并行处理的，多线程并行，那就需要多个。

注意：常量、静态变量、代码都在方法区，注意如果变量是对象的引用，那么对象的存储还是在堆区。


## Heap Memory和Native Memory

JVM管理的内存可以总体划分为两部分：Heap Memory和Native Memory。前者我们比较熟悉，是供Java应用程序使用的；后者也称为C-Heap，是供JVM自身进程使用的。Heap Memory及其内部各组成的大小可以通过JVM的一系列命令行参数来控制，在此不赘述。Native Memory没有相应的参数来控制大小，其大小依赖于操作系统进程的最大值（对于32位系统就是3~4G，各种系统的实现并不一样），以及生成的Java字节码大小、创建的线程数量、维持java对象的状态信息大小（用于GC）以及一些第三方的包，比如JDBC驱动使用的native内存。

native进程：采用C/C++实现，不包含dalvik实例的linux进程，/system/bin/目录下面的程序文件运行后都是以native进程形式存在的。

java进程：实例化了 dalvik 虚拟机实例的 linux 进程，进程的入口 main 函数为 java 函数。dalvik 虚拟机实例的宿主进程是fork()调用创建的 linux 进程，所以每一个 android 上的 java 进程实际上就是一个 linux 进程，只是进程中多了一个 dalvik 虚拟机实例。因此，java 进程的内存分配比 native 进程复杂。Android 系统中的应用程序基本都是 java 进程，如桌面、电话、联系人、状态栏等等。


