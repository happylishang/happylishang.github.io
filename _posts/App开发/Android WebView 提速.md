Android H5加速

# WebView三种提速方式

1.  离线资源预下载
2.  Webview资源Cache
3.  Prefetch请求并行加载    

三者的理念基本一致：**尽量提前准备好资源，优先使用本地资源**，不过三者还是有区别的，资源预下载跟Cache侧重资源，而Prefetch并行加载侧重于业务请求。另外**离线资源跟本地资源Cache**都牵扯到请求拦截，而Prefetch不需要处理拦截，Android 的拦截入口在WebViewClient的shouldInterceptRequest方法，默认情况下不拦截：

    public WebResourceResponse shouldInterceptRequest(WebView view,
            String url) {
        return null;
    }

如果需要拦截的话，重写shouldInterceptRequest ，通过本地缓存提供WebResourceResponse入口
 
    @Override
    public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
       WebResourceResponse response = null;
       WebResourceIntercept intercept = onGetResourceIntercept();
        return response;
    }


## 预加载+缓存流程与Prefetch流程对比


![](https://user-gold-cdn.xitu.io/2020/7/28/1739544e034a1e14?w=1325&h=705&f=png&s=109297)

## 预下载与Cache缓存

离线资源预下载已经有类似功能，修改点是**下载更新及命中的逻辑**，有两点需要确认1、URL 匹配的效率 2、读本地缓存的效率

* 离线资源预下载Android没什么特别问题，只会拦截GET请求，不存在post请求问题，需要注意UI线程中匹配效率 （选择内存缓存、读文件文件缓、还是数据库）
* webview的cache建议**前端及后端统一**，走标准Cache规范，Webview默认处理，避免二次拦截，反而降低效率

## Prefetch

Prefetch跟上面两个有很大差异，上两个侧重资源，而Prefetch侧重业务请求，在即将加载网页的时候，并发请求业务数据，前端需要业务数据渲染的时候，先看之前并发请求是否回来，如回来则用预取的，提高速度，如果没有，继续走默认前端请求。


