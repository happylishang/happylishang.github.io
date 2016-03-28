---
layout: post
title: "MeasureSpec与被嵌套的ListView跟GrideView"
description: "Java"
category: android开发

---
 	
####  	MeasureSpec的作用
 	
*  	根据父控件传入的参数，设计自身，其实父空间传入的是供你参考的。如果要改变，那就改变

*  	另外，自己可以通过MeasureSpec告诉Child控件，应该有什么规范

*  	MeasureSpec只是个参考，具体将来要多大，还是View自己说了算。如果View自己不定义，wrapcontent就跟matchparent一个效果
 
#### 全部展开的GrideView
 		
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

 #### GrideView		自身实现逻辑
 
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
       