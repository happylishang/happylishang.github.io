### 简介
  
  该博客，是参考Jekyll-now进行搭建的，界面简单，只是额外添加了一个分类。主要是自己撰写一些技术文档，个人技术总结。
  
#### 添加本地图片的方式
  存到images下面，如果想要技能预览又能显示，可以采用如下方式
  ```
  <img src="../../../images/android/dalvik/framework.png"  alt="Android进程框架" width="800"/>

```
#### 添加Top返回

参考<http://www.smslit.top/jekyll/2015/10/28/backToTop-Jekyll.html>
```
<div id="backtop">
   <a href="#">TOP</a>
</div> 
```
 添加按钮的css样式，可以按照自己喜欢修改样式即可。
```
#backtop a { /* back to top button */
    text-align: center;
    line-height: 50px;
    font-size: 16px;
    width:50px;
    height: 50px;
    position: fixed;
    bottom: 10px; /* 小按钮到浏览器底边的距离 */
    right: 60px; /* 小按钮到浏览器右边框的距离 */
    color: rgb(64,120,192); /* 小按钮中文字的颜色 */
    z-index: 1000;
    background: #fff; /* 小按钮底色 */
    padding: auto; /* 小按钮中文字到按钮边缘的距离 */
    border-radius: 50px; /* 小按钮圆角的弯曲程度（半径）*/
    -moz-border-radius: 50px;
    -webkit-border-radius: 50px;
    font-weight: bold; /* 小按钮中文字的粗细 */
    text-decoration: none !important;
    box-shadow:0 1px 2px rgba(0,0,0,.15), 0 1px 0 #ffffff inset;
}

#backtop a:hover { /* 小按钮上有鼠标悬停时 */
    background: rgba(64,120,192,0.8); /* 小按钮的底色 */
    color: #fff; /* 文字颜色 */
}
```
