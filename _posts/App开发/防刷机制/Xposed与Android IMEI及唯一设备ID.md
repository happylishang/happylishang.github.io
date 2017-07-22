应该是做“或”而不是做“与”


应该是做匹配，而不是把左右的统一起来，做橡胶，这样只会降低修改成本，这个手段应该交给后台，不仅仅是本地计算。

> 杨从安（数盟网络CEO）： 设备数据的规模性实际上是我们认为会不断扩大自己技术优势的地方。APP推广反作弊一个非常核心的指标就是设备识别的准确度， 这个指标很大程度上依赖于后台的真实设备数据库。打个比方，我们获取了一台小米手机的IMIE码和其他硬件信息，然后跟我们后台的数据库做校验，如果**数据匹配**，那就可以确定这部手机确实是小米公司正式发售的产品；如果不匹配，那这部手机有可能就是改码手机，需要重点监测。**截止2016年5月，我们累积覆盖去重设备超过5.46亿，占中国活跃安卓设备的95%以上。这个数据库确保了我们对存量用户的识别能力达到96%，**异常设备识别误差小于1.04%，这个精度是目前业界最高的。
 
> 这个是可以的， 也是其他的产品不能比较的。 具备可校验性，以确保ID的可靠。 这个的生成具有自己的规则，可以判断是否由数盟下发、其对应的app、开发者等信息等。无论是数盟还是数盟客户在拿到这个ID后，都可以很方便的去校验出这个ID是否是正


方案：存储手机的信息，做匹配，如果可以匹配的上，就是同一台设备，

## 甄别的特征值与权重

**所有通过Java类直接取到的数据都能作假**

* 手机型号（1）
* 手机Android版本（2）
* 基带版本（10）
* 内核版本（2）
* 版本号（3）
* IP地址（20）
* 序列号（30）
* IMEI（30）只有带有无线通信模块的才有
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

 
 
![431499323461_.pic.jpg](http://upload-images.jianshu.io/upload_images/1460468-fa523b01e778c3fc.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![441499323463_.pic.jpg](http://upload-images.jianshu.io/upload_images/1460468-e605d688b76e73e1.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 
![451499323464_.pic.jpg](http://upload-images.jianshu.io/upload_images/1460468-c81f0dee9e9dcc52.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 
![461499323465_.pic.jpg](http://upload-images.jianshu.io/upload_images/1460468-f072f24aebc79c08.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![471499323466_.pic.jpg](http://upload-images.jianshu.io/upload_images/1460468-077dea1818a6c9ce.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![481499323471_.pic.jpg](http://upload-images.jianshu.io/upload_images/1460468-64e9e9fa66fbe17d.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![491499323472_.pic.jpg](http://upload-images.jianshu.io/upload_images/1460468-49f0f544e9be7cb5.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


# 实现方法

* 反HOOK
* adb命令
* Native服务


# 反Hook，将被Hook的接口给还原

xposed框架并不改变原来的代码，只是将自己的代码给Hook进去，所以原来的函数都是没被改变的。这个是绕过Hook的关键，如果通过Hook改变了系统服务，那我们也没办法，因为Android对于硬件的访问基本都是通过封装服务，然后给Client调用，直接访问不太现实，比如获取IMEI，本质上是通过向芯片发送AT命令获取数据，理论上可以通过Adb来处理，但是由于不同的芯片对应AT命令集合应该不一样，另外在获取数据的时候，对应的串口名称也可能不同，有的是tty，有的是ttyUSB0，所以不太现实。

Xposed的做法一般是Hook服务的代理，比如Hook掉TelephoneManager中的getbDeviceId方法，但是如果能将方法给还原，还是可以拿到数据的，Binder通信的关键是知道Transaction ID，跟通信数据，知道这些，我们就能自己实现Binder的跨进程通信，绕过被Hook的函数，adb shell service call iphonesubinfo 1其实也是同样的原理。


            @Override
            public java.lang.String getbDeviceId(java.lang.String callingPackage) throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                java.lang.String _result;
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    _data.writeString(callingPackage);
                    mRemote.transact(Stub.TRANSACTION_getbDeviceId, _data, _reply, 0);
                    _reply.readException();
                    _result = _reply.readString();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
                return _result;
            }




Xposed之类的Hook不太可能直接Hook掉系统服务，只能Hook系统服务的代理，因为根据AIDL生成的系统服务接口是有ID号的，很难一一对应，所以核心是找到真正的系统服务代理。

[Xposed原理 深入理解Android（三）：Xposed详解](http://www.infoq.com/cn/articles/android-in-depth-xposed)       
[Android热修复升级探索——追寻极致的代码热替换](https://yq.aliyun.com/articles/74598?t=t1)
[Android ADB命令大全(通过ADB命令查看wifi密码、MAC地址、设备信息、操作文件、查看文件、日志信息、卸载、启动和安装APK等)](https://zmywly8866.github.io/2015/01/24/all-adb-command.html)    
[ART深度探索开篇：从Method Hook谈起] (http://weishu.me/2017/03/20/dive-into-art-hello-world/)

# 不需要root权限的adb命令

获取IMEI

		adb shell service call iphonesubinfo 1 | awk -F "'" '{print $2}' | sed '1 d' | tr -d '.' | awk '{print}' ORS=
		
		adb shell dumpsys iphonesubinfo

获取MAC   

	adb shell  cat /sys/class/net/wlan0/address

获得序列号

	 adb get-serialno
	  

# Native服务

Native服务需要与服务Code对应上，但是不同版本，不同手机厂Code不一样，底层怎么定位是个问题：AIDL服务生成的CODE是根据其文件函数声明的顺序。

# AT命令

AT命令不知道会不会跟不同的基带模块关系过于密切，如果密切，可能不同的芯片需要不同的命令，这些命令怎么统一的呢？应该跟每个手机厂商有关系。每个芯片有自己的AT命令库.不同芯片的3G模块所支持的AT 指令集会有差异，具体需要查看对应规格书。并且AT命令对应的串口也不一定能够着找到有的名字是tty，有的是ttyUSB0。



# MAC地址 比较靠谱，但是一定要获取真实的MAC

#  可能的问题

*  版本兼容：不同的版本获取IMEI的实现可能不同
* 只是针对Xposed的Hook处理，不能保证一定准确，如果ROM层或者服务层作假，就没有办法
* IMEI的需要Phone权限，如果不给可能获取不到，平板之类的没有无线通信模块的设备没哟IMEI
* 序列号