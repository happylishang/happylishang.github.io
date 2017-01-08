---
layout: post
title: "仿淘宝、京东拖拽商品详情(可嵌套ViewPager、ListView、WebView、FragmentTabhost)"
description: "Java"
category: View定制
image: http://upload-images.jianshu.io/upload_images/1460468-f026299ee35b008e.gif?imageMogr2/auto-orient/strip

---

对于电商App，商品详情无疑是很重要的一个模块，观察主流购物App的详情界面，发现大部分都是做成了上下两部分，上面展示商品规格信息，下面是H5商品详情，或者是嵌套了一个包含H5详情及评论列表的ViewPager界面，本文就是实现了一个兼容不同需求的上下滚动黏滞View.

**[DragScrollDetailsLayout  GitHub链接 ](https://github.com/happylishang/DragScrollDetailsLayout)**

# 实现效果图

首先看一下实现效果图

####  简单的ScrollView+Webview

当然，如果将Webview替换成其他的ListView之类的也是支持的。

![scrollview+webview.gif](http://upload-images.jianshu.io/upload_images/1460468-f026299ee35b008e.gif?imageMogr2/auto-orient/strip)

#### ScrollView+ViewPager

适用场景：底部需要添加多个界面，并且需要滑动

![scrollview+viewpager.gif](http://upload-images.jianshu.io/upload_images/1460468-88eed3e50cc798b1.gif?imageMogr2/auto-orient/strip)

#### ScrollView+Fragmenttabhost

适用场景：底部需要添加多个界面，但是不需要滑动

![scrollview+fragmenttabhost.gif ](http://upload-images.jianshu.io/upload_images/1460468-e1e243a70b498f2a.gif?imageMogr2/auto-orient/strip)

# 实现

对于这个需求的场景，很容易想到可以分成上下两部分来实现，只需要一个Vertical的LinearLayout

首先自定义View内部先声明两个顶层子ViewmUpstairsView、 View mDownstairsView，并且采用一个变量CurrentTargetIndex标记当前处于操作那个View，

		public class DragScrollDetailsLayout extends LinearLayout {
		    private View mUpstairsView;
		    private View mDownstairsView;
		    private View mCurrentTargetView;
		
		
		    public enum CurrentTargetIndex {
		        UPSTAIRS,
		        DOWNSTAIRS;
		
		        public static CurrentTargetIndex valueOf(int index) {
		            return 1 == index ? DOWNSTAIRS : UPSTAIRS;
		        }
		    }
 

其余的就是处理滚动及动画的问题，对于处理滚动与动画有如下几个问题需要解决

* 如何知道上面或者下面的View已经滚动的到顶部或者底部
* 滚动到边界时，如何拦截处理滑动
* 松手后如何处理后续的动效

   
## 如何判断滚动边界

首先来看第一个问题，如何知道上面或者下面的View滚动到了边界，其实Android源码中有个类ViewCompat，它有个函数canScrollVertically(View view, int offSet, MotionEvent ev)就可以判断当前View是否可以向哪个方向滚动，offset的正负值用来判断向上还是向下，当然，仅仅靠这个函数还是不够的，因为ViewGroup是可以相互嵌套的，也许ViewGroup本身不能滚动，但是其内部的子View却可以滚动，这时候，就需要递归遍历相关的View，比如对于ViewPager中嵌套了包含WebView或者List的Fragment。不过，并非所有的子View都需要遍历，只有与TouchEvent相关的View才需要判断。因此还需要写个函数判断View是否在TouchEvent所在的区域，如下函数isTransformedTouchPointInView：

    /***
     * 判断MotionEvent是否处于View上面
     */
    protected boolean isTransformedTouchPointInView(MotionEvent ev, View view) {
        float x = ev.getRawX();
        float y = ev.getRawY();
        int[] rect = new int[2];
        view.getLocationInWindow(rect);
        float localX = x - rect[0];
        float localY = y - rect[1];
        return localX >= 0 && localX < (view.getRight() - view.getLeft())
                && localY >= 0 && localY < (view.getBottom() - view.getTop());
    }

之后我们可以利用该函数对View进行递归遍历，判断最上层的ViewGroup是否可以上下滑动
    
	    private boolean canScrollVertically(View view, int offSet, MotionEvent ev) {
	
	        if (!mChildHasScrolled && !isTransformedTouchPointInView(ev, view)) {
	            return false;
	        }
	        if (ViewCompat.canScrollVertically(view, offSet)) {
	            mChildHasScrolled = true;
	            return true;
	        }
	        if (view instanceof ViewPager) {
	            return canViewPagerScrollVertically((ViewPager) view, offSet, ev);
	        }
	        if (view instanceof ViewGroup) {
	            ViewGroup vGroup = (ViewGroup) view;
	            for (int i = 0; i < vGroup.getChildCount(); i++) {
	                if (canScrollVertically(vGroup.getChildAt(i), offSet, ev)) {
	                    mChildHasScrolled = true;
	                    return true;
	                }
	            }
	        }
	        return false;
	    }
知道View是否可以上下滑动到边界后，拦截事件的时机就比较清晰了，那么接着看第二个问题，如何拦截滑动。

## 事件拦截处理

onInterceptTouchEvent在返回True之后，就不会再执行了，我们只需要把握准确的拦截时机，比如如果处于上面的View，就要对上拉事件比较敏感，处于底部就要对下拉事件敏感，同时还要将无效的手势归零，比如，操作上面的View时，如果先是下拉，并且是无效的下拉，那么就要将拦截点重置。

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        switch (ev.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                mDownMotionX = ev.getX();
                mDownMotionY = ev.getY();
                if (mVelocityTracker == null) {
                    mVelocityTracker = VelocityTracker.obtain();
                }
                mVelocityTracker.clear();
                mChildHasScrolled=false;
                break;
            case MotionEvent.ACTION_MOVE:
                adjustValidDownPoint(ev);
                return checkCanInterceptTouchEvent(ev);
            default:
                break;
        }
        return false;
    }

checkCanInterceptTouchEvent主要用来判断是否需要拦截，并非不可滚动，就需要拦截事件，不可滚动只是一个必要条件而已，
    
       private boolean checkCanInterceptTouchEvent(MotionEvent ev) {
       final float xDiff = ev.getX() - mDownMotionX;
       final float yDiff = ev.getY() - mDownMotionY;
       if (!canChildScrollVertically((int) yDiff,ev)) {
           mInitialInterceptY = (int) ev.getY();
           if (Math.abs(yDiff) > mTouchSlop && Math.abs(yDiff) >= Math.abs(xDiff)
                   && !(mCurrentViewIndex == CurrentTargetIndex.UPSTAIRS && yDiff > 0
                   || mCurrentViewIndex == CurrentTargetIndex.DOWNSTAIRS && yDiff < 0)) {
               return true;
           }
       }
       return false;
    }	
    
事件拦截之后，就是对Move事件进行处理

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        switch (ev.getActionMasked()) {
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                flingToFinishScroll();
                recycleVelocityTracker();
                break;
            case MotionEvent.ACTION_MOVE:
                scroll(ev);
                break;
            default:
                break;
        }
        return true;
    }
    
滚动比较简单，直接调用scrollTo就可以，同时为了收集滚动速度，还可以用VelocityTracker做一下记录：
    
    private void scroll(MotionEvent event) {
        if (mCurrentViewIndex == CurrentTargetIndex.UPSTAIRS) {
            if (getScrollY() <= 0 && event.getY() > mInitialInterceptY) {
                mInitialInterceptY = (int) event.getY();
            }
            scrollTo(0, (int) (mInitialInterceptY - event.getY()));
        } else {
            if (getScrollY() >= mUpstairsView.getMeasuredHeight() && event.getY() < mInitialInterceptY) {
                mInitialInterceptY = (int) event.getY();
            }
            scrollTo(0, (int) (mInitialInterceptY - event.getY() + mUpstairsView.getMeasuredHeight()));
        }
        mVelocityTracker.addMovement(event);
    }    

## 收尾动画

在Up事件之后，还要简单的处理一下一下收尾的滚动动画，比如，滚动距离不够要复原，否则，就滚动到目标视图，这里主要是根据Up事件的位置，计算需要滚动的距离，并通过Scroller来完成剩下的滚动。

    private void flingToFinishScroll() {

        final int pHeight = mUpstairsView.getMeasuredHeight();
        final int threshold = (int) (pHeight * mPercent);
        float scrollY = getScrollY();
        if (CurrentTargetIndex.UPSTAIRS == mCurrentViewIndex) {
            if (scrollY <= 0) {
                scrollY = 0;
            } else if (scrollY <= threshold) {
                if (needFlingToToggleView()) {
                    scrollY = pHeight - getScrollY();
                    mCurrentViewIndex = CurrentTargetIndex.DOWNSTAIRS;
                } else
                    scrollY = -getScrollY();
            } else {
                scrollY = pHeight - getScrollY();
                mCurrentViewIndex = CurrentTargetIndex.DOWNSTAIRS;
            }
        } else if (CurrentTargetIndex.DOWNSTAIRS == mCurrentViewIndex) {
            if (pHeight - scrollY <= threshold) {
                if (needFlingToToggleView()) {
                    scrollY = -getScrollY();
                    mCurrentViewIndex = CurrentTargetIndex.UPSTAIRS;
                } else
                    scrollY = pHeight - scrollY;
            } else if (scrollY < pHeight) {
                scrollY = -getScrollY();
                mCurrentViewIndex = CurrentTargetIndex.UPSTAIRS;
            }
        }
        mScroller.startScroll(0, getScrollY(), 0, (int) scrollY, mDuration);
        if (mOnSlideDetailsListener != null) {
            mOnSlideDetailsListener.onStatueChanged(mCurrentViewIndex);
        }
        postInvalidate();
    }

以上就是常用商品详情黏滞布局的实现。最后附上GitHub链接 **[DragScrollDetailsLayout  GitHub链接 ](https://github.com/happylishang/DragScrollDetailsLayout)**