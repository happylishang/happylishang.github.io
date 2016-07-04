---
layout: post
title: "Chormium WebView原理分析--入门篇"
description: "TodoLis"
categories: [Chormium]

---



#### WebView加载数据原理

Webview加载数据其实还是通过Http网络请求去下载数据，下载后本地解析渲染，其实就是这个原理，页面内部的js网络请求也是本地Webview发送的。	        