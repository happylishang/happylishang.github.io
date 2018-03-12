# Qemu模拟器概念

>QEMU （Quick Emulator） is a generic and open source machine emulator and virtualizer.

Bochs 是一款可移植的IA-32仿真器，它利用模拟的技术来仿真目标系统，具体来说，将是将目标系统的指令分解，然后模拟分解后的指令以达到同样的效果。这种方法将每一条目标指令分解成多条主机系统的指令，很明显会大大降低仿真的速度。

qemu则是采用动态翻译的技术，先将目标代码翻译成一系列等价的被称为“微操作”（micro-operations）的指令，然后再对这些指令进行拷贝，修改，连接，最后产生一块本地代码。这些微操作排列复杂，从简单的寄存器转换模拟到整数/浮点数学函数模拟再到load/store操作模拟，其中load/store操作的模拟需要目标操作系统分页机制的支持。

qemu对客户代码的翻译是按块进行的，并且翻译后的代码被缓存起来以便将来重用。在没有中断的情况下，翻译后的代码仅仅是被链接到一个全局的链表上，目的是保证整个控制流保持在目标代码中，当异步的中断产生时，中断处理函数就会遍历串连翻译后代码的全局链表来在主机上执行翻译后的代码，这就保证了控制流从目标代码跳转到qemu代码。简单概括下：指定某个中断来控制翻译代码的执行，即每当产生这个中断时才会去执行翻译后的代码，没有中断时仅仅只是个翻译过程而已。这样做的好处就是，代码是是按块翻译，按块执行的，不像Bochs翻译一条指令，马上就执行一条指令。


* CPU模拟器（目前支持x86、PowerPC、ARM以及Sparc）
* 设备模拟（比如VGA显示器、16450串口、PS/2鼠标键盘、IDE硬盘、NE2000网卡等等）
* 在宿主机设备和模拟设备间协作的常规设备（比如块设备、字符设备、网络设备）
* 实例化了模拟设备的机器描述（比如PC、PowerMac、Sun4m）
* 调试器
* 用户界面


### 模拟器流派划分

目前市面上安卓模拟器软件看着种类繁多，但其实只有两大流派：Bluestacks和Virutalbox。Bluestacks的历史可以追溯到2011年，是最早在PC上实现流畅运行安卓系统的方案。Bluestacks的原理是把Android底层API接口翻译成Windows API，对PC硬件本身没有要求，在硬件兼容性方面有一定的优势。但Bluestacks需要翻译的Android接口数量巨大，很难面面俱到，而且存在软件翻译的开销，在性能和游戏兼容性方面欠佳。

Virtualbox是数据库巨头Oracle旗下的开源项目，**通过在Windows内核底层直接插入驱动模块，创建一个完整虚拟的电脑环境运行安卓系统，加上CPU VT硬件加速，性能和兼容性都更好**，比如intel-VT技术，国内像靠谱助手、新浪手游助手等一大批手游助手类都是直接基于Bluestacks内核，因为Bluestacks没有公开源代码无法深度定制，只能简单的优化，再包装界面后上市。其他的像海马玩、逍遥安卓、夜神、ITools这类的产品都是基于Virtualbox，能力弱的（如海马玩、ITools）直接采用Oracle发布的Virtualbox商业版，能力强的（如逍遥安卓、夜神）则对Virtualbox源代码深度定制后重新编译来进一步提高性能和兼容性。每个安卓模拟器有其各自特点，但都不能尽善尽美。



# 虚拟化技术

本质上，虚拟化就是由位于下层的软件模块，通过向上一层软件模块提供一个与它原先所期
待的运行环境完全一致的接口的方法，抽象出一个虚拟的软件或硬件接口，使得上层软件可
以直接运行在虚拟环境上。

Full virtualization: 所抽象的 VM 具有完全的物理机特性，OS 在其上运行不需要任何修改。
典型的有 VMWare, Virtualbox, Virtual PC, KVM-x86 ...)

VMM 对物理资源的虚拟可以划分为三个部分：处理器虚拟化、内存虚拟化和 I/O 设备虚拟
化。其中以处理器的虚拟化最为关键。


经典的虚拟化方法主要使用“特权解除” (Privilege deprivileging) 和“陷入－模拟” (Trap-and-Emulation)的方式。即：将 Guest OS 运行在非特权级（特权解除），而将 VMM
运行于最高特权级（完全控制系统资源）。解除了 Guest OS 的特权后，Guest OS 的大
部分指令仍可以在硬件上直接运行，只有当执行到特权指令时，才会陷入到 VMM 模拟执行
（陷入－模拟）。其早期的代表系统是 IBM VM/370

 # ARM binary code translator

# 特权模式内存地址检测也许也是个方案

![](http://ytliu.github.com/images/2012-12-30-3.png)


In the following graphic, the left flow is the ARM cache, while the right is a simplified x86 cache. Both sides show multiple caches sitting in front of the CPU, which speeds up memory access. On the x86 side the multiple caches are sequential, but the ARM splits the L1 cache into 2 parallel caches (I-Cache and D-Cache), which then pass to the L2-Cache.

The ARM architecture is one of the few with a dedicated data and instruction cache, sometimes known as the Harvard cache architecture. Unfortunately, the instruction and dedicated caches are not synchronized, so if a value at a certain address in one cache gets update it might not necessarily have to be updated in the other one.

By default, the Android emulator delivered with the Android SDK is based on QEMU, which is an open source CPU emulator supporting multiple architectures.


Based on the architecture caching information, two different caches exist on real devices;one for data access and one for instructions. **The emulator does not have this kind of split cache**. This difference can be accommodated using a user space program such as an app.



## Intel Houdini 比 QEMU 快在哪里？

qemu的做法是进入arm 的libc.so 继续模拟arm指令，直到调用syscall时进行调用约定的翻译；houdini的做法是直接获得虚拟cpu的r0，然后调用x86的puts(r0)，自然就快了很多。


houdini比qemu快的主要原因是，houdini只是那几个so是靠他翻译的，其他部分全是原生指令，靠硬件虚拟化跑的，而QEMU则是所有东西都要从ARM翻译过来，这样肯定是慢了的。

最近看了下houdini。houdini快的根本原因在于对寄存器的优化做的非常好。ARM到x86的一个巨大特点是，多寄存器到少寄存器CPU的一个映射过程。houdini通过运行时对，代码块的统计，将相互关联的代码块连层一片（区），将这个区中使用最频繁的arm寄存器与x86寄存器做映射，同一个区间的调用，可以减少内存<->寄存器之间的切换，提高效率。houdini寄存器的优化做的非常好。

# Qemu Self-modifying code and translated code invalidation


KVM cache 默认使用 writethrough，它是在调用 write 写入数据的同时将数据写入磁盘缓存和后端块设备才返回，缺点是写入性能较低，但是安全性高。

![](http://blog.chinaunix.net/attachment/201301/22/23225855_1358825621mjmM.jpg)
![](http://blog.chinaunix.net/attachment/201301/22/23225855_1358825914zVK0.gif)
![](http://blog.chinaunix.net/attachment/201301/22/23225855_1358825814ZOLw.gif)   

执行过程，自修改代码，会将当前代码块无效化，因此可以看成简单的单缓存X86架构。

在大多数CPU上，自修改代码很容易处理。通过执行一条特殊的代码缓存废弃指令，可以发出信号指示出该代码已被修改。这足以废弃相应的翻译代码。
然而，在一些CPU例如x86上，当代码被修改时，应用程序不能发出信号以废弃指令缓存。所以，自修改代码是一个特殊的挑战。

当生成了一个TB的翻译代码时，如果相应的宿主机页不是只读的，那么它将会被设置为写保护。如果有一个针对该页的写访问产生，Linux 会产生一个SEGV信号，QEMU会废弃该页中所有的翻译代码，并使该页重置为可写。通过维护一个包含给定页中所有TB的链表，可以有效完成正翻译代码的废弃任务。除此之外，还有其他链表用来取消直接block链。

当使用软件MMU时，代码废弃将更加高效：如果某个代码页由于写访问而频繁做废弃代码操作，将会创建一个展示该页内部代码的bitmap。每次往该页的存储操作都将检查bitmap，以知晓该页的代码是否需要废弃。这避免了该页仅作数据修改时就进行代码废弃操作。
 
Self-modifying code is a special challenge in x86 emulation because no instruction cache
invalidation is signaled by the application when code is modified.
When translated code is generated for a basic block, the corresponding host page is write
protected if it is not already read-only. Then, if a write access is done to the page, Linux
raises a SEGV signal. QEMU then invalidates all the translated code in the page and
enables write accesses to the page.
Correct translated code invalidation is done efficiently by maintaining a linked list of every
translated block contained in a given page. Other linked lists are also maintained to undo
direct block chaining.

在RISC架构上，Qemu写操作会利用内存屏障及cache flush技术保持缓存跟内存的数据一致。

>On RISC targets, correctly written software uses memory barriers and cache flushes, so
some of the protection above would not be necessary. However, QEMU still requires that
the generated code always matches the target instructions in memory in order to handle
exceptions correctly.

因此，Qemu保证了自修改代码在Cache跟内存中一致，改了，无效然后直接加载，但是ARM真机不会同步这么及时，除非用户手动__clear_cache。

# arm需要用户主动清理cache才会同步

参考文档：[ARM Cache Flush on mmap’d Buffers with __clear_cache()](https://minghuasweblog.wordpress.com/2013/03/29/arm-cache-flush-on-mmapd-buffers-with-clear-cache/)       


![Arm Cache不同步原理.jpg](http://upload-images.jianshu.io/upload_images/1460468-30be15757c6ef007.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![X86 Android 模拟器 Cache修改原理.jpg](http://upload-images.jianshu.io/upload_images/1460468-4551d2f0a5799486.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# intel 的houdini技术

前些天有网友在QQ群里和我说过一个叫 BlueStacks 的程序，可以安装在Windows上运行带arm代码的（使用NDK开发的）程序， 而且并非是ANDROID SDK 里的使用 QEMU 完整模拟整个系统硬件环境。 好奇之下，我下载并安装了它，分析了一下大体的原理，但没找到关键猫腻。。失望中。。

昨天因为查一份资料，google又抽风，不得不拿起GoAgent翻越伟大的Wall，发现了一个讨论，

        https://groups.google.com/group/android-x86/browse_thread/thread/ff71e83494e2fd8d/d7d3dfdb9c581fc8 （翻墙访问）

根据这个讨论，我又搜索了一些资料，知道Intel 发布了它自打把XSCALE系列打包出售给马维尔后的第一款手机CPU Atom Z2460,国内跟风的有联想K800手机,印度跟风的有一个XOLO 手机,还有一个不记得名称了，关键不在于跟不跟风，在于他们发布的手机是基于X86指令集的ATOM CPU， 但可以兼容运行大部分带有Native ARM代码的应用，关键之处就是靠了一个Intel并未公开发表的技术 ARM binary code translator， 在这个页面大概描述了其用到的东西：http://www.buildroid.org/blog/?p=198 
        
添加了一些代码补丁

*         一个libhoudini.so 
*         一个libdvm_houdini.so（intel修改版的libdvm） （dalvik虚拟机的动态库）
*         一堆android的arm版本的lib文件houdini_armlibs.tgz

主要修改了dalvik虚拟机的dvmLoadNativeCode函数，当其调用的**dlopen函数失败时，调用自己的my_dlopen重试，加载arm的lib文件**，在这种场景下虚拟了一个ARM的CPU，注意，只是虚拟CPU，并不像ANDROID SDK一样模拟整个系统，类似QEMU的Linux User Mode，由此，我翻出以前对BlueStacks的分析， 发现了他的lib文件大多都有一个对应的lib***.so-arm文件，继续分析发现在/bin 目录里的一个程序，arm-runtime，是个elf程序，拿起ida一看， 恍然大悟，arm-runtime就是qemu的user mode 进程改了个名，（当然代码肯定有改动），原来BlueStacks是把qemu的 user mode 移植到了windows上， 怪不得都说它的模拟怎么怎么快，根源在这儿。同时它利用了Intel-VT技术，跑的很快。

>If you install Bluestacks (beta or the production version in Asus Vibe), using adb, you can easily extract a file /system/bin/arm-runtime. This is the key file which Bluestacks uses to run ARM apps. Perform a binary analysis, and you will see it is in fact Qemu. In newer version of its beta, the process name of arm-runtime is changed to zygote_arm. So discovering this fact becomes a little harder. 

>结合X86的性能与QEMU的兼容，执行native so时，如果找不到X86的lib，就用arm的，并用libhoudini.so进行翻译，而对于普通Java代码，则可以直接运行，无需翻译。

# Intel的加速技术

只要你的CPU是intel的产品并且支持VT（virtualization Technology）就可以使用HAXM技术将你的模拟器的速度提升至真机的水平。Intel只提供了windows版和MAC版，而没有linux版，其实linux版就是KVM，只要启用了KVM，自然就是HAXM了。那就是传说中的KVM（Kernel-based Virtual Machine），同样的，它需要硬件的支持，比如intel的VT和AMD的V，它是基于硬件的完全虚拟化。

# 一个虚拟机是一个进程

#  Hardware Accelerated eXecution Manager跟Genymotion采用相同的技术

it uses the Intel Virtualization Technology (or VT) in the same way Geanymotion uses VirtualBox
 
# 参考文档

[虚拟化技术原理精要](http://jackslab.org/people/comcat/mydoc/virt.tech.essential.pdf)      
[在Android X86上执行Native ARM 代码---Android X86应用兼容的探讨](http://www.cnblogs.com/binsys/archive/2012/08/25/2655882.html)       
[How to check if an app uses binary translation](http://blog.apedroid.com/2013/05/how-to-check-if-app-uses-binary.html)       
[QEMU vs. VirtualBox](http://www.linuxjournal.com/content/qemu-vs-virtualbox)         
[QEMU内核机制详解 hao ](http://www.embeddedlinux.org.cn/html/xingyexinwen/201303/15-2490.html)             
[QEMU启动过程](http://blog.csdn.net/YuZhiHui_No1/article/details/66973331)               
[https://qemu.weilnetz.de/doc/2.7/qemu-tech.pdf](https://qemu.weilnetz.de/doc/2.7/qemu-tech.pdf)           
[qemu源码架构  ](http://baobaoyangzhou.blog.163.com/blog/static/11783125020121068502213/)                 
[ qemu源码架构](http://blog.csdn.net/QFire/article/details/78107088)             
[Linux内核中的内存屏障(1)](http://larmbr.com/2014/02/14/the-memory-barriers-in-linux-kernel(1)/)       
[KVM 介绍（2）：CPU 和内存虚拟化](http://www.cnblogs.com/sammyliu/p/4543597.html)      
[Speed up the Android Virtual Device using Geanymotion or Intel HAXM](http://www.qilineggs.com/2014/02/speed-up-android-virtual-device-using.html)    (KVM)
 
   