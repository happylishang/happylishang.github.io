---
layout: post
title: "View的测量原理与自定义View尺寸处理"
category: View
 

---


#### MeasureSpec详细解析



#### Scrowller及OverScowller使用及平滑处理

Scroll其实类似于ValueeAnimator，只是个工具类，就提的更新交给用户自己 

#### 不同层次的手势处理


####  getMeasuredHeight()与getHeight的区别

	The size of a view is expressed with a width and a height. A view actually possess two pairs of width and height values.
	
	The first pair is known as measured width and measured height. These dimensions define how big a view wants to be within its parent (see Layout for more details.) The measured dimensions can be obtained by calling getMeasuredWidth() and getMeasuredHeight().
	
	The second pair is simply known as width and height, or sometimes drawing width and drawing height. These dimensions define the actual size of the view on screen, at drawing time and after layout. These values may, but do not have to, be different from the measured width and height. The width and height can be obtained by calling getWidth() and getHeight().

实际上在当屏幕可以包裹内容的时候，他们的值相等，只有当view超出屏幕后，才能看出他们的区别：getMeasuredHeight()是实际View的大小，与屏幕无关，而getHeight的大小此时则是屏幕的大小。当超出屏幕后，getMeasuredHeight()等于getHeight()加上屏幕之外没有显示的大小
 