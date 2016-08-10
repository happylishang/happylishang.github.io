---
layout: post
title: "RecyclerView源码分析"
description: "android"
category: android开发

---

#ChildItemView的测量与ItemDecoration的关系 ,如何考虑ItemDecoration占有与padding

        public void measureChildWithMargins(View child, int widthUsed, int heightUsed) {
            final LayoutParams lp = (LayoutParams) child.getLayoutParams();

            final Rect insets = mRecyclerView.getItemDecorInsetsForChild(child);
            widthUsed += insets.left + insets.right;
            heightUsed += insets.top + insets.bottom;

            final int widthSpec = getChildMeasureSpec(getWidth(), getWidthMode(),
                    getPaddingLeft() + getPaddingRight() +
                            lp.leftMargin + lp.rightMargin + widthUsed, lp.width,
                    canScrollHorizontally());
 
 首先获取ItemDecoration设置的边距,计算的时候注意的
 
 添加机制：是由每个LayoutManger负责子View的添加：通过adapter获得View添加。比如LinearLayoutManager：
 
     void layoutChunk(RecyclerView.Recycler recycler, RecyclerView.State state,
            LayoutState layoutState, LayoutChunkResult result) {
        View view = layoutState.next(recycler);
        if (view == null) {
            if (DEBUG && layoutState.mScrapList == null) {
                throw new RuntimeException("received null view when unexpected");
            }
            // if we are laying out views in scrap, this may return null which means there is
            // no more items to layout.
            result.mFinished = true;
            return;
        }
        LayoutParams params = (LayoutParams) view.getLayoutParams();
        if (layoutState.mScrapList == null) {
            if (mShouldReverseLayout == (layoutState.mLayoutDirection
                    == LayoutState.LAYOUT_START)) {
                addView(view);
            } else {
                addView(view, 0);
            }    
            
#ChildItemView的绘制与ItemDecoration的关系
# RecyclerView的onDraw

RecyclerView是个容器ViewGroup，一般，对于ViewGroup而言，自己是不需要onDraw进行绘制的，ViewGroup的super.onDraw(c)会使得内部的View被绘制，但是由于ItemDecorations不是View，没有被添加到ViewGroup中，所以RecyclerView要自己绘制：

    @Override
    public void onDraw(Canvas c) {
        super.onDraw(c);

        final int count = mItemDecorations.size();
        for (int i = 0; i < count; i++) {
            mItemDecorations.get(i).onDraw(c, this, mState);
        }
    }

#参考文档

[ListView源码分析](https://github.com/CharonChui/AndroidNote/blob/master/Android%E5%8A%A0%E5%BC%BA/ListView%E6%BA%90%E7%A0%81%E5%88%86%E6%9E%90.md)
