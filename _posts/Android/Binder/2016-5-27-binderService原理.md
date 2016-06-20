---
layout: default
title: "binderService原理"
description: "Java"
categories: [android,Binder]
tags: [Binder]

---

## getSystemSrvice进程唤醒的关键是ServiceManager这个单利的中转，找ServiceManager，容易，找其他的node，ServiceManager帮你找到。并帮你创建本地的ref，之后就很简单找到了，不是维护全局List，而是让ServiceManager维护服务List

#### binderService 是通过AMS进行中转，如果Service没启动，就启动Service，之后进行Publish将新进程的Bidner的代理转发给各个端口，谁需要发给谁，但是5.0已经将权限回收，binderService用途不大。

#### SocketPair

#### 初衷

### bindService是异步的
### 回调时机

写一个Binder服务，为其他想要调用的提供Bind接口，复用 ，Service也只是一个入口

#### AMS绑定过程，注意看看是否已经绑定过，如果绑定过，就直接拿到

		
		int bindServiceLocked(IApplicationThread caller, IBinder token,
		            Intent service, String resolvedType,
		            IServiceConnection connection, int flags, int userId) {
		…
		            ConnectionRecord c = new ConnectionRecord(b, activity,
		                    connection, flags, clientLabel, clientIntent);
		 
		            IBinder binder = connection.asBinder();
		 
		            if ((flags&Context.BIND_AUTO_CREATE) != 0) {
		                s.lastActivity = SystemClock.uptimeMillis();
		                if (bringUpServiceLocked(s, service.getFlags(), callerFg, false) != null){
		                    return 0;
		                }
		            }
		…
		            if (s.app != null && b.intent.received) {
		                // Service is already running, so we can immediately
		                // publish the connection.
		                try {
		                    c.conn.connected(s.name, b.intent.binder);
		                } catch (Exception e) {
		                    Slog.w(TAG, "Failure sending service " + s.shortName
		                            + " to connection " + c.conn.asBinder()
		                            + " (in " + c.binding.client.processName + ")", e);
		                }
		 
		                // If this is the first app connected back to this binding,
		                // and the service had previously asked to be told when
		                // rebound, then do so.
		                if (b.intent.apps.size() == 1 && b.intent.doRebind) {
		                    requestServiceBindingLocked(s, b.intent, callerFg, true);
		                }
		            } else if (!b.intent.requested) {
		                requestServiceBindingLocked(s, b.intent, callerFg, false);
		            }
		}
		
### 参考文档：


【1】[android4.4组件分析--service组件-bindService源码分析](http://blog.csdn.net/xiashaohua/article/details/40424767)