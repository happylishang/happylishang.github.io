
### Http 抓包

安装后，配置 开启Http 抓包

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/19a285f52f4247be9b1233dfb7a48dd9~tplv-k3u1fbpfcp-watermark.image?)

charless查看本地IP

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c5765b9a47d1483882ee6da20daa856c~tplv-k3u1fbpfcp-watermark.image?)


之后手机配置代理就可以抓http的包

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/34ce6e65ae3a44918a6a1f909c719c2b~tplv-k3u1fbpfcp-watermark.image?)

当然Charless这里要同意。

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/d6701d14eea14464aaf03cb40319c5b0~tplv-k3u1fbpfcp-watermark.image?)

### Http 抓包+代理配置

开启Charless抓包后，本地配置host既可以修改环境


![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/1af40721f2414c61ab4fee561a5bae61~tplv-k3u1fbpfcp-watermark.image?)

你会看到访问的IP其实就是你配置的host对应的ip


![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/197c88833a90405ab4cf2e3ef7727c3c~tplv-k3u1fbpfcp-watermark.image?)

### Http 数据MOCK

可以用本地方式map-local


![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/39dae8b7ae97453782facc2f77ace3a0~tplv-k3u1fbpfcp-watermark.image?)

        {
		 "code": 2000,
					"data": {
						"userName": "测试造数55554",
						"mobile": "140****0001",
						"verifyStatus": 1,
						"idCode": "830543385395924821",
						"idType": 0,
						"userTag": 0
					}
				}


成功数据只需要填写Body就可以

当然也可以用map-remote映射成其他接口，https://designer.mocky.io/ 这个平台直接提供mock接口能力，还具备记忆功能，

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e4015d2281ea4dd297f968c7766c8618~tplv-k3u1fbpfcp-watermark.image?)

配置方式类似，


![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/94148bd4935c48f59bd4b92fae831d1f~tplv-k3u1fbpfcp-watermark.image?)


如果想要动态修改返回Code可以用rewrite的能力


![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/3a9d841a99754a23a71cd3b02ccdfca5~tplv-k3u1fbpfcp-watermark.image?)

### HTTPS抓包

如果抓https，也要把Https打开，另外Android7.0以上的设备支持难度较高，要么ROOT要么修改APP

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b026a864f42746879c8c450f15d0054c~tplv-k3u1fbpfcp-watermark.image?)

也要在手机上安装根证书

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/85c4daf4f15141a594a58b71c24554f1~tplv-k3u1fbpfcp-watermark.image?)

​​
## MAC上抓HTTPS的包

安装根证书并信任

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/91eda4898c544dd4a4bd111d5c985842~tplv-k3u1fbpfcp-watermark.image?)

信任

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/1c13b23bffb64d43972608002370be6b~tplv-k3u1fbpfcp-watermark.image?)

打开配置，设置监听所有域名

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f1ba295556994cdabb0c133dae862af6~tplv-k3u1fbpfcp-watermark.image?)

之后就可以看到https请求

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c4da1d65a3354055a0851ace74d8dd14~tplv-k3u1fbpfcp-watermark.image?)