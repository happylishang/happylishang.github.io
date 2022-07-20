---
layout: post
title: "理解Android VSYNC与图形系统中的双缓冲、三缓冲"
category: Android


---


# VSYNC与画面撕裂

VSYNC即vertical sync，也称为垂直同步，是一种图形技术，主要就是强制将帧速率与显示器的刷新率同步，最初由 GPU 制造商提出，主要用来处理屏幕撕裂。那么首先了解下两个名词：FPS与屏幕刷新频率

* 帧率[Frame Rate，单位FPS]-显卡生成帧的速率，也可以认为是数据处理的速度
* 屏幕刷新频率 [Refresh Rate单位赫兹/HZ]：是指硬件设备刷新屏幕的频率，值一般是固定的，以黑白电视的电子扫描枪类比，比如60Hz的显示屏，每16ms电子枪从上到下从左到右一行一行逐渐把图片绘制出来。

两者要同步配合好才能高效的显示图像，可以人为帧率对应的是图像数据的输出，刷新率对应的是图像数据的屏幕展示，如果帧率高过显示设备的刷新率，而又没有采用合适的同步技术，会出现什么问题呢？





如果两者的频率不匹配，就很容易发生屏幕撕裂，即多帧的局部数据共同组成了一个完整帧，显示在屏幕里面，如下图所示

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7c03e6553ccc49beae246ff2c3e0cd8b~tplv-k3u1fbpfcp-watermark.image?)


which is when your screen displays portions of multiple frames in one go. Tearing occurs when the monitor’s refresh rate (how many times it updates per second) is not in sync with the frames per second.


如果GPU显卡性能非常强悍，帧率可以非常高，甚至会高于屏幕刷新频率。那VSync是什么？

VSync即垂直同步，一开始由GPU厂商提出，主要用来处理屏幕撕裂的问题[FPS帧率与屏幕刷新频率不同步的时候就会发生] [本文参考视频 Google IO](https://www.youtube.com/watch?v=Q8m9sHdyXnE)，

理想情况下：The display (LCD, AMOLED, whatever) gets each frame from the graphics chip, and starts drawing it line by line. Ideally, you want the display to get a new frame from the graphics chip after it is finished drawing the previous frame. Tearing occurs when the graphics chip loads a new frame in the middle of the LCD draw, so you get half of one frame and half of another.






如果只有一块缓存，在没有加锁的情况下，容易出现。即：在屏幕更新的时候，如果显卡输出帧率很高，在A帧的数据上半部分刚更新完时，B帧就到了，如果没采取同步锁机制，可以认为**帧到了就可用**，在继续刷新下半部分时，由于只有一块存储，A被B覆盖，绘制用的数据就是B帧，此时就会出现上半部分是A下半部分是B，这就是屏幕撕裂，**个人觉得描述成显卡瞬时帧率过高也许更好**。同正常帧绘制相比，正常的帧给时间才就能完整绘制一帧，但撕裂的帧没有机会补全。

![image.png](https://upload-images.jianshu.io/upload_images/1460468-d8a7b252191b7ad8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

相比较画面撕裂场景如下：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-4424c66d36b291f2.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

不过按照Android官方指导的说法，屏幕撕裂还有另外一种解释，那就是显示器用了半成品的帧，不过我是不太理解他说的这点。[参考视频](https://youtu.be/1iaHxmfZGGc?list=UU_x5XG1OV2P6uZZ5FSM9Ttw&t=112)

以上说的是只有一块显示存储的情况，其实只要加锁就能解决。那么如果多增加一块显示存储区能解决吗？显卡绘制成功后，先写入BackBuffer，不影响当前正在展示的FrameBuffer，这就是双缓冲，但是理论上其实也不行，因为BackBuffer毕竟也是要展示的，也要”拷贝“到FrameBuffer，在A帧没画完，BackBuffer如果不加干预，直接”拷贝“到FrameBuffer同样出现撕裂。所以**同步锁的机制才是关键**，必须有这么一个机制告诉GPU显卡，**要等待当前帧绘完整，才能替换当前帧**。但如果仅仅单缓存加锁的话GPU显卡会被挂啊？这就让效率低了，那就一边加同步锁，同时再多加一个缓存，垂直同步（VSYNC）就可看做是这么个东西，其实两者是配合使用的。

![image.png](https://upload-images.jianshu.io/upload_images/1460468-30ac3ea4118e9390.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

再来看下VSYNC，屏幕刷新从左到右水平扫描（Horizontal Scanning），从上到下垂直扫描Vertical Scanning，**垂直扫描完成则整个屏幕刷新完毕，这便是告诉外界可以绘制下一帧的时机**，在这里发出VSync信号，通知GPU给FrameBuffer传数据，完成后，屏幕便可以开始刷新，所以或许称之为**帧同步**更合适。VSYNC**强制帧率和显示器刷新频率同步**，如果当前帧没绘制完，即使下一帧准备好了，也禁止使用下一帧，直到显示器绘制完当前帧，等下次刷新的时候，才会用下一帧。比如：如果显示器的刷新频率是60HZ显示器，开了垂直同步后，显示帧率就会被锁60，即使显卡输出高，也没用。对Android系统而言，垂直同步信号除了强制帧率和显示器刷新频率同步外，还有其他很多作用，VSYNC是APP端重绘、SurfaceFlinger图层合成的触发点，只有收到VSYNC信号，它们才会工作，以上便是个人对引入VSYNC与双缓冲的见解。

** VSYNC在Android中还有个作用，帮助UI线程的渲染任务抢占CPU，在VSYNC到达是，CPU 放下手中的任务，优先处理UI绘制**

It does this by preventing the GPU from doing anything to the display memory until the monitor has concluded its current refresh cycle — effectively not feeding it any more information until it’s ready for it. Through a combination of double buffering and page flipping, VSync synchronizes the drawing of frames onto the display only when it has finished a refresh cycle, so you shouldn’t ever see tears when VSync is enabled.


VSync cannot improve your resolution, colors, or brightness levels like HDR. It’s a preventative technology that’s focused on stopping a specific problem rather than making improvements. It also tends to harm performance.

# 单缓冲：

CPU处理完，GPU处理，串联的模型，效率低一个处理完，另一个处理




# 双缓冲  ：配合垂直同步



In computer science, multiple buffering is the use of more than one buffer to hold a block of data, so that a "reader" will see a complete (though perhaps old) version of the data, rather than a partially updated version of the data being created by a "writer". It is very commonly used for computer display images. It is also used to avoid the need to use dual-ported RAM (DPRAM) when the readers and writers are different devices.



In computer graphics, double buffering is a technique for drawing graphics that shows no (or less) stutter, tearing, and other artifacts.

也是处理图形撕裂的一部分技术

# 双缓冲的进阶：三缓冲


![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/4e95445860a447ee82a610bcb025a65f~tplv-k3u1fbpfcp-watermark.image?)


Sets 1, 2 and 3 represent the operation of single, double and triple buffering, respectively, with vertical synchronization (vsync) enabled. In each graph, time flows from left to right. Note that 3 shows a swap chain with three buffers; the original definition of triple buffering would throw away frame C as soon as frame D finished, and start drawing frame E into buffer 1 with no delay. Set 4 shows what happens when a frame (B, in this case) takes longer than normal to draw. In this case, a frame update is missed. In time-sensitive implementations such as video playback, the whole frame may be dropped. With a three-buffer swap chain in set 5, drawing of frame B can start without having to wait for frame A to be copied to video memory, reducing the chance of a delayed frame missing its vertical retrace.

拷贝的时间，你可以做的别的，GPU的时间，CPU可以做别的



>  双缓冲保证低延时，三缓冲保证稳定性，双缓冲不在16ms中间开始，有足够时间绘制 三缓冲增加其韧性。


在Android系统里，除了双缓冲，还有个三缓冲，不过这个三缓冲是对于**屏幕硬件刷新**之外而言，它关注的是整个Android图形系统的消费者模型，跟Android自身的VSYNC用法有关系，在 Jelly Bean 中Android扩大了VSYNC使用场景与效果，不仅用在屏幕刷新防撕裂，同时也用在APP端绘制及SurfaceFlinger合成那，此时对VSYNC利用有点像Pipeline流水线，贯穿整个绘制流程，对比下VSYNC扩展使用的区别：


![image.png](https://upload-images.jianshu.io/upload_images/1460468-966ca5f42592eeff.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

如果想要达到60FPS的流畅度，每16毫秒必须刷新一帧，否则动画、视频就没那么丝滑，扩展后：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-f61ba65d9e250aca.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


对于没采用VSYNC做调度的系统来说，比如Project Butter之前的系统（4.1以下），CPU的对于显示帧的处理是凌乱的，优先级也没有保障，处理完一帧后，CPU可能并不会及时处理下一帧，可能会优先处理其他消息，等到它开始处理UI生成帧的时候，可能已经处于VSYNC的中间，这样就很**容易跨两个VYSNC**信号，导致掉帧。在Jelly Bean中，下一帧的处理被限定在VSync信号到达时，并且看Android的处理UI重绘消息的优先级是比较高的，其他的同步消息均不会执行，从而保证每16ms处理一帧有序进行，同时由于是**在每个VSYNC信号到达时就处理帧，可以尽量避免跨越两帧的情况出现**。

上面的流程中，Android已经采用了双缓冲，**双缓冲不仅仅是两份存储，它是一个概念，双缓冲是一条链路，不是某一个环节，是整个系统采用的一个机制，需要各个环节的支持，从APP到SurfaceFlinger、到图像显示都要参与协作。**对于APP端而言，每个Window都是一个双缓冲的模型，一个Window对应一个Surface，而每个Surface里至少映射两个存储区，一个给图层合成显示用，一个给APP端图形处理，这便是应于上层的双缓冲。Android4.0之后基本都是默认硬件加速，CPU跟GPU都是并发处理任务的，CPU处理完之后就完工，等下一个VSYNC到来就可以进行下一轮操作。也就是CPU、GPU、显示都会用到Buffer，VSYNC+双缓冲在理想情况下是没有问题的，但如果某个环节出现问题，那就不一样了如下（帧耗时超过16ms）：

![双缓冲jank](https://www.androidpolice.com/wp-content/uploads/2012/07/0001_Layer-72.png)

可以看到在第二个阶段，存在CPU资源浪费，为什么呢？双缓冲Surface只会提供两个Buffer，一个Buffer被DisPlay占用（SurfaceFlinger用完后不会释放当前的Buffer，只会释放旧的Buffer,**直观的想一下，如果新Buffer生成受阻，那么肯定要保留一个备份给SF用，才能不阻碍合成显示，就必定要一直占用一个Buffer，新的Buffer来了才释放老的**），另一个被GPU处理占用，所以，CPU就无法获取到Buffer处理当前UI，在Jank的阶段空空等待。一般出现这种场景都是连续的：比如复杂视觉效果每一帧可能需要20ms（CPU 8ms +GPU 12ms），GPU可能会一直超负荷，CPU跟GPU一直抢Buffer，这样带来的问题就是滚雪球似的掉帧，一直浪费，**完全没有利用CPU与GPU并行处理的效率，成了串行处理**，如下所示

![image.png](https://upload-images.jianshu.io/upload_images/1460468-3de0622bf2e05a14.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

如何处理呢？让多增加一个Buffer给CPU用，让它提前忙起来，这样就能做到三方都有Buffer可用，CPU跟GPU不用争一个Buffer，真正实现并行处理。如下：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-b88cf9b2eb3d6bb0.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

如上图所示，虽然即使每帧需要20ms（CPU 8ms +GPU 12ms），但是由于多加了一个Buffer，实现了CPU跟GPU并行，便可以做到了只在开始掉一帧，后续却不掉帧，双缓冲充分利用16ms做到低延时，三缓冲保障了其稳定性，为什么4缓冲没必要呢？因为三个既可保证并行，四个徒增资源浪费。  
 
# 总结

* 同步是防止画面撕裂的关键，VSYNC同步能防止画面撕裂
* VSYNC+双缓冲在Android中能有序规划渲染流程，降低延时
* Android已经采用了双缓冲，双缓冲不仅仅是两份存储，它是一个概念，双缓冲是一条链路，不是某一个环节，是整个系统采用的一个机制，需要各个环节的支持，从APP到SurfaceFlinger、到图像显示都要参与协作
* 三缓冲在UI复杂情况下能保证画面的连续性，提高柔韧性
