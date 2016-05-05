---
layout: post
title: "FragmentActivity及Fragment本质及后台杀死处理机制"
description: "Java"
category: android开发

---

###### **前言：Fragment只是View管理的一种方式**

>  [背景](#background)   
>  [add一个Fragment并显示的原理](#add_fragment)        
>  [FragmentActivity被后台杀死后恢复逻辑](#fragment_activity_restore)    
>  [普通的Fragment流程及所谓Fragment生命周期 依托FragmentActivity进行](#life_circle)       
>  [FragmentTabHost的后天杀死重建](#lFragmentTabHost_restore_life)     
>  [FragmentPagerAdapter的后台杀死重建](#FragmentPagerAdapter_restore)         
>  [后台杀死处理方式](#how_to_resolve)    
>  [Fragment使用很多坑，尤其是被后台杀死后恢复](#Fragment_bugs)    
>  [Can not perform this action after onSaveInstanceState](#Can_not_onSaveInstanceState)  
          
>  为什么返回主菜单，但是再回来不重建呢？？  

>  [结束语](#end)     
>  [参考文档](#ref_doc)    
   
#### 分析问题的方法与步骤

* **什么时候会出现这个问题**
* **为什么会出现**
* **怎么处理，能解决问题**

<a name="background"></a>

#### 背景

开发的时候，虽然一直遵守谷歌的Android开发文档，创建Fragment尽量采用推荐的参数传递方式，并且保留默认的Fragment无参构造方法，这样避免绝大部分APP被后台杀死，恢复崩溃的问题，但是对于原理的了解紧限于恢复时的重建机制，采用反射机制，并使用了默认的构造参数，直到使用FragmentDialog，示例代码如下：

	public class DialogFragmentActivity extends AppCompatActivity {
	
	    @Override
	    protected void onCreate(Bundle savedInstanceState) {
	        super.onCreate(savedInstanceState);
	        setContentView(R.layout.activity_dialog_fragment_test);
	        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
	        setSupportActionBar(toolbar);
	        DialogFragment dialogFragment = new FragmentDlg();
	        dialogFragment.show(getSupportFragmentManager(), "");
	    }

上面的DialogFragmentActivity内部创建了一个FragmentDialog，并显示，如果，此时被后台杀死，或旋转屏幕，被恢复的DialogFragmentActivity时会出现两个FragmentDialog，一个被系统恢复的，一个新建的。这种场景对于普通的Fragment也适用。如果单个Activity采用普通的add方式添加，被后台杀死后恢复，就会有两个Fragment出现。

<a name="add_fragment"/>

#### Add一个Fragment并显示的原理

通常我们FragmentActivity使用Fragment的方法如下：

	Fragment fr = Fragment.instance("")
	getSupportFragmentManager().beginTransaction()
	.add(R.id.container,fr).commit();

其中	getSupportFragmentManager返回的是 FragmentManagerImpl，踏实FragmentActivity的一个内部变量，其实Android无处不采用了设计模式，这里就是FragmentActivity把逻辑的管理交给FragmentManagerImpl，

    final FragmentManagerImpl mFragments = new FragmentManagerImpl();
    final FragmentContainer mContainer = new FragmentContainer() {
        @Override
        @Nullable
        public View findViewById(int id) {
            return FragmentActivity.this.findViewById(id);
        }

        @Override
        public boolean hasView() {
            Window window = FragmentActivity.this.getWindow();
            return (window != null && window.peekDecorView() != null);
        }
    };

FragmentManagerImpl的beginTransaction()函数返回的是一个BackStackRecord()

    @Override
    public FragmentTransaction beginTransaction() {
        return new (this);
    }
    
其实从名字就可以看出，只是FragmentActivity里面回退栈的一条记录，add函数实现如下，

    public FragmentTransaction add(Fragment fragment, String tag) {
        doAddOp(0, fragment, tag, OP_ADD);
        return this;
    }
    
为什么说FragmentManager是FragmentActivity的C，看下面：

	final class FragmentManagerImpl extends FragmentManager implements LayoutInflaterFactory {
	    static boolean DEBUG = false;
	    static final String TAG = "FragmentManager";
	    
	    static final boolean HONEYCOMB = android.os.Build.VERSION.SDK_INT >= 11;
	
	    static final String TARGET_REQUEST_CODE_STATE_TAG = "android:target_req_state";
	    static final String TARGET_STATE_TAG = "android:target_state";
	    static final String VIEW_STATE_TAG = "android:view_state";
	    static final String USER_VISIBLE_HINT_TAG = "android:user_visible_hint";
	
	    ArrayList<Runnable> mPendingActions;
	    Runnable[] mTmpActions;
	    boolean mExecutingActions;
	    
	    ArrayList<Fragment> mActive;
	    ArrayList<Fragment> mAdded;
	    ArrayList<Integer> mAvailIndices;
	    ArrayList<BackStackRecord> mBackStack;
	    ArrayList<Fragment> mCreatedMenus;
可以看出FragmentManagerImpl维护一个Activity所有的Fragment，Fragments可以看做是M，V是Activity自身。FragmentManagerImpl的State是和Activity的State一致的，这是管理Fragment的关键。其实Fragment自身是没有什么生命周期的，完全依靠FragmentManagerImpl模拟。

fragment.mFragmentManager都会指向Activity中唯一的FragmentManager，其实对于每个add，Android都将他们封装成一个度里的Action，在每个Action内部自己处理自己的逻辑，这个做法值得学习，


    private void doAddOp(int containerViewId, Fragment fragment, String tag, int opcmd) {
        fragment.mFragmentManager = mManager;

        if (tag != null) {
            if (fragment.mTag != null && !tag.equals(fragment.mTag)) {
                throw new IllegalStateException("Can't change tag of fragment "
                        + fragment + ": was " + fragment.mTag
                        + " now " + tag);
            }
            fragment.mTag = tag;
        }

        if (containerViewId != 0) {
            if (fragment.mFragmentId != 0 && fragment.mFragmentId != containerViewId) {
                throw new IllegalStateException("Can't change container ID of fragment "
                        + fragment + ": was " + fragment.mFragmentId
                        + " now " + containerViewId);
            }
            fragment.mContainerId = fragment.mFragmentId = containerViewId;
        }

        Op op = new Op();
        op.cmd = opcmd;
        op.fragment = fragment;
        addOp(op);
    }
        
之后commit这个Transaction

    public int commit() {
        return commitInternal(false);
    }
    
在真正处理这个 Transaction之前，或者说更新UI之前，Android做了一项检查，就是当前的

    int commitInternal(boolean allowStateLoss) {
        if (mCommitted) throw new IllegalStateException("commit already called");
        if (FragmentManagerImpl.DEBUG) {
            Log.v(TAG, "Commit: " + this);
            LogWriter logw = new LogWriter(TAG);
            PrintWriter pw = new PrintWriter(logw);
            dump("  ", null, pw, null);
        }
        mCommitted = true;
        if (mAddToBackStack) {
            mIndex = mManager.allocBackStackIndex(this);
        } else {
            mIndex = -1;
        }
        mManager.enqueueAction(this, allowStateLoss);
        return mIndex;
    }

    public void enqueueAction(Runnable action, boolean allowStateLoss) {
        if (!allowStateLoss) {
            checkStateLoss();
        }
        synchronized (this) {
            if (mDestroyed || mActivity == null) {
                throw new IllegalStateException("Activity has been destroyed");
            }
            if (mPendingActions == null) {
                mPendingActions = new ArrayList<Runnable>();
            }
            mPendingActions.add(action);
            if (mPendingActions.size() == 1) {
                mActivity.mHandler.removeCallbacks(mExecCommit);
                mActivity.mHandler.post(mExecCommit);
            }
        }
    }

为什么会有Can not perform this action after onSaveInstanceState

    private void checkStateLoss() {
        if (mStateSaved) {
            throw new IllegalStateException(
                    "Can not perform this action after onSaveInstanceState");
        }
        if (mNoTransactionsBecause != null) {
            throw new IllegalStateException(
                    "Can not perform this action inside of " + mNoTransactionsBecause);
        }
    }
    
最终会回调 FragmentManager的方法

    public void addFragment(Fragment fragment, boolean moveToStateNow) {
        if (mAdded == null) {
            mAdded = new ArrayList<Fragment>();
        }
        if (DEBUG) Log.v(TAG, "add: " + fragment);
        makeActive(fragment);
        if (!fragment.mDetached) {
            if (mAdded.contains(fragment)) {
                throw new IllegalStateException("Fragment already added: " + fragment);
            }
            mAdded.add(fragment);
            fragment.mAdded = true;
            fragment.mRemoving = false;
            if (fragment.mHasMenu && fragment.mMenuVisible) {
                mNeedMenuInvalidate = true;
            }
            if (moveToStateNow) {
                moveToState(fragment);
            }
        }
    }    
    
这里看一下添加View的代码，其实Fragment只是View的一个比较复杂的封装，FragmentManager最后将Fragment在Activity中显示出来。


     void moveToState(Fragment f, int newState, int transit, int transitionStyle,
            boolean keepActive) {
        // Fragments that are not currently added will sit in the onCreate() state.
        if ((!f.mAdded || f.mDetached) && newState > Fragment.CREATED) {
            newState = Fragment.CREATED;
        }
        
                            f.mContainer = container;
                            f.mView = f.performCreateView(f.getLayoutInflater(
                                    f.mSavedFragmentState), container, f.mSavedFragmentState);
                            if (f.mView != null) {
                                f.mInnerView = f.mView;
                                if (Build.VERSION.SDK_INT >= 11) {
                                    ViewCompat.setSaveFromParentEnabled(f.mView, false);
                                } else {
                                    f.mView = NoSaveStateFrameLayout.wrap(f.mView);
                                }
                                if (container != null) {
                                    Animation anim = loadAnimation(f, transit, true,
                                            transitionStyle);
                                    if (anim != null) {
                                        f.mView.startAnimation(anim);
                                    }
                                    container.addView(f.mView);
                                }
                                

之后根据当前Activity的状态，决定是否显示Fragment，这里是正常的流程，至于后台杀死，就要看第二个异常处理的流程。
    
<a name="fragment_activity_restore"></a>

#### FragmentActivity被后台杀死后恢复逻辑

当App被后台异常杀死后，再次点击icon，或者从最近任务列表进入的时候，系统会帮助恢复当时的场景，重新创建Activity，对于FragmentActivity，由于其中有Framgent，逻辑会相对再复杂一些，系统会首先重建被销毁的Fragment。看FragmentActivity的onCreat代码：

    protected void onCreate(@Nullable Bundle savedInstanceState) {
        this.mFragments.attachHost((Fragment)null);
        super.onCreate(savedInstanceState);
        FragmentActivity.NonConfigurationInstances nc = (FragmentActivity.NonConfigurationInstances)this.getLastNonConfigurationInstance();
        if(nc != null) {
            this.mFragments.restoreLoaderNonConfig(nc.loaders);
        }

        if(savedInstanceState != null) {
            Parcelable p = savedInstanceState.getParcelable("android:support:fragments");
            this.mFragments.restoreAllState(p, nc != null?nc.fragments:null);
        }

        this.mFragments.dispatchCreate();
    }
    
可以看出，如果savedInstanceState不为空，并且，缓存了“android:support:fragments”所对应的Fragments，系统会重新恢复Fragment，恢复过程中，最终会调用

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

从Fragment f = (Fragment)clazz.newInstance();也可以看出为需要保留Framgent的默认构造方法。重新创建Framgent之后会返回FragmentActivity，并通过this.mFragments.dispatchCreate();将Framgent设置为onCreated状态。此时正是新建，还未显示。如何显示呢？其实可以有两个Fragment处于onResume状态的。




<a name="life_circle"></a>  

####  所谓Fragment生命周期是依托FragmentActivity的



<a name="lFragmentTabHost_restore_life"></a>

####  FragmentTabHost的后天杀死重建 

<a name="FragmentPagerAdapter_restore"> </a>

####  FragmentPagerAdapter的后台杀死重建    
       
<a name="how_to_resolve"> </a>   
 
####  后台杀死处理方式--如何处理FragmentActivity的后台杀死重建

                
* 最简单的方式，但是效率可能一般，取消系统恢复，每次恢复的时候，避免系统重建做法如下

如果是supportv4中的FragmentActivity

    @Override
    protected void onSaveInstanceState(Bundle outState) {
          super.onSaveInstanceState(outState);   
           outState.putParcelable("android:support:fragments", null); 
    }
   
或者

    protected void onCreate(Bundle savedInstanceState) {
	     if (savedInstanceState != null) {
	     savedInstanceState.putParcelable(“android:support:fragments”, null);}
	     super.onCreate(savedInstanceState);
	}  

如果是系统的Actvity改成是“android:fragments"
 
* 手动选择处理方式，



<a name="Fragment_bugs"> </a>   

####  Fragment使用很多坑，尤其是被后台杀死后恢复     

<a name="end"> </a>   
    
####  结束语  

 


#### 应用何时会被后台杀死

在近期的任务列表里面，有些不是主动结束掉的任务，会因为内存紧张等原因被后台杀死。

PhoneWindowManager 

	 List<ActivityManager.RecentTaskInfo> recentTasks = am  
	                .getRecentTasks(MAX_RECENT_TASKS,  
	                        ActivityManager.RECENT_IGNORE_UNAVAILABLE);  
	                                                。。。
	/*
	 * 
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
 
 

###  原理，


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

#### Activity退回后台，不退出应用 False跟Activity

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

<a name="Can_not_onSaveInstanceState"/>
	        
#### 	Fragment Transactions & Activity State Loss  解决IllegalStateException: Can not perform this action after onSaveInstanceState     

   

大致意思是说 commit方法是在Activity的onSaveInstanceState()之后调用的，这样会出错，因为onSaveInstanceState，方法是在该Activity即将被销毁前调用，来保存Activity数据的，如果在保存玩状态后再给它添加Fragment就会出错。解决办法就是把commit（）方法替换成 commitAllowingStateLoss()就行了，其效果是一样的。
	        	       
Dispatch onResume() to fragments. Note that for better inter-operation with older versions of the platform, at the point of this call the fragments attached to the activity are not resumed. This means that in some cases the previous state may still be saved, not allowing fragment transactions that modify the state. To correctly interact with fragments in their proper state, you should instead override onResumeFragments()
	        	       
官方文档 对FragmentActivity.onResume的解释：将onResume() 分发给fragment。注意，为了更好的和旧版本兼容，这个方法调用的时候，依附于这个activity的fragment并没有到resumed状态。着意味着在某些情况下，前面的状态可能被保存了，此时不允许fragment transaction再修改状态。从根本上说，你不能确保activity中的fragment在调用Activity的OnResume函数后是否是onresumed状态，因此你应该避免在执行fragment transactions直到调用了onResumeFragments函数。
总的来说就是，你无法确定activity当前的fragment在activity onResume的时候也跟着resumed了，因此要避免在onResumeFragments之前进行fragment transaction，因为到onResumeFragments的时候，状态已经恢复并且它们的确是resumed了的。


	        	        
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
 
 
 
>  对于FragmentTabhost

 
	 final class FragmentManagerImpl extends FragmentManager implements LayoutInflaterFactory {  
	 
	     int mCurState = Fragment.INITIALIZING;        
	     


    public void addTab(TabHost.TabSpec tabSpec, Class<?> clss, Bundle args) {
        tabSpec.setContent(new DummyTabFactory(mContext));
        String tag = tabSpec.getTag();

        TabInfo info = new TabInfo(tag, clss, args);

        if (mAttached) {
            // If we are already attached to the window, then check to make
            // sure this tab's fragment is inactive if it exists.  This shouldn't
            // normally happen.
            info.fragment = mFragmentManager.findFragmentByTag(tag);
            if (info.fragment != null && !info.fragment.isDetached()) {
                FragmentTransaction ft = mFragmentManager.beginTransaction();
                ft.detach(info.fragment);
                ft.commit();
            }
        }

        mTabs.add(info);
        addTab(tabSpec);
    }
    
重建之后，不会再次重建，会根据Tag查找到 ，但是如果，你主动重建，就会重复 。

> 对于FragmentPagerAdapter

    @Override
    public Object instantiateItem(ViewGroup container, int position) {
        if (mCurTransaction == null) {
            mCurTransaction = mFragmentManager.beginTransaction();
        }

        final long itemId = getItemId(position);

        // Do we already have this fragment?
        String name = makeFragmentName(container.getId(), itemId);
        Fragment fragment = mFragmentManager.findFragmentByTag(name);
        if (fragment != null) {
            if (DEBUG) Log.v(TAG, "Attaching item #" + itemId + ": f=" + fragment);
            mCurTransaction.attach(fragment);
        } else {
            fragment = getItem(position);
            if (DEBUG) Log.v(TAG, "Adding item #" + itemId + ": f=" + fragment);
            mCurTransaction.add(container.getId(), fragment,
                    makeFragmentName(container.getId(), itemId));
        }
        if (fragment != mCurrentPrimaryItem) {
            fragment.setMenuVisibility(false);
            fragment.setUserVisibleHint(false);
        }

        return fragment;
    }
         
 Viewpager跟Fragmenttabhost他们会自己处理，

   

<a name="ref_doc"/>
	        
###  参考文档

[Fragment Transactions & Activity State Loss](http://www.androiddesignpatterns.com/2013/08/fragment-transaction-commit-state-loss.html)精      

[Lowmemorykiller笔记](http://blog.csdn.net/guoqifa29/article/details/45370561) **精** 

[Fragment实例化，Fragment生命周期源码分析](http://johnnyyin.com/2015/05/19/android-fragment-life-cycle.html)

[ android.app.Fragment$InstantiationException的原因分析](http://blog.csdn.net/sun927/article/details/46629919)

[Android Framework架构浅析之【近期任务】](http://blog.csdn.net/lnb333666/article/details/7869465)

[Android Low Memory Killer介绍](http://mysuperbaby.iteye.com/blog/1397863)

 
[Android开发之InstanceState详解]( http://www.cnblogs.com/hanyonglu/archive/2012/03/28/2420515.html )

[Square：从今天开始抛弃Fragment吧！](http://www.jcodecraeer.com/a/anzhuokaifa/androidkaifa/2015/0605/2996.html)

[对Android近期任务列表（Recent Applications）的简单分析](http://www.cnblogs.com/coding-way/archive/2013/06/05/3118732.html)

[ Android——内存管理-lowmemorykiller 机制](http://blog.csdn.net/jscese/article/details/47317765)    

[ ActivityStackSupervisor分析](http://blog.csdn.net/guoqifa29/article/details/40015127)