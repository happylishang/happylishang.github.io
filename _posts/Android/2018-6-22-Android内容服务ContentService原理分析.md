---
layout: post
title: "Android内容服务ContentService原理分析"
category: Android  

---

ContentService可以看做Android中一个系统级别的消息中心，可以说搭建了一个系统级的观察者模型，APP可以向消息中心注册观察者，选择订阅自己关心的消息，也可以通过消息中心发送信息，通知其他进程，简单模型如下：

![“ContentService简单框架”.png](https://upload-images.jianshu.io/upload_images/1460468-f6b66069f275eec3.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

ContentService服务伴随系统启动，本身是一个Binder系统服务，运行在SystemServer进程。作为系统服务，最好能保持高效运行，因此ContentService通知APP都是异步的，也就是oneway的，仅仅插入目标进程（线程）的Queue队列，不必等待执行。下面简单分析一下整体的架构，主要从一下几个方面了解下运行流程：

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

前文已经说过，ContentService可以看做是通知的中转站，进程A想要通知其他注册了某个Uri的进程，必须首先向ContentService分发中心发送消息，再由ContentService通知其他进程中的观察者，简化模型如下图：

![ContentService框架.png](https://upload-images.jianshu.io/upload_images/1460468-846b5aa82fbb1761.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

简单跟踪下通知流程，入口函数如下

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

ContentService收到请求进一步处理，无非就是搜索之前的树，找到对应的节点，将节点上注册回调List通知一遍，具体逻辑如下：
	
	@Override
	    public void notifyChange(Uri uri, IContentObserver observer,
	            boolean observerWantsSelfNotifications, boolean syncToNetwork,
	            int userHandle) {

	        <!--权限检测-->
	        // This makes it so that future permission checks will be in the context of this
	        // process rather than the caller's process. We will restore this before returning.
	        
	        <!--找回调，处理回调-->
	        long identityToken = clearCallingIdentity();
	        try {
	            ArrayList<ObserverCall> calls = new ArrayList<ObserverCall>();
	            synchronized (mRootNode) {
	            <!--1 从根节点开始查找binder回调代理-->
	                mRootNode.collectObserversLocked(uri, 0, observer, observerWantsSelfNotifications,
	                        userHandle, calls);
	            }
	            final int numCalls = calls.size();
	            for (int i=0; i<numCalls; i++) {
	                ObserverCall oc = calls.get(i);
	                try {
	                <!--2 通知-->
	                    oc.mObserver.onChange(oc.mSelfChange, uri, userHandle);
	                } 
	               ...

    
从上面代码可以看出，其实就是两步，**先搜集所有的Binder回调，之后通过回调通知APP端**，搜集过程也是个递归的过程，也会存在父子粘连的一些回调逻辑（子Uri是否有必要通知路径中的父Uri回调），理解很简单，不再详述。这步之后，消息就通过Binder被传送给App端，在APP端，Binder实体的onTransact被回调，并处理相应的事务：
            

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

这里有一点需要注意，那就是IContentObserver中onChange是一个oneway请求，可以说，总是异步的，ContentService将消息塞入到APP端Binder线程的执行队列后就返回，不会等待处理结果才返回。

	interface IContentObserver
	{
	    /**
	     * This method is called when an update occurs to the cursor that is being
	     * observed. selfUpdate is true if the update was caused by a call to
	     * commit on the cursor that is being observed.
	     */
	     contentService 用的是oneway
	    oneway void onChange(boolean selfUpdate, in Uri uri, int userId);
	}


之后其实就是调用ContentObserver的dispatchChange，dispatchChange**可能是在Binder线程中同步执行，也可能是发送到一个与Handler绑定的线程中执行**，如下，
    
    private void dispatchChange(boolean selfChange, Uri uri, int userId) {
        if (mHandler == null) {
            onChange(selfChange, uri, userId);
        } else {
            mHandler.post(new NotificationRunnable(selfChange, uri, userId));
        }
    }
    
但是整体上来看，由于Binder oneway的存在，ContentService的通知是个异步的过程，一般来说，Binder线程应该是先与Main线程执行，因为Main还要等待AMS的返回呢，不过，也不一定，两个线程的竞争加上任务的繁重程度，所以是无法从根本上保证同步的。

#  一个奇葩问题的注意事项 Binder循环调用

假设有这样一个场景：

* A进程notify，
* A进程再收到通知
* A进程请求获取ContentProvider的数据，并且ContentProvider位于A进程

这个时候，如果，采用的是同步，也就是ContentObserver没有设置Handler，那就会遇到一个问题，系统会提示你没有权限访问ContentProvider，

 > java.lang.SecurityException: Permission Denial: reading XXX  uri content://MyContentProvider from pid=0, uid=1000 requires the provider be exported, or grantUriPermission()
 
 为什么，明明是当前App中声明的ContentProvider，为什么不能访问，并且pid=0, uid=1000 是怎么来的，其实这个时候是因为Binder机制中的一个小"BUG"，需要用户自己避免,ContentProvider在使用的时候会校验权限，
 
     /** {@hide} */
    protected int enforceReadPermissionInner(Uri uri, String callingPkg, IBinder callerToken)
            throws SecurityException {
        final Context context = getContext();
        // Binder.getCallingPid获取的可能不是我们想要的进程PID
        final int pid = Binder.getCallingPid();
        final int uid = Binder.getCallingUid();
        String missingPerm = null;
        int strongestMode = MODE_ALLOWED;
        ...

        final String failReason = mExported
                ? " requires " + missingPerm + ", or grantUriPermission()"
                : " requires the provider be exported, or grantUriPermission()";
        throw new SecurityException("Permission Denial: reading "
                + ContentProvider.this.getClass().getName() + " uri " + uri + " from pid=" + pid
                + ", uid=" + uid + failReason);
    }

Binder.getCallingPid()获取的可能并不是我们想要的进程PID，因为之前同步访问的时候 Binder.getCallingPid()被赋值为系统进程PID，在同步访问的时候，由于ContentProvider本身在A进程中，会直接调用ContentProvider的相应服务函数，但是Binder.getCallingPid()返回值并没有被更新，因为这个时候访问的时候不会走跨进程，  Binder.getCallingPid()的返回值不会被 更新，也就是说  Binder.getCallingPid()获取的进程是上一个notify时候的系统进程，那么自然也就没有权限。如果将ContentProvider放到A进程之外的进程，就不会有问题，当然，Android提供了解决方案，那就是

	<!--将Binder.getCallingPid()的值设定为当前进程-->
 
    final long identity = Binder.clearCallingIdentity();
    ...
    <!--恢复之前保存的值-->
    Binder.restoreCallingIdentity(identity);

以上两个函数配合使用，就可以避免之前的问题。这个问题Google不能从Binder上在底层解决吗？总觉是Binder通信的BUG。



# 总结    

* ContentService是一个系统级别的消息中心，提供系统级别的观察者模型
* ContentService的通信模型  其实是典型的Android 双C/S模型
* ContentService内部是通过树+list的方式管理ContentObserver回调
* ContentService在分发消息的时候，整体上是异步的，在APP端可以在Binder线程中同步处理，也可以发送到Handler绑定的线程中异步处理，具体看APP端配置