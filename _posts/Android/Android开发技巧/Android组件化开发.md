> 组件化：更像一种**开发规范**，一种高内聚低耦合原则的落地实践

## 组件化开发到底在搞什么

组件化最终落地是一个个独立的业务及功能组件，

组件化最终应该是一种开发集成模式，他为开发带来了便利跟安全，但是，并不一定说，非常简洁高效。


## 要将项目模块化拆分，需要解决以下几个问题：

* 模块间页面跳转；
* 模块间事件通信；如何处理回调 ？
* 模块间服务调用；如何处理回调 ？ 对外暴露的服务，定义抽象实现放在公共暴露场景，不通过字符串获取 Arouter的不是特别好用
* 模块的独立运行；
* 模块间页面跳转路由拦截 



## 组件间通信

这了提到的**组件**暂**狭义上限定业务类组件**， 而组件间通信简单归为如下3种：

* 第一种，A组件路由到B组件，无需任何返回值【一般指Activity路由】
* 第二种，A组件路由到B，B需要提供返回值或者说回调【可以指Activity路由后的返回值，或者纯服务回调】
* 第三种，A组件路由到B，B组件需要将结果以广播的形式通知其他组件【Activity跳转 + Eventbus】


示意图如下：

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/80679b7e687545ec9a5da1134423ffe4~tplv-k3u1fbpfcp-watermark.image)

以上三种能力中，前两个要依赖Router框架的基础能力来实现，第三个需要借助Android的EventBus广播来实现。重点要强调的是2，有返回值【回调】，因为它可能牵扯到一个协议的问题。一个组件要暴露结果的方式一般是固定的， 抛出的结果如何被外部解析，这个就是一个协议规范的问题。在Android中，界面B的结果想要直传给上一个界面A，一般是通过onActivityResult来实现，但是当前router框架对此并不友好，后期需要扩展，不过扩展只是优化使用，基本能力已经具备。

调用界面A如果想要获取target B界面的返回值，只要在A Activity中实现 onActivityResult既可以，为了方便调用onActivityResult可以通过不可见Fragment封装成回调的方式来处理。

    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
    
    }

* requestCode 调用放A用来标识自己发起
* resultCode  B用来回传是否是成功or失败
* Intent data **暴露处理结果的核心，需重点关注其暴露方式**

B返回的Intent data 如何解析的问题牵扯一个规范，即：一个业务组件如何暴露自己的返回值,示意图如下


![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/61988057cd12429491bfdb0f5e7a4e8a~tplv-k3u1fbpfcp-watermark.image)


当一个UI组件被完整输出的时候，它应该输出以上三方面的东西，其中第一个是必选项，后两个可以选，即：被调用组件可以没有返回数据及广播。

 
## 路由自动注册

 
### Application 上下文问题

每个模块可能需要Application进行初始化操作，Application如何传递进去，这是第一个问题，Google提供jetpack框架中有一个很不错的框架startUp

### 抽象层剥离


### EventBus如何处理

全局Event，局部Event，如何处理Event库，

EventBus如何避免入侵业务，多个模块合作的时候，避免改动其他模块？

>  原则：

尽量避免发送EventBus，多以Eventbus的库可以单独抽离，每次增加Event，可以审核严格些，能不增加Event就不增加。


EventBus只给上层业务用，牵扯到改动，相关业务组件必须更新？or 每个组件进行初步过滤？

### 数据MODEL如何处理复用


业务自己自己处理自己的VO，只要这样，才能达到每个业务自己维护自己？不与别的牵连


### 必须有清晰的边界意识，哪些代码放在哪些地方

*  专项给谁用的，就放在谁那里
* 工具内部，不要糅杂外部业务逻辑
* 一定注意隔离，内聚的意义

### 基类里面的业务逻辑怎么处理

## 分批

* deviceid 各种util
* Feedback +crash
* AppContext + ServerEnv


### 自动注册Moduel

### EventBus 的Event下沉
