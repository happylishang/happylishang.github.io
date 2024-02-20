Android启动页的背景主要指冷启动时windowBackground配置的背景图，这个阶段业务还未加载，配置的主要用在StartWindow上，与闪屏复用，一来是防止白屏、二来是品牌宣传。最简单的做法是为windowBackground直接配置一张图

        <item name="android:windowBackground">@drawable/bg_splash_logo</item>

但是这种方式在不同的屏幕上就会出现不同程度的拉伸，所以后面出现了可以用layer-list配合bitmap灵活控制，

	<layer-list xmlns:android="http://schemas.android.com/apk/res/android">
	    <item android:drawable="@color/white" />
	    <item>
	        <bitmap
	            android:gravity="center"
	            android:src="@mipmap/bg_splash" />
	    </item>
	</layer-list>

一般来说简约背景+slogon的场景都能满足，但还是有些需求无法满足，启动页背景一般可分两类，一类是slogon标语+纯色或渐变色背景，一类是全屏图的做法，全屏图一般都要求禁止拉伸，这种场景稍微麻烦一点点，windowBackground不支持等比例缩放，也就不存在centerCrop类的选项，需要额外处理下。

## slogon标语+纯色或者渐变色背景

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
		
		
##  全屏背景图+禁止拉伸

Android启动背景里的drawable或者bitmap仅支持

* 水平填充满 [不能垂直等比例拉伸]
* 垂直填充满 [不能水平等比例拉伸]
* 整体拉伸或者压缩填充满屏幕
* 不拉伸填充[top\bottom\center等]

因为不存在等比例拉伸，所以也无法利用centerCrop这样的缩放，拉伸均会导致变形，所以，需要采用不拉伸的策略，对齐方式看个人需求，center、或者top|center_horozion等不同场景都能满足需求，不过这里的关键是要提供一个足够大的图，来满足不同尺寸的设备。

以center为例，我们将背景图放在XXH文件夹下，假如我们的图是1080 * 1920 ， 转换成dp，就是360dp * 640dp。如果有些手机比较长，那可能就无法完全覆盖屏幕，因为我们不拉伸，另外有些手机能够手动调节显示大小，更改dp数量，比如pixel

![img_v3_0287_cdad22a4-2ef1-4037-8a6a-5b75e77b38cg.jpg](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e88cd0029c244cec91875569d68b4c77~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=1080&h=2280&s=117196&e=jpg&b=f2eff3)

正常情况下 默认densityDpi 3.0625 ，修改显示大小后，densityDpi 2.75 ，从而dp数增多，变成360 * (3/2.75)  x  640 *(3/2.75) ,那么XXH文件夹下的图就不够用了，必须准备1200 * 2140才够用，所以说，要让图足够宽，足够长，如下所示

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/221417c71e554550abda154f07b512fe~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=558&h=970&s=44107&e=png&b=bfffff)

如果有些手机是1080*2200这种非标准屏幕，那也要注意留的足够长。如果是以顶部居中为标准，那就变成如下示意，裁底部：

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7aea59f1995f4d1d9d56967e0082fff6~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=558&h=970&s=39455&e=png&b=bfffff)

首先能保证1080*1920的主流机型不会存在问题，其次冗余边缘区域为其他的尺寸留足了兼容空间，当然也没必要过大，太大的图会拖慢启动速度。

	<layer-list xmlns:android="http://schemas.android.com/apk/res/android">
	    <item>
	        <bitmap
	            android:gravity="top|center_horizontal"
	            android:src="@mipmap/bg_splash_top" />
	    </item>
	</layer-list>
	
如此就可以完成不拉伸全屏启动图适配。		