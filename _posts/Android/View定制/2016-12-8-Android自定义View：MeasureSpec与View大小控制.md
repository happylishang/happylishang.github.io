---
layout: post
title: "MeasureSpec与View大小控制"
description: "Java"
category: android开发
image: http://upload-images.jianshu.io/upload_images/1460468-d8e25ab337751361.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---

自定义View是Android开发中最普通的需求，灵活控制View的尺寸是开发者面临的第一个问题，比如，为什么明明使用的是WRAP_CONTENT却跟MATCH_PARENT表现相同。在处理View尺寸的时候，我们都知道最好在onMeasure中设定好自定义View尺寸，那么究竟如何合理的选择这个尺寸呢。例如，如果要实现一个自定义ViewGroup，应该如何适配它的大小呢，直观来说，可能有以下问题需要考虑：


* 自定的ViewGroup最好不要超过父控件的大小，这样才能保证自己能在父控件中完整显示
* 自定的ViewGroup的子控件最好不要超过自己的大小，这样才能保证子控件显示完整
* 如果明确为自定的ViewGroup指定了尺寸，最好要按照指定的尺寸设置
* 顶层View尺寸是谁指定的呢？

以上三个问题可能是自定义ViewGroup最需要考虑的问题，首先先看下第一个问题。


# 父容器的限制与MeasureSpec

先假定，父容器是300dp*300dp的尺寸，如果子View的布局参数是
    
    <!--场景1-->
	android:layout_width="match_parent"
	android:layout_height="match_parent"
			  
那么按照我们的期望，子View的尺寸要是300dp*300dp，如果子View的布局参数是

    <!--场景2-->
	android:layout_width="100dp"
	android:layout_height="100dp"
	
按照我们的期望，子View的尺寸要是100dp*100dp，如果子View的布局参数是

    <!--场景3-->
	android:layout_width="wrap_content"
	android:layout_height="wrap_content"
	
按照我们的期望，子View的尺寸可以按照自己需求的尺寸来确定，但是最好不要超过300dp*300dp。那么怎么把这些要求告诉子View呢？MeasureSpec其实就是承担这种作用：***MeasureSpec是父控件提供给子View的一个参数，作为设定自身大小参考，只是个参考，要多大，还是View自己说了算***。先看下MeasureSpec的构成，MeasureSpec由size和mode组成，mode包括三种，UNSPECIFIED、EXACTLY、AT_MOST，size就是配合mode给出的参考尺寸，具体意义如下：

* UNSPECIFIED(未指定),父控件对子控件不加任何束缚，子元素可以得到任意想要的大小，这种MeasureSpec一般是由父控件自身的特性决定的。比如ScrollView，它的子View可以随意设置大小，无论多高，都能滚动显示，这个时候，size一般就没什么意义。
* EXACTLY(完全)，父控件为子View指定确切大小，希望子View完全按照自己给定尺寸来处理，跟上面的场景1跟2比较相似，这时的MeasureSpec一般是父控件根据自身的MeasureSpec跟子View的布局参数来确定的。一般这种情况下size>0,有个确定值。
* AT_MOST(至多)，父控件为子元素指定最大参考尺寸，希望子View的尺寸不要超过这个尺寸，跟上面场景3比较相似。这种模式也是父控件根据自身的MeasureSpec跟子View的布局参数来确定的，一般是子View的布局参数采用wrap_content的时候。

先来看一下ViewGroup源码中measureChild时怎么为子View构造MeasureSpec的：

	 protected void measureChild(View child, int parentWidthMeasureSpec,
	         int parentHeightMeasureSpec) {
	     final LayoutParams lp = child.getLayoutParams();
	
	     final int childWidthMeasureSpec = getChildMeasureSpec(parentWidthMeasureSpec,
	             mPaddingLeft + mPaddingRight, lp.width);
	     final int childHeightMeasureSpec = getChildMeasureSpec(parentHeightMeasureSpec,
	             mPaddingTop + mPaddingBottom, lp.height);
	
	     child.measure(childWidthMeasureSpec, childHeightMeasureSpec);
	 }
	 
由于任何View都是支持Padding参数的，因此，在为子View设置参考尺寸的时候，需要先把自己的Padding给去除，这同时也是为了Layout做铺垫。接着看getChildMeasureSpec

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
                resultSize = View.sUseZeroUnspecifiedMeasureSpec ? 0 : size;
                resultMode = MeasureSpec.UNSPECIFIED;
            } else if (childDimension == LayoutParams.WRAP_CONTENT) {
                // Child wants to determine its own size.... find out how
                // big it should be
                resultSize = View.sUseZeroUnspecifiedMeasureSpec ? 0 : size;
                resultMode = MeasureSpec.UNSPECIFIED;
            }
            break;
        }
        return MeasureSpec.makeMeasureSpec(resultSize, resultMode);
    }
    
可以看到父控件会参考自己的MeasureSpec跟子View的布局参数，为子View构建合适的MeasureSpec，盗用网上的一张图来描述就是

![MeasureSpec构建](http://upload-images.jianshu.io/upload_images/1460468-d8e25ab337751361.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
        
当子View接收到父控件传递的MeasureSpec的时候，就可以知道父控件希望自己如何显示，这个点对于开发者而言就是onMeasure函数，先来看下View.java中onMeasure函数的实现：

    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        setMeasuredDimension(getDefaultSize(getSuggestedMinimumWidth(), widthMeasureSpec),
                getDefaultSize(getSuggestedMinimumHeight(), heightMeasureSpec));
    }

其中getSuggestedMinimumWidth是根据设置的背景跟最小尺寸得到一个备用的参考尺寸，接着看getDefaultSize，如下：
    
    public static int getDefaultSize(int size, int measureSpec) {
        int result = size;
        int specMode = MeasureSpec.getMode(measureSpec);
        int specSize = MeasureSpec.getSize(measureSpec);

        switch (specMode) {
        case MeasureSpec.UNSPECIFIED:
            result = size;
            break;
        case MeasureSpec.AT_MOST:
        case MeasureSpec.EXACTLY:
            result = specSize;
            break;
        }
        return result;
    }

可以看到，如果自定义View没有重写onMeasure函数，MeasureSpec.AT_MOST跟MeasureSpec.AT_MOST的表现是一样的，也就是对于场景2跟3的表现其实是一样的，也就是wrap_content就跟match_parent一个效果，现在我们知道MeasureSpec的主要作用：***父控件传递给子View的参考***，那么子View拿到后该如何用呢？

# 自定义View尺寸的确定

接收到父控件传递的MeasureSpec后，View应该如何用来处理自己的尺寸呢？如果View不是ViewGroup相对就比较简答，只需要参照MeasureSpec，并跟自身需求来设定尺寸即可，其实，默认onMeasure的就是完全按照父控件传递MeasureSpec设定的尺寸。这里重点讲一下ViewGroup，measure的目的其实就是为了确认自己的宽高，不过为了计算合理的宽高尺寸，ViewGroup在measure自己的时候，也必须知道所有子View的宽高，举个例子，用一个常用的流式布局FlowLayout来讲解一下如何定义自己的尺寸。

先分析一下FLowLayout流式布局（从左到右）的特点：FLowLayout将所有子View从左往右依次放置，如果当前行，放不开的就换行。从流失布局的特点来看，在确定FLowLayout尺寸的时候，我们需要知道下列信息，

* 父容器传递给FlowLayout的MeasureSpec推荐的大小（超出了，显示不出来，又没意义）
* FlowLayout中所有子View的宽度与宽度：计算宽度跟高度的时候需要用的到。

首先看父容器传递给FlowLayout的MeasureSpec，对开发者而言，它可见于onMeasure函数，是通过onMeasure的参数传递进来的，它的意义上面的已经说过了，现在来看，怎么用比较合理？其实ViewGroup.java源码中也提供了比较简洁的方法，有两个比较常用的measureChildren跟resolveSize，在之前的分析中我们知道measureChildren会调用getChildMeasureSpec为子View创建MeasureSpec，并通过measureChild测量每个子View的尺寸。那么resolveSize呢，看下面源码，resolveSize(int size, int measureSpec)的两个输入参数，第一个参数：size，是View自身希望获取的尺寸，第二参数：measureSpec，其实父控件传递给View，推荐View获取的尺寸，resolveSize就是综合考量两个参数，最后给一个建议的尺寸：

	 public static int resolveSize(int size, int measureSpec) {
	        return resolveSizeAndState(size, measureSpec, 0) & MEASURED_SIZE_MASK;
	    }
 
    public static int resolveSizeAndState(int size, int measureSpec, int childMeasuredState) {
        final int specMode = MeasureSpec.getMode(measureSpec);
        final int specSize = MeasureSpec.getSize(measureSpec);
        final int result;
        switch (specMode) {
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
       case MeasureSpec.UNSPECIFIED:
            default:
                result = size;
        }
        return result | (childMeasuredState & MEASURED_STATE_MASK);
    }
    
可以看到：

*  如果父控件传递给的MeasureSpec的mode是MeasureSpec.UNSPECIFIED，就说明，父控件对自己没有任何限制，那么尺寸就选择自己需要的尺寸size
*  如果父控件传递给的MeasureSpec的mode是MeasureSpec.EXACTLY，就说明父控件有明确的要求，希望自己能用measureSpec中的尺寸，这时就推荐使用MeasureSpec.getSize(measureSpec)
*  如果父控件传递给的MeasureSpec的mode是MeasureSpec.AT_MOST，就说明父控件希望自己不要超出MeasureSpec.getSize(measureSpec)，如果超出了，就选择MeasureSpec.getSize(measureSpec)，否则用自己想要的尺寸就行了

对于FlowLayout，可以假设每个子View都可以充满FlowLayout，因此，可以直接用measureChildren测量所有的子View的尺寸：

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {

        int widthSize = MeasureSpec.getSize(widthMeasureSpec);
        int paddingLeft = getPaddingLeft();
        int paddingRight = getPaddingRight();
        int paddingBottom = getPaddingBottom();
        int paddingTop = getPaddingTop();
        int count = getChildCount();
        int maxWidth = 0;
        int totalHeight = 0;
        int lineWidth = 0;
        int lineHeight = 0;
        int extraWidth = widthSize - paddingLeft - paddingRight;
        
        <!--直接用measureChildren测量所有的子View的高度-->
        measureChildren(widthMeasureSpec, heightMeasureSpec);
        
        <!--现在可以获得所有子View的尺寸-->
        
        for (int i = 0; i < count; i++) {
            View view = getChildAt(i);
            if (view != null && view.getVisibility() != GONE) {
                if (lineWidth + view.getMeasuredWidth() > extraWidth) {
                    totalHeight += lineHeight ;
                    lineWidth = view.getMeasuredWidth();
                    lineHeight = view.getMeasuredHeight();
                    maxWidth = widthSize;
                } else {
                    lineWidth += view.getMeasuredWidth();
                }
                <!--获取每行的最高View尺寸-->
                lineHeight = Math.max(lineHeight, view.getMeasuredHeight());
            }
        }
        totalHeight = Math.max(totalHeight + lineHeight, lineHeight);
        maxWidth = Math.max(lineWidth, maxWidth);
        
        <!--totalHeight 跟 maxWidth都是FlowLayout渴望得到的尺寸-->
        <!--至于合不合适，通过resolveSize再来判断一遍，当然，如果你非要按照自己的尺寸来，也可以设定，但是不太合理-->
        totalHeight = resolveSize(totalHeight + paddingBottom + paddingTop, heightMeasureSpec);
        lineWidth = resolveSize(maxWidth + paddingLeft + paddingRight, widthMeasureSpec);
        setMeasuredDimension(lineWidth, totalHeight);
    }

可以看到，设定自定义ViewGroup的尺寸其实只需要三部：

* 测量所有子View，获取所有子View的尺寸
* 根据自身特点计算所需要的尺寸
* 综合考量需要的尺寸跟父控件传递的MeasureSpec，得出一个合理的尺寸

# 顶层View的MeasureSpec是谁指定

传递给子View的MeasureSpec是父容器根据自己的MeasureSpec及子View的布局参数所确定的，那么根MeasureSpec是谁创建的呢？我们用最常用的两种Window来解释一下，Activity与Dialog，DecorView是Activity的根布局，传递给DecorView的MeasureSpec是系统根据Activity或者Dialog的Theme来确定的，也就是说，最初的MeasureSpec是直接根据Window的属性构建的，一般对于Activity来说，根MeasureSpec是EXACTLY+屏幕尺寸，对于Dialog来说，如果不做特殊设定会采用AT_MOST+屏幕尺寸。这里牵扯到WindowManagerService跟ActivityManagerService，感兴趣的可以跟踪一下WindowManager.LayoutParams ，后面也会专门分析一下，比如如何最简单试下全屏的Dialog就跟这些知识相关。
