> 语言只是语言，结合平台才有线程之类的概念

##  setTimeout的Timer线程

	function hello() {
	    console.log("Hello World");
	
	    setTimeout(() => {
	        console.log("Hello World 3000");
	    },3000)
	
	    console.log("Hello World after setTimeout");
	}
	
上述函数执行的时候， setTimeout会将任务加入到Timer线程，等到倒计时结束，timer线程将回调任务插入到JS主线程，跟Android的MessageQueue+Loop有点类似，只不过，浏览器里timer是一个单独的倒计时线程。

## Event Loop概念：参考Android的Looper+Queue

* 一个进程中，只有一个JS内核，负责处理Javascript脚本程序
* JS引擎一直等待着任务队列中任务的到来，然后加以处理
* GUI渲染线程与JS引擎线程是互斥的
* JS引擎线程生存在Render进程（浏览器渲染进程）JavaScript可以修改DOM，如果不是单线程，以谁为准？就像UI线程只有一个一样

WebWorker会造成js多线程吗？

Worker接口会生成真正的操作系统级别的线程。所以这里的webworker不是一个新的js引擎线程。而是操作系统级别的线程。线程的执行不会影响到原有的js引擎的执行，也不会影响到浏览器渲染Render进程。

所以WebWorker有以下限制： 1、不能访问DOM和BOM对象的，Location和navigator的只读访问，并且navigator封装成了WorkerNavigator对象，更改部分属性。无法读取本地文件系统

#### 渲染线程：js线程负责修改dom，定制UI，渲染线程负责渲染出来

也叫渲染引擎（或者大家俗语上的浏览器内核，但其实**浏览器内核包括渲染引擎和JS引擎**），主要作用是：

负责渲染浏览器界面，解析HTML，CSS，构建DOM树和RenderObject树，布局和绘制等。
当界面需要重绘（Repaint）或由于某种操作引发回流(reflow)时，该线程就会执行
注意，**GUI渲染线程与JS引擎线程是互斥的**，当JS引擎执行时GUI线程会被挂起（相当于被冻结了），GUI更新会被保存在一个队列中等到JS引擎空闲时立即被执行。


android 的UI线程相当于  把Js线程跟渲染线程统一了，js中既然是互斥，为何不统一成一个线程呢？


#### 其他线程

事件触发线程

* 归属于浏览器而不是JS引擎，用来控制事件循环（可以理解，JS引擎自己都忙不过来，需要浏览器另开线程协助）
* 当JS引擎执行代码块如setTimeOut时（也可来自浏览器内核的其他线程,如鼠标点击、AJAX异步请求等），会将对应任务添加到事件线程中
* 当对应的事件符合触发条件被触发时，该线程会把事件添加到待处理队列的队尾，等待JS引擎的处理
* 注意，由于JS的单线程关系，所以这些待处理队列中的事件都得排队等待JS引擎处理（当JS引擎空闲时才会去执行）

定时触发器线程

* 传说中的setInterval与setTimeout所在线程
* 浏览器定时计数器并不是由JavaScript引擎计数的,（因为JavaScript引擎是单线程的, 如果处于阻塞线程状态就会影响记计时的准确）
* 因此通过单独线程来计时并触发定时（计时完毕后，添加到事件队列中，等待JS引擎空闲后执行）



### 参考文档

[https://lq782655835.github.io/blogs/js/http-base-2.browser.html](https://lq782655835.github.io/blogs/js/http-base-2.browser.html)

[输入URL背后的技术步骤](https://lq782655835.github.io/blogs/js/http-base-1.url.html)