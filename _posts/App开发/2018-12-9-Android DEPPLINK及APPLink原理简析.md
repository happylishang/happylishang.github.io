---

layout: post
title: Android DEPPLINK及APPLink原理简析
category: Android

---


APP开发中经常会有这种需求：在浏览器或者短信中唤起APP，如果安装了就唤起，否则引导下载。对于Android而言，这里主要牵扯的技术就是deeplink，也可以简单看成scheme，Android一直是支持scheme的，但是由于Android的开源特性，不同手机厂商或者不同浏览器厂家处理的千奇百怪，有些能拉起，有些不行，本文只简单分析下link的原理，包括deeplink，也包括Android6.0之后的AppLink。**其实个人认为，AppLink可以就是deeplink，只不过它多了一种类似于验证机制，如果验证通过，就设置默认打开，如果验证不过，则退化为deeplink**，如果单从APP端来看，区别主要在Manifest文件中的android:autoVerify="true"，如下，


**APPLINK只是在安装时候多了一个验证，其他跟之前deeplink一样，如果没联网，验证失败，那就跟之前的deeplink表现一样**

> deeplink配置（不限http/https）

    <intent-filter>
        <data android:scheme="https" android:host="test.example.com"  />
        <category android:name="android.intent.category.DEFAULT" />
        <action android:name="android.intent.action.VIEW" />
        <category android:name="android.intent.category.BROWSABLE" />
    </intent-filter>

	 （不限http/https）
     <intent-filter>
		    <data android:scheme="example" />
		    <!-- 下面这几行也必须得设置 -->
		    <category android:name="android.intent.category.DEFAULT" />
		    <action android:name="android.intent.action.VIEW" />
		    <category android:name="android.intent.category.BROWSABLE" />
    </intent-filter>
            
            
> applink配置（只能http/https）
    
    <intent-filter android:autoVerify="true">
        <data android:scheme="https" android:host="test.example.com"  />
        <category android:name="android.intent.category.DEFAULT" />
        <action android:name="android.intent.action.VIEW" />
        <category android:name="android.intent.category.BROWSABLE" />
    </intent-filter>
            
在Android原生的APPLink实现中，需要APP跟服务端双向验证才能让APPLink生效，如果如果APPLink验证失败，APPLink会完全退化成deepLink，这也是为什么说APPLINK是一种特殊的deepLink，所以先分析下deepLink，deepLink理解了，APPLink就很容易理解。

# deepLink原理分析

deeplink的scheme相应分两种：一种是只有一个APP能相应，另一种是有多个APP可以相应，比如，如果为一个APP的Activity配置了http scheme类型的deepLink，如果通过短信或者其他方式唤起这种link的时候，一般会出现一个让用户选择的弹窗，因为一般而言，系统会带个浏览器，也相应这类scheme，比如下面的例子：

	>adb shell am start -a android.intent.action.VIEW   -c android.intent.category.BROWSABLE  -d "https://test.example.com/b/g"

    <intent-filter>
        <data android:scheme="https" android:host="test.example.com"  />
        <category android:name="android.intent.category.DEFAULT" />
        <action android:name="android.intent.action.VIEW" />
        <category android:name="android.intent.category.BROWSABLE" />
    </intent-filter>
    
![image.png](https://upload-images.jianshu.io/upload_images/1460468-91770dac931bbb36.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

如果是设置了一个私用的，并且没有跟其他app重复的，那么会直接打开，比如下面的：

	>adb shell am start -a android.intent.action.VIEW   -c android.intent.category.BROWSABLE  -d "example://test.example.com/b/g"
	
     <intent-filter>
		    <data android:scheme="example" />
		    <!-- 下面这几行也必须得设置 -->
		    <category android:name="android.intent.category.DEFAULT" />
		    <action android:name="android.intent.action.VIEW" />
		    <category android:name="android.intent.category.BROWSABLE" />
    </intent-filter>

当然，如果私有scheme跟其他APP的重复了，还是会唤起APP选择界面（其实是一个ResolverActivity）。下面就来看看scheme是如何匹配并拉起对应APP的。

## startActivity入口与ResolverActivity

无论APPLink跟DeepLink其实都是通过唤起一个Activity来实现界面的跳转，无论从APP外部：比如短信、浏览器，还是APP内部。通过在APP内部模拟跳转来看看具体实现，写一个H5界面，然后通过Webview加载，不过Webview不进行任何设置，这样跳转就需要系统进行解析，走deeplink这一套：	
	
	<html>
	<body> 
		<a href="https://test.example.com/a/g">Scheme跳转</a>
	</body>
	</html>
           
点击Scheme跳转，一般会唤起如下界面，让用户选择打开方式：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-91770dac931bbb36.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

如果通过adb打印log，你会发现**ActivityManagerService**会打印这样一条Log：

	> 12-04 20:32:04.367   887  9064 I ActivityManager: START u0 {act=android.intent.action.VIEW dat=https://test.example.com/... cmp=android/com.android.internal.app.ResolverActivity (has extras)} from uid 10067 on display 0

其实看到的选择对话框就是ResolverActivity，不过我们先来看看到底是走到ResolverActivity的，也就是这个scheme怎么会唤起App选择界面，在短信中，或者Webview中遇到scheme，他们一般会发出相应的Intent（当然第三方APP可能会屏蔽掉，比如微信就换不起APP），其实上面的作用跟下面的代码结果一样：
	
	    val intent = Intent()
	    intent.setAction("android.intent.action.VIEW")
	    intent.setData(Uri.parse("https://test.example.com/a/g"))
	    intent.addCategory("android.intent.category.DEFAULT")
	    intent.addCategory("android.intent.category.BROWSABLE")
	    startActivity(intent)
	    
那剩下的就是看startActivity，在6.0的源码中，startActivity最后会通过ActivityManagerService调用ActivityStatckSupervisor的startActivityMayWait
 
>  ActivityStatckSUpervisor
 
	 final int startActivityMayWait(IApplicationThread caller, int callingUid, String callingPackage, Intent intent, String resolvedType, IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor, IBinder resultTo, String resultWho, int requestCode, int startFlags, ProfilerInfo profilerInfo, WaitResult outResult, Configuration config, Bundle options, boolean ignoreTargetSecurity, int userId, IActivityContainer iContainer, TaskRecord inTask) {
	    ...
	    boolean componentSpecified = intent.getComponent() != null;
	    //创建新的Intent对象，即便intent被修改也不受影响
	    intent = new Intent(intent);
		 //收集Intent所指向的Activity信息, 当存在多个可供选择的Activity,则直接向用户弹出resolveActivity [见2.7.1]
	    ActivityInfo aInfo = resolveActivity(intent, resolvedType, startFlags, profilerInfo, userId);
	    ...
	    
	    }
	    
startActivityMayWait会通过**resolveActivity**先找到目标Activity，这个过程中，可能找到多个匹配的Activity，这就是ResolverActivity的入口：
  
      ActivityInfo resolveActivity(Intent intent, String resolvedType, int startFlags,
            ProfilerInfo profilerInfo, int userId) {
        // Collect information about the target of the Intent.
        ActivityInfo aInfo;
        try {
            ResolveInfo rInfo =
                AppGlobals.getPackageManager().resolveIntent(
                        intent, resolvedType,
                        PackageManager.MATCH_DEFAULT_ONLY
                                    | ActivityManagerService.STOCK_PM_FLAGS, userId);
            aInfo = rInfo != null ? rInfo.activityInfo : null;
        } catch (RemoteException e) {
            aInfo = null;
        }
        
   
可以认为，所有的四大组件的信息都在PackageManagerService中有登记，想要找到这些类，就必须向PackagemanagerService查询，

> PackageManagerService

    @Override
    public ResolveInfo resolveIntent(Intent intent, String resolvedType,
            int flags, int userId) {
        if (!sUserManager.exists(userId)) return null;
        enforceCrossUserPermission(Binder.getCallingUid(), userId, false, false, "resolve intent");
        List<ResolveInfo> query = queryIntentActivities(intent, resolvedType, flags, userId);
        return chooseBestActivity(intent, resolvedType, flags, query, userId);
    }

PackageManagerService会通过queryIntentActivities找到所有适合的Activity，再通过chooseBestActivity提供选择的权利。这里分如下三种情况：

* 仅仅找到一个，直接启动
* 找到了多个，并且设置了其中一个为默认启动，则直接启动相应Acitivity
* **找到了多个，切没有设置默认启动，则启动ResolveActivity供用户选择**

关于如何查询，匹配的这里不详述，仅仅简单看看如何唤起选择页面，或者默认打开，比较关键的就是chooseBestActivity，
    
    private ResolveInfo chooseBestActivity(Intent intent, String resolvedType,
            int flags, List<ResolveInfo> query, int userId) {
        		 <!--查询最好的Activity-->
                ResolveInfo ri = findPreferredActivity(intent, resolvedType,
                        flags, query, r0.priority, true, false, debug, userId);
                if (ri != null) {
                    return ri;
                }
                ...
    }
            
        ResolveInfo findPreferredActivity(Intent intent, String resolvedType, int flags,
            List<ResolveInfo> query, int priority, boolean always,
            boolean removeMatches, boolean debug, int userId) {
        if (!sUserManager.exists(userId)) return null;
        // writer
        synchronized (mPackages) {
            if (intent.getSelector() != null) {
                intent = intent.getSelector();
            }
             
            <!--如果用户已经选择过默认打开的APP，则这里返回的就是相对应APP中的Activity-->
            ResolveInfo pri = findPersistentPreferredActivityLP(intent, resolvedType, flags, query,
                    debug, userId);
            if (pri != null) {
                return pri;
            }
			<!--找Activity-->
            PreferredIntentResolver pir = mSettings.mPreferredActivities.get(userId);
            ...
                        final ActivityInfo ai = getActivityInfo(pa.mPref.mComponent,
                                flags | PackageManager.GET_DISABLED_COMPONENTS, userId);
            ...
    }


    @Override
    public ActivityInfo getActivityInfo(ComponentName component, int flags, int userId) {
        if (!sUserManager.exists(userId)) return null;
        enforceCrossUserPermission(Binder.getCallingUid(), userId, false, false, "get activity info");
        synchronized (mPackages) {
            ...
            <!--弄一个ResolveActivity的ActivityInfo-->
            if (mResolveComponentName.equals(component)) {
                return PackageParser.generateActivityInfo(mResolveActivity, flags,
                        new PackageUserState(), userId);
            }
        }
        return null;
    }
  
其实上述流程比较复杂，这里只是自己简单猜想下流程，找到目标Activity后，无论是真的目标Acitiviy，还是ResolveActivity，都会通过startActivityLocked继续走启动流程，这里就会看到之前打印的Log信息：

> ActivityStatckSUpervisor

    final int startActivityLocked(IApplicationThread caller...{
        if (err == ActivityManager.START_SUCCESS) {
            Slog.i(TAG, "START u" + userId + " {" + intent.toShortString(true, true, true, false)
                    + "} from uid " + callingUid
                    + " on display " + (container == null ? (mFocusedStack == null ?
                            Display.DEFAULT_DISPLAY : mFocusedStack.mDisplayId) :
                            (container.mActivityDisplay == null ? Display.DEFAULT_DISPLAY :
                                    container.mActivityDisplay.mDisplayId)));
        }


如果是ResolveActivity还会根据用户选择的信息将一些设置持久化到本地，这样下次就可以直接启动用户的偏好App。其实以上就是deeplink的原理，说白了一句话：**scheme就是隐式启动Activity，如果能找到唯一或者设置的目标Acitivity则直接启动，如果找到多个，则提供APP选择界面。**        
 
# AppLink原理

一般而言，每个APP都希望被自己制定的scheme唤起，这就是Applink，之前分析deeplink的时候提到了ResolveActivity这么一个选择过程，而AppLink就是自动帮用户完成这个选择过程，并且选择的scheme是最适合它的scheme（开发者的角度）。因此对于AppLink要分析的就是如何完成了这个默认选择的过程。

目前Android源码提供的是一个双向认证的方案：**在APP安装的时候，客户端根据APP配置像服务端请求，如果满足条件，scheme跟服务端配置匹配的上，就为APP设置默认启动选项**，所以这个方案很明显，在安装的时候需要联网才行，否则就是完全不会验证，那就是普通的deeplink，既然是在安装的时候去验证，那就看看PackageManagerService是如何处理这个流程的：

> PackageManagerService

    private void installPackageLI(InstallArgs args, PackageInstalledInfo res) {
        final int installFlags = args.installFlags;
        <!--开始验证applink-->
        startIntentFilterVerifications(args.user.getIdentifier(), replace, pkg);
        ...
        
        }

    private void startIntentFilterVerifications(int userId, boolean replacing,
            PackageParser.Package pkg) {
        if (mIntentFilterVerifierComponent == null) {
            return;
        }

        final int verifierUid = getPackageUid(
                mIntentFilterVerifierComponent.getPackageName(),
                (userId == UserHandle.USER_ALL) ? UserHandle.USER_OWNER : userId);

        mHandler.removeMessages(START_INTENT_FILTER_VERIFICATIONS);
        final Message msg = mHandler.obtainMessage(START_INTENT_FILTER_VERIFICATIONS);
        msg.obj = new IFVerificationParams(pkg, replacing, userId, verifierUid);
        mHandler.sendMessage(msg);
    }
 
 startIntentFilterVerifications发送一个消息开启验证，随后调用verifyIntentFiltersIfNeeded进行验证
    
     private void verifyIntentFiltersIfNeeded(int userId, int verifierUid, boolean replacing,
            PackageParser.Package pkg) {
        	...
            <!--检查是否有Activity设置了AppLink-->
	        final boolean hasDomainURLs = hasDomainURLs(pkg);
	        if (!hasDomainURLs) {
	            if (DEBUG_DOMAIN_VERIFICATION) Slog.d(TAG,
	                    "No domain URLs, so no need to verify any IntentFilter!");
	            return;
	        }
        	<!--是否autoverigy-->
            boolean needToVerify = false;
            for (PackageParser.Activity a : pkg.activities) {
                for (ActivityIntentInfo filter : a.intents) {
                <!--needsVerification是否设置autoverify -->
                    if (filter.needsVerification() && needsNetworkVerificationLPr(filter)) {
                        needToVerify = true;
                        break;
                    }
                }
            }
          <!--如果有搜集需要验证的Activity信息及scheme信息-->
            if (needToVerify) {
                final int verificationId = mIntentFilterVerificationToken++;
                for (PackageParser.Activity a : pkg.activities) {
                    for (ActivityIntentInfo filter : a.intents) {
                        if (filter.handlesWebUris(true) && needsNetworkVerificationLPr(filter)) {
                            if (DEBUG_DOMAIN_VERIFICATION) Slog.d(TAG,
                                    "Verification needed for IntentFilter:" + filter.toString());
                            mIntentFilterVerifier.addOneIntentFilterVerification(
                                    verifierUid, userId, verificationId, filter, packageName);
                            count++;
                        }    }   } }  }
       <!--开始验证-->
        if (count > 0) {
            mIntentFilterVerifier.startVerifications(userId);
        } 
    }
 
可以看出，验证就三步：检查、搜集、验证。在检查阶段，首先看看是否有设置http/https scheme的Activity，并且是否满足设置了Intent.ACTION_DEFAULT与Intent.ACTION_VIEW，如果没有，则压根不需要验证，

     * Check if one of the IntentFilter as both actions DEFAULT / VIEW and a HTTP/HTTPS data URI
     */
    private static boolean hasDomainURLs(Package pkg) {
        if (pkg == null || pkg.activities == null) return false;
        final ArrayList<Activity> activities = pkg.activities;
        final int countActivities = activities.size();
        for (int n=0; n<countActivities; n++) {
            Activity activity = activities.get(n);
            ArrayList<ActivityIntentInfo> filters = activity.intents;
            if (filters == null) continue;
            final int countFilters = filters.size();
            for (int m=0; m<countFilters; m++) {
                ActivityIntentInfo aii = filters.get(m);
                // 必须设置Intent.ACTION_VIEW 必须设置有ACTION_DEFAULT 必须要有SCHEME_HTTPS或者SCHEME_HTTP，查到一个就可以
                if (!aii.hasAction(Intent.ACTION_VIEW)) continue;
                if (!aii.hasAction(Intent.ACTION_DEFAULT)) continue;
                if (aii.hasDataScheme(IntentFilter.SCHEME_HTTP) ||
                        aii.hasDataScheme(IntentFilter.SCHEME_HTTPS)) {
                    return true;
                }
            }
        }
        return false;
    }

检查的第二步试看看是否设置了autoverify，当然中间还有些是否设置过，用户是否选择过的操作，比较复杂，不分析，不过不影响对流程的理解：
   
   <!--检查是否设置了autoverify，并且再次检查是否是http https类-->
   
    public final boolean needsVerification() {
        return getAutoVerify() && handlesWebUris(true);
    }

    public final boolean getAutoVerify() {
        return ((mVerifyState & STATE_VERIFY_AUTO) == STATE_VERIFY_AUTO);
    }
    
只要找到一个满足以上条件的Activity，就开始验证。如果想要开启applink，Manifest中配置必须像下面这样
 
        <intent-filter android:autoVerify="true">
            <data android:scheme="https" android:host="xxx.com" />
            <data android:scheme="http" android:host="xxx.com" />
            <!--外部intent打开，比如短信，文本编辑等-->
            <action android:name="android.intent.action.VIEW" />
            <category android:name="android.intent.category.DEFAULT" />
        </intent-filter>


搜集其实就是搜集intentfilter信息，下面直接看验证过程，

 
    @Override
        public void startVerifications(int userId) {
            ...
                sendVerificationRequest(userId, verificationId, ivs);
            }
            mCurrentIntentFilterVerifications.clear();
        }

        private void sendVerificationRequest(int userId, int verificationId,
                IntentFilterVerificationState ivs) {

            Intent verificationIntent = new Intent(Intent.ACTION_INTENT_FILTER_NEEDS_VERIFICATION);
            verificationIntent.putExtra(
                    PackageManager.EXTRA_INTENT_FILTER_VERIFICATION_ID,
                    verificationId);
            verificationIntent.putExtra(
                    PackageManager.EXTRA_INTENT_FILTER_VERIFICATION_URI_SCHEME,
                    getDefaultScheme());
            verificationIntent.putExtra(
                    PackageManager.EXTRA_INTENT_FILTER_VERIFICATION_HOSTS,
                    ivs.getHostsString());
            verificationIntent.putExtra(
                    PackageManager.EXTRA_INTENT_FILTER_VERIFICATION_PACKAGE_NAME,
                    ivs.getPackageName());
            verificationIntent.setComponent(mIntentFilterVerifierComponent);
            verificationIntent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);

            UserHandle user = new UserHandle(userId);
            mContext.sendBroadcastAsUser(verificationIntent, user);
        }

目前Android的实现是通过发送一个广播来进行验证的，也就是说，这是个异步的过程，验证是需要耗时的（网络请求），所以安装后，一般要等个几秒Applink才能生效，广播的接受处理者是：IntentFilterVerificationReceiver

	
	public final class IntentFilterVerificationReceiver extends BroadcastReceiver {
	    private static final String TAG = IntentFilterVerificationReceiver.class.getSimpleName();
	...
	
	    @Override
	    public void onReceive(Context context, Intent intent) {
	        final String action = intent.getAction();
	        if (Intent.ACTION_INTENT_FILTER_NEEDS_VERIFICATION.equals(action)) {
	            Bundle inputExtras = intent.getExtras();
	            if (inputExtras != null) {
	                Intent serviceIntent = new Intent(context, DirectStatementService.class);
	                serviceIntent.setAction(DirectStatementService.CHECK_ALL_ACTION);
                   ...
	                serviceIntent.putExtras(extras);
	                context.startService(serviceIntent);
	            }

IntentFilterVerificationReceiver收到验证消息后，通过start一个DirectStatementService进行验证，兜兜转转最终调用IsAssociatedCallable的verifyOneSource，


    private class IsAssociatedCallable implements Callable<Void> {

         ...
        private boolean verifyOneSource(AbstractAsset source, AbstractAssetMatcher target,
                Relation relation) throws AssociationServiceException {
            Result statements = mStatementRetriever.retrieveStatements(source);
            for (Statement statement : statements.getStatements()) {
                if (relation.matches(statement.getRelation())
                        && target.matches(statement.getTarget())) {
                    return true;
                }
            }
            return false;
        }
        	        
 IsAssociatedCallable会逐一对需要验证的intentfilter进行验证，具体是通过DirectStatementRetriever的retrieveStatements来实现：
        	        
    @Override
    public Result retrieveStatements(AbstractAsset source) throws AssociationServiceException {
        if (source instanceof AndroidAppAsset) {
            return retrieveFromAndroid((AndroidAppAsset) source);
        } else if (source instanceof WebAsset) {
            return retrieveFromWeb((WebAsset) source);
        } else {
           ..
                   }
    }

AndroidAppAsset好像是Google的另一套assetlink类的东西，好像用在APP web登陆信息共享之类的地方 ，不看，直接看retrieveFromWeb：从名字就能看出，这是获取服务端Applink的配置，获取后跟本地校验，如果通过了，那就是applink启动成功：
    
        
    private Result retrieveStatementFromUrl(String urlString, int maxIncludeLevel,
                                            AbstractAsset source)
            throws AssociationServiceException {
        List<Statement> statements = new ArrayList<Statement>();
        if (maxIncludeLevel < 0) {
            return Result.create(statements, DO_NOT_CACHE_RESULT);
        }

        WebContent webContent;
        try {
            URL url = new URL(urlString);
            if (!source.followInsecureInclude()
                    && !url.getProtocol().toLowerCase().equals("https")) {
                return Result.create(statements, DO_NOT_CACHE_RESULT);
            }
            <!--通过网络请求获取配置-->
            webContent = mUrlFetcher.getWebContentFromUrlWithRetry(url,
                    HTTP_CONTENT_SIZE_LIMIT_IN_BYTES, HTTP_CONNECTION_TIMEOUT_MILLIS,
                    HTTP_CONNECTION_BACKOFF_MILLIS, HTTP_CONNECTION_RETRY);
        } catch (IOException | InterruptedException e) {
            return Result.create(statements, DO_NOT_CACHE_RESULT);
        }
        
        try {
            ParsedStatement result = StatementParser
                    .parseStatementList(webContent.getContent(), source);
            statements.addAll(result.getStatements());
            <!--如果有一对多的情况，或者说设置了“代理”，则循环获取配置-->
            for (String delegate : result.getDelegates()) {
                statements.addAll(
                        retrieveStatementFromUrl(delegate, maxIncludeLevel - 1, source)
                                .getStatements());
            }
            <!--发送结果-->
            return Result.create(statements, webContent.getExpireTimeMillis());
        } catch (JSONException | IOException e) {
            return Result.create(statements, DO_NOT_CACHE_RESULT);
        }
    }
 
 其实就是通过UrlFetcher获取服务端配置，然后发给之前的receiver进行验证:
 
        public WebContent getWebContentFromUrl(URL url, long fileSizeLimit, int connectionTimeoutMillis)
            throws AssociationServiceException, IOException {
        final String scheme = url.getProtocol().toLowerCase(Locale.US);
        if (!scheme.equals("http") && !scheme.equals("https")) {
            throw new IllegalArgumentException("The url protocol should be on http or https.");
        }

        HttpURLConnection connection = null;
        try {
            connection = (HttpURLConnection) url.openConnection();
            connection.setInstanceFollowRedirects(true);
            connection.setConnectTimeout(connectionTimeoutMillis);
            connection.setReadTimeout(connectionTimeoutMillis);
            connection.setUseCaches(true);
            connection.setInstanceFollowRedirects(false);
            connection.addRequestProperty("Cache-Control", "max-stale=60");
			 ...
            return new WebContent(inputStreamToString(
                    connection.getInputStream(), connection.getContentLength(), fileSizeLimit),
                expireTimeMillis);
        } 

看到这里的HttpURLConnection就知道为什么Applink需在安装时联网才有效，到这里其实就可以理解的差不多，后面其实就是针对配置跟App自身的配置进行校验，如果通过就设置默认启动，并持久化，验证成功的话可以通过

	adb shell dumpsys package d   
 
 查看结果:
 
	  Package: com.xxx
	  Domains: xxxx.com
	  Status: always : 200000002
    
验证后再通过PackageManagerService持久化到设置信息，如此就完成了Applink验证流程。    
    
# Chrome浏览器对于自定义scheme的拦截

> A little known feature in Android lets you launch apps directly from a web page via an Android Intent. One scenario is launching an app when the user lands on a page, which you can achieve by embedding an iframe in the page with a custom URI-scheme set as the src, as follows:   <  iframe src="paulsawesomeapp://page1"> </iframe>. This works in the Chrome for Android browser, version 18 and earlier. It also works in the Android browser, of course.
  
> The functionality has changed slightly in Chrome for Android, versions 25 and later. It is no longer possible to launch an Android app by setting an iframe's src attribute. For example, navigating an iframe to a URI with a custom scheme such as paulsawesomeapp:// will not work even if the user has the appropriate app installed. Instead, you should implement a user gesture to launch the app via a custom scheme, or use the “intent:” syntax described in this article.


也就是在chrome中不能通过iframe跳转自定义scheme唤起APP了，直接被block，如下图：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-c83e86daeb44c505.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

但是仍然可以通过window.location.href唤起：

	function clickAndroid1(){
	       window.location.href="yaxxxuan://lab/u.xx.com";
	}

或者通过<a>跳转标签唤起

	<a href="yanxuan://lab/u.you.com">测试</a>

当然，如果自定义了https/http的也是可以的。总的来说Chrome除了Iframe，其他的好像都没问题。

	<a href="https://xxx.com/a/g">  https 跳转</a>

# 国内乱七八糟的浏览器（观察日期2019-6-11）


* 360浏览器，可以通过iframe、<a>、<ref> 方式调用scheme，除了不支持https/http，其他都支持
* UC浏览器可以通过iframe、<a>、<ref> 方式调用scheme（即便如此，也可能被屏蔽（域名）） ，无法通过https/http/intent 
* QQ浏览器可以通过iframe、<a>、<ref> 、intent 方式调用scheme，（也可能被屏蔽（域名），目前看没屏蔽） ，但是无法通过https/http

# 总结


其实关于applink有几个比较特殊的点：

* applink第一它只验证一次，在安装的时候，为什么不每次启动动检测呢？可能是为了给用户自己选怎留后门。
* applink验证的时候需要联网，不联网的方案行吗？个人理解，不联网应该也可以，只要在安装的时候，只本地验证好了，但是这样明显没有双向验证安全，因为双向验证证明了网站跟app是一对一应的，这样才能保证安全，防止第三方打包篡改。


# 参考文档  

[Android M DeepLinks AppLinks 详解](http://fanhongwei.github.io/blog/2015/12/17/app-links-deep-links/)        
[Verify Android App Links](https://developer.android.com/training/app-links/verify-site-associations)      
