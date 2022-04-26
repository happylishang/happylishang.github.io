HTTPS目前是网站标配，否则浏览器会提示链接不安全，同HTTP相比比，HTTPS提供安全通信，具体原因是多了个“S”层，或者说SSL层[Secure Sockets Layer]，现在一般都是TLS[Transport Layer Security]，它是HTTP**明文**通信变成安全**加密通信**的基础，核心就在加密上：

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/2e2b937a6b134b7486b3352d100ef294~tplv-k3u1fbpfcp-watermark.image?)

如上图所示，HTTP明文通信经中间路由最终发送给对方，如果中间某个路由节点抓取了数据，就可以直接看到通信内容，甚至可以篡改后，路由给目标对象，如下：

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/615ffafd9b234d0abe622bc9f3d1ffb0~tplv-k3u1fbpfcp-watermark.image?)

因此，HTTP传输是不安全的，但是，如果传输的是双方可校验的密文，就可以避免被偷窃、篡改，保证传输的安全性，这就是SSL/TLS层做的事情。

## HTTPS从哪些方面保证传输的安全性 ？

* 数据的保密性
* 校验双方身份的真实性
* 数据的完整性

* ①保密：在握手协议中定义了会话密钥后，所有的消息都被加密;
* ②鉴别：可选的客户端认证，和强制的服务器端认证;
* ③完整性：传送的消息包括消息完整性检查(使用MAC)。


## 直白的说法，用非对称加密 协商好通信的对称秘钥，然后用对称秘钥进行通信


![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f7019822b33148ffb60429ffbbfbc303~tplv-k3u1fbpfcp-watermark.image?)

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/af7dc23663924342bfb98525a1e9d171~tplv-k3u1fbpfcp-watermark.image?)

Https协议要做到什么 ：比如防止中间路由器网管篡改信息

中间人


我们需要一个办法来保证服务器传输的公钥确实是服务器的，而不是第三方的。这个时候，我们需要使用 数字证书。数字证书由权威机构 (CA, Certificate Authority) 颁发，里面包含有服务器的公钥，证书文件使用 CA 私钥进行加密。当客户端与服务器建立加密通信的时候，服务器不再返回公钥，而是返回他的数字证书。客户端拿到证书，使用对应的 CA 的公钥解密，然后获取到服务器的公钥。这里有一个问题，客户端怎么拿到 CA 的公钥呢？如果还是去CA 服务器获取的话，那么我们又会回到问题的原点即怎样保证 CA 公钥不被人篡改。因此，大部分浏览器中，权威 CA 的公钥都是内置的，不需要去获取。这就保证了 CA 公钥的正确性。第三方没有办法伪造证书，因为第三方没有 CA 的私钥（当然，CA 被入侵的例子的也是有的，技术永远解决不了人的问题）。


### HTTPS 中间人攻击

HTTPS 可以防止用户在不知情的情况下通信链路被监听，对于主动授信的抓包操作是不提供防护的，因为这个场景用户是已经对风险知情。要防止被抓包，需要采用应用级的安全防护，例如采用私有的对称加密，同时做好移动端的防反编译加固，防止本地算法被破解。


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

RSA加密算法是一种非对称加密算法   AES

###   Certificate  服务端发送证书链 

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8204a7f74b7b4982aea40fc8c9bf1f19~tplv-k3u1fbpfcp-watermark.image?)


![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/a699f40c013c4e1dbced50085cdc9008~tplv-k3u1fbpfcp-watermark.image?)

将服务器配置的证书（链）发送到客户端。证书是一个文件，里面含有目标网站的各种信息。例如网站的域名，证书的有效时间，签发机构等，其中最重要的是这两个：

* 用于生成对称秘钥的公钥
* 由上级证书签发的签名

CER格式的证书 ：CER用于**存储公钥证书**的文件格式，CER文件中的公共证书使用数字签名来映射具有特定身份的公共密钥，从而验证网站。可以使用Base64（PEM）和DER等不同编码算法来编码CER文件的内容。


![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/56fbc9e8317e40c9b8aac044cb932869~tplv-k3u1fbpfcp-watermark.image?)

#### 解决办法是采用「信任链」。保证证书的有效性

首先，有一批证书颁发机构（Certificate Authority，简称为 CA），由他们生成秘钥对，其中私钥保存好，公钥以证书的格式安装在我们的操作系统中，这就是 根证书。

我们的手机、电脑、电视机的操作系统中都预装了 CA 的根证书，他们是所有信任构建的基石。当然，我们也可以自己下载任意的根证书进行安装。

接下来，只要设计一个体系，能够证明 A 证书签发了 B 证书即可。这样对于收到的任何一个证书，顺藤摸瓜，只要最上面的根证书在系统中存在，即可证明该证书有效。

比如说，我们收到了服务器发过来的 C 证书，我们验证了 C 是由 B 签发的，然后又验证了 B 是由 A 签发的，而 A 在我们的系统中存在，那也就证明了 C 这个证书的有效性。

这其中，A 是根证书，B 是中间证书，C 是叶证书（类似树中的叶节点）。中间证书可以有很多个，信任的链条可以任意长，只要最终能到根证书即可。

得益于 RSA 的非对称性质，验证 A 是否签发了 B 证书很简单：

计算 B 的 hash 值（算法随便，比如 SHA1）
使用 A 的 私钥 对该 hash 进行加密，加密以后的内容叫做「签名（Signature）」
将该「签名」附在 B 证书中
A 使用自己的私钥给 B 生成签名的过程也就是「签发证书」，其中 A 叫做 Issuer，B 叫做 Subject。

这样，当我们收到 B 证书时，首先使用 A 证书的公钥（公钥存储在证书中）解开签名获得 hash，然后计算 B 的 hash，如果两个 hash 匹配，说明 B 确实是由 A 签发的。

重复上面的过程，直到根证书，就可以验证某个证书的有效性。

### Server Key Exchange 

> In Diffie-Hellman, the client can't compute a premaster secret on its own; both sides contribute to computing it, so the client needs to get a Diffie-Hellman public key from the server. In ephemeral Diffie-Hellman, that public key isn't in the certificate (that's what ephemeral Diffie-Hellman means). So the server has to send the client its ephemeral DH public key in a separate message so that the client can compute the premaster secret (remember, both parties need to know the premaster secret, because that's how they derive the master secret). That message is the ServerKeyExchange.

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


## 前向保密

前向保密（英語：Forward Secrecy，FS）有时也被称为完全前向保密（英語：Perfect Forward Secrecy，PFS），是密码学中通讯协议的一种安全特性，指的是长期使用的主密钥泄漏不会导致过去的会话密钥泄漏。 前向保密能够保护过去进行的通讯不受密码或密钥在未来暴露的威胁。



# RSA 算法与EDHCE的区别


RSA简单但是废弃，但是对于理解HTTPS很有帮助，EDHCE协商更优秀，现在都是用这种，得益于算DH算法的优点。


# Charles抓包原理

握手阶段，Charles会将服务端的公钥进行篡改，生成自己的公钥，本地新人证书后，证书链便可信任，进而抓包通信。证书的可靠性是靠证书链来保证，只要证书是合法的证书机构颁发的，那么网站就是安全的