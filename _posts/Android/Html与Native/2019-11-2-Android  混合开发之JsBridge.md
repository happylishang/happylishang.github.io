---
layout: post
title: "Android  混合开发之JsBridge"
description: "Android"
categories: [Android]

---

电商或者内容类APP中，H5通常都会占据一席之地,Native跟H5通信会必不可少，比如某些场景H5通知native去分享，native通知H5局部刷新等，Android本身也提供这样的接口，比如addJavascriptInterface、loadUrl("javascript:..."），而需要支持的能力也要是双工的。

* 1：H5通知Native(**可能需要处理回调**)，
* 2：Native通知H5（**也可能需要处理回调**）

实现这种机制的方式并不唯一，但使用不当经常会引入很多问题，比如：H5同Native需要一个中间js文件，实现简单的通信协议，这个js文件有的产品做法是让前端自己加载，有的做法是客户端注入，也就是通过loadUrl("javascript:..."）注入。采用客户端注入这种方式就多少有问题，因为没有一个很合适的时机既保证注入成功，又保证注入及时。如果在onPageStarted时注入，很多手机会注入失败，如果onPageFinished时注入，又太迟，导致很多功能打折扣。再比如：有些人通过prompt方式实现H5通知Native，而prompt是一个可能产生问题的同步方法，一旦无法返回，整个js环境就会挂掉，导致所有H5页面都无法打开，下面简单说下两种实现，一是通过addJavascriptInterface，另一种就
是通过prompt。



# 借助WebView.addJavascriptInterface实现H5与Native通信

WebView的addJavascriptInterface方法允许Natvive向Web页面注入Java对象，之后，在js中便可以直接访问该对象，使用@JavascriptInterface注解的方法。比如通过如下代码向前端注入一个名字为mJsMethodApi的java对象

	class JsMethodApi {
	     
	    /**
	     * js调用native，可能需要回调
	     */
	    @JavascriptInterface
	    public void callNative(String jsonString) {
	        ...
	    }
	}

    webView.addJavascriptInterface(new JsMethodApi(), "mJsMethodApi");
 
在前端的js代码中，是可以直接通过mJsMethodApi.callNative(jsonString)通知Native的，而且通过addJavascriptInterface注入的对象在H5的任何地方都可以调用，不存在注入时机跟注入失败的问题，在H5的head里调用都没问题。

	<head>
	    <script type="text/javascript"  >
	       JsMethodApi.callNative('头部就可以回调');
	    </script>
	</head>
	
经测试，其实是可以通知到Native的，不过有一点需要注意callNative是这JavaBridge这个线程中执行的，虽然不提清楚它跟JS线程的关系，但JS会阻塞等待callNative函数执行完毕再往下走，所以 @JavascriptInterface注解的方法里面最好也不要做耗时操作，最好利用Handler封装一下，让每个任务自己处理，耗时的话就开线程自己处理。

如果前端通知Native时需要回调怎么办？可以抽离到一个中间的js，为每个任务设置一个ID，暂存回调函数，等到Native处理结束后，先走这个中间的js，找到对应的js回调函数执行即可，

	
	 var _callbacks = {};
	 
	 function callNative(method, params, success_cb, error_cb) {
	
	     var request = {
	         version: jsRPCVer,
	         method: method,
	         params: params,
	         id: _current_id++
	     };
	  <!--暂存回调函数-->
	     if (typeof success_cb !== 'undefined') {
	         _callbacks[request.id] = {
	             success_cb: success_cb,
	             error_cb: error_cb
	         };
	     }
	     <!--利用JsMethodApi通知Native-->
	    JsMethodApi.callNative(JSON.stringify(request));
	 };

以上js代码完成回调的暂存、通知native执行，native那边会收到js消息，同时里面包含着id，等到native执行完毕后，将执行结果与消息id通知到这个中间层js，找到对应的回调函数执行即可，如下：
	
	
	 jsRPC.onJsCallFinished = function(message) {
	        var response = message;
	             <!--找到回调函数-->
	             var success_cb = _callbacks[response.id].success_cb;
	             <!--删除-->
	             delete _callbacks[response.id];
	             <!--执行回调函数-->
	             success_cb(response.result);
	 };

这样就完成H5通知Native，同时Native将结果回传给H5，并完成回调这样一条通路。Native通知H5,这条路怎么办？流程大概类似，同样可以基于一个消息ID完成回调，不过更加灵活，因为Native通知前端的接口不太好统一，具体使用自己把握。
	
[参考工程   https://github.com/happylishang/CMJsBridge ](https://github.com/happylishang/CMJsBridge)


> 注意不要混淆

如果混淆了，@JavascriptInterface注解的方法可能就没了，结果是，JS就没办法知己调用对应的方法，导致通信失败。

> 关于漏洞问题

4.2以后，WebView会禁止JS调用没有添加@JavascriptInterface方法, 解决了安全漏洞，而且很少APP兼容到4.2以前，安全问题可以忽略。

> 关于阻塞问题

JavascriptInterface注入的方法被js调用时，可以看做是一个同步调用，虽然两者位于不同线程，但是应该存在一个等待通知的机制来保证，所以Native中被回调的方法里尽量不要处理耗时操作，否则js会阻塞等待较长时间，如下图


![801573097289_.pic.jpg](https://upload-images.jianshu.io/upload_images/1460468-e5851bd3d2c9edcf.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)



# 通过prompt实现H5与Native的通信

日常使用Webview的时候一般都会设置WebChromeClient，用来处理一些进度、title之类的事件，除此之外，WebChromeClient还提供了几个js回调的入口，如onJsPrompt，onJsAlert等，在前端调用​window.alert​，​window.confirm​，​window.prompt​时，

	  public boolean onJsAlert(WebView view, String url, String message,
	            JsResult result) {
	        return false;
	    }
	 
	    public boolean onJsConfirm(WebView view, String url, String message,
	            JsResult result) {
	        return false;
	    }
	
	 
	    public boolean onJsPrompt(WebView view, String url, String message,
	            String defaultValue, JsPromptResult result) {
	        return false;
	    }
	    
    

在js调用​window.alert​，​window.confirm​，​window.prompt​时，​会调用WebChromeClient​对应方法，可以此为入口，作为消息传递通道，考虑到开发习惯，一般不会选择alert跟confirm，​通常会选promopt作为入口，在App中就是onJsPrompt作为jsbridge的调用入口。由于onJsPrompt是在UI线程执行，所以尽量不要做耗时操作，可以借助Handler灵活处理。对于回调的处理跟上面的addJavascriptInterface的方式一样即可，采用消息ID方式做暂存区分，区别就是这里采用 prompt(JSON.stringify(request));通知native，如下：

	 function callNative(method, params, success_cb, error_cb) {
	
	     var request = {
	         version: jsRPCVer,
	         method: method,
	         params: params,
	         id: _current_id++
	     };
	
	     if (typeof success_cb !== 'undefined') {
	         _callbacks[request.id] = {
	             success_cb: success_cb,
	             error_cb: error_cb
	         };
	     }
	    prompt(JSON.stringify(request));
	 };


同之前JavaBridge线程类似，这里prompt的js线程必须要等待UI线程中onJsPrompt返回才会唤醒，可以认为是个同步阻塞调用（应该是通过线程等待来做的）。

	public class JsWebChromeClient extends WebChromeClient {
	
	    JsBridgeApi mJsBridgeApi;
	
	    public JsWebChromeClient(JsBridgeApi jsBridgeApi) {
	        mJsBridgeApi = jsBridgeApi;
	    }
	
	    @Override
	    public boolean onJsPrompt(WebView view, String url, String message, String defaultValue, JsPromptResult result) {
		        try {
	            if (mJsBridgeApi.handleJsCall(message)) {
	            <!--如果睡眠10s js就会等待10s-->
					//    Thread.sleep(10000);
	                result.confirm("sdf");
	                return true;
	            }
	        } catch (Exception e) {
	            return true;
	        }
	        //   未处理走默认逻辑
	        return super.onJsPrompt(view, url, message, defaultValue, result);
	    }
	}

如果在onJsPrompt睡眠10s，js的prompt函数一定会阻塞等待10s才返回，这个设计就要求我们不能在onJsPrompt中做耗时操作，systrace中可以验证。

![image.png](https://upload-images.jianshu.io/upload_images/1460468-5020ae9e5d64582b.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

上图中，chrome_iothread看做js线程。

# prompt的一个坑导致js挂掉

从表现上来看，onJsPrompt必须执行完毕，prompt函数才会返回，否则js线程会一直阻塞在这里。实际使用中确实会发生这种情况，尤其是APP中有很多线程的场景下，怀疑是这么一种场景：

* 第一步：js线程在执行prompt时被挂起，
* 第二部 ：UI线程被调度，恰好销毁了Webview，调用了 （webview的detroy），detroy之后，导致 onJsPrompt不会被回调，prompt一直等着，js线程就一直阻塞，导致所有webview打不开，一旦出现可能需要杀进程才能解决。

如果不主动destroy webview，可以很大程度避免这个问题，具体Chrome的实现如何，还没分析过，这里只是根据现象推测如此。而WebView.addJavascriptInterface并不会有这个问题，无论是否主动destroy Webview，都不会上述问题，可能chrome对addJavascriptInterface这种方式做了额外处理，在自己销毁的时候，主动唤起JS线程，但是onJsPrompt所在的UI线程显然没处理这种场景。

[参考工程   https://github.com/happylishang/CMJsBridge ](https://github.com/happylishang/CMJsBridge)


简单跟一下原理：JsDialogHelper是onJsPrompt承接的入口：

	@SystemApi
	public class JsDialogHelper {
	
	    private static final String TAG = "JsDialogHelper";
	
	    // Dialog types
	    public static final int ALERT   = 1;
	    public static final int CONFIRM = 2;
	    public static final int PROMPT  = 3;
	    public static final int UNLOAD  = 4;
	 
	 
	    public boolean invokeCallback(WebChromeClient client, WebView webView) {
	        switch (mType) {
	            case ALERT:
	                return client.onJsAlert(webView, mUrl, mMessage, mResult);
	            case CONFIRM:
	                return client.onJsConfirm(webView, mUrl, mMessage, mResult);
	            case UNLOAD:
	                return client.onJsBeforeUnload(webView, mUrl, mMessage, mResult);
	            case PROMPT:
	                return client.onJsPrompt(webView, mUrl, mMessage, mDefaultValue, mResult);
	            default:
	                throw new IllegalArgumentException("Unexpected type: " + mType);
	        }
	    }



# 总结

* 最好通过前端注入，这样就可以避免注入失败与注入时机不好把握的问题
* **建议采用WebView.addJavascriptInterface实现**，可以避免prompt挂掉js环境的问题
* 通过@JavascriptInterface的方法中不要同步处理耗时操作，需要返回值的方法需要阻塞调用（尽量减少）
* 如果非要用prompt，尽量不要自己destroy webview，很容导致js环境挂了，所有webview打不开网页
* 如论哪种实现，都不要直接处理耗时操作，会阻塞js线程。
