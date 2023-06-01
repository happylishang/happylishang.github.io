如果自己实现一个支持嵌套滚动的框架，需要注意的是：全局拦截+保持唯一

*  TotalDragDistance 通过一个滚动距离全局控制子View的scroll表现
* 1：拦截后无法继续透传，联动需依赖全局拦截
* 2：为了防止无效fling：加入方向判断
* 3：overscroll体验好于scroller
* 4：无需自己通过VelocityTracker统计速度，GestureDetector够用
* 5：底部顶部可以加入可滚动判断，提前结束fling，反正跳帧
* 6：不的状态记得对确定状态校准，防止抖动