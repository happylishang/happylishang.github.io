在开发中，假如，A、B进程有部分信息需要同步，这个时候怎么处理呢？设想这么一个场景，有个业务复杂的Activity非常占用内存，并引发OOM，所以，想要把这个Activity放到单独进程，以保证OOM时主进程不崩溃。但是，两个整个APP有些信息需要保持同步，比如登陆信息等，无论哪个进程登陆或者修改了相应信息，都要同步到另一个进程中去，这个时候怎么做呢？

* 第一种：一个进程里面的时候，经常采用SharePreference来做，但是SharePreference不支持多进程，它基于单个文件的，默认是没有考虑同步互斥，而且，APP对SP对象做了缓存，不好互斥同步，虽然可以通过FileLock来实现互斥，但同步仍然是一个问题。
* 第二种：基于Binder通信实现Service完成跨进程数据的共享，能够保证单进程访问数据，不会有互斥问题，可是同步的事情仍然需要开发者手动处理。
* 第三种：基于Android提供的ContentProvider来实现，ContentProvider同样基于Binder，不存在进程间互斥问题，对于同步，也做了很好的封装，不需要开发者额外实现。

因此，在Android开发中，如果需要多进程同步互斥，ContentProvider是一个很好的选择，本文就来看看，它的这个技术究竟是怎么实现的。

# 概述

Content providers are one of the primary building blocks of Android applications, providing content to applications. They encapsulate data and provide it to applications through the single ContentResolver interface. A content provider is only required if you need to share data between multiple applications. For example, the contacts data is used by multiple applications and must be stored in a content provider. If you don't need to share data amongst multiple applications you can use a database directly via SQLiteDatabase.

ContentProvider为Android数据的存储和获取抽象了统一的接口，并支持在不同的应用程序之间共享数据，Android内置的许多数据都是使用ContentProvider形式供开发者调用的 (如视频，音频，图片，通讯录等)，它采用索引表格的形式来组织数据，无论数据来源是什么，ContentProvider都会认为是一种表，这一点从ContentProvider提供的抽象接口就能看出。


	class XXX ContentProvider extends ContentProvider{
	
	    @Override
	    public boolean onCreate() {
	        return false;
	    }
	
	    @Nullable
	    @Override
	    public Cursor query(@NonNull Uri uri, @Nullable String[] projection, @Nullable String selection, @Nullable String[] selectionArgs, @Nullable String sortOrder) {
	        return null;
	    }
	
	    @Nullable
	    @Override
	    public String getType(@NonNull Uri uri) {
	        return null;
	    }
	
	    @Nullable
	    @Override
	    public Uri insert(@NonNull Uri uri, @Nullable ContentValues values) {
	        return null;
	    }
	
	    @Override
	    public int delete(@NonNull Uri uri, @Nullable String selection, @Nullable String[] selectionArgs) {
	        return 0;
	    }
	
	    @Override
	    public int update(@NonNull Uri uri, @Nullable ContentValues values, @Nullable String selection, @Nullable String[] selectionArgs) {
	        return 0;
	    }
	}

可以看到每个ContentProvider都需要自己实现增、删、改、查的功能，因此，可以将ContentProvider看做Android提供一个抽象接口层，用于访问表格类的存储媒介，表格只是一个抽象，至于底层存储媒介到底如何组织，完全看用户实现，也就是说ContentProvider自身是没有数据更新及操作能力，它只是将这种操作进行了统一抽象。

![ContentProvider抽象接口.jpg](http://upload-images.jianshu.io/upload_images/1460468-b6b022d11c5e8a0a.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

了解了ContentProvider的概念及作用后，下面就从用法来看看ContentProvider是如何支持多进程同步通信的。


# ContentProvider代理的同步获取

多进程对于ContentProvider的访问请求最终都会按照队列进入ContentProvider进程，而在单进程中，ContentProvider对于数据的访问很容易做到多线程互斥，一个Sycronized关键字就能搞定，看一下基本用法：

        ContentResolver contentResolver = AppProfile.getAppContext().getContentResolver();
        ContentValues contentValues = new ContentValues();
        contentValues.put(key, value);
        contentResolver.insert(FileContentProvider.CONTENT_URI, contentValues);
        contentResolver.notifyChange(FileContentProvider.CONTENT_URI, null);
        
getContentResolver 其实获取的是一个ApplicationContentResolver实例，定义在ContextImpl中，只有在真正操作数据的时候才会去获取Provider， 详细看一下插入操作：

	    public final @Nullable Uri insert(@NonNull Uri url, @Nullable ContentValues values) {
	    <!--首先获取Provider代理-->
	        IContentProvider provider = acquireProvider(url);
	        try {
	    <!--利用IContentProvider代理插入数据-->
	            Uri createdRow = provider.insert(mPackageName, url, values);
	            return createdRow;
	        } 
	    }
        @Override
        protected IContentProvider acquireUnstableProvider(Context c, String auth) {
            return mMainThread.acquireProvider(c,
                    ContentProvider.getAuthorityWithoutUserId(auth),
                    resolveUserIdFromAuthority(auth), false);
        }

这里是一个典型的基于Binder通信的AIDL实现，IContentProvider的Proxy与Stub分别是ContentProviderProxy与ContentProvider的内部类  

	abstract public class ContentProviderNative extends Binder implements IContentProvider 
	
	class Transport extends ContentProviderNative，

首先看一下ActivityThread的acquireProvider，对于当前进程而言acquireProvider是一个同步的过程，如果ContentProvider所处的进程已经启动，那么acquireProvider可以直接获取服务代理，如果未启动，则等待ContentProvider进程启动，再获取代理。
	
	   public final IContentProvider acquireProvider(
	            Context c, String auth, int userId, boolean stable) {
	        final IContentProvider provider = acquireExistingProvider(c, auth, userId, stable);
	        if (provider != null) {
	            return provider;
	        }
	        IActivityManager.ContentProviderHolder holder = null;
	        try {
	        <!--关键点1 获取Provider，如果没有安装，则等待安装完毕-->
	            holder = ActivityManagerNative.getDefault().getContentProvider(
	                    getApplicationThread(), auth, userId, stable);
	        } catch (RemoteException ex) {
	        }
	        if (holder == null) {
	            return null;
	        }
	
			<!--关键点2 这里仅仅是增加计数 ，Provider到这里其实已经安装完毕-->
	        // Install provider will increment the reference count for us, and break
	        // any ties in the race.
	        holder = installProvider(c, holder, holder.info,
	                true /*noisy*/, holder.noReleaseNeeded, stable);
	        return holder.provider;
	    }

首先看一下关键点1，这里阻塞等待直到获取Provider代理，如果Provider未启动，则先启动，直接看一下ActivityManagerService（其实Android四大组件都归他管理），简单看一下获取流程（只描述个大概）：
	
	 private final ContentProviderHolder getContentProviderImpl(IApplicationThread caller,
	            String name, IBinder token, boolean stable, int userId) {
	        ContentProviderRecord cpr;
	        ContentProviderConnection conn = null;
	        ProviderInfo cpi = null;
		        synchronized(this) {
	            ...<!--关键点1  查看是否已有记录-->
	            // First check if this content provider has been published...
	            cpr = mProviderMap.getProviderByName(name, userId);
               ...
	            boolean providerRunning = cpr != null;
	            <!--如果有-->
	            if (providerRunning) {
	                cpi = cpr.info;
	                String msg;
					  <!--关键点2 是否允许调用进程自己实现ContentProvider-->
	                if (r != null && cpr.canRunHere(r)) {
	                    // This provider has been published or is in the process
	                    // of being published...  but it is also allowed to run
	                    // in the caller's process, so don't make a connection
	                    // and just let the caller instantiate its own instance.
	                    ContentProviderHolder holder = cpr.newHolder(null);
	                    // don't give caller the provider object, it needs
	                    // to make its own.
	                    holder.provider = null;
	                    return holder;
	                }
	
	                final long origId = Binder.clearCallingIdentity();
	
                   <!--关键点3 使用ContentProvider进程中的ContentProvider，仅仅增加引用计数-->		                // In this case the provider instance already exists, so we can
	                // return it right away.
	                conn = incProviderCountLocked(r, cpr, token, stable);
	                ...
	            }
	 
	            boolean singleton;
	            <!--如果provider未启动-->
	            if (!providerRunning) {
	                try {
	                    checkTime(startTime, "getContentProviderImpl: before resolveContentProvider");
	                    cpi = AppGlobals.getPackageManager().
	                        resolveContentProvider(name,
	                            STOCK_PM_FLAGS | PackageManager.GET_URI_PERMISSION_PATTERNS, userId);
	                } catch (RemoteException ex) {}
	                ...
	                ComponentName comp = new ComponentName(cpi.packageName, cpi.name);
	                cpr = mProviderMap.getProviderByClass(comp, userId);
	                ...
	                <!--查看目标进程是否启动-->
	                        ProcessRecord proc = getProcessRecordLocked(
	                                cpi.processName, cpr.appInfo.uid, false);
	                        if (proc != null && proc.thread != null) {
	                            if (!proc.pubProviders.containsKey(cpi.name)) {
	                                proc.pubProviders.put(cpi.name, cpr);
	                                try {
	                                    proc.thread.scheduleInstallProvider(cpi);
	                                } catch (RemoteException e) {
	                                }
	                            }
	                        } else {
	                        <!--如果未启动，启动进程，并安装-->
	                            proc = startProcessLocked(cpi.processName,
	                                    cpr.appInfo, false, 0, "content provider",
	                                    new ComponentName(cpi.applicationInfo.packageName,
	                                            cpi.name), false, false, false);
	                            checkTime(startTime, "getContentProviderImpl: after start process");
	                            if (proc == null) {
	                                return null;
	                            }
	                        }
	                        cpr.launchingApp = proc;
	                        mLaunchingProviders.add(cpr);
	                    } finally {
	                 ...
	       // 线程阻塞等待，直到provider启动 published，Wait for the provider to be published...
	        synchronized (cpr) {
	            while (cpr.provider == null) {
	
	                try {
	                    if (conn != null) {
	                        conn.waiting = true;
	                    }
	                    cpr.wait();
	                } catch (InterruptedException ex) {
	                } finally {
	                    if (conn != null) {
	                        conn.waiting = false;
	                    }
	                }
	            }
	        }
	        return cpr != null ? cpr.newHolder(conn) : null;
	    }
	    
ContentProvider的启动同Activity或者Service都是比较类似的，如果进程未启动，就去启动进程，在创建进程之后，调用ActivityThread的attach方法，通知AMS新的进程创建完毕,并初始化ProcessRecord，随后，查询所有和本进程相关的ContentProvider信息，并调用bindApplication方法，通知新进程安装并启动这些ContentProvider。ContentProvider有些不一样的就是：** ContentProvider调用端会一直阻塞，直到ContentProvider published才会继续执行**，这一点从下面可以看出：

	  synchronized (cpr) {
		            while (cpr.provider == null) {	    

其次，这里有个疑惑的地方，ContentProvider一般都是随着进程启动的，不过为什么会存在进程启动，但是ContentProvider未published的问题呢？不太理解，难道是中间可能存在什么同步问题吗？下面这部分代码完全看不出为什么存在：

	   if (proc != null && proc.thread != null) {
		                         <!--如果进程启动，发消息安装Providers-->
		                            if (!proc.pubProviders.containsKey(cpi.name)) {
		                                proc.pubProviders.put(cpi.name, cpr);
		                                try {
		                                    proc.thread.scheduleInstallProvider(cpi);
		                                } catch (RemoteException e) {
		                                }
		                            }
		                        } 


# ContentProvider数据的更新

通过ContentProvider对于数据的操作都是同步的，不过contentResolver.notifyChange通知是异步的

     contentResolver.insert(FileContentProvider.CONTENT_URI, contentValues);
     contentResolver.notifyChange(FileContentProvider.CONTENT_URI, null);

ContentProviderProxy会发消息给服务端，而服务端这里直接调用抽象的insert函数，如果需要insert操作是同步的，那么再实现ContentProvider的时候，就可以直接向数据库写数据，当然也可以实现Handler，自己做异步处理。

	abstract public class ContentProviderNative extends Binder implements IContentProvider {

	    @Override
	    public boolean onTransact(int code, Parcel data, Parcel reply, int flags)
	            throws RemoteException {
	            ...
		    case INSERT_TRANSACTION:
		    {
		        data.enforceInterface(IContentProvider.descriptor);
		        String callingPkg = data.readString();
		        Uri url = Uri.CREATOR.createFromParcel(data);
		        ContentValues values = ContentValues.CREATOR.createFromParcel(data);
		        Uri out = insert(callingPkg, url, values);
		        reply.writeNoException();
		        Uri.writeToParcel(reply, out);
		        return true;
		    }

这里有一点要注意，Binder框架默认是不支持Stub端同步的，也就是说，即时基于ContentProvider，如果需要对一个文件进行完全互斥访问，在单个进程内同样需要处理互斥操作，不过单进程互斥好处理，Sycronized关键字就可以了。

# ContentProvider数据变更通知

ContentProvider支持多进程访问，当一个进程操作ContentProvider变更数据之后，可能希望其他进程能收到通知，比如进程A往数据库插入了一条聊天信息，希望在进程B的UI中展现出来，这个时候就需要一个通知机制，Android也是提供了支持，不过它是一个通用的数据变更同步通知：基于ContentService服务：

    <!--1 注册-->
    public static void registerObserver(ContentObserver contentObserver) {
        ContentResolver contentResolver = AppProfile.getAppContext().getContentResolver();
        contentResolver.registerContentObserver(FileContentProvider.CONTENT_URI, true, contentObserver);
    }
 
     <!--2 通知-->
     contentResolver.notifyChange(FileContentProvider.CONTENT_URI, null);

上面的两个可能在统一进程，也可能在不同进程，

    public final void registerContentObserver(Uri uri, boolean notifyForDescendents,
            ContentObserver observer, int userHandle) {
        try {
            getContentService().registerContentObserver(uri, notifyForDescendents,
                    observer.getContentObserver(), userHandle);
        } catch (RemoteException e) {
        }
    }
    
其实这里跟ContentProvider的关系已经不是很大，这里牵扯到另一个服务：ContentService，它是Android平台中数据更新通知的执行者，由SystemServer进程启动，所有APP都能调用它发送数据变动通知，其实就是一个观察者模式，牵扯到另一个服务，不过多讲解。

# android:multiprocess在ContentProvider中的作用

	     
默认情况下是不指定android:process跟multiprocess的，它们的值默认为false，会随着应用启动的时候加载，如果对provider指定android:process和android:multiprocess，表现就会不一通了，如果设置android:process，那ContentProvider就不会随着应用启动，如果设置了android:multiprocess，则可能存在多个ContentProvider实例。

>If the app runs in multiple processes, this attribute determines whether multiple instances of the content provder are created. If true, each of the app's processes has its own content provider object. If false, the app's processes share only one content provider object. The default value is false.
Setting this flag to true may improve performance by reducing the overhead of interprocess communication, but it also increases the memory footprint of each process.

android:multiprocess的作用是：是否允许在调用者的进程里实例化provider，如果android:multiprocess=false，则系统中只会存在一个provider实例，否则，可以存在多个，多个的话，可能会提高性能，因为它避免了跨进程通信，毕竟，对象就在自己的进程空间，可以直接访问，但是，这会增加系统负担，另外，对于单进程能够保证的互斥问题，也会无效，如果APP需要数据更新，还是保持不开启的好。

# 总结

* ContentProvider只是Android为了跨进程共享数据提供的一种机制，
* 本身基于Binder实现，
* 在操作数据上只是一种抽象，具体要自己实现

# 参考文档

[ContentProvider的启动流程分析](http://blog.csdn.net/zhenjie_chang/article/details/62889188)