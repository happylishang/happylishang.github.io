### 背景 难点 工作 收益 未来展望？ 

### 背景



![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/75c825e2d24845bbb7f9e8d34e1c3f34~tplv-k3u1fbpfcp-watermark.image?)


### 自动化合规检查？人肉


今天找张三核查，核查规则张三定，明天找李四，核查规则李四定，每个视角都能查出问题，不断的整改，改成一个四不像，比如我们的拍照功能，点击拍照，按照有些合规的要求，要先弹出第一个说明，为什么要申请相机权限，第二个才是系统的权限，申请相机权限，用户是傻子吗，还需要解释为什么需要相机，拍照不用相机用陀螺仪吗，真的想说一句，什么东西。

隐私合规：本是处于隐私保护，但最后却成了，扯一张虎皮，领一群狐朋狗友，拦路抢羊。


### 关于统一的拦截 隐私弹窗的设计


* 第一个弹窗固定放在闪屏，跟随APP更新，
* 第二个弹窗走首页，动态展示

### 如何HOOK探测同以前的隐私调用

在第一个隐私策略同意之前不许搜集任何信息，其实也不许做任何上报类的操作


## Frida 用法

* 安装python3 ，安装Python3 安装python3 配套的pip
*  安装Frida    	pip install frida frida-tools Frida --v 可查看安装的版本
* root的手机上安装frida-server  ,并修改权限 chmod 777 frida-server   ，比如下载的这个版本必须和PC环境 Frida --v 相同 比如./frida-server-16.0.2-android-arm 
* 手机上启动server /data/local/tmp/frida-server-16.0.2-android-arm 
* PC 启动 **python3**  /Users/personal/prj/Github/camille/camille.py com.netease.yanxuan   Frida及脚本,
* adb shell '/data/local/tmp/*frida* --version'

tips注意如果你用的是python3，这里都是python3  pip3 ，他们是一套，正确启动如下

	git clone https://github.com/zhengjim/camille.git
	cd camille
	pip3 install -r requirements.txt
	python3 camille.py -h

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c171971a37e2471c8dec6c59771076d7~tplv-k3u1fbpfcp-watermark.image?)
 
如果端口绑定，则杀死特定进程，并启动frida

	ps -A | grep frida
	
	kill -9 <process>
	
主要是环境配置 python3  pip  等，环境好了直接就跑起来了

	cd /Users/personal/prj/Github/camille/
	
	 python /Users/personal/prj/Github/camille/camille.py com.netease.yanxuan

    xlwt插
    pip3  install xlwt 
   
### 如何查看APK是否调用了系统限定的API

你想APP，用jadx直接看就可以，后者自己通过dex2jar换成jar，再换成java，好像效果类似

	#!/bin/bash
	cd /Users/personal/soft/dex-tools-2.1
	if [   -n  "$2" ] ;then
	  sh d2j-dex2jar.sh -f  "$1" -o "$2"
	else
	  pre="$1"
	  #用自己接住自己
	  pre=`echo ${pre/%.dex/.jar}`
	  sh d2j-dex2jar.sh -f  "$1" -o $pre
	fi
	
dex2jar 再将jar换成java

也可以直接jadex-gui，还可以将所有java导出，方便查看



或者扫描Smali代码  **https://juejin.cn/post/7039512844768903198**


adb shell am start -W  -d "yanxuan://yxwebview?url=https://test.yx.mail.netease.com/vip-app/index.html#/coredata?tab=xiaoshou"  -a  android.intent.action.VIEW


 