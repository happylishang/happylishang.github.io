---
layout: post
title: "FragmentActivity及Fragment本质及后台杀死处理机制"
description: "Java"
category: android开发

---

# 分析问题的方法与步骤

* **什么时候会出现这个问题**
* **为什么会出现**
* **怎么处理，能解决问题**

###### **前言：Fragment只是Activity更加方便管理View的一种方式**

>  [背景](#background)   
>  [add一个Fragment并显示的原理--及所谓Fragment生命周期](#add_fragment)        
>  [FragmentActivity被后台杀死后恢复逻辑](#fragment_activity_restore)    
>  [FragmentTabHost的后台杀死重建逻辑](#lFragmentTabHost_restore_life)     
>  [ViewPager及FragmentPagerAdapter的后台杀死重建](#FragmentPagerAdapter_restore)          
>  [FragmentPagerAdapter与FragmentStatePagerAdapter的使用时机](#FragmentPagerAdapter_FragmentStatePagerAdapter)
>  [后台杀死处理方式](#how_to_resolve)    
>  [Fragment使用很多坑，尤其是被后台杀死后恢复](#Fragment_bugs)    
>  [onSaveInstanceState与OnRestoreInstance的调用时机](#onSaveInstanceState_OnRestoreInstance)   
>  [Can not perform this action after onSaveInstanceState](#Can_not_onSaveInstanceState)           
>  [结束语](#end)     
>  [参考文档](#ref_doc)    

<a name="background"></a>

#### 背景

开发的时候，虽然一直遵守谷歌的Android开发文档，创建Fragment尽量采用推荐的参数传递方式，并且保留默认的Fragment无参构造方法，避免绝大部分后台杀死-恢复崩溃的问题，但是对于原理的了解紧限于恢复时的重建机制，采用反射机制，并使用了默认的构造参数，直到使用FragmentDialog，示例代码如下：

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

上面的DialogFragmentActivity内部创建了一个FragmentDialog，并显示，如果，此时被后台杀死，或旋转屏幕，被恢复的DialogFragmentActivity时会出现两个FragmentDialog，一个被系统恢复的，一个新建的。这种场景对于普通的Fragment也适用，如果单个Activity采用普通的add方式添加，被后台杀死后恢复，就会有两个Fragment出现。为什么出现两个？

    public void show(FragmentManager manager, String tag) {
        mDismissed = false;
        mShownByMe = true;
        FragmentTransaction ft = manager.beginTransaction();
        ft.add(this, tag);
        ft.commit();
    }
    
DialgoFragment的show逻辑跟Fragment的add其实是一样的，Fragment被add后，如果没有move，就一直是有效，restore后还是会显示的。DialgoFragment不会 add the transaction to the back stack.dismiss的时候 a new transaction will be executed to remove it from the activity.DialogFragment本质上说就是Fragment，只是其内部还有一个dialog而已。你既可以当它是Dialog使用，也可以把它作为Fragment使用，不过界面显示是按照Dialog显示的

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.dialog_f_v, container, false);
    }
    
Fragment被添加到Activity中管理，但是View没有，View 被添加到Dialog中去了，因为 ViewGroup container这里其实container是null。正因为是null，

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        if (!mShowsDialog) {
            return;
        }

        View view = getView();
        if (view != null) {
            if (view.getParent() != null) {
                throw new IllegalStateException("DialogFragment can not be attached to a container view");
            }
            mDialog.setContentView(view);
        }

后面的getView获取的View是可以通过mDialog.setContentView添加到Dialog中去的， 其实是个dialog，管理采用的却是Fragment

<a name="add_fragment"/>

#### Add一个Fragment并显示的原理--所谓Fragment生命周期

通常我们FragmentActivity使用Fragment的方法如下：假设是在oncreate函数中：

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        	super.onCreate(savedInstanceState);
			Fragment fr = Fragment.instance("")
			getSupportFragmentManager().beginTransaction()
			.add(R.id.container,fr).commit();

其中	getSupportFragmentManager返回的是 FragmentManagerImpl，FragmentManagerImpl是FragmentActivity的一个内部类，其实Android无处不采用了设计模式，FragmentActivity把管理的逻辑交给FragmentManagerImpl：

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
        doAddOp(0, fragment, tag, OP_ADD);//异步操作的，跟Hander类似
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
	    
可以看出FragmentManagerImpl维护一个Activity所有的Fragment，Fragments可以看做是M，V是Activity自身。FragmentManagerImpl的State是和Activity的State一致的，这是管理Fragment的关键。其实Fragment自身是没有什么生命周期的，完全依靠FragmentManagerImpl模拟。fragment.mFragmentManager都会指向Activity中唯一的FragmentManager，其实对于每个add，Android都将他们封装成一个度里的Action，在每个Action内部自己处理自己的逻辑，这个做法值得学习，


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
    
这里看一下添加Fragment转换成View，并添加到Container的代码，其实Fragment只是View的一个比较复杂的封装，FragmentManager最后将Fragment在Activity中显示出来。所谓Fragment生命周期是依托FragmentActivity的


     void moveToState(Fragment f, int newState, int transit, int transitionStyle,
            boolean keepActive) {
        // Fragments that are not currently added will sit in the onCreate() state.
        if ((!f.mAdded || f.mDetached) && newState > Fragment.CREATED) {
            newState = Fragment.CREATED;
        }
        
	switch (f.mState) {
	                case Fragment.INITIALIZING:
	                    if (DEBUG) Log.v(TAG, "moveto CREATED: " + f);
	                    if (f.mSavedFragmentState != null) {
	                        f.mSavedFragmentState.setClassLoader(mActivity.getClassLoader());
	                        f.mSavedViewState = f.mSavedFragmentState.getSparseParcelableArray(
	                                FragmentManagerImpl.VIEW_STATE_TAG);
	                        f.mTarget = getFragment(f.mSavedFragmentState,
	                                FragmentManagerImpl.TARGET_STATE_TAG);
	                        if (f.mTarget != null) {
	                            f.mTargetRequestCode = f.mSavedFragmentState.getInt(
	                                    FragmentManagerImpl.TARGET_REQUEST_CODE_STATE_TAG, 0);
	                        }
	                        f.mUserVisibleHint = f.mSavedFragmentState.getBoolean(
	                                FragmentManagerImpl.USER_VISIBLE_HINT_TAG, true);
	                        if (!f.mUserVisibleHint) {
	                            f.mDeferStart = true;
	                            if (newState > Fragment.STOPPED) {
	                                newState = Fragment.STOPPED;
	                            }
	                        }
	                    }
	                    f.mActivity = mActivity;
	                    f.mParentFragment = mParent;
	                    f.mFragmentManager = mParent != null
	                            ? mParent.mChildFragmentManager : mActivity.mFragments;
	                    f.mCalled = false;
	                    f.onAttach(mActivity);
	                    if (!f.mCalled) {
	                        throw new SuperNotCalledException("Fragment " + f
	                                + " did not call through to super.onAttach()");
	                    }
	                    if (f.mParentFragment == null) {
	                        mActivity.onAttachFragment(f);
	                    }
	
	                    if (!f.mRetaining) {
	                        f.performCreate(f.mSavedFragmentState);
	                    }
	                    f.mRetaining = false;
	                    if (f.mFromLayout) {
	                        // For fragments that are part of the content view
	                        // layout, we need to instantiate the view immediately
	                        // and the inflater will take care of adding it.
	                        f.mView = f.performCreateView(f.getLayoutInflater(
	                                f.mSavedFragmentState), null, f.mSavedFragmentState);
	                        if (f.mView != null) {
	                            f.mInnerView = f.mView;
	                            if (Build.VERSION.SDK_INT >= 11) {
	                                ViewCompat.setSaveFromParentEnabled(f.mView, false);
	                            } else {
	                                f.mView = NoSaveStateFrameLayout.wrap(f.mView);
	                            }
	                            if (f.mHidden) f.mView.setVisibility(View.GONE);
	                            f.onViewCreated(f.mView, f.mSavedFragmentState);
	                        } else {
	                            f.mInnerView = null;
	                        }
	                    }
	                case Fragment.CREATED:
	                    if (newState > Fragment.CREATED) {
	                        if (DEBUG) Log.v(TAG, "moveto ACTIVITY_CREATED: " + f);
	                        if (!f.mFromLayout) {
	                            ViewGroup container = null;
	                            if (f.mContainerId != 0) {
	                                container = (ViewGroup)mContainer.findViewById(f.mContainerId);
	                                if (container == null && !f.mRestored) {
	                                    throwException(new IllegalArgumentException(
	                                            "No view found for id 0x"
	                                            + Integer.toHexString(f.mContainerId) + " ("
	                                            + f.getResources().getResourceName(f.mContainerId)
	                                            + ") for fragment " + f));
	                                }
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
	                                if (f.mHidden) f.mView.setVisibility(View.GONE);
	                                f.onViewCreated(f.mView, f.mSavedFragmentState);
	                            } else {
	                                f.mInnerView = null;
	                            }
	                        }
	
	                        f.performActivityCreated(f.mSavedFragmentState);
	                        if (f.mView != null) {
	                            f.restoreViewState(f.mSavedFragmentState);
	                        }
	                        f.mSavedFragmentState = null;
	                    }
	                case Fragment.ACTIVITY_CREATED:                              

注意上面一些State的变化，跟当前Activity的State保持一致，之后根据当前Activity的状态，决定是否显示Fragment，这里是正常的流程，至于后台杀死，就要看第二个异常处理的流程。newState代表是当前Actvity传递给的FragmentManager的state，位于FragmentManager中，FragmentManager可以看做是FragmentActvity的管理器C，Fragmentmanager会根据mCurState的值，修改当前别添加的fragment的状态，如果是Actvity处于resume状态，那么被添加的fragment就会被处理成激活状态 当然首先要初始化新建的fragment ,然后匹配新状态，是否有必要将状态等级提升，很明显，没有被added或者或者说已经detach的Fragment是不用走到resume的


        // Fragments that are not currently added will sit in the onCreate() state.
        if ((!f.mAdded || f.mDetached) && newState > Fragment.CREATED) {
            newState = Fragment.CREATED;
        }        
            
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



<a name="lFragmentTabHost_restore_life"></a>

####  FragmentTabHost的后台杀死重建 onRestoreInstanceState、onAttachedToWindow

onRestoreInstanceState之后，会调用onAttachedToWindow，

在onAttachedToWindow时候，会首先调用mFragmentManager.findFragmentByTag，被后台杀死后，这里能获取到相应的Fragment，因此不用重建。其实

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        String currentTab = getCurrentTabTag();

        // Go through all tabs and make sure their fragments match
        // the correct state.
        FragmentTransaction ft = null;
        for (int i=0; i<mTabs.size(); i++) {
            TabInfo tab = mTabs.get(i);
            tab.fragment = mFragmentManager.findFragmentByTag(tab.tag);
            if (tab.fragment != null && !tab.fragment.isDetached()) {
                if (tab.tag.equals(currentTab)) {
                    // The fragment for this tab is already there and
                    // active, and it is what we really want to have
                    // as the current tab.  Nothing to do.
                    mLastTab = tab;
                } else {
                    // This fragment was restored in the active state,
                    // but is not the current tab.  Deactivate it.
                    if (ft == null) {
                        ft = mFragmentManager.beginTransaction();
                    }
                    ft.detach(tab.fragment);
                }
            }
        }

        // We are now ready to go.  Make sure we are switched to the
        // correct tab.
        mAttached = true;
        ft = doTabChanged(currentTab, ft);
        if (ft != null) {
            ft.commit();
            mFragmentManager.executePendingTransactions();
        }
    }



<a name="FragmentPagerAdapter_restore"> </a>

####  ViewPager及FragmentPagerAdapter的后台杀死重建 

ViewPager的情形注意serCurrent，如果设置了一次，后台杀死后，重建ViewPager，恢复现场，调用setCurrent。如果手动将android.support.fragments置空，很容易引发崩溃。其实ViewPager默认支持重建，但是如果MVP开发Presenter就要注意是否合理的被创建。有些场景，如果手动清理android.support.fragments，就会引起崩溃，因为ViewPager也会保存现场，如果置空，重建就会遇到问题，当然如果在onCreate中已经添加了Fragment的除外。比如那些先网络请求，再更新PagerAdapter的，数量是动态的那种，就会出现问题。

	  at android.support.v4.app.FragmentManagerImpl.getFragment(SourceFile:587)
	       at android.support.v4.app.FragmentStatePagerAdapter.restoreState(SourceFile:211)
	       at android.support.v4.view.ViewPager.onRestoreInstanceState(SourceFile:1318)
	       at android.view.View.dispatchRestoreInstanceState(View.java:14770)
	       
ViewPager的PagerAdapter如何复用被杀死的Pager，并且不引起崩溃？菜单栏刷新，如何处理

    @Override
    public HomeFragmentItem getItem(int position) {
        HomeFragmentItem fragment = null;
        if (mFragmentHashMap.get(position) == null) {
            Class frgClass = mFragments[position];
            try {
                frgClass.newInstance();
                fragment = (HomeFragmentItem) frgClass.newInstance();
                mFragmentHashMap.put(position, fragment);
            } catch (InstantiationException e) {
                e.printStackTrace();
            } catch (IllegalAccessException e) {
                e.printStackTrace();
            }
        } else {
            fragment = mFragmentHashMap.get(position);
        }
        return fragment;
    }


    @Override
    public Object instantiateItem(ViewGroup container, int position) {
        HomeFragmentItem fragmentItem = (HomeFragmentItem) super.instantiateItem(container, position);
        mFragmentHashMap.put(position, fragmentItem);
        return fragmentItem;
    }
           

mFirstLayout =true 可能是还没有创建Fragment，那么我们就不能获取Fragment，也不能使用里面的东西，但是可以调用dispatchOnPageSelected，至于里面如何操作就不知道了

        if (mFirstLayout) {
            // We don't have any idea how big we are yet and shouldn't have any pages either.
            // Just set things up and let the pending layout handle things.
            
            <!--这里是说 ，可能没有页面，但是页面的回调可以做。其实这里如果牵扯到了Menu等回调，也许还有问题-->
            
            mCurItem = item;
            if (dispatchSelected) {
                dispatchOnPageSelected(item);
            }
            requestLayout();
        } else {
            populate(item);
            scrollToItem(item, smoothScroll, velocity, dispatchSelected);
        }
  
  其实注意创建过程，如果开始FragmentActivity中存在备份，就不用再次getItem。
  
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
  
      private static String makeFragmentName(int viewId, long id) {
        return "android:switcher:" + viewId + ":" + id;
    }        

ViewPager重建，Adapter的设置尽量靠后，如果靠前，并且设置了位置，后台杀死重启可能崩溃，不如网络请求回来动态的处理，再说开始设置一个空的Adapter有意义吗？尤其对于FragmentStateAdapter，更加容易引起bug，毕竟网络请求后，还会再次处理的，如果onCreate里面设置了Adapter，并且Fragment已经确定，那就一定不会有崩溃的问题。

    @Override
    public void onRestoreInstanceState(Parcelable state) {
        if (!(state instanceof SavedState)) {
            super.onRestoreInstanceState(state);
            return;
        }

        SavedState ss = (SavedState)state;
        super.onRestoreInstanceState(ss.getSuperState());

        if (mAdapter != null) {
            mAdapter.restoreState(ss.adapterState, ss.loader);
            setCurrentItemInternal(ss.position, false, true);
        } else {
            mRestoredCurItem = ss.position;
            mRestoredAdapterState = ss.adapterState;
            mRestoredClassLoader = ss.loader;
        }
    }
    
        
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


<a name="FragmentPagerAdapter_FragmentStatePagerAdapter"/>

#### FragmentPagerAdapter与FragmentStatePagerAdapter的使用场景
 
* FragmentPagerAdapter适用于存在刷新的界面 ，比如列表Fragment，如果采用FragmentStatePagerAdapter就需要保存现场，并且数据的加载会把逻辑弄乱
* FragmentStatePagerAdapter更加适合图片类的处理，笔记图片预览等，一屏幕显示完全的，否则用FragmentStatePagerAdapter只会比FragmentPagerAdapter更复杂，还要自己缓存Fragment列表。



####  OnRestoreInstanceState的调用时机是在什么时候？ 保存后，看看是否被杀死，被杀死机会回调，注意，不仅仅是Fragment，还有View，尤其是ViewPager


                mInstrumentation.callActivityOnCreate(activity, r.state);
                if (!activity.mCalled) {
                    throw new SuperNotCalledException(
                        "Activity " + r.intent.getComponent().toShortString() +
                        " did not call through to super.onCreate()");
                }
                r.activity = activity;
                r.stopped = true;
                if (!r.activity.mFinished) {
                    activity.performStart();
                    r.stopped = false;
                }
                if (!r.activity.mFinished) {
                    if (r.state != null) {
                        mInstrumentation.callActivityOnRestoreInstanceState(activity, r.state);
                    }
                }
                if (!r.activity.mFinished) {
                    activity.mCalled = false;
                    mInstrumentation.callActivityOnPostCreate(activity, r.state);
                    if (!activity.mCalled) {
                        throw new SuperNotCalledException(
                            "Activity " + r.intent.getComponent().toShortString() +
                            " did not call through to super.onPostCreate()");
                    }
                }
            }
            
            

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
 

<a name="Can_not_onSaveInstanceState"/>
	        
#### 	Fragment Transactions & Activity State Loss  解决IllegalStateException: Can not perform this action after onSaveInstanceState     

   

大致意思是说 commit方法是在Activity的onSaveInstanceState()之后调用的，这样会出错，因为onSaveInstanceState，方法是在该Activity即将被销毁前调用，来保存Activity数据的，如果在保存玩状态后再给它添加Fragment就会出错。解决办法就是把commit（）方法替换成 commitAllowingStateLoss()就行了，其效果是一样的。
	        	       
	Dispatch onResume() to fragments. Note that for better inter-operation with older versions of the platform, at the point of this call the fragments attached to the activity are not resumed. This means that in some cases the previous state may still be saved, not allowing fragment transactions that modify the state. To correctly interact with fragments in their proper state, you should instead override onResumeFragments()
	        	       
官方文档 对FragmentActivity.onResume的解释：将onResume() 分发给fragment。注意，为了更好的和旧版本兼容，这个方法调用的时候，依附于这个activity的fragment并没有到resumed状态。着意味着在某些情况下，前面的状态可能被保存了，此时不允许fragment transaction再修改状态。从根本上说，你不能确保activity中的fragment在调用Activity的OnResume函数后是否是onresumed状态，因此你应该避免在执行fragment transactions直到调用了onResumeFragments函数。总的来说就是，你无法确定activity当前的fragment在activity onResume的时候也跟着resumed了，因此要避免在onResumeFragments之前进行fragment transaction，因为到onResumeFragments的时候，状态已经恢复并且它们的确是resumed了的。不当的commit场景：**How to avoid the exception?：**在onCreate、或者点击事件中commit transactions是不会产生任何问题的。但是如果transactions的操作涉及其他Activity生命周期方法的话。比如onActivityResult(), onStart(), and onResume()，这些场景就比较棘手，例如不要在onResume里commit transactions，因为onResume有可能在activity’s state has been restored之前调用，

	**Be careful when committing transactions inside Activity lifecycle methods. A large majority of applications will only ever commit transactions the very first time onCreate() is called and/or in response to user input, and will never face any problems as a result. However, as your transactions begin to venture out into the other Activity lifecycle methods, such as onActivityResult(), onStart(), and onResume(), things can get a little tricky. For example, you should not commit transactions inside the FragmentActivity#onResume() method, as there are some cases in which the method can be called before the activity’s state has been restored (see the documentation for more information). If your application requires committing a transaction in an Activity lifecycle method other than onCreate(), do it in either FragmentActivity#onResumeFragments() or Activity#onPostResume(). These two methods are guaranteed to be called after the Activity has been restored to its original state, and therefore avoid the possibility of state loss all together. (As an example of how this can be done, check out my answer to this StackOverflow question for some ideas on how to commit FragmentTransactions in response to calls made to the Activity#onActivityResult() method).**

其次不要在在异步异步回调中处理transactions事件，比如AsyncTask，LoaderManager，回调不关心Activity的状态是否被restore，常见的场景：home键返回主页，会调用onSaveInstanceState() ，onStop()，如果AsyncTask在此之后被执行，就会导致异常，并且从用户体验的角度来说也并不好。

	Avoid performing transactions inside asynchronous callback methods. This includes commonly used methods such as AsyncTask#onPostExecute() and LoaderManager.LoaderCallbacks#onLoadFinished(). The problem with performing transactions in these methods is that they have no knowledge of the current state of the Activity lifecycle when they are called. For example, consider the following sequence of events:
	An activity executes an AsyncTask.The user presses the “Home” key, causing the activity’s onSaveInstanceState() and onStop() methods to be called.The AsyncTask completes and onPostExecute() is called, unaware that the Activity has since been stopped.A FragmentTransaction is committed inside the onPostExecute() method, causing an exception to be thrown.In general, the best way to avoid the exception in these cases is to simply avoid committing transactions in asynchronous callback methods all together. Google engineers seem to agree with this belief as well. According to this post on the Android Developers group, the Android team considers the major shifts in UI that can result from committing FragmentTransactions from within asynchronous callback methods to be bad for the user experience. If your application requires performing the transaction inside these callback methods and there is no easy way to guarantee that the callback won’t be invoked after onSaveInstanceState(), you may have to resort to using commitAllowingStateLoss() and dealing with the state loss that might occur. (See also these two StackOverflow posts for additional hints, here and here).

如果万不得已，可以使用commitAllowingStateLoss()，这只是一种妥协，最好还是改进交互

	Use commitAllowingStateLoss() only as a last resort. The only difference between calling commit() and commitAllowingStateLoss() is that the latter will not throw an exception if state loss occurs. Usually you don’t want to use this method because it implies that there is a possibility that state loss could happen. The better solution, of course, is to write your application so that commit() is guaranteed to be called before the activity’s state has been saved, as this will result in a better user experience. Unless the possibility of state loss can’t be avoided, commitAllowingStateLoss() should not be used.

比较合理的使用方法：If you are using the support-v4 library and FragmentActivity, try to always use onResumeFragments() instead of onResume() in your FragmentActivity implementations.FragmentActivity#onResume() documentation:To correctly interact with fragments in their proper state, you should instead override onResumeFragments().

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
         

 
####   Viewpager跟Fragmenttabhost有自己的回复逻辑，当然这些都是在FramgentManaget恢复完FragmentActivity之后，在Fragment出现前，也就是3.0之前，系统只会恢复Activity内部的View

#####  对于FragmentTabhost

 
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

##### 对于FragmentPagerAdapter

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
    

 
<a name="onSaveInstanceState_OnRestoreInstance"/>

#### onSaveInstanceState与OnRestoreInstance的调用时机 

##### 点击home键为什么返回主菜单会调用onSaveInstanceState，再回来会不会重建，调用OnRestoreInstance呢
一般情况下，是不会的，因为系统不会回收的那么快。其实点击Home键跟Activity跳转的原理是一样的，从Activity A 跳转到Activity B也会调用 A的onSaveInstanceState，但是只要A没有被系统回收掉，就不会调用A的OnRestoreInstance，因为在ActivityManagerService中，A所登记的状态是没有被后台Kill过的。其实Activity所有状态变化的最终依赖都是ActivityManagerService。  

  
####  FragmentTabHost奇葩的毕现 ，点击主屏幕与FragmentTabHost点击事件比较接近的时候崩溃

This problem occurs if tab selection action performs after onSaveInstanceState get called. One example like, if user selects and holds any tab and at the same time also selects the Home Button.To solve this issue just

	call mTabHost.getTabWidget().setEnabled(false); under onPause of the Fragment/Activity
	and call mTabHost.getTabWidget().setEnabled(true); under onResume. 


####  结束语 


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

[A Deeper Look of ViewPager and FragmentStatePagerAdaper](http://billynyh.github.io/blog/2014/03/02/fragment-state-pager-adapter/)

[View的onSaveInstanceState和onRestoreInstanceState过程分析](http://www.cnblogs.com/xiaoweiz/p/3813914.html)