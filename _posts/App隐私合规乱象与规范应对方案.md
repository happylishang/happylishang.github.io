### 背景 难点 工作 收益 未来展望？ 

### 背景



![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/75c825e2d24845bbb7f9e8d34e1c3f34~tplv-k3u1fbpfcp-watermark.image?)


### 自动化合规检查？人肉


今天找张三核查，核查规则张三定，明天找李四，核查规则李四定，每个视角都能查出问题，不断的整改，改成一个四不像，比如我们的拍照功能，点击拍照，按照有些合规的要求，要先弹出第一个说明，为什么要申请相机权限，第二个才是系统的权限，申请相机权限，用户是傻子吗，还需要解释为什么需要相机，拍照不用相机用陀螺仪吗，真的想说一句，什么东西。

隐私合规：本是处于隐私保护，但最后却成了，扯一张虎皮，领一群狐朋狗友，拦路抢羊。


### 关于统一的拦截 隐私弹窗的设计


* 第一个弹窗固定放在闪屏，跟随APP更新，
* 第二个弹窗走首页，动态展示


在第一个隐私策略同意之前不许搜集任何信息，其实也不许做任何上报类的操作

/data/local/tmp/frida-server   

 sudo pip install frida
 
 
ps -A | grep frida

杀死进程
kill -9 <process>



frida -U -f com.tencent.k12gy -l D:\ADB\fridascript.js --no-paus


主要是环境配置 python3  pip  等，环境好了直接就跑起来了

cd /Users/personal/prj/Github/camille/

 python /Users/personal/prj/Github/camille/camille.py com.netease.yanxuan
