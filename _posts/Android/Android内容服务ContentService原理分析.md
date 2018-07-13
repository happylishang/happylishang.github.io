ContentService可以看做Android中一个系统级别的消息中心，可以说搭建了一个系统级的观察者模型，APP可以向消息中心注册观察者，选择订阅自己关心的消息，也可以通过消息中心发送信息，通知其他进程，简单模型如下：

![“ContentService简单框架”.png](https://upload-images.jianshu.io/upload_images/1460468-f6b66069f275eec3.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

ContentService服务伴随系统启动，本身是一个Binder系统服务，运行在SystemServer进程。作为系统服务，最好能保持高效运行，如果不是非常紧急，APP可以选择异步处理消息，提高ContentService的运行效率，通过APP设置，可以让ContentService在分发消息的时候，仅仅插入目标进程的自己的Queue队列。下面简单分析一下整体的架构，主要从一下几个方面了解下运行流程：

* ContentService启动跟实质
* 注册观察者
* 管理观察者
* 消息分发

# ContentService启动跟实质

ContentService服务伴随系统启动，更准确的说是伴随SystemServer进程启动，其入口函数如下：
    
    public static ContentService main(Context context, boolean factoryTest) {
    	 <!--新建Binder服务实体-->
        ContentService service = new ContentService(context, factoryTest);
        <!--添加到ServiceManager中-->
        ServiceManager.addService(ContentResolver.CONTENT_SERVICE_NAME, service);
        return service;
    }

同AMS、WMS等系统服务类似，ContentService也是一个Binder服务实体，而且受ServiceManager管理，需要注册ServiceManager中，方便APP将来获取该服务的代理。ContentService是一个Binder服务实体，具体实现如下：
    

        <!--关键点1-->
	 public final class ContentService extends IContentService.Stub {
	    private static final String TAG = "ContentService";
	    private Context mContext;
	    private boolean mFactoryTest;
	    private final ObserverNode mRootNode = new ObserverNode("");
	    private SyncManager mSyncManager = null;
	    private final Object mSyncManagerLock = new Object();
		 。。。

IContentService.Stub由IContentService.aidl文件生成，IContentService.aidl文件中定义了ContentService能提供的基本服务，比如注册/注销观察者、通知观察者等，如下：

	interface IContentService {
		<!--注销一个观察者-->
		 void unregisterContentObserver(IContentObserver observer);
		 <!--注册一个观察者-->
	    void registerContentObserver(in Uri uri, boolean notifyForDescendants,
	            IContentObserver observer, int userHandle);
	    <!--通知观察者-->
	    void notifyChange(in Uri uri, IContentObserver observer,
	            boolean observerWantsSelfNotifications, boolean syncToNetwork,
	            int userHandle);
	    ...
	}

虽然从使用上来说，ContentService跟ContentProvider关系紧密，但是理论上讲，这是完全独立的两套东西，ContentService是一个独立的消息分发模型，可以完全独立于ContentProvider使用（总觉的这种设计是不是有些问题），看一下基本用法：

>1、注册一个观察者：

    public static void registerObserver(Context context,ContentObserver contentObserver) {
        ContentResolver contentResolver = context.getContentResolver();
        contentResolver.registerContentObserver(Uri.parse("content://"+"test"), true, contentObserver);
    }
        
>2、通知观察者
 
     public static void notity(Context context) {
        ContentResolver contentResolver = context.getContentResolver();
        contentResolver.notifyChange(Uri.parse("content://"+"test"),null);
    }
    
可以看到，期间只是借用了ContentResolver，但是并没有牵扯到任何ContentProvider，也就是说，ContentService其实主要是为了提供了一个系统级的消息中心，下面简单看一下注册跟通知流程


# 注册观察者流程

App一般都是借助ContentResolver来注册Content观察者，ContextResoler其实是Context的一个成员变量，本身是一个ApplicationContentResolver对象，它是ContentResolver的子类，

	    private ContextImpl(ContextImpl container, ActivityThread mainThread,
	            LoadedApk packageInfo, IBinder activityToken, UserHandle user, boolean restricted,
	            Display display, Configuration overrideConfiguration, int createDisplayWithId) {
				 ...
	   			 mContentResolver = new ApplicationContentResolver(this, mainThread, user);
	   			 ...

通过ContentResolver注册ContentObserver代码如下：

	    public final void registerContentObserver(Uri uri, boolean notifyForDescendents,
	            ContentObserver observer, int userHandle) {
	        try {

		<!--获取ContentService，并注册-->
	            getContentService().registerContentObserver(uri, notifyForDescendents,
	                    observer.getContentObserver(), userHandle);
	        } catch (RemoteException e) {
	        }
	    }
    
可以看到，注册的过程首先是获取ContentService服务代理，然后通过这个代理像ContentService注册观察者，典型的Binder服务通信模型，获取服务的实现如下，
    
    /** @hide */
    public static final String CONTENT_SERVICE_NAME = "content";
    /** @hide */
    public static IContentService getContentService() {
        if (sContentService != null) {
            return sContentService;
        }
        IBinder b = ServiceManager.getService(CONTENT_SERVICE_NAME);
        sContentService = IContentService.Stub.asInterface(b);
        return sContentService;
    }
    
其实就是通过系统服务的名称，向ServiceManager查询并获取服务代理，请求成功后，便可以通过代理发送请求，这里请求的任务是注册，这里有一点要注意，那就是**在注册的时候，要同时打通ContentService向APP发送消息的链路**，这个链路其实就是另一个Binder通信路线，具体做法就是将ContentObserver封装成一个Binder服务实体注册到ContentService中，注册成功后，ContentService就会握有ContentObserver的代理，将来需要通知APP端的时候，就可以通过该代理发送通知，双C/S模型在Android框架中非常常见。具体代码是，通过ContentObserver获取一个IContentObserver对象，APP端将该对象通过binder传递到ContentService服务，如此ContentService便能通过Binder向APP端发送通知
 
     public IContentObserver getContentObserver() {
        synchronized (mLock) {
            if (mTransport == null) {
                mTransport = new Transport(this);
            }
            return mTransport;
        }
    }

mTransport本质是一个Binder服务实体，同时握有ContentObserver的强引用，将来通知到达的时候，便能通过ContentObserver分发通知
    
     private static final class Transport extends IContentObserver.Stub {
        private ContentObserver mContentObserver;

        public Transport(ContentObserver contentObserver) {
            mContentObserver = contentObserver;
        }

        @Override
        public void onChange(boolean selfChange, Uri uri, int userId) {
            ContentObserver contentObserver = mContentObserver;
            if (contentObserver != null) {

			<!--通过 contentObserver发送回调通知-->
                contentObserver.dispatchChange(selfChange, uri, userId);
            }
        }

        public void releaseContentObserver() {
            mContentObserver = null;
        }
    }
    
Transport本身是一个Binder实体对象，被注册到ContentService中，ContentService会维护一个Transport代理的集合，通过代理，可以通知不同的进程，继续看register流程，registerContentObserver通过binder通信最终会调用都ContentService的registerContentObserver函数：

    @Override
    public void registerContentObserver(Uri uri, boolean notifyForDescendants,
            IContentObserver observer, int userHandle) {
        <!--权限检查-->
        if (callingUserHandle != userHandle &&
                mContext.checkUriPermission(uri, pid, uid, Intent.FLAG_GRANT_READ_URI_PERMISSION)
                        != PackageManager.PERMISSION_GRANTED) {
            enforceCrossUserPermission(userHandle,
                    "no permission to observe other users' provider view");
        }
        ...
        <!--2 添加到监听队列-->
        synchronized (mRootNode) {
            mRootNode.addObserverLocked(uri, observer, notifyForDescendants, mRootNode,
                    uid, pid, userHandle);
        }
    }

这里主要看下点2：**监听对象的添加**，ContentService对象内部维护了一个树，用于管理监听对象，主要是根据Uri的路径进行分组，既方便管理，同时又提高查找及插入效率，每个Uri路径对象对应一个节点，也就是一个ObserverNode对象，每个节点中维护一个监听List，而ContentService持有RootNode根对象，

	 private final ObserverNode mRootNode = new ObserverNode("");
	 
![Content树.png](https://upload-images.jianshu.io/upload_images/1460468-3f1823c5571efc4e.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

每个ObserverNode维护了一个ObserverEntry队列，ObserverEntry与ContentObserver一一对应，一个Uri对应一个ObserverNode，一个ObserverNode下可以有多个ContentObserver，也就是会多个ObserverEntry，每个ObserverEntry还有一些其他辅助信息，比如要跟Uri形成键值对，ObserverEntry还将自己设置成了Binder讣告的接受者，一旦APP端进程结束，可以通过Binder讣告机制让ContentService端收到通知，并做一些清理工作，具体实现如下：

     public static final class ObserverNode {
        private class ObserverEntry implements IBinder.DeathRecipient {
            public final IContentObserver observer;
            public final int uid;
            public final int pid;
            public final boolean notifyForDescendants;
            private final int userHandle;
            private final Object observersLock;

            public ObserverEntry(IContentObserver o, boolean n, Object observersLock,
                    int _uid, int _pid, int _userHandle) {
                this.observersLock = observersLock;
                observer = o;
                uid = _uid;
                pid = _pid;
                userHandle = _userHandle;
                notifyForDescendants = n;
                try {
                    observer.asBinder().linkToDeath(this, 0);
                } catch (RemoteException e) {
                    binderDied();
                }
            }
          <!--做一些清理工作，删除observer-->
            public void binderDied() {
                synchronized (observersLock) {
                    removeObserverLocked(observer);
                }
            }
			 。。。
        }

        public static final int INSERT_TYPE = 0;
        public static final int UPDATE_TYPE = 1;
        public static final int DELETE_TYPE = 2;

        private String mName;
        private ArrayList<ObserverNode> mChildren = new ArrayList<ObserverNode>();
        <!--维护自己node的回调队列-->
        private ArrayList<ObserverEntry> mObservers = new ArrayList<ObserverEntry>();	 	. ..
                
继续看看下Observer的add流程，ObserverNode 的addObserverLocked函数被外部调用（被rootnode）的时候，一般传递的index是0，自己递归调用的时候，才不是0，**其实添加Observer的过程是一个递归的过程，首先通过Uri路径，递归找到对应的ObserverNode，然后像ObserverNode的监听队列中添加Observer**。

        private void addObserverLocked(Uri uri, int index, IContentObserver observer,
                boolean notifyForDescendants, Object observersLock,
                int uid, int pid, int userHandle) {
                
            // If this is the leaf node add the observer
            <!--已经找到叶子节点，那么可以直接在node中插入ObserverEntry->
            if (index == countUriSegments(uri)) {
                mObservers.add(new ObserverEntry(observer, notifyForDescendants, observersLock,
                        uid, pid, userHandle));
                return;
            }

            // Look to see if the proper child already exists
            <!--一层层往下剥离-->
            String segment = getUriSegment(uri, index);
			  ...
            int N = mChildren.size();
            <!--递归查找-->
            for (int i = 0; i < N; i++) {
                ObserverNode node = mChildren.get(i);
                if (node.mName.equals(segment)) {
                    node.addObserverLocked(uri, index + 1, observer, notifyForDescendants,
                            observersLock, uid, pid, userHandle);
                    return;
                }
            }

            // No child found, create one
            <!--找不到，就新建，并插入-->
            ObserverNode node = new ObserverNode(segment);
            mChildren.add(node);
            node.addObserverLocked(uri, index + 1, observer, notifyForDescendants,
                    observersLock, uid, pid, userHandle);
        }
  
比如：要查询content://A/B/C对应的ObserverNode，首先会找到Authority，找到A对应的ObserverNode，之后在A的children中查找Path=B的Node，然后在B的Children中查找Path=C的Node，找到该Node之后，往这个node的ObserverEntry列表中添加一个对象，到这里就注册就完成了。
   
#  通知流程    

前文已经说过，ContentService可以看做是通知的中转站，进程A想要通知其他注册了某个Uri的进程，必须首先向ContentService这个消息分发中心发送消息，再由ContentService通知其他进程中的观察者，简化模型如下图：

![ContentService框架.png](https://upload-images.jianshu.io/upload_images/1460468-846b5aa82fbb1761.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

下面简单跟踪下通知流程，入口函数如下

     public static void notity(Context context) {
        ContentResolver contentResolver = context.getContentResolver();
        contentResolver.notifyChange(Uri.parse("content://"+"test"),null);
    }
    
ContentResolver的notifyChange会进一步通过Binder，请求ContentService发送通知，

    public void notifyChange(Uri uri, ContentObserver observer, boolean syncToNetwork,
            int userHandle) {
        try {
            getContentService().notifyChange(
                    uri, observer == null ? null : observer.getContentObserver(),
                    observer != null && observer.deliverSelfNotifications(), syncToNetwork,
                    userHandle);
        } catch (RemoteException e) {
        }
    }

那么ContentService无非就是通过之前的树，找到对应的节点，然后将节点注册回调List通知一遍，
	
	@Override
	    public void notifyChange(Uri uri, IContentObserver observer,
	            boolean observerWantsSelfNotifications, boolean syncToNetwork,
	            int userHandle) {
	        if (Log.isLoggable(TAG, Log.VERBOSE)) {
	            Log.v(TAG, "Notifying update of " + uri + " for user " + userHandle
	                    + " from observer " + observer + ", syncToNetwork " + syncToNetwork);
	        }
	
	        final int uid = Binder.getCallingUid();
	        final int pid = Binder.getCallingPid();
	        final int callingUserHandle = UserHandle.getCallingUserId();
	        // Notify for any user other than the caller requires uri grant or cross user permission
	        if (callingUserHandle != userHandle &&
	                mContext.checkUriPermission(uri, pid, uid, Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
	                        != PackageManager.PERMISSION_GRANTED) {
	            enforceCrossUserPermission(userHandle, "no permission to notify other users");
	        }
	
	        // We passed the permission check; resolve pseudouser targets as appropriate
	        if (userHandle < 0) {
	            if (userHandle == UserHandle.USER_CURRENT) {
	                userHandle = ActivityManager.getCurrentUser();
	            } else if (userHandle != UserHandle.USER_ALL) {
	                throw new InvalidParameterException("Bad user handle for notifyChange: "
	                        + userHandle);
	            }
	        }
	
	        // This makes it so that future permission checks will be in the context of this
	        // process rather than the caller's process. We will restore this before returning.
	        long identityToken = clearCallingIdentity();
	        try {
	            ArrayList<ObserverCall> calls = new ArrayList<ObserverCall>();
	            synchronized (mRootNode) {
	                mRootNode.collectObserversLocked(uri, 0, observer, observerWantsSelfNotifications,
	                        userHandle, calls);
	            }
	            final int numCalls = calls.size();
	            for (int i=0; i<numCalls; i++) {
	                ObserverCall oc = calls.get(i);
	                try {
	                    oc.mObserver.onChange(oc.mSelfChange, uri, userHandle);
	                    if (Log.isLoggable(TAG, Log.VERBOSE)) {
	                        Log.v(TAG, "Notified " + oc.mObserver + " of " + "update at " + uri);
	                    }
	                } catch (RemoteException ex) {
	                    synchronized (mRootNode) {
	                        Log.w(TAG, "Found dead observer, removing");
	                        IBinder binder = oc.mObserver.asBinder();
	                        final ArrayList<ObserverNode.ObserverEntry> list
	                                = oc.mNode.mObservers;
	                        int numList = list.size();
	                        for (int j=0; j<numList; j++) {
	                            ObserverNode.ObserverEntry oe = list.get(j);
	                            if (oe.observer.asBinder() == binder) {
	                                list.remove(j);
	                                j--;
	                                numList--;
	                            }
	                        }
	                    }
	                }
	            }
	            if (syncToNetwork) {
	                SyncManager syncManager = getSyncManager();
	                if (syncManager != null) {
	                    syncManager.scheduleLocalSync(null /* all accounts */, callingUserHandle, uid,
	                            uri.getAuthority());
	                }
	            }
	        } finally {
	            restoreCallingIdentity(identityToken);
	        }
	    }
    
ContentService发送通知可能是同步的，也可能是异步的，同步的就是Binder通信到来的时候，直接调用ContentObserver的回到，而异步，则是**插入消息就返回**，具体看下实现
        
     private static final class Transport extends IContentObserver.Stub {
        private ContentObserver mContentObserver;

        public Transport(ContentObserver contentObserver) {
            mContentObserver = contentObserver;
        }

        @Override
        public void onChange(boolean selfChange, Uri uri, int userId) {
            ContentObserver contentObserver = mContentObserver;
            if (contentObserver != null) {

			<!--通过 contentObserver发送回调通知-->
                contentObserver.dispatchChange(selfChange, uri, userId);
            }
        }

        public void releaseContentObserver() {
            mContentObserver = null;
        }
    }

ContentObserver在dispatchChange的时候，如果设置了Handler，就是异步，否则就是同步
    
    private void dispatchChange(boolean selfChange, Uri uri, int userId) {
        if (mHandler == null) {
            onChange(selfChange, uri, userId);
        } else {
            mHandler.post(new NotificationRunnable(selfChange, uri, userId));
        }
    }
    
到这里，通知流程就完成了。

# 总结    

* ContentService是一个系统级别的消息中心，提供系统级别的观察者模型
* ContentService的通信模型  其实是典型的Android 双C/S模型
* ContentService内部是通过树+list的方式管理ContentObserver回调
* ContentService在分发消息的时候，可以同步也可以异步，具体看APP端配置