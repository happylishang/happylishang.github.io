## 使用onInterceptTouchEvent实现嵌套滚动

如果自己实现一个支持嵌套滚动的框架，需要注意的是：全局拦截+保持唯一

*  TotalDragDistance 通过一个滚动距离全局控制子View的scroll表现
* 1：拦截后无法继续透传，联动需依赖全局拦截
* 2：为了防止无效fling：加入方向判断
* 3：overscroll体验好于scroller
* 4：无需自己通过VelocityTracker统计速度，GestureDetector够用
* 5：底部顶部可以加入可滚动判断，提前结束fling，反正跳帧
* 6：不的状态记得对确定状态校准，防止抖动
*  7： OverScroller fling之后，注意即可invalid否则可能无法触发compute


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
    
不过同onInterceptTouchEvent相比，NestScroll框架更灵活，毕竟NestScroll可以在两侧同时处理自己需要的操作，而onInterceptTouchEvent往往之能依赖Parent，child的空间太小。不过对于fling的衔接，NestScroll也只能定制，因为target: View在衔接的时候，是变化的。