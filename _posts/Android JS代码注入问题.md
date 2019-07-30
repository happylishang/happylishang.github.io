1、不同ROM时机不同

    @Override
    public void onPageStarted(WebView view, String url, Bitmap favicon) {
        super.onPageStarted(view, url, favicon);
        if (mJsApi != null) {
            mJsApi.loadReady(view.getContext());
            //先通知一次
            mJsApi.notifyJsReady();
        }
        
  注入不一定成功 ，也就是注入的js不一定可用 Nexus5  Pixel都可以，但是华为小米却不行难道做了什么
  
  
      @Override
    public void onPageFinished(WebView view, String url) {     
    
   
   一定是成功的， 
   
   
       @Override
    public void onPageCommitVisible(WebView view, String url) {
    
    
    
Notify the host application that WebView content left over from previous page navigations will no longer be drawn.

onPageCommitVisible被调用时，前一个页面的webview内容就不会再被绘制了（哪怕前一个没绘制完成）



同时也可以保证在此刻Webview是复用且可见的，确保不会再显示过时的内容，老的页面不会再被绘制， 

也可以说这个点老的View被清理干净了，新的View即将绘制。也就是说，此方法被调用，HTTP响应的主体开始加载时(本地资源不行吗？)，或者说开始加载DOM，并逐步可见。onPageCommitVisible发生在文档加载早期，此时css、图片等还都不可用

安全点
在回调可用于确定使回收的WebView

This callback can be used to determine the point at which it is safe to make a recycled WebView visible, ensuring that no stale content is shown.

 It is called at the earliest point at which it can be guaranteed that WebView#onDraw will no longer draw any content from previous navigations. 
 
 
 The next draw will display either the WebView#setBackgroundColor of the WebView, or some of the contents of the newly loaded page.

This method is called when the body of the HTTP response has started loading, is reflected in the DOM, and will be visible in subsequent draws. This callback occurs early in the document loading process, and as such you should expect that linked resources (for example, CSS and images) may not be available.

For more fine-grained notification of visual state updates, see WebView#postVisualStateCallback.

Please note that all the conditions and recommendations applicable to WebView#postVisualStateCallback also apply to this API.

This callback is only called for main frame navigations.


也是成功的。



此回调可用于确定使回收的WebView可见的安全点，。
它最早被调用，可以保证WebView＃onDraw将不再从以前的导航中提取任何内容。
下一个绘图将显示WebView的WebView＃setBackgroundColor或新加载页面的一些内容。
当HTTP响应的主体开始加载时，此方法被调用，反映在DOM中，并且将在后续绘制中可见。
此回调发生在文档加载过程的早期，因此您应该期望链接的资源（例如，CSS和图像）可能不可用。