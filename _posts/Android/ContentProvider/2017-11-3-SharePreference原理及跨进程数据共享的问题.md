---
layout: post
title: SharePreference原理及跨进程数据共享的问题
category: Android
image: http://upload-images.jianshu.io/upload_images/1460468-c30485e5d121f874.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---

SharedPreferences是Android提供的数据持久化的一种手段，适合单进程、小批量的数据存储与访问。为什么这么说呢？因为SharedPreferences的实现是基于单个xml文件实现的，并且，所有持久化数据都是一次性加载到内存，如果数据过大，是不合适采用SharedPreferences存放的。而适用的场景是单进程的原因同样如此，由于Android原生的文件访问并不支持多进程互斥，所以SharePreferences也不支持，如果多个进程更新同一个xml文件，就可能存在同不互斥问题，后面会详细分析这几个问题。

# SharedPreferences的实现原理之:持久化数据的加载

首先，从基本使用简单看下SharedPreferences的实现原理：
        
        mSharedPreferences = context.getSharedPreferences("test", Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = mSharedPreferences.edit();
        editor.putString(key, value);
        editor.apply();
        
        
 context.getSharedPreferences其实就是简单的调用ContextImpl的getSharedPreferences，具体实现如下
        
           @Override
    public SharedPreferences getSharedPreferences(String name, int mode) {
        SharedPreferencesImpl sp;
        synchronized (ContextImpl.class) {
            if (sSharedPrefs == null) {
                sSharedPrefs = new ArrayMap<String, ArrayMap<String, SharedPreferencesImpl>>();
            }

            final String packageName = getPackageName();
            ArrayMap<String, SharedPreferencesImpl> packagePrefs = sSharedPrefs.get(packageName);
            if (packagePrefs == null) {
                packagePrefs = new ArrayMap<String, SharedPreferencesImpl>();
                sSharedPrefs.put(packageName, packagePrefs);
            }
            sp = packagePrefs.get(name);
            if (sp == null) {
            <!--读取文件-->
                File prefsFile = getSharedPrefsFile(name);
                sp = new SharedPreferencesImpl(prefsFile, mode);
                <!--缓存sp对象-->
                packagePrefs.put(name, sp);
                return sp;
            }
        }
        <!--跨进程同步问题-->
        if ((mode & Context.MODE_MULTI_PROCESS) != 0 ||
            getApplicationInfo().targetSdkVersion < android.os.Build.VERSION_CODES.HONEYCOMB) {
            sp.startReloadIfChangedUnexpectedly();
        }
        return sp;
    }

以上代码非常简单，直接描述下来就是先去内存中查询与xml对应的SharePreferences是否已经被创建加载，如果没有那么该创建就创建，该加载就加载，在加载之后，要将所有的key-value保存到内幕才能中去，当然，如果首次访问，可能连xml文件都不存在，那么还需要创建xml文件，与SharePreferences对应的xml文件位置一般都在/data/data/包名/shared_prefs目录下，后缀一定是.xml，数据存储样式如下

![sp对应的xml数据存储模型](http://upload-images.jianshu.io/upload_images/1460468-c30485e5d121f874.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

这里面数据的加载的地方需要看下，比如，SharePreferences数据的加载是同步还是异步？数据加载是new SharedPreferencesImpl对象时候开始的，

     SharedPreferencesImpl(File file, int mode) {
        mFile = file;
        mBackupFile = makeBackupFile(file);
        mMode = mode;
        mLoaded = false;
        mMap = null;
        startLoadFromDisk();
    }
    
startLoadFromDisk很简单，就是读取xml配置，如果其他线程想要在读取之前就是用的话，就会被阻塞，一直wait等待，直到数据读取完成。
    
        private void loadFromDiskLocked() {
       ...
        Map map = null;
        StructStat stat = null;
        try {
            stat = Os.stat(mFile.getPath());
            if (mFile.canRead()) {
                BufferedInputStream str = null;
                try {
        <!--读取xml中配置-->
                    str = new BufferedInputStream(
                            new FileInputStream(mFile), 16*1024);
                    map = XmlUtils.readMapXml(str);
                }...
        mLoaded = true;
        ...
        <!--唤起其他等待线程-->
        notifyAll();
    }
    
可以看到其实就是直接使用xml解析工具XmlUtils，直接在当前线程读取xml文件，所以，如果xml文件稍大，尽量不要在主线程读取，读取完成之后，xml中的配置项都会被加载到内存，再次访问的时候，其实访问的是内存缓存。

# SharedPreferences的实现原理之:持久化数据的更新 

通常更新SharedPreferences的时候是首先获取一个SharedPreferences.Editor，利用它缓存一批操作，之后当做事务提交，有点类似于数据库的批量更新：

        SharedPreferences.Editor editor = mSharedPreferences.edit();
        editor.putString(key1, value1);
        editor.putString(key2, value2);
        editor.putString(key3, value3);
        editor.apply();//或者commit
        
Editor是一个接口，这里的实现是一个EditorImpl对象，它首先批量预处理更新操作，之后再提交更新，在提交事务的时候有两种方式，一种是apply，另一种commit，两者的区别在于：何时将数据持久化到xml文件，前者是异步的，后者是同步的。Google推荐使用前一种，因为，就单进程而言，只要保证内存缓存正确就能保证运行时数据的正确性，而持久化，不必太及时，这种手段在Android中使用还是很常见的，比如权限的更新也是这样，况且，Google并不希望SharePreferences用于多进程，因为不安全，手下卡一下apply与commit的区别
 
        public void apply() {
        <!--添加到内存-->
            final MemoryCommitResult mcr = commitToMemory();
            final Runnable awaitCommit = new Runnable() {
                    public void run() {
                        try {
                            mcr.writtenToDiskLatch.await();
                        } catch (InterruptedException ignored) {
                        }
                    }
                };

            QueuedWork.add(awaitCommit);
            Runnable postWriteRunnable = new Runnable() {
                    public void run() {
                        awaitCommit.run();
                        QueuedWork.remove(awaitCommit);
                    }
                };
            <!--延迟写入到xml文件-->
            SharedPreferencesImpl.this.enqueueDiskWrite(mcr, postWriteRunnable);
            <!--通知数据变化-->
            notifyListeners(mcr);
        }
     
     public boolean commit() {
            MemoryCommitResult mcr = commitToMemory();
            SharedPreferencesImpl.this.enqueueDiskWrite(
                mcr, null /* sync write on this thread okay */);
            try {
                mcr.writtenToDiskLatch.await();
            } catch (InterruptedException e) {
                return false;
            }
            notifyListeners(mcr);
            return mcr.writeToDiskResult;
        }     

从上面可以看出两者最后都是先调用commitToMemory，将更改提交到内存，在这一点上两者是一致的，之后又都调用了enqueueDiskWrite进行数据持久化任务，不过commit函数一般会在当前线程直接写文件，而apply则提交一个事务到已给线程池，之后直接返回，实现如下：
        
     private void enqueueDiskWrite(final MemoryCommitResult mcr,
                                  final Runnable postWriteRunnable) {
        final Runnable writeToDiskRunnable = new Runnable() {
                public void run() {
                    synchronized (mWritingToDiskLock) {
                        writeToFile(mcr);
                    }
                    synchronized (SharedPreferencesImpl.this) {
                        mDiskWritesInFlight--;
                    }
                    if (postWriteRunnable != null) {
                        postWriteRunnable.run();
                    }
                }
            };
       final boolean isFromSyncCommit = (postWriteRunnable == null);
        if (isFromSyncCommit) {
            boolean wasEmpty = false;
            synchronized (SharedPreferencesImpl.this) {
                wasEmpty = mDiskWritesInFlight == 1;
            }
            <!--如果没有其他线程在写文件，直接在当前线程执行-->
            if (wasEmpty) {
                writeToDiskRunnable.run();
                return;
            }
        }
       QueuedWork.singleThreadExecutor().execute(writeToDiskRunnable);
    }

不过如果有线程在写文件，那么就不能直接写，这个时候就跟apply函数一致了，但是，如果直观说两者的区别的话，**直接说commit同步，而apply异步应该也是没有多大问题的**。
 
#  SharePreferences多进程使用问题

SharePreferences在新建的有个mode参数，可以指定它的加载模式，MODE_MULTI_PROCESS是Google提供的一个多进程模式，但是这种模式并不是我们说的支持多进程同步更新等，它的作用只会在getSharedPreferences的时候，才会重新从xml重加载，如果我们在一个进程中更新xml，但是没有通知另一个进程，那么另一个进程的SharePreferences是不会自动更新的。

    @Override
    public SharedPreferences getSharedPreferences(String name, int mode) {
        SharedPreferencesImpl sp;
        ...
        if ((mode & Context.MODE_MULTI_PROCESS) != 0 ||
            getApplicationInfo().targetSdkVersion < android.os.Build.VERSION_CODES.HONEYCOMB) {
            // If somebody else (some other process) changed the prefs
            // file behind our back, we reload it.  This has been the
            // historical (if undocumented) behavior.
            sp.startReloadIfChangedUnexpectedly();
        }
        return sp;
    }
    
也就是说MODE_MULTI_PROCESS只是个鸡肋Flag，对于多进程的支持几乎为0，下面是Google文档，简而言之，就是：**不要用**。
    
MODE_MULTI_PROCESS does not work reliably in some versions of Android, and furthermore does not provide any mechanism for reconciling concurrent modifications across processes.  Applications should not attempt to use it.  Instead, they should use an explicit cross-process data management approach such as ContentProvider。

响应的Google为多进程提供了一个数据同步互斥方案，那就是基于Binder实现的ContentProvider，关于ContentProvider后文分析。

# 总结

* SharePreferences是Android基于xml实现的一种数据持久话手段
* SharePreferences不支持多进程
* SharePreferences的commit与apply一个是同步一个是异步（大部分场景下）
* 不要使用SharePreferences存储太大的数据
     