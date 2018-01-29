
# 背景

视频比图片可以让用户更能直观地感受到商品的细节、材质，有利于用户购买决策；视频的生动性也有利于吸引用户下单。


# 现有问题

Android端已经基于VideoView支持视频播放，但是，原来的方案无法满足商品详情的需求，尤其在小视频跟全屏切换的时候，原方案不能做到“不间断”播放，最直接的问题就是黑屏，无法继续播放。因此，详情视频需求**不能采用**原来的全屏播放方案及控件。

# 方案选择

为了保证视频的无断点播放，需要自己控制MediaPlayer跟渲染的控件，目前采用MediaPlayer+TextureView，虽然SurfaceView也可以，但，SurfaceView默认背景黑色，并且本身相当于独占一个Surface，不太好控制，做的demo也验证了这个问题。看了几个视频播放的APP，也未采用SurfaceView，猜测应该是基TextureView：

>云音乐全屏视频 （仅看到一个Surface）

![云音乐全屏Surface图](http://upload-images.jianshu.io/upload_images/1460468-b0411ba3b0a52967.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

>今日头条全屏视频 （仅看到一个Surface）

![头条全屏播放Surface图](http://upload-images.jianshu.io/upload_images/1460468-d03dea487ca99b27.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

因此，最后选择MediaPlayer+TextureView。

## 解决无间断播放问题

原理：**维持同一个MediaPlayer数据流，并动态将TextureView从原来的容器中摘下来，添加到目标容器中**，这一点是原来的VideoView无法做到的，VideoView内部对于Surface的attach及detach做了处理，添加删除的时候会会重新绑定，导致视频播放不连续。

实现内容：自定义播放器及控制逻辑（开发时间一周）
遗留问题：原生MediaPlayer不太好用，对Error监听不及时

# 缓存问题

Android系统中，原生MediaPlayer的缓存大小无法修改（一般很小），并且，缓存文件也无法得到，虽然在Android4.0之后，缓存被调节到了一个稍大值，但是如果视频文件较大，MediaPlayer仍然无法缓存全部文件，这就会导致，哪怕在同一个场景循环播放，仍然会多次耗费流量。

## 将来的缓存方案：

考虑Socket层插个服务代理，进行一次嫁接，转发请求与返回，同时将数据缓存下来，每次访问的时候，如果有缓存，直接访问本地视频文件，没有的话，就委托服务器读取，简单示意图：

![视频缓存.jpg](http://upload-images.jianshu.io/upload_images/1460468-007cd6a5d5770777.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


待完善....
