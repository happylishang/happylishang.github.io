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
 
 
        
# 参考文档  

https://developer.android.com/training/app-links/verify-site-associations

