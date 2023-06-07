如果自己实现一个支持嵌套滚动的框架，需要注意的是：全局拦截+保持唯一

*  TotalDragDistance 通过一个滚动距离全局控制子View的scroll表现
* 1：拦截后无法继续透传，联动需依赖全局拦截
* 2：为了防止无效fling：加入方向判断
* 3：overscroll体验好于scroller
* 4：无需自己通过VelocityTracker统计速度，GestureDetector够用
* 5：底部顶部可以加入可滚动判断，提前结束fling，反正跳帧
* 6：不的状态记得对确定状态校准，防止抖动
*  7： OverScroller fling之后，注意即可invalid否则可能无法触发compute

## NestScrollView与ScrollView都是依赖全展开的基础上的

NestScrollView作为嵌套滚动的父容器不是一个很好的选择，作为子容器还行


## NestScroll特点

### 父类
//垂直滚动的ScrollView嵌套recyvleview
//FrameLayout对于自定垂直的布局很方便
//自定义父类其实也要拦截底部的，不然不好处理fling，或者说，也是类似的拦截，尤其衔接的问题，
// target越界了，先放的第二任衔接的不是原来的target如何处理衔接问题呢，没有子target可给了
//很少有万能的自定义View，看场景实现吧


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
  
  如果是View，那么只能onTouchEvent处理，但是写法类似，down的时候start，drag的时候，dispatchNestedPreScroll，其实并非要实现所有方法，基本上只有两个，一个是drag一个是fling、
        
### 如何自定义一个NestedScrollingParent类

 * Touch处理仍旧无法省略，必须计算滚动距离，dispatchTouchEvent或者onTouch都行，dispatchTouchEvent好一些
 * GestureDetector用于传递fling
 * NestedScrollingChildHelper用于处理拖动传递
 * 不过好处是跟父控件都可以省略intercept之类的逻辑
 * ACTION_MOVE
 