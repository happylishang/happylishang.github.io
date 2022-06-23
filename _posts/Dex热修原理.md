Java对象的Method在native层会有对应的ArtMethod指针


AndFix的实现：native层利用FromReflectedMethod 方法拿到Java层Method对应native层的ArtMethod指针，然后执行替换的，替换方法实现


![uIXbfWcpRG.jpg](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b2e82f908e42408c9e8f143fa1584165~tplv-k3u1fbpfcp-watermark.image?)