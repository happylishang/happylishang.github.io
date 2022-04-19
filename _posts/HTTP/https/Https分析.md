Https协议要做到什么

* 数据的保密性
* 校验双方身份的真实性
* 数据的完整性

中间人


## TSL1.2链接建立[DHE/ECDHE]

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/18c4245997d04e6aaf95703a6cfbe874~tplv-k3u1fbpfcp-watermark.image?)



### Client Hello

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8ce07f0d85a34d47b5c524650b8536dc~tplv-k3u1fbpfcp-watermark.image?)


![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b1e6d167bbdc4ac18c6696ba1757144f~tplv-k3u1fbpfcp-watermark.image?)

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/405790c64435485ca2c28a2ba4667eeb~tplv-k3u1fbpfcp-watermark.image?)



### Server Hello

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e34eb38cee2b43f391e0324e5e0e9e68~tplv-k3u1fbpfcp-watermark.image?)

###   Certificate  服务端发送证书链 

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8204a7f74b7b4982aea40fc8c9bf1f19~tplv-k3u1fbpfcp-watermark.image?)


![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/a699f40c013c4e1dbced50085cdc9008~tplv-k3u1fbpfcp-watermark.image?)

CER格式的证书

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