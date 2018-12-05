APPLink跟DeepLink其实都是用来唤起某个APP的特定界面的做法，一般是从APP外部，比如短信里面，或者浏览器里面。Android流程跟基本的startActivity类似，只不过多了一个选怎过程，因为这类跳转一般都是通过Action_View进行跳转，而响应这个Action的APP可能不止一个，而且还有些默认设置之类的，6.0之前可以都看作scheme的deeplink，6.0之后多了个APPLink，在安装时候，系统会对APP进行校验，如果APP配置了支持http/https，且可以自动校验，那么就去APP制定的服务器下载验证，验证过了后，其他APP通过Action_View并配置了scheme跳转的时候，就可以打开当前的配置，不过，这个流程难道需要服务器，可能是为了安全吧，因为，不依赖服务器，其实也完全可以做到。

**APPLINK只是在安装时候多了一个验证，其他跟之前deeplink一样，如果没联网，验证失败，那就跟之前的deeplink表现一样**

h5内部最好用自定义的scheme，不用http打头的，很容易被webview自己拦截，其次如果，不选择app，而选择浏览器打开，会怎样，需要重定位吗？

private Result retrieveFromAndroid(AndroidAppAsset asset) throws AssociationServiceException {
    try {
AndroidPackageInfoFetcher
    
        
Google弄了个AssetLink可以再APP跟Web之间共享，好像还跟Google框架有关系，比如登陆信息，类似账户信息共享。

Share stored credentials between apps and sites

	Share login credentials from the source to the target, so the user only needs to sign in once to access both services.	Details

先无视掉吧。 参考文档：https://developers.google.com/identity/smartlock-passwords/android/associate-apps-and-sites

If your app that uses Smart Lock for Passwords shares a user database with your website—or if your app and website use federated sign-in providers such as Google Sign-In—you can associate the app with the website so that users save their credentials once and then automatically sign in to both the app and the website.
        

流程

* 安装
* 校验scheme及http scheme（联网才行）
* 持久化本地，放在哪？
* 通过其他唤起APP
* 已经校验设置了？直接唤起，否则选择 对于短信一般http 否则 自定义scheme
* 选择界面ResolveActivity更新配置
* 下一轮选择

限制：是否会唤起APP，完全取决于第三方APP是否会发送对应的intent，系统没有强制行动规则，所以像微信，就完全禁止了scheme唤起，无论你发什么，都没有用，只能加他的白名单，像短信也类似，不过一般短信会允许http链接跳转第三方app，配合APPLink短信一般可以直接唤起

1、APP link使用
2、唤起原理
3、安装原理


https://digitalassetlinks.googleapis.com/v1/statements:list?
   source.web.site=https://you.163.com&relation=delegate_permission/common.handle_all_urls
 
 
难道只有

# 参考文档

[Android M DeepLinks AppLinks 详解](http://fanhongwei.github.io/blog/2015/12/17/app-links-deep-links/)        
[Verify Android App Links](https://developer.android.com/training/app-links/verify-site-associations)

    private void installPackageLI(InstallArgs args, PackageInstalledInfo res) {
        final int installFlags = args.installFlags;
        ...
        startIntentFilterVerifications(args.user.getIdentifier(), replace, pkg);

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
    
    ...
    
        case START_INTENT_FILTER_VERIFICATIONS: {
            IFVerificationParams params = (IFVerificationParams) msg.obj;
            verifyIntentFiltersIfNeeded(params.userId, params.verifierUid,
                    params.replacing, params.pkg);
            break;
        }
        
        
        
       
    private void verifyIntentFiltersIfNeeded(int userId, int verifierUid, boolean replacing,
            PackageParser.Package pkg) {
        ...
            // If any filters need to be verified, then all need to be.
            boolean needToVerify = false;
            for (PackageParser.Activity a : pkg.activities) {
                for (ActivityIntentInfo filter : a.intents) {
                <!--needsVerification是否设置autoverify -->
                    if (filter.needsVerification() && needsNetworkVerificationLPr(filter)) {
                        if (DEBUG_DOMAIN_VERIFICATION) {
                            Slog.d(TAG, "Intent filter needs verification, so processing all filters");
                        }
                        needToVerify = true;
                        break;
                    }
                }
            }

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
                        }
                    }
                }
            }
        }

        if (count > 0) {
            mIntentFilterVerifier.startVerifications(userId);
        } 
    }
 


    
 
    @Override
        public void startVerifications(int userId) {
            // Launch verifications requests
            int count = mCurrentIntentFilterVerifications.size();
            for (int n=0; n<count; n++) {
                int verificationId = mCurrentIntentFilterVerifications.get(n);
                final IntentFilterVerificationState ivs =
                        mIntentFilterVerificationStates.get(verificationId);

                String packageName = ivs.getPackageName();

                ArrayList<PackageParser.ActivityIntentInfo> filters = ivs.getFilters();
                final int filterCount = filters.size();
                ArraySet<String> domainsSet = new ArraySet<>();
                for (int m=0; m<filterCount; m++) {
                    PackageParser.ActivityIntentInfo filter = filters.get(m);
                    domainsSet.addAll(filter.getHostsList());
                }
                ArrayList<String> domainsList = new ArrayList<>(domainsSet);
                synchronized (mPackages) {
                    if (mSettings.createIntentFilterVerificationIfNeededLPw(
                            packageName, domainsList) != null) {
                        scheduleWriteSettingsLocked();
                    }
                }
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
            if (DEBUG_DOMAIN_VERIFICATION) Slog.d(TAG,
                    "Sending IntentFilter verification broadcast");
        }




public final class IntentFilterVerificationReceiver extends BroadcastReceiver {
    private static final String TAG = IntentFilterVerificationReceiver.class.getSimpleName();

    private static final Integer MAX_HOSTS_PER_REQUEST = 10;

    private static final String HANDLE_ALL_URLS_RELATION
            = "delegate_permission/common.handle_all_urls";

    private static final String ANDROID_ASSET_FORMAT = "{\"namespace\": \"android_app\", "
            + "\"package_name\": \"%s\", \"sha256_cert_fingerprints\": [\"%s\"]}";
    private static final String WEB_ASSET_FORMAT = "{\"namespace\": \"web\", \"site\": \"%s\"}";
    private static final Pattern ANDROID_PACKAGE_NAME_PATTERN =
            Pattern.compile("^[a-zA-Z_][a-zA-Z0-9_]*(\\.[a-zA-Z_][a-zA-Z0-9_]*)*$");
    private static final String TOO_MANY_HOSTS_FORMAT =
            "Request contains %d hosts which is more than the allowed %d.";

    private static void sendErrorToPackageManager(PackageManager packageManager,
            int verificationId) {
        packageManager.verifyIntentFilter(verificationId,
                PackageManager.INTENT_FILTER_VERIFICATION_FAILURE,
                Collections.<String>emptyList());
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        final String action = intent.getAction();
        if (Intent.ACTION_INTENT_FILTER_NEEDS_VERIFICATION.equals(action)) {
            Bundle inputExtras = intent.getExtras();
            if (inputExtras != null) {
                Intent serviceIntent = new Intent(context, DirectStatementService.class);
                serviceIntent.setAction(DirectStatementService.CHECK_ALL_ACTION);

                int verificationId = inputExtras.getInt(
                        PackageManager.EXTRA_INTENT_FILTER_VERIFICATION_ID);
                String scheme = inputExtras.getString(
                        PackageManager.EXTRA_INTENT_FILTER_VERIFICATION_URI_SCHEME);
                String hosts = inputExtras.getString(
                        PackageManager.EXTRA_INTENT_FILTER_VERIFICATION_HOSTS);
                String packageName = inputExtras.getString(
                        PackageManager.EXTRA_INTENT_FILTER_VERIFICATION_PACKAGE_NAME);

                Log.i(TAG, "Verify IntentFilter for " + hosts);

                Bundle extras = new Bundle();
                extras.putString(DirectStatementService.EXTRA_RELATION, HANDLE_ALL_URLS_RELATION);

                String[] hostList = hosts.split(" ");
                if (hostList.length > MAX_HOSTS_PER_REQUEST) {
                    Log.w(TAG, String.format(TOO_MANY_HOSTS_FORMAT,
                            hostList.length, MAX_HOSTS_PER_REQUEST));
                    sendErrorToPackageManager(context.getPackageManager(), verificationId);
                    return;
                }

                try {
                    ArrayList<String> sourceAssets = new ArrayList<String>();
                    for (String host : hostList) {
                        sourceAssets.add(createWebAssetString(scheme, host));
                    }
                    extras.putStringArrayList(DirectStatementService.EXTRA_SOURCE_ASSET_DESCRIPTORS,
                            sourceAssets);
                } catch (MalformedURLException e) {
                    Log.w(TAG, "Error when processing input host: " + e.getMessage());
                    sendErrorToPackageManager(context.getPackageManager(), verificationId);
                    return;
                }
                try {
                    extras.putString(DirectStatementService.EXTRA_TARGET_ASSET_DESCRIPTOR,
                            createAndroidAssetString(context, packageName));
                } catch (NameNotFoundException e) {
                    Log.w(TAG, "Error when processing input Android package: " + e.getMessage());
                    sendErrorToPackageManager(context.getPackageManager(), verificationId);
                    return;
                }
                extras.putParcelable(DirectStatementService.EXTRA_RESULT_RECEIVER,
                        new IsAssociatedResultReceiver(
                                new Handler(), context.getPackageManager(), verificationId));

                serviceIntent.putExtras(extras);
                context.startService(serviceIntent);
            }
        } else {
            Log.w(TAG, "Intent action not supported: " + action);
        }
    }

    private String createAndroidAssetString(Context context, String packageName)
            throws NameNotFoundException {
        if (!ANDROID_PACKAGE_NAME_PATTERN.matcher(packageName).matches()) {
            throw new NameNotFoundException("Input package name is not valid.");
        }

        List<String> certFingerprints =
                Utils.getCertFingerprintsFromPackageManager(packageName, context);

        return String.format(ANDROID_ASSET_FORMAT, packageName,
                Utils.joinStrings("\", \"", certFingerprints));
    }

    private String createWebAssetString(String scheme, String host) throws MalformedURLException {
        if (!Patterns.DOMAIN_NAME.matcher(host).matches()) {
            throw new MalformedURLException("Input host is not valid.");
        }
        if (!scheme.equals("http") && !scheme.equals("https")) {
            throw new MalformedURLException("Input scheme is not valid.");
        }

        return String.format(WEB_ASSET_FORMAT, new URL(scheme, host, "").toString());
    }

    /**
     * Receives the result of {@code StatementService.CHECK_ACTION} from
     * {@link DirectStatementService} and passes it back to {@link PackageManager}.
     */
    private static class IsAssociatedResultReceiver extends ResultReceiver {

        private final int mVerificationId;
        private final PackageManager mPackageManager;

        public IsAssociatedResultReceiver(Handler handler, PackageManager packageManager,
                int verificationId) {
            super(handler);
            mVerificationId = verificationId;
            mPackageManager = packageManager;
        }

        @Override
        protected void onReceiveResult(int resultCode, Bundle resultData) {
            if (resultCode == DirectStatementService.RESULT_SUCCESS) {
                if (resultData.getBoolean(DirectStatementService.IS_ASSOCIATED)) {
                    mPackageManager.verifyIntentFilter(mVerificationId,
                            PackageManager.INTENT_FILTER_VERIFICATION_SUCCESS,
                            Collections.<String>emptyList());
                } else {
                    mPackageManager.verifyIntentFilter(mVerificationId,
                            PackageManager.INTENT_FILTER_VERIFICATION_FAILURE,
                            resultData.getStringArrayList(DirectStatementService.FAILED_SOURCES));
                }
            } else {
                sendErrorToPackageManager(mPackageManager, mVerificationId);
            }
        }
    }
}


> 12-03 17:07:22.362 14747  6055 I Finsky  : [258] com.google.android.finsky.p2p.f.run(3): Wrote row to frosting DB: 723
> 12-03 17:07:22.365  6207  6059 I SingleHostAsyncVerifier: Verification result: checking for a statement with source b <
> 12-03 17:07:22.365  6207  6059 I SingleHostAsyncVerifier:   a: "https://u.163.com"
> 12-03 17:07:22.365  6207  6059 I SingleHostAsyncVerifier: >
> 12-03 17:07:22.365  6207  6059 I SingleHostAsyncVerifier: , relation delegate_permission/common.handle_all_urls, and target a <
> 12-03 17:07:22.365  6207  6059 I SingleHostAsyncVerifier:   a <
> 12-03 17:07:22.365  6207  6059 I SingleHostAsyncVerifier:     a: "31:38:96:7E:26:41:9D:B1:73:EC:04:B2:0C:91:09:E0:42:72:DE:21:A5:D7:3B:47:E0:49:62:5F:FF:C4:D9:7A"
> 12-03 17:07:22.365  6207  6059 I SingleHostAsyncVerifier:   >
> 12-03 17:07:22.365  6207  6059 I SingleHostAsyncVerifier:   b: "com.netease.yanxuan"
> 12-03 17:07:22.365  6207  6059 I SingleHostAsyncVerifier: >
> 12-03 17:07:22.365  6207  6059 I SingleHostAsyncVerifier:  --> false.
> 12-03 17:07:22.366  6207  6049 I IntentFilterIntentOp: Verification 2 complete. Success:false. Failed hosts:u.163.com.
> 12-03 17:07:22.375  6207  6075 I Icing   : Usage reports ok 0, Failed Usage reports 0, indexed 0, rejected 0, imm upload false





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
        
        
            @Override
    public Result retrieveStatements(AbstractAsset source) throws AssociationServiceException {
        if (source instanceof AndroidAppAsset) {
            return retrieveFromAndroid((AndroidAppAsset) source);
        } else if (source instanceof WebAsset) {
            return retrieveFromWeb((WebAsset) source);
        } else {
            throw new AssociationServiceException("Namespace is not supported.");
        }
    }
    
    
        private Result retrieveFromAndroid(AndroidAppAsset asset) throws AssociationServiceException {
        try {
            List<String> delegates = new ArrayList<String>();
            List<Statement> statements = new ArrayList<Statement>();

            List<String> certFps = mAndroidFetcher.getCertFingerprints(asset.getPackageName());
            if (!Utils.hasCommonString(certFps, asset.getCertFingerprints())) {
                throw new AssociationServiceException(
                        "Specified certs don't match the installed app.");
            }

            AndroidAppAsset actualSource = AndroidAppAsset.create(asset.getPackageName(), certFps);
            for (String statementJson : mAndroidFetcher.getStatements(asset.getPackageName())) {
                ParsedStatement result =
                        StatementParser.parseStatement(statementJson, actualSource);
                statements.addAll(result.getStatements());
                delegates.addAll(result.getDelegates());
            }

            for (String delegate : delegates) {
                statements.addAll(retrieveStatementFromUrl(delegate, MAX_INCLUDE_LEVEL,
                        actualSource).getStatements());
            }

            return Result.create(statements, DO_NOT_CACHE_RESULT);
        } catch (JSONException | IOException | NameNotFoundException e) {
            Log.w(DirectStatementRetriever.class.getSimpleName(), e);
            return Result.create(Collections.<Statement>emptyList(), DO_NOT_CACHE_RESULT);
        }
    }
    
    
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
            for (String delegate : result.getDelegates()) {
                statements.addAll(
                        retrieveStatementFromUrl(delegate, maxIncludeLevel - 1, source)
                                .getStatements());
            }
            return Result.create(statements, webContent.getExpireTimeMillis());
        } catch (JSONException | IOException e) {
            return Result.create(statements, DO_NOT_CACHE_RESULT);
        }
    }

    private Result retrieveFromWeb(WebAsset asset)
            throws AssociationServiceException {
        return retrieveStatementFromUrl(computeAssociationJsonUrl(asset), MAX_INCLUDE_LEVEL, asset);
    }
    
    
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

            if (connection.getResponseCode() != HttpURLConnection.HTTP_OK) {
                return new WebContent("", DO_NOT_CACHE_RESULT);
            }

            if (connection.getContentLength() > fileSizeLimit) {
                return new WebContent("", DO_NOT_CACHE_RESULT);
            }

            Long expireTimeMillis = getExpirationTimeMillisFromHTTPHeader(
                    connection.getHeaderFields());

            return new WebContent(inputStreamToString(
                    connection.getInputStream(), connection.getContentLength(), fileSizeLimit),
                expireTimeMillis);
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }
    
#  检查是否有   hasDomainURLs
    
    /**
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
 
 比如下面   
    
    
        <intent-filter android:autoVerify="true">
            <data android:scheme="https" android:host="u.163.com" android:pathPrefix="/a/" />
            <data android:scheme="http" android:host="u.163.com" android:pathPrefix="/a/" />
            <!--外部intent打开，比如短信，文本编辑等-->
            <action android:name="android.intent.action.VIEW" />
            <category android:name="android.intent.category.DEFAULT" />
            <!--浏览器可以打开-->
            <category android:name="android.intent.category.BROWSABLE" />
        </intent-filter>

第二步校验

    public final boolean needsVerification() {
        return getAutoVerify() && handlesWebUris(true);
    }

     */
    public final boolean getAutoVerify() {
        return ((mVerifyState & STATE_VERIFY_AUTO) == STATE_VERIFY_AUTO);
    }
    
    
    
    
    private Activity parseActivity(Package owner, Resources res,
            XmlResourceParser parser, int flags, String[] outError, CachedComponentArgs cachedArgs,
            boolean receiver, boolean hardwareAccelerated){
            ...
                            if (!parseIntent(res, parser, true /*allowGlobs*/, true /*allowAutoVerify*/,
                        intent, outError)) {
                    return null;
                }
                
                
    private boolean parseIntent(Resources res, XmlResourceParser parser, boolean allowGlobs,
            boolean allowAutoVerify, IntentInfo outInfo, String[] outError)
                    throws XmlPullParserException, IOException {

        ...
        if (allowAutoVerify) {
            outInfo.setAutoVerify(sa.getBoolean(
                    com.android.internal.R.styleable.AndroidManifestIntentFilter_autoVerify,
                    false));
        }
        
        
             public final boolean handlesWebUris(boolean onlyWebSchemes) {
        // Require ACTION_VIEW, CATEGORY_BROWSEABLE, and at least one scheme
        if (!hasAction(Intent.ACTION_VIEW)
            || !hasCategory(Intent.CATEGORY_BROWSABLE)
            || mDataSchemes == null
            || mDataSchemes.size() == 0) {
            return false;
        }
        
        
        // Now allow only the schemes "http" and "https"
        final int N = mDataSchemes.size();
        for (int i = 0; i < N; i++) {
            final String scheme = mDataSchemes.get(i);
            final boolean isWebScheme =
                    SCHEME_HTTP.equals(scheme) || SCHEME_HTTPS.equals(scheme);
            if (onlyWebSchemes) {
                // If we're specifically trying to ensure that there are no non-web schemes
                // declared in this filter, then if we ever see a non-http/https scheme then
                // we know it's a failure.
                if (!isWebScheme) {
                    return false;
                }
            } else {
                // If we see any http/https scheme declaration in this case then the
                // filter matches what we're looking for.
                if (isWebScheme) {
                    return true;
                }
            }
        }

        // We get here if:
        //   1) onlyWebSchemes and no non-web schemes were found, i.e success; or
        //   2) !onlyWebSchemes and no http/https schemes were found, i.e. failure.
        return onlyWebSchemes;
    }
    
    
    
    The assetlinks.json file must be served as Content-Type: application/json in the HTTP headers, and it cannot be a redirect (that is, 301 or 302 response codes are not followed).
    
    
# 查看Verify的权限   

adb shell dumpsys package d

Note: Make sure you wait at least 20 seconds after installation of your app to allow for the system to complete the verification process.

The command returns a listing of each user or profile defined on the device, preceded by a header in the following format:

App linkages for user 0:
Following this header, the output uses the following format to list the link-handling settings for that user:

Package: com.android.vending
Domains: play.google.com market.android.com
Status: always : 200000002
This listing indicates which apps are associated with which domains for that user:

Package - Identifies an app by its package name, as declared in its manifest.
Domains - Shows the full list of hosts whose web links this app handles, using blank spaces as delimiters.
Status - Shows the current link-handling setting for this app. An app that has passed verification, and whose manifest contains android:autoVerify="true", shows a status of always. The hexadecimal number after this status is related to the Android system's record of the user’s app linkage preferences. This value does not indicate whether verification succeeded.



	12-04 19:26:36.223  2215 19404 I SingleHostAsyncVerifier: Verification result: checking for a statement with source b <
	12-04 19:26:36.223  2215 19404 I SingleHostAsyncVerifier:   a: "https://u.163.com"
	12-04 19:26:36.223  2215 19404 I SingleHostAsyncVerifier: >
	12-04 19:26:36.223  2215 19404 I SingleHostAsyncVerifier: , relation delegate_permission/common.handle_all_urls, and target a <
	12-04 19:26:36.223  2215 19404 I SingleHostAsyncVerifier:   a <
	12-04 19:26:36.223  2215 19404 I SingleHostAsyncVerifier:     a: "31:38:96:7E:26:41:9D:B1:73:EC:04:B2:0C:91:09:E0:42:72:DE:21:A5:D7:3B:47:E0:49:62:5F:FF:C4:D9:7A"
	12-04 19:26:36.223  2215 19404 I SingleHostAsyncVerifier:   >
	12-04 19:26:36.223  2215 19404 I SingleHostAsyncVerifier:   b: "com.netease.yanxuan"
	12-04 19:26:36.223  2215 19404 I SingleHostAsyncVerifier: >
	12-04 19:26:36.223  2215 19404 I SingleHostAsyncVerifier:  --> true.
	12-04 19:26:36.223  2215 19403 I IntentFilterIntentOp: Verification 125 complete. Success:true. Failed hosts:.




# ResolverActivity


> ActivityStatckSUpervisor

        if (err == ActivityManager.START_SUCCESS) {
            Slog.i(TAG, "START u" + userId + " {" + intent.toShortString(true, true, true, false)
                    + "} from uid " + callingUid
                    + " on display " + (container == null ? (mFocusedStack == null ?
                            Display.DEFAULT_DISPLAY : mFocusedStack.mDisplayId) :
                            (container.mActivityDisplay == null ? Display.DEFAULT_DISPLAY :
                                    container.mActivityDisplay.mDisplayId)));
        }
        
 

> 12-04 20:32:04.367   887  9064 I ActivityManager: START u0 {act=android.intent.action.VIEW dat=https://u.163.com/... cmp=android/com.android.internal.app.ResolverActivity (has extras)} from uid 10067 on display 0
 
 
>  ActivityStatckSUpervisor
 
	 final int startActivityMayWait(IApplicationThread caller, int callingUid, String callingPackage, Intent intent, String resolvedType, IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor, IBinder resultTo, String resultWho, int requestCode, int startFlags, ProfilerInfo profilerInfo, WaitResult outResult, Configuration config, Bundle options, boolean ignoreTargetSecurity, int userId, IActivityContainer iContainer, TaskRecord inTask) {
	    ...
	    boolean componentSpecified = intent.getComponent() != null;
	    //创建新的Intent对象，即便intent被修改也不受影响
	    intent = new Intent(intent);
	
	    //收集Intent所指向的Activity信息, 当存在多个可供选择的Activity,则直接向用户弹出resolveActivity [见2.7.1]
	    ActivityInfo aInfo = resolveActivity(intent, resolvedType, startFlags, profilerInfo, userId);
	    
  
  
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
        
   
>    packagemanagerservice


    @Override
    public ResolveInfo resolveIntent(Intent intent, String resolvedType,
            int flags, int userId) {
        if (!sUserManager.exists(userId)) return null;
        enforceCrossUserPermission(Binder.getCallingUid(), userId, false, false, "resolve intent");
        List<ResolveInfo> query = queryIntentActivities(intent, resolvedType, flags, userId);
        return chooseBestActivity(intent, resolvedType, flags, query, userId);
    }

@Override
    public List<ResolveInfo> queryIntentActivities(Intent intent,
            String resolvedType, int flags, int userId) {
        if (!sUserManager.exists(userId)) return Collections.emptyList();
        enforceCrossUserPermission(Binder.getCallingUid(), userId, false, false, "query intent activities");
        ComponentName comp = intent.getComponent();
        if (comp == null) {
            if (intent.getSelector() != null) {
                intent = intent.getSelector();
                comp = intent.getComponent();
            }
        }

        if (comp != null) {
            final List<ResolveInfo> list = new ArrayList<ResolveInfo>(1);
            final ActivityInfo ai = getActivityInfo(comp, flags, userId);
            if (ai != null) {
                final ResolveInfo ri = new ResolveInfo();
                ri.activityInfo = ai;
                list.add(ri);
            }
            return list;
        }

        // reader
        synchronized (mPackages) {
            final String pkgName = intent.getPackage();
            if (pkgName == null) {
                List<CrossProfileIntentFilter> matchingFilters =
                        getMatchingCrossProfileIntentFilters(intent, resolvedType, userId);
                // Check for results that need to skip the current profile.
                ResolveInfo xpResolveInfo  = querySkipCurrentProfileIntents(matchingFilters, intent,
                        resolvedType, flags, userId);
                if (xpResolveInfo != null && isUserEnabled(xpResolveInfo.targetUserId)) {
                    List<ResolveInfo> result = new ArrayList<ResolveInfo>(1);
                    result.add(xpResolveInfo);
                    return filterIfNotPrimaryUser(result, userId);
                }

                // Check for results in the current profile.
                List<ResolveInfo> result = mActivities.queryIntent(
                        intent, resolvedType, flags, userId);

                // Check for cross profile results.
                xpResolveInfo = queryCrossProfileIntents(
                        matchingFilters, intent, resolvedType, flags, userId);
                if (xpResolveInfo != null && isUserEnabled(xpResolveInfo.targetUserId)) {
                    result.add(xpResolveInfo);
                    Collections.sort(result, mResolvePrioritySorter);
                }
                result = filterIfNotPrimaryUser(result, userId);
                if (hasWebURI(intent)) {
                    CrossProfileDomainInfo xpDomainInfo = null;
                    final UserInfo parent = getProfileParent(userId);
                    if (parent != null) {
                        xpDomainInfo = getCrossProfileDomainPreferredLpr(intent, resolvedType,
                                flags, userId, parent.id);
                    }
                    if (xpDomainInfo != null) {
                        if (xpResolveInfo != null) {
                            // If we didn't remove it, the cross-profile ResolveInfo would be twice
                            // in the result.
                            result.remove(xpResolveInfo);
                        }
                        if (result.size() == 0) {
                            result.add(xpDomainInfo.resolveInfo);
                            return result;
                        }
                    } else if (result.size() <= 1) {
                        return result;
                    }
                    result = filterCandidatesWithDomainPreferredActivitiesLPr(intent, flags, result,
                            xpDomainInfo, userId);
                    Collections.sort(result, mResolvePrioritySorter);
                }
                return result;
            }
            final PackageParser.Package pkg = mPackages.get(pkgName);
            if (pkg != null) {
                return filterIfNotPrimaryUser(
                        mActivities.queryIntentForPackage(
                                intent, resolvedType, flags, pkg.activities, userId),
                        userId);
            }
            return new ArrayList<ResolveInfo>();
        }
    }
    
    private ResolveInfo chooseBestActivity(Intent intent, String resolvedType,
            int flags, List<ResolveInfo> query, int userId) {
        if (query != null) {
            final int N = query.size();
            if (N == 1) {
                return query.get(0);
            } else if (N > 1) {
                final boolean debug = ((intent.getFlags() & Intent.FLAG_DEBUG_LOG_RESOLUTION) != 0);
                // If there is more than one activity with the same priority,
                // then let the user decide between them.
                ResolveInfo r0 = query.get(0);
                ResolveInfo r1 = query.get(1);
                if (DEBUG_INTENT_MATCHING || debug) {
                    Slog.v(TAG, r0.activityInfo.name + "=" + r0.priority + " vs "
                            + r1.activityInfo.name + "=" + r1.priority);
                }
                // If the first activity has a higher priority, or a different
                // default, then it is always desireable to pick it.
                if (r0.priority != r1.priority
                        || r0.preferredOrder != r1.preferredOrder
                        || r0.isDefault != r1.isDefault) {
                    return query.get(0);
                }
                // If we have saved a preference for a preferred activity for
                // this Intent, use that.
                ResolveInfo ri = findPreferredActivity(intent, resolvedType,
                        flags, query, r0.priority, true, false, debug, userId);
                if (ri != null) {
                    return ri;
                }
                ri = new ResolveInfo(mResolveInfo);
                ri.activityInfo = new ActivityInfo(ri.activityInfo);
                ri.activityInfo.applicationInfo = new ApplicationInfo(
                        ri.activityInfo.applicationInfo);
                if (userId != 0) {
                    ri.activityInfo.applicationInfo.uid = UserHandle.getUid(userId,
                            UserHandle.getAppId(ri.activityInfo.applicationInfo.uid));
                }
                // Make sure that the resolver is displayable in car mode
                if (ri.activityInfo.metaData == null) ri.activityInfo.metaData = new Bundle();
                ri.activityInfo.metaData.putBoolean(Intent.METADATA_DOCK_HOME, true);
                return ri;
            }
        }
        return null;
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
            if (DEBUG_PREFERRED) intent.addFlags(Intent.FLAG_DEBUG_LOG_RESOLUTION);

            // Try to find a matching persistent preferred activity.
            ResolveInfo pri = findPersistentPreferredActivityLP(intent, resolvedType, flags, query,
                    debug, userId);

            // If a persistent preferred activity matched, use it.
            if (pri != null) {
                return pri;
            }

            PreferredIntentResolver pir = mSettings.mPreferredActivities.get(userId);
            // Get the list of preferred activities that handle the intent
            if (DEBUG_PREFERRED || debug) Slog.v(TAG, "Looking for preferred activities...");
            List<PreferredActivity> prefs = pir != null
                    ? pir.queryIntent(intent, resolvedType,
                            (flags & PackageManager.MATCH_DEFAULT_ONLY) != 0, userId)
                    : null;
            if (prefs != null && prefs.size() > 0) {
                boolean changed = false;
                try {
                    // First figure out how good the original match set is.
                    // We will only allow preferred activities that came
                    // from the same match quality.
                    int match = 0;

                    if (DEBUG_PREFERRED || debug) Slog.v(TAG, "Figuring out best match...");

                    final int N = query.size();
                    for (int j=0; j<N; j++) {
                        final ResolveInfo ri = query.get(j);
                        if (DEBUG_PREFERRED || debug) Slog.v(TAG, "Match for " + ri.activityInfo
                                + ": 0x" + Integer.toHexString(match));
                        if (ri.match > match) {
                            match = ri.match;
                        }
                    }

                    if (DEBUG_PREFERRED || debug) Slog.v(TAG, "Best match: 0x"
                            + Integer.toHexString(match));

                    match &= IntentFilter.MATCH_CATEGORY_MASK;
                    final int M = prefs.size();
                    for (int i=0; i<M; i++) {
                        final PreferredActivity pa = prefs.get(i);
                        if (DEBUG_PREFERRED || debug) {
                            Slog.v(TAG, "Checking PreferredActivity ds="
                                    + (pa.countDataSchemes() > 0 ? pa.getDataScheme(0) : "<none>")
                                    + "\n  component=" + pa.mPref.mComponent);
                            pa.dump(new LogPrinter(Log.VERBOSE, TAG, Log.LOG_ID_SYSTEM), "  ");
                        }
                        if (pa.mPref.mMatch != match) {
                            if (DEBUG_PREFERRED || debug) Slog.v(TAG, "Skipping bad match "
                                    + Integer.toHexString(pa.mPref.mMatch));
                            continue;
                        }
                        // If it's not an "always" type preferred activity and that's what we're
                        // looking for, skip it.
                        if (always && !pa.mPref.mAlways) {
                            if (DEBUG_PREFERRED || debug) Slog.v(TAG, "Skipping mAlways=false entry");
                            continue;
                        }
                        final ActivityInfo ai = getActivityInfo(pa.mPref.mComponent,
                                flags | PackageManager.GET_DISABLED_COMPONENTS, userId);
                        if (DEBUG_PREFERRED || debug) {
                            Slog.v(TAG, "Found preferred activity:");
                            if (ai != null) {
                                ai.dump(new LogPrinter(Log.VERBOSE, TAG, Log.LOG_ID_SYSTEM), "  ");
                            } else {
                                Slog.v(TAG, "  null");
                            }
                        }
                        if (ai == null) {
                            // This previously registered preferred activity
                            // component is no longer known.  Most likely an update
                            // to the app was installed and in the new version this
                            // component no longer exists.  Clean it up by removing
                            // it from the preferred activities list, and skip it.
                            Slog.w(TAG, "Removing dangling preferred activity: "
                                    + pa.mPref.mComponent);
                            pir.removeFilter(pa);
                            changed = true;
                            continue;
                        }
                        for (int j=0; j<N; j++) {
                            final ResolveInfo ri = query.get(j);
                            if (!ri.activityInfo.applicationInfo.packageName
                                    .equals(ai.applicationInfo.packageName)) {
                                continue;
                            }
                            if (!ri.activityInfo.name.equals(ai.name)) {
                                continue;
                            }

                            if (removeMatches) {
                                pir.removeFilter(pa);
                                changed = true;
                                if (DEBUG_PREFERRED) {
                                    Slog.v(TAG, "Removing match " + pa.mPref.mComponent);
                                }
                                break;
                            }

                            // Okay we found a previously set preferred or last chosen app.
                            // If the result set is different from when this
                            // was created, we need to clear it and re-ask the
                            // user their preference, if we're looking for an "always" type entry.
                            if (always && !pa.mPref.sameSet(query)) {
                                Slog.i(TAG, "Result set changed, dropping preferred activity for "
                                        + intent + " type " + resolvedType);
                                if (DEBUG_PREFERRED) {
                                    Slog.v(TAG, "Removing preferred activity since set changed "
                                            + pa.mPref.mComponent);
                                }
                                pir.removeFilter(pa);
                                // Re-add the filter as a "last chosen" entry (!always)
                                PreferredActivity lastChosen = new PreferredActivity(
                                        pa, pa.mPref.mMatch, null, pa.mPref.mComponent, false);
                                pir.addFilter(lastChosen);
                                changed = true;
                                return null;
                            }

                            // Yay! Either the set matched or we're looking for the last chosen
                            if (DEBUG_PREFERRED || debug) Slog.v(TAG, "Returning preferred activity: "
                                    + ri.activityInfo.packageName + "/" + ri.activityInfo.name);
                            return ri;
                        }
                    }
                } finally {
                    if (changed) {
                        if (DEBUG_PREFERRED) {
                            Slog.v(TAG, "Preferred activity bookkeeping changed; writing restrictions");
                        }
                        scheduleWritePackageRestrictionsLocked(userId);
                    }
                }
            }
        }
        if (DEBUG_PREFERRED || debug) Slog.v(TAG, "No preferred activity to return");
        return null;
    }



    @Override
    public ActivityInfo getActivityInfo(ComponentName component, int flags, int userId) {
        if (!sUserManager.exists(userId)) return null;
        enforceCrossUserPermission(Binder.getCallingUid(), userId, false, false, "get activity info");
        synchronized (mPackages) {
            PackageParser.Activity a = mActivities.mActivities.get(component);

            if (DEBUG_PACKAGE_INFO) Log.v(TAG, "getActivityInfo " + component + ": " + a);
            if (a != null && mSettings.isEnabledLPr(a.info, flags, userId)) {
                PackageSetting ps = mSettings.mPackages.get(component.getPackageName());
                if (ps == null) return null;
                return PackageParser.generateActivityInfo(a, flags, ps.readUserState(userId),
                        userId);
            }
            if (mResolveComponentName.equals(component)) {
                return PackageParser.generateActivityInfo(mResolveActivity, flags,
                        new PackageUserState(), userId);
            }
        }
        return null;
    }
  
  弄一个  ResolverActivity
  
        public static final ActivityInfo generateActivityInfo(ActivityInfo ai, int flags,
            PackageUserState state, int userId) {
        if (ai == null) return null;
        if (!checkUseInstalledOrHidden(flags, state)) {
            return null;
        }
        // This is only used to return the ResolverActivity; we will just always
        // make a copy.
        ai = new ActivityInfo(ai);
        ai.applicationInfo = generateApplicationInfo(ai.applicationInfo, flags, state, userId);
        return ai;
    }

# 验证成功失败都有回调

	  private static class IsAssociatedResultReceiver extends ResultReceiver {
	
	        private final int mVerificationId;
	        private final PackageManager mPackageManager;
	
	        public IsAssociatedResultReceiver(Handler handler, PackageManager packageManager,
	                int verificationId) {
	            super(handler);
	            mVerificationId = verificationId;
	            mPackageManager = packageManager;
	        }
	
	        @Override
	        protected void onReceiveResult(int resultCode, Bundle resultData) {
	            if (resultCode == DirectStatementService.RESULT_SUCCESS) {
	                if (resultData.getBoolean(DirectStatementService.IS_ASSOCIATED)) {
	                    // 验证成功则一定说明所有的都是成功的，不需要传递
	                    mPackageManager.verifyIntentFilter(mVerificationId,
	                            PackageManager.INTENT_FILTER_VERIFICATION_SUCCESS,
	                            Collections.<String>emptyList());
	                } else {
	                    // 如果失败，则传递失败的数据，保留成功的
	                    mPackageManager.verifyIntentFilter(mVerificationId,
	                            PackageManager.INTENT_FILTER_VERIFICATION_FAILURE,
	                            resultData.getStringArrayList(DirectStatementService.FAILED_SOURCES));
	                }
	            } else {
	                sendErrorToPackageManager(mPackageManager, mVerificationId);
	            }
	        }
	    }
    
成功是所有的都会成功，失败，是部分或者全部失败，部分失败，成功的还是可以支持的

    @Override
    public void verifyIntentFilter(int id, int verificationCode, List<String> failedDomains)
            throws RemoteException {
        mContext.enforceCallingOrSelfPermission(
                Manifest.permission.INTENT_FILTER_VERIFICATION_AGENT,
                "Only intentfilter verification agents can verify applications");

        final Message msg = mHandler.obtainMessage(INTENT_FILTER_VERIFIED);
        final IntentFilterVerificationResponse response = new IntentFilterVerificationResponse(
                Binder.getCallingUid(), verificationCode, failedDomains);
        msg.arg1 = id;
        msg.obj = response;
        mHandler.sendMessage(msg);
    }
    

# 参考文档  

https://developer.android.com/training/app-links/verify-site-associations

