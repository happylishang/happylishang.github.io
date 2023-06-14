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

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/67a57dcd3a4a40c99f472d95b6b1ec04~tplv-k3u1fbpfcp-watermark.image?)

### NestedScrolling只处理拖动[target无法改变]，Fling交给Parent处理

在这个框架中，**子View必须主动启动嵌套滑动**、并且在Move的时候**主动请求父ViewGroup进行处理**，这样才能完成协同，并非简单的打开开关，所有的定制逻辑仍旧需要开发者自己处理，只是替代了onInterceptTouchEvent，提供了子View回传事件给父View的能力，不用父View主动拦截，也能获取接管子View事件的能力。

以开头描述的场景为例，如果上部分用ScrollView下部分用WebView，那么必须将两者都改造成NestedScrollingChild，也就是NestedScrollView与NestedWebView，NestedScrollView谷歌已经提供，NestedWebView目前没有，需要自己封装，可以看看如何配合实现一套嵌套滑动交互:


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

1.  setNestedScrollingEnabled(true) ,后续的dispatch都依赖该开关
2. 子View收到DOWN事件的时候启动startNestedScroll(ViewCompat.SCROLL_AXIS_VERTICAL)，【无论有没有NestedScrollingParent】
2. 父布局其实这个时候也会响应，只有存在支持嵌套滑动的父布局，后续dispatchNestedPreScroll等函数才有意义才有意义
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

### NestedScrolling框架 使用注意点

1.    fling处理：尽量不使用内层GestureDetector来获取，因为内外侧获取MotionEvent不是统一的，所以内外层获取的fling初始速度可能不同，衔接易出问题，还是统一给外层自己做
2.   **move处理拖拽：尽量使用rawY**，因为MotionEvent获取的Y在嵌套滚动时候不如rawY直观，rawY始终是相对屏幕，而Y是相对自己View，在父View进行滚动的时候，target的Y几乎是不动的

## 强大的RecyclerView

**RecyclerView适配一切**，利用RecyclerView内嵌WebView也能实现上述效果：**但需要主动控制内部可滚动Item**。RecyclerView自身实现了onInterceptTouchEvent逻辑，理论上内部子View是无法获取到拦截之后的事件，只能依赖外部主动控制，否则WebView被拖到顶部就结束了，内部无法继续拖拽。但是RecyclerView本身实现了NestedScrollingChild3，可以看做是一个支持嵌套滑动的Child，在NestedScrolling框架中Move事件时一般会**直接调用dispatchNestedPreScroll**，之后dispatchNestedPreScroll会区分是否能启用嵌套滑动。因此除了借助 onInterceptTouchEvent逻辑，还可以借助dispatchNestedPreScroll来处理，一种很猥琐的做法：**继承RecyclerView，复写dispatchNestedPreScroll**，这个时候**继承类先父类RecyclerView获取事件处理的优先权**，在一定程度上看做实现了onInterceptTouchEvent的NestedScrollingChild，在复写的dispatchNestedPreScroll种处理子View的滚动。即可自身滚动，也能控制内部可滚动View的滚动，**但很难做到那么通用**。不过在做业务的时候，思路有时候胜过单纯的技术，尤其嵌套滑动，不需要过分追求通用型控件。

对于上述交互场景，只需在dispatchNestedPreScroll做如下处理：**只需要主动接手内部子View的操控，外部的操控无需处理**

	 override fun dispatchNestedPreScroll( dx: Int,  dy: Int,  consumed: IntArray?,  offsetInWindow: IntArray?,  type: Int
	    ): Boolean {
		    var consumedSelf = false
		    // 先让父布局处理，还是后处理？
		    val parentScrollConsumed = mParentScrollConsumed
		    val parentConsumed = super.dispatchNestedPreScroll(    dx,  dy,   parentScrollConsumed,   offsetInWindow,   type
		    )
		    consumed?.let {
		        consumed[1] += parentScrollConsumed[1]
		    }
		    <!--再交给自己已处理-->
		    if (type == ViewCompat.TYPE_TOUCH) {
			            <!--对于向上滚动，如果自身可是滚动就直接滚动自身，说明还没到顶部RecyclerView会自己处理，自己拦截过了无需外部干预，如果自身不能滚，就滚动内部的可滚动target-->
	                if (!canScrollVertically(1)) {//外部自身的操控无需处理
	                		<!--fetchNestedChild是用来获取内部的可滚动View，这个看具体业务操作-->
		        val remain = dy - (consumed?.get(1) ?: 0)
		        if (remain > 0) {
		            //  已经到顶了
		            if (!canScrollVertically(1)) {
		                val target = fetchBottomNestedScrollChild()
		                target?.apply {
		                    this.scrollBy(0, remain)
		                    consumed?.let {
		                        it[1] += remain
		                    }
		                    consumedSelf = true
		                }
		            }
		        }
		        // down 其实还是自己控制，而不是底层控制
		        if (remain < 0) {
		            val target = fetchBottomNestedScrollChild()
		            target?.apply {
		                if (this.canScrollVertically(-1)) {
		                    this.scrollBy(0, remain)
		                    //   消耗完，不给底层机会
		                    consumed?.let {
		                        it[1] += remain
		                    }
		                    consumedSelf = true
		                }
		            }
		        }
		    }
		    return consumedSelf || parentConsumed
	}

拖拽是比较容易处理的，比较棘手的是对于fling的处理，fling是一次性的，如果Recycleview继承类dispatchNestedPreFling自己处理了fling后，父布局就获取不到，衔接就比较麻烦

	   override fun dispatchNestedPreFling(velocityX: Float, velocityY: Float): Boolean {
	        fling(velocityY)
	        return true
	    }

如果Recycleview自身通过OverScroller处理完毕后，还有盈余，就需要将盈余给外部，先处理内部，还是先处理外部都是可选的，看用户自己

	override fun computeScroll() {
	    if (overScroller.computeScrollOffset()) {
	        val current = overScroller.currY
	        val dy = current - mCurrentFling
	        mCurrentFling = current
	
	        val target = fetchBottomNestedScrollChild()
	        if (dy > 0) {
	            if (canScrollVertically(1)) {
	                scrollBy(0, dy)
	            } else {
	                if (target?.canScrollVertically(1) == true) {
	                    target.scrollBy(0, dy)
	                } else {
	                    if (!overScroller.isFinished) {
	                        overScroller.abortAnimation()
	                        // fling 先内部，给上面接管一部分
			       startNestedScroll(ViewCompat.SCROLL_AXIS_VERTICAL)
		   		 dispatchNestedFling(0f, overScroller.currVelocity, false)
		    		stopNestedScroll()
	                    }

处理方式就是内部不可fling之后，主动通过startNestedScroll(ViewCompat.SCROLL_AXIS_VERTICAL)与 dispatchNestedFling再次交给父布局。



### 总结

流畅交互靠微调