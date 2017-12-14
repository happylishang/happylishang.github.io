Xposed常被Root设备用来刷单使用，其原理为何？简单概括就是一句话：**在Zygote进程中预加载并篡改Java类**，这样一来，该类就会会污染，而且是在所有进程中都是被污染状态，原因有两点

* 1：所有Android的应用进程都是由Zygote fork而来
* 2：Java的类加载机制是双亲委派模型

XposedBridge有一个私有的Native（JNI）方法hookMethodNative