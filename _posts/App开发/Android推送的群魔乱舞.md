# 前言

#### 国内的Android推送就是个悲剧

国内的Android缺少Google 的生态，如Google的Paly Store，Google Mobile Services（GSM）等，导致衍生出很多畸形的产业，比如五花八门的APP市场，光怪陆离的推送平台，这里要说的是推送平台。Google本身的GSM服务是包含一套推送在里面的，跟iOS系统的推送类似，它保证每台手机维护一个推送通道就能收到各方推送，但由于Google没法进入中国市场，国产Android基本上算被阉割了一个核心部件，由此衍生的种种弊端数不胜数，首当其冲的就是推送。

国内的手机厂商基本都有自家的推送服务，来替代GSM的缺失，性能、用法参差不齐。**在离线场景**下（APP死亡），如果想要收到推送，就必须接入对应厂家的推送服务，否则压根收不到。所以Android APP在诞生之初基本就要集成华为push、小米push、魅族push、oppo push、Vivo push等，相对GSM，复杂且没有增益，就好比用江南七怪代替了黄老邪，难用的一B。然而，你别无选择。不过国内各种厂商倒是乐此不疲，他们多了一个触达用户及统计的渠道，并且还能不受Google挟制，对于开发者而言，就要麻烦很多，工作量平白翻了很多倍；有的聊天APP为了走自家的推送SDK，还要琢磨各种黑科技：包活，APP相互唤起等，恶之花，开的漫山遍野。更有意思的是，为了解决这种问题，制定出规范，还促生个各种机构，像推送联盟，绿色联盟等，但并没什么卵用，成立3年，乱象依旧，很多说Android很垃圾，那推送的这个问题要负一大半责任。

吐槽完，你仍然要接。

# 推送概念

为什么一定要接厂商的推送SDK呢？不接入收不到推送吗？想要弄清这个东西，就要对推送有个简单的了解，推送：它的点在**推（push）**上，与其对应的是拉（Pull），核心就是客户端跟服务器建立一个长链接，服务器会将信息分发到各个客户端，简化示意如下：

![](https://user-gold-cdn.xitu.io/2020/7/21/1737065cc4f37326?w=549&h=269&f=png&s=23529)

对于手机端APP来说，推送分APP在线推送还是离线推送，其实就是APP是否存活，APP存活情况下，有多种选择，如果APP通过Socket跟自家服务器建立了链接，则可以由自家服务器直接推送到APP端，也可以通过后端推送到第三方推送服务，借由第三方推送给APP端，也就是在线情况下，可以不用接入第三方SDK。但是在APP死亡的情况，只有一种方式：借由第三方推送服务，推送给手机端，这种场景，APP必须接入第三方厂商SDK，拿华为平台为例，其推送模型如下：

![华为消息回执模式](https://communityfile-drcn.op.hicloud.com/FileServer/getFile/cmtyPub/011/111/111/0000000000011111111.20191209145826.18881910394678423178289091037441:50510604043059:2800:5F8AF1B8CC7A165514C25424F6044946223BC929FFEA69304BC00BB7C269AF96.gif?needInitFileName=true?needInitFileName=true)

与两者对应也有两种消息的概念：透传消息与通知栏消息：

* 透传消息：APP存活情况下，由推送服务直接把消息发送给APP应用，由APP自己选择如何处理，注意透**传的前提是APP存活** ，透传消息可以不用接入第三方SDK。

* 通知栏消息：在设备接收到消息之后，由系统弹出标准安卓通知，用户点击通知栏才激活应用，这种场景，APP 无需存活（活着也不受影响），离线场景下，只有通知栏消息这一条路。

对于在线消息，APP存活，APP端可以统计到所有需要的信息，如论是推送达到记录，推送内容还是点击，但是对于离线就没那么简单了，同怎么接入，怎么发消息相比，业务方会更加关心到达率、点击率这些数据，下面看一下如何统计这些数据。

# 推送统计问题 （离线推送）

### 如何到达率

这里不考虑在线推送，只考虑离线（APP死亡），那么离线推送APP能统计到达吗？

答案是 **不能**，原因其实很简单，APP进程都死了，怎么统计。这种情况下，通知的展示属于系统行为，APP压根无法感知，更无从统计。不过，各三方推送服务平台扔提供了推送到达统计的能力，即采用三方推送平台的回执，以上面的华为推送模型为例：

![华为消息回执模式](https://communityfile-drcn.op.hicloud.com/FileServer/getFile/cmtyPub/011/111/111/0000000000011111111.20191209145826.18881910394678423178289091037441:50510604043059:2800:5F8AF1B8CC7A165514C25424F6044946223BC929FFEA69304BC00BB7C269AF96.gif?needInitFileName=true?needInitFileName=true)

可以看到，离线推送的情况下，华为设备在展示完通知栏消息后，会给华为Push服务一个回执，而华为Push服务会把这个回执头传给开发者服务器，如此，APP服务端就能判断推送是否到达。

### 如何统计点击率

同样，在离线推送的场景下，能统计到点击事件吗？关于这个场景，不同的厂商ROM及SDK真是乱七八糟，有的支持，有的不行，简单整理下如下：

ROM      | 小米             |    华为          |    魅族     |   oppo      |    vivo
---|-----| -------| ----| ----| -----| 
App是否可以统计到离线点击事件 | 是 | 否 |  是|否|是
 
因此，各方平台给的方式并没太多参考意义，必须通过其他方式来统计点击，离线推送基本都是通过scheme方式来处理，可以通过加参数来搞定，后续详述。
 
 
推送送达率=本次推送真正送达的设备数/所覆盖的所有设备数（按理说，是应该清理掉无效设备）


### 哪些因素影响送达率

* 1)  留存率。已经卸载了APP，肯定收不到，但是有些三方平台可能会归结到分母中，需要自家后台根据回执手动清理regID。
* 2) 消息有效期，基本所有第三方PUSH平台都支持设置有效期，有效期越短，触达设备就越少，送达率会下降，可以适当选择有效时间。
* 3) 联网情况， 在有效期内，设备没联网，也无法送达，但会被计入分母
* 4) 目标人群设备的选取，活跃人群设备送达率肯定要高于全量推送

因此为了能精准的计算送达率，APP服务端要定期清理无效regID（推送token），否则统计的送达率也会偏低

 
#  各离线推送平台接入事项

很多大公司都有自家的推送SDK来处理透传消息，小公司一般不具备这个能力，所以在接入Push的时候也分两种情况，

* 1：有自己加的PushSDK，
* 2：没有自家PushSDK

如果APP有自己的PushSDK，那只要接入第三方离线推送能力就好了，一些关于透传的处理配置可以完全不用关心，用自己PushSDK那套就可以。如果没有自家PushSDK，那就需要选择一个SDK进行透传处理，当然，仍要接入第三方离线推送能力。不过即使如此，各家ROM的接入规则也个不相同，比如小米有个奇葩的权限叫：“后台弹出界面权限 ”，如果后端服务Push姿势不对，可能会引入奇葩问题：比如，手机能收到PUSH，但是拉不起界面，坑爹。

下面简单看下各ROM计入注意事项，先只看离线能力，不考虑透传：

## 小米接入注意事项 

关于MIPUSH的接入，直接看官方文档即可，没太多问题，需要注意的是，小米有个奇葩的权限设置：**后台弹出界面权限**  ，该权限默认是关闭，这个选项可能会影响推送通知的打开。

![](https://user-gold-cdn.xitu.io/2020/7/21/173712a7a3ac2fdf?w=642&h=320&f=png&s=95442)



	    @Override
	    public void onNotificationMessageClicked(final Context context, final MiPushMessage message) {
    
消息格式			 
				 
		{
		    "ack":"true",
		    "alert":"店庆爆款返场！乳胶床垫直降500，拉杆箱仅7折！😱每满150减25消费券全品类通用，最后1天>>",
		    "alert2":"{\"subtitle\":\"\",\"title\":\"明天之后⏰恢复原价\",\"body\":\"店庆爆款返场！乳胶床垫直降500，拉杆箱仅7折！😱每满150减25消费券全品类通用，最后1天>>\"}",
		    "appid":"12",
		    "badge":0,
		    "badgeMode":0,
		    "batch":"crm_task_20200414160053263_1",
		    "broadcast":false,
		    "mkCPayload":false,
		    "mutableContent":"1",
		    "now":0,
		    "payload":"{\"id\":0,\"imageUrls\":[],\"schemeUrl\":\"yanxuan://yxwebview?url=https%3A%2F%2Fact.you.163.com%2Fact%2Fpub%2FDisjY2u1n9p4SB3.html%3Fanchor%3DSeen3xcj%26opOrderId%3Dcrm_task_20200414160053263_1\",\"title\":\"明天之后⏰恢复原价\",\"type\":8}",
		    "sound":"default",
		    "subtype":"yanxuan",
		    "total":1000,
		    "uid":"12#hbyxtest52@163.com",
		"pushChannel":"mi",
		    "uid_type":"0"
		}
				 
				 
*  在线消息 通过一个Service通知可以统一处理
*  离线，就用schemeUrl这种



Android 9之后禁止后台启动，并且想小米之类的ROM是禁止后台启动的，因此离线推送尽量采用，打开应用内特定页面的方式来实现，一般可以理解为Scheme URL，或者说Intent URi，已避免受到通知，但是无法唤起界面的尴尬。


## 华为推送


* 推送数据格式：

华为离线推送无法感知click，所有数据通过intent uri传输给APP，因此其唤起类型是
	
		{
		    "hps":{
		        "msg":{
		            "action":{
		                "param":{
		                    "intent":"intent://member?url=http%3A%2F%2Fm.you.163.com%2Fmembership%2Findex&_yanxuan_hwpush=1&_mid=a397314518947995648#Intent;scheme=yanxuan;launchFlags=0x4000000;end"
		                },
		                "type":1
		            },
		            "type":3,
		            "body":{
		                "title":"huawei免邮券礼包",
		                "content":"快来领取你的每月专属免运费券，立即领取>>"
		            }
		        },
		    }
		}




# 魅族

推送不要夹杂太多用户信息，只保留必要字段即可

魅族uri定义更像是schema


	 {
		noticeBarType = 0,
		 title = 'meizu明天之后⏰恢复原价', 
		 content = '店庆爆款返场！乳胶床垫直降500，拉杆箱仅7折！😱每满150减25消费券全品类通用，最后1天>>',
		  noticeExpandType = 0, 
		  noticeExpandContent = '',
		   clickType = 2, 
		   url = 'yanxuan://yxwebview?url=https%3A%2F%2Fact.you.163.com%2Fact%2Fpub%2FDisjY2u1n9p4SB3.html%3Fanchor%3DSeen3xcj%26opOrderId%3Dcrm_task_20200414160053263_1', parameters = null, activity = '', customAttribute = '{"uid":"12#hbyxtest52@163.com","uid_type":"0","alert":"店庆爆款返场！乳胶床垫直降500，拉杆箱仅7折！😱每满150减25消费券全品类通用，最后1天>>","payload":{"body":"{\"id\":0,\"title\":\"明天之后⏰恢复原价\",\"schemeUrl\":\"yanxuan://yxwebview?url=https%3A%2F%2Fact.you.163.com%2Fact%2Fpub%2FDisjY2u1n9p4SB3.html%3Fanchor%3DSeen3xcj%26opOrderId%3Dcrm_task_20200414160053263_1\",\"imageUrls\":[],\"type\":8}"},"appid":"12","os_type":"android","ack":"true","mid":"a468132600632836096","msg_type":"yanxuan"}', isOffLine = true, validTime = 24, pushTimeType = 0, startTime = null, isFixSpeed = false, fixSpeedRate = 0, isSuspend = true, isClearNoticeBar = true, isFixDisplay = false, fixStartDisplayDate = null, fixEndDisplayDate = null, vibrate = true, lights = true, sound = true, notifyKey = , extra = {
			callback.type = 3
		}
	}



## oppo
 
  离线推送支持选择点击后面的表现，不过oppo同样无法感知click事件
  
 * 0，启动应用；
* 1，打开应用内页（activity的intent action）； ===< action android:name="com.coloros.push.demo.internal" />
* 2，打开网页；
* 4，打开应用内页（activity）；【非必填，默认值为0】;  ------com.coloros.push.demo.component.InternalActivity
* 5 ,Intent scheme URL       -----command://test?key1=val1&key2=val2

为了避免无法启动的问题，这里选click_action_type选择5，如果想要知道推送的 一些标识，需要通过click_action_activity中加scheme参数来实现， 具体数据格式如下
 
		{
		    "notification":{
		        "app_message_id":"a467789237882716160",
		        "channel_id":"yanxuan_notification_channel",
		        "click_action_activity":"yanxuan://yxwebview?url=https%3A%2F%2Fact.you.163.com%2Fact%2Fpub%2FDisjY2u1n9p4SB3.html%3Fanchor%3DSeen3xcj%26opOrderId%3Dcrm_task_20200414160053263_1",
		        "click_action_type":5,
		        "content":"明天之后恢复原价",
		        "title":"明天之后恢复原价"
		    },
		    "target_type":2,
		    "target_value":"CN_29c9ed3771b470e24138944b373a2f22"
		}
		
		
## vivo

Vivo跟oppo很类似，不过它可以收到click事件，同样，其click动作也支持多种表现：

* 1：打开APP首页
* 2：打开链接
* 3：自定义
* 4：打开app内指定页面

为了防止禁止后台启动，我们不采用自定义的方式，而知直接定制好动作，这样也能加快启动动作 ： "skipType":4,

	{
	    "classification":1,
	    "content":"adssdsr345436",
	    "extra":{
	        "callback":"https://test4push.you.163.com/push/receipt/third/12/vivo",
	        "callback.param":"data"
	    },
	    "notifyType":1,
	    "pushMode":1,
	    "regId":"15905547110541891320627",
	    "requestId":"a467798011733344256",
	    "skipContent":"yanxuan://yxwebview?url=https%3A%2F%2Fact.you.163.com%2Fact%2Fpub%2FDisjY2u1n9p4SB3.html%3Fanchor%3DSeen3xcj%26opOrderId%3Dcrm_task_20200414160053263_1",
	    "skipType":4,
	    "title":"adssdsr345436"
	}


#  目前线上问题 (目前基本都已解决或者有方案解决)

* 小米离线拉不起来，**更改click唤起类型解决**
* click点击重复，不在额外统计，采用统一的统计类型
* 国内推送的回执，成功才有，不然没有
 
离线推送统一采用schema方式，在线推送，透传，无所谓。心跳如何保证。



# 到达率

推送送达率=本次推送真正送达的设备数/所覆盖的所有设备数（按理说，是应该清理掉无效设备）


#### 有哪些影响送达率的因素？

* 1) 应用的留存率。已经卸载了app的设备，肯定是推送不到的，按照目前的计算方式，大部分的卸载设备会被计入分母（计划推送数）当中。
* 2) 应用所在设备的联网情况。如果在消息有效期内，设备一直不联网，那消息也是不能送达的，但也会被计入分母当中。
* 3) 消息的有效期。有效期越短，在有效期内联网的设备数势必就越少，因此送达率会随之下降。
* 4) 目标设备的选取。如果选取的是全量用户，那其送达率肯定会比按照用户联网情况精准提取目标设备（如选取7天内有过打开应用行为的用户）要低。


### 2.关于regID

* 2.1. regID是根据什么生成的？

regID一般是客户端向推送服务注册时，推送服务端根据设备标识、appID以及当前时间戳生成，因此能够保证每个设备上每个app对应的regID都是不同的。

* 2.2. regID会不会变化？

当app注册成功后，小米推送服务客户端SDK会在本地通过shared_prefs来保存这个regID，之后app调用注册，SDK后会在本地直接读取出这个regID并直接返回，不会重新请求服务器。因此只要应用不卸载重装或者清除应用本地数据，regID就不会变化。否则，如果SDK没有从本地读取到缓存的regID，则会向服务端重新请求，此时regID会重新生成。

* 2.3. regID在哪些情况下会失效？

1) app卸载重装或者清除数据后重新注册，这种情况下会生成一个新的regID，而老的regID会失效；
2) app调用了unregisterPush；
3) app卸载时，如果能成功上报，则regID会被判定失效；
4) 设备超过3个月没有和小米push服务器建立长连接；


# 点击率


点击率 可以统一采用scheme加参数，唤起方式进行统计


# 严选Token问题


严选双token：

* 离线推送所需token（第三方regid）
* 在线推送token（wzp自己生成）
 
如果APP在线，并且推送打开，则走在线token推送，如果离线则走离线regid推送

# 数据传输
