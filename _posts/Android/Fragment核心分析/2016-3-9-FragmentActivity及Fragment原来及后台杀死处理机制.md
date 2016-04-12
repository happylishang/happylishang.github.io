---
layout: post
title: "FragmentActivity及Fragment原来及后台杀死处理机制"
description: "Java"
category: android开发

---

#### 场景与问题

* 什么时候会有这个问题
* 为什么会有，已经会有什么后果
* 怎么处理

#### 应用何时会被后台杀死

在近期的任务列表里面，有些不是主动结束掉的任务，会因为内存紧张等原因被后台杀死。

PhoneWindowManager 

	 List<ActivityManager.RecentTaskInfo> recentTasks = am  
	                .getRecentTasks(MAX_RECENT_TASKS,  
	                        ActivityManager.RECENT_IGNORE_UNAVAILABLE);  
	                        
	                        。。。
	  /** 
     * 切换应用 
     */  
    private void switchTo(RecentTag tag) {  
        if (tag.info.id >= 0) {  
            // 这是一个活跃的任务，所以把它移动到最近任务的前面  
            final ActivityManager am = (ActivityManager) getContext()  
                    .getSystemService(Context.ACTIVITY_SERVICE);  
            am.moveTaskToFront(tag.info.id, ActivityManager.MOVE_TASK_WITH_HOME);  
        } else if (tag.intent != null) {  
            tag.intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY  
                    | Intent.FLAG_ACTIVITY_TASK_ON_HOME);  
            try {  
                getContext().startActivity(tag.intent);  
            } catch (ActivityNotFoundException e) {  
                Log.w("Recent", "Unable to launch recent task", e);  
            }  
        }  
    }                       
                        
后台杀死如何处理RecentTaskInfo

#### 后台杀死的后果


### Activity内部的Fragment后台杀死后重建，不是ViewPager的，由DialogFragment 得到的处理

每次重新创建DialogFragment，不要让系统恢复

    @Override
    protected void onSaveInstanceState(Bundle outState) {

    <!--    if (outState != null) {
            outState.putParcelable("android:support:fragments", null);
        }-->
        super.onSaveInstanceState(outState);
       <!--放在后面才有效--> 
            if (outState != null) {
            outState.putParcelable("android:support:fragments", null);
        }
        
    }
   
   那里具体原因是什么？多个？ 
   
 
   另一种做法
   
注：如果是FragmentActivity则在 onCreate之前添加如下

	if (savedInstanceState != null) {
	                        savedInstanceState.putParcelable(“android:support:fragments”, null);
	                }
	super.onCreate(savedInstanceState);


如果是actvity改成是“android:fragments"
 
 
###  原理，

其实，对于很多东西，Fragment 也只是暂存，暂存，并不会处理多余的逻辑，如果想要复用的话，还是需要自己取回原理的东西，数据或者其他的东西。至于恢复Fragment，如果Activity不是主动再次添加，也只是重建而已，不会显示，至于通常的显示，那只是再次添加到界面而已，为何DialogFragment如此明显，因为，他没有添加，会自己显示啊

    /**
     * Called by the system, as part of destroying an
     * activity due to a configuration change, when it is known that a new
     * instance will immediately be created for the new configuration.  You
     * can return any object you like here, including the activity instance
     * itself, which can later be retrieved by calling
     * {@link #getLastNonConfigurationInstance()} in the new activity
     * instance.
     
     仅仅是优化选项，不是为了必然处理
     
     This function is called purely as an optimization, and you must
     * not rely on it being called.  When it is called, a number of guarantees
     * will be made to help optimize configuration switching:
     
     
     
    /**
     * Retain all appropriate fragment and loader state.  You can NOT
     * override this yourself!  Use {@link #onRetainCustomNonConfigurationInstance()}
     * if you want to retain your own state.
     */
    @Override
    public final Object onRetainNonConfigurationInstance() {
        if (mStopped) {
            doReallyStop(true);
        }

        Object custom = onRetainCustomNonConfigurationInstance();

        ArrayList<Fragment> fragments = mFragments.retainNonConfig();
        boolean retainLoaders = false;
        if (mAllLoaderManagers != null) {
            // prune out any loader managers that were already stopped and so
            // have nothing useful to retain.
            final int N = mAllLoaderManagers.size();
            LoaderManagerImpl loaders[] = new LoaderManagerImpl[N];
            for (int i=N-1; i>=0; i--) {
                loaders[i] = mAllLoaderManagers.valueAt(i);
            }
            for (int i=0; i<N; i++) {
                LoaderManagerImpl lm = loaders[i];
                if (lm.mRetaining) {
                    retainLoaders = true;
                } else {
                    lm.doDestroy();
                    mAllLoaderManagers.remove(lm.mWho);
                }
            }
        }
        if (fragments == null && !retainLoaders && custom == null) {
            return null;
        }
        
        NonConfigurationInstances nci = new NonConfigurationInstances();
        nci.activity = null;
        nci.custom = custom;
        nci.children = null;
        nci.fragments = fragments;
        nci.loaders = mAllLoaderManagers;
        return nci;
    }
 
#### 如何应对


#### Activity退回后台，不退出应用 false根Activity

    /**
     * Move the task containing this activity to the back of the activity
     * stack.  The activity's order within the task is unchanged.
     * 
     * @param nonRoot If false then this only works if the activity is the root
     *                of a task; if true it will work for any activity in
     *                a task.
     * 
     * @return If the task was moved (or it was already at the
     *         back) true is returned, else false.
     */
    public boolean moveTaskToBack(boolean nonRoot) {
        try {
            return ActivityManagerNative.getDefault().moveActivityTaskToBack(
                    mToken, nonRoot);
        } catch (RemoteException e) {
            // Empty
        }
        return false;
    }
但是back返回键，可能会触发onSaveInstanceState

   Android calls onSaveInstanceState() before the activity becomes vulnerable to being destroyed by the system, but does not bother calling it when the instance is actually being destroyed by a user action 
(such as pressing the BACK key) 


	        
#### 	Fragment Transactions & Activity State Loss  解决IllegalStateException: Can not perform this action after onSaveInstanceState        

大致意思是说 commit方法是在Activity的onSaveInstanceState()之后调用的，这样会出错，因为onSaveInstanceState

方法是在该Activity即将被销毁前调用，来保存Activity数据的，如果在保存玩状态后再给它添加Fragment就会出错。解决办法就

是把commit（）方法替换成 commitAllowingStateLoss()就行了，其效果是一样的。
	        
	        
**How to avoid the exception?**

Avoiding Activity state loss becomes a whole lot easier once you understand what is actually going on. If you’ve made it this far in the post, hopefully you understand a little better how the support library works and why it is so important to avoid state loss in your applications. In case you’ve referred to this post in search of a quick fix, however, here are some suggestions to keep in the back of your mind as you work with FragmentTransactions in your applications:

**Be careful when committing transactions inside Activity lifecycle methods. A large majority of applications will only ever commit transactions the very first time onCreate() is called and/or in response to user input, and will never face any problems as a result. However, as your transactions begin to venture out into the other Activity lifecycle methods, such as onActivityResult(), onStart(), and onResume(), things can get a little tricky. For example, you should not commit transactions inside the FragmentActivity#onResume() method, as there are some cases in which the method can be called before the activity’s state has been restored (see the documentation for more information). If your application requires committing a transaction in an Activity lifecycle method other than onCreate(), do it in either FragmentActivity#onResumeFragments() or Activity#onPostResume(). These two methods are guaranteed to be called after the Activity has been restored to its original state, and therefore avoid the possibility of state loss all together. (As an example of how this can be done, check out my answer to this StackOverflow question for some ideas on how to commit FragmentTransactions in response to calls made to the Activity#onActivityResult() method).**

Avoid performing transactions inside asynchronous callback methods. This includes commonly used methods such as AsyncTask#onPostExecute() and LoaderManager.LoaderCallbacks#onLoadFinished(). The problem with performing transactions in these methods is that they have no knowledge of the current state of the Activity lifecycle when they are called. For example, consider the following sequence of events:
An activity executes an AsyncTask.
The user presses the “Home” key, causing the activity’s onSaveInstanceState() and onStop() methods to be called.
The AsyncTask completes and onPostExecute() is called, unaware that the Activity has since been stopped.
A FragmentTransaction is committed inside the onPostExecute() method, causing an exception to be thrown.
In general, the best way to avoid the exception in these cases is to simply avoid committing transactions in asynchronous callback methods all together. Google engineers seem to agree with this belief as well. According to this post on the Android Developers group, the Android team considers the major shifts in UI that can result from committing FragmentTransactions from within asynchronous callback methods to be bad for the user experience. If your application requires performing the transaction inside these callback methods and there is no easy way to guarantee that the callback won’t be invoked after onSaveInstanceState(), you may have to resort to using commitAllowingStateLoss() and dealing with the state loss that might occur. (See also these two StackOverflow posts for additional hints, here and here).

Use commitAllowingStateLoss() only as a last resort. The only difference between calling commit() and commitAllowingStateLoss() is that the latter will not throw an exception if state loss occurs. Usually you don’t want to use this method because it implies that there is a possibility that state loss could happen. The better solution, of course, is to write your application so that commit() is guaranteed to be called before the activity’s state has been saved, as this will result in a better user experience. Unless the possibility of state loss can’t be avoided, commitAllowingStateLoss() should not be used.

[参考文档 ：Fragment Transactions & Activity State Loss](http://www.androiddesignpatterns.com/2013/08/fragment-transaction-commit-state-loss.html)	


    @Override
    protected void onResumeFragments() {
        super.onResumeFragments();
        if (currentTab != null) {
            int position = TabType.getTabPosition(currentTab);
            if (position >= 0) {
                fragmentTabHost.setCurrentTab(position);
            }
        }
    }        
If you are using the support-v4 library and FragmentActivity, try to always use onResumeFragments() instead of onResume() in your FragmentActivity implementations.

FragmentActivity#onResume() documentation:

To correctly interact with fragments in their proper state, you should instead override onResumeFragments().


                f.mActivity = mActivity;
                    f.mParentFragment = mParent;
                    f.mFragmentManager = mParent != null
                            ? mParent.mChildFragmentManager : mActivity.mFragments;
                    f.mCalled = false;
                    f.onAttach(mActivity);



**For instance the application wants to access a Fragment that was inflated during onCreate(). The best place for this is onResumeFragments().**
   
####onRetainNonConfigurationInstance和 onSaveInstanceState、getLastNonConfigurationInstance

	
	 @Override
	        public Object onRetainNonConfigurationInstance() {
	                return this;
	        }



	
#### Fragment必须提供默认构造方法的原理 反射机制重建Fragment实例 默认无参构造函数

   void restoreAllState(Parcelable state, ArrayList<Fragment> nonConfig) {	  ...
           mActive = new ArrayList<Fragment>(fms.mActive.length);
        if (mAvailIndices != null) {
            mAvailIndices.clear();
        }
        for (int i=0; i<fms.mActive.length; i++) {
            FragmentState fs = fms.mActive[i];
            if (fs != null) {
                Fragment f = fs.instantiate(mActivity, mParent);
                if (DEBUG) Log.v(TAG, "
	
	    /**
     * Create a new instance of a Fragment with the given class name.  This is
     * the same as calling its empty constructor.
     *
     * @param context The calling context being used to instantiate the fragment.
     * This is currently just used to get its ClassLoader.
     * @param fname The class name of the fragment to instantiate.
     * @param args Bundle of arguments to supply to the fragment, which it
     * can retrieve with {@link #getArguments()}.  May be null.
     * @return Returns a new fragment instance.
     * @throws InstantiationException If there is a failure in instantiating
     * the given fragment class.  This is a runtime exception; it is not
     * normally expected to happen.
     */
    public static Fragment instantiate(Context context, String fname, @Nullable Bundle args) {
        try {
            Class<?> clazz = sClassMap.get(fname);
            if (clazz == null) {
                // Class not found in the cache, see if it's real, and try to add it
                clazz = context.getClassLoader().loadClass(fname);
                sClassMap.put(fname, clazz);
            }
            Fragment f = (Fragment)clazz.newInstance();
            if (args != null) {
                args.setClassLoader(f.getClass().getClassLoader());
                f.mArguments = args;
            }
            return f;
        } catch (ClassNotFoundException e) {
            throw new InstantiationException("Unable to instantiate fragment " + fname
                    + ": make sure class name exists, is public, and has an"
                    + " empty constructor that is public", e);
        } catch (java.lang.InstantiationException e) {
            throw new InstantiationException("Unable to instantiate fragment " + fname
                    + ": make sure class name exists, is public, and has an"
                    + " empty constructor that is public", e);
        } catch (IllegalAccessException e) {
            throw new InstantiationException("Unable to instantiate fragment " + fname
                    + ": make sure class name exists, is public, and has an"
                    + " empty constructor that is public", e);
        }
    }
    
####  Fragment重建流程

*   如果非空，重建Fragment并将它们设置为Initialing，毕竟还没有resume

       if (savedInstanceState != null) {
            Parcelable p = savedInstanceState.getParcelable(FRAGMENTS_TAG);
            mFragments.restoreAllState(p, nc != null ? nc.fragments : null);
        }
* 第二步，就是专为onCreate
 
        mFragments.dispatchCreate();  

* 第三部 等到Actviity Onresume，就让Fragment resume，至于后面 onPostResume 等待深度剖析

* 第四步 

	    @Override
	    protected void onResume() {
	        super.onResume();
	        mHandler.sendEmptyMessage(MSG_RESUME_PENDING);
	        mResumed = true;
	        mFragments.execPendingActions();
	    } 

* 第五步

	    @Override
	    protected void onResume() {
	        super.onResume();
	        mHandler.sendEmptyMessage(MSG_RESUME_PENDING);
	        mResumed = true;
	        mFragments.execPendingActions();
	    } 
         
#### 何时何地调用什么，

MVC模式的体现，newState代表是当前Actvity传递给的FragmentManager的state，位于FragmentManager中，FragmentManager可以看做是FragmentActvity的管理器C，Fragmentmanager会根据mCurState的值，修改当前别添加的fragment的状态，如果是Actvity处于resume状态，那么被添加的fragment就会被处理成激活状态 当然首先要初始化新建的fragment ,然后匹配新状态，是否有必要将状态等级提升。 很明显，没有被added或者或者说已经detach的Fragment是不用走到resume的


        // Fragments that are not currently added will sit in the onCreate() state.
        if ((!f.mAdded || f.mDetached) && newState > Fragment.CREATED) {
            newState = Fragment.CREATED;
        }
 
 
	 final class FragmentManagerImpl extends FragmentManager implements LayoutInflaterFactory {  
	 
	     int mCurState = Fragment.INITIALIZING;           
###  参考文档
[Lowmemorykiller笔记](http://blog.csdn.net/guoqifa29/article/details/45370561) **精** 

[Fragment实例化，Fragment生命周期源码分析](http://johnnyyin.com/2015/05/19/android-fragment-life-cycle.html)

[ android.app.Fragment$InstantiationException的原因分析](http://blog.csdn.net/sun927/article/details/46629919)

[Android Framework架构浅析之【近期任务】](http://blog.csdn.net/lnb333666/article/details/7869465)

[Android Low Memory Killer介绍](http://mysuperbaby.iteye.com/blog/1397863)

 
[Android开发之InstanceState详解]( http://www.cnblogs.com/hanyonglu/archive/2012/03/28/2420515.html )