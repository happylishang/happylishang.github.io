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

目前市面上安卓模拟器软件看着种类繁多，但其实只有两大流派：Bluestacks和Virutalbox。Bluestacks的历史可以追溯到2011年，是最早在PC上实现流畅运行安卓系统的方案。Bluestacks的原理是把Android底层API接口翻译成Windows API，对PC硬件本身没有要求，在硬件兼容性方面有一定的优势。但Bluestacks需要翻译的Android接口数量巨大，很难面面俱到，而且存在软件翻译的开销，在性能和游戏兼容性方面欠佳。Virtualbox是数据库巨头Oracle旗下的开源项目，通过在Windows内核底层直接插入驱动模块，创建一个完整虚拟的电脑环境运行安卓系统，加上CPU VT硬件加速，性能和兼容性都更好，但是对于电脑CPU有一定要求，超过五年以上的电脑使用起来比较吃力。国内像靠谱助手、新浪手游助手等一大批手游助手类都是直接基于Bluestacks内核，因为Bluestacks没有公开源代码无法深度定制，只能简单的优化，再包装界面后上市。

其他的像海马玩、逍遥安卓、夜神、ITools这类的产品都是基于Virtualbox，能力弱的（如海马玩、ITools）直接采用Oracle发布的Virtualbox商业版，能力强的（如逍遥安卓、夜神）则对Virtualbox源代码深度定制后重新编译来进一步提高性能和兼容性。



# 虚拟化技术

本质上，虚拟化就是由位于下层的软件模块，通过向上一层软件模块提供一个与它原先所期
待的运行环境完全一致的接口的方法，抽象出一个虚拟的软件或硬件接口，使得上层软件可
以直接运行在虚拟环境上。

Full virtualization: 所抽象的 VM 具有完全的物理机特性，OS 在其上运行不需要任何修改。
典型的有 VMWare, Virtualbox, Virtual PC, KVM-x86 ...)

VMM 对物理资源的虚拟可以划分为三个部分：处理器虚拟化、内存虚拟化和 I/O 设备虚拟
化。其中以处理器的虚拟化最为关键。


经典的虚拟化方法主要使用“特权解除” (Privilege deprivileging) 和“陷入－模拟” (Trapand-Emulation)的方式。即：将 Guest OS 运行在非特权级（特权解除），而将 VMM
运行于最高特权级（完全控制系统资源）。解除了 Guest OS 的特权后，Guest OS 的大
部分指令仍可以在硬件上直接运行，只有当执行到特权指令时，才会陷入到 VMM 模拟执行
（陷入－模拟）。其早期的代表系统是 IBM VM/370



        前些天有网友在QQ群里和我说过一个叫 BlueStacks 的程序，可以安装在Windows上运行带arm代码的（使用NDK开发的）程序， 而且并非是ANDROID SDK 里的使用 QEMU 完整模拟整个系统硬件环境。 好奇之下，我下载并安装了它，分析了一下大体的原理，但没找到关键猫腻。。失望中。。

        昨天因为查一份资料，google又抽风，不得不拿起GoAgent翻越伟大的Wall，发现了一个讨论，

        https://groups.google.com/group/android-x86/browse_thread/thread/ff71e83494e2fd8d/d7d3dfdb9c581fc8 （FQ访问）

        根据这个讨论，我又搜索了一些资料，知道Intel 发布了它自打把XSCALE系列打包出售给马维尔后的第一款手机CPU Atom Z2460,国内跟风的有联想K800手机,印度跟风的有一个XOLO 手机,还有一个不记得名称了，关键不在于跟不跟风，在于他们发布的手机是基于X86指令集的ATOM CPU， 但可以兼容运行大部分带有Native ARM代码的应用，关键之处就是靠了一个Intel并未公开发表的技术 ARM binary code translator， 在这个页面大概描述了其用到的东西：


        http://www.buildroid.org/blog/?p=198 
        一个代码补丁

        一个libhoudini.so

        一个libdvm_houdini.so（intel修改版的libdvm） （dalvik虚拟机的动态库）

        一堆android的arm版本的lib文件houdini_armlibs.tgz

        根据补丁，可以知道其主要修改了dalvik虚拟机的dvmLoadNativeCode函数，当其调用的dlopen函数失败时，调用自己的my_dlopen重试， 加载arm的lib文件，用IDA6.1对libhoudini.so进行分析，可以发现其大概是虚拟了一个ARM的CPU，注意，只是虚拟CPU，并不像ANDROID SDK一样 模拟整个系统，这个让我想到了QEMU 的Linux User Mode，由此，我翻出以前对BlueStacks的分析， 发现了他的lib文件大多都有一个对应的lib***.so-arm文件，继续分析发现在/bin 目录里的一个程序，arm-runtime，是个elf程序，拿起ida一看， 恍然大悟，arm-runtime就是qemu的user mode 进程改了个名，（当然代码肯定有改动），原来BlueStacks是把qemu的 user mode 移植到了windows上， 怪不得都说它的模拟怎么怎么快，根源在这儿。

        有兴趣的可以百度一下下载K800的ROM文件解包进行分析，也可以分析下BlueStacks的ROM，现在仍让我困惑的就是K800的加载方式大概知道了，但经过对BlueStacks ROM文件的逆向分析，到目前为止还没有找到其只是如何执行arm-runtime这个程序来执行arm native代码的，难道是binfmt_misc方式？

        此文仅限抛砖引玉，不知道能不能引来，具体技术细节我也不是很了解，希望技术大牛们解我等小菜之迷惑。

        补充一个连接：http://www.buildroid.org/blog/?p=175
其实我发此文是想到Windows 8 RT 是ARM的，微软说和现行的程序CPU体系不一样，不能兼容，那么我们可不可以把QEMU的Linux User Mode移植到未来的
Windows8RT 或 Windows8上，一次来让Windows8RT和Windows8的应用互相兼容呢?


# ARM binary code translator
# 特权模式内存地址检测也许也是个方案

![](http://ytliu.github.com/images/2012-12-30-3.png)
# 参考文档

[虚拟化技术原理精要](http://jackslab.org/people/comcat/mydoc/virt.tech.essential.pdf)      
[在Android X86上执行Native ARM 代码---Android X86应用兼容的探讨](http://www.cnblogs.com/binsys/archive/2012/08/25/2655882.html)       
[How to check if an app uses binary translation](http://blog.apedroid.com/2013/05/how-to-check-if-app-uses-binary.html)       
[QEMU vs. VirtualBox](http://www.linuxjournal.com/content/qemu-vs-virtualbox)      