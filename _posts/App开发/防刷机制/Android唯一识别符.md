# Android中常用的唯一标识符


### IMEI : (International Mobile Equipment Identity) 或者MEID :（ Mobile Equipment IDentifier ）

IMEI是国际移动设备身份码的缩写，由15位数字组成的"电子串号"，它与每台手机一一对应，而且该码是全世界唯一的（应该）。MEID是全球唯一的56bit CDMA制式移动终端标识号，用来对CDMA制式移动式设备进行身份识别。

### DEVICE_ID （IMEI或者MEID，或者ESN ，不同平台返回不一致）

问题

* 非手机设备：如平板电脑、电视、音乐播放器等，系统中也就没有电话服务模块，自然也就无法获得IMEI或者MEID。
* 权限问题：获取DEVICE_ID需要READ_PHONE_STATE权限，如果用户不授权，将获取不到。 
* 小厂商的bug，可能重复，也可能没有
 
###  MAC 或者蓝牙地址
 
* 如果WiFi没有打开过，是无法获取其Mac地址的；
* 蓝牙是只有在打开的时候才能获取到其Mac地址。
 
### Serial Number

硬件序列，在Android 2.2 以上可以通过 android.os.Build.SERIAL 获得序列号。在一些没有电话功能的设备会提供，某些手机上也可能提供（所以就是经常会返回Unknown），但是这种方式很容易被HOOK

### ANDROID_ID

ANDROID_ID是设备第一次启动时产生和存储的64bit的一个数，当设备被wipe后该数重置，ANDROID_ID 在手机升级时也可能会变化的。

对比下同一台手机刷机前后的AndroidID就会发现，两者不一致： 

* Nexus 6P-7.0 AndroidId：42265436a328e9a9
* Nexus 6P-8.0 AndroidId：2d9e2d652460b187

# 遇到的问题

在一些被ROOT的手机上，经常会装上XPosed等第三方框架，这就导致很多Java层API都可以被Hook，如果我们在通过这些API拿数据的时候，获取的就不是真实的数据，Xposed框架及第三方插件是常见的作假、刷单手段。而如果想要拿到真实的数据就必须绕过被Hook的API，调用真实的API或者直接调用服务。具体实现方法有如下几种：

* Java层反HOOK：不直接调用可能被Hook的API，
* adb命令：C实现，可以降低被hook的概率
* 编写native代码，绕过Java层Hook，或者通过AT命令或者其他手段直接访问硬件

以上三种方法各有优缺点，Java层实现的话，业务层面相对较高，对底层ROM、厂商、版本的要求相对较低，越往下面走，对于版本、或者厂商的依赖越强。

adb命令的话，使用不太方便，因为有些服务其实还是通过Binder去请求，通过adb返回的数据比较原始，解析稍微麻烦些，而且可能因为版本及厂商的不同导致命令不统一，比如6.0的手机上需要如下命令


		adb shell service call iphonesubinfo 1 | awk -F "'" '{print $2}' | sed '1 d' | tr -d '.' | awk '{print}' ORS=

而之前的可能需要如下命令
		
		adb shell dumpsys iphonesubinfo
		

native代码实现：如果服务是通过AIDL定义的Java层服务，Native层访问Java层服务就很别扭，因为native需要先获取Java层的TransactionID，不同的服务ID、参数都不一样，而且不同版本可能也会变，这就给native层实现带来了兼容的问题。

而想要通过Native代码直接访问硬件也会遇到很多问题，因为我们压根不知道硬件设备的设备名，比如发送AT命令获取IMEI，不同厂商基带模块设置的串口号不一定相同，有的是tty，有的是TTYUSB0；其次，不同基带模块的AT命令集也不一样，Android框架将很多底层实现给屏蔽了，APP只能通过请求系统服务才能获取到底层信息。

综合考虑，在Java层反Hook的代价较小，并且，一般的作假插件不会直接Hook系统服务，只会Hook服务代理，如果系统服务被Hook，很多内部调用同样可能会出现问题，在Java层相对安全，但是同样可以分为几个层次，越往下被Hook的风险越低，但是兼容性越差。

* 用户API层反Hook
* 隐藏API层反Hook
* 针对Proxy的反Hook
* 服务被Hook（无解）

以Xposed为例，Xposed本身没有修改代码的能力，只是将自己的代码Hook到目标函数中去，在目标函数调用之前或者之后进行一些处理，也就是所目标类对应的接口还是被保留的，否则直接调用就会出错，以DeviceID的获取为例子,通过系统API获取IMEI方式如下

	((TelephonyManager) getSystemService(Context.TELEPHONY_SERVICE)).getDeviceId()

Xposed只需将TelephonyManager的getDeviceId()函数给Hook掉，我们就无法通过系统API获取到真正的DeviceID，看下TelephonyManager的getDeviceId函数：

    public String getDeviceId() {
        try {
            return getITelephony().getDeviceId();
        } catch (RemoteException ex) {
            return null;
        } catch (NullPointerException ex) {
            return null;
        }
    }
    
    private ITelephony getITelephony() {
        return ITelephony.Stub.asInterface(ServiceManager.getService(Context.TELEPHONY_SERVICE));
    }
    
可以看到getDeviceId函数首先通过getITelephony获取TELEPHONY_SERVICE服务的代理，然后通过Binder通信想TELEPHONY_SERVICE服务请求获取IMEI信息。而一旦被Hook，那我们的调用就可能变成如下：

    public String getDeviceId() {
         ...
         return hook-> getDeviceId();
    }
    
也就是说：getDeviceId被Hook后，最后返回的时候，其实返回的不是原来的结果，那这里的反Hook方式可以分成两种：

* 仅仅对getDeviceId()反Hook
  
  我们假设Xposed只hook了getDeviceId ，而没有对private方法getITelephony进行任何修改，那这个时候可以通过反射获取getITelephony方法，然后利用ITelephony.Stub.asInterface生成的Binder服务代理访问TELEPHONY_SERVICE服务，获得IMEI。

* 针对隐藏方法getITelephony的反Hook  

由于是Java层，private方法同样可以被Hook，如果getITelephony被Hook，那上面的方法就失效，这个时候，我们可以直接利用ServiceManager.getService的获取方法，绕过隐藏方法被Hook的坑。以上两种方法是在代理层做的反Hook，但是如果Proxy类被Hook，以上两种方法都会失效。

* 针对Prxoy做的反HooK

AIDL文件在预编译阶段会生成辅助Java接口类，这些是Java层Binder服务的基础，如果这些类被Hook，比如:

        private static class Proxy implements com.android.internal.telephony.ITelephony {
            private android.os.IBinder mRemote;
            ..
            }

如果这个类被Hook，那么我们只能自己复原Binder的Proxy才可以，我们可以自己根据AIDL生成辅助方法，让后通过ServiceManager获得服务代理，进而转换，执行。如果服务或者ServiceManager被Hook，那就无能为力，好比整个服务都被Hook，而不仅仅是入口被Hook。

	private static String getRemoteDeviceId(String callingPackage,
	                                                      IBinder remote,
	                                                      String DESCRIPTOR,
	                                                      int tid) throws RemoteException {
	        android.os.Parcel _data = android.os.Parcel.obtain();
	        android.os.Parcel _reply = android.os.Parcel.obtain();
	        String _result;
	        try {
	            _data.writeInterfaceToken(DESCRIPTOR);
	            _data.writeString(callingPackage);
	            remote.transact(tid, _data, _reply, 0);
	            _reply.readException();
	            _result = _reply.readString();
	        } finally {
	            _reply.recycle();
	            _data.recycle();
	        }
	        return _result;
	    }
	
	    //根据方法名，反射获得方法transactionId
	    private static int getTransactionId(Object proxy,
	                                        String name) throws RemoteException, NoSuchFieldException, IllegalAccessException {
	        int transactionId = 0;
	        Class outclass = proxy.getClass().getEnclosingClass();
	        Field[] fields=outclass.getDeclaredFields();
	        Field idField = outclass.getDeclaredField(name);
	        idField.setAccessible(true);
	        transactionId = (int) idField.get(proxy);
	        return transactionId;
	    }

# 如何判断模拟器

模拟器很容易安装Xposed框架，并进行造假，对于模拟器的识别如果限制的太严格，则容易误杀，如果限制太轻，则容易被钻漏洞，目前对于模拟器的识别主要是依据以下几个指标：

*  通过Build.java里面的参数，查找一些模拟器相关的信息，比如没有手机制造商信息，设备信息中包含模拟器信息等，但是很容易Hook作假。
* 模拟器没有无线通信模块，所以其IMEI要么是空，要么是通过作假写入的固定值，很多模拟的DeviceId都是0000000000的格式，如果通过系统API拿到的是全是是0000000000的格式，则可以看做模拟器，但是系统API很容易被Hook，可以将IMEI改成非0000000格式，如果取到的不是0000000，却不能简单的看做不是模拟器，仅仅是一个充分非必要条件。
* 通过反Hook读取的IMEI如果是0000000000，则可以看做是模拟器，但是如果是null却不可以，因为可能是权限问题导致的。
* 可以通过反Hook可以拿到相对真实的IMEI，对比系统API读取的信息跟反Hook拿到的是否一致，如果系统API读取的非空，但是反Hook拿到的是null，或者都是0000000格式，那说明这是一个模拟器。
* 真机基本都是基于ARM架构，而模拟器通常是x86，打印出的cpu信息经常包括 intel、amd，这也可以作为一个参考，不过有误杀的可能性。

我们目前的做法可以对以上几个充分非必要条件进行做或，只要满足一个就看做是模拟器，但是由于怕误杀，目前无法保证100%识别，假如，有人重新编译ROM，在服务层面作假，那真的是很难识别出来，将来，可以通过真机模型的匹配进行更加深层的识别。
 
# 结论

通过Java层反Hook，可以在三个层面获取IMEI，对于存在无线通信模块的手机应该都没问题，只不过需要权限，如果用户不被权限，我们也没办法。

对于AndroidID，目前只能做到Java层用户API反Hook，如果隐藏方法被Hook，就获取不到。因为隐藏方法太不统一，没办法自己完全复写，后期看看能不能优化。

针对MAC地址，6.0之前，获取mac地址的方案可以采用以上方法避免被Hook，但是，6.0之后，不行，Google屏蔽了系统API，返回的是统一为伪码，但是，可以通过sys/class/net/wlan0下的address获取。这种方法似乎可以不被Hook，但是不同ROM对于wifi是否打开过及是否打开要求不一。

所以目前基本能保证：

* 用户授权情况下IMEI可以获取成功（不包括平板）
* 对于AndroidID，可以保证用户API层反Hook获取成功
* 对于MAC，可以保证6.0之前的用户层API层反HOOK，6.0之后，通过读取系统文件获取，6.0之后的还没找到如何通过直接访问服务获取mac地址 
* 整合主流的模拟器识别手段+反HOOK获取IMEI进行判断，基本能识别Xposed手段的造假。


# 目前《严选》采用的DeviceID获取手段

方法：通过系统API获取MAC地址+DeviceID（IMEI），

问题：被作假的成本很低，目前的Xposed就能达到作假的目的，比如IEMI 在最浅的用户API层的作假就无法识别。


# 将来的优化方案

虽然通过反Hook能拿到相对真实的硬件信息，但是，并也不能保证100%准确，毕竟都是要通过服务来拿，主流的Hook手段都是Hook代理，所以我们可以绕过被Hook的代理，自己去请求服务，但是，如果服务被Hook，就无法拿到真实的硬件信息，毕竟我们没有办法直接访问硬件。

另外，反Hook的手段分了几个层次，虽然越深越可靠，但是越可能因为ROM的差异导致获取不到硬件信息，反而得不偿失，可以由深到浅反向拿信息，如果在深层拿到，真实性越高，拿不到的话，再逐层往上层拿，直到在在用户API层或者在相对统一的隐藏API层拿到，并将这部分获取的信息作为一个参考备份，与目前的数据对比，根据获取成功的概率，逐步判断是否可以采用当前的方案。

以下是IMEI Hook及不同层次的反Hook示意图：

![Hook与反Hook结果对比](https://user-gold-cdn.xitu.io/2017/7/14/55ad28a4ef1ad27509292a160f1ec50a)

下图是AndroidID的篡改及两个Level的反Hook

![AndroidID Hook与反Hook](https://user-gold-cdn.xitu.io/2017/7/14/ac4881c366103d40265a16eec35502c2)

对于其他的硬件信息如果想要反Hook，可能也要针对特定的服务做相应的处理。

# 参考文档

[用cache来区分模拟器和真机的idea](http://wps2015.org/drops/drops/%E5%88%A9%E7%94%A8cache%E7%89%B9%E6%80%A7%E6%A3%80%E6%B5%8BAndroid%E6%A8%A1%E6%8B%9F%E5%99%A8.html)      