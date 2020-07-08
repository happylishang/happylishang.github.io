### 背景

JsBridge是H5与Native进行通信的一种实现方式，目前线上jsBridge经过两轮重构后，大问题基本没了，一期兼容，二期重构，基本解决了白屏导致的问题。目前Android端唯一的问题就是：**JsBridge创建的时机 **。目前的实现是native将js代码注入H5，对于Android而言，这个注入时机比较靠后（提前的话，注入会失败），导致前端使用JsBridge比较滞后，如下

![image.png](https://upload-images.jianshu.io/upload_images/1460468-64018113df4223a0.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

为了让H5侧能够提前调用JsBridge，则必须将注入时机提前，其实完全可以放到H5 head里。自己加载就好了。

### 收益

JsBridge时机提前，方法在任何时机都可用，可以用来提升用户体验及其他技术优化

* 将标题栏或者一些其他预设的逻辑提前，优化用户体验
* 将H5从本地获取数据的时机提前

### 解决思路

客户端提供js文件，前端将这份js文件自己放在Head里进行加载，这样就能保证前端想用的时候，均可用。

	<html>
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
	<head>
	    <script src="jsbridgeandroid.js"></script>
	    ...
	</head>
	 ...

jsbridgeandroid.js更新频率降低，可以采用适当的缓存策略，避免H5每次都加载，更新后，时机如下

![image.png](https://upload-images.jianshu.io/upload_images/1460468-6564832970b120f8.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

### 改造及上线方案

> 对于前段而言：

旧版存量H5无需处理，新版H5 在head中会加在引入 jsbridgeandroid.js文件即可

> 对于客户端

* 旧版不许考虑
* 新版APP：需要 监听jsonRPC.notify("markNewJsBridge","") 回调，用来区分新老页面

        jsonRPC.notify("markNewJsBridge","")

新页面调用markNewJsBridge方法后，客户端不再注入js文件，如果没有收到该方法，说明还是老页面，依旧需要注入。

### 测试机上线

> 测试：

* 新旧APP及新旧H5的混合场景都要验证

> 上线

* APP 跟H5无需同步

### 人力

* 客户端：李尚  7d
* 前端：汪邵云 
* QA：

