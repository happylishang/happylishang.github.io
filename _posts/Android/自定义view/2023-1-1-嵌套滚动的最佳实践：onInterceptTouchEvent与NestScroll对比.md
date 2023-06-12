嵌套滚动：指内外两层均可滚动，比如上半部分是一个有限的列表，下半部分是WebView，在上半部分展示到底的时候，外部父布局滚动内部View，将底部WevView拉起来，滚动到顶部之后再将滚动交给内部WebView，之后滚动的就是内部WebView，如下图：

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
 
     @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {

        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
	             //一些校准操作校准初始值
                mScroller.abortAnimation();
                mLastY = event.getY();
                mBeDraging = false;
                 break;
            case MotionEvent.ACTION_MOVE:
                if (isVerticalGesture(event)) {
                    return true;
                } else {
                    mLastY = event.getY();
                }
                break;
            default:
                break;
        }
        return super.onInterceptTouchEvent(event);
    }
    
一般而言垂直滚动超过某个TouchSlop，并且垂直拖动距离超过横向拖动距离，则认为垂直拖动有效

    /**
     * 判断有效拖动
     */
    boolean isVerticalGesture(MotionEvent event) {
        return mBeDraging
                || (mVerticalGestureFlag = Math.abs(event.getY() - mDownY) > Math.abs(event.getX() - mDownX)
                && Math.abs(event.getY() - mDownY) > mTouchSlop);
    }

拖动开始，便是全局拦截Move事件的开始，子View后续无法获取到Touch事件，其实大多数场景而言，父布局接管之后，也没有必要再给子View分发事件，之后父布局自行处理拖拽与fling。

## onInterceptTouchEvent方式处理拖拽与fling

自行处理拖拽与fling需要注意衔接准备好GestureDetector，用于将来的fling，建议放在dispatchTouchEvent整体处理事件的消费

    private GestureDetector gestureDetector = new GestureDetector(getContext(), new GestureDetector.SimpleOnGestureListener() {

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        gestureDetector.onTouchEvent(event);
        return super.dispatchTouchEvent(event);
    }

滚动的控制方式：自己计算出可滚动的距离，可以利用View自身的canScrollVertically

    public boolean canScrollVertically(int direction) {
        final int offset = computeVerticalScrollOffset();
        final int range = computeVerticalScrollRange() - computeVerticalScrollExtent();
        if (range == 0) return false;
        if (direction < 0) {
            return offset > 0;
        } else {
            return offset < range - 1;
        }
    }


onInterceptTouchEvent实现嵌套滚动

如果自己实现一个支持嵌套滚动的框架，需要注意的是：全局拦截+保持唯一

*  TotalDragDistance 通过一个滚动距离全局控制子View的scroll表现
* 1：拦截后无法继续透传，联动需依赖全局拦截
* 2：为了防止无效fling：加入方向判断
* 3：overscroll体验好于scroller
* 4：无需自己通过VelocityTracker统计速度，GestureDetector够用
* 5：底部顶部可以加入可滚动判断，提前结束fling，反正跳帧
* 6：不的状态记得对确定状态校准，防止抖动
*  7： OverScroller fling之后，注意即可invalid否则可能无法触发compute


## 利用NestedScroll框架实现：只处理拖动[target无法改变]，fling全交给Parent处理

必须有NestedScrollingChild、NestedScrollingParent的配对实现，


## 如何优化


## 嵌套滚动的View必须要处理衔接，

尽管每个可滚动的View都有fling的能力，但是无法保证每个View fling采用的Scroller是相同的表现，也就是其速度可能存在衔接问题，所以，fling必须要在外部容器统一处理算是比较好的选择方案

### 嵌套滚动NestedScrollingChild、NestedScrollingParent

个人感觉是处理两个嵌套滚动的问题，一对一，比较好，牵扯到多个NestedScrollingChild的时候，这套框架在处理衔接上就力不从心，存在NestedScrollingChild-> NestedScrollingParent-> NestedScrollingChild2的问题无法处理，target不好动态更改！



## 使用NestScroll完成嵌套滚动：更灵活

### 如何自定义一个NestedScrollingChild类

NestedScrollingChild并不是直接实现一个NestedScrollingChild3就可以了，NestedScroll框架可以解决解决拦截后子View无法获取后续Touch事件的问题，其触发点都是从children开始的，实现NestedScrollingChild3的目的是为了让父类进行回调，不过启动要子View自己启动。也就是startNestedScroll其实是子View自己调用的，时机是什么时候呢？

一般而言，NestedScrollingChild需要再接收到Touch事件的时候进行消费，Android框架本身的事件传递并没有改变，只有这样事件才能保证Touch源源不断的传给自己，所以可以选择在Down事件的时候启动自己的嵌套滑动，一般可以借助NestedScrollingChildHelper，其实就是打通父子同步，找到NestedScrollingParent，之后再拖拽的时候，继续利用NestedScrollingChildHelper协同。在自己处理的时候，如果NestedScrollingChild本身是继承一ViewGroup，建议在dispatchTouchEvent，因为onTouchEvent可能不会消费Down，在将来处理fling的时候有问题，对于自己的onTouchEvent，也建议直接飞起，NestedScrollingChild真正在业务中用的时候，其实还是父容器统一处理比较好，因为通常的衔接是多个嵌套滑动的child一起，如果不是父类自己处理，对于fling的衔接经常出问题。

    override fun dispatchTouchEvent(ev: MotionEvent?): Boolean {
        super.dispatchTouchEvent(ev)
        ev?.let { gestureDetector.onTouchEvent(it) }
        when (ev?.action) {
            MotionEvent.ACTION_MOVE -> {
            <!--派发-->
                dispatchNestedPreScroll(
                    0, (mLastY - ev?.y!!).toInt(), mScrollConsumed, null,
                    ViewCompat.TYPE_TOUCH
                )
                mLastY = ev?.y!!
            }
            MotionEvent.ACTION_DOWN -> {
            	<!--启动-->
                startNestedScroll(ViewCompat.SCROLL_AXIS_VERTICAL, ViewCompat.TYPE_TOUCH)
                mLastY = ev?.y!!
            }
        }
        return true
    }
    
    override fun onTouchEvent(ev: MotionEvent?): Boolean {
        return false
    }

    private val gestureDetector: GestureDetector =
        GestureDetector(context, object : SimpleOnGestureListener() {
            override fun onFling(
                e1: MotionEvent,
                e2: MotionEvent,
                velocityX: Float,
                velocityY: Float
            ): Boolean {
                return chileNestHelper.dispatchNestedPreFling(velocityX, -velocityY)
            }
        })
  
 如果是View，那么只能onTouchEvent处理，但是写法类似，down的时候start，drag的时候，dispatchNestedPreScroll，其实并非要实现所有方法，基本上只有两个，一个是drag一个是fling，通常来讲，在真正的业务中，子View只负责scrollBy或者scrollTo就可以了，自身不需要处理fling之类的操作，全部由NestedScrollingParent来协同。
        
### 如何自定义一个NestedScrollingParent类

NestedScrollingParent一般而言无需自己处理Touch事件，NestedScrollingParent属于被动响应类型的ViewGroup，所有的事件可以认为来自子View，因为你一旦选择用NestedScroll框架，势必要child/parent配合，所有NestedScrollingParent一般不需要重写dispatchTouchEvent，但是为了处理未完成的fling，一般在down的时候，需要将OverScroller停止，

    override fun dispatchTouchEvent(ev: MotionEvent?): Boolean {
        if (ev?.action == MotionEvent.ACTION_DOWN)
            overScrollerNest.abortAnimation()
        return super.dispatchTouchEvent(ev)
    }

一般我们不会像NestScrollView一样将View整个展开，那样基本上也就失去了意义，比如将recyclerview全展开，recyclerview就无法复用，浪费资源，一般可使用FrameLayout作为自定义NestedScrollingParent的父类，所有的View都可以获取到最大延展布局，方便测量与布局，NestedScrollingParent种计算可便宜的高度，与控制子View的滚动距离仍旧是不可避免的，当NestedScrollingChild传递dispatchNestedPreScroll将scroll信息传递过来的时候，NestedScrollingParent需要判断子View是否可以滚动，如果不可以就自己滚动，同时也要处理滚动过界让后一个View衔接的问题。在drag结束，还需要处理NestedScrollingChild的dispatchNestedPreFling，一般而言，这个时候不能让自View自己fling，衔接会有问题，所以NestedScrollingParent可以直接接管，整体处理。

     override fun onNestedPreFling(target: View, velocityX: Float, velocityY: Float): Boolean {
        mLastOverScrollerValue = 0
        overScrollerNest.fling(
            0, 0, velocityX.toInt(),
            velocityY.toInt(), 0, 0, -totalHeight * 10, totalHeight * 10
        )
        //  这里必须加上，不然可能无法触发
        invalidate()
        return true
    }
 
 在处理滚动时候，可以认为NestedScrollingParent消耗了所有的滚动，屏蔽NestedScrollingChild自己滚动的能力
 
     override fun onNestedPreScroll(target: View, dx: Int, dy: Int, consumed: IntArray, type: Int) {
        overScrollerNest.abortAnimation()
        scrollInner(dy)
        consumed[1] = dy
    }

 全部交给NestedScrollingParent控制滚动距离，其实这等同于onInterceptTouchEvent的拦截方式，并没有多大的提升，就Google框架而言，其实也没有很好的NestedScrollingParent例子，NestScrollView应该是个失败的例子，全展开，跟ScrollView有什么区别。
 
     private fun scrollInner(dy: Int) {
        var pConsume: Int = 0
        var cConsume: Int = 0
        if (dy > 0) {
            if (scrollY in 1 until measuredHeight) {
                pConsume = Math.min(dy, measuredHeight - scrollY)
                scrollBy(0, pConsume)
                cConsume = dy - pConsume
                if (bottomView.canScrollVertically(cConsume)) {
                    bottomView.scrollBy(0, cConsume)
                }
                upView.scrollTo(0, measuredHeight)
            } else if (scrollY == 0) {
                bottomView.scrollTo(0, 0)
                if (upView.canScrollVertically(dy)) {
                    upView.scrollBy(0, dy)
                } else {
                    if (canScrollVertically(dy)) {
                        scrollBy(0, dy)
                    }
                }
            } else if (scrollY == measuredHeight) {
                upView.scrollTo(0, measuredHeight)
                if (bottomView.canScrollVertically(dy)) {
                    bottomView.scrollBy(0, dy)
                } else {
                    if (canScrollVertically(dy)) {
                        scrollBy(0, dy)
                    }
                }
            }
        } else {
            if (scrollY in 1 until measuredHeight) {
                pConsume = Math.max(dy, -scrollY)
                scrollBy(0, pConsume)
                cConsume = dy - pConsume
                upView.scrollTo(0, measuredHeight)
                if (bottomView.canScrollVertically(cConsume)) {
                    bottomView.scrollBy(0, cConsume)
                }
            } else if (scrollY == measuredHeight) {
                upView.scrollTo(0, measuredHeight)
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
    }
    
不过同onInterceptTouchEvent相比，NestScroll框架更灵活，毕竟NestScroll可以在两侧同时处理自己需要的操作，而onInterceptTouchEvent往往之能依赖Parent，child的空间太小。不过对于fling的衔接，NestScroll也只能定制，因为target: View在衔接的时候，是变化的

### 一种很狗的NestedScrollingParent写法：利用NestedScrollingChild做NestedScrollingParent

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


### 嵌套滚动尽量采用rawY  禁止自己处理MOVE，这样能防止抖动

一边滚动，一遍处理嵌套滚动的时候，rawY优于Y  Y 是相对于当前View的位置， rawY可以计算绝对偏移


### NestScroll在处理协同滚动的时候比较合理，尤其是多个部位，滚动速度不一致的时候也许比较合适 A滚动B的2被动时候，A隐藏，顶部隐藏

比如吸顶


### 内外层获取MotionEvent不是统一的

MotionEvent在传递的时候，中间有层层处理，获取的fling速度不一定一样，最好还是外层统一处理fling比较稳妥，这样也可以避免上下两个View衔接的时候，不同的fling问题，可以保持fling速度一致，

或者父View只计算速度，目前来看外部父容器自己统一处理fling是比较好的操作，不同子View的位置可以灵活自己控制。嵌套只处理拖动


MotionEvent.getRawY 与MotionEvent.getY有区别，如果是onInterceptTouchEvent由于是外层拦截，其实没差别，如果是嵌套，拖动的时候使用了内存MotionEvent.getY，那可能会有问题，外层动了，内存otionEvent.getY可能还是不变的。