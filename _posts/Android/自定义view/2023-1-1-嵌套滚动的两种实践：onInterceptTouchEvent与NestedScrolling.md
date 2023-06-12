嵌套滚动：内外两层均可滚动，比如上半部分是一个有限的列表，下半部分是WebView，在内层上半部分展示到底的时候，外部父布局整体滚动内部View，将底部WevView拉起来，滚动到顶部之后再将滚动交给内部WebView，之后滚动的就是内部WebView，如下图：

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/5462bff1df454fa78f0f3154cc9cfae9~tplv-k3u1fbpfcp-watermark.image?)

## 实现：onInterceptTouchEvent或者NestedScroll

按照上下两部分构建父布局，父ViewGroup建议继承FrameLayout/RelativeLayout来实现，方便处理测量[无需复写]与布局，在计算出全部View高度后，可以计算最大父布局滚动距离：

    override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {
        var top = t
        var bottom = b
        for (i in 0 until childCount) {
            getChildAt(i).layout(l, top, r, bottom)
            top += getChildAt(i).measuredHeight
            bottom += getChildAt(i).measuredHeight
            totalHeight += getChildAt(i).measuredHeight
        }
        maxScrollHeight = totalHeight - measuredHeight
    }

上述交互有两种比较常用的方式，一种是onInterceptTouchEvent全局拦击Touch事件来实现拖动与Fling的处理，另一种是借助后期推出的NestedScroll框架来实现。先简单看下传统的onInterceptTouchEvent拦截的方式：核心的处理事两个操作，一个是拖动、一个是UP后的Fling，onInterceptTouchEvent首先要确定拦截的时机：判断有效拖动
 
    override fun onInterceptTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                mLastY = event.rawY
                mDownY = event.rawY
                mDownX = event.rawX
                mBeDraging = false
            }
            MotionEvent.ACTION_MOVE -> if (abs(event.rawY - mDownY) >  ViewConfiguration.get(
                    context
                ).scaledTouchSlop) {
                mBeDraging = true
                return true
            } else {
                mLastY = event.rawY
            }
            else -> {}
        }
        return super.onInterceptTouchEvent(event)
    }
       
一般而言垂直滚动超过某个TouchSlop就可以认为拖动有效，拖动开始，子View后续无法获取到Touch事件，其实大多数场景而言，父布局接管之后，没有必要再给子View分发事件，之后自行处理拖拽与Fling。

## onInterceptTouchEvent方式处理拖拽与fling

处理拖拽与fling需要注意衔接，所以需要准备好GestureDetector，用于将来的fling，建议放在dispatchTouchEvent整体处理事件的消费

    override fun dispatchTouchEvent(ev: MotionEvent?): Boolean {
        if (ev != null) {
            gestureDetector.onTouchEvent(ev)
        }
        if (ev?.action == MotionEvent.ACTION_DOWN)
            overScrollerNest.abortAnimation()

        if (ev?.action == MotionEvent.ACTION_MOVE && mBeDraging) {
            scrollInner((mLastY - ev.rawY).roundToInt())
            mLastY = ev.rawY
        }
        return super.dispatchTouchEvent(ev)
    }

拖拽的控制方式：自己计算出可滚动的距离，可以利用View的canScrollVertically判断View是否能消费，从而决定留给哪个View

	  private fun scrollInner(dy: Int) {
	        var pConsume: Int = 0
	        var cConsume: Int = 0
	        if (dy > 0) {
	            if (scrollY in 1 until maxScrollHeight) {
	                pConsume = dy.coerceAtMost(maxScrollHeight - scrollY)
	                scrollBy(0, dy)
	                cConsume = dy - pConsume
	                if (bottomView.canScrollVertically(cConsume) && cConsume != 0) {
	                    bottomView.scrollBy(0, cConsume)
	                }
	            } else if (scrollY == 0) {
	                bottomView.scrollTo(0, 0)
	                if (upView.canScrollVertically(dy)) {
	                    upView.scrollBy(0, dy)
	                } else {
	                    if (canScrollVertically(dy)) {
	                        scrollBy(0, dy)
	                    }
	                }
	            } else if (scrollY >= maxScrollHeight) {
	                scrollTo(0, maxScrollHeight)
	                if (bottomView.canScrollVertically(dy)) {
	                    bottomView.scrollBy(0, dy)
	                } else {
	                    overScrollerNest.abortAnimation()
	                }
	            }
	        } else {
	            if (scrollY in 1 until maxScrollHeight) {
	                pConsume = Math.max(dy, -scrollY)
	                scrollBy(0, pConsume)
	                cConsume = dy - pConsume
	                if (bottomView.canScrollVertically(cConsume)) {
	                    bottomView.scrollBy(0, cConsume)
	                }
	            } else if (scrollY == maxScrollHeight) {
	                if (bottomView.canScrollVertically(dy)) {
	                    bottomView.scrollBy(0, dy)
	                } else {
	                    if (canScrollVertically(dy)) {
	                        scrollBy(0, dy)
	                    }
	                }
	            } else {
	                if (upView.canScrollVertically(dy)) {
	                    upView.scrollBy(0, dy)
	                }
	                bottomView.scrollTo(0, 0)
	            }
	        }
	        invalidate()
	    }
	    
拖拽结束后，fling跟上利用GestureDetector的onFling，直接让Scroller接上即可  overScroller = OverScroller(context)，一般用OverScroller的体验好一些：
       
    private GestureDetector gestureDetector = new GestureDetector(getContext(), new GestureDetector.SimpleOnGestureListener() {

        @Override
        public boolean onFling(@NonNull MotionEvent e1, @NonNull MotionEvent e2, float velocityX, float velocityY) {
            if (!(Math.abs(e1.getX() - e2.getX()) > mTouchSlop && Math.abs(velocityX) > Math.abs(velocityY))) {
           	    <!--衔接滚动-->
                overScroller.fling(0, 0, 0, (int) velocityY, 0, 0, -10 * ScreenUtil.getDisplayHeight(), 10 * ScreenUtil.getDisplayHeight());
                <!--必须触发一次-->
                postInvalidate();
            }
            return super.onFling(e1, e2, velocityX, velocityY);
        }
    });

在computeScroll里从新计算应该滚动的距离，可以看到全局接管，并利用scrollBy自行控制滚动的偏移量是这种方案的核心

	    var mLastOverScrollerValue = 0
	    override fun computeScroll() {
	        super.computeScroll()
	        if (overScrollerNest.computeScrollOffset()) {
	            scrollInner(overScrollerNest.currY - mLastOverScrollerValue)
	            mLastOverScrollerValue = overScrollerNest.currY
	            invalidate()
	        }
	    }    
    
如此就可以利用onInterceptTouchEvent实现嵌套滚动，不涉及太多内部View【仅仅是获取了内部View的高度及判断是否可滚】，一切交给父布局即可。


## 利用NestedScrolling框架实现嵌套滑动

Android5.0推出了嵌套滑动机制NestedScrolling，让**父View和子View在滑动时相互协调配合**，为了向前兼容又抽离了NestedScrollingChild、NestedScrollingParent、NestedScrollingChildHelper、NestedScrollingParentHelper等支持类，不过在23年的场景下基本不需要使用这些辅助类了。NestedScrolling的核心是子View一直能收到Move事件，在自己处理之前先交给父View消费，父View处理完之后，再将余量还给子View，让子View自己处理，可以看出这套框架必须**父子配合**，也就是NestedScrollingChild、NestedScrollingParent是配套的。5.0之后View与ViewGroup本身就实现了NestedScrollingChild+NestedScrollingParent的框架，自定义布局的时候只需要定制与启用，也就是必须进行二次开发，目前Google提供的最好用的就是RecyclerView。有张图很清晰的描述NestedScrolling框架是如何工作的：

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/306ad9ff52e24df1aeb7da0f9e011728~tplv-k3u1fbpfcp-watermark.image?)

### NestedScrolling只处理拖动[target无法改变]，Fling交给Parent处理

在这个框架中，**子View必须主动启动嵌套滑动**、并且在Move的时候**主动请求父ViewGroup进行处理**，这样才能完成协同，并非简单的打开开关，所有的定制逻辑仍旧需要开发者自己处理，只是替代了onInterceptTouchEvent，提供了子View回传事件给父View的能力，不用父View主动拦截，也能获取接管子View事件的能力。

以开头描述的场景为例，如果上部分用ScrollView下部分用WebView，那么必须将两者都改造成NestedScrollingChild，也就是NestedScrollView与NestedWebView，NestedScrollView谷歌已经提供，NestedWebView目前没有，需要自己封装，可以看看如何配合实现一套嵌套滑动交互


	class NetScrollWebView @JvmOverloads constructor(
	    context: Context, attrs: AttributeSet? = null,
	) : WebView(context, attrs) {
	
	    private val mTouchSlop = android.view.ViewConfiguration.get(context).scaledTouchSlop
	    private val mScrollOffset = IntArray(2)
	    private val mScrollConsumed = IntArray(2)
	    init {
	    <!--启动嵌套滑动-->
	        isNestedScrollingEnabled = true
	    }
	    
	    private var mLastY: Float = 0f
	    private var dragIng: Boolean = false
	    override fun dispatchTouchEvent(ev: MotionEvent?): Boolean {
	        when (ev?.action) {
	            MotionEvent.ACTION_MOVE -> {
	                if (abs(ev.rawY - mLastY) > mTouchSlop) {
	                    dragIng = true
	                } else {
	                    super.dispatchTouchEvent(ev)
	                }
	                if (dragIng) {
	                    if (parent != null) {
	                        parent.requestDisallowInterceptTouchEvent(true)
	                    }
	                    <!--主动调用dispatchNestedPreScroll，请求父容器处理-->
	                    dispatchNestedPreScroll(
	                        0, (mLastY - ev.rawY).toInt(), mScrollConsumed, mScrollOffset
	                    )
	                    mLastY = ev.rawY
	                }
	            }
	            MotionEvent.ACTION_DOWN -> {
	                dragIng = false
	                super.dispatchTouchEvent(ev)
	                <!--startNestedScroll启动嵌套滑动-->
	                startNestedScroll(ViewCompat.SCROLL_AXIS_VERTICAL)
	                mScrollConsumed[1] = 0
	                mLastY = ev.rawY
	            }
	          MotionEvent.ACTION_UP -> 
                stopNestedScroll()
		    else -> super.dispatchTouchEvent(ev)
	        }
	        return true
	    }
	
	    // 强制自己不消费move
	    override fun onTouchEvent(ev: MotionEvent?): Boolean {
	        if (dragIng || ev?.action == MotionEvent.ACTION_MOVE)
	            return false
	        return super.onTouchEvent(ev)
	    }

	}

1. 在子View收到DOWN事件的时候，开启嵌套滑动startNestedScroll(ViewCompat.SCROLL_AXIS_VERTICAL)
2. 父布局其实这个时候也会响应，只有存在支持嵌套滑动的父布局，后续的dispatchNestedPreScroll等函数才有意义才有意义
3. 假设存在支持嵌套滑动的父布局，在MOVE的时候，调用dispatchNestedPreScroll让父布局处理
4.  在MotionEvent.ACTION_UP的时候，stopNestedScroll ，由于WebView是ViewGroup，所以可以直接在dispatchTouchEvent处理，如果是View可以在onTouchEvent中处理

如此一个简单的NestedScrollingChild就完成了，但是只有这个并不能完成上述需求，还需要一个NestedScrollingParent来配合，其实这里大部分的功能跟上述onInterceptTouchEvent的实现的类似，只不过

		class NestUpDownTwoPartsScrollView @JvmOverloads constructor(
	    context: Context,
	    attrs: AttributeSet? = null,
	    defStyleAttr: Int = 0,
	    defStyleRes: Int = 0,
	) : FrameLayout(context, attrs, defStyleAttr, defStyleRes) {
	
	
	    private val gestureDetector: GestureDetector =
	        GestureDetector(context, object : GestureDetector.SimpleOnGestureListener() {
	
	            override fun onFling(
	         	   与onInterceptTouchEvent一致
	              ...
	        })
 	    ...
 	    <!--标志父布局支持垂直的嵌套滑动-->
 	    
	    override fun onStartNestedScroll(child: View, target: View, nestedScrollAxes: Int): Boolean {
	        return nestedScrollAxes == ViewCompat.SCROLL_AXIS_VERTICAL
	    }
		<!--被NestedScrollingChild回调-->
	    override fun onNestedPreScroll(target: View, dx: Int, dy: Int, consumed: IntArray) {
	        overScrollerNest.abortAnimation()
	        scrollInner(dy)
	        <!--完全给消费-->
	        consumed[1] = dy
	    }
	<!--拦截子View们的fling-->	
	
	    override fun onNestedPreFling(target: View, velocityX: Float, velocityY: Float): Boolean {
	        //  获取的fling速度有差异，原因不详
	        return true
	    }
	
	    var mLastOverScrollerValue = 0
	    <!--自己处理fling-->
	    override fun computeScroll() {
	        super.computeScroll()
	         <!--自己处理fling-->
	        if (overScrollerNest.computeScrollOffset()) {
	            scrollInner(overScrollerNest.currY - mLastOverScrollerValue)
	            mLastOverScrollerValue = overScrollerNest.currY
	            invalidate()
	        }
	    }
	
	    private lateinit var overScrollerNest: OverScroller
	
	    override fun computeVerticalScrollRange(): Int {
	        return totalHeight
	    }
	
	    private fun scrollInner(dy: Int) {
	       ...	  与onInterceptTouchEvent一致
	    }
	
	    override fun dispatchTouchEvent(ev: MotionEvent?): Boolean {
	        if (ev != null) {
	            gestureDetector.onTouchEvent(ev)
	        }
	        if (ev?.action == MotionEvent.ACTION_DOWN)
	            overScrollerNest.abortAnimation()
	        return super.dispatchTouchEvent(ev)
	    }
	}

父布局的操作如下

1. onStartNestedScroll 返回true 启动
2. 被子View调用onNestedPreScroll开始协同滑动
3. onNestedPreFling接管Fling
4. 利用GestureDetector+OverScroller自行处理Fling

可以看到，在这个框架下，可以比较灵活的接管拖动，不用自己拦截，而且消费多少，可以父子协商，关于Fling，可以处理成一致，而且有两个滚动布局衔接的时候，交给外部统一处理应该也是最合理的做法，防止两个View的Scroller不一致，而且嵌套滑动也无法处理target切换的问题。

### NestedScrolling 使用注意点

1.    fling处理：尽量不使用内层GestureDetector来获取，因为内外侧获取MotionEvent不是统一的，所以内外层获取的fling初始速度可能不同，衔接易出问题，还是统一给外层自己做
2.   **move处理拖拽：尽量使用rawY**，因为MotionEvent获取的Y在嵌套滚动时候不如rawY直观，rawY始终是相对屏幕，而Y是相对自己View，在父View进行滚动的时候，target的Y几乎是不动的

## 强大且万能的RecyclerView

**RecyclerView适配一切**，所有的嵌套滑动都能用RecyclerView来处理，

一种很狗的NestedScrollingParent写法：利用NestedScrollingChild做NestedScrollingParent

思路有时候胜过单纯的技术，RecyclerView自己继承自己的典范，拦截后，多余的交给自己，有点类似于自己做自己的父布局，先看看消费不消费，之后交给子View，或者交给自己的后续处理流程

	 override fun dispatchNestedPreScroll(
	        dx: Int,
	        dy: Int,
	        consumed: IntArray?,
	        offsetInWindow: IntArray?,
	        type: Int
	    ): Boolean {
	        var consumedSelf = false
	        if (type == ViewCompat.TYPE_TOUCH) {
	            // up
	            if (dy > 0) {
	                if (!canScrollVertically(1)) {
	                    val target = fetchNestedChild()
	                    target?.apply {
	                        this.scrollBy(0, dy)
	
	                        consumed?.let {
	                            it[1] = dy
	                        }
	
	                        consumedSelf = true
	                    }
	                }
	            }
	            // down
	            if (dy < 0) {
	                val target = fetchNestedChild()
	                target?.apply {
	                    if (this.canScrollVertically(-1)) {
	                        this.scrollBy(0, dy)
	
	                        consumed?.let {
	                            it[1] = dy
	                        }
	
	                        consumedSelf = true
	                    }
	                }
	            }
	        }
	
	        // Now let our nested parent consume the leftovers
	        val parentScrollConsumed = mParentScrollConsumed
	        val parentConsumed = super.dispatchNestedPreScroll(dx, dy - (consumed?.get(1)?:0), parentScrollConsumed, offsetInWindow, type)
	        consumed?.let {
	            consumed[1] += parentScrollConsumed[1]
	        }
	        return consumedSelf || parentConsumed
	    }
	    
	    
##  嵌套滚动中RecyclerView万能：RecyclerView原则只能嵌套一个可滚动的东西，

哪怕是RecyclerView嵌套RecyclerView，只要外层的RecyclerView是高度有限的，内层就很好控制，向上外层先开始滚动，向下的话，先判断是不是内层可滚动，如果不可在滚动外层，这些是策略
RecyclerView里面就没必要用ScrollVIew了，或者说有了RecyclerView，ScrollVIew可以退出历史舞台了。

思路有时候胜过单纯的技术，RecyclerView自己继承自己的典范，拦截后，多余的交给自己，有点类似于自己做自己的父布局，先看看消费不消费，之后交给子View，或者交给自己的后续处理流程

 

### NestScroll在处理协同滚动的时候比较合理，尤其是多个部位，滚动速度不一致的时候也许比较合适 A滚动B的2被动时候，A隐藏，顶部隐藏

比如吸顶


### 内外层获取MotionEvent不是统一的

MotionEvent在传递的时候，中间有层层处理，获取的fling速度不一定一样，最好还是外层统一处理fling比较稳妥，这样也可以避免上下两个View衔接的时候，不同的fling问题，可以保持fling速度一致，

或者父View只计算速度，目前来看外部父容器自己统一处理fling是比较好的操作，不同子View的位置可以灵活自己控制。嵌套只处理拖动


MotionEvent.getRawY 与MotionEvent.getY有区别，如果是onInterceptTouchEvent由于是外层拦截，其实没差别，如果是嵌套，拖动的时候使用了内存MotionEvent.getY，那可能会有问题，外层动了，内存otionEvent.getY可能还是不变的。