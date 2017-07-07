应该是做“或”而不是做“与”


应该是做匹配，而不是把左右的统一起来，做橡胶，这样只会降低修改成本，这个手段应该交给后台，不仅仅是本地计算。

> 杨从安（数盟网络CEO）： 设备数据的规模性实际上是我们认为会不断扩大自己技术优势的地方。APP推广反作弊一个非常核心的指标就是设备识别的准确度， 这个指标很大程度上依赖于后台的真实设备数据库。打个比方，我们获取了一台小米手机的IMIE码和其他硬件信息，然后跟我们后台的数据库做校验，如果**数据匹配**，那就可以确定这部手机确实是小米公司正式发售的产品；如果不匹配，那这部手机有可能就是改码手机，需要重点监测。**截止2016年5月，我们累积覆盖去重设备超过5.46亿，占中国活跃安卓设备的95%以上。这个数据库确保了我们对存量用户的识别能力达到96%，**异常设备识别误差小于1.04%，这个精度是目前业界最高的。
 

> 这个是可以的， 也是其他的产品不能比较的。 具备可校验性，以确保ID的可靠。 这个的生成具有自己的规则，可以判断是否由数盟下发、其对应的app、开发者等信息等。无论是数盟还是数盟客户在拿到这个ID后，都可以很方便的去校验出这个ID是否是正


方案：存储手机的信息，做匹配，如果可以匹配的上，就是同一台设备，

## 甄别的特征值与权重

* 手机型号（1）
* 手机Android版本（2）
* 基带版本（10）
* 内核版本（2）
* 版本号（3）
* IP地址（20）
* 序列号（30）
* IMEI（30）
* ICCID（20）
* 传感器信息

![](https://www.shuzilm.cn/img/jietu/xiangxishuju1.png)

## 训练模型

![341499245696_.pic_hd.jpg](http://upload-images.jianshu.io/upload_images/1460468-cba043a6a6d5db75.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# 从使用方式上来看

 数盟的需要联网才可以用，也就说，要把手机信息带给后台校验，之后才可以。并且其携带的数据量，不小2.6k，可以说收集了很多当前手机的信息。
 
 ![421499323212_.pic.jpg](http://upload-images.jianshu.io/upload_images/1460468-47eada0f4b6cfb3a.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 

# 获取硬件信息

很难，一般只有root用户才能直接访问硬件信息，ROM定制的时候，硬件抽象层服务


# 判断模拟器

检测方法：没有绝对安全的检测方法，可以将很多场景做“或”，发现满足一个是模拟器，就是模拟器。

[Android模拟器检测常用方法](http://blog.csdn.net/sinat_33150417/article/details/51320228)          


硬件参数信息：

![431499323461_.pic_hd.jpg](http://upload-images.jianshu.io/upload_images/1460468-438f641a1a574521.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 
![431499323461_.pic.jpg](http://upload-images.jianshu.io/upload_images/1460468-fa523b01e778c3fc.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![441499323463_.pic_hd.jpg](http://upload-images.jianshu.io/upload_images/1460468-2b471a02c8fbb476.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 

![441499323463_.pic.jpg](http://upload-images.jianshu.io/upload_images/1460468-e605d688b76e73e1.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![451499323464_.pic_hd.jpg](http://upload-images.jianshu.io/upload_images/1460468-c067e3771db0a9cc.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![451499323464_.pic.jpg](http://upload-images.jianshu.io/upload_images/1460468-c81f0dee9e9dcc52.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![461499323465_.pic_hd.jpg](http://upload-images.jianshu.io/upload_images/1460468-244d77cc9755aaca.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 

![461499323465_.pic.jpg](http://upload-images.jianshu.io/upload_images/1460468-f072f24aebc79c08.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![471499323466_.pic_hd.jpg](http://upload-images.jianshu.io/upload_images/1460468-cecaa894d2e6edf9.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


![471499323466_.pic.jpg](http://upload-images.jianshu.io/upload_images/1460468-077dea1818a6c9ce.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![481499323471_.pic_hd.jpg](http://upload-images.jianshu.io/upload_images/1460468-dd7f967b771bf895.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 

![481499323471_.pic.jpg](http://upload-images.jianshu.io/upload_images/1460468-64e9e9fa66fbe17d.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![491499323472_.pic_hd.jpg](http://upload-images.jianshu.io/upload_images/1460468-7eaaf74636551172.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


![491499323472_.pic.jpg](http://upload-images.jianshu.io/upload_images/1460468-49f0f544e9be7cb5.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![501499323475_.pic_hd.jpg](http://upload-images.jianshu.io/upload_images/1460468-0a9ac004c39ef8af.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)



# 实现方法

* 反HOOK
* adb命令
* Native服务

[Android ADB命令大全(通过ADB命令查看wifi密码、MAC地址、设备信息、操作文件、查看文件、日志信息、卸载、启动和安装APK等)](https://zmywly8866.github.io/2015/01/24/all-adb-command.html)     


一下命令不需要权限

获取IMEI

		adb shell service call iphonesubinfo 1 | awk -F "'" '{print $2}' | sed '1 d' | tr -d '.' | awk '{print}' ORS=
		
		adb shell dumpsys iphonesubinfo

获取MAC   

	adb shell  cat /sys/class/net/wlan0/address

获得序列号

	 adb get-serialno
	  
