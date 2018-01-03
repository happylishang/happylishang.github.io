---
layout: default
title: ViewPager刷新问题原理分析及解决方案（FragmentPagerAdapter+FragementStatePagerAdapter）
categories: [Android]

---

Android开发中经常用到ViewPager+Fragment+Adapter的场景，一般每个Fragment控制自己的刷新，但是如果想要刷新整个ViewPager怎么做呢？或者想要将缓存的Fragent给重建怎么做呢？之前做业务的时候遇到一个问题，ViewPage在第二次setAdapter的如果用的是FragmentPager并不会导致页面刷新，但是采用FragementStatePagerAdapter却会刷新？不由得有些好奇，随跟踪了部分源码，简单整理如下：

# ViewPager+FragmentPagerAdapter为何不能通过setAdapter做到整体刷新

第二次设置PagerAdapter的时候，首先会将原来的Fragment进行清理，之后在调用populate()重建，只是重建的时候并不一定真的重新创建Fragment，如下：

    public void setAdapter(PagerAdapter adapter) {
        if (mAdapter != null) {
            ...
            for (int i = 0; i < mItems.size(); i++) {
                final ItemInfo ii = mItems.get(i);
                <!--全部destroy-->
                mAdapter.destroyItem(this, ii.position, ii.object);
            }
            mAdapter.finishUpdate(this);
            <!--清理-->
            mItems.clear();
            removeNonDecorViews();
            <!--重置位置-->
            mCurItem = 0;
            scrollTo(0, 0);
        }
        ...
        if (!wasFirstLayout) {
        <!--重新设置Fragment-->
           populate();
           }
        ...   
        }        
        
之前说过，第二次通过setAdapter的方式来设置ViewPager的FragmentAdapter时不会立即刷新的效果，但是如果往后滑动几屏会发现其实是有效果了？为什么呢，因为第二次setAdapter的时候，已经被FragmentManager缓存的Fragent不会被新建，也不会被刷新，因为FragmentAdapter在调用destroy的时候，采用的是detach的方式，并未真正的销毁Fragment，仅仅是打算销毁了View，这就导致FragmentManager中仍旧保留正Fragment的缓存：

    @Override
    public void destroyItem(ViewGroup container, int position, Object object) {
        if (mCurTransaction == null) {
            mCurTransaction = mFragmentManager.beginTransaction();
        }
        // 仅仅detach
        mCurTransaction.detach((Fragment)object);
    }
 
 Transaction.detach函数最终会调用FragmentManager的detachFragment函数，将Fragment从当前Activity detach
 
         public void detachFragment(Fragment fragment, int transition, int transitionStyle) {
        if (!fragment.mDetached) {
            <!--只是detach -->
            fragment.mDetached = true;
            if (fragment.mAdded) {
            <!--如果是被added 从added列表中移除-->
                if (mAdded != null) {
                    mAdded.remove(fragment);
                }
                ...
                fragment.mAdded = false;
                <!--将状态设置为Fragment.CREATED-->
                moveToState(fragment, Fragment.CREATED, transition, transitionStyle, false);
            }
        }
    }

可以看到，这里仅仅会将Fragment设置为Fragment.CREATED，对于Fragment.CREATED状态的Fragment，FragmentManager是不会调用makeInactive进行清理的，

    void moveToState(Fragment f, int newState, int transit, int transitionStyle,
            boolean keepActive) {
            ...
	 case Fragment.CREATED:
             if (newState < Fragment.CREATED) {
                 ...
                    if (!keepActive) {
                        if (!f.mRetaining) {
                            makeInactive(f);
                        } else {
                            f.mActivity = null;
                            f.mParentFragment = null;
                            f.mFragmentManager = null;
                        }
                   ...
                 
因为只有makeInactive才会清理Fragment的引用如下：

    void makeInactive(Fragment f) {
        if (f.mIndex < 0) {
            return;
        }
        <!--置空mActive列表对于Fragment的强引用-->
        mActive.set(f.mIndex, null);
        if (mAvailIndices == null) {
            mAvailIndices = new ArrayList<Integer>();
        }
        mAvailIndices.add(f.mIndex);
        mActivity.invalidateFragment(f.mWho);
        f.initState();
    }
    
      
可见，Fragment的缓存仍旧留在FragmentManager中。新的FragmentPagerAdapter被设置后，会通过instantiateItem函数来获取Fragment，这个时候它首先会从FragmentManager的缓存中去取Fragment，取到的Fragment其实就是之前未销毁的Fragment，这也是为什么不会刷新的原因：

    @Override
    public Object instantiateItem(ViewGroup container, int position) {
    	<!--新建一个事务-->
        if (mCurTransaction == null) {
            mCurTransaction = mFragmentManager.beginTransaction();
        }
        final long itemId = getItemId(position);
        <!--利用id与container的id创建name-->
        String name = makeFragmentName(container.getId(), itemId);
        <!--根据name在Activity的FragmentManager中查找缓存Fragment-->
        Fragment fragment = mFragmentManager.findFragmentByTag(name);
        <!--如果找到的话，直接使用当前Fragment-->
        if (fragment != null) {
            mCurTransaction.attach(fragment);
        } else {
        <!--如果找不到则新建，并新建name，添加到container中去-->
            fragment = getItem(position);
            mCurTransaction.add(container.getId(), fragment,
                    makeFragmentName(container.getId(), itemId));
        }
        if (fragment != mCurrentPrimaryItem) {
            fragment.setMenuVisibility(false);
            fragment.setUserVisibleHint(false);
        }
       return fragment;
    }
    
从上面代码可以看到，在新建Fragment对象的时候，首先是通过mFragmentManager.findFragmentByTag(name);查找是否已经有Fragment缓存，第二次设置Adapter的时候，由于部分Fragment已经被添加到FragmentManager的缓存中去了，新的Adapter仍然能通过mFragmentManager.findFragmentByTag(name)找到缓存Fragment，阻止了Fragment的新建，因此不会有整体刷新的效果。**那如果想要整体刷新怎么办呢？可以使用FragementStatePagerAdapter**，两者对于Fragment的缓存管理不同。

# ViewPager+FragementStatePagerAdapter可以通过setAdapter做到整体刷新

同样先看一下FragementStatePagerAdapter的destroyItem函数，FragementStatePagerAdapter在destroyItem的时候使用的是remove的方式，这种方式对于没有添加到回退栈的Fragment操作来说，不仅会销毁view，还会销毁Fragment。

    @Override
    public void destroyItem(ViewGroup container, int position, Object object) {
        Fragment fragment = (Fragment)object;

        if (mCurTransaction == null) {
            mCurTransaction = mFragmentManager.beginTransaction();
        }
       while (mSavedState.size() <= position) {
            mSavedState.add(null);
        }
        mSavedState.set(position, mFragmentManager.saveFragmentInstanceState(fragment));
        <!--FragementStatePagerAdapter先清理自己的缓存-->
        mFragments.set(position, null);
        <!--直接删除-->
        mCurTransaction.remove(fragment);
    }

可见FragementStatePagerAdapter会首先通过mFragments.set(position, null)清理自己的缓存，然后，通过Transaction.remove清理在FragmentManager中的缓存，Transaction.remove最终会调用FragmentManager的removeFragment函数： 

    public void removeFragment(Fragment fragment, int transition, int transitionStyle) {
    <!-- 其实两者的主要区别就是看是否在回退栈，如果在，表现就一致，如果不在，表现不一致-->
        final boolean inactive = !fragment.isInBackStack();
        if (!fragment.mDetached || inactive) {
            if (mAdded != null) {
                mAdded.remove(fragment);
            }
            ...
            fragment.mAdded = false;
            fragment.mRemoving = true;
            <!--将状态设置为Fragment.CREATED或者Fragment.INITIALIZING-->
            moveToState(fragment, inactive ? Fragment.INITIALIZING : Fragment.CREATED,
                    transition, transitionStyle, false);
        }
    }

FragementStatePagerAdapter中的Fragment在添加的时候，都没有addToBackStack，所以moveToState会将状态设置为Fragment.INITIALIZING ，

    void moveToState(Fragment f, int newState, int transit, int transitionStyle,
            boolean keepActive) {
            ...
	 case Fragment.CREATED:
             if (newState < Fragment.CREATED) {
                 ...
                    if (!keepActive) {
                        if (!f.mRetaining) {
                            makeInactive(f);
                        } else {
                            f.mActivity = null;
                            f.mParentFragment = null;
                            f.mFragmentManager = null;
                        }
                   ...
                   

Fragment.INITIALIZING < Fragment.CREATED，这里一般会调用makeInactive函数清理Fragment的引用，这里其实就算销毁了Fragment在FragmentManager中的缓存。

ViewPager通过populate因此再次新建的时候，FragementStatePagerAdapter的instantiateItem 一定会新建Fragment，因为之前的Fragment已经被清理掉了，在自己的Fragment缓存列表中取不到，就新建。看如下代码：

        @Override
    public Object instantiateItem(ViewGroup container, int position) {
		<!--查看FragementStatePagerAdapter中是否有缓存的Fragment，如果有直接返回-->
        if (mFragments.size() > position) {
            Fragment f = mFragments.get(position);
            if (f != null) {
                return f;
            }
        }
       ...
		 <!--关键点   如果在FragementStatePagerAdapter找不到，直接新建，不关心FragmentManager中是否有-->
        Fragment fragment = getItem(position);
        <!--查看是否需恢复，如果需要，则恢复-->
        if (mSavedState.size() > position) {
            Fragment.SavedState fss = mSavedState.get(position);
            if (fss != null) {
                fragment.setInitialSavedState(fss);
            }
        }
        ...
        mFragments.set(position, fragment);
        mCurTransaction.add(container.getId(), fragment);

        return fragment;
    }

从上面代码也可以看出，FragementStatePagerAdapter在新建Fragment的时候，不会去FragmentMangerImpl中去取，而是直接在FragementStatePagerAdapter的缓存中取，如果取不到，则直接新建Fragment，如果通过setAdapter设置了新的FragementStatePagerAdapter，一定会新建所有的Fragment，就能够达到整体刷新的效果。


# FragmentPagerAdapter如何通过notifyDataSetChanged刷新ViewPager

FragmentPagerAdapter中的数据发生改变时，往往要重新将数据设置到Fragment，或者干脆新建Fragment，而对于用FragmentPagerAdapter的ViewPager来说，只是利用其notifyDataSetChanged是不够的，跟踪源码会发现，notifyDataSetChanged最终会调用ViewPager中的dataSetChanged：

![notifyDataSetChanged流程](http://upload-images.jianshu.io/upload_images/1460468-981abb27409ef986.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

    void dataSetChanged() {
        ...
        for (int i = 0; i < mItems.size(); i++) {
            final ItemInfo ii = mItems.get(i);
            final int newPos = mAdapter.getItemPosition(ii.object);
           if (newPos == PagerAdapter.POSITION_UNCHANGED) {
                continue;
            }
           if (newPos == PagerAdapter.POSITION_NONE) {
                mItems.remove(i);
                i--;
               ...
               mAdapter.destroyItem(this, ii.position, ii.object);
                needPopulate = true;
               ...
                continue;
            }
        ...
        if (needPopulate) {
            final int childCount = getChildCount();
            for (int i = 0; i < childCount; i++) {
                final View child = getChildAt(i);
                final LayoutParams lp = (LayoutParams) child.getLayoutParams();
                if (!lp.isDecor) {
                    lp.widthFactor = 0.f;
                }
            }
            setCurrentItemInternal(newCurrItem, false, true);
            requestLayout();
        }
    }

默认情况下FragmentPagerAdapter中的getItemPosition返回的是PagerAdapter.POSITION_UNCHANGED，所以这里不会
destroyItem，即时设置了PagerAdapter.POSITION_NONE，调用了其destroyItem，也仅仅是detach，销毁了View，Fragment仍旧不会重建，必须手动更改参数才可以，这个时机在哪里呢？FragmentAdapter的getItem函数会在第一次需要创建Fragment的时候调用，如果需要将参数传递给Fragment，可以通过Fragment.setArguments()来设置，但是仅仅在getItem新建的时候有效，一旦被Fragment被创建，就会被FragmentManager缓存，如果不主动释放，对于当前位置的Fragment来说，getItem函数是不会再次被调用的，原因已经在上文的instantiateItem函数处说明了，它会首先去缓存中取。那这个时候，如何更新呢？Fragment.setArguments是不能再调用的，因为被attach过的Fragment来说不能再次通过setArguments被设置参数，否则抛出异常

    public void setArguments(Bundle args) {
        if (mIndex >= 0) {
            throw new IllegalStateException("Fragment already active");
        }
        mArguments = args;
    }
    
那如果真要更改就需要在其instantiateItem的时候，通过额外的接口手动设置，同时也必须将getItemPosition返回值设置为POSITION_NONE，这样才会每次都走View的新建流程，才有可能刷新：

    public int getItemPosition(Object object) {
        return POSITION_NONE;
    }

至于参数如何设置呢？这里就需要用户手动提供接口变更参数了，在自定义的FragmentAdapter覆盖instantiateItem，自己手动获取缓存Fragment，在attach之前，将参数给重新设置进去，之后，Fragment在走onCreateView流程的时候，就会获取到新的参数。

    @Override
    public Object instantiateItem(ViewGroup container, int position) {

        String name = makeFragmentName(container.getId(), position);
        Fragment fragment =((FragmentActivity) container.getContext()).getSupportFragmentManager().findFragmentByTag(name);

        if(fragment instanceof MyFragment){
            Bundle bundle=new Bundle();
            bundle.putString("msg",""+System.currentTimeMillis());
            ( (MyFragment) fragment).resetArgument(bundle);
        }

        return super.instantiateItem(container, position);
    }

    private static String makeFragmentName(int viewId, long id) {
        return "android:switcher:" + viewId + ":" + id;
    }
    
如此，便可以完成FragmentPagerAdapter中Fragment的刷新。并且到这里我们也知道了，对于FragmentPagerAdapter来说，用户完全不需要自己缓存Fragment，只需要缓存View，因为FragmentPagerAdapter不会销毁Fragment，也不会销毁FragmentManager中缓存的Fragment，至于缓存的View要不要刷新，可能就要你具体的业务需求了。

# FragmentStatePagerAdapter如何通过notifyDataSetChanged刷新ViewPager页面

对于FragmentStatePagerAdapter相对容易些，如果不需要考虑效率，重建所有的Fragment即可，只需要复写其getItemPosition函数

    public int getItemPosition(Object object) {
        return POSITION_NONE;
    }
    
因为FragmentStatePagerAdapter中会真正的remove Fragment，达到完全重建的效果。


# Fragmentmanager Transaction栈的意义

最后看一下Fragmentmanager中Transaction栈，FragmentManager的Transaction栈到底是做什么的呢？FragmentManager对于Fragment的操作是分批量进行的，在一个Transaction中有多个add、remove、attach操作，Android是有返回键的，为了支持点击返回键恢复上一个场景的操作，Android的Fragment管理引入Transaction栈，更方便回退，其实将一个Transaction的操作全部翻转：添加变删除、attach变detach，反之亦然。对于每个入栈的Transaction，都是需要出栈的，而且每个操作都有前后文，比如进入与退出的动画，当需要翻转这个操作，也就是点击返回键的时候，需要知道如何翻转，也就是需要记录当前场景，对于remove，如果没有入栈操作，说明不用记录上下文，可以直接清理掉。对于ViewPager在使用FragmentPagerAdapter/FragmentStatePagerAdapter的时候都不会addToBackStack，这也是为什么detach跟remove有时候表现一致或者不一致的原因。简单看一下出栈操作，其实就是将原来从操作翻转一遍，当然，并不是完全照搬，还跟当前的Fragment状体有关。

    public void popFromBackStack(boolean doStateMove) {
       Op op = mTail;
        while (op != null) {
            switch (op.cmd) {
                case OP_ADD: {
                    Fragment f = op.fragment;
                    f.mNextAnim = op.popExitAnim;
                    mManager.removeFragment(f,
                            FragmentManagerImpl.reverseTransit(mTransition),
                            mTransitionStyle);
                } break;
                case OP_REPLACE: {
                    Fragment f = op.fragment;
                    if (f != null) {
                        f.mNextAnim = op.popExitAnim;
                        mManager.removeFragment(f,
                                FragmentManagerImpl.reverseTransit(mTransition),
                                mTransitionStyle);
                    }
                    if (op.removed != null) {
                        for (int i=0; i<op.removed.size(); i++) {
                            Fragment old = op.removed.get(i);
                            old.mNextAnim = op.popEnterAnim;
                            mManager.addFragment(old, false);
                        }
                    }
                } break;
                ...

# FragmentManager对于Fragment的缓存管理

FragmentManager主要维护三个重要List，一个是mActive Fragment列表，一个是mAdded FragmentList，还有个BackStackRecord回退栈

    ArrayList<Fragment> mActive;
    ArrayList<Fragment> mAdded;
    ArrayList<BackStackRecord> mBackStack;

mAdded列表是被当前添加到Container中去的，而mActive是全部参与的Fragment集合，只要没有被remove，就会一致存在，可以认为mAdded的Fragment都是活着的，而mActive的Fragment却可能被处决，并被置null，只有makeInactive函数会这么做。
               
    void makeInactive(Fragment f) {
        if (f.mIndex < 0) {
            return;
        }
        mActive.set(f.mIndex, null);
        if (mAvailIndices == null) {
            mAvailIndices = new ArrayList<Integer>();
        }
        mAvailIndices.add(f.mIndex);
        mActivity.invalidateFragment(f.mWho);
        f.initState();
    }
	        
FragmentPagerAdapter获取试图获取的Fragment就是从这两个列表中读取的 。

    public Fragment findFragmentByTag(String tag) {
        if (mAdded != null && tag != null) {
            for (int i=mAdded.size()-1; i>=0; i--) {
                Fragment f = mAdded.get(i);
                if (f != null && tag.equals(f.mTag)) {
                    return f;
                }
            }
        }
        if (mActive != null && tag != null) {
            for (int i=mActive.size()-1; i>=0; i--) {
                Fragment f = mActive.get(i);
                if (f != null && tag.equals(f.mTag)) {
                    return f;
                }
            }
        }
        return null;
    }    

# 总结

本文简单分析了下ViewPager在使用FrgmentPagerAdapter跟FragmentStatePagerAdapter遇到问题，原理、及问题的解决方案。
    
#  参考文档

[为什么调用 FragmentPagerAdapter.notifyDataSetChanged() 并不能更新其 Fragment？](http://www.cnblogs.com/dancefire/archive/2013/01/02/why-notifyDataSetChanged-does-not-work.html)