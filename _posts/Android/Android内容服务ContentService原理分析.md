ContentService可以看做Android中一个系统级别的消息中心，可以说搭建了一个系统级的观察者模型，APP可以向消息中心注册观察者，选择订阅自己关心的消息，也可以通过消息中心发送信息，通知其他进程。ContentService服务伴随系统启动，本身是一个Binder系统服务，运行在SystemServer进程，作为系统服务，ContentService不可能阻塞为某个APP提供服务，这也注定了在分发消息的时候，是通过向目标进程插入消息的方式来处理（类似AMS），下面简单分析一下整体的架构，主要从一下几个方面了解下运行流程：

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

虽然从使用上来说，ContentService跟ContentProvider关系紧密，但是理论上讲，这是完全独立的两套东西，ContentService是一个独立的消息分发模型，可以完全独立于ContentProvider使用，看一下基本用法：

>1、注册一个观察者：

    public static void registerObserver(Context context,ContentObserver contentObserver) {
        ContentResolver contentResolver = context.getContentResolver();
        contentResolver.registerContentObserver(FileContentProvider.CONTENT_URI, true, contentObserver);
    }
        
>2、通知观察者
 
     public static void notity(Context context,Uri uri) {
        ContentResolver contentResolver = context.getContentResolver();
        contentResolver.notifyChange(uri);
    }
    
可以看到，期间只是借用了ContentResolver，但是并没有牵扯到ContentProvider，任何进程都能ContentService提供了一个系统级的观察者模型，只是，比较适合做通知，不太适合发通知的时候，传递数据。



# 注册流程

先看下注册观察者

    public final void registerContentObserver(Uri uri, boolean notifyForDescendents,
            ContentObserver observer, int userHandle) {
        try {
            getContentService().registerContentObserver(uri, notifyForDescendents,
                    observer.getContentObserver(), userHandle);
        } catch (RemoteException e) {
        }
    }
    
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
    
采用的是单利的模式，这里直接向ServiceManager请求   ContentService服务，请求成功后，便可以获得该服务的代理，之后通过代理发送请求，首先看下注册，通过ContentObserver获取一个IContentObserver对象，APP端将该对象通过binder传递到ContentService服务，如此ContentService便能通过Binder向APP端发送通知
 
     public IContentObserver getContentObserver() {
        synchronized (mLock) {
            if (mTransport == null) {
                mTransport = new Transport(this);
            }
            return mTransport;
        }
    }
    
     private static final class Transport extends IContentObserver.Stub {
        private ContentObserver mContentObserver;

        public Transport(ContentObserver contentObserver) {
            mContentObserver = contentObserver;
        }

        @Override
        public void onChange(boolean selfChange, Uri uri, int userId) {
            ContentObserver contentObserver = mContentObserver;
            if (contentObserver != null) {
                contentObserver.dispatchChange(selfChange, uri, userId);
            }
        }

        public void releaseContentObserver() {
            mContentObserver = null;
        }
    }
    
其实就是Android框架中非常常用的双C/S通信，   Transport本身是一个Binder实体对象，被注册到ContentService中，ContentService会维护一个Transport的List，将来通知不同的进程,接着看下register

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

主要看下点2，添加监听对象，其实ContentService维护了一个监听对象的树，主要是根据Uri的路径，方便管理，同时提高查找及插入效率，每个监听对象对应一个节点，也就是一个ObserverNode对象， 而ContentService持有RootNode根对象，

	 private final ObserverNode mRootNode = new ObserverNode("");
	 
![Content树.png](https://upload-images.jianshu.io/upload_images/1460468-3f1823c5571efc4e.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

而每个ObserverNode维护了一个ObserverEntry队列，每个ObserverEntry都对应一个Observer，同一个Uri可以有多个Observer，也就是会多个ObserverEntry，同时还有一些其他辅助信息，比如要跟Uri形成键值对，ObserverEntry还将自己设置成了Binder讣告的接受者，一旦APP端进程结束，可以通过Binder讣告机制让ContentService端收到通知，并做一些清理工作，具体实现如下：

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
                
接着看下Observer的add流程，addObserverLocked被外部调用的时候，一般传递的index是0，自己递归调用的时候，才不是0

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
  
  比如我们查询content://A/B/C对应的ObserverNode，首先会找到Authority对应的A的ObserverNode，之后在A的children中查找Path=B的Node，然后在B的Children中查找Path=C的Node，找到该Node之后，往这个node的 ObserverEntry列表中添加一个对象，到这里就注册就完成了。
  
  
#  通知流程    

ContentService可以看做是通知的中转站，进程A想要通知其他注册了某个Uri的进程，必须首先向ContentService这个消息分发中心发送消息，再由ContentService通知其他进程中的观察者。

![ContentService框架.png](https://upload-images.jianshu.io/upload_images/1460468-846b5aa82fbb1761.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
