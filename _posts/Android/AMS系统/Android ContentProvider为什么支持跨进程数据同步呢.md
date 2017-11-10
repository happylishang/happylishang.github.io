在Android中，文件的访问默认是没有进程间互斥同步机制的，需要自己实现，而Android提供的就是ContentProvider，同理对于数据的访问也是，毕竟，数据库其实就是表格文件。那么ContentProvider到底是如何支持跨进程访问的呢？ SP

ContentProvider：为存储和获取数据提供统一的接口，可以在不同的应用程序之间共享数据。 
Android内置的许多数据都是使用ContentProvider形式，供开发者调用的 (如视频，音频，图片，通讯录等)。 
1. 使用表的形式来组织数据 
- 无论数据来源是什么，ContentProvider都会认为是一种表。（把数据组织成表格） 



Android中最常用的进程通信工具就是Binder，ContentProvider就是基于它来实现的，只不过ContentProvider同时要接受AMS管理。

# 启动 为什么ContentProvider会自动启动，而不用别的接口访问触发呢？

	
	<provider  
	    android:name="com.test.MyProvider"  
	    android:authorities="com.test.provider.authority"  
	    android:multiprocess="true"  
	    android:process=":core" /> 
	     
我们知道默认不指定android:process的话，provider组件所在的进程名就是包名，multiprocess默认为false，则provider会随着应用启动的时候加载。如果对provider指定android:process和android:multiprocess，那么会产生如下几种情况：

1. android:process=":fore"，android:multiprocess="true"：provider不会随应用的启动而加载，当调用到provider的时候才会加载，加载时provider是在调用者的进程中初始化的。这时候可能定义provider的fore进程还没有启动。
2. android:process=":fore"（android:multiprocess默认情况下为"false"）：provider不会随应用的启动而加载，当调用到provider的时候才会加载，加载时provider是在“fore”进程中初始化的。
3. android:multiprocess="true"：provider会随着应用启动的时候加载，加载时provider是在应用默认主进程中初始化的。对于android:multiprocess=true，意味着provider可以多实例，那么由调用者在自己的进程空间实例化一个ContentProvider对象，此时定义ContentProvider的App可能并没有启动。
4. android:multiprocess="false"：provider会随着应用启动的时候加载，加载时provider是在应用默认主进程中初始化的。对于android:multiprocess=false（默认值），由系统把定义该ContentProvider的App启动起来(一个独立的Process)并实例化ContentProvider，这种ContentProvider只有一个实例，运行在自己App的Process中。所有调用者共享该ContentProvider实例，调用者与ContentProvider实例位于两个不同的Process。
**总之，android:multiprocess 应该理解为：是否允许在调用者的进程里实例化provider，而跟定义它的进程没有关系。**



这个属性用于设置Activity的实例能否被加载到与启动它的那个组件所在的进程中，如果设置为true，则可以，否则不可以。默认值是false。

通常，一个新的Activity实例会被加载到定义它的应用程序的进程中，以便应用程序的所有Activity都运行在同一个进程中。但是，如果这个属性被设置为true，那么这个Activity的实例就可以运行在多个进程中，允许系统在使用它们的进程中来创建实例（权限许可的情况下），这几乎是从来都不需要的事情。


If the app runs in multiple processes, this attribute determines whether multiple instances of the content provder are created. If true, each of the app's processes has its own content provider object. If false, the app's processes share only one content provider object. The default value is false.
Setting this flag to true may improve performance by reducing the overhead of interprocess communication, but it also increases the memory footprint of each process.


# 访问
# 增删查询 删除接口
# 只提供抽象接口，底层到底接的是谁，Sql还是 文件，并不关心



     mContentResolver = new ApplicationContentResolver(this, mainThread, user);


ApplicationContentResolver 如何获取ContentProvider

        @Override
        protected IContentProvider acquireUnstableProvider(Context c, String auth) {
            return mMainThread.acquireProvider(c,
                    ContentProvider.getAuthorityWithoutUserId(auth),
                    resolveUserIdFromAuthority(auth), false);
        }

acquireProvider
	
	   public final IContentProvider acquireProvider(
	            Context c, String auth, int userId, boolean stable) {
	        final IContentProvider provider = acquireExistingProvider(c, auth, userId, stable);
	        if (provider != null) {
	            return provider;
	        }
	
	        // There is a possible race here.  Another thread may try to acquire
	        // the same provider at the same time.  When this happens, we want to ensure
	        // that the first one wins.
	        // Note that we cannot hold the lock while acquiring and installing the
	        // provider since it might take a long time to run and it could also potentially
	        // be re-entrant in the case where the provider is in the same process.
	        IActivityManager.ContentProviderHolder holder = null;
	        try {
	            holder = ActivityManagerNative.getDefault().getContentProvider(
	                    getApplicationThread(), auth, userId, stable);
	        } catch (RemoteException ex) {
	        }
	        if (holder == null) {
	            Slog.e(TAG, "Failed to find provider info for " + auth);
	            return null;
	        }
	
	        // Install provider will increment the reference count for us, and break
	        // any ties in the race.
	        holder = installProvider(c, holder, holder.info,
	                true /*noisy*/, holder.noReleaseNeeded, stable);
	        return holder.provider;
	    }



插入是同步的，即使在另一个线程也会等待：

        ContentResolver contentResolver = AppProfile.getAppContext().getContentResolver();
        ContentValues contentValues = new ContentValues();
        contentValues.put(key, value);
        contentResolver.insert(FileContentProvider.CONTENT_URI, contentValues);
        contentResolver.notifyChange(FileContentProvider.CONTENT_URI, null);
