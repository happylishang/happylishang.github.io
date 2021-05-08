当用户或者客服或者内部人员反馈问题后，如何搜集信息，进行问题排查 ？

##  如果是用户反馈的信息可以查看用户的反馈日志，


![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/879efea754b943deac729192dc2257c1~tplv-k3u1fbpfcp-watermark.image)

查看附件中的内容，如果是常见的crash一般会有crash日志，如下


![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c9936a022252483c85e6d7181902c886~tplv-k3u1fbpfcp-watermark.image)


## 如果是客服反馈的，可以先引导用户发反馈，如果走不通，可以借助Firebase或者APM平台

客服反馈的一般都会有用户id，可以根据用户id在APM上查询用户的设备信息，或者账号信息，进而获取其他日志，比如APM可以查询用户的操作轨迹，firebase也可以根据设备ID，查看用户的崩溃情况[Firebase链接](https://console.firebase.google.com/project/yanxuan-app/crashlytics/app/android:com.netease.yanxuan/issues?hl=zh-cn&state=open&time=last-twenty-four-hours&type=crash)


![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/93baf4989b264a8fa46a41cdb4d7685d~tplv-k3u1fbpfcp-watermark.image)

如果是反馈的网络问题，可以去APM根据设备信息，或者账号信息查询 [ APM 链接 ](http://yx.mail.netease.com/caesar-admin-mobile/#/search?appId=yanxuan-android&searchText=941b3e5ba26b26f39e28ae35d63aabb&searchType=deviceId)

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/d973fbabc9c54f9aa6bdc8c7ace5f8b6~tplv-k3u1fbpfcp-watermark.image)


也可以借助APM 用户轨迹查询用户的操作轨迹，看看共性，或者利用设备型号等信息查看占比[用户访问轨迹](http://yx.mail.netease.com/goldeneye/d/h-WX6xOMz/yong-hu-fang-wen-gui-ji-ji-wen-ti-pai-cha?orgId=25&refresh=5m)



![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/5146c4e215944baaaa6d5b6d3afbb8e6~tplv-k3u1fbpfcp-watermark.image)


