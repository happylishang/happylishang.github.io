 
        if (!mWindow.mIsFloating) {
            boolean disallowAnimate = !isLaidOut();
            disallowAnimate |= ((mLastWindowFlags ^ attrs.flags)
                    & FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS) != 0;
            mLastWindowFlags = attrs.flags;
				<!--如果是悬浮窗口，那么mLastTopInset、mLastBottomInset、mLastRightInset、mLastLeftInset都是0-->
            if (insets != null) {
                mLastTopInset = getColorViewTopInset(insets.getStableInsetTop(),
                        insets.getSystemWindowInsetTop());
                mLastBottomInset = getColorViewBottomInset(insets.getStableInsetBottom(),
                        insets.getSystemWindowInsetBottom());
                mLastRightInset = getColorViewRightInset(insets.getStableInsetRight(),
                        insets.getSystemWindowInsetRight());
                mLastLeftInset = getColorViewRightInset(insets.getStableInsetLeft(),
                        insets.getSystemWindowInsetLeft());
                        ...
                }

            boolean navBarToRightEdge = isNavBarToRightEdge(mLastBottomInset, mLastRightInset);
            boolean navBarToLeftEdge = isNavBarToLeftEdge(mLastBottomInset, mLastLeftInset);
            int navBarSize = getNavBarSize(mLastBottomInset, mLastRightInset, mLastLeftInset);
            updateColorViewInt(mNavigationColorViewState, sysUiVisibility,
                    mWindow.mNavigationBarColor, mWindow.mNavigationBarDividerColor, navBarSize,
                    navBarToRightEdge || navBarToLeftEdge, navBarToLeftEdge,
                    0 /* sideInset */, animate && !disallowAnimate, false /* force */);

            boolean statusBarNeedsRightInset = navBarToRightEdge
                    && mNavigationColorViewState.present;
            boolean statusBarNeedsLeftInset = navBarToLeftEdge
                    && mNavigationColorViewState.present;
            int statusBarSideInset = statusBarNeedsRightInset ? mLastRightInset
                    : statusBarNeedsLeftInset ? mLastLeftInset : 0;
            updateColorViewInt(mStatusColorViewState, sysUiVisibility,
                    calculateStatusBarColor(), 0, mLastTopInset,
                    false /* matchVertical */, statusBarNeedsLeftInset, statusBarSideInset,
                    animate && !disallowAnimate,
                    mForceWindowDrawsStatusBarBackground);
        }
        
 WindowInsets到底有多少，布局位置，是什么       
 

        
 
     /* package */ WindowInsets getWindowInsets(boolean forceConstruct) {
        if (mLastWindowInsets == null || forceConstruct) {
            mDispatchContentInsets.set(mAttachInfo.mContentInsets);
            mDispatchStableInsets.set(mAttachInfo.mStableInsets);
            Rect contentInsets = mDispatchContentInsets;
            Rect stableInsets = mDispatchStableInsets;
            // For dispatch we preserve old logic, but for direct requests from Views we allow to
            // immediately use pending insets.
            if (!forceConstruct
                    && (!mPendingContentInsets.equals(contentInsets) ||
                        !mPendingStableInsets.equals(stableInsets))) {
                contentInsets = mPendingContentInsets;
                stableInsets = mPendingStableInsets;
            }
            Rect outsets = mAttachInfo.mOutsets;
            if (outsets.left > 0 || outsets.top > 0 || outsets.right > 0 || outsets.bottom > 0) {
                contentInsets = new Rect(contentInsets.left + outsets.left,
                        contentInsets.top + outsets.top, contentInsets.right + outsets.right,
                        contentInsets.bottom + outsets.bottom);
            }
            mLastWindowInsets = new WindowInsets(contentInsets,
                    null /* windowDecorInsets */, stableInsets,
                    mContext.getResources().getConfiguration().isScreenRound(),
                    mAttachInfo.mAlwaysConsumeNavBar);
        }
        return mLastWindowInsets;
    }
           
           
floating = true window默认情况下是没有顶部状态栏浮层还有底部导航栏浮层的。 窗口显示的位置跟一下几个定义也有关系：





	
	
			/**
	         * For windows that are full-screen but using insets to layout inside
	         * of the screen decorations, these are the current insets for the
	         * content of the window.
	         */
	        final Rect mContentInsets = new Rect();
	
	
从Activity窗口剔除掉状态栏所占用的区域之后，所得到的区域就称为内容区域（Content Region）。顾名思义，内容区域就是用来显示Activity窗口的内容的。我们再抽象一下，假设Activity窗口的四周都有一块类似状态栏的区域，那么将这些区域剔除之后，得到中间的那一块区域就称为内容区域，而被剔除出来的区域所组成的区域就称为内容边衬区域（Content Insets）。Activity窗口的内容边衬区域可以用一个四元组（content-left, content-top, content-right, content-bottom）来描述，其中，content-left、content-right、content-top、content-bottom分别用来描述内容区域与窗口区域的左右上下边界距离。

	       
	        /**
	         * For windows that are full-screen but using insets to layout inside
	         * of the screen decorations, these are the current insets for the
	         * actual visible parts of the window.
	         */
	        final Rect mVisibleInsets = new Rect();

这时候Activity窗口的内容区域的大小有可能没有发生变化，这取决于它的Soft Input Mode。我们假设Activity窗口的内容区域没有发生变化，但是它在底部的一些区域被输入法窗口遮挡了，即它在底部的一些内容是不可见的。从Activity窗口剔除掉状态栏和输入法窗口所占用的区域之后，所得到的区域就称为可见区域（Visible Region）。同样，我们再抽象一下，假设Activity窗口的四周都有一块类似状态栏和输入法窗口的区域，那么将这些区域剔除之后，得到中间的那一块区域就称为可见区域，而被剔除出来的区域所组成的区域就称为可见边衬区域（Visible Insets）。Activity窗口的可见边衬区域可以用一个四元组（visible-left, visible-top, visible-right, visible-bottom）来描述，其中，visible-left、visible-right、visible-top、visible-bottom分别用来描述可见区域与窗口区域的左右上下边界距离。
        	
	        /**
	         * For windows that are full-screen but using insets to layout inside
	         * of the screen decorations, these are the current insets for the
	         * stable system windows.
	         */
	        final Rect mStableInsets = new Rect();

基本同上，注意，这里需要定的是都在全屏界面下有小，也就是说，这些都是floating = false的前提下。否则，这些数值都是0，也就是说，没哟可以绘制的区域。

那这个时候的背景黑色从哪里来的呢

# 有一个不是WindowManager.LayoutParams.MATCH_PARENT，就不会有黑色默认状态栏

      dialog.getWindow().setLayout(WindowManager.LayoutParams.MATCH_PARENT, WindowManager.LayoutParams.MATCH_PARENT);
	
# 状态栏自己更新机制 --默认情况下，状态栏颜色就是黑色的，只有强制要求改变，才会改变颜色。


    public void add(View statusBarView, int barHeight) {

        // Now that the status bar window encompasses the sliding panel and its
        // translucent backdrop, the entire thing is made TRANSLUCENT and is
        // hardware-accelerated.
        mLp = new WindowManager.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                barHeight,
                WindowManager.LayoutParams.TYPE_STATUS_BAR,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_TOUCHABLE_WHEN_WAKING
                        | WindowManager.LayoutParams.FLAG_SPLIT_TOUCH
                        | WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH
                        | WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS,
                PixelFormat.TRANSLUCENT);
        mLp.token = new Binder();
        mLp.gravity = Gravity.TOP;
        mLp.softInputMode = WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE;
        mLp.setTitle("StatusBar");
        mLp.packageName = mContext.getPackageName();
        mStatusBarView = statusBarView;
        mBarHeight = barHeight;
        mWindowManager.addView(mStatusBarView, mLp);
        mLpChanged = new WindowManager.LayoutParams();
        mLpChanged.copyFrom(mLp);
    }	
	
# 	状态栏只有一个更新了之后，看看是否会更新回来呢
	           
# 状态栏icon等是否改变跟dimeenable也有关系	           
# 跟踪下为什么可以改颜色

       StatusBarUtil.setTranslucent(dialog.getWindow());
        YXStatusBarUtil.setImmersionIconColor(dialog.getWindow(), true);
        
        
 虽然不能改变状态栏颜色，但是系统状态栏的黑色却可以去除掉。如何去除掉，那就是 windowDrawsSystemBarBackgrounds，那为何floating的时候，有的消费，有的不消费，导致导航栏可见呢


  