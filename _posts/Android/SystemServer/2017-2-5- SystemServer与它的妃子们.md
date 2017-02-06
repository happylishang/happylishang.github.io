---
layout: post
title: "SystemServer与它的妃子们"
description: "Android"
categories: [Android]

---

# SystemServer的启动

# 妃子们的启动，通信与Handler的Looper

比如AMS，自己是个BBinder实体，但是为了异步处理请求，重新开启了一个Thread，并且绑定Looper，利用Handler消息机制处理。

# 地位

# 功能