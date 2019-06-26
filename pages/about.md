---
layout: page
title: About
description: 每个和尚都应该为自己挖口井
comments: true
menu: 关于
permalink: /about/
---



## 联系

{% for website in site.data.social %}
* {{ website.sitename }}：[@{{ website.name }}]({{ website.url }})
{% endfor %}

## Skill Keywords


* Android源码框架（AMS WMS Binder Handler）
* 打包签名(V1 V2漏洞)
* App与H5混合开发
* 性能优化 泄露+OOM 
* 新技术
* 风控防刷+模拟器识别
* NDK混合开发
* 


{% for category in site.data.skills %}
### {{ category.name }}
<div class="btn-inline">
{% for keyword in category.keywords %}
<button class="btn btn-outline" type="button">{{ keyword }}</button>
{% endfor %}
</div>
{% endfor %}
