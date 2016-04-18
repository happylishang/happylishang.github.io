---
layout: post
title: "Html与Native混合编程"
description: "Android"
categories: [android,html5]

---

#### webview调用网页内部js函数只需要指定js函数名即可

* 不带参数

		 mWebView.loadUrl("javascript: showFromHtml()");
	 
* 	 带参数

	 
		mWebView.loadUrl("javascript：test('" + aa+ "')"); //aa是js的函数test()的参数
		
**
注意：如果是字符串一定要用单引号，或者双引号再次包裹一层，否则js会当做变量，导致出错。
**	 

#### html网页内部调用Java的实现比较灵活

* 比较落后的 有风险

		mWebView.addJavascriptInterface(getHtmlObject(), "jsObj");
		
不过在Android 4.2以上系统，通过在Java的远程方法上面声明@JavascriptInterface可以解决WebView漏洞。如下面代码：

	    class JsObject {  
	           @JavascriptInterface  
	           public String toString() { return "injectedObject"; }  
	        }
对于Android 4.2以下的系统Google公司官方没有提供解决方案。为替代addJavascriptInterface方法，可以利用prompt方法传参以完成java与js的交互。对应java中的onJsPrompt方法的声明如下：

		public boolean onJsPrompt( WebView view, String url, String message, String defaultValue, JsPromptResult result )
		
通过这个方法，JS能把信息（文本）传递到Java，而Java也能把信息（文本）传递到JS中，具体实施方法如下：

* 1）让JS调用一个Javascript方法，在这个方法中调用prompt方法，通过prompt把JS中的信息传递过来，这些信息应该是我们组合成的一段有意义的文本，可能包含：特定标识，参数等。在onJsPrompt方法中，我们去解析传递过来的文本，得到约定好的特定标识，参数等，再通过特定标识调用指定的java方法，并传入参数。具体的Java代码如下：

		final class MyWebChromeClient extends WebChromeClient 
		{   
		    public boolean onJsPrompt( WebView view, String url, String message, String defaultValue, JsPromptResult result )
		    {
		        if( message.equals("1") )
		          {
		          //解析参数defaultValue
		        //调用java方法并得到结果
		           }
		           //返回结果
		        result.confirm("result");
		        return true;
		    }
		}
 
* 2）关于返回值，可以通过result返回回去，这样就可以把Java中方法的处理结果返回到Js中。

* 3）在Javascript方法中，通过调用prompt方法传入标识和参数（依次对应onJsPrompt方法中的message、defaultValue参数），以通知java需要使用的方法及对应参数。prompt方法中第一个参数可以传送约定好的特定方法标识，prompt方法中第二个参数可以传入对应的参数序列。具体的Javascript代码如下：
		
		   function showHtmlcallJava()
		   {
		      var ret = prompt( "1", "param1;param2" );
		     //ret值即为java传回的”result”
		     //根据返回内容作相应处理
		    }
	    
    
*  jsbridge			 

#### jsbridge原理与使用



#### webview javascript 注入js文件，再调用其方法

	URL url = new URL("http://www.rayray.ray/ray.js");
	in = url.openStream();
	byte buff[] = new byte[1024];
	ByteArrayOutputStream fromFile = new ByteArrayOutputStream();
	FileOutputStream out = null;
	do {
	       int numread = in.read(buff);
	       if (numread <= 0) {
	         break;
	         }
	        fromFile.write(buff, 0, numread);
	     } while (true);
	String wholeJS = fromFile.toString();

	@Override
	public void onPageFinished(WebView view, String url) 
	 {
	        super.onPageFinished(view, url);
	         webview.loadUrl("javascript:" + wholeJS);
	 }
 
#### webview设置UserAgent setUserAgentString

	   String oldUA = webView.getSettings().getUserAgentString();
	        String newUA = oldUA + " somtString/" + SystemUtil.getAppVersion();
	        webView.getSettings().setUserAgentString(newUA);

 


###  Android Webview Cookie

在Android应用程序中经常会加载一个WebView页，如果需要客户端向WebView传递信息，比如Cookie，也是可以的。需要应用程序先将Cookie注入进去，打开该网页时，WebView会将加载的url通过http请求传输到服务器。同时，在这次请求中，会将Cookie信息通过http header传递过去。

客户端通过以下代码设置cookie

	public static void synCookies(Context context, String url) {  
	        CookieSyncManager.createInstance(context);  
	        CookieManager cookieManager = CookieManager.getInstance();  
	        cookieManager.setCookie(url, "uid=1243432");              
	        CookieSyncManager.getInstance().sync();  
	    }
	    
CookieManager会将这个Cookie存入该应用程序/data/data/databases/目录下的webviewCookiesChromium.db数据库的cookies表中

打开网页，WebView从数据库中读取该cookie值，放到http请求的头部，传递到服务器

客户端可以在注销登录时清除该应用程序用到的所有cookies
   
	private void removeCookie(Context context) {
	        CookieSyncManager.createInstance(context);  
	        CookieManager cookieManager = CookieManager.getInstance(); 
	        cookieManager.removeAllCookie();
	        CookieSyncManager.getInstance().sync();  
	    }

**注:这里一定要注意一点，在调用设置Cookie之后不能再设置**

		webView.getSettings().setBuiltInZoomControls(true);  
		webView.getSettings().setJavaScriptEnabled(true);  

这类属性，否则设置Cookie无效。

**注: cookieManager.setCookie每次设置一项，设置多个可能无效**

**注: cookieManager.setCookie每次设置一项，设置多个可能无效**

**注: cookieManager.setCookie不能采用预先拼接，一次性设置进去**


#### 参考文档 

[ Android JsBridge的原理与实现](http://blog.csdn.net/sbsujjbcy/article/details/50752595)
 
#### 手机端特有的web属性

**参考文档：http://blog.csdn.net/w2865673691/article/details/44941495**

    不能缩放
	<meta content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=0" name="viewport">     
	
	可以缩放
	<meta content="width=device-width, initial-scale=1.0, user-scalable=yes" name="viewport">  
	<meta content="yes" name="apple-mobile-web-app-capable">     
	<meta content="black" name="apple-mobile-web-app-status-bar-style">     
	<meta content="telephone=no" name="format-detection">

第一个meta标签表示：强制让文档的宽度与设备的宽度保持1:1，并且文档最大的宽度比例是1.0，且不允许用户点击屏幕放大浏览；

	width - viewport的宽度 height - viewport的高度   
	initial-scale - 初始的缩放比例  
	minimum-scale - 允许用户缩放到的最小比例   
	maximum-scale - 允许用户缩放到的最大比例  
	user-scalable - 用户是否可以手动缩放
	
第二个meta标签是iphone设备中的safari私有meta标签，它表示：允许全屏模式浏览；
第三个meta标签也是iphone的私有标签，它指定的iphone中safari顶端的状态条的样式；

#### webview缩放问题

最重要的是让web端控制缩放，而不是自己段处理。
缩放后，要使内容适配屏幕，不超出屏幕外显示，实现换行。这方面效果应该由html控制，而不是webview控制

         webView.setWebViewClient(yxWebViewClient);

        // 设置可以支持缩放
        webView.getSettings().setSupportZoom(true);
        // 设置出现缩放工具
        webView.getSettings().setBuiltInZoomControls(true);
        //设置可在大视野范围内上下左右拖动，并且可以任意比例缩放
        webView.getSettings().setUseWideViewPort(true);
        //设置默认加载的可视范围是大视野范围
        webView.getSettings().setLoadWithOverviewMode(true);
        //自适应屏幕
        webView.getSettings().setLayoutAlgorithm(WebSettings.LayoutAlgorithm.SINGLE_COLUMN);
        
必须

        // 设置可以支持缩放
        webView.getSettings().setSupportZoom(true);
        // 设置出现缩放工具
        webView.getSettings().setBuiltInZoomControls(true);

如果不想显示控制的缩放按钮

        webView.getSettings().setDisplayZoomControls(false);
                
        
#### 参考文档 

 

[ Android JSBridge的原理与实现](http://blog.csdn.net/sbsujjbcy/article/details/50752595)
 
[ Android JsBridge的原理与实现](http://blog.csdn.net/sbsujjbcy/article/details/50752595)
 

[JS与WebView交互存在的一些问题](http://www.jianshu.com/p/93cea79a2443)

[ Android WebView的Js对象注入漏洞解决方案](http://blog.csdn.net/leehong2005/article/details/11808557#%E3%80%91)

[webview javascript 注入方法](http://www.cnblogs.com/rayray/p/3680500.html)