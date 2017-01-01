---
layout: post
title: "ToolBar与Actionbar的使用"
description: "Java"
category: android

---

#### 两种实现方式配合使用也许更好

给你不用的自定义布局。但是全局的话还是用ActionBar 不过要配合Theme来用。



#### 去除toolbar中左边距问题 

[去除toolbar中左边距问题 ](http://blog.csdn.net/Android_caishengyan/article/details/50715805)

* 局部方案 当然我们要使用自定义布局才行
	
	    <android.support.v7.widget.Toolbar
	        android:id="@+id/toolbar"
	        android:layout_width="match_parent"
	        android:layout_height="?attr/actionBarSize"
	        android:background="?attr/colorPrimary"
	        app:contentInsetEnd="0dp"
	        app:contentInsetStart="0dp" />
 
 Java代码中
	 
	 //使用ToolBar的情况  
	        toolbar.setContentInsetsAbsolute(0, 0);  
          
#### 统一的返回Toolbar  多了一层布局

[薄荷Toolbar(ActionBar)的适配方案](http://stormzhang.com/android/2015/08/16/boohee-toolbar/)
  
#### ActionBar的处理方式  全局 

*  去除边距的方式


	
    <style name="AppTheme" parent="Theme.AppCompat">
	        <item name="actionBarStyle"> @style/MyActionBarStyle</item>
	    </style>
	
	    <style name="MyActionBarStyle" parent="@style/Widget.AppCompat.Light.ActionBar.Solid">	    
 	    <!--解决左边一直有一块边距的问题 contentInsetStart = 0 -->
	    <item name="contentInsetStart">0dp</item>
	    <item name="contentInsetEnd">0dp</item>
	    </style>
    
#### BaseActivity

	   protected void onCreate(Bundle savedInstanceState) {
	       super.onCreate(savedInstanceState);
	
	       android.support.v7.app.ActionBar _actionBar =getSupportActionBar();
	       if(_actionBar !=null){
	          View bar= LayoutInflater.from(this).inflate(R.layout.lv_tool_bar,null);
	
	
	           _actionBar.setDisplayShowCustomEnabled(true);
	           _actionBar.setCustomView(bar,new ActionBar.LayoutParams(ActionBar.LayoutParams.MATCH_PARENT, ActionBar.LayoutParams.MATCH_PARENT));
 

        }


#### Hide ActionBar title just for one activity
 
You probably want to create a sub-theme which hides the action bar:

	<style name="AppTheme.NoActionBar">
	    <item name="android:windowActionBar">false</item>
	</style>
	
and then apply it to your activity in the manifest like so:

	<activity android:name="NonActionBarActivity"
	          android:theme="@style/AppTheme.NoActionBar" />
	          
Using the dot notation (AppTheme.NoActionBar) makes this NoActionBar theme inherit everything from AppTheme and override the setting on android:windowActionBar. If you prefer you can explicitly note the parent:

	<style name="AppThemeWithoutActionBar" parent="AppTheme">
	    <item name="android:windowActionBar">false</item>
	</style>
	
Also, if you're using the AppCompat libraries, you probably want to add a namespace-less version to the sub-theme.


                      
####  参考文档

[Setting Up the App Bar](http://developer.android.com/intl/zh-cn/training/appbar/setting-up.html#add-toolbar)