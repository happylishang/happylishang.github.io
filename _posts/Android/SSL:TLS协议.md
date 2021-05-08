SSL的升级版TLS 1.0版，TLS 1.0通常被标示为SSL 3.1，TLS 1.1为SSL 3.2，TLS 1.2为SSL 3.3。


SSL/TLS协议的基本思路是采用公钥加密法，也就是说，客户端先向服务器端索要公钥，然后用公钥加密信息，服务器收到密文后，用自己的私钥解密。

*    客户端向服务器端索要并验证公钥。 
*   双方协商生成"对话密钥"。
*    双方采用"对话密钥"进行加密通信。

"握手阶段"涉及四次通信 ，"握手阶段"的所有通信都是明文的。


![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/abaca8e4f06e4cfaadb636f9c8d1f6ad~tplv-k3u1fbpfcp-watermark.image)


![](https://pic4.zhimg.com/80/v2-5aff714cb0cd14387cfad488adef97db_720w.jpg)