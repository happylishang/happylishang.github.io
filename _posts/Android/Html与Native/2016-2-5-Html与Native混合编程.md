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

#### 参考文档 

[ Android JSBridge的原理与实现](http://blog.csdn.net/sbsujjbcy/article/details/50752595)

[JS与WebView交互存在的一些问题](http://www.jianshu.com/p/93cea79a2443)

[ Android WebView的Js对象注入漏洞解决方案](http://blog.csdn.net/leehong2005/article/details/11808557#%E3%80%91)

[webview javascript 注入方法](http://www.cnblogs.com/rayray/p/3680500.html)