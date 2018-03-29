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