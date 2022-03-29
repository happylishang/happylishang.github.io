* MVP需要定义各种Interface
* MVC VC可能比较臃肿
* MVVM，databinding的方式，真的恶心，如果讲databinding换成用户自己监听，还算可以也即是下面的
* MVI即Model-View-Intent，

* 核心还是一个观察者模式，只是如何清晰简单的实现这个观察者模式，说人话就是，方便不方便注册更新观察者。


Moshi 相比Gson，多了支持默认值，至于空安全，其实Gson跟Moshi都会crash，只不过gson在用的时候，moshi在解析的时候。

