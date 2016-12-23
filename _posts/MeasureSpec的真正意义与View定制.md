自定义View是开发中最普通的需求，而自定义View的时候如何控制View的尺寸是开发者面临的第一个问题，比如为什么明明使用的是WRAP_CONTENT却跟MATCH_PARENT的表现相同，本文以FlowLayout的实现为一个小例子来说明MeasureSpec真正的意义，以及View定制如何用好这个参数。
先分析一下FLowLayout流式布局的特点：将子View从左往右以此放置，放不开的就换行。那么如何控制FLowLayout的高度与宽度呢？会有一下几个问题需要考虑
* FlowLayout应该考虑父容器装的大小（否则超出了，又显示不出来）
* FlowLayout应该知道所有子View的宽度与宽度
* FlowLayout应该考虑自己布局参数

先看第一个问题，FlowLayout应该考虑父容器的大小

## 考虑父容器的大小

自定义子View获取父控件大小的信息的入口只有一个，那就是onMeasure函数

	onMeasure(int widthMeasureSpec, int heightMeasureSpec)

onMeasure中的widthMeasureSpec（heightMeasureSpec）参数是父容器根据子View的布局参数+自身的MeasureSpec所构建的，参考一下ViewGroup的measureChild跟measureChildWithMargins，一般系统的ViewGroup都会直接或者间接调用这类函数去measure子View，

getChildMeasureSpec有三个参数，第一个是ViewGroup传递自身的MeasureSpec，第二个是ViewGroup的padding，第三个一般是子View的LayoutPara中的width或者height
	
	public static int getChildMeasureSpec(int spec, int padding, int childDimension) {
	        int specMode = MeasureSpec.getMode(spec);
	        int specSize = MeasureSpec.getSize(spec);
	
	        int size = Math.max(0, specSize - padding);
	
	        int resultSize = 0;
	        int resultMode = 0;
	
	        switch (specMode) {
	        // Parent has imposed an exact size on us  我就这么多，你想要多少，如果你要全部，我可以给你最多的
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
	
	        // Parent has imposed a maximum size on us 最多给多少，你要多少，我就去申请多少，并且全部给你，但是不是一次性的申请最大的给你的，同时也是给自己	        case MeasureSpec.AT_MOST:
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
	
	        // Parent asked to see how big we want to be  要多少给多，反着你爷爷有钱，要多少，你爸爸帮你去要。
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
    
很明显需要将所有的子View测量一遍，
任何View大小的测量首先都是测量自己所需要的宽与高，之后通过setMeasuredDimension(sizeWidth,sizeHeight)将宽高设置进去，其实最终决定View尺寸的也是setMeasuredDimension，所以说View的尺寸完全是自己定的也没什么过错，只是没将父容器给的参考考虑进去罢了。所以先心里有个概念：MeasureSpec是父控件传给子控件的参数，仅供子控件测量时作为一个参考，仅仅是个参考，要多大，还是View自己说了算，但是为了适应父控件，绝大多说情况，MeasureSpec还是有用的。比如父控件200dp，一般来讲，父控件是不希望子空间超出200dp的范围的，因为自己显示不了。