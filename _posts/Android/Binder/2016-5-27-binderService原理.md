---
layout: default
title: "binderService原理"
description: "Java"
categories: [android,Binder]
tags: [Binder]

---

## 进程唤醒的关键是ServiceManager这个单利的中转，找ServiceManager，容易，找其他的node，ServiceManager帮你找到。并帮你创建本地的ref，之后就很简单找到了，不是维护全局List，而是让ServiceManager维护服务List，跟鲁棒

> [目录]    
> [Binder概述](#sumery_binder)  
> [问题引入原理](#binder_qusetions_index)  
> [ServiceManager化身大管家](#ServiceManager)  
> [Service实现逻辑](#service_part_arch)  
> [    - Service自身服务的实现](#service_self_implement)  
> [    - Service注册逻辑](#service_self_register)   
> [Android应用层对Binder的支持](#java_binder_ref)     
 
### 参考文档：


【1】	<http://blog.csdn.net/universus/article/details/6211589# Binder> 接收线程管理，这里真正理解了如何区分查找目标线程或者进城

【2】	<http://blog.csdn.net/yangwen123/article/details/9254827> BC_ENTER_LOOPER的作用，注册线程到Binder驱动，并不是所有的线程都是Binder线程

【3】	<http://www.cnblogs.com/angeldevil/archive/2013/03/10/2952586.html>  Android指针管理：RefBase,SP,WP

【4】	
