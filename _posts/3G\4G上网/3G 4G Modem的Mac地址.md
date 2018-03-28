## 3G/4G Modem的Mac地址

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
