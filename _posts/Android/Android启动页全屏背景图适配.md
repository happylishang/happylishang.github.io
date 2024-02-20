启动页的背景这里主要指冷启动，业务还未加载配置在StartWindow上的背景，其实也与闪屏复用，一来是防止白屏、二来是品牌宣传。

启动页背景一般两类，一类是slogon标语+纯色或渐变色背景，一类是全屏图。

## slogon标语类启动背景

这类处理起来比较简单，只需要将slogon切出来，配合layer-list即可

		<layer-list xmlns:android="http://schemas.android.com/apk/res/android">
		    <item android:drawable="@color/white" />
		    <item android:top="200dp">
		        <bitmap
		            android:gravity="top"
		            android:src="@mipmap/slogon" />
		    </item>
		</layer-list>
		
如果是渐变色的背景、或者不怎么怕拉伸的纹理图片，也可以直接配置

		<layer-list xmlns:android="http://schemas.android.com/apk/res/android">
		    <item android:drawable="@drawable/bg_splash" />
		    <item android:top="200dp">
		        <bitmap
		            android:gravity="top"
		            android:src="@mipmap/slogon" />
		    </item>
		</layer-list>
		
		
##  全屏图片类


		
		