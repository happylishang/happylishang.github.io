---
layout: post
title: "通用RecyclerView的ItemDecoration及全展开RecyclerView的实现"
description: "View定制"
category: android开发

---

# RecyclerView的几种常用场景

Android L面世之后，Google就推荐在开发项目中使用RecyclerView来取代ListView，因为RecyclerView的灵活性跟性能都要比ListView更强，但是，带来的问题也不少，比如：列表分割线都要开发者自己控制，再者，RecyclerView的测量与布局的逻辑都委托给了自己LayoutManager来处理，如果需要对RecyclerView进行改造，相应的也要对其LayoutManager进行定制。本文主要就以以下场景给出RecyclerView使用参考：

* 如何实现带分割线的线性RecyclerView
* 如何实现带分割线网格式RecyclerView
* 如何实现全展开的线性RecyclerView(比如：嵌套到ScrollView中使用)
* 如何实现全展开的网格式RecyclerView(比如：嵌套到ScrollView中使用)

先看一下实现样式，为了方便控制，边界的均不设置分割线，方便定制，如果需要可以采用Padding或者Margin来实现。Github连接 **[RecyclerItemDecoration](https://github.com/happylishang/RecyclerItemDecoration)**


![网格式列表样式](http://upload-images.jianshu.io/upload_images/1460468-2ecbed8e5d3076e0.gif?imageMogr2/auto-orient/strip)

![全展开的网格式列表](http://upload-images.jianshu.io/upload_images/1460468-a663f26677c53449.gif?imageMogr2/auto-orient/strip)

![全展开的线性列表](http://upload-images.jianshu.io/upload_images/1460468-8e9ab06297bdbe21.gif?imageMogr2/auto-orient/strip)


# 不同场景RecyclerView实现

## 默认的纵向线性RecyclerView
首先看一下最简单的纵向线性RecyclerView，一般用以下代码：

        LinearLayoutManager linearLayoutManager = new LinearLayoutManager(this);
        linearLayoutManager.setOrientation(LinearLayoutManager.VERTICAL);
        mRecyclerView.setLayoutManager(linearLayoutManager);

以上就是最简单的线性RecyclerView的实现，但默认不带分割线，如果想要使用比如20dp的黑色作为分割线，就需要自己定制，Google为RecyclerView提供了ItemDecoration，它的作用就是为Item添加一些附属信息，比如：分割线，浮层等。

## 带分割线的线性RecyclerView--LinearItemDecoration

RecyclerView提供了addItemDecoration接口与ItemDecoration类用来定制分割线样式，在RecyclerView源码中，是怎么用使用ItemDecoration的呢，与普通View的绘制流程一致：measure->layout->draw，也就是说，在draw的时候，measure跟layout都已经完成，已经为为ItemDecoration的绘制挪出了空间，到底什么时候为ItemDecoration计算出的空间呢？看一下RecyclerView的measureChildWithMargins，它通过getItemDecorInsetsForChild函数获得ItemDecoration需要的空间，在measure跟layout的时候考虑进去。

      public void measureChildWithMargins(View child, int widthUsed, int heightUsed) {
          final LayoutParams lp = (LayoutParams) child.getLayoutParams();

          final Rect insets = mRecyclerView.getItemDecorInsetsForChild(child);
          widthUsed += insets.left + insets.right;
          heightUsed += insets.top + insets.bottom;

          final int widthSpec = getChildMeasureSpec(getWidth(), getWidthMode(),
                  getPaddingLeft() + getPaddingRight() +
                          lp.leftMargin + lp.rightMargin + widthUsed, lp.width,
                  canScrollHorizontally());
          final int heightSpec = getChildMeasureSpec(getHeight(), getHeightMode(),
                  getPaddingTop() + getPaddingBottom() +
                          lp.topMargin + lp.bottomMargin + heightUsed, lp.height,
                  canScrollVertically());
          if (shouldMeasureChild(child, widthSpec, heightSpec, lp)) {
              child.measure(widthSpec, heightSpec);
          }
      }
        
        
下面函数的意义是：计算出每个RecyclerView子Item的ItemDecoration所需要的边界信息
       
    Rect getItemDecorInsetsForChild(View child) {
        final LayoutParams lp = (LayoutParams) child.getLayoutParams();
        if (!lp.mInsetsDirty) {
            return lp.mDecorInsets;
        }

        final Rect insets = lp.mDecorInsets;
        insets.set(0, 0, 0, 0);
        final int decorCount = mItemDecorations.size();
        for (int i = 0; i < decorCount; i++) {
            mTempRect.set(0, 0, 0, 0);
            <!--通过这里知道，需要绘制的空间位置-->
            mItemDecorations.get(i).getItemOffsets(mTempRect, child, this, mState);
            insets.left += mTempRect.left;
            insets.top += mTempRect.top;
            insets.right += mTempRect.right;
            insets.bottom += mTempRect.bottom;
        }
        lp.mInsetsDirty = false;
        return insets;
    }
    
对于普通的线性布局就是如下，其中outRect参数主要是控制每个Item上下左右的分割线所占据的宽度跟高度，这个尺寸跟我们绘制的时候的尺寸是对应的
    
    @Override
    public void getItemOffsets(Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        if (mOrientation == VERTICAL_LIST) {
            if (parent.getChildAdapterPosition(view) < parent.getAdapter().getItemCount() - 1) {
                outRect.set(0, 0, 0, mSpanSpace);
            } else {
                outRect.set(0, 0, 0, 0);
            }
        } else {
            if (parent.getChildAdapterPosition(view) < parent.getAdapter().getItemCount() - 1) {
                outRect.set(0, 0, mSpanSpace, 0);
            } else {
                outRect.set(0, 0, 0, 0);
            }
        }
    }   




RecyclerView在onDraw函数中会调用ItemDecoration的onDraw，绘制分割线或者其他辅助信息，ItemDecoration 
支持上下左右四个方向定制占位分割线等信息，具体要绘制的样式跟位置都完全由开发者确定，所以自由度非常大，但是对于线性RecyclerView，只需要考虑下面的分割线即可：

    @Override
    public void onDraw(Canvas c) {
        super.onDraw(c);

        final int count = mItemDecorations.size();
        for (int i = 0; i < count; i++) {
            mItemDecorations.get(i).onDraw(c, this, mState);
        }
    }
        
在来看一下LinearItemDecoration的onDraw（只看Vertical的）
    
    
    @Override
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        if (mOrientation == VERTICAL_LIST) {
            drawVertical(c, parent);
        } else {
            drawHorizontal(c, parent);
        }
    }
    
## 带分割线的网格式RecyclerView--GridLayoutItemDecoration  

网格式RecyclerView需要根据每个Item的位置为其设置好ItemDocration，比如最左面的不需要左边占位，最右面的不需要右面的占位，最后一行不需要底部的占位，如下图所示

![网格式ItemDocration的限制](http://upload-images.jianshu.io/upload_images/1460468-01441ee79842622c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

我们知道每个View都要通过getItemOffsets来设置自己ItemDecoration四个方向的占位，比如网格五等分,这五个Item如何处理四个方向的占位呢？这里采用最直接的分配，假设N等分，那么每个Item需要占用的是N-1/N，

    @Override
    public void getItemOffsets(Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        final int position = parent.getChildAdapterPosition(view);
        final int totalCount = parent.getAdapter().getItemCount();
        int everyCharge = mHorizonSpan * (mSpanCount-1)/ (mSpanCount);
        int modValue = position % mSpanCount;
        //(0 4/5)|(1/5 3/5)|(2/5 2/5)|(3/5 1/5)|(4/5 0)
        final int left = Math.round(modValue * mHorizonSpan / mSpanCount);
        final int right = everyCharge - left;

        if (!isLastRaw(parent, position, mSpanCount, totalCount)) {
            outRect.set(left, 0, right, mVerticalSpan);
        } else {
            outRect.set(left, 0, right, 0);
        }
    }
     
## 全展开的线性RecyclerView--ExpandedLinearLayoutManager


## 全展开的网格式RecyclerView--ExpandedGridLayoutManager
 