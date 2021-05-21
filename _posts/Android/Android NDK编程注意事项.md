Android开发中，Native开发的场景不是特别多，一般而言只要上层

## Native工程搭建

核心PRJ配置如下图

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/0ae524be70fb490b864310a4e3080bc4~tplv-k3u1fbpfcp-watermark.image)

支持native开发的核心配置如下

* 添加CMakeLists.txt编译配置文件
* build文件中添加Native配置

		android {
 
    		compileSdkVersion 30
    		...
			    defaultConfig {
			        ...
			        externalNativeBuild {
			            cmake {
			                cppFlags ""
			            }
			        }
			    }
		
			    externalNativeBuild {
			        cmake {
			        <!--刚才的CMakeLists.txt编译配置文件-->
			            path "src/main/cpp/CMakeLists.txt"
			            version "3.10.2"
			        }
			    }

* 修改CMakeLists.txt，一般有模板可以供你参考

你可以通过指导搭建一个空的Native工程作为参考：

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/51d5587b4318409cb500eb88210b1aee~tplv-k3u1fbpfcp-watermark.image)

如果想要做成Module组件，可以新建一个Library,拷贝上述配置即可。

## Native编码

Java与Native名字：两种处理方式

> 1，静态绑定的方式

采用包名+类名+方法名组合的方式，强制映射

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/a7d5b4270d1a4188903bd65be525f922~tplv-k3u1fbpfcp-watermark.image)

> 2、动态注册的方式【比较安全】

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/1b522839402344e28dce6523f854e8c0~tplv-k3u1fbpfcp-watermark.image)

这种方式相对第一种安全一些，可以帮助隐藏符号表，有一定的混淆效果。


## Native SO 安全

动态注册可以隐藏方法名，让破解者更难找，但这也只是防止破解so的一个点，还可以通过加入一些校验的方式来防止其他APP使用你的so库，比如埋一些自己APP特有值，通过so中反查对比，通过这种方式防止so被其他APP直接调用，可以直接用的特征值有

* 自己APP端包名
* 打包的秘钥签名
* 自己创建的一些核验文案
* 第三方加固


