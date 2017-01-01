---
layout: post
description: "Java"
title: "Java反射机制"
category: Java
tags: [Binder]

---


* 什么是Java反射机制
* Java反射机制有什么用
* 怎么用

#### Class概念

Class类封装一个对象和接口运行时的状态，当装载类时，Class类型的对象自动创建。      Class 没有公共构造方法。Class 对象是在加载类时由 Java 虚拟机以及通过调用类加载器中的 defineClass 方法自动构造的，因此不能显式地声明一个Class对象。 一般某个类的Class对象被载入内存，它就用来创建这个类的所有对象。

class 是java的关键字, 在声明java类时使用;
Class 是java JDK提供的一个类,完整路径为 java.lang.Class;

区别是指两个或两个以上的事物间的不同,当两种相似的事物作比较时，它们的不同点便是区别。
那么 class和Class的相似性就只有一个,那都是单词"class",就是一个为首字母大写,一个为小写.

class和Class的作用:
1. class只用于类声明;
2. Class则用于抽象类的相关信息. java是面向对象的, 一般是把一些事物抽象成一个类,比如将学生信息抽象成Student这个类;Student类会抽象学生的姓名/性别/生日等信息;
那么java中也就把java中的类也抽象成了一个类叫Class;Class中抽象了类的包名/类名/属性数组/方法数组等;

#### 一般做法和反射API

* //一般做法

		MyClass myClass = new MyClass(0);  
		myClass.increase(2);
		System.out.println("Normal -> " + myClass.count);
	
* 反射API

			try {
		    Constructor constructor = MyClass.class.getConstructor(int.class); //获取构造方法
		    MyClass myClassReflect = constructor.newInstance(10); //创建对象
		    Method method = MyClass.class.getMethod("increase", int.class);  //获取方法
		    method.invoke(myClassReflect, 5); //调用方法
		    Field field = MyClass.class.getField("count"); //获取域
		    System.out.println("Reflect -> " + field.getInt(myClassReflect)); //获取域的值
		} catch (Exception e) { 
		    e.printStackTrace();
		} 
		
		
#### 动态代理

代理模式： 代理对象和被代理对象一般实现相同的接口，调用者与代理对象进行交互。代理的存在对于调用者来说是透明的，调用者看到的只是接口。代理对象则可以封装一些内部的处理逻辑，如访问控制、远程通信、日志、缓存等。比如一个对象访问代理就可以在普通的访问机制之上添加缓存的支持。这种模式在RMI和EJB中都得到了广泛的使用。传统的代理模式的实现，需要在源代码中添加一些附加的类。这些类一般是手写或是通过工具来自动生成。JDK 5引入的动态代理机制，允许开发人员在运行时刻动态的创建出代理类及其对象。在运行时刻，可以动态创建出一个实现了多个接口的代理类。每个代理类的对象都会关联一个表示内部处理逻辑的InvocationHandler接 口的实现。当使用者调用了代理对象所代理的接口中的方法的时候，这个调用的信息会被传递给InvocationHandler的invoke方法。在 invoke方法的参数中可以获取到代理对象、方法对应的Method对象和调用的实际参数。invoke方法的返回值被返回给使用者。这种做法实际上相 当于对方法调用进行了拦截。熟悉AOP的人对这种使用模式应该不陌生。但是这种方式不需要依赖AspectJ等AOP框架。

#### 参考文档

[深入研究java.lang.Class类](http://lavasoft.blog.51cto.com/62575/15433/)