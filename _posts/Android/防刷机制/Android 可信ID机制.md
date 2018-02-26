# 背景

APP开发中常需要获取Android的Deviceid，以应对防刷，目前常用的几个设备识别码主要有IMEI或者MEID，这两者也是常说的DeviceId，不过IMEI在Android6.0之后需要权限才能获取，而且，在Java层IMEI很容易被Hook，并不靠谱，同样有问题的还包括MAC地址、蓝牙地址，序列号等，都可能被篡改，为了解决这个问题，从以下两方面入手，一方面，可以采用相对底层的方法获取“准确”的特征值，另一方面，可以借助后台大数据做匹配去重，进行设备的甄别，并为每个设备打上一个标识，这就是可信ID的背景

# Android可信ID整体流程

客户端拿真实度较高的信息交给服务端，服务端返回唯一设备识别符（也就是可信ID-TrustId），以标识当前设备，如果手机存在信息篡改，服务端需要识别作假信息，并映射到同一个设备识别符（TrustId），之后，客户端往APP服务发请求的时候，都要携带TrustId，如果业务后台需要甄别，可拿着这个TrustId自行去可信ID服务查询：

![可信ID流程.jpg](http://upload-images.jianshu.io/upload_images/1460468-c7fe93188ac1a271.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


# Android客户端信息搜集及DeviceID生成策略  

客户端需要搜集设备的特征值，主要包括IMEI、 MAC、 序列号、 AndroidId、内存、手机厂商、型号等信息，同时要为后台生成一个DeviceId，作为后台数据库的主键值，生成规则如下

* 如果IMEI+MAC都能获取到，并且IMEI不是000000000000格式，MAC地址不是02：00：00：00：00：00格式，则利用IMEI+MAC的MD5作为DeviceID
* 如果不能同时拿到两个，但IMEI或者MAC地址有效，则利用其中有效的一个生成DeviceID
* 如果IMEI与MAC都为空，则取序列号的MD5作为DeviceID
* 如果序列号也是空则取AndroidID作为DeviceID（Android设备都会有）
* 以上都无效UUID随机生成

![客户端DeviceId生成流程图](http://upload-images.jianshu.io/upload_images/1460468-192120fa68a8bd82.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

生成的DeviceId作为服务端的一个主键，由于旧版本已经有了DeviceId，并存到文件中，目前不能直接删掉与替换。

# 服务器生成TrustID规则

服务端以客户端传来的DEVICE_ID作为主键进行数据入库，其中，DEVICE_ID与TrustId是多对一的关系，也就是说，不同的DEVICE_ID可能是因为客户端伪造信息生成的，但是，如果其他特征信息被后台甄别、并匹配成功，则可以映射为同一个TrustId，当然，这里有一套匹配算法。目前后台采用的是积分制，TrustID后台为Android特征信息加上权值，如果一台DEVICE_ID不同的设备到来，后台会匹配这些特征，并且累计分值，当分值超多我们的阈值，就认为匹配成功，经过两个版本的测试，目前线上的权值及积分策略如下，阈值设定为10：

*     DEVICE_ID   5分   
*     ANDROID_ID  5分   
*     MAC_ADDRESS 5分 （非 02：00：00：00：00：00）
*     IMEI         4分  （非 000000000000000）
*     SERIAL       8分 （经过统计重复率很低，所以分值较高）
*     MANUFACTURER  1分   
*     BRAND   1分 
*     MODEL   1分 
*     MEM_INFO   2分 
*     IMSI       2分 

举个例子，如下图，虽然A B两台设备DEVICE_ID不同，但是IMEI跟序列号都相同，累计超过了10分，那么就看做同一台设备，映射到同一个TrustId：

![Android可信ID生成及映射](http://upload-images.jianshu.io/upload_images/1460468-f36cdc9e5e2a8ef8.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


# 客户端TrustID更新时机

每次App启动的时候都去请求，并更新本地TrustID，在下一次重启之前，都是用该TrustID。

# 客户端获取“准确”Android特征属性的方法

这里的“准确”其实是防篡改的意思，虽然系统提供了IMEI、MAC地址、序列号、AndroidID等API，但是由于都是Java层方法，很容易被Hook，尤其是有些专门刷单的，在手机Root之后，利用Xposed框架里的一些插件很容易将获取的数据给篡改。举个最简单的IMEI的获取，常用的获取方式如下：

	TelephonyManager telephonyManager = ((TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE));
    return telephonyManager.getDeviceId()
        
假如，Root用户利用Xposed Hook了TelephonyManager类的getDeviceId()方法，在Xposed插件中，在afterHookedMethod方法中，将DeviceId设置为随机数，这样每次获取的DeviceId都是不同的，

	public class XposedModule implements IXposedHookLoadPackage {
	
			try {
				findAndHookMethod(TelephonyManager.class.getName(), lpparam.classLoader, "getDeviceId", new XC_MethodHook() {
								@Override
							protected void afterHookedMethod(MethodHookParam param) throws Throwable {
								super.afterHookedMethod(param);
									param.setResult("" + System.currentTimeMillis());
							}
						});
			} catch (Exception e1) {
			}catch (Error e) {
			} }

所以为了获取相对准确的设备信息，我们采取相应的应对措施，比如：

* 可以采用一些系统隐藏的接口来获取设备信息，隐藏的接口不太容易被篡改，因为可能或导致整个系统运行不正常
* 可以自己通过Binder通信的方式向服务请求信息，比如IMEI号，就是想Phone服务发送请求获取的，当然如果Phone服务中的Java类被Hook，那么这种方式也是获取不到正确的信息的
* 可以采用Native方式获取设备信息，这种哦方式可以有效的避免被Xposed Hook，不过其实仍然可以被adbi在本地层Hook。

从源码层面看一下看一下如何获取getDeviceId，源码如下

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

从这里知道，如果getDeviceId被Hook但是getITelephony没被Hook，我们就可以直接通过反射获取TelephonyManager的getITelephony方法，进一步通过ITelephony的getDeviceId获取DeviceId，不过这个方法跟ROM版本有关系，比较早的版本压根没有getITelephony方法，早期可能通过IPhoneSubInfo的getDeviceId来获取，以上两种方式都很容被Hook，既然可以Hook getDeviceId方法，同理也可以Hook getITelephony方法，这个层次的反Hook并没有多大意义。因此，可以稍微深入一下，ITelephony.Stub.asInterface，这是一个很明显的Binder通信的方式，那么不让我们自己获取Binder代理，进而利用Binder通信的方式向Phone服务发送请求，获取设备DeviceId，Phone服务是利用aidl文件生成的Proxy与Stub，可以基于这个来实现我们的代码，Binder通信比较重要的几点：InterfaceDescriptor+TransactionId+参数，获取DeviceId的几乎不需要什么参数（低版本可能需要）。具体做法是：

* 直接通过ServiceManager的getService方法获取我们需要的Binder服务代理，这里其实就是phone服务
* 利用com.android.internal.telephony.ITelephony$Stub的asInterface方法获取Proxy对象
* 利用反射获取getDeviceId的Transaction id
* 利用Proxy向Phone服务发送请求，获取DeviceId。

具体实现如下，这种做法可以应对代理方的Hook。

	 public static int getTransactionId(Object proxy,
	                                        String name) throws RemoteException, NoSuchFieldException, IllegalAccessException {
	        int transactionId = 0;
	        Class outclass = proxy.getClass().getEnclosingClass();
	        Field idField = outclass.getDeclaredField(name);
	        idField.setAccessible(true);
	        transactionId = (int) idField.get(proxy);
	        return transactionId;
	    }

    //根据方法名，反射获得方法transactionId
    public static String getInterfaceDescriptor(Object proxy) throws NoSuchMethodException, InvocationTargetException, IllegalAccessException {
        Method getInterfaceDescriptor = proxy.getClass().getDeclaredMethod("getInterfaceDescriptor");
        return (String) getInterfaceDescriptor.invoke(proxy);
    }
    

	 static String getDeviceIdLevel2(Context context) {
	
	        String deviceId = "";
	        try {
	            Class ServiceManager = Class.forName("android.os.ServiceManager");
	            Method getService = ServiceManager.getDeclaredMethod("getService", String.class);
	            getService.setAccessible(true);
	            IBinder binder = (IBinder) getService.invoke(null, Context.TELEPHONY_SERVICE);
	            Class Stub = Class.forName("com.android.internal.telephony.ITelephony$Stub");
	            Method asInterface = Stub.getDeclaredMethod("asInterface", IBinder.class);
	            asInterface.setAccessible(true);
	            Object binderProxy = asInterface.invoke(null, binder);
	            try {
	                Method getDeviceId = binderProxy.getClass().getDeclaredMethod("getDeviceId", String.class);
	                if (getDeviceId != null) {
	                    deviceId = binderGetHardwareInfo(context.getPackageName(),
	                            binder, getInterfaceDescriptor(binderProxy),
	                            getTransactionId(binderProxy, "TRANSACTION_getDeviceId"));
	                }
	            } catch (Exception e) {
	            }
	            Method getDeviceId = binderProxy.getClass().getDeclaredMethod("getDeviceId");
	            if (getDeviceId != null) {
	                deviceId = binderGetHardwareInfo("",
	                        binder, BinderUtil.getInterfaceDescriptor(binderProxy),
	                        BinderUtil.getTransactionId(binderProxy, "TRANSACTION_getDeviceId"));
	            }
	        } catch (Exception e) {
	        }
	        return deviceId;
	    }
	
	    private static String binderGetHardwareInfo(String callingPackage,
	                                                IBinder remote,
	                                                String DESCRIPTOR,
	                                                int tid) throws RemoteException {
	
	        android.os.Parcel _data = android.os.Parcel.obtain();
	        android.os.Parcel _reply = android.os.Parcel.obtain();
	        String _result;
	        try {
	            _data.writeInterfaceToken(DESCRIPTOR);
	            if (!TextUtils.isEmpty(callingPackage)) {
	                _data.writeString(callingPackage);
	            }
	            remote.transact(tid, _data, _reply, 0);
	            _reply.readException();
	            _result = _reply.readString();
	        } finally {
	            _reply.recycle();
	            _data.recycle();
	        }
	        return _result;
	    }
    
## 利用Native方法反Xposed Hook

此外，有很多系统参数我们是通过Build类来获取的，比如序列号、手机硬件信息等，例如获取序列号，在Java层直接利用Build的feild获取即可

    public static final String SERIAL = getString("ro.serialno");
    
    private static String getString(String property) {
        return SystemProperties.get(property, UNKNOWN);
    }
       
不过SystemProperties的get方法很容被Hook，被Hook之后序列号就可以随便更改，不过好在SystemProperties类是通过native方法来获取硬件信息的，我们可以自己编写native代码来获取硬件参数，这样就避免被Java Hook，

    public static String get(String key) {
        if (key.length() > PROP_NAME_MAX) {
            throw new IllegalArgumentException("key.length > " + PROP_NAME_MAX);
        }
        return native_get(key);
    }

来看一下native源码

	static jstring SystemProperties_getSS(JNIEnv *env, jobject clazz,
	                                      jstring keyJ, jstring defJ)
	{
	    int len;
	    const char* key;
	    char buf[PROPERTY_VALUE_MAX];
	    jstring rvJ = NULL;
	
	    if (keyJ == NULL) {
	        jniThrowNullPointerException(env, "key must not be null.");
	        goto error;
	    }
	    key = env->GetStringUTFChars(keyJ, NULL);
	    len = property_get(key, buf, "");
	    if ((len <= 0) && (defJ != NULL)) {
	        rvJ = defJ;
	    } else if (len >= 0) {
	        rvJ = env->NewStringUTF(buf);
	    } else {
	        rvJ = env->NewStringUTF("");
	    }
	
	    env->ReleaseStringUTFChars(keyJ, key);
	
	error:
	    return rvJ;
	}

参考这部分源码，自己实现.so库即可，就可以避免被Java层Hook，这样获取的信息相对准确性较高。

