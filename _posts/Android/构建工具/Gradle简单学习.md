# Android Gradle

## Gradle与Gradlew的概念

## buildscript、repositories、allprojects、dependencies的概念		
	// Top-level build file where you can add configuration options common to all sub-projects/modules.
	
	buildscript {
	
	    apply from: "test.gradle"
	    ext.kotlin_version = '1.3.31'
	    repositories {
	
	        maven {
	            url 'https://maven.google.com/'
	            name 'Google'
	        }
	        jcenter()
	    }
	    dependencies {
	        classpath 'com.android.tools.build:gradle:3.4.1'
	        classpath "org.jetbrains.kotlin:kotlin-gradle-plugin:$kotlin_version"
	    }
	}
	
	allprojects {
	    repositories {
	        maven {
	            url 'https://maven.google.com/'
	            name 'Google'
	        }
	        jcenter()
	        maven { url "https://jitpack.io" }
	
	
	    }
	}

* 1、 buildscript里是gradle脚本执行所需依赖，分别是对应的maven库和插件
* 2、 allprojects里是项目本身需要的依赖，比如我现在要依赖我自己maven库的toastutils库，那么我应该将maven {url 'https://dl.bintray.com/calvinning/maven'}写在这里，而不是buildscript中，不然找不到。
 
 
*  The "buildscript" configuration section is for gradle itself (i.e. changes to how gradle is able to perform the build). So this section will usually include the Android Gradle plugin. （为了构建，仅仅是工具）
* The "allprojects" section is for the modules being built by Gradle.（为了模块，用于代码）
* Oftentimes the repository section is the same for both, since both will get their dependencies from jcenter usually (or maybe maven central). But the "dependencies" section will be different.（repository标识库的来源，很多模块可以相同，但是，每个模块自己的dependencies--用到的库是不同的）
* Usually the "dependencies" section for "allprojects" is empty since the dependencies for each module are unique and will be in the "build.gradle" file within each of the modules. However, if all of the modules shared the same dependencies then they could be listed here. （allprojects中的dependencies一般是空，因为很少有的库需被所有模块需要，一般都是每个模块自己定义自己的）


## 构建任务及配置

//标识改构建组使用com.android.application插件（插件名，该插件中会有各种构建任务，而这里是对构建任务的配置）
apply plugin: 'com.android.application'
apply plugin: 'kotlin-android'

## Gradle插件

什么是Gradle插件：其实就是Task的集合，最大的作用是让构建逻辑可重用




事实上，“Hello world”甚至可能在task被执行前打印。为了理解发生了什么，我们需要重返基础。在第一章，我们讨论了Gradle构建的生命周期。在任何Gradle构建中都有三个阶段：初始化阶段、配置阶段和执行阶段。当以如上面例子相同的方式添加代码到task中，实际上建立了task的配置。即使你要执行一个不同的task，“Hello World”消息还是会显示出来。

如果想在执行阶段添加action到task中，使用这个符号：

task hello << {
    println 'Hello, world!'
}
1
2
3
唯一的差别在于<<在闭包前面。告诉Gradle这块代码是针对执行阶段而非配置阶段的。

