 

## RSA算法、ECDHE的区别

RSA简单但是废弃，但是对于理解HTTPS很有帮助，ECDHE协商更优秀，现在都是用这种，得益于算DH算法的优点。

ECDHE协商秘钥流程

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/af7dc23663924342bfb98525a1e9d171~tplv-k3u1fbpfcp-watermark.image?)

ECDHE[Elliptic Curve Diffie-Hellman Ephemeral ]

## 前向保密

前向保密（英語：Forward Secrecy，FS）有时也被称为完全前向保密（英語：Perfect Forward Secrecy，PFS），是密码学中通讯协议的一种安全特性，指的是长期使用的主密钥泄漏不会导致过去的会话密钥泄漏。 前向保密能够保护过去进行的通讯不受密码或密钥在未来暴露的威胁。

既然固定一方的私钥有被破解的风险，那么干脆就让双方的私钥在每次密钥交换通信时，都是随机生成的、临时的，这个方式也就是 DHE 算法，E 全称是 ephemeral（临时性的）。

所以，即使有个牛逼的黑客破解了某一次通信过程的私钥，其他通信过程的私钥仍然是安全的，因为每个通信过程的私钥都是没有任何关系的，都是独立的，这样就保证了「前向安全」。


  
 
> 参考文档  https://blog.csdn.net/mrpre/category_9270159.html
> 参考文档 【https://blog.csdn.net/mrpre/article/details/77867439】
> 参考文档  https://www.cnblogs.com/xiaolincoding/p/14318338.html
> 参考文档  https://www.cnblogs.com/xiaolincoding/p/14318338.html
> 参考文档  https://blog.csdn.net/wvqusrtg/article/details/110092210