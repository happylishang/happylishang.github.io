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

