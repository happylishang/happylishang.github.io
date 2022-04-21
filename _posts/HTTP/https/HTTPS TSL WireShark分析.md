Https协议要做到什么

* 数据的保密性
* 校验双方身份的真实性
* 数据的完整性

中间人


## TSL1.2链接建立[DHE/ECDHE]

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/18c4245997d04e6aaf95703a6cfbe874~tplv-k3u1fbpfcp-watermark.image?)

参考文档：https://blog.csdn.net/mrpre/category_9270159.html

### Client Hello

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8ce07f0d85a34d47b5c524650b8536dc~tplv-k3u1fbpfcp-watermark.image?)


![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b1e6d167bbdc4ac18c6696ba1757144f~tplv-k3u1fbpfcp-watermark.image?)

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/405790c64435485ca2c28a2ba4667eeb~tplv-k3u1fbpfcp-watermark.image?)

启动TSL握手过程，**提供自己所能支持各种算法，同时提供一个将来所能用到的随机数**。参考文档【https://blog.csdn.net/mrpre/article/details/77867439】


### Server Hello

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e34eb38cee2b43f391e0324e5e0e9e68~tplv-k3u1fbpfcp-watermark.image?)

Handshake Type: Server Hello (2)，主要对Client Hello的响应 ，**确定使用的加密套件**，上图看出使用的是Cipher Suite: TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 (0xc02f)。

###   Certificate  服务端发送证书链 

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8204a7f74b7b4982aea40fc8c9bf1f19~tplv-k3u1fbpfcp-watermark.image?)


![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/a699f40c013c4e1dbced50085cdc9008~tplv-k3u1fbpfcp-watermark.image?)

将服务器配置的证书（链）发送到客户端。


CER格式的证书 ：CER用于**存储公钥证书**的文件格式，CER文件中的公共证书使用数字签名来映射具有特定身份的公共密钥，从而验证网站。可以使用Base64（PEM）和DER等不同编码算法来编码CER文件的内容。


![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/56fbc9e8317e40c9b8aac044cb932869~tplv-k3u1fbpfcp-watermark.image?)


### Server Key Exchange 

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b540854aefcd4c43b0bc21f8362fe617~tplv-k3u1fbpfcp-watermark.image?)

###  Server Hello Done

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/3c05475b5ed440539b89f74770e51493~tplv-k3u1fbpfcp-watermark.image?)


###  Client Key Exchange, Change Cipher Spec, Encrypted Handshake Message

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/9e04b43b0de9420db98f0dcbd41deec4~tplv-k3u1fbpfcp-watermark.image?)

### Change Cipher Spec, Encrypted Handshake Message

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/12ed22a64dcc4a4da9997d9ff08659d6~tplv-k3u1fbpfcp-watermark.image?)

###  	Application Data

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/825e9cdcb5934d53891ef56eeedf1805~tplv-k3u1fbpfcp-watermark.image?)

## TSL1.2链接建立[ ]


![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/92b55a44972540efa0cae0ab64767be7~tplv-k3u1fbpfcp-watermark.image?)