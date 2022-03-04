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