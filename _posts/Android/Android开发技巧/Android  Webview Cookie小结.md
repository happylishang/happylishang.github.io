* Cookie是啥怎么用
* Set-Cookie是啥怎么用
* Webview怎么用Cookie
* Android Cookie存储与同步


### Cookie是啥怎么用

Cookie的维基百科定义：

> 某些网站为了辨别用户身份而储存在用户本地终端上的数据，也即是主要用来标识。由于HTTP协议是无状态的，服务器不知道用户上一次做了什么，以购物场景为例，用户浏览了几个页面，买了一盒饼干和两瓶饮料，最后结帐时，由于HTTP的无状态性，不通过额外的手段，服务器并不知道用户到底买了什么，Cookie就是用来绕开HTTP的无状态性的“额外手段”之一，服务器可以设置或读取Cookies中包含信息，借此维护用户跟服务器会话中的状态。


 
文主要聚焦在Android的Webview里，也就是主要是Http，当然，非Http协议也可以参考这个思路。

### Android Webview的Cookie怎么用

* 静态注入


* 动态更新Set-Cookie

Android中Cookie存储位置data/data/包名/app_WebView/Default/Cookies

![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b341290711b4413982607dbeac23e1d3~tplv-k3u1fbpfcp-watermark.image)

文件类型：一个数据库，存储格式如下：

![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/dfbdca13bb7e4849ba495786f50e9ebd~tplv-k3u1fbpfcp-watermark.image)

有几个字段比较敏感：expires_utc、has_expires、is_persistent

is_persistent 和 has_expires表明一个cookie是否有过期时间，过期时，cookie就会失效，无法使用这个cookie。但是，如果expires_utc、is_persistent、has_expires同时为0，可以看做永久有效。


### Android Cookie存储与同步

5.1之前采用CookieSyncManager实现Cookie的同步，Cookie被设置后即刻生效，但是并没有被持久化到本地文件，CookieSyncManager就是用来做这个事的。

> The CookieSyncManager is used to synchronize the browser cookie store between RAM and permanent storage. To get the best performance, browser cookies are saved in RAM. A separate thread saves the cookies between, driven by a timer.

不过CookieSyncManager在API 21 也就是5.0 Deprecated，后面都不需要了，Android 5.0之后

>  The WebView now automatically syncs cookies as necessary. You no longer need to create or use the CookieSyncManager.
 To manually force a sync you can use the CookieManager method {@link CookieManager#flush} which is a synchronous
            
            
![](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/a00b651982d847258a385a6fa132c1db~tplv-k3u1fbpfcp-watermark.image)           

如果想要立刻写到文件，可以通过flush方法实现：


>    Ensures all cookies currently accessible through the getCookie API are
>    written to persistent storage.
>    This call will block the caller until it is done and may perform I/O.
   
正如方法说明一样，flush()会触发IO操作，所以会阻塞当前线程。如果不担心断电可能导致同步丢失，不建议手动flush，交给系统即可。

    

### 不同进程的的webview cookie不公用

Android 8.0之后，

![](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ad504f4f9d3c481084e9c8ceb66f133e~tplv-k3u1fbpfcp-watermark.image)

### HttpOnly：Js无法访问也无法操作

防止js操作或者直接使用Cookie，该属性不能由Js设置，只能后端服务设置

![](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/5e7bf23220854fffaa6560e8e0b4f7fb~tplv-k3u1fbpfcp-watermark.image) 

该属性可以被修改覆盖

### Perminent

如果后端不设置，就是sessioncookie，不会持久到本地

![](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b83bce55a3e545f18adcccfdca5a278a~tplv-k3u1fbpfcp-watermark.image)

但是如果客户端自己种cookie，不设置，那就是永久有效
