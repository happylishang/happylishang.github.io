---

layout: post
title: HTTPS/TSL协议与WireShark分析
category: Android

---


HTTPS目前是网站标配，否则浏览器会提示链接不安全，同HTTP相比比，HTTPS提供安全通信，具体原因是多了个“S”层，或者说SSL层[Secure Sockets Layer]，现在一般都是TLS[Transport Layer Security]，它是HTTP**明文**通信变成安全**加密通信**的基础，SSL/TLS介于应用层和TCP层之间，从应用层数据进行加密再传输。安全的核心就在加密上：


![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/af195d42e4c046b3abeffe316b1a069c~tplv-k3u1fbpfcp-watermark.image?)

如上图所示，HTTP明文通信经中间路由最终发送给对方，如果中间某个路由节点抓取了数据，就可以直接看到通信内容，甚至可以篡改后，路由给目标对象，如下：

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/615ffafd9b234d0abe622bc9f3d1ffb0~tplv-k3u1fbpfcp-watermark.image?)

可见HTTP传输是不安全的，但，如果传输的是只有双方可校验的密文，就可以避免被偷窃、篡改，保证传输的安全性，这就是SSL/TLS层做的事情。

## HTTPS从哪些方面保证传输的安全性？

SSL/TLS协议主要从三方面来保证数据传输的安全性：保密、鉴别、完整：

* **身份校验与鉴别**：强制服务器端认证与客户端认证【SSL证书有效性】，来保证消息的源头准确
* **数据保密性**：通过非对称与对称加密保证传输的数据无法被解析
* **数据的完整性**：利用MAC[Message Authentication Codes]消息摘要算法来保证

####  第一个问题：怎么保证通信的另一端是目标端

对用户端而言：怎么保证访问的网站就是目标网站？答案就是**证书**。每个HTTPS网站都需要TLS证书，在数据传输开始前，服务端先将证书下发到用户端，由用户根据证书判断是否是目标网站。这其中的原理是什么，证书又是如何标识网站的有效性呢？证书也叫 digital certificate 或者public key certificate，是密码学中的概念，在TLS中就是指CA证书【**由证书的签发机构（Certificate Authority，简称为 CA）颁布的证书**】，好比是权威部门的公章，WIKI百科解释如下：

> In cryptography, a public key certificate, also known as a digital certificate or identity certificate, is an electronic document used to prove the validity of a public key.[1] The certificate includes information about the key, information about the identity of its owner (called the subject), and the digital signature of an entity that has verified the certificate's contents (called the issuer). If the signature is valid, and the software examining the certificate trusts the issuer, then it can use that key to communicate securely with the certificate's subject. In email encryption, code signing, and e-signature systems, a certificate's subject is typically a person or organization. However, in Transport Layer Security (TLS) a certificate's subject is typically a computer or other device, though TLS certificates may identify organizations or individuals in addition to their core role in identifying devices. TLS, sometimes called by its older name Secure Sockets Layer (SSL), is notable for being a part of HTTPS, a protocol for securely browsing the web.

大意就是证书包含了目标站点的身份信息，并可以通过某种方式校验其合法性，对于任何一个HTTPS网站，你都可以拿到其CA证书公钥信息，在Chrome浏览器中点击HTTPS网站的锁标志，就可以查看公钥信息，并可以导出CA二进制文件：

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8b68cfe8e7ff490c888808132500e3c3~tplv-k3u1fbpfcp-watermark.image?)

浏览器就是通过这个文件来校验网站是否安全合法，可以看到，证书其实内置了一个颁发链条关系，根证书机构->次级证书机构->次次级->网站自身，只要验证这个链条是安全的，就证明网站合法，背后的技术其实是**信任链+RSA的非对称加密+系统内置根证书**。CA在颁发证书的时候，会用自己的私钥计算出要颁发证书的签名，其公钥是公开的，只要签名可被公钥验证就说明该证书是由该CA颁发的，核心校验逻辑如下

* 签名算法：散列函数计算**公开明文信息摘要**，之后采用签名机构的CA私钥对信息摘要进行加密，密文即签名;那如果想要验证证书有效，
* 验签算法：读取证书中的相关的明文信息，采用签名相同的散列函数计算得到信息摘要A，然后获取签名机构的CA公钥，对签名信息进行解密，得到证书信息摘要B，如果A=B则说明证书是由其上级CA签发的，    
    
那么上级的CA又是如何保证安全呢？重复上述操作即可，最终都是靠根证书来验证的，根证书的安全性不需要验证，由系统保证，如此就形成了一个证书的信任链，也就能验证当前网站证书的有效性，证书的信任链校验如下：

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/a8a8807bfea94724a72792a4fe051b6b~tplv-k3u1fbpfcp-watermark.image?)

#### 第二个问题：如何保证数据保密性

TSL协议最大的提升点就是数据的安全，通HTTP通信相比，HTTPS的通信是加密的，在协商阶段，通过非对称加密确定对称加密使用的秘钥，之后利用对称秘钥进行加密通信，这样传输的数据就是密文，就算中间节点泄漏，也可以保证数据不被窃取，从而保证通信数据的安全性。

#### 第三个个问题：数据的完整性

第三个问题，虽然中间节点无法窃取数据，但是还是可以随意更改数据的，那么怎么保证数据的完整性呢，这个其实任何数据传输中都会有这个问题，通过MAC[Message Authentication Codes]信息摘要算法就可以解决这个问题，同普通MD5、SHA等对比，MAC消息的散列加入了秘钥的概念，更加安全，是MD5和SHA算法的升级版，可以认为TSL完整性是数据保密性延伸，接下来就借助WireShark看看TSL握手的过程，并看看是如何实现身份鉴别、保密性、完整性的。

## HTTPS传输的安全性WireShark原理分析

HTTPS安全通信简化来说：**在协商阶段用非对称加密协商好通信的对称秘钥**，然后**用对称秘钥加密进行数据通信**，简易的WireShark TLS/SSL协商过程示意如下：

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ea267a45e8f04ac0aaa075f0e9b99fd1~tplv-k3u1fbpfcp-watermark.image?)

细化分离后示意如下：

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7d3e212f184e49d492c7b7ec733fbe68~tplv-k3u1fbpfcp-watermark.image?)

握手分多个阶段，不多一次握手可以完成多个动作。

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


### Server Key Exchange 

> In Diffie-Hellman, the client can't compute a premaster secret on its own; both sides contribute to computing it, so the client needs to get a Diffie-Hellman public key from the server. In ephemeral Diffie-Hellman, that public key isn't in the certificate (that's what ephemeral Diffie-Hellman means). So the server has to send the client its ephemeral DH public key in a separate message so that the client can compute the premaster secret (remember, both parties need to know the premaster secret, because that's how they derive the master secret). That message is the ServerKeyExchange.

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/11d81a3153444cddbb38991ae05fb774~tplv-k3u1fbpfcp-watermark.image?)

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



# RSA算法与ECDHE[Elliptic Curve Diffie-Hellman Ephemeral ]的区别


RSA简单但是废弃，但是对于理解HTTPS很有帮助，ECDHE协商更优秀，现在都是用这种，得益于算DH算法的优点。

ECDHE协商秘钥流程

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/af7dc23663924342bfb98525a1e9d171~tplv-k3u1fbpfcp-watermark.image?)

# HTTPS是否可以防止中间人攻击及抓包

 无法避免自动授权抓包，Charles就有这个能力，在握手阶段，Charles可以将服务端的公钥进行篡改，生成自己的公钥，本地新人证书后，证书链便可信任，进而抓包通信。证书的可靠性是靠证书链来保证，只要证书是合法的证书机构颁发的，那么网站就是安全的

中间人攻击其实跟Https抓包原理一样，都是要强制添加一个自己的信任根证书



Https协议要做到什么 ：比如防止中间路由器网管篡改信息

中间人


我们需要一个办法来保证服务器传输的公钥确实是服务器的，而不是第三方的。这个时候，我们需要使用 数字证书。数字证书由权威机构 (CA, Certificate Authority) 颁发，里面包含有服务器的公钥，证书文件使用 CA 私钥进行加密。当客户端与服务器建立加密通信的时候，服务器不再返回公钥，而是返回他的数字证书。客户端拿到证书，使用对应的 CA 的公钥解密，然后获取到服务器的公钥。这里有一个问题，客户端怎么拿到 CA 的公钥呢？如果还是去CA 服务器获取的话，那么我们又会回到问题的原点即怎样保证 CA 公钥不被人篡改。因此，大部分浏览器中，权威 CA 的公钥都是内置的，不需要去获取。这就保证了 CA 公钥的正确性。第三方没有办法伪造证书，因为第三方没有 CA 的私钥（当然，CA 被入侵的例子的也是有的，技术永远解决不了人的问题）。

### HTTPS 中间人攻击

HTTPS 可以防止用户在不知情的情况下通信链路被监听，对于主动授信的抓包操作是不提供防护的，因为这个场景用户是已经对风险知情。要防止被抓包，需要采用应用级的安全防护，例如采用私有的对称加密，同时做好移动端的防反编译加固，防止本地算法被破解。

