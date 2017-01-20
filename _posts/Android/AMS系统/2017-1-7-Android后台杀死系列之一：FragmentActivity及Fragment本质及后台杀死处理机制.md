---
layout: post
title: "Android后台杀死系列之一：Fragment本质及FragmentActivity后台杀死处理机制"
description: "Java"
category: Android  
image: http://upload-images.jianshu.io/upload_images/1460468-d21d44117662ccc3.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

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

可以看出，只有r.state != null的时候，才通过mInstrumentation.callActivityOnRestoreInstanceState回调OnRestoreInstanceState，r.state就是ActivityManagerService通过Binder传给ActivityThread数据，主要用来做场景恢复。以上就是onSaveInstanceState与OnRestoreInstance执行时机的一些分析。下面结合具体的系统View控件来分析一下这两个函数的具体应用：比如ViewPager与FragmentTabHost，这两个空间是主界面最常用的控件，内部对后台杀死做了兼容，这也是为什么被杀死后，Viewpager在恢复后，能自动定位到上次浏览的位置。


# ViewPager应对后台杀死做的兼容


首先看一下ViewPager做的兼容，ViewPager在后台杀死的情况下，仍然能恢复到上次关闭的位置，这也是对体验的一种优化，这其中的原理是什么？之前分析onSaveInstanceState与onRestoreInstanceState的时候，只关注了Fragment的处理，其实还有一些针对Window窗口及Vie的处理，先看一下onSaveInstanceState针对窗口保存了什么：

    protected void onSaveInstanceState(Bundle outState) {
        outState.putBundle(WINDOW_HIERARCHY_TAG, mWindow.saveHierarchyState());
      }

> PhonwWinow.java

    @Override
    public Bundle saveHierarchyState() {
        Bundle outState = new Bundle();
        if (mContentParent == null) {
            return outState;
        }
        
        SparseArray<Parcelable> states = new SparseArray<Parcelable>();
        mContentParent.saveHierarchyState(states);
        outState.putSparseParcelableArray(VIEWS_TAG, states);

        // save the focused view id
          View focusedView = mContentParent.findFocus();
          ...
          outState.putInt(FOCUSED_ID_TAG, focusedView.getId());
        // save the panels
        if (panelStates.size() > 0) {
            outState.putSparseParcelableArray(PANELS_TAG, panelStates);
        }
        if (mActionBar != null) {
            outState.putSparseParcelableArray(ACTION_BAR_TAG, actionBarStates);
        }

        return outState;
    }
    
Window其实就是PhonwWinow，saveHierarchyState其实就是针对当前窗口中的View保存一些场景信息 ，比如：当前获取焦点的View的id、ActionBar、View的一些状态，当然saveHierarchyState递归遍历所有子View，保存所有需要保存的状态：

> ViewGroup.java


    @Override
    protected void dispatchSaveInstanceState(SparseArray<Parcelable> container) {
        super.dispatchSaveInstanceState(container);
        final int count = mChildrenCount;
        final View[] children = mChildren;
        for (int i = 0; i < count; i++) {
            View c = children[i];
            if ((c.mViewFlags & PARENT_SAVE_DISABLED_MASK) != PARENT_SAVE_DISABLED) {
                c.dispatchSaveInstanceState(container);
            }
        }
    }
    
可见，该函数首先通过super.dispatchSaveInstanceState保存自身的状态，再递归传递给子View。onSaveInstanceState主要用于获取View需要保存的State，并将自身的ID作为Key，存储到SparseArray<Parcelable> states列表中，其实就PhoneWindow的一个列表，这些数据最后会通过Binder保存到ActivityManagerService中去
    
> View.java
  
      protected void dispatchSaveInstanceState(SparseArray<Parcelable> container) {
        if (mID != NO_ID && (mViewFlags & SAVE_DISABLED_MASK) == 0) {
            mPrivateFlags &= ~PFLAG_SAVE_STATE_CALLED;
            Parcelable state = onSaveInstanceState();
            if ((mPrivateFlags & PFLAG_SAVE_STATE_CALLED) == 0) {
                throw new IllegalStateException(
                        "Derived class did not call super.onSaveInstanceState()");
            }
            if (state != null) {
                container.put(mID, state);
            }
        }
    }
    
那么针对ViewPager到底存储了什么信息？通过下面的代码很容易看出，其实就是新建个了一个SavedState场景数据，并且将当前的位置mCurItem存进去。
  
      @Override
    public Parcelable onSaveInstanceState() {
        Parcelable superState = super.onSaveInstanceState();
        SavedState ss = new SavedState(superState);
        ss.position = mCurItem;
        if (mAdapter != null) {
            ss.adapterState = mAdapter.saveState();
        }
        return ss;
    }
到这里存储的事情基本就完成了。接下来看一下ViewPager的恢复以及onRestoreInstanceState到底做了什么，

    protected void onRestoreInstanceState(Bundle savedInstanceState) {
        if (mWindow != null) {
            Bundle windowState = savedInstanceState.getBundle(WINDOW_HIERARCHY_TAG);
            if (windowState != null) {
                mWindow.restoreHierarchyState(windowState);
            }
        }
    }

从代码可以看出，其实就是获取当时保存的窗口信息，之后通过mWindow.restoreHierarchyState做数据恢复，

    @Override
    public void restoreHierarchyState(Bundle savedInstanceState) {
        if (mContentParent == null) {
            return;
        }

        SparseArray<Parcelable> savedStates
                = savedInstanceState.getSparseParcelableArray(VIEWS_TAG);
        if (savedStates != null) {
            mContentParent.restoreHierarchyState(savedStates);
        }
        ...
        
        if (mActionBar != null) {
        	...
              mActionBar.restoreHierarchyState(actionBarStates);
          }
    }
    
对于ViewPager会发生什么？从源码很容易看出，其实就是取出SavedState，并获取到异常杀死的时候的位置，以便后续的恢复，

> ViewPager.java

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
    
以上就解释了ViewPager是如何通过onSaveInstanceState与onRestoreInstanceState保存、恢复现场的。如果是ViewPager+FragmentAdapter的使用方式，就同时涉及FragmentActivity的恢复、也牵扯到Viewpager的恢复，其实FragmentAdapter也同样针对后台杀死做了一些兼容，防止重复新建Fragment，看一下FragmentAdapter的源码：

> FragmentPagerAdapter.java 

    @Override
    public Object instantiateItem(ViewGroup container, int position) {
        if (mCurTransaction == null) {
            mCurTransaction = mFragmentManager.beginTransaction();
        }

        final long itemId = getItemId(position);

        // Do we already have this fragment?
        <!--是否已经新建了Fragment？？-->
        
        String name = makeFragmentName(container.getId(), itemId);
        Fragment fragment = mFragmentManager.findFragmentByTag(name);
        
        1 如果Activity中存在相应Tag的Fragment，就不要通过getItem新建
        
        if (fragment != null) {
            mCurTransaction.attach(fragment);
        } else {
        2 如果Activity中不存在相应Tag的Fragment，就需要通过getItem新建
            fragment = getItem(position);
            mCurTransaction.add(container.getId(), fragment,
                    makeFragmentName(container.getId(), itemId));
        }
        if (fragment != mCurrentPrimaryItem) {
            FragmentCompat.setMenuVisibility(fragment, false);
            FragmentCompat.setUserVisibleHint(fragment, false);
        }

        return fragment;
    }

从1与2 可以看出，通过后台恢复，在FragmentActivity的onCreate函数中，会重建Fragment列表，那些被重建的Fragment不会再次通过getItem再次创建，再来看一下相似的控件FragmentTabHost，FragmentTabHost也是主页常用的控件，FragmentTabHost也有相应的后台杀死处理机制，从名字就能看出，这个是专门针对Fragment才创建出来的控件。

![后台杀死时View的保存及恢复](http://upload-images.jianshu.io/upload_images/1460468-359f4134ac54d901.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 
#    FragmentTabHost应对后台杀死做的兼容 

FragmentTabHost其实跟ViewPager很相似，在onSaveInstanceState执行的时候保存当前位置，并在onRestoreInstanceState恢复postion，并重新赋值给Tabhost，之后FragmentTabHost在onAttachedToWindow时，就可以根据恢复的postion设置当前位置，代码如下：

> FragmentTabHost.java


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
    
在FragmentTabHost执行onAttachedToWindow时候，会首先getCurrentTabTag ,如果是经历了后台杀死，这里得到的值其实是恢复的SavedState里的值，之后通过doTabChanged切换到响应的Tab，注意这里切换的时候，Fragment由于已经重建了，是不会再次新建的。

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        String currentTab = getCurrentTabTag();
        ...
        
        ft = doTabChanged(currentTab, ft);
        
        if (ft != null) {
            ft.commit();
            mFragmentManager.executePendingTransactions();
        }
    }

 
# App开发时针对后台杀死处理方式
                
* 最简单的方式，但是效果一般：**取消系统恢复** 

比如：针对FragmentActivity ，不重建：

    protected void onCreate(Bundle savedInstanceState) {
	     if (savedInstanceState != null) {
	     savedInstanceState.putParcelable(“android:support:fragments”, null);}
	     super.onCreate(savedInstanceState);
	}  

如果是系统的Actvity改成是“android:fragments"，不过这里需要注意：对于ViewPager跟FragmentTabHost不需要额外处理，处理了可能反而有反作用。

针对Window，如果不想让View使用恢复逻辑，在基类的FragmentActivity中覆盖onRestoreInstanceState函数即可。

    protected void onRestoreInstanceState(Bundle savedInstanceState) {
    }
    
当然以上的做法都是比较粗暴的做法，最好还是顺着Android的设计，在需要保存现场的地方保存，在需要恢复的地方，去除相应的数据进行恢复。以上就是后台杀死针对FragmentActivity、onSaveInstanceState、onRestoreInstanceState的一些分析，后面会有两篇针对后台杀死原理，以及ActivityManagerService如何处理杀死及恢复的文章。
  
	        
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