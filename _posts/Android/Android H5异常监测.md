## 认识WebViewClient

Android中如果不设置WebViewClient，则默认走外部加载（浏览器或者能够响应Http开头的配置），设置WebViewClient之后由APP自身处理，至于是加载还是做其他事情由shouldOverrideUrlLoading返回值决定。

> Give the host application a chance to take control when a URL is about to be loaded in the current WebView. If a WebViewClient is not provided, by default WebView will ask Activity Manager to choose the proper handler for the URL. If a WebViewClient is provided, returning true causes the current WebView to abort loading the URL, while returning false causes the WebView to continue loading the URL as usual.

> 
> Note: Do not call WebView#loadUrl(String) with the request's URL and then return true. This unnecessarily cancels the current load and starts a new load with the same URL. The correct way to continue loading a given URL is to simply return false, without calling WebView#loadUrl(String).

为了安全考虑，post请求不调用该方法

> This method is not called for POST requests.

可能有私有路由协议处理：

> This method may be called for subframes and with non-HTTP(S) schemes; calling WebView#loadUrl(String) with such a URL will fail.
> 
> 

第一帧可见时机：

                webView.postVisualStateCallback(1000, new WebView.VisualStateCallback() {
                    @Override
                    public void onComplete(long l) {
                        LogUtils.v("ac  "+ SystemClock.uptimeMillis());

                    }
                });