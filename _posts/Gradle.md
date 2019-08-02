Grade

Gradle构建流程
先配置，再执行
> Configure project :snailt
hello world
> Task :snailt:printHelloWorld UP-TO-DATE


	task("printHelloWorld") {
	    print("hello world")
	}

如果是如上写法，那在配置阶段就输出了