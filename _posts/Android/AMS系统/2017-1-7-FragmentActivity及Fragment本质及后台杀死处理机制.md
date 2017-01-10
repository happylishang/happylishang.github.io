---
layout: post
title: "Android后台杀死系列之--Fragment本质及FragmentActivity后台杀死处理机制"
description: "Java"
category: android开发

---
 
 
App在后台久置后，再次从桌面或最近的任务列表唤醒时经常会发生崩溃，这往往是App在后台被系统杀死，再次恢复的时候遇到了问题，而在使用FragmentActivity+Fragment的时候，经常会遇到：比如Fragment没有提供默认构造方法，就会重建的时候因为反射创建Fragment失败而崩溃，再比如，在onCreate里面new 一个FragmentDialog，并且show，在被后台杀死，再次唤醒的时候，就会show两个对话框，这是为什么？其实这就涉及了后台杀死及恢复的机制，其中涉及的知识点主要是FragmentActivity、ActivityManagerService、LowMemoryKiller机制、ActivityStack、Binder等一系列知识点。放在一篇文章里面可能会有些长，因此，Android后台杀死系列写了三篇：

* 开篇：FragmentActivity的onSaveInstance与onRestorInstance
* 原理篇1：后台杀死与LowmemoryKiller(主要讲述App被后台杀死的原理)
* 原理篇2：后台杀死与恢复(主要讲述App恢复重建的流程及原理)

本篇是Android后台杀死系列的第一篇，主要讲解在开发过程中，由于后台杀死涉及的一些崩溃，以及如何避免这些崩溃，还有就是简单的介绍一下onSaveInstanceState与onRestoreInstanceState执行时机与原理，这两个函数也是Android面试时常问的两个点，是比简单的启动模式Activity声明周期稍微更深入细致一些的地方，也通过这个点引入后台杀死及恢复原理。


<a name="fragment_activity_restore"></a>

# FragmentActivity被后台杀死后恢复逻辑

当App被后台异常杀死后，再次点击icon，或者从最近任务列表进入的时候，系统会帮助恢复当时的场景，重新创建Activity，对于FragmentActivity，由于其中有Framgent，逻辑会相对再复杂一些，系统会首先重建被销毁的Fragment。看FragmentActivity的onCreat代码： 


## 举个栗子

我们创建一个Activity，并且在onCreate函数中新建并show一个DialogFragment，之后通过某种方式将APP异常杀死([RogueKiller模拟后台杀死工具](http://sj.qq.com/myapp/detail.htm?apkName=com.snail.roguekiller))，再次从最近的任务唤起App的时候，会发现显示了两个DialogFragment，代码如下：

	public class DialogFragmentActivity extends AppCompatActivity {
	
	    @Override
	    protected void onCreate(Bundle savedInstanceState) {
	        super.onCreate(savedInstanceState);
	        DialogFragment dialogFragment = new FragmentDlg();
	        dialogFragment.show(getSupportFragmentManager(), "");
	    }
	    
这不仅让我们奇怪，为什么呢？虽然被杀死了，但是onCreate函数在执行的时候还是只执行了一次啊，为什么会出现两个DialogFragment，这里其实就有一个DialogFragment是通过Android自身的恢复重建机制重建出来，在异常杀死的情况下onCreate(Bundle savedInstanceState)函数的savedInstanceState参数也不是null，而是包含了被杀死时所保存的场景信息。再来看个崩溃的例子，新建一个CrashFragment，并且丢弃默认无参构造方法：

	public class CrashFragment extends Fragment {
	
	    public CrashFragment(String tag) {
	        super();
	    }
	}

之后再Activity中Add或replace添加这个CrashFragment，在CrashFragment显示后，通过[RogueKiller模拟后台杀死工具](http://sj.qq.com/myapp/detail.htm?apkName=com.snail.roguekiller)模拟后台杀死，再次从最近任务列表里唤起App的时候，就会遇到崩溃，
    
    Caused by: android.support.v4.app.Fragment$InstantiationException: 
	  Unable to instantiate fragment xxx.CrashFragment: 
	  make sure class name exists, is public, and has an empty constructor that is public
			at android.support.v4.app.Fragment.instantiate(Fragment.java:431)
			at android.support.v4.app.FragmentState.instantiate(Fragment.java:102)
			at android.support.v4.app.FragmentManagerImpl.restoreAllState(FragmentManager.java:1952)
			at android.support.v4.app.FragmentController.restoreAllState(FragmentController.java:144)
			at android.support.v4.app.FragmentActivity.onCreate(FragmentActivity.java:307)
			at android.support.v7.app.AppCompatActivity.onCreate(AppCompatActivity.java:81)
 

上面的这两个问题主要涉及后台杀死后FragmentActivity自身的恢复机制，其实super.onCreate(savedInstanceState)在恢复时做了很多我们没有看到的事情，先看一下崩溃：

## 为什么Fragment没有无参构造方法会引发崩溃

看一下support-V4中FragmentActivity中onCreate代码如下：

    protected void onCreate(@Nullable Bundle savedInstanceState) {
        mFragments.attachHost(null /*parent*/);

        super.onCreate(savedInstanceState);
						...
        if (savedInstanceState != null) {
            Parcelable p = savedInstanceState.getParcelable(FRAGMENTS_TAG);
            mFragments.restoreAllState(p, nc != null ? nc.fragments : null);
        }
        mFragments.dispatchCreate();
    }

 可以看到如果savedInstanceState != null，就会执行mFragments.restoreAllState逻辑，其实这里就牵扯到恢复时重建逻辑，再被后台异常杀死前，或者说在Activity的onStop执行前，Activity的现场以及Fragment的现场都是已经被保存过的，其实是被保存早ActivityManagerService中，保存的格式FragmentState，重建的时候，会采用反射机制重新创Fragment
 
    void restoreAllState(Parcelable state, List<Fragment> nonConfig) {
     
     	 ...
             for (int i=0; i<fms.mActive.length; i++) {
            FragmentState fs = fms.mActive[i];
            if (fs != null) {
                Fragment f = fs.instantiate(mHost, mParent);
                mActive.add(f);
        ...
 
其实就是调用FragmentState的instantiate，进而调用Fragment的instantiate，最后通过反射，构建Fragment，也就是，被加到FragmentActivity的Fragment在恢复的时候，会被自动创建，并且采用Fragment的默认无参构造方法，如果没哟这个方法，就会抛出InstantiationException异常，这也是为什么第二个例子中会出现崩溃的原因。
 
 
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
    q
 可以看到场景二提示的errormsg跟抛出的异常是可以对应上的，其实Fragment源码里面也说得很清楚：
 
     /**
     * Default constructor.  <strong>Every</strong> fragment must have an
     * empty constructor, so it can be instantiated when restoring its
     * activity's state.  It is strongly recommended that subclasses do not
     * have other constructors with parameters, since these constructors
     * will not be called when the fragment is re-instantiated; instead,
     * arguments can be supplied by the caller with {@link #setArguments}
     * and later retrieved by the Fragment with {@link #getArguments}.
     * 
     * <p>Applications should generally not implement a constructor.  The
     * first place application code an run where the fragment is ready to
     * be used is in {@link #onAttach(Activity)}, the point where the fragment
     * is actually associated with its activity.  Some applications may also
     * want to implement {@link #onInflate} to retrieve attributes from a
     * layout resource, though should take care here because this happens for
     * the fragment is attached to its activity.
     */
     
    public Fragment() {
    }
 
大意就是，Fragment必须有一个空构造方法，这样才能保证重建流程，并且，Fragment的子类也不推荐有带参数的构造方法，最好采用setArguments来保存参数。下面再来看下为什么会出现两个DialogFragment。
 
## 为什么出现两个DialogFragment

Fragment在被创建之后，如果不通过add或者replace添加到Activity的布局中是不会显示的，在保存现场的时候，也是保存了add的这个状态的，来看一下Fragment的add逻辑：此时被后台杀死，或旋转屏幕，被恢复的DialogFragmentActivity时会出现两个FragmentDialog，一个被系统恢复的，一个新建的。
    
<a name="add_fragment"/>

### Add一个Fragment，并显示的原理--所谓Fragment生命周期

通常我们FragmentActivity使用Fragment的方法如下：假设是在oncreate函数中：

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        	super.onCreate(savedInstanceState);
			Fragment fr = Fragment.instance("")
			getSupportFragmentManager().beginTransaction()
			.add(R.id.container,fr).commit();

其中getSupportFragmentManager返回的是FragmentManager的子类FragmentManagerImpl，FragmentManagerImpl是FragmentActivity的一个内部类，其Fragment的管理逻辑都是由FragmentManagerImpl来处理的，本文是基于4.3，后面的高版本引入了FragmentController其实也只是多了一层封装，原理差别不是太大，有兴趣可以自己分析：

	public class FragmentActivity extends Activity{
		...
	    final FragmentManagerImpl mFragments = new FragmentManagerImpl();
	   ...
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
    
从名字就可以看出，beginTransaction是为FragmentActivity生成一条Transaction（事务），可以执行，也可以反向，作为退栈的一个依据，FragmentTransaction的add函数实现如下，

    public FragmentTransaction add(Fragment fragment, String tag) {
        doAddOp(0, fragment, tag, OP_ADD);//异步操作的，跟Hander类似
        return this;
    }
 
 
    private void doAddOp(int containerViewId, Fragment fragment, String tag, int opcmd) {
        fragment.mFragmentManager = mManager;
		 ...
        Op op = new Op();
        op.cmd = opcmd;
        op.fragment = fragment;
        addOp(op);
    }
        
之后commit这个Transaction,  将Transaction插入到Transaction队列中去，最终会回调FragmentManager的addFragment方法，将Fragment添加FragmentManagerImpl到维护Fragment列表中去，并且根据当前的Activity状态，将Fragment调整到合适的状态，代码如下：

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

为什么说FragmentManager是FragmentActivity管理Fragment的核心呢，请看下面：

	final class FragmentManagerImpl extends FragmentManager implements LayoutInflaterFactory {
		...
		
	    ArrayList<Runnable> mPendingActions;
	    Runnable[] mTmpActions;
	    boolean mExecutingActions;
	    
	    ArrayList<Fragment> mActive;
	    ArrayList<Fragment> mAdded;
	    ArrayList<Integer> mAvailIndices;
	    ArrayList<BackStackRecord> mBackStack;
	 
	    
可以看出FragmentManagerImpl帮FragmentActivity维护着所有管理Fragment的列表，FragmentManagerImpl的State是和Activity的State一致的，这是管理Fragment的关键。其实Fragment自身是没有什么生命周期的，它只是一个View的封装，完全依靠FragmentManagerImpl来进行同步模拟生命周期，比如在onCreate函数中创建Fragment，add后，在执行的到Activity自身的onCreateView之前，Fragment的onCreateView是不会执行的，也就是Fragment是被动式的跟FragmentActivity保持一致。既然Fragment只是个View的封装，那么它是如何转换成View，并添加到Container中去的呢？关键是moveToState函数，这个函数强制将新add的Fragment的生命周期与Activity同步：

     void moveToState(Fragment f, int newState, int transit, int transitionStyle,
            boolean keepActive) {
            ...        
         if (f.mState < newState) { //低于当前Activity的状态
            switch (f.mState) {
                case Fragment.INITIALIZING:
						...
                    f.mActivity = mActivity;
                    f.mParentFragment = mParent;
                    f.mFragmentManager = mParent != null
                            ? mParent.mChildFragmentManager : mActivity.mFragments;
                    f.mCalled = false;
                    f.onAttach(mActivity);
                   ...
                    if (!f.mRetaining) {
                        f.performCreate(f.mSavedFragmentState);
                    } 
                case Fragment.CREATED:
                    if (newState > Fragment.CREATED) {
                
                          f.mView = f.performCreateView(f.getLayoutInflater(
                          f.mSavedFragmentState), container, f.mSavedFragmentState);
                          f.onViewCreated(f.mView, f.mSavedFragmentState);
                     
                        f.performActivityCreated(f.mSavedFragmentState);
                        if (f.mView != null) {
                            f.restoreViewState(f.mSavedFragmentState);
                        }
                        f.mSavedFragmentState = null;
                    }
	            case Fragment.ACTIVITY_CREATED:
	            case Fragment.STOPPED:
	                    if (newState > Fragment.STOPPED) {
	                        f.performStart();
	                    }
                case Fragment.STARTED:
                    if (newState > Fragment.STARTED) {
           	           f.mResumed = true;
                        f.performResume();

可以看出，add Fragment之后，需要让Fragment跟当前Activity的State保持一致。现在回归正题，对于后台杀死状态下，为什么会show两个DialogFragment呢，我们需要接着看就要Fragment的异常处理的流程，在Fragment没有无参构造方法会引发崩溃里面，分析只是走到了Fragment的构建，现在接着往下走。提供无参构造函数后，Fragment可以正确的新建出来，之后呢？之后就是一些恢复逻辑，接着看restoreAllState

    void restoreAllState(Parcelable state, ArrayList<Fragment> nonConfig) {
 
        if (state == null) return;
        FragmentManagerState fms = (FragmentManagerState)state;
        mActive = new ArrayList<Fragment>(fms.mActive.length);
         for (int i=0; i<fms.mActive.length; i++) {
            FragmentState fs = fms.mActive[i];
            if (fs != null) {
                Fragment f = fs.instantiate(mActivity, mParent);
 
                mActive.add(f);
                fs.mInstance = null;

        // Build the list of currently added fragments.
        if (fms.mAdded != null) {
            mAdded = new ArrayList<Fragment>(fms.mAdded.length);
            for (int i=0; i<fms.mAdded.length; i++) {
                Fragment f = mActive.get(fms.mAdded[i]);
                if (f == null) {
                    throwException(new IllegalStateException(
                            "No instantiated fragment for index #" + fms.mAdded[i]));
                }
                f.mAdded = true;
                if (DEBUG) Log.v(TAG, "restoreAllState: added #" + i + ": " + f);
                if (mAdded.contains(f)) {
                    throw new IllegalStateException("Already added!");
                }
                mAdded.add(f);
            }
        
        // Build the back stack.
        if (fms.mBackStack != null) {
            mBackStack = new ArrayList<BackStackRecord>(fms.mBackStack.length);
            for (int i=0; i<fms.mBackStack.length; i++) {
                BackStackRecord bse = fms.mBackStack[i].instantiate(this);
 
                mBackStack.add(bse);
                if (bse.mIndex >= 0) {
                    setBackStackIndex(bse.mIndex, bse);
    }
   
其实到现在现在Fragment相关的信息已经恢复成功了，之后随着FragmentActivity周期显示或者更新了，这些都是被杀死后，在FragmentActiivyt的onCreate函数处理的，也就是默认已经将之前的Fragment添加到mAdded列表中去了，但是，在场景一，我们有手动新建了一个Fragment，并添加进去，所以，mAdded函数中就有连个两个Fragment。这样，在FragmentActivity调用onStart函数之后，会新建mAdded列表中Fragment的视图，将其添加到相应的container中去，并在Activity调用onReusume的时候，显示出来做的，这个时候，就会显示两份，其实如果，在这个时候，你再杀死一次，恢复，就会显示三分，在杀死，重启，就是四份。。。。

    @Override
    protected void onStart() {
        super.onStart();

        mStopped = false;
        mReallyStopped = false;
        mHandler.removeMessages(MSG_REALLY_STOPPED);

        if (!mCreated) {
            mCreated = true;
            mFragments.dispatchActivityCreated();
        }

        mFragments.noteStateNotSaved();
        mFragments.execPendingActions();

        mFragments.doLoaderStart();

        // NOTE: HC onStart goes here.

        mFragments.dispatchStart();
        mFragments.reportLoaderStart();
    }
    
以上就是针对两个场景，对FramgentActivity的一些分析，主要是回复时候，对于Framgent的一些处理。     

<a name="onSaveInstanceState_OnRestoreInstance"/>

# onSaveInstanceState与OnRestoreInstance的调用时机 

在在点击home键，或者跳转其他界面的时候，都会回调用onSaveInstanceState，但是再次唤醒却不一定调用OnRestoreInstance,这是为什么呢？onSaveInstanceState与OnRestoreInstance难道不是配对使用的？在Android中，onSaveInstanceState是为了预防Activity被后台杀死的情况做的预处理，如果Activity没有被后台杀死，那么自然也就不需要进行现场的恢复，也就不会调用OnRestoreInstance，而大多数情况下，Activity不会那么快被杀死。

## onSaveInstanceState的调用时机

onSaveInstanceState函数是Android针对可能被后台杀死的Activity做的一种预防，它的执行时机在2.3之前是在onPause之前，2.3之后，放在了onStop函数之前，也就说Activity失去焦点后，可能会由于内存不足，被回收的情况下，都会去执行onSaveInstanceState。对于startActivity函数的调用很多文章都有介绍，可以简单参考下老罗的博客[Android应用程序内部启动Activity过程（startActivity）的源代码分析](http://blog.csdn.net/luoshengyang/article/details/6703247)，比如在Activity A 调用startActivity启动Activity B的时候，会首先通过AMS  pause Activity A，之后唤起B，在B显示，再stop A，在stop A的时候，需要保存A的现场，因为不可见的Activity都是可能被后台杀死的，比如，在开发者选项中打开**不保留活动**，就会达到这种效果，在启动另一个Activity时，上一个Activity的保存流程大概如下，这里先简单描述，在下一篇原理篇的时候，会详细讲解下流程：

![恢复启动流程.png](http://upload-images.jianshu.io/upload_images/1460468-d21d44117662ccc3.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

在2.3之后，onSaveInstanceState的时机都放在了onStop之前，看一下FragmentActivity的onSaveInstanceState源码：

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        Parcelable p = mFragments.saveAllState();
        if (p != null) {
            outState.putParcelable(FRAGMENTS_TAG, p);
        }
    }

可以看出，首先就是父类的onSaveInstanceState，主要是保存一些窗口及View的信息，比如ViewPager当前显示的是第几个View等。之后，就是就是通过FragmentManager的saveAllState，来保存FragmentActivity自身的现场-Fragment的一些状态，这些数据是FragmentActivity恢复Framgent所必须的数据，处理不好就会出现上面的那种异常。

## OnRestoreInstanceState的调用时机

之前已经说过，OnRestoreInstanceState虽然与onSaveInstanceState是配对实现的，但是其调用却并非完全成对的，在Activity跳转或者返回主界面时，onSaveInstanceState是一定会调用的，但是OnRestoreInstanceState却不会，它只有Activity或者App被异常杀死，走恢复流程的时候才会被调用。如果没有被异常杀死，不走Activity的恢复新建流程，也就不会回调OnRestoreInstanceState，简单看一下Activity的加载流程图：

![onRestoreInstance调用时机.png](http://upload-images.jianshu.io/upload_images/1460468-5d8135f45ecee77f.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看出，OnRestoreInstanceState的调用时机是在onStart之后，在onPostCreate之前。那么正常的创建为什么没调用呢？看一下ActivityThread中启动Activity的源码：

	 private Activity performLaunchActivity(Activi
	         
	         ...
	          mInstrumentation.callActivityOnCreate(activity, r.state);
	          	   
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
	            
	               }
         }

可以看出，只有r.state != null的时候，才会通过mInstrumentation.callActivityOnRestoreInstanceState回调OnRestoreInstanceState，这里的r.state就是ActivityManagerService通过Binder传给ActivityThread数据，主要用来做场景恢复。以上就是onSaveInstanceState与OnRestoreInstance执行时机的一些分析。下面结合具体的系统View空间来分析一下使用：比如ViewPager与FragmentTabHost，这两个空间是主界面最常用的控件，内部对后台杀死做了兼容，这也是为什么被杀死后，Viewpager在恢复后，能自动定位到上次浏览的位置。


# 一些系统View控件对后台杀死做的兼容


##  FragmentTabHost的后台杀死重建 onRestoreInstanceState、onAttachedToWindow

系统在onCreate回复Fragment之后，会首先调用onRestoreInstanceState恢复数据，之后会调用onAttachedToWindow添加到窗口显示，在onRestoreInstanceState会将当前postion重新赋值给Tabhost，在onAttachedToWindow时，就可以根据它设置当前位置。

    @Override
    protected Parcelable onSaveInstanceState() {
        Parcelable superState = super.onSaveInstanceState();
        SavedState ss = new SavedState(superState);
        ss.curTab = getCurrentTabTag();
        return ss;
    }

    @Override
    protected void onRestoreInstanceState(Parcelable state) {
        if (!(state instanceof SavedState)) {
            super.onRestoreInstanceState(state);
            return;
        }
        SavedState ss = (SavedState) state;
        super.onRestoreInstanceState(ss.getSuperState());
        setCurrentTabByTag(ss.curTab);
    }
    
在onAttachedToWindow时候，会首先调用mFragmentManager.findFragmentByTag，被后台杀死后，这里能获取到相应的Fragment，因此不用重建。那些本来就没点击过的Tab其实还是null，在doTabChanged才真正的创建。

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

#  ViewPager及FragmentPagerAdapter的后台杀死重建 

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
 
#  后台杀死处理方式--如何处理FragmentActivity的后台杀死重建
                
* 最简单的方式，但是效率可能一般，取消系统恢复，每次恢复的时候，避免系统重建做法如下：

如果是supportv4中的FragmentActivity 

    protected void onCreate(Bundle savedInstanceState) {
	     if (savedInstanceState != null) {
	     savedInstanceState.putParcelable(“android:support:fragments”, null);}
	     super.onCreate(savedInstanceState);
	}  

如果是系统的Actvity改成是“android:fragments"，不过这里需要注意：对于ViewPager跟FragmentTabHost不需要额外处理，处理了可能反而有反作用。




 

<a name="Can_not_onSaveInstanceState"/>
	        
# 	Fragment Transactions & Activity State Loss  解决IllegalStateException: Can not perform this action after onSaveInstanceState     
 

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

	
# 为什么Fragment必须提供默认构造方法 

后台杀死后，FragmentManager会根据反射机制重建Fragment实例，此时采用的是默认无参构造函数

	   void restoreAllState(Parcelable state, ArrayList<Fragment> nonConfig) {	  ...
	           mActive = new ArrayList<Fragment>(fms.mActive.length);
	        if (mAvailIndices != null) {
	            mAvailIndices.clear();
	        }
	        for (int i=0; i<fms.mActive.length; i++) {
	            FragmentState fs = fms.mActive[i];
	            if (fs != null) {
	                Fragment f = fs.instantiate(mActivity, mParent);
 	
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
  
#   Viewpager与FragmentTabhost的恢复逻辑，

Viewpager与Fragmenttabhost有自己的恢复逻辑，当然这些都是在FramgentManager恢复完FragmentActivity之后，在Android 3.0之前，系统只会恢复Activity内部的View的状态

对于FragmentTabhost，重建之后，不会再次重建，会根据Tag查找到 ，但是如果，你主动重建，就会重复 。

 
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
    

对于Viewpager

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
 


<a name="FragmentPagerAdapter_FragmentStatePagerAdapter"/>

# FragmentPagerAdapter与FragmentStatePagerAdapter的使用场景
 
* FragmentPagerAdapter适用于存在刷新的界面 ，比如列表Fragment，如果采用FragmentStatePagerAdapter就需要保存现场，并且数据的加载会把逻辑弄乱
* FragmentStatePagerAdapter更加适合图片类的处理，笔记图片预览等，一屏幕显示完全的，否则用FragmentStatePagerAdapter只会比FragmentPagerAdapter更复杂，还要自己缓存Fragment列表。

 

 
	        
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