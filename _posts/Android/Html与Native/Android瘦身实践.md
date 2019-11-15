5.0.3版本32.3M

![image.png](https://upload-images.jianshu.io/upload_images/1460468-76848f0bb5910979.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

>  对比3.5版本  20M 

![image.png](https://upload-images.jianshu.io/upload_images/1460468-cbdc7edd69361b61.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


相比之下 lib库大了6M，res大了0.6 asset大了0.2，代码大了4.2（有我们的代码也有第三方库引入的代码），这些是增大的主要原因，总体看，基本是第三方SDK的原因导致我们的包变大，其实主要是weex跟网易支付


##  什么占得多细分

*  lib库： 8.2M

![image.png](https://upload-images.jianshu.io/upload_images/1460468-f391bbce9a65039f.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

> 各个lib大小（已经是压缩后）
 * libweexjss.so 			3M 
* libstidocr_stream.so   1.2M   
* libstidinteractive_liveness.so  1.1M
* libfacial_action.so  664.6k
* libc++_shared.so    277.7k
* libuptsmaddon.so     267k
* libuptsmaddonmi.so     267k 
* libcpp-share-id.so    235K 
* libnetsecsdk-3.2.9.so   177.8k 
* libweexjsc.so				164.2 
* libstatic-webp.so	162k
* libCtaApiLib.so   136k
* libimagepipeline.so  130k
* libcrashlytics.so   97k
* libentryexpro.so  60k
* libgifimage.so  48k 
* lib37CF018B.so   41k
* libmmkv.so        30k

> assets资源（已经是压缩后）5.2M


![image.png](https://upload-images.jianshu.io/upload_images/1460468-452623c4fdbabc3f.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

* unicorn_emoji 表情包  云商服    1.1M 
* SenseID_Ocr_Idcard.model"     支付类的模型   1m
* SenseID_Ocr_Bankcard.model"    支付类的模型  990k
* location.json"                 地址库 518k
* M_Align_occlusion.model"			模型430k
* M_Detect_Hunter_SmallFace.model"  模型420k
* M_Liveness_Cnn_half.model"    模型160k
* data.bin"          sdk内置
* yxskin"（内置皮肤包，可删） 89K
* weex/"（内置weex备份 可删） 70k
 

> res资源  5.4M
 
 
![image.png](https://upload-images.jianshu.io/upload_images/1460468-d374da7c08e333f3.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

2X图可以优化掉 、无用图可以清理掉，drawable -

![image.png](https://upload-images.jianshu.io/upload_images/1460468-5f64e39396aff183.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

raw文件是一些原始的 如推送声音等文件，除非压缩原资源，否则也没什么压缩空间

> 代码：9.5
 
也是伴随第三方sdk及业务增加而增加。

 ![image.png](https://upload-images.jianshu.io/upload_images/1460468-881f4fad351b6cd2.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)



## 优化手段

*  取消2X图  32326246 ->  31308568  减小0.97M 
*  删除无用资源   Analyze -> Run Inspection by Name -> unused resources ->    31308568 ---> 31188400 减小100k
*  语言zh  已处理  
*  wep  已经做了资源压缩
*  so库 v7 +压缩  已经是这么处理的
*  图片压缩 已经做了处理
 
**优化后缩减 1.1M**