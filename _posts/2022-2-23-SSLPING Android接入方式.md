SSL Pinning是一种防止中间人攻击的技术，

第一种方式用本地文件：

    fun getCertificata(): CertificatePinner? {
        var ca: Certificate? = null
        try {
            val cf: CertificateFactory = CertificateFactory.getInstance("X.509")
            val caInput: InputStream =
                AppProfile.getContext().getResources().openRawResource(R.raw.beep)
            ca = try {
                cf.generateCertificate(caInput)
            } finally {
                caInput.close()
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        var certPin = ""
        if (ca != null) {
            certPin = CertificatePinner.pin(ca)
        }
        return CertificatePinner.Builder()
            .add("*.moutai519.com.cn", certPin)
            .build()
    }

第二种方式，先弄个错误的，LOG中会打印正确的

    private val okHttpClient: OkHttpClient by lazy {
        OkHttpClient().newBuilder()
            .connectTimeout(10L, TimeUnit.SECONDS)
            .readTimeout(10L, TimeUnit.SECONDS)
            .writeTimeout(10L, TimeUnit.SECONDS)
            .cache(Cache(File(appContext.cacheDir, "http"), DISK_CACHE_SIZE))
            .apply {
                if (!BuildConfig.DEBUG && EndpointsSwitcher.get(AppContextProvider.getContext()) == Endpoints.Production) {
                    proxy(Proxy.NO_PROXY)
                        .certificatePinner(
                            CertificatePinner.Builder()
                                .add(
                                    "*.moutai519.com.cn",
                                    "sha256/FziwB72a7rq1h2Kii98Q7cKDkiRIl2/uE6E1794cKZc="
                                ).add(
                                    "*.moutai519.com.cn",
                                    "sha256/4H6OXny7MqJPbCOTpHyS0fSSUeHk/I5nKbIyuQwnfsA="
                                ).add(
                                    "*.moutai519.com.cn",
                                    "sha256/r/mIkG3eEpVdm+u/ko/cwxzOMo1bk4TyHIlByibiA5E="
                                ).build()
                        )
                } else {
                    addInterceptor(
                        HttpLoggingInterceptor { Log.d("HTTP", it) }
                            .also { it.level = HttpLoggingInterceptor.Level.BODY }
                    )
                }
            }
            .addInterceptor(apiInterceptor)
            .build()
    }
    
    
 以上两种方式均可以，在SSL pinging生效的情况下，即使用了代理，也无法抓包，效果如下：
 
 
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/fb6de7e2d0304f4c8fdb84b8bf9e9529~tplv-k3u1fbpfcp-watermark.image?)

客户端会主动拒绝此次通信，因为它发现服务端的证书不正确。
 
![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/062033dcc0c343fe99ec3497c941520f~tplv-k3u1fbpfcp-watermark.image?)

当然，Android客户端还可以禁止代理比如OkHttp的

            proxy(Proxy.NO_PROXY)
            


# 破解

Xposed 插件justTrustMe

	
	
	public class Main implements IXposedHookLoadPackage {
	    private static final String TAG = "JustTrustMe";
	    String currentPackageName = BuildConfig.FLAVOR;
	
	    public void handleLoadPackage(final XC_LoadPackage.LoadPackageParam loadPackageParam) throws Throwable {
	        this.currentPackageName = loadPackageParam.packageName;
	        Log.d(TAG, "Hooking DefaultHTTPClient for: " + this.currentPackageName);
	        XposedHelpers.findAndHookConstructor(DefaultHttpClient.class, new Object[]{new XC_MethodHook() {
	            /* class just.trust.me.Main.AnonymousClass1 */
	
	            /* access modifiers changed from: protected */
	            public void afterHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                XposedHelpers.setObjectField(methodHookParam.thisObject, "defaultParams", (Object) null);
	                XposedHelpers.setObjectField(methodHookParam.thisObject, "connManager", Main.this.getSCCM());
	            }
	        }});
	        Log.d(TAG, "Hooking DefaultHTTPClient(HttpParams) for: " + this.currentPackageName);
	        XposedHelpers.findAndHookConstructor(DefaultHttpClient.class, new Object[]{HttpParams.class, new XC_MethodHook() {
	            /* class just.trust.me.Main.AnonymousClass2 */
	
	            /* access modifiers changed from: protected */
	            public void afterHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                XposedHelpers.setObjectField(methodHookParam.thisObject, "defaultParams", (HttpParams) methodHookParam.args[0]);
	                XposedHelpers.setObjectField(methodHookParam.thisObject, "connManager", Main.this.getSCCM());
	            }
	        }});
	        Log.d(TAG, "Hooking DefaultHTTPClient(ClientConnectionManager, HttpParams) for: " + this.currentPackageName);
	        XposedHelpers.findAndHookConstructor(DefaultHttpClient.class, new Object[]{ClientConnectionManager.class, HttpParams.class, new XC_MethodHook() {
	            /* class just.trust.me.Main.AnonymousClass3 */
	
	            /* access modifiers changed from: protected */
	            public void afterHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                HttpParams httpParams = (HttpParams) methodHookParam.args[1];
	                XposedHelpers.setObjectField(methodHookParam.thisObject, "defaultParams", httpParams);
	                XposedHelpers.setObjectField(methodHookParam.thisObject, "connManager", Main.this.getCCM(methodHookParam.args[0], httpParams));
	            }
	        }});
	        Log.d(TAG, "Hooking SSLSocketFactory(String, KeyStore, String, KeyStore) for: " + this.currentPackageName);
	        XposedHelpers.findAndHookConstructor(SSLSocketFactory.class, new Object[]{String.class, KeyStore.class, String.class, KeyStore.class, SecureRandom.class, HostNameResolver.class, new XC_MethodHook() {
	            /* class just.trust.me.Main.AnonymousClass4 */
	
	            /* access modifiers changed from: protected */
	            public void afterHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                KeyManager[] keyManagerArr;
	                String str = (String) methodHookParam.args[0];
	                KeyStore keyStore = (KeyStore) methodHookParam.args[1];
	                String str2 = (String) methodHookParam.args[2];
	                SecureRandom secureRandom = (SecureRandom) methodHookParam.args[4];
	                if (keyStore != null) {
	                    keyManagerArr = (KeyManager[]) XposedHelpers.callStaticMethod(SSLSocketFactory.class, "createKeyManagers", new Object[]{keyStore, str2});
	                } else {
	                    keyManagerArr = null;
	                }
	                TrustManager[] trustManagerArr = {new ImSureItsLegitTrustManager()};
	                XposedHelpers.setObjectField(methodHookParam.thisObject, "sslcontext", SSLContext.getInstance(str));
	                XposedHelpers.callMethod(XposedHelpers.getObjectField(methodHookParam.thisObject, "sslcontext"), "init", new Object[]{keyManagerArr, trustManagerArr, secureRandom});
	                XposedHelpers.setObjectField(methodHookParam.thisObject, "socketfactory", XposedHelpers.callMethod(XposedHelpers.getObjectField(methodHookParam.thisObject, "sslcontext"), "getSocketFactory", new Object[0]));
	            }
	        }});
	        Log.d(TAG, "Hooking static SSLSocketFactory(String, KeyStore, String, KeyStore) for: " + this.currentPackageName);
	        XposedHelpers.findAndHookMethod("org.apache.http.conn.ssl.SSLSocketFactory", loadPackageParam.classLoader, "getSocketFactory", new Object[]{new XC_MethodReplacement() {
	            /* class just.trust.me.Main.AnonymousClass5 */
	
	            /* access modifiers changed from: protected */
	            public Object replaceHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                return (SSLSocketFactory) XposedHelpers.newInstance(SSLSocketFactory.class, new Object[0]);
	            }
	        }});
	        Log.d(TAG, "Hooking SSLSocketFactory(Socket) for: " + this.currentPackageName);
	        XposedHelpers.findAndHookMethod("org.apache.http.conn.ssl.SSLSocketFactory", loadPackageParam.classLoader, "isSecure", new Object[]{Socket.class, new XC_MethodReplacement() {
	            /* class just.trust.me.Main.AnonymousClass6 */
	
	            /* access modifiers changed from: protected */
	            public Object replaceHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                return true;
	            }
	        }});
	        Log.d(TAG, "Hooking TrustManagerFactory.getTrustManagers() for: " + this.currentPackageName);
	        XposedHelpers.findAndHookMethod("javax.net.ssl.TrustManagerFactory", loadPackageParam.classLoader, "getTrustManagers", new Object[]{new XC_MethodHook() {
	            /* class just.trust.me.Main.AnonymousClass7 */
	
	            /* access modifiers changed from: protected */
	            public void afterHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                if (Main.this.hasTrustManagerImpl()) {
	                    Class findClass = XposedHelpers.findClass("com.android.org.conscrypt.TrustManagerImpl", loadPackageParam.classLoader);
	                    TrustManager[] trustManagerArr = (TrustManager[]) methodHookParam.getResult();
	                    if (trustManagerArr.length > 0 && findClass.isInstance(trustManagerArr[0])) {
	                        return;
	                    }
	                }
	                methodHookParam.setResult(new TrustManager[]{new ImSureItsLegitTrustManager()});
	            }
	        }});
	        Log.d(TAG, "Hooking HttpsURLConnection.setDefaultHostnameVerifier for: " + this.currentPackageName);
	        XposedHelpers.findAndHookMethod("javax.net.ssl.HttpsURLConnection", loadPackageParam.classLoader, "setDefaultHostnameVerifier", new Object[]{HostnameVerifier.class, new XC_MethodReplacement() {
	            /* class just.trust.me.Main.AnonymousClass8 */
	
	            /* access modifiers changed from: protected */
	            public Object replaceHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                return null;
	            }
	        }});
	        Log.d(TAG, "Hooking HttpsURLConnection.setSSLSocketFactory for: " + this.currentPackageName);
	        XposedHelpers.findAndHookMethod("javax.net.ssl.HttpsURLConnection", loadPackageParam.classLoader, "setSSLSocketFactory", new Object[]{javax.net.ssl.SSLSocketFactory.class, new XC_MethodReplacement() {
	            /* class just.trust.me.Main.AnonymousClass9 */
	
	            /* access modifiers changed from: protected */
	            public Object replaceHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                return null;
	            }
	        }});
	        Log.d(TAG, "Hooking HttpsURLConnection.setHostnameVerifier for: " + this.currentPackageName);
	        XposedHelpers.findAndHookMethod("javax.net.ssl.HttpsURLConnection", loadPackageParam.classLoader, "setHostnameVerifier", new Object[]{HostnameVerifier.class, new XC_MethodReplacement() {
	            /* class just.trust.me.Main.AnonymousClass10 */
	
	            /* access modifiers changed from: protected */
	            public Object replaceHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                return null;
	            }
	        }});
	        Log.d(TAG, "Hooking WebViewClient.onReceivedSslError(WebView, SslErrorHandler, SslError) for: " + this.currentPackageName);
	        XposedHelpers.findAndHookMethod("android.webkit.WebViewClient", loadPackageParam.classLoader, "onReceivedSslError", new Object[]{WebView.class, SslErrorHandler.class, SslError.class, new XC_MethodReplacement() {
	            /* class just.trust.me.Main.AnonymousClass11 */
	
	            /* access modifiers changed from: protected */
	            public Object replaceHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                ((SslErrorHandler) methodHookParam.args[1]).proceed();
	                return null;
	            }
	        }});
	        Log.d(TAG, "Hooking WebViewClient.onReceivedSslError(WebView, int, string, string) for: " + this.currentPackageName);
	        XposedHelpers.findAndHookMethod("android.webkit.WebViewClient", loadPackageParam.classLoader, "onReceivedError", new Object[]{WebView.class, Integer.TYPE, String.class, String.class, new XC_MethodReplacement() {
	            /* class just.trust.me.Main.AnonymousClass12 */
	
	            /* access modifiers changed from: protected */
	            public Object replaceHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                return null;
	            }
	        }});
	        XposedHelpers.findAndHookMethod("javax.net.ssl.SSLContext", loadPackageParam.classLoader, "init", new Object[]{KeyManager[].class, TrustManager[].class, SecureRandom.class, new XC_MethodHook() {
	            /* class just.trust.me.Main.AnonymousClass13 */
	
	            /* access modifiers changed from: protected */
	            public void beforeHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                methodHookParam.args[0] = null;
	                methodHookParam.args[1] = new TrustManager[]{new ImSureItsLegitTrustManager()};
	                methodHookParam.args[2] = null;
	            }
	        }});
	        XposedHelpers.findAndHookMethod("android.app.Application", loadPackageParam.classLoader, "attach", new Object[]{Context.class, new XC_MethodHook() {
	            /* class just.trust.me.Main.AnonymousClass14 */
	
	            /* access modifiers changed from: protected */
	            public void afterHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                Context context = (Context) methodHookParam.args[0];
	                Main.this.processOkHttp(context.getClassLoader());
	                Main.this.processHttpClientAndroidLib(context.getClassLoader());
	                Main.this.processXutils(context.getClassLoader());
	            }
	        }});
	        if (hasTrustManagerImpl()) {
	            Log.d(TAG, "Hooking com.android.org.conscrypt.TrustManagerImpl for: " + this.currentPackageName);
	            XposedHelpers.findAndHookMethod("com.android.org.conscrypt.TrustManagerImpl", loadPackageParam.classLoader, "checkServerTrusted", new Object[]{X509Certificate[].class, String.class, new XC_MethodReplacement() {
	                /* class just.trust.me.Main.AnonymousClass15 */
	
	                /* access modifiers changed from: protected */
	                public Object replaceHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                    return 0;
	                }
	            }});
	            XposedHelpers.findAndHookMethod("com.android.org.conscrypt.TrustManagerImpl", loadPackageParam.classLoader, "checkServerTrusted", new Object[]{X509Certificate[].class, String.class, String.class, new XC_MethodReplacement() {
	                /* class just.trust.me.Main.AnonymousClass16 */
	
	                /* access modifiers changed from: protected */
	                public Object replaceHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                    return new ArrayList();
	                }
	            }});
	            XposedHelpers.findAndHookMethod("com.android.org.conscrypt.TrustManagerImpl", loadPackageParam.classLoader, "checkServerTrusted", new Object[]{X509Certificate[].class, String.class, SSLSession.class, new XC_MethodReplacement() {
	                /* class just.trust.me.Main.AnonymousClass17 */
	
	                /* access modifiers changed from: protected */
	                public Object replaceHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                    return new ArrayList();
	                }
	            }});
	        }
	    }
	
	    public boolean hasTrustManagerImpl() {
	        try {
	            Class.forName("com.android.org.conscrypt.TrustManagerImpl");
	            return true;
	        } catch (ClassNotFoundException unused) {
	            return false;
	        }
	    }
	
	    /* access modifiers changed from: private */
	    /* access modifiers changed from: public */
	    private javax.net.ssl.SSLSocketFactory getEmptySSLFactory() {
	        try {
	            SSLContext instance = SSLContext.getInstance("TLS");
	            instance.init(null, new TrustManager[]{new ImSureItsLegitTrustManager()}, null);
	            return instance.getSocketFactory();
	        } catch (KeyManagementException | NoSuchAlgorithmException unused) {
	            return null;
	        }
	    }
	
	    public ClientConnectionManager getSCCM() {
	        try {
	            KeyStore instance = KeyStore.getInstance(KeyStore.getDefaultType());
	            instance.load(null, null);
	            TrustAllSSLSocketFactory trustAllSSLSocketFactory = new TrustAllSSLSocketFactory(instance);
	            trustAllSSLSocketFactory.setHostnameVerifier(SSLSocketFactory.ALLOW_ALL_HOSTNAME_VERIFIER);
	            SchemeRegistry schemeRegistry = new SchemeRegistry();
	            schemeRegistry.register(new Scheme("http", PlainSocketFactory.getSocketFactory(), 80));
	            schemeRegistry.register(new Scheme("https", trustAllSSLSocketFactory, 443));
	            return new SingleClientConnManager((HttpParams) null, schemeRegistry);
	        } catch (Exception unused) {
	            return null;
	        }
	    }
	
	    public ClientConnectionManager getTSCCM(HttpParams httpParams) {
	        try {
	            KeyStore instance = KeyStore.getInstance(KeyStore.getDefaultType());
	            instance.load(null, null);
	            TrustAllSSLSocketFactory trustAllSSLSocketFactory = new TrustAllSSLSocketFactory(instance);
	            trustAllSSLSocketFactory.setHostnameVerifier(SSLSocketFactory.ALLOW_ALL_HOSTNAME_VERIFIER);
	            SchemeRegistry schemeRegistry = new SchemeRegistry();
	            schemeRegistry.register(new Scheme("http", PlainSocketFactory.getSocketFactory(), 80));
	            schemeRegistry.register(new Scheme("https", trustAllSSLSocketFactory, 443));
	            return new ThreadSafeClientConnManager(httpParams, schemeRegistry);
	        } catch (Exception unused) {
	            return null;
	        }
	    }
	
	    public ClientConnectionManager getCCM(Object obj, HttpParams httpParams) {
	        String simpleName = obj.getClass().getSimpleName();
	        if (simpleName.equals("SingleClientConnManager")) {
	            return getSCCM();
	        }
	        if (simpleName.equals("ThreadSafeClientConnManager")) {
	            return getTSCCM(httpParams);
	        }
	        return null;
	    }
	
	    /* access modifiers changed from: private */
	    /* access modifiers changed from: public */
	    private void processXutils(ClassLoader classLoader) {
	        Log.d(TAG, "Hooking org.xutils.http.RequestParams.setSslSocketFactory(SSLSocketFactory) (3) for: " + this.currentPackageName);
	        try {
	            classLoader.loadClass("org.xutils.http.RequestParams");
	            XposedHelpers.findAndHookMethod("org.xutils.http.RequestParams", classLoader, "setSslSocketFactory", new Object[]{javax.net.ssl.SSLSocketFactory.class, new XC_MethodHook() {
	                /* class just.trust.me.Main.AnonymousClass18 */
	
	                /* access modifiers changed from: protected */
	                public void beforeHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                    Main.super.beforeHookedMethod(methodHookParam);
	                    methodHookParam.args[0] = Main.this.getEmptySSLFactory();
	                }
	            }});
	            XposedHelpers.findAndHookMethod("org.xutils.http.RequestParams", classLoader, "setHostnameVerifier", new Object[]{HostnameVerifier.class, new XC_MethodHook() {
	                /* class just.trust.me.Main.AnonymousClass19 */
	
	                /* access modifiers changed from: protected */
	                public void beforeHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                    Main.super.beforeHookedMethod(methodHookParam);
	                    methodHookParam.args[0] = new ImSureItsLegitHostnameVerifier();
	                }
	            }});
	        } catch (Exception unused) {
	            Log.d(TAG, "org.xutils.http.RequestParams not found in " + this.currentPackageName + "-- not hooking");
	        }
	    }
	
	    /* access modifiers changed from: package-private */
	    public void processOkHttp(ClassLoader classLoader) {
	        Log.d(TAG, "Hooking com.squareup.okhttp.CertificatePinner.check(String,List) (2.5) for: " + this.currentPackageName);
	        try {
	            classLoader.loadClass("com.squareup.okhttp.CertificatePinner");
	            XposedHelpers.findAndHookMethod("com.squareup.okhttp.CertificatePinner", classLoader, "check", new Object[]{String.class, List.class, new XC_MethodReplacement() {
	                /* class just.trust.me.Main.AnonymousClass20 */
	
	                /* access modifiers changed from: protected */
	                public Object replaceHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                    return true;
	                }
	            }});
	        } catch (ClassNotFoundException unused) {
	            Log.d(TAG, "OKHTTP 2.5 not found in " + this.currentPackageName + "-- not hooking");
	        }
	        Log.d(TAG, "Hooking okhttp3.CertificatePinner.check(String,List) (3.x) for: " + this.currentPackageName);
	        try {
	            classLoader.loadClass("okhttp3.CertificatePinner");
	            XposedHelpers.findAndHookMethod("okhttp3.CertificatePinner", classLoader, "check", new Object[]{String.class, List.class, new XC_MethodReplacement() {
	                /* class just.trust.me.Main.AnonymousClass21 */
	
	                /* access modifiers changed from: protected */
	                public Object replaceHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                    return null;
	                }
	            }});
	        } catch (ClassNotFoundException unused2) {
	            Log.d(TAG, "OKHTTP 3.x not found in " + this.currentPackageName + " -- not hooking");
	        }
	        try {
	            classLoader.loadClass("okhttp3.internal.tls.OkHostnameVerifier");
	            XposedHelpers.findAndHookMethod("okhttp3.internal.tls.OkHostnameVerifier", classLoader, "verify", new Object[]{String.class, SSLSession.class, new XC_MethodReplacement() {
	                /* class just.trust.me.Main.AnonymousClass22 */
	
	                /* access modifiers changed from: protected */
	                public Object replaceHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                    return true;
	                }
	            }});
	        } catch (ClassNotFoundException unused3) {
	            Log.d(TAG, "OKHTTP 3.x not found in " + this.currentPackageName + " -- not hooking OkHostnameVerifier.verify(String, SSLSession)");
	        }
	        try {
	            classLoader.loadClass("okhttp3.internal.tls.OkHostnameVerifier");
	            XposedHelpers.findAndHookMethod("okhttp3.internal.tls.OkHostnameVerifier", classLoader, "verify", new Object[]{String.class, X509Certificate.class, new XC_MethodReplacement() {
	                /* class just.trust.me.Main.AnonymousClass23 */
	
	                /* access modifiers changed from: protected */
	                public Object replaceHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                    return true;
	                }
	            }});
	        } catch (ClassNotFoundException unused4) {
	            Log.d(TAG, "OKHTTP 3.x not found in " + this.currentPackageName + " -- not hooking OkHostnameVerifier.verify(String, X509)(");
	        }
	    }
	
	    /* access modifiers changed from: package-private */
	    public void processHttpClientAndroidLib(ClassLoader classLoader) {
	        Log.d(TAG, "Hooking AbstractVerifier.verify(String, String[], String[], boolean) for: " + this.currentPackageName);
	        try {
	            classLoader.loadClass("ch.boye.httpclientandroidlib.conn.ssl.AbstractVerifier");
	            XposedHelpers.findAndHookMethod("ch.boye.httpclientandroidlib.conn.ssl.AbstractVerifier", classLoader, "verify", new Object[]{String.class, String[].class, String[].class, Boolean.TYPE, new XC_MethodReplacement() {
	                /* class just.trust.me.Main.AnonymousClass24 */
	
	                /* access modifiers changed from: protected */
	                public Object replaceHookedMethod(XC_MethodHook.MethodHookParam methodHookParam) throws Throwable {
	                    return null;
	                }
	            }});
	        } catch (ClassNotFoundException unused) {
	            Log.d(TAG, "httpclientandroidlib not found in " + this.currentPackageName + "-- not hooking");
	        }
	    }
	
	    /* access modifiers changed from: private */
	    public class ImSureItsLegitTrustManager implements X509TrustManager {
	        @Override // javax.net.ssl.X509TrustManager
	        public void checkClientTrusted(X509Certificate[] x509CertificateArr, String str) throws CertificateException {
	        }
	
	        @Override // javax.net.ssl.X509TrustManager
	        public void checkServerTrusted(X509Certificate[] x509CertificateArr, String str) throws CertificateException {
	        }
	
	        private ImSureItsLegitTrustManager() {
	        }
	
	        public List<X509Certificate> checkServerTrusted(X509Certificate[] x509CertificateArr, String str, String str2) throws CertificateException {
	            return new ArrayList();
	        }
	
	        public X509Certificate[] getAcceptedIssuers() {
	            return new X509Certificate[0];
	        }
	    }
	
	    private class ImSureItsLegitHostnameVerifier implements HostnameVerifier {
	        public boolean verify(String str, SSLSession sSLSession) {
	            return true;
	        }
	
	        private ImSureItsLegitHostnameVerifier() {
	        }
	    }
	
	    public class TrustAllSSLSocketFactory extends SSLSocketFactory {
	        SSLContext sslContext = SSLContext.getInstance("TLS");
	
	        public TrustAllSSLSocketFactory(KeyStore keyStore) throws NoSuchAlgorithmException, KeyManagementException, KeyStoreException, UnrecoverableKeyException {
	            super(keyStore);
	            AnonymousClass1 r4 = new X509TrustManager(Main.this) {
	                /* class just.trust.me.Main.TrustAllSSLSocketFactory.AnonymousClass1 */
	
	                @Override // javax.net.ssl.X509TrustManager
	                public void checkClientTrusted(X509Certificate[] x509CertificateArr, String str) throws CertificateException {
	                }
	
	                @Override // javax.net.ssl.X509TrustManager
	                public void checkServerTrusted(X509Certificate[] x509CertificateArr, String str) throws CertificateException {
	                }
	
	                public X509Certificate[] getAcceptedIssuers() {
	                    return null;
	                }
	            };
	            this.sslContext.init(null, new TrustManager[]{r4}, null);
	        }
	
	        @Override // org.apache.http.conn.scheme.LayeredSocketFactory, org.apache.http.conn.ssl.SSLSocketFactory
	        public Socket createSocket(Socket socket, String str, int i, boolean z) throws IOException, UnknownHostException {
	            return this.sslContext.getSocketFactory().createSocket(socket, str, i, z);
	        }
	
	        @Override // org.apache.http.conn.scheme.SocketFactory, org.apache.http.conn.ssl.SSLSocketFactory
	        public Socket createSocket() throws IOException {
	            return this.sslContext.getSocketFactory().createSocket();
	        }
	    }
	}           
	
	
	
破解的关键在于破解pin的机制，要么是设置的地方，要么是系统生效的地方	    