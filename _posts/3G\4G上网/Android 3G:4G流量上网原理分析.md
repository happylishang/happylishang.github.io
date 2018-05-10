
手机一般会提供两种上网方式：Wifi或者3G/4G上网，Wifi上网其实就是利用网卡通过以太网上网；3G/4G则是通过基带，利用蜂窝网络进行上网，之前已经简单的阐述了**[Wifi上网跟3G上网的区别](https://mp.csdn.net/mdeditor/79809066)**，本文主要探索Android 3G/4G上网的流程及原理。

* 无线上网硬件模型
* 3G/4G上网流程
* 3G/4G上网协议ppp
* ppp如何建立
* ppp的语音短信业务跟数据上网是同一种机制吗？或者说语音服务需要ppp吗（应该不需要）
* ppp如何影响IP地址
* socket如何通过基带模块发送数据、接收数据

# 概述

手机的信号是以电磁波的形式在空气中进行传播的，手机拨打电话时，会把语音转化成信号，然后通过电磁波的形式，发送到距离最近的基站A，基站A接收到信号之后，再通过交换机转发到覆盖对方手机信号的基站B，基站B再把信号发送给对方手机，手机接收到信号之后再把信号转换成语音，从而实现双方通话。嵌入式系统经常需要具备无线上网的功能，但在有的应用场景中无法使用wifi，这时可以通过GPRS模块上网。GPRS模块是基于AT命令进行控制的。对于单片机这类没有复杂操作系统的平台来说，往往要通过应用程序，直接发送AT命令给GPRS模块，以使GPRS模块连接到网络并建立TCP连接，进而完成通信。对于具有Linux、Android等系统的平台而言，则不需要自己编写程序发送AT命令，可以使用ppp服务进行拨号上网。


> MAC addresses only apply to IEEE 802 technologies, of which cellular networks are not.

* IEEE 802.1：高层局域网协议（Bridging (networking) and Network Management）
* IEEE 802.2：逻辑链路控制（Logical link control）
* IEEE 802.3：以太网（Ethernet）
* IEEE 802.4：令牌总线（Token bus）
* IEEE 802.5：令牌环（Token-Ring）
* IEEE 802.6：城域网（MAN, Metropolitan Area Network）

1.移动4G网络：移动的3G网络就是移动的痛，移动的网络中当有语音来电时都会选择回落到GSM网络的，极少回落3G网络 的，因为移动很清楚自己的3G网络无论是覆盖范围还是信号稳定度都很渣的。大家都知道2G网络不能在打电话的同时连接数据业务，因为移动4G语音回落2G 会导致电脑断网的。

2、.联通4G网络：联通3G的WCDMA网络速度快，信号稳定，语音电话时会回落到42Mb/s的3G网络，WCDMA允许通 话的同时连接数据业务，从这里可以看出，虽然联通的4G手机如果采用CSFB方案也不支持4G网络和语音同时进行，但是由其回落到WCDMA网络允许通 话的同时连接数据业务，因此语音通话时不会断网，但此时也不是工作在4G模式

3、电信4G网络由于CDMA与LTE并不是一个体系中的技术，所以LTE语音通话要回落到CDMA，通话结束再返回LTE 网络，电信就要在基站上做很大的改动，投入的资金较多的。

以太网跟蜂窝网络的区别

两者同为链路层协议，同时都处于TCP/IP协议的IP层之下，只是略有不同

PPP: Point-to-Point Protocol，链路层协议。用户实现点对点的通讯。
PPP协议中提供了一整套方案来解决链路建立、维护、拆除、上层协议协商、认证等问题。具体包含这样几个部分：链路控制协议LCP（Link Control Protocol）；网络控制协议NCP（Network Control Protocol）；认证协议，最常用的包括口令验证协议PAP（Password Authentication Protocol）和挑战握手验证协议CHAP（Challenge-Handshake Authentication Protocol）。




# 手机的基带模型

手机一般有**两块网卡**，只是不同时使用，Wifi的那种以太网卡，还有就是3G 4G的无限Modem型网卡，不过两者不同，无线Modem的没有Mac地址的概念，这种概念只存在以太网中，48位Mac地址，支持ARP RARP协议反查等等，PPP协议是如何确定IP跟MAC的呢？传统的以太网有事如何创建IP的嗯，Wifi扫描，获得IP ,DNS，路由表等等，之后再同学，PPP拨号走的是同样的逻辑。

手机功能的实现以应用处理器（AP）为主，基带芯片提供通信功能。可以把AP看作计算机，把基带芯片看作AP的无线modem。基带芯片就是用来合成即将发射的基带信号，或对接收到的基带信号进行解码。


# pppd上网模型

![](http://hi.csdn.net/attachment/201103/20/0_1300594096PX7Z.gif)

Android 3G/4G无线上网的网络协议模型如下：

![Android 无线流量上网模型.jpg](https://upload-images.jianshu.io/upload_images/1460468-80c9f07ed98f5d5f.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

pppd 提供基本的 LCP ，验证(authentication)的支援， 以及一个用来建立并配置网际网路协定 (Internat Protocol (IP) )（叫做 IP 控制 协定，IPCP）的 NCP 。

**pppd的作用是铺路，而不是走路，为上网铺路，铺完后，才走TCP/IP协议上网**

pppd是一个用户空间的后台服务进程(daemon。pppd实现了所有鉴权、压缩/解压和加密/解密等扩展功能的控制协议。pppd只是一个普通的用户进程，pppd与内核中的PPP协议处理模块之间通过设备文件(/dev/ppp)进行通信。pppd有一个辅助工具chat，用来与GSM模组建立会话。它向串口发送AT命令，建立与GSM模组的会话，以便让PPP协议可以在串口上传输数据包。

/dev/ppp

设备文件/dev/ppp。通过read系统调用，**pppd可以读取PPP协议处理模块的数据包**，当然，PPP协议处理模块只会把应该由pppd处理的数据包发给pppd。通过write系统调用，pppd可以把要发送的数据包传递给PPP协议处理模块。通过ioctrl系统调用，pppd可以设置PPP协议的参数，可以建立/关闭连接。在pppd里，每种协议实现都在独立的C文件中，它们通常要实现protent接口，该接口主要用于处理数据包，和fsm_callbacks接口，该接口主要用于状态机的状态切换。数据包的接收是由main.c: get_input统一处理的，然后根据协议类型分发到具体的协议实现上。而数据包的发送则是协议实现者根据需要调用output函数完成的。


发送数据

应用程序通过socket 接口发送TCP/IP数据包，这些TCP/IP数据包如何流经PPP协议处理模块，然后通过串口发送出去呢？pppd在make_ppp_unit函数调用ioctrl(PPPIOCNEWUNIT)创建一个网络接口（如ppp0），内核中的PPP协议模块在处理PPPIOCNEWUNIT时，调用register_netdev向内核注册ppp的网络接口，该网络接口的传输函数指向ppp_start_xmit。当应用程序发送数据时，内核根据IP地址和路由表，找到ppp网络接口，然后调用ppp_start_xmit函数，此时控制就转移到PPP协议处理模块了。ppp_start_xmit调用函数ppp_xmit_process去发送队列中的所有数据包，ppp_xmit_process又调用ppp_send_frame去发送单个数据包,
 ppp_send_frame根据设置，调用压缩等扩展处理之后，又经ppp_push调用pch->chan->ops->start_xmit发送数据包。pch->chan->ops->start_xmit是什么？它就是具体的传输方式了，比如说对于串口发送方式，则是ppp_async.c:
 ppp_asynctty_open中注册的ppp_async_send函数，ppp_async_send经ppp_async_push函数调用tty->driver->write把数据发送串口。


接收数据

接收数据的情形又是如何的？ppp_async.c在初始化(ppp_async_init)，调用tty_register_ldisc向tty注册了行规程处理接口，也就是一组回调函数，当串口tty收到数据时，它就会回调ppp_ldisc的
 ppp_asynctty_receive函数接收数据。ppp_asynctty_receive调用ppp_async_input把数据buffer转换成sk_buff，并放入接收队列ap->rqueue中。ppp_async另外有一个tasklet(ppp_async_process)专门处理接收队列ap->rqueue中的数据包，ppp_async_process一直挂在接收队列ap->rqueue上，一旦被唤醒，它就调用ppp_input函数让PPP协议处理模块处理该数据包。

在ppp_input函数中，数据被分成两路，一路是控制协议数据包，放入pch->file.rqb队列，交给pppd处理。另外一路是用户数据包，经ppp_do_recv/ppp_receive_frame进行PPP处理之后，再由netif_rx提交给上层协议处理，最后经 socket传递到应用程序。

 
![](http://hi.csdn.net/attachment/201112/27/0_132495801078on.gif)

# PPP

在数据链路层，PPP（Point-to-Point Protocol）协议提供了一种标准点对点的传输方式，为各种主机、网桥和路由器通信提供通用连接方案。PPP协议主要包括以下三个部分：

*  	令数据帧封装格式：基于HDLC(High Level Data Control，高层数据控制链路)标准，为串行数据链路上传输的数据包定义封装格式。
*  	链路控制协议LCP(Link Control Protocol)：用于封装格式选项的自动协商、链路的建立和终止、探测链路错误和配置错误。
*   认证协议，最常用的包括口令验证协议PAP（Password Authentication Protocol）和挑战握手验证协议CHAP（Challenge-Handshake Authentication Protocol）。
*  	网络控制协议NCP(Network Control Protocol)：PPP协议针对每一种网络层协议都有相应的网络控制协议，并通过它们完成点对点通信时网络层参数的配置，如I P地址、DNS的动态协商。

PPP协议下的令数据帧封装格式如下图：

![PPP协议数据帧格式](https://upload-images.jianshu.io/upload_images/1460468-6f0f749ee0019e99.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到，由于PPP协议是点对点的，不需要太多信息，因此这里是没有48位MAC地址概念的，因此，PPP也就无所谓 ARP（地址解析协议）和RARP（逆地址解析协议），这两个是某些网络接口（如以太网和令牌环网）使用的特殊协议。


# PPP数据链路建立流程
 
Android系统如果想要利用PPP协议进行数据通信，必须首先按照PPP协议建立数据通信链路。PPP数据链路的建立需要完成三个步骤，包括链路层配置、链路认证以及网络层配置，这个过程中，通信双方必须通过协商，确定数据包格式、IP地址等链路参数，才能正确建立PPP数据链路。在实际操作中，PPP数据链路的建立可分以下几个阶段：

* (1) 链路不可用阶段（Link Dead Phase）：PPP链路从这个阶段开始和结束，在该阶段，整条链路处于不可用状态，当通信双方检测到物理线路激活时，会从该阶段转入链路建立阶段。
* (2) 链路建立阶段 (Link Establishment Phase)：在此阶段，PPP链路将通过LCP进行协商，确定工作方式、认证方式、链路压缩等。如果LCP协商成功，则转入Opened状态，表示底层链路已经正确建立，如果链路协商失败，则会返回到第一阶段。链路建立成功后，如果配置了PPP认证，则会进入认证阶段，如果没有配置，则直接转入网络层协议阶段。
* (3) 认证阶段 (Authentication Phase)：在此阶段，PPP将进行用户认证工作，通过PAP或者CHAP验证用户名、密码等身份信息，如果认证失败，PPP链路进入链路终止阶段，拆除链路，如果认证成功则转入网络层协议阶段。
* (4) 网络层协议阶段 (Network-Layer Protocol Phase)；在此阶段，每种网络层协议会通过相应网络控制协议进行配置，本课题通过IPCP协商双方IP地址、DNS等，协商成功后，PPP链路便可基于TCP/IP发送或接收报文。
* (5) 链路终止阶段 (Link Termination Phase)：PPP能在任何时候终止链路，如认证失败、载波丢失等情况均会导致链路终止，PPP协议通过交换LCP报文来关闭链路，并通知网络层与物理层强制关闭链路，返回链路不可用阶段。链路建立流程如图3.19所示：

 ![链路建立流程](https://upload-images.jianshu.io/upload_images/1460468-9142952ab146c8e2.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 
 通信链路建立后，Android系统会为上层应用创建一个虚拟网络接口ppp0，该接口创建之初就已经从3G网络获得了动态分配的IP地址、路由表，对上层应用而言它是一块真实的，并且已经激活的网卡设备，用户可以像使用以太网卡一样，使用ppp0进行TCP/IP网络通信。

# Android流量数据上网的实现

在Android系统中，Java层3G应用访问网络时，会首先通过DataConnection类查看数据上网链路是否已经建立建立，如果已经建立，则直接使用已建立的网络接口，通过TCP/IP协议进行通信，如果还未建立，则需要首先建立数据通路。由3.5.1节可知，PPP拨号上网配置十分复杂，需要一系列的协商、验证，为了开发方便，Linux研发人对PPP数据链路的建立过程进行抽象，实现了pppd拨号应用程序，专门用于管理PPP数据链路的建立与关闭，本小节通过移植pppd-2.3.5实现Android系统的3G上网管理。

MC509基带模块正确加载之后会映射出多个ttyUSB设备文件，其中ttyUSB4用作AT命令发送通道，ttyUSB0用作数据通信通道，Android系统在底层通过串口多路复用机制，实现了USB串口的多路复用。在使用ttyUSB0建立数据链路的时候，首先通过发送AT命令，查看3G模块是否已经打开，随后利用pppd拨号程序建立数据链路。拨号链接过程遵循PPP通信协议，双方动态协商链路硬件配置参数、IP地址、DNS等信息，拨号成功后，Android系统会为MC509基带模块映射一个虚拟网络接口ppp0，之后，Android系统便可利用该接口进行网络通信了，Android系统数据链路的建立与应用模型如图：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-587bab1c2eee6a8c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到pppd其实只是负责建立数据链路，建立之后，数据上网的不会依赖pppd服务，打个比方就是：**pppd只负责修路，不负责运货**。

# 简单的开启与关闭流程
	
每次开启数据通道时，Android系统都会通过pppd拨号程序加载该配置信息，并利用gprs-connect-chat脚本建立数据链路。gprs-connect-chat表示链路建立的会话过程，位于/etc/ppp/chat目录下，该文件内容如下：

	# /etc/ppp/chat/gprs_connect_chat	建立数据连接脚本
	TIMEOUT 5										//连接超时为5秒
	ABORT 'NO CARRIER'
	ABORT 'ERROR'
	ABORT 'NO DIALTONE'
	ABORT 'BUSY'
	ABORT 'NO ANSWER'
	OK-AT-OK ATD#777								//返回一个OK则拨号#777
	CONNECT

在利用gprs_connect_chat脚本建立链接的过程中，主处理器首先向基带模块发送一个AT命令，测试模块是否开启，如果Modem返回诸如BUSY、NO CARRIER、ERROR之类的信息，则说明模块关闭，取消链路建立，链接失败；如果Modem返回OK，说明模块开启，然后拨号#777，期待模块返回CONNECT，如果返回了CONNECT，则表明链接建立成功。与之对应，数据链路断开则使用gprs_disconnect_chat	脚本来完成会话，内容如下：

	# /etc/ppp/chat/gprs_disconnect_chat	  断开数据连接脚本
	ABORT "BUSY"
	ABORT "ERROR"
	ABORT "NO DIALTONE"
	'' "/K"
	'' "+++ATH"

数据通路断开时，Android系统首先向基带模块发送AT命令测试模块是否开启，如果开启，则发送ATH，断开数据链接。至此，Android系统就能利用MC509基带模块正常拨号上网了。


# 参考文档

[ 在ARM-linux上实现4G模块PPP拨号上网](https://blog.csdn.net/zqixiao_09/article/details/52540887)    
[PPP和PPPoE的工作原理](https://blog.csdn.net/easebone/article/details/7370369)      
[链路层：SLIP、PPP、ARP、RARP](https://blog.csdn.net/mr_avin/article/details/54784059)       
[Linux PPP实现源码分析](https://blog.csdn.net/osnetdev/article/details/8958058)      
[移动终端基带芯片的基本架构介绍之二（移动终端中的基带芯片）](https://blog.csdn.net/lxl584685501/article/details/46771429)      
[Linux PPP详细介绍](https://blog.csdn.net/jmq_0000/article/details/7105287)