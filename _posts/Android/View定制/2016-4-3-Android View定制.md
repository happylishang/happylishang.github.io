---
layout: post
title: "Android View定制"
category: View
 

---


#### MeasureSpec详细解析


*  	根据父控件传入的参数，设计自身，其实父空间传入的是供你参考的。如果要改变，那就改变

*  	另外，自己可以通过MeasureSpec告诉Child控件，应该有什么规范

*  	MeasureSpec只是个参考，具体将来要多大，还是View自己说了算。如果View自己不定义，wrapcontent就跟matchparent一个效果
 
##### 全部展开的GrideView
 		
	public class WrapContentGridView extends GridView {

	public WrapContentGridView(Context context) {
		super(context);
	}

	public WrapContentGridView(Context context, AttributeSet attrs) {
		super(context, attrs);
	}
	
	/**
	 * wrap_content之后mx3还能上下滚动，不需要滚动在此禁止上下滚动
	 */
	@Override
	public boolean dispatchTouchEvent(MotionEvent ev) {
		if (ev.getAction() == MotionEvent.ACTION_MOVE) {
			return true;
		}
		return super.dispatchTouchEvent(ev);
	}

	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		int heightSpec;
		if (getLayoutParams().height == LayoutParams.WRAP_CONTENT) {
			// The great Android "hackatlon", the love, the magic.
			// The two leftmost bits in the height measure spec have
			// a special meaning, hence we can't use them to describe height.
			heightSpec = MeasureSpec.makeMeasureSpec(Integer.MAX_VALUE >> 2, MeasureSpec.AT_MOST);
		} else {
			// Any other height should be respected as is.
			heightSpec = heightMeasureSpec;
		}
		super.onMeasure(widthMeasureSpec, heightSpec);
	}
	
    }

 #### GrideView自身实现逻辑
 
        if (heightMode == MeasureSpec.AT_MOST) {
            int ourSize =  mListPadding.top + mListPadding.bottom;
           
            final int numColumns = mNumColumns;
            for (int i = 0; i < count; i += numColumns) {
                ourSize += childHeight;
                if (i + numColumns < count) {
                    ourSize += mVerticalSpacing;
                }
                if (ourSize >= heightSize) {
                    ourSize = heightSize;
                    break;
                }
            }
            heightSize = ourSize;
        }
        
        
因为ListView跟GrideView自身做了兼容，计算，上面的代码才可以行得通  


#### Scrowller及OverScowller使用及平滑处理

Scroll其实类似于ValueeAnimator，只是个工具类，就提的更新交给用户自己 

#### 不同层次的手势处理


####  getMeasuredHeight()与getHeight的区别

getMeasuredHeight()是在onMeasure之后可以获取的View的应该设置的高度，但是getHeight是获取的layout之后的参数，就是布局后的。

 
    /**
     * Assign a size and position to this view.
     *
     * This is called from layout.
     *
     * @param left Left position, relative to parent
     * @param top Top position, relative to parent
     * @param right Right position, relative to parent
     * @param bottom Bottom position, relative to parent
     * @return true if the new size and position are different than the
     *         previous ones
     * {@hide}
     */
    protected boolean setFrame(int left, int top, int right, int bottom) {
        boolean changed = false;
        
 
	The size of a view is expressed with a width and a height. A view actually possess two pairs of width and height values.
	
	The first pair is known as measured width and measured height. These dimensions define how big a view wants to be within its parent (see Layout for more details.) The measured dimensions can be obtained by calling getMeasuredWidth() and getMeasuredHeight().
	
	The second pair is simply known as width and height, or sometimes drawing width and drawing height. These dimensions define the actual size of the view on screen, at drawing time and after layout. These values may, but do not have to, be different from the measured width and height. The width and height can be obtained by calling getWidth() and getHeight().
	
实际上，View的绘制位置是由Layout后的mLeft，mRight 之类的东西确认的。

    public void draw(Canvas canvas) {
        if (mClipBounds != null) {
        .....
        
        int left = mScrollX + paddingLeft;
        int right = left + mRight - mLeft - mPaddingRight - paddingLeft;
        int top = mScrollY + getFadeTop(offsetRequired);
        int bottom = top + getFadeHeight(offsetRequired);	

实际上在当屏幕可以包裹内容的时候，他们的值相等，只有当view超出屏幕后，才能看出他们的区别：getMeasuredHeight()是实际View的大小，与屏幕无关，而getHeight的大小此时则是屏幕的大小。当超出屏幕后，getMeasuredHeight()等于getHeight()加上屏幕之外没有显示的大小
 