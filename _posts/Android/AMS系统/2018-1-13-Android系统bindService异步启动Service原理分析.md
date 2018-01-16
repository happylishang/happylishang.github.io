---
layout: default
title: Android系统bindService异步启动Service原理分析
categories: [android]

---

Android中bindService是一个异步的过程，什么意思呢？使用bindService无非是想获得一个Binder服务的Proxy，但这个代理**获取到**的时机并非由bindService发起端控制，而是由Service端来控制，也就是说bindService之后，APP端并不会立刻获得Proxy，而是要等待Service通知APP端，具体流程可简化如下：

* APP端先通过bindService去AMS登记，说明自己需要绑定这样一个服务，并留下派送地址
* APP回来，继续做其他事情，可以看做是非阻塞的
* AMS通知Service端启动这个服务
* Service启动，并通知AMS启动完毕
* AMS跟住之前APP端留下的地址通知APP端，并将Proxy代理传递给APP端 

通过代码来看更直接

		void test(){
	        bindService(intent, new ServiceConnection() {
	            @Override
	            public void onServiceConnected(ComponentName componentName, IBinder iBinder) {
	               iMyAidlInterface = IMyAidlInterface.Stub.asInterface(iBinder);
	               Log.v(TAG, "onServiceConnected..." );
	            }
	           @Override
	            public void onServiceDisconnected(ComponentName componentName) {
	           }
	        }, Context.BIND_AUTO_CREATE);
	        Log.v(TAG, "end..." );
        }
 
bindService的过程中，上面代码的Log应该是怎么样的呢？如果bindService是一个同步过程，那么Log应该如下：

	TAG  onServiceConnected ...
	TAG  end ...

但是由于是个异步过程，真实的Log如下
	
	TAG  end ...    
	TAG  onServiceConnected ...

也就是说bindService不会阻塞等待APP端获取Proxy，而是直接返回，这些都可以从源码获得支持，略过，直接去ActivityManagerNative去看

    public int bindService(IApplicationThread caller, IBinder token,
            Intent service, String resolvedType, IServiceConnection connection,
            int flags, int userId) throws RemoteException {
        Parcel data = Parcel.obtain();
        Parcel reply = Parcel.obtain();
        data.writeInterfaceToken(IActivityManager.descriptor);
        data.writeStrongBinder(caller != null ? caller.asBinder() : null);
        data.writeStrongBinder(token);
        service.writeToParcel(data, 0);
        data.writeString(resolvedType);
        data.writeStrongBinder(connection.asBinder());
        data.writeInt(flags);
        data.writeInt(userId);
        <!--阻塞等待-->
        mRemote.transact(BIND_SERVICE_TRANSACTION, data, reply, 0);
        reply.readException();
        int res = reply.readInt();
        data.recycle();
        reply.recycle();
        return res;
    }
    
mRemote.transact(BIND_SERVICE_TRANSACTION, data, reply, 0)确实会让APP端调用线程阻塞，等待AMS执行BIND_SERVICE_TRANSACTION请求，不过AMS在执行这个请求的时候并非是唤醒Service才返回，它返回的时机更早，接着看ActivityManagerService，

    public int bindService(IApplicationThread caller, IBinder token,
            Intent service, String resolvedType,
            IServiceConnection connection, int flags, int userId) {
        ...
        synchronized(this) {
            return mServices.bindServiceLocked(caller, token, service, resolvedType,
                    connection, flags, userId);
        }
    }
 ActivityManagerService直接调用ActiveServices的函数bindServiceLocked，请求绑定Service，到这里APP端线程依旧阻塞，等待AMS端返回，假定Service所处的进程已经启动但是Service没有启动，这时ActiveServices会进一步调用bindServiceLocked->realStartServiceLocked来启动Service，有趣的就在这里：
 
	 private final void realStartServiceLocked(ServiceRecord r,
	            ProcessRecord app) throws RemoteException {
	        ...
	        <!--请求Service端启动Service-->
	            app.thread.scheduleCreateService(r, r.serviceInfo,
	                    mAm.compatibilityInfoForPackageLocked(r.serviceInfo.applicationInfo));
	        ...
	        <!--请求绑定Service-->
	        requestServiceBindingsLocked(r);
	        
app.thread.scheduleCreateService也是一个Binder通信过程，他其实是请求ActivityThread中的ApplicationThread服务，当然这个时候AMS端也是阻塞的，

    // 插入消息，等待主线程执行
    public final void scheduleCreateService(IBinder token,
            ServiceInfo info, CompatibilityInfo compatInfo) {
        CreateServiceData s = new CreateServiceData();
        s.token = token;
        s.info = info;
        s.compatInfo = compatInfo;
        <!--向Loop的MessagerQueue插入一条消息就返回-->
        queueOrSendMessage(H.CREATE_SERVICE, s);
    }

不过，这个请求直接向Service端的ActivityThread线程中直接插入一个消息就返回了，而并未等到该请求执行，因为AMS使用的非常频繁，不可能老等待客户端完成一些任务，所以AMS端向客户端发送完命令就直接返回，这个时候其实Service还没有被创建，也就是这个请求只是完成了一半，onServiceConnected也并不会执行，onServiceConnected什么时候执行呢？app.thread.scheduleCreateService向APP端插入第一条消息，是用来创建Service的， requestServiceBindingsLocked其实就是第二条消息，用来处理绑定的

	 private final boolean requestServiceBindingLocked(ServiceRecord r,
	            IntentBindRecord i, boolean rebind) {
             		...
               <!-- 第二个消息,请求处理绑定-->
                r.app.thread.scheduleBindService(r, i.intent.getIntent(), rebind);

第二条消息是处理一些绑定需求，Android的Hanlder消息处理机制保证了第二条消息一定是在第一条消息之后执行，

     public final void scheduleBindService(IBinder token, Intent intent,
            boolean rebind) {
        BindServiceData s = new BindServiceData();
        s.token = token;
        s.intent = intent;
        s.rebind = rebind;
        queueOrSendMessage(H.BIND_SERVICE, s);
    }	   
 
 以上两条消息插入后，AMS端被唤醒，进而重新唤醒之前阻塞的bindService端，而这个时候，Service并不一定被创建，所以说这是个未知的异步过程，Service端处理第一条消息的时会创建Service，
 
     private void handleCreateService(CreateServiceData data) {
        ...
        LoadedApk packageInfo = getPackageInfoNoCheck(
                data.info.applicationInfo, data.compatInfo);
        Service service = null;
        try {
            java.lang.ClassLoader cl = packageInfo.getClassLoader();
            service = (Service) cl.loadClass(data.info.name).newInstance();
       ...

执行第二条消息的时候， 会向AMS请求publishService，其实就是告诉AMS，服务启动完毕，可以向之前请求APP端派发代理了。
 
     private void handleBindService(BindServiceData data) {
        Service s = mServices.get(data.token);
        if (s != null) {
           try {
            data.intent.setExtrasClassLoader(s.getClassLoader());
            try {
                if (!data.rebind) {
                    IBinder binder = s.onBind(data.intent);
                    ActivityManagerNative.getDefault().publishService(
                            data.token, data.intent, binder);
                ...                       
 
AMS端收到publishService消息之后，才会向APP端发送通知，进而通过Binder回调APP端onServiceConnected函数，同时传递Proxy Binder服务代理

	void publishServiceLocked(ServiceRecord r, Intent intent, IBinder service) {
        ...
         try {
        <!--通过binder 回到APP端的onServiceConnected--> 
            c.conn.connected(r.name, service);
        } catch (Exception e) {
 
到这里，onServiceConnected才会被回调，不过，**至于Service端那两条消息什么时候执行，谁也不能保证**，也许因为特殊原因，那两条消息永远不被执行，那onServiceConnected也就不会被回调，但是这不会影响AMS与APP端处理其他问题，因为这些消息是否被执行已经不能阻塞他们两个了，简单流程如下：
                                           
![bindService的异步流程](http://upload-images.jianshu.io/upload_images/1460468-83703abbcda65cf6.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
