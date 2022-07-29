---
layout: post
title: "理解Android VSYNC与图形系统中的双缓冲、三缓冲"
category: Android


---

# VSYNC与画面撕裂

VSYNC即vertical sync，也称为垂直同步，是一种图形技术，主要就是强制将帧速率与显示器的刷新率同步，最初由 GPU 制造商提出，主要用来处理屏幕撕裂。首先了解下两个名词：FPS与屏幕刷新频率

* 帧率[Frame Rate，单位FPS]-显卡生成帧的速率，也可以认为是数据处理的速度
* 屏幕刷新频率 [Refresh Rate单位赫兹/HZ]：是指硬件设备刷新屏幕的频率，值一般是固定的，以黑白电视的电子扫描枪类比，比如60Hz的显示屏，每16ms电子枪从上到下从左到右一行一行逐渐把图片绘制出来。

两者要同步配合好才能高效的显示图像，可以人为帧率对应的是图像数据的输出，刷新率对应的是图像数据的屏幕展示，如果帧率同设备的刷新率不一致，而又没有采用合适的同步技术，会出现什么问题呢？可能会出现上述的屏幕撕裂[多帧的局部数据共同组成了一个完整帧]，示意如下：

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7c03e6553ccc49beae246ff2c3e0cd8b~tplv-k3u1fbpfcp-watermark.image?)

理论上来讲，只要没做到读/写线性同步就有几率发生撕裂， 只有帧数据完整更新+显示设备完整渲染才能阻止撕裂，相对应的撕裂的复现场景有两种：

* 1：**显示设备未完整渲染**： 假设显示设备**只有一块**显存存放显示数据，在没有同步加锁的情况下，帧数据由CPU/GPU处理完可随时写入到显存，如果恰好在上一帧A还没100%在屏幕显示完的时候，B帧到达，并且覆盖了A，那么在继续刷新下半部分时，绘制的就是B帧数据，此时就会出现上半部分是A下半部分是B，即发生屏幕撕裂：如下

![image.png](https://upload-images.jianshu.io/upload_images/1460468-4424c66d36b291f2.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 
* 2 **帧数据未完整更新 **：依旧假设显示设备**只有一块**显存存放显示数据，如果在GPU覆盖旧帧的间隙，也就是显存数据没有100%刷新的时候，通知渲染到屏幕，这个时候同样会发生上述事情，即使用了半成品的帧：撕裂帧。[参考视频](https://youtu.be/1iaHxmfZGGc?list=UU_x5XG1OV2P6uZZ5FSM9Ttw&t=112)

所以**同步锁的机制是撕裂的关键**，必须有这么一个机制告诉GPU显卡，**要等待当前帧绘完整，才能替换当前帧**，即VSYNC，VSYNC**强制帧率和显示器刷新频率同步**，如果当前帧没绘制完，即使下一帧准备好了，也禁止使用下一帧，直到显示器绘制完当前帧，即：60HZ显示器，开了垂直同步后，显示帧率就会限定最高60，即使显卡输出高达90FPS也没用，甚至可以认为他是一种妥协性优化，一定程度上还会降低性能。以上都是针对一块显示存储的情况，理论上只要加锁就能解决，读的时候禁止写，但这么做无疑会大大降低效率，所以不能简单依靠单纯加锁解决问题。
 
# 双缓冲+垂直同步

如何解决单缓冲+同步的性能问题呢？多增加一块显示存储区能解决吗？假定显示设备有两块显存，BackBuffer与FrontBuffer，可以简单的认为CPU/GPU占据一个缓冲、当前呈现的数据占据一个缓冲，GPU/CPU 绘制更新BackBuffer，不需要关心正在展示的FrontBuffer，这就是双缓冲，相比于单缓存，双缓冲可让写与读分离，提高效率。但紧靠双缓冲理论上解决不了撕裂的问题，BackBuffer毕竟也是要展示的，也要”拷贝“到FrontBuffer，如果不对拷贝操作添加干预，也可能出现撕裂，VSYNC机制必须兼具**禁止在刷新的过程中更新FrontBuffer**的功能，所有的COPY或者说是Page flipping操作都要等待上一帧完全渲染完才可以，渲染完成之后，显示设备就按节奏可以发出下一个VSYNC信号，通知BackBuffer与FrontBuffer间进行拷贝，拷贝结束后，接着进行下一帧屏幕渲染，这样就能避免屏幕撕裂，当然，如果BackBuffer还未来得及完成帧更新也是需要阻断拷贝过程，否则就是渲染了半成品的帧，所以个人人为，Vsync解决撕裂、双缓冲来解决性能。


> It does this by preventing the GPU from doing anything to the display memory until the monitor has concluded its current refresh cycle — effectively not feeding it any more information until it’s ready for it. Through a combination of double buffering and page flipping, VSync synchronizes the drawing of frames onto the display only when it has finished a refresh cycle, so you shouldn’t ever see tears when VSync is enabled.


![image.png](https://upload-images.jianshu.io/upload_images/1460468-30ac3ea4118e9390.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


对Android系统而言，VSYNC除了强制帧率和显示器刷新频率同步外，还有其他很多作用，在Android Jelly Bean之前VSYNC使用的场景比较少，只用在最后缓冲区切换，系统的其他环节没用，这种做法可能会让CPU浪费在其他低优先级的业务上，如下图:

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/1c602db00e23427989b505ebd6e6a5fa~tplv-k3u1fbpfcp-watermark.image?)

如此情况就是一次jank

![image.png](https://upload-images.jianshu.io/upload_images/1460468-966ca5f42592eeff.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


Jelly Bean之前VSYNC仅用在最后的图像显示阶段，防止屏幕撕裂，但是并未协调UI的绘制，CPU对于显示帧的处理是凌乱的，VSYNC到达后，如果CPU被其他任务占据，UI绘制的执行就会延迟，等到它开始处理UI生成帧的时候，可能已经处于16ms的中间，这样就很**容易跨两个VYSNC**信号，导致掉帧。在Jelly Bean中，下一帧的处理被限定在VSync信号到达时，并且依赖Android的消息屏障机制，将UI重绘消息的优先级是提高，其他的同步消息均不会执行，由于是**在每个VSYNC信号到达时就处理帧，可以让UI绘制充分使用16ms耗时，可以尽量避免跨越两帧的情况出现**。

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/fc7a8a63aa984a0bb06ab18796935c5c~tplv-k3u1fbpfcp-watermark.image?)

这种做法保证了UI绘制的执行时间，虽然不能完全解决jank【比如本身绘制就超过16ms】，但是对于本来就小于16ms的任务是能保证的，从而降低jank的概率，因此VSYNC+双缓冲能够很好降低单缓冲的性能问题，降低延时。

# 双缓冲的进阶：三缓冲

之前的VSYNC+双缓冲流程图示都是用1、2、3代表第帧来表示更新流程，接线来用缓冲区代表，看一下双缓冲的数据流向，理想情况下，16ms内CPU处理完数据，将缓冲区A交给GPU，GPU接着处理A，结束后，等下个VSYNC与前面展示缓冲区B交换，A进行屏幕渲染，B回收用来继续生成下一帧，如下图所示：

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/fc71e135be09466697802ddc17f4e1a8~tplv-k3u1fbpfcp-watermark.image?)

在这种模型下，CPU与GPU其实是一种串行处理的操作，存在资源的浪费，因为两者其一必空闲，毕竟没有多余的缓冲区让其处理数据，理想情况下其实双缓冲并未有什么不妥，但是一旦CPU或者GPU处理超时，jank就很容易发生。


>  VSYNC+双缓冲保证低延时，三缓冲保证稳定性：让闲置的资源动起来

双缓冲模型中显示、CPU、GPU处理都会用到Buffer，VSYNC+双缓冲在理想情况下是没有问题的，但如果某个环节出现问题，那就不一样了，比如某些帧耗时是[CPU 8ms +GPU 12ms]，超过了16ms，如下：

![双缓冲jank](https://www.androidpolice.com/wp-content/uploads/2012/07/0001_Layer-72.png)

可以看到在第二个阶段，存在CPU资源浪费，双缓冲只会提供两个Buffer，B被GPU处理占用，A正在用显示，那么在第二个16ms里面，CPU就无法获取到Buffer处理UI更新，在Jank的阶段空空等待。而且，一般出现这种场景都是连续的：比如复杂视觉效果，那么GPU可能会一直超负荷，CPU一直跟GPU抢Buffer，这样带来的问题就是滚雪球似的掉帧，一直浪费，**完全没有利用CPU与GPU并行处理的效率，成了串行处理**，如下所示

![image.png](https://upload-images.jianshu.io/upload_images/1460468-3de0622bf2e05a14.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

如何处理呢？多增加一个Buffer给CPU用，让它提前忙起来，这样就能做到三方都有Buffer可用，CPU不用跟GPU争一个Buffer，真正实现并行处理。如下：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-b88cf9b2eb3d6bb0.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

如上图所示，虽然即使每帧需要20ms【CPU 8ms +GPU 12ms】，但是由于多加了一个Buffer，实现了CPU跟GPU并行，便可以做到了只在开始掉一帧，后续却不掉帧，双缓冲充分利用16ms做到低延时，三缓冲保障了其稳定性，为什么4缓冲没必要呢？因为三个既可保证并行，四个徒增资源浪费。  在Android系统中，双缓冲不仅仅是两份存储，它是一个概念，双缓冲是一条链路，不是某一个环节，是整个系统采用的一个机制，需要各个环节的支持，从APP到SurfaceFlinger、到图像显示都要参与协作。对于APP端而言，每个Window都是一个双缓冲的模型，一个Window对应一个Surface，而每个Surface里至少映射两个存储区，一个给图层合成显示用，一个给APP端图形处理，这便是应于上层的双缓冲。

# 总结

* 同步是防止画面撕裂的关键，VSYNC同步能防止画面撕裂
* VSYNC+双缓冲在Android中能有序规划渲染流程，降低延时
* Android已经采用了双缓冲，双缓冲不仅仅是两份存储，它是一个概念，双缓冲是一条链路，不是某一个环节，是整个系统采用的一个机制，需要各个环节的支持，从APP到SurfaceFlinger、到图像显示都要参与协作
* 三缓冲在UI复杂情况下能保证画面的连续性，提高柔韧性




![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/4e95445860a447ee82a610bcb025a65f~tplv-k3u1fbpfcp-watermark.image?)


Sets 1, 2 and 3 represent the operation of single, double and triple buffering, respectively, with vertical synchronization (vsync) enabled. In each graph, time flows from left to right. Note that 3 shows a swap chain with three buffers; the original definition of triple buffering would throw away frame C as soon as frame D finished, and start drawing frame E into buffer 1 with no delay. Set 4 shows what happens when a frame (B, in this case) takes longer than normal to draw. In this case, a frame update is missed. In time-sensitive implementations such as video playback, the whole frame may be dropped. With a three-buffer swap chain in set 5, drawing of frame B can start without having to wait for frame A to be copied to video memory, reducing the chance of a delayed frame missing its vertical retrace.

拷贝的时间，你可以做的别的，GPU的时间，CPU可以做别的


# 参考文档

[Google I/O 2012 - For Butter or Worse: Smoothing Out Performance in Android UIs
](https://www.youtube.com/watch?v=Q8m9sHdyXnE)

[Android Performance Patterns: Understanding VSYNC
](https://www.youtube.com/watch?v=1iaHxmfZGGc&list=UU_x5XG1OV2P6uZZ5FSM9Ttw&index=2288)