---
title: 分类目录
layout: default
comments: yes
---

<div id='tag_cloud'>
{% for cat in site.categories %}
<a href="#{{ cat[0] }}" title="{{ cat[0] }}" rel="{{ cat[1].size }}">{{ cat[0] }} ({{ cat[1].size }})</a>
{% endfor %}
</div>

<ul>
{% for cat in site.categories %}
  <li class="listing-seperator" id="{{ cat[0] }}"> {{ cat[0] }} </li>
{% for post in cat[1] %}
  <li class="listing-item">{line-height:20px;}
{{ post.date | date_to_string }}   <a href="{{ site.url }}{{ post.url }}" title="{{ post.title }}">{{ post.title }} <br /></a> 
  </li>
{% endfor %}
{% endfor %}
</ul>

<script src="/media/js/jquery.tagcloud.js" type="text/javascript" charset="utf-8"></script> 
<script language="javascript">
$.fn.tagcloud.defaults = {
    size: {start: 1, end: 1, unit: 'em'},
      color: {start: '#f8e0e6', end: '#ff3333'}
};

$(function () {
    $('#tag_cloud a').tagcloud();
});
</script>
