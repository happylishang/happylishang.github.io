---
layout: post
title: "利用ARM与X86的区别识别Android模拟器"
description: "Android"

---
	
Android模拟器常常被用来刷单，如何准确的识别模拟器成为App开发中的一个重要模块，目前也有专门的公司提供相应的SDK供开发者识别模拟器。 目前流行的Android模拟器主要分为两种，一种是基于Qemu，另一类是基于Genymotion，网上现在流行用一些模拟器特征进行鉴别，比如：

* 通过判断IMEI是否全部为0000000000格式
* 判断Build中的一些模拟器特征值
* 匹配Qemu的一些特征文件以及属性
* 通过获取cpu信息，将x86的给过滤掉（真机一般都是基于ARM）

等等，不过里面的很多手段都能通过改写ROM或者Xposed作假，让判断的性能打折扣。其实，现在绝大部分手机都是基于ARM架构，可以将其他CPU架构给忽略不计，模拟器全部运行在PC上，因此，只需要判断是运行的设备否是ARM架构即可。

ARM与PC的X86在架构上有很大区别，ARM采用的哈弗架构将指令存储跟数据存储分开，与之对应的，ARM的一级缓存分为I-Cache（指令缓存）与D-Cahce（数据缓存），而X86只有一块缓存，如果我们将一段代码可执行代码动态映射到内存，在执行的时候，X86架构上是可以修改这部分代码的，而ARM的I-Cache可以看做是只读，因为，当我们往PC对应的地址写指令的时候，其实是往D-Cahce写，而I-Cache中的指令并未被更新，这样，两端程序就会在ARM与x86上有不同的表现，根据计算结果便可以知道究竟是还在哪个平台上运行。但是，无论是x86还是ARM，只要是静态编译的程序，都没有**修改代码段**的权限，所以，首先需要将上面的汇编代码翻译成可执行文件，再需要申请一块内存，将可执行代码段映射过去，执行。
以下实现代码是测试代码的核心，主要就是将地址e2844001的指令**add     r4, r4, #1**，在运行中动态替换为e2877001的指令**add     r7, r7, #1**，这里目标是ARM-V7架构的，要注意它采用的是三级流水，PC值=当前程序执行位置+8。通过arm交叉编译链编译出的可执行代码如下：
	
	8410:       e92d41f0        push    {r4, r5, r6, r7, r8, lr}
	8414:       e3a07000        mov     r7, #0
	8418:       e1a0800f        mov     r8, pc      // 本平台针对ARM7，三级流水  PC值=当前程序执行位置+8
	841c:       e3a04000        mov     r4, #0
	8420:       e2877001        add     r7, r7, #1
	8424:       e5985000        ldr     r5, [r8]
	
	00008428 <code>:
	
	8428:       e2844001        add     r4, r4, #1
	842c:       e1a0800f        mov     r8, pc
	8430:       e248800c        sub     r8, r8, #12   // PC值=当前程序执行位置+8
	8434:       e5885000        str     r5, [r8]
	8438:       e354000a        cmp     r4, #10
	843c:       aa000002        bge     844c <out>
	8440:       e357000a        cmp     r7, #10
	8444:       aa000000        bge     844c <out>
	8448:       eafffff6        b       8428 <code>
	
	0000844c <out>:
	
	844c:       e1a00004        mov     r0, r4
	8450:       e8bd81f0        pop     {r4, r5, r6, r7, r8, pc}
		
如果是在ARM上运行，e2844001处指令无法被覆盖，最终执行的是**add r4,#1** ，而在x86平台上，执行的是**add r7,#1** ,代码执行完毕， r0的值在模拟器上是1，而在真机上是10。之后，将上述可执行代码通过mmap，映射到内存并执行即可，具体做法如下：

	void (*asmcheck)(void);
	int detect() {
	    char code[] =
            "\xF0\x41\x2D\xE9"
            "\x00\x70\xA0\xE3"
            "\x0F\x80\xA0\xE1"
            "\x00\x40\xA0\xE3"
            "\x01\x70\x87\xE2"
            "\x00\x50\x98\xE5"
            "\x01\x40\x84\xE2"
            "\x0F\x80\xA0\xE1"
            "\x0C\x80\x48\xE2"
            "\x00\x50\x88\xE5"
            "\x0A\x00\x54\xE3"
            "\x02\x00\x00\xAA"
            "\x0A\x00\x57\xE3"
            "\x00\x00\x00\xAA"
            "\xF6\xFF\xFF\xEA"
            "\x04\x00\xA0\xE1"
            "\xF0\x81\xBD\xE8";
	
	    void *exec = mmap(NULL, (size_t) getpagesize(), PROT, MAP_ANONYMOUS | MAP_SHARED, -1, (off_t) 0);
	    memcpy(exec, code, sizeof(code) + 1);
	    asmcheck = (void *) exec;
	    asmcheck();
	    __asm __volatile (
	    "mov %0,r0 \n"
	    :"=r"(a)
	    );
	    munmap(exec, getpagesize());
	    return a;
	}

经验证， 无论是Android自带的模拟器，还是夜神模拟器，或者Genymotion造假的模拟器，都能准确识别。在32位真机上完美运行，但是在64位的真机上可能会存在兼容性问题，可能跟arm64-v8a的指令集不同有关系，**也希望人能指点**。为了防止在真机上出现崩溃，最好还是单独开一个进程服务，利用Binder实现模拟器鉴别的查询。

另外，对于Qemu的模拟器还有一种任务调度的检测方法，但是实验过程中发现不太稳定，并且仅限Qemu，不做参考，不过这里给出原文链接：
[DEXLabs](http://www.dexlabs.org/blog/btdetect)

**仅供参考，欢迎指正**

# 参考文档

[QEMU emulation detection](https://wiki.koeln.ccc.de/images/d/d5/Openchaos_qemudetect.pdf)       
[DEXLabs](http://www.dexlabs.org/blog/btdetect)         
[利用cache特性检测Android模拟器](http://www.evil0x.com/posts/14360.html)       