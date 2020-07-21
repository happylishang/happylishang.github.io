# 前言


#### 国内的Android推送就是个悲剧

国内Android缺少Google的生态，如Google的Paly Store，Google Mobile Services（GSM）等，导致衍生出很多畸形的产业，比如五花八门的APP市场，光怪陆离的推送平台，这里要说的是推送平台。Google本身的GSM服务是包含一套推送在里面的，跟iOS系统的推送类似，它保证每台手机维护一个推送通道就能收到各方推送，但由于Google没法进入中国市场，国产Android基本上算被阉割了一个核心部件，由此衍生的种种弊端数不胜数，首当其冲的就是推送。
  
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

简单看下各ROM计入注意事项，只看离线能力，不考虑透传：

## 小米

关于MIPUSH的接入，直接看官方文档即可，没太多问题，需要注意的是，小米有个奇葩的权限设置：**后台弹出界面权限** ，该权限默认是关闭，这个选项可能会影响推送通知的点击行为，小米有两大中点击行为需要考虑，第一种，

### 完全自定义点击行为

在这种行为下，开发者可以拦截默认点击行为，自定义如何处理后续事件，点击通知后，封装消息MiPushMessage通过PushMessageReceiver继承类的onNotificationMessageClicked方法传到APP进程，开发者可自行处理，如果想要启动界面，只需要在其中调用context.startActivity方法即可，**但是**，这种自定义的行为会受到**后台弹出界面权限**的影响，尤其是高版本的MIUI ROM中。

![](https://user-gold-cdn.xitu.io/2020/7/21/173712a7a3ac2fdf?w=642&h=320&f=png&s=95442)

你会发现，在这些手机上，此方式压根没法拉起APP，除非通过先启动一个Service，然后在Service中拉起，非常像小米的一个BUG，即使通过此下策能拉起，你会发现，拉起速度非常慢，所以这种策略其实可以毙了。

###  预定义点击行为

预定义点击行为不用用户在onNotificationMessageClicked中处理，系统会直接拉起目标页面，小米支持三种预定义点击行为：

* (1) 打开当前的Launcher Activity 
* (2) 打开当前app内的任意一个Activity 
* (3) 打开网页。 

APP一般会采用第二种行为，打开APP任意一个Activity，其实最终会选择一个DeepLink Activity，由其路由到其他界面。服务端调用Message.Builder类的extra(String key, String value)方法，将key设置为Constants.EXTRA_PARAM_NOTIFY_EFFECT，value设置为Constants.NOTIFY_ACTIVITY便可以达到该效果，用户点击了客户端弹出的通知消息后，封装消息的MiPushMessage对象通过Intent传到客户端，客户端可在Activity中解析，并自行处理后续流程。离线推送情况下，推送服务端核心字段如下：

![](https://user-gold-cdn.xitu.io/2020/7/21/17371a4f67c2a118?w=1570&h=388&f=png&s=324407)

采用离线非透传消息，并利用extra自定义Click行为，最后推送给小米的消息格式简化如下：	 
	
	{
		title=通知标题, 
		description=通知内容, 
		restrictedPackageNames=[com.test.example], 
		notifyType=1, 
		notifyId=1249808047, 
		extra.callback=https://test4push.xxx.163.com/push/receipt/third/12/xiaomi,
		<!--打开任意Activity配置-->
		extra.intent_uri=yanxuan://re?opOrderId=crm_a1d05c1d3d1743e192a08b461a376785_20200715,
		extra.notify_effect=2
	}

extra.intent_uri的值就是APP端定义的私有scheme，点击通知会直接拉起相应的DeepLink Activity，从而唤起应用，至于DeepLink Activity最终路由到哪个界面，可以从extra.intent_uri中解析出来。对于上文层说过的click事件不易统计的问题，可以通过在scheme家参数的方式解决，如下：

	extra.intent_uri= yanxuan://re?opOrderId=0200715, 

转为

	extra.intent_uri= yanxuan://re?opOrderId=0200715&platform=xiaomi 

之后在路由Activity中可以解析出platform参数，从而标记click事件及来源平台。预定义行为系统会帮我们处理好唤起，在APP中，不需要在onNotificationMessageClicked再次响应click事件了，避免重复处理，后面各方SDK的能力基本都跟小米类似，没多少花样。

## 华为

接入流程同小米类似，按文档即可，华为的预定义行为有如下四种:

* 1：用户定义Uri点击行为，打开目标界面
* 2：点击后打开特定网页
* 3：点击后打开应用
* 4：点击后打开富媒体信息

华为无法感知离线推送click，一般选择用户自定义Uri点击行为，所有数据必须通过intent uri传输给APP，对应参数意义如下：

![](https://user-gold-cdn.xitu.io/2020/7/21/17371e1f83b68861?w=1648&h=796&f=png&s=144132)

选择type=1 跟 intent uri配合，intent生成格式如下：

	Intent intent = new Intent(Intent.ACTION_VIEW);
	intent.setData(Uri.parse("pushscheme://com.huawei.codelabpush/deeplink?name=abc&age=180"));
	String intentUri = intent.toUri(Intent.URI_INTENT_SCHEME);

最终通过API发送给华为push平台数据格式简化如下：
	
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

跟小米类似，可以将推送平台的参数塞入到scheme，不再敖述。

# 魅族

魅族推送类似，也支持四种预定义行为：

* 打开应用主页
* 打开应用内页面
*  打开URI页面
* 客户端自定义

同样建议选择预定义Uri页面，具体参数如下

![](https://user-gold-cdn.xitu.io/2020/7/21/17371e99c0b483fd?w=1736&h=952&f=png&s=181086)

最终发送数据格式简化如下：

	 {
		 noticeBarType = 0,
		 title = 'meizu明天之后⏰恢复原价', 
		 content = '店庆爆款返场！乳胶床垫直降500，拉杆箱仅7折！😱每满150减25消费券全品类通用，最后1天>>',
		   clickType = 2, 
		   url = 'yanxuan://yxwebview?url=https%3A%2F%2Fact.you.163.com%2Fact%2Fpub%2FDisjY2u1n9p4SB3.html%3Fanchor%3DSeen3xcj%26opOrderId%3Dcrm_task_20200414160053263_1'
		}
	}

clickType = 2 配合Uri Schema来实现，拉起对应界面。

## oppo
 
接入与上面类似，同时，oppo无法感知click事件，它支持五种预定义行为（有冗余）：
  
* 0，启动应用；
* 1，打开应用内页（activity的intent action） 
* 2，打开网页；
* 4，打开应用内页（利用activity全名） 
* 5, Intent scheme URL  

处理类似，这里选click_action_type选择5，可以通过通过click_action_activity中加scheme参数来实现， 具体数据格式如下
 
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

Vivo跟oppo很类似，不过它也可以收到click事件（并没什么卵用），其click动作也支持多种表现：

* 1：打开APP首页
* 2：打开链接
* 3：自定义
* 4：打开app内指定页面

同样，为了防止禁止后台启动，不采用自定义的方式，而直接打开打开app内指定页面， "skipType":4,

	{
	    "classification":1,
	    "content":"adssdsr345436",
	    "notifyType":1,
	    "pushMode":1,
	    "regId":"15905547110541891320627",
	    "requestId":"a467798011733344256",
	    "skipContent":"yanxuan://yxwebview?url=https%3A%2F%2Fact.you.163.com%2Fact%2Fpub%2FDisjY2u1n9p4SB3.html%3Fanchor%3DSeen3xcj%26opOrderId%3Dcrm_task_20200414160053263_1",
	    "skipType":4,
	    "title":"adssdsr345436"
	}


以上是几种离线推送的接入方式，整体总结就是：

* 选择**预定义**方式，不要采用**自定义**的方式
* 可以通过scheme中加参数的方式，统一鉴别click事件
* 不要自行处理click事件，在预定义的方式下，没有任何意义
* 如果只要离线推送功能，没必要处理透传配置

# 总结