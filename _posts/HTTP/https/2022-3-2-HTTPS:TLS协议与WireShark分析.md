---

layout: post
title: HTTPS/TLS协议与WireShark分析
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

TLS协议最大的提升点就是数据的安全，通HTTP通信相比，HTTPS的通信是加密的，在协商阶段，通过非对称加密确定对称加密使用的秘钥，之后利用对称秘钥进行加密通信，这样传输的数据就是密文，就算中间节点泄漏，也可以保证数据不被窃取，从而保证通信数据的安全性。

#### 第三个个问题：数据的完整性

第三个问题，虽然中间节点无法窃取数据，但是还是可以随意更改数据的，那么怎么保证数据的完整性呢，这个其实任何数据传输中都会有这个问题，通过MAC[Message Authentication Codes]信息摘要算法就可以解决这个问题，同普通MD5、SHA等对比，MAC消息的散列加入了秘钥的概念，更加安全，是MD5和SHA算法的升级版，可以认为TLS完整性是数据保密性延伸，接下来就借助WireShark看看TLS握手的过程，并看看是如何实现身份鉴别、保密性、完整性的。

## HTTPS传输的安全性WireShark原理分析

HTTPS安全通信简化来说：**在协商阶段用非对称加密协商好通信的对称秘钥**，然后**用对称秘钥加密进行数据通信**，简易的WireShark TLS/SSL协商过程示意如下：

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ea267a45e8f04ac0aaa075f0e9b99fd1~tplv-k3u1fbpfcp-watermark.image?)

细化分离后示意如下：

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7d3e212f184e49d492c7b7ec733fbe68~tplv-k3u1fbpfcp-watermark.image?)

握手分多个阶段，不过一次握手可以完成多个动作，而且也并不是所有类型的握手都是上述模型，因为协商对称秘钥的算法不止一种，所以握手的具体操作也并非一成不变，比如RSA就比ECDHE要简单的多，目前主流使用的都是ECDHE，具体流程拆分如下：


#### Client Hello 【TLS/SSL握手发起】

Client Hello是TLS/SSL握手发起的第一个动作，类似TCP的SYN，Client Hello 阶段客户端会指定版本，随机数、支持的密码套件供服务端选择，具体的包数据如下
 
  ![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8ce47a18cf704fa4a3014e96188cf856~tplv-k3u1fbpfcp-watermark.image?)启动TLS握手过程，**提供自己所能支持各种算法，同时提供一个将来所能用到的随机数**。
  
ContentType指示TLS通信处于哪个阶段阶段，值22代表Handshake，握手阶段，Version是TLS的版本1.2，在握手阶段，后面链接的就是握手协议，这里是Client Hello，值是1，同时还会创建一个随机数random给Server，它会在生成session key【对称密钥】时使用。之后就是支持的供服务端选择的密码套件，接下来等服务端返回。
 
#### Server Hello 

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e34eb38cee2b43f391e0324e5e0e9e68~tplv-k3u1fbpfcp-watermark.image?)

Handshake Type: Server Hello (2)，作为对Client Hello的响应 ，**确定使用的加密套件**: TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 (0xc02f)，密钥协商使用 ECDHE，签名使用 RSA，
数据通信通信使用 AES 对称加密，并且密钥长度是128位，GCM分组，同时生成一个服务端的random及会话ID回传。

####  Certificate  服务端发送证书

这一步服务器将配置的证书【链】发送给客户端，客户端基于上文所述的证书链校验证书的有效性，这里发送的证书是二进制格，可以利用wireshark右键“Export Packet Bytes”功能，导出.CER格式的证书。如下可以看到传输的证书链。

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/9e70d47eb40a4b2188c75877bcb2a4f2~tplv-k3u1fbpfcp-watermark.image?)

导出的CER格式的证书如下，最关键的就其公钥跟数字签名。

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/56fbc9e8317e40c9b8aac044cb932869~tplv-k3u1fbpfcp-watermark.image?)


#### Server Key Exchange   Server Hello Done


Server Key Exchange是针对选定的ECDHE协商所必须的步骤，Diffie-Hellman模型解释如下：

> In Diffie-Hellman, the client can't compute a premaster secret on its own; both sides contribute to computing it, so the client needs to get a Diffie-Hellman public key from the server. In ephemeral Diffie-Hellman, that public key isn't in the certificate (that's what ephemeral Diffie-Hellman means). So the server has to send the client its ephemeral DH public key in a separate message so that the client can compute the premaster secret (remember, both parties need to know the premaster secret, because that's how they derive the master secret). That message is the ServerKeyExchange.

大意就是ephemeral Diffie-Hellman不会使用证书中的静态公钥参与对称秘钥的生成，而是需要服务端与客户端通过彼此协商确定对称秘钥，而D-H算法模型需要两对非对称秘钥对，各端保留自己的私钥，同时握有双方的公钥，然后基于D-H算法双端可以算出一样的对称加密秘钥，而这就需要C/S互传自己的公钥

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/45b7ac6852b84bbebbc67b7f9e23db7c~tplv-k3u1fbpfcp-watermark.image?)

服务端做完这一步，其实ECDHE算法中服务端需要提供的信息已经结束，发送 Server Hello Done告诉客户端，然后等待客户端回传它的D-H公钥。
 
![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/3c05475b5ed440539b89f74770e51493~tplv-k3u1fbpfcp-watermark.image?)


算法：

	Client端私钥keyc，计算C端公钥pubKC = g^keyc mod p，Server端私钥keys，计算S端公钥pubKS = g ^ keys mod p
	
	pubKS ^ keyc mod p=  pubKC ^ keys mod p 

其中p和g是公开的DH参数，只要P是一个足够大的数，在不知道私钥的情况下，即使截获了双方的公钥，也是很难破解的。

####  Client Key Exchange, Change Cipher Spec, Encrypted Handshake Message

客户端收到服务端的证书后，利用信任链检测证书有效性，同时也会同Server Key Exchange 类似，将自己的Diffie-Hellman公钥发送给Server端，

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/9e04b43b0de9420db98f0dcbd41deec4~tplv-k3u1fbpfcp-watermark.image?)

至此，ECDHE协商所需要的信息都传输完毕， 双方都可以基于ECDHE算法算出的共享密钥，同时结合之前的随机数生成最终的对称加密秘钥：

		  客户端随机数 & 服务端随机数  &  ECDHE 算法算出的共享密钥 

之后客户端发送Change Cipher Spec与 Encrypted Handshake Message标识握手完成，同时传输一个加密的数据给Server，验证双方确立的秘钥是否正确，这就需要服务端也要重复这个操作给客户端，这样才能验证彼此的加解密一致，即服务端也要来一次Encrypted Handshake Message回传给客户端校验，

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/12ed22a64dcc4a4da9997d9ff08659d6~tplv-k3u1fbpfcp-watermark.image?)

走完如上流程，双方就确认了正确的对称加密通道，后面就是TLS的数据通信，其Record Layer的ContentType  也会变成 Content Type: Application Data (23)：

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/d338c3a349f341a9b6b35bf824179c2f~tplv-k3u1fbpfcp-watermark.image?)

#### Client Hello与Server Hello阶段交换的随机数有什么用 

最终对称会话密钥包含三部分因子：

* 客户端随机数
* 服务端随机数 
* ECDHE 算法算出的共享密钥

Client Hello与Server Hello阶段交换的随机数，是为了提高秘钥的「随机」程度而进行的，这样有助于提高会话密钥破解难度。


## HTTPS中间人攻击及抓包

HTTPS通过加密与完整性校验可以防止数据包破解与篡改，但对于主动授信的抓包操作是没法防护，比如Charles抓包，在这个场景用户已经风险，并且将Charles提供的证书信任为根证书，这从源头上构建了一条虚拟的信任链：在握手阶段，Charles利用自己的公钥，生成客户端可以信任的篡改证书，从而可以充作中间人进而抓包，所谓中间人攻击，感觉跟Https抓包原理一样，都是要强制添加一个自己的信任根证书。
 
### 参考 
 
> 参考文档  https://blog.csdn.net/mrpre/category_9270159.html
> 参考文档 【https://blog.csdn.net/mrpre/article/details/77867439】
> 参考文档  https://www.cnblogs.com/xiaolincoding/p/14318338.html
> 参考文档  https://www.cnblogs.com/xiaolincoding/p/14318338.html
> 参考文档  https://blog.csdn.net/wvqusrtg/article/details/110092210