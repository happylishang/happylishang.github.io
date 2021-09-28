1、冷启动与路由


* 无效布局
* 闪屏背景图，缩减
* 启动优先级，异步线程
* 路由界面的背景

冷气动如何优化，闪屏如何处理

视频：缓存处理，预加载等

配置中文

	   resConfigs "en", "zh-rCN"
   
冷启动耗时统计：   



 

adb shell am start -W  -d "yanxuan://yxwebview?url=https%3A%2F%2Fact.you.163.com%2Fact%2Fpub%2Fssr%2F8JxAkm0k17Xh.html%3F_mid%3D100000%26brand%3Dhuawei"  -a  android.intent.action.VIEW