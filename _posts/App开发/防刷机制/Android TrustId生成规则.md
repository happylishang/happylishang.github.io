# Android TrustId生成规则

# 客户端部分

客户端拿真实度较高的信息交给服务端，服务端返回唯一trustId，如果手机存在作假，服务端需要识别作假信息，并映射到同一个手机的trustId，之后，客户端往APP服务发请求的时候，都要携带trustId

![可信ID流程.jpg](http://upload-images.jianshu.io/upload_images/1460468-c7fe93188ac1a271.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


## DeviceID生成策略  

先去拿如下四个关键信息，IMEI、 MAC、 序列号、 AndroidId ，根据取到的数据及有效性生成DeviceId。规则如下

* 如果IMEI+MAC都能获取到，并且IMEI不是000000000000格式，MAC地址不是02：00：00：00：00：00格式，则利用IMEI+MAC的MD5作为DeviceID
* 如果不能同时拿到两个，但IMEI或者MAC地址有效，则利用其中有效的一个生成DeviceID
* 如果IMEI与MAC都为空，则取序列号的MD5作为DeviceID
* 如果序列号也是空则取AndroidID作为DeviceID（Android设备都会有）
* 以上都无效UUID随机生成

![客户端DeviceId生成流程图](http://upload-images.jianshu.io/upload_images/1460468-192120fa68a8bd82.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


生成的DeviceId作为服务端的一个主键，由于旧设备已经有了DeviceId，并存到文件中，目前不能直接删掉与替换。

## 客户端更新时机

每次App启动的时候都去请求，这里有两个问题

*   第一 是否会对服务器造成压力 
*  第二 客户端trustid的更新时机


# 服务器部分

## trustId生成规则

DEVICE_ID作为主键，如果APP端传来的DEVICE_ID不同，则需要添加一条新的记录到数据库，但是一条trustId可以对应多条记录，如果DEVICE_ID不同，但是其他的硬件信息与已有的记录能够匹配成功，则映射为同一个trustId。

## 鉴定是否是同以台设备 （trustId的映射 ）

根据不同硬件信息字段来进行积分，累积分值超过10分看做同一台设备。以目前3.3版本的数据作为参考，**积分策略需要根据后端统计数据不断优化，字段的分值需要微调--增加与减少**，目前线上的积分策略：

*     DEVICE_ID   5分   
*     ANDROID_ID  5分   
*     MAC_ADDRESS 5分 （非 02：00：00：00：00：00）
*     IMEI        5分 调整到4分（非 0000000000000000）
*     SERIAL      5分  调整到8分（看目前数据，考虑升分，重复率低）
*     SIMULATOR     0分
*     MANUFACTURER  1分  （考虑减低为0 参考价值低）
*     BRAND   1分（考虑减低为0参考价值低）
*     MODEL   1分（保持手机型号为1）
*     CPU_ABI 1分（考虑减低为0 参考价值低）
*     DEVICE  1分（考虑减低为0 参考价值低）
*     BOARD   1分（考虑减低为0 参考价值低）
*     HARDWARE  1分（考虑减低为0 参考价值低）
*     MEM_INFO  1分 调整到2分（考虑增加分值 手机的内存精确到K，同一种型号，有的也会存在一定差距）
*     IMSI      1分 调整到2分（考虑增加分值 sim卡相同，同一台手机的概率增加）
    
## 版本问题

是否需要为trustId设置版本控制，将来deviceId是否会废弃，是否统一用trustId来处理，比如某版本，trustId生成策略改了，需要客户端统一升级trustId等。

# 后续，持续完善中