---
layout: post
title: "View的测量原理与自定义View尺寸处理"
category: View
 

---


#### MeasureSpec详细解析

1、根据Activity的Theme，获取最顶层Window的参数，构建顶层MeasureSpec，传递给下层
2、构建View

#### Scrowller及OverScowller使用及平滑处理

Scroll其实类似于ValueeAnimator，只是个工具类，就提的更新交给用户自己 

#### 不同层次的手势处理


####  getMeasuredHeight()与getHeight的区别

	The size of a view is expressed with a width and a height. A view actually possess two pairs of width and height values.
	
	The first pair is known as measured width and measured height. These dimensions define how big a view wants to be within its parent (see Layout for more details.) The measured dimensions can be obtained by calling getMeasuredWidth() and getMeasuredHeight().
	
	The second pair is simply known as width and height, or sometimes drawing width and drawing height. These dimensions define the actual size of the view on screen, at drawing time and after layout. These values may, but do not have to, be different from the measured width and height. The width and height can be obtained by calling getWidth() and getHeight().

实际上在当屏幕可以包裹内容的时候，他们的值相等，只有当view超出屏幕后，才能看出他们的区别：getMeasuredHeight()是实际View的大小，与屏幕无关，而getHeight的大小此时则是屏幕的大小。当超出屏幕后，getMeasuredHeight()等于getHeight()加上屏幕之外没有显示的大小


分析问题的关键，注意找关键函数 

### 比如Activity跟WMS建立关系的点，Activity Theme的创建点， 

      activity.attach(appContext, this, getInstrumentation(), r.token,
                        r.ident, app, r.intent, r.activityInfo, title, r.parent,
                        r.embeddedID, r.lastNonConfigurationInstances, config);

                if (customIntent != null) {
                    activity.mIntent = customIntent;
                }
                r.lastNonConfigurationInstances = null;
                activity.mStartedActivity = false;
                int theme = r.activityInfo.getThemeResource();
                if (theme != 0) {
                    activity.setTheme(theme);
                }

                activity.mCalled = false;
                mInstrumentation.callActivityOnCreate(activity, r.state);
                
###                 到底在那个点显示View

### setcontentview就算不调用，也会显示，因为还是会默认getDecorView，创建DocView，窗口的显示最终要从resume开始



		final void handleResumeActivity(I

           r.window = r.activity.getWindow();  
            //获取为窗口创建的视图DecorView对象  
            View decor = r.window.getDecorView();  
            decor.setVisibility(View.INVISIBLE);  
            //在attach函数中就为当前Activity创建了WindowManager对象  
            ViewManager wm = a.getWindowManager();  
            //得到该视图对象的布局参数  
            ②WindowManager.LayoutParams l = r.window.getAttributes();  
            //将视图对象保存到Activity的成员变量mDecor中  
            
            。。。
              //将创建的视图对象DecorView添加到Activity的窗口管理器中  
                ③wm.addView(decor, l);  
                
                
####       token

ActivityManagerService这一侧，都有一个对应的ActivityRecord对象，用来描述该Activity组件的运行状态。这个Binder代理对象会被保存在Window类的成员变量mAppToken中，这样当前正在处理的窗口就可以知道与它所关联的Activity组件是什么

### 到底在哪里绘制
 