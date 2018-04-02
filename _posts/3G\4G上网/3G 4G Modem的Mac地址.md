# Android手机两种网络Wifi/3G/4G的Mac地址

>1X/3G/4G interfaces on cellular devices do have a MAC address, but those MACs are dynamically assigned and change on every reboot of the device... this is because MAC addresses only apply to IEEE 802 technologies, of which cellular networks are not.

>So yes, cellular networks are dynamically assigned a MAC address on a smartphone when that device is powered on or rebooted, however, these dynamically assigned MACs cannot be used in a firewall (it would literally be pointless to do so).

>However, @joeqwerty comment is incorrect: "MAC addresses are locally significant, so you can't block based on the MAC address of a remote device"

>While MAC addresses are locally significant, you can, and should, allow or block network connections via the MAC address of a remote device. It is possible, quite easily, to change a MAC address on a device, however it's more secure than blocking IP addresses, and less secure than blocking host names.

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

# 蜂窝网络的"MAC地址"及IP地址的分配

手机的信号是以电磁波的形式在空气中进行传播的，手机拨打电话时，会把语音转化成信号，然后通过电磁波的形式，发送到距离最近的基站A，基站A接收到信号之后，再通过交换机转发到覆盖对方手机信号的基站B，基站B再把信号发送给对方手机，手机接收到信号之后再把信号转换成语音，从而实现双方通话。

嵌入式系统经常需要具备无线上网的功能，但在有的应用场景中无法使用wifi，这时可以通过GPRS模块上网。GPRS模块是基于AT命令进行控制的。对于单片机这类没有复杂操作系统的平台来说，往往要通过应用程序，直接发送AT命令给GPRS模块，以使GPRS模块连接到网络并建立TCP连接，进而完成通信。对于具有Linux、Android等系统的平台而言，则不需要自己编写程序发送AT命令，可以使用ppp服务进行拨号上网。


PPP即Point to Point Protocol，是一种用于建立通过拨号调制解调器的网络连接、DSL连接或者其它类型的点对点连接的协议。

严格来说，蜂窝网络没有传统意义上的48位MAC地址，可以看下一下PPP协议的数据封装格式，由于是点对点的，不需要太多信息，而且PPP不支持 ARP（地址解析协议）和RARP（逆地址解析协议），这两个是某些网络接口（如以太网和令牌环网）使用的特殊协议。

![PPP协议数据帧格式](https://upload-images.jianshu.io/upload_images/1460468-6f0f749ee0019e99.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到，在PPP协议中，压根没有48位MAC地址的概念。


PPPoE（英语：Point-to-Point Protocol Over Ethernet），以太网上的点对点协议，是将点对点协议（PPP）封装在以太网（Ethernet）框架中的一种网络隧道协议。

与传统的接入方式相比，PPPoE具有较高的性能价格比，它在包括小区组网建设等一系列应用中被广泛采用，目前流行的宽带接入方式ADSL就使用了PPPoE协议。随着低成本的宽带技术变得日益流行，DSL(Digital Subscriber Line)数字用户线技术更是使得许多计算机在互联网上能够酣畅淋漓的冲浪了。但是这也增加了DSL服务提供商们对于网络安全的担心。通过ADSL方式上网的计算机大都是通过以太网卡(Ethernet)与互联网相连的。同样使用的还是普通的TCP/IP方式，并没有附加新的协议。另外一方面，调制解调器的拨号上网，使用的是PPP协议，即Point to Point Protocol，点到点协议，该协议具有用户认证及通知IP地址的功能。PPP over Ethernet(PPPoE)协议，是在以太网络中转播PPP帧信息的技术，尤其适用于ADSL等方式。

PPP: Point-to-Point Protocol，链路层协议。用户实现点对点的通讯。
PPP协议中提供了一整套方案来解决链路建立、维护、拆除、上层协议协商、认证等问题。具体包含这样几个部分：链路控制协议LCP（Link Control Protocol）；网络控制协议NCP（Network Control Protocol）；认证协议，最常用的包括口令验证协议PAP（Password Authentication Protocol）和挑战握手验证协议CHAP（Challenge-Handshake Authentication Protocol）。

PPPoE分为两个阶段：

PPPoE发现
由于传统的PPP连接是创建在串行链路或拨号时创建的ATM虚电路连接上的，所有的PPP帧都可以确保通过电缆到达对端。但是以太网是多路访问的，每一个节点都可以相互访问。以太帧包含目的节点的物理地址（MAC地址），这使得该帧可以到达预期的目的节点。 因此，为了在以太网上创建连接而交换PPP控制报文之前，两个端点都必须知道对端的MAC地址，这样才可以在控制报文中携带MAC地址。PPPoE发现阶段做的就是这件事。除此之外，在此阶段还将创建一个会话ID，以供后面交换报文使用。

PPP会话
一旦连接的双方知道了对端的MAC地址，会话就创建了。



DHCP
　　
# 手机几块网卡

如果从技术角度来看，两块，只是不同时使用，Wifi的那种以太网卡，还有就是3G 4G的无限Modem型网卡，不过两者不同，无限Modem的没有Mac地址的概念，这种概念只存在以太网中，48位Mac地址，支持ARP RARP协议反查等等，PPP协议是如何确定IP跟MAC的呢？传统的以太网有事如何创建IP的嗯，Wifi扫描，获得IP ,DNS，路由表等等，之后再同学，PPP拨号走的是同样的逻辑。



在智能手机中，手机功能的实现以应用处理器（AP）为主，基带芯片提供通信功能。可以把AP看作计算机，把基带芯片看作AP的无线modem。  射频部分和基带部分是基带芯片的核心。目前的主流是将射频收发器(小信号部分)集成到手机基带中，未来射频前端也有可能集成到手机基带里，而随着模拟基带和数字基带的集成越来越成为必然的趋势，射频可能最终将被完全集成到手机基带芯片中。射频部分一般是信息发送和接收的部分；基带部分一般是信息处理的部分。基带芯片就是用来合成即将发射的基带信号，或对接收到的基带信号进行解码。
       
# 参考文档

[ 在ARM-linux上实现4G模块PPP拨号上网](https://blog.csdn.net/zqixiao_09/article/details/52540887)    
[PPP和PPPoE的工作原理](https://blog.csdn.net/easebone/article/details/7370369)      
[链路层：SLIP、PPP、ARP、RARP](https://blog.csdn.net/mr_avin/article/details/54784059)       
[Linux PPP实现源码分析](https://blog.csdn.net/osnetdev/article/details/8958058)      
[移动终端基带芯片的基本架构介绍之二（移动终端中的基带芯片）](https://blog.csdn.net/lxl584685501/article/details/46771429)