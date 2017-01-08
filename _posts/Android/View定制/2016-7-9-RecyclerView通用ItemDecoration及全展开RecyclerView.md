---
layout: post
title: "通用RecyclerView的ItemDecoration及全展开RecyclerView的实现"
description: "View定制"
category: android开发
image: http://upload-images.jianshu.io/upload_images/1460468-a663f26677c53449.gif?imageMogr2/auto-orient/strip

---


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

RecyclerView提供了addItemDecoration接口与ItemDecoration类用来定制分割线样式，那么，在RecyclerView源码中，是怎么用使用ItemDecoration的呢。与普通View的绘制流程一致，RecyclerView也要经过measure->layout->draw，并且在measure、layout之后，就应该按照ItemDecoration的限制，为RecyclerView的分割线挪出空间。RecyclerView的measure跟Layout其实都是委托给自己的LayoutManager的，在LinearLayoutManager测量或者布局时都会直接或者间接调用RecyclerView的measureChildWithMargins函数，而measureChildWithMargins函数会进一步找到addItemDecoration添加的ItemDecoration，通过其getItemOffsets函数获取所需空间信息，源码如下：

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
        
        
可见measureChildWithMargins会首先通过getItemDecorInsetsForChild计算出每个child的ItemDecoration所限制的边界信息，之后将边界所需的空间作为已用空间为child构造MeasureSpec，最后用MeasureSpec对child进行尺寸测量：child.measure(widthSpec, heightSpec);来看一下getItemDecorInsetsForChild函数：
       
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
一般而言，不会同时设置多类ItemDecoration，太麻烦，对于普通的线性布局列表，其实就简单设定一个自定义ItemDecoration即可，其中outRect参数主要是控制每个Item上下左右的分割线所占据的宽度跟高度，这个尺寸跟绘制的时候的尺寸应该对应（如果需要绘制的话），看一下LinearItemDecoration的getItemOffsets实现：
    
    @Override
    public void getItemOffsets(Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        if (mOrientation == VERTICAL_LIST) {
        
        <!--垂直方向 ，最后一个不设置padding-->
            if (parent.getChildAdapterPosition(view) < parent.getAdapter().getItemCount()1) {
                outRect.set(0, 0, 0, mSpanSpace);
            } else {
                outRect.set(0, 0, 0, 0);
            }
        } else {
         <!--水平方向 ，最后一个不设置padding-->
            if (parent.getChildAdapterPosition(view) < parent.getAdapter().getItemCount()1) {
                outRect.set(0, 0, mSpanSpace, 0);
            } else {
                outRect.set(0, 0, 0, 0);
            }
        }
    }   

measure跟layout之后，再来看一下RecyclerView的onDraw函数， RecyclerView在onDraw函数中会调用ItemDecoration的onDraw，绘制分割线或者其他辅助信息，ItemDecoration 支持上下左右四个方向定制占位分割线等信息，具体要绘制的样式跟位置都完全由开发者确定，所以自由度非常大，其实如果不是太特殊的需求的话，onDraw函数完全可以不做任何处理，仅仅用背景色就可以达到简单的分割线的目的，当然，如果想要定制一些特殊的图案之类的需话，就需要自己绘制，来看一下LinearItemDecoration的onDraw（只看Vertical的）
    
    
    @Override
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        if (mOrientation == VERTICAL_LIST) {
            drawVertical(c, parent);
        } else {
           ...
        }
    }
    
其实，如果不是特殊的绘制需求，比如显示七彩的，或者图片，完全不需要任何绘制，如果一定要绘制，注意绘制的尺寸区域跟原来getItemOffsets所限制的区域一致，绘制的区域过大不仅不会显示出来，还会引起过度绘制的问题：
    
	public void drawVertical(Canvas c, RecyclerView parent) {						       int totalCount = parent.getAdapter().getItemCount();		       final int childCount = parent.getChildCount();		       for (int i = 0; i < childCount; i++) {		           final View child = parent.getChildAt(i);		           final RecyclerView.LayoutParams params = (RecyclerView.LayoutParams) child		                   .getLayoutParams();		           final int top = child.getBottom() + params.bottomMargin +		                   Math.round(ViewCompat.getTranslationY(child));		           final int bottom = top + mVerticalSpan;				           final int left = child.getLeft() + params.leftMargin;		           final int right = child.getRight() + params.rightMargin;				           if (!isLastRaw(parent, i, mSpanCount, totalCount))		               if (childCounti > mSpanCount) {		                   drawable.setBounds(left, top, right, bottom);		                   drawable.draw(c);		    }		
		}		
	}
	
 简单看一下真个流程图
    
![RecyclerView的ItemDocration绘制](http://upload-images.jianshu.io/upload_images/1460468-52fa42dfce0e40ca.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

## 带分割线的网格式RecyclerView--GridLayoutItemDecoration  

网格式RecyclerView的处理流程跟上面的线性列表类似，不过网格式的需要根据每个Item的位置为其设置好边距，比如最左面的不需要左边占位，最右面的不需要右面的占位，最后一行不需要底部的占位，如下图所示

![网格式ItemDocration的限制](http://upload-images.jianshu.io/upload_images/1460468-01441ee79842622c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

RecyclerView的每个childView都会通过getItemOffsets来设置自己ItemDecoration，对于网格式的RecyclerView，需要在四个方向上对其ItemDecoration进行限制，来看一下其实现类GridLayoutItemDecoration的getItemOffsets：


    @Override
    public void getItemOffsets(Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        final int position = parent.getChildAdapterPosition(view);
        final int totalCount = parent.getAdapter().getItemCount();
        int left = (position % mSpanCount == 0) ? 0 : mHorizonSpan;
        int bottom = ((position + 1) % mSpanCount == 0) ? 0 : mVerticalSpan;
        if (isVertical(parent)) {
            if (!isLastRaw(parent, position, mSpanCount, totalCount)) {
                outRect.set(left, 0, 0, mVerticalSpan);
            } else {
                outRect.set(left, 0, 0, 0);
            }
        } else {
            if (!isLastColumn(parent, position, mSpanCount, totalCount)) {
                outRect.set(0, 0, mHorizonSpan, bottom);
            } else {
                outRect.set(0, 0, 0, bottom);
            }
        }
    }
    
其实上面的代码就是根据RecyclerView滑动方向（横向或者纵向）以及child的位置（是不是最后一行或者最后一列），对附属区域进行限制，同样，如果不是特殊的分割线样式，通过背景就基本可以实现需求，不用特殊draw。
     
## 全展开的线性RecyclerView--ExpandedLinearLayoutManager

展开的RecyclerView的实现跟分割线添加时不同的逻辑，

    
## 全展开的线性RecyclerView--ExpandedLinearLayoutManager

RecyclerView全展开的逻辑跟分割线完全不同，全展开主要是跟measure逻辑相关，简单看一下RecyclerView（v-22版本，相对简单）的measure源码：

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
			...
			
			<!--关键代码，如果mLayout（LayoutManager）非空,就采用LayoutManager的mLayout.onMeasure-->
        if (mLayout == null) {
            defaultOnMeasure(widthSpec, heightSpec);
        } else {
            mLayout.onMeasure(mRecycler, mState, widthSpec, heightSpec);
        }

        mState.mInPreLayout = false; // clear
    }
由以上代码可以看出，在为RecyclerView设置了LayoutManager之后，RecyclerView的measure逻辑其实就是委托给了它的LayoutManager，这里以LinearLayoutManager为例，不过LinearLayoutManager源码里面并没有重写onMeasure函数，也就是说，对于RecyclerView的线性样式，对于尺寸的处理采用的是跟ViewGroup一样的处理，完全由父控件限制，不过对于v-23里面有了一些修改，就是增加了对wrap_content的支持。既然这样，我们就可以把设置尺寸的时机放到LayoutManager的onMeasure中，对全展开的RecyclerView来说，其实就是将所有child测量一遍，之后将每个child需要高度或者宽度累加，看一下ExpandedLinearLayoutManager的实现：在测量child的时候，采用RecyclerView的measureChildWithMargins，该函数已经将ItemDecoration的占位考虑进去，之后通过getDecoratedMeasuredWidth获取真正需要占用的尺寸。

    @Override
    public void onMeasure(RecyclerView.Recycler recycler, RecyclerView.State state,
                          int widthSpec, int heightSpec) {
        final int widthMode = View.MeasureSpec.getMode(widthSpec);
        final int heightMode = View.MeasureSpec.getMode(heightSpec);
        final int widthSize = View.MeasureSpec.getSize(widthSpec);
        final int heightSize = View.MeasureSpec.getSize(heightSpec);
        int measureWidth = 0;
        int measureHeight = 0;
        int count;
        if (mMaxItemCount < 0 || getItemCount() < mMaxItemCount) {
            count = getItemCount();
        } else {
            count = mMaxItemCount;
        }
        for (int i = 0; i < count; i++) {
            int[] measuredDimension = getChildDimension(recycler, i);
            if (measuredDimension == null || measuredDimension.length != 2)
                return;
            if (getOrientation() == HORIZONTAL) {
                measureWidth = measureWidth + measuredDimension[0];
               <!--获取最大高度-->
                measureHeight = Math.max(measureHeight, measuredDimension[1]);
            } else {
                measureHeight = measureHeight + measuredDimension[1];
                <!--获取最大宽度-->
                measureWidth = Math.max(measureWidth, measuredDimension[0]);
            }
        }

        measureHeight = heightMode == View.MeasureSpec.EXACTLY ? heightSize : measureHeight;
        measureWidth = widthMode == View.MeasureSpec.EXACTLY ? widthSize : measureWidth;
        if (getOrientation() == VERTICAL && measureWidth > widthSize) {
            measureWidth = widthSize;
        } else if (getOrientation() == HORIZONTAL && measureHeight > heightSize) {
            measureHeight = heightSize;
        }
        setMeasuredDimension(measureWidth, measureHeight);
    }


    private int[] getChildDimension(RecyclerView.Recycler recycler, int position) {
        try {
            int[] measuredDimension = new int[2];
            View view = recycler.getViewForPosition(position);
            //测量childView，以便获得宽高（包括ItemDecoration的限制）
            super.measureChildWithMargins(view, 0, 0);
            //获取childView，以便获得宽高（包括ItemDecoration的限制），以及边距
            RecyclerView.LayoutParams p = (RecyclerView.LayoutParams) view.getLayoutParams();
            measuredDimension[0] = getDecoratedMeasuredWidth(view) + p.leftMargin + p.rightMargin;
            measuredDimension[1] = getDecoratedMeasuredHeight(view) + p.bottomMargin + p.topMargin;
            return measuredDimension;
        } catch (Exception e) {
            Log.d("LayoutManager", e.toString());
        }
        return null;
    }

## 全展开的网格式RecyclerView--ExpandedGridLayoutManager

全展开的网格式RecyclerView的实现跟线性的十分相似，唯一不同的就是在确定尺寸的时候，不是将每个child的尺寸叠加，而是要将每一行或者每一列的尺寸叠加，这里假定行高或者列宽都是相同的，其实在使用中这两种场景也是最常见的，看如下代码，其实除了加了行与列判断逻辑，其他基本跟上面的全展开线性的类似。

     @Override
    public void onMeasure(RecyclerView.Recycler recycler, RecyclerView.State state, int widthSpec, int heightSpec) {
        final int widthMode = View.MeasureSpec.getMode(widthSpec);
        final int heightMode = View.MeasureSpec.getMode(heightSpec);
        final int widthSize = View.MeasureSpec.getSize(widthSpec);
        final int heightSize = View.MeasureSpec.getSize(heightSpec);
        int measureWidth = 0;
        int measureHeight = 0;
        int count = getItemCount();
        int span = getSpanCount();
        for (int i = 0; i < count; i++) {
            measuredDimension = getChildDimension(recycler, i);
            if (getOrientation() == HORIZONTAL) {
                if (i % span == 0 ) {
                    measureWidth = measureWidth + measuredDimension[0];
                }
                measureHeight = Math.max(measureHeight, measuredDimension[1]);
            } else {
                if (i % span == 0) {
                    measureHeight = measureHeight + measuredDimension[1];
                }
                measureWidth = Math.max(measureWidth, measuredDimension[0]);
            }
        }
        measureHeight = heightMode == View.MeasureSpec.EXACTLY ? heightSize : measureHeight;
        measureWidth = widthMode == View.MeasureSpec.EXACTLY ? widthSize : measureWidth;
        setMeasuredDimension(measureWidth, measureHeight);
    }
    
最后附上横向滑动效果图：

![横向滑动](http://upload-images.jianshu.io/upload_images/1460468-2dca8377271e7bf7.gif?imageMogr2/auto-orient/strip%7CimageView2/2/w/1080/q/30)

以上就是比较通用的RecyclerView使用场景及所做的兼容 ，最后附上Github链接**[RecyclerItemDecoration](https://github.com/happylishang/RecyclerItemDecoration)**，欢迎star，fork。