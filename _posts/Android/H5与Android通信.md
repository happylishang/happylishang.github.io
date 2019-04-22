# iframe + CustomWebViewClient

* 在JS代码动态添加一个iframe，将其src属性设置为JS需要传给Java的参数（例如bridge://uncle.nought.com?arg=xxx）。
* 在Java代码中，定义一个CustomWebViewClient extends WebViewClient，然后mWebView.setWebViewClient(new CustomWebViewClient())。
* 在Java代码中的CustomWebViewClient中，重写shouldOverrideUrlLoading(WebView view, String url)方法，自己处理url参数，并return true。
* 这时JS代码就可以把参数通过url传递给Java，Java拿到参数去执行相应的工作了。如果JS需要返回值，那么通过Java调用JS代码的形式把返回值返回给JS。

这是一种比较Trick的方式。js在执行的过程中去给整个dom添加一个iframe，并将这个iframe设置为display:none。然后通过这个iframe去load一个url，触发WebViewClient的shouldOverrideUrlLoading()，然后在这里面，我们可以决定如何处理JS传递过来的参数。由于这个url我们是自己来解析和处理的，不打算交给WebView去直接load，所以我们其实可以自己定义一个协议，例如bridge://uncle.nought.com?arg1=x&arg2=y。然后在WebView的WebViewClient里面拿到这个nought://开头的url后，我们自己写Java代码处理arg等参数。

这样的：

CustomWebViewClient的shouldOverrideUrlLoading返回true，表示由Java处理url，WebView不用管。
CustomWebViewClient的shouldOverrideUrlLoading返回false，表示Java不管这个url，由WebView自己处理url（一般还会再添加一行代码webView.loadUrl(url)）。
可能你还会觉得白白添加iframe进来不好吧，那么不加也是可以的。只要你能让当前WebView去加载一个url就可以了，所以这样window.location.href='bridge://uncle.nought.com?arg=xxx'都是可以的！并没有任何问题！

# addJavascriptInterface


# 参考文档


[Android WebView调用JS](http://unclechen.github.io/2015/11/26/Android%20WebView%E8%B0%83%E7%94%A8JS/)       

