---
layout: post
title: "MeasureSpec与View定制"
description: "Java"
category: android开发

---
 	
#  	MeasureSpec的作用
 	
*  	根据父控件传入的参数，设计自身，其实父空间传入的是供你参考的。如果要改变，那就改变

*  	另外，自己可以通过MeasureSpec告诉Child控件，应该有什么规范

*  	MeasureSpec只是个参考，具体将来要多大，还是View自己说了算。如果View自己不定义，wrapcontent就跟matchparent一个效果
 

# getChildMeasureSpec

	public static int getChildMeasureSpec(int spec, int padding, int childDimension) {
	        int specMode = MeasureSpec.getMode(spec);
	        int specSize = MeasureSpec.getSize(spec);
	
	        int size = Math.max(0, specSize - padding);
	
	        int resultSize = 0;
	        int resultMode = 0;
	
	        switch (specMode) {
	        // Parent has imposed an exact size on us
	        case MeasureSpec.EXACTLY:
	            if (childDimension >= 0) {
	                resultSize = childDimension;
	                resultMode = MeasureSpec.EXACTLY;
	            } else if (childDimension == LayoutParams.MATCH_PARENT) {
	                // Child wants to be our size. So be it.
	                resultSize = size;
	                resultMode = MeasureSpec.EXACTLY;
	            } else if (childDimension == LayoutParams.WRAP_CONTENT) {
	                // Child wants to determine its own size. It can't be
	                // bigger than us.
	                resultSize = size;
	                resultMode = MeasureSpec.AT_MOST;
	            }
	            break;
	
	        // Parent has imposed a maximum size on us
	        case MeasureSpec.AT_MOST:
	            if (childDimension >= 0) {
	                // Child wants a specific size... so be it
	                resultSize = childDimension;
	                resultMode = MeasureSpec.EXACTLY;
	            } else if (childDimension == LayoutParams.MATCH_PARENT) {
	                // Child wants to be our size, but our size is not fixed.
	                // Constrain child to not be bigger than us.
	                resultSize = size;
	                resultMode = MeasureSpec.AT_MOST;
	            } else if (childDimension == LayoutParams.WRAP_CONTENT) {
	                // Child wants to determine its own size. It can't be
	                // bigger than us.
	                resultSize = size;
	                resultMode = MeasureSpec.AT_MOST;
	            }
	            break;
	
	        // Parent asked to see how big we want to be
	        case MeasureSpec.UNSPECIFIED:
	            if (childDimension >= 0) {
	                // Child wants a specific size... let him have it
	                resultSize = childDimension;
	                resultMode = MeasureSpec.EXACTLY;
	            } else if (childDimension == LayoutParams.MATCH_PARENT) {
	                // Child wants to be our size... find out how big it should
	                // be
	                resultSize = 0;
	                resultMode = MeasureSpec.UNSPECIFIED;
	            } else if (childDimension == LayoutParams.WRAP_CONTENT) {
	                // Child wants to determine its own size.... find out how
	                // big it should be
	                resultSize = 0;
	                resultMode = MeasureSpec.UNSPECIFIED;
	            }
	            break;
	        }
	        return MeasureSpec.makeMeasureSpec(resultSize, resultMode);
	    }
	       
# measureChildWithMargins 这个函数很重要 是自定义ViewGroup的关键


不考虑margin参数

    protected void measureChild(View child, int parentWidthMeasureSpec,
            int parentHeightMeasureSpec) {
        final LayoutParams lp = child.getLayoutParams();

        final int childWidthMeasureSpec = getChildMeasureSpec(parentWidthMeasureSpec,
                mPaddingLeft + mPaddingRight, lp.width);
        final int childHeightMeasureSpec = getChildMeasureSpec(parentHeightMeasureSpec,
                mPaddingTop + mPaddingBottom, lp.height);

        child.measure(childWidthMeasureSpec, childHeightMeasureSpec);
    }
    
考虑margin参数  

    protected void measureChildWithMargins(View child,
            int parentWidthMeasureSpec, int widthUsed,
            int parentHeightMeasureSpec, int heightUsed) {
        final MarginLayoutParams lp = (MarginLayoutParams) child.getLayoutParams();

        final int childWidthMeasureSpec = getChildMeasureSpec(parentWidthMeasureSpec,
                mPaddingLeft + mPaddingRight + lp.leftMargin + lp.rightMargin
                        + widthUsed, lp.width);
        final int childHeightMeasureSpec = getChildMeasureSpec(parentHeightMeasureSpec,
                mPaddingTop + mPaddingBottom + lp.topMargin + lp.bottomMargin
                        + heightUsed, lp.height);

        child.measure(childWidthMeasureSpec, childHeightMeasureSpec);
       
    }
    

接下来获取View高度   并且加上margin才能算子View占据的高度，
    
              final int childHeight = child.getMeasuredHeight();
              final int totalLength = mTotalLength;

		     //  其实这里已经++，只是为了区分margin带来的影响，看看哪个最大
              mTotalLength = Math.max(totalLength, totalLength + childHeight + lp.topMargin +
                      lp.bottomMargin + getNextLocationOffset(child));

之后加上padding就可以确定根据child获取的View的高度，最后再次根据自己确定一下resolveSizeAndState
 
    int heightSizeAndState = resolveSizeAndState(heightSize, heightMeasureSpec, 0); ---》LinearLayout


	第一个是自己需要的高度，第二个是parent限定的参考，如果按照Android给的推荐，那个就是下面的规格，当然，你也可以无视，自己设定
	
    public static int resolveSizeAndState(int size, int measureSpec, int childMeasuredState) {
        int result = size;
        int specMode = MeasureSpec.getMode(measureSpec);
        int specSize =  MeasureSpec.getSize(measureSpec);
        switch (specMode) {
        case MeasureSpec.UNSPECIFIED:
            result = size;
            break;
        case MeasureSpec.AT_MOST:
            if (specSize < size) {
                result = specSize | MEASURED_STATE_TOO_SMALL;
            } else {
                result = size;
            }
            break;
        case MeasureSpec.EXACTLY:
            result = specSize;
            break;
        }
        return result | (childMeasuredState&MEASURED_STATE_MASK);
    }
    


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


#GrideView自身实现逻辑
 
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

          