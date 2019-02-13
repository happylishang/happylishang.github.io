> 换肤的本质：动态更新UI资源

目前基本都是AppcompatActivity

    
![image.png](https://upload-images.jianshu.io/upload_images/1460468-b72993685e23d28b.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)   
   
![image.png](https://upload-images.jianshu.io/upload_images/1460468-cdc8872a0d4363c4.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

最终调用的其实都是setFactory2

![image.png](https://upload-images.jianshu.io/upload_images/1460468-1025e70bf28396b7.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

真正的实现类是Factory2Wrapper


![image.png](https://upload-images.jianshu.io/upload_images/1460468-08cbd8413b8535ab.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

AppCompatDelegateImplV7实现了LayoutInflaterFactory接口，setFactory2传递的参数是AppCompatDelegateImplV7自己：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-f1cefd131dd64483.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


![image.png](https://upload-images.jianshu.io/upload_images/1460468-81fbae2a70c6704a.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![image.png](https://upload-images.jianshu.io/upload_images/1460468-8c62a964e8223388.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到，Google现在把很多逻辑都留给了兼容库，这样方便更新一些新的特性，这样是为什么推荐直接用AppCompatEditText，AppCompatTextView，因为就算你不写，底层也会帮你转成相应的AppCompatXXX。

# Application提供Activity的LifeCycle回调

加载资源后，统一apply，加载之前的先apply一遍，无效，加载后的，apply立刻生效，

不用非得重启生效，第一次下载后，直接apply也可以生效，只不过可能有个变化而已。

为什么Factory2不能合理创建View，它只能创建一些AppCompat类的基础View，其他的不行，所以经常null，最后调用的还是Layoutinflate的createViewFromTag，所以对于自定义的View，如果想要Hook，并达到换肤的目的，需要重写部分，Factory2就不是换肤的关键了，它只是一个Hook点，提供还如的关键入口与拦截，，Factory2主要

# 资源加载

关键点，资源加载，如何实现动态资源加载

并且，id不重复，资源的加载可是通过路径来实现的呢，用原来的也可以啊，只是路径问题

# LoadApk 与 Assetmanager分析

![image.png](https://upload-images.jianshu.io/upload_images/1460468-ff857ac1f6357929.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


![image.png](https://upload-images.jianshu.io/upload_images/1460468-b45f27f6bbcbe9b7.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![image.png](https://upload-images.jianshu.io/upload_images/1460468-2c8d0be820c2c36b.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到，主Apk的资源是在启动之初就已经设定进去了，之后利用LoadApk新建Resources对象，是个单例

![image.png](https://upload-images.jianshu.io/upload_images/1460468-9592aa6116154cbb.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![image.png](https://upload-images.jianshu.io/upload_images/1460468-44feae335f969598.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![image.png](https://upload-images.jianshu.io/upload_images/1460468-03a28cf88b7de103.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![image.png](https://upload-images.jianshu.io/upload_images/1460468-43e9a2df00c5153b.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

后面App中默认使用的Resources基本全是这个，可以看到，APP有唯一的Resources对象及AssetManager，这里先不考虑系统Resources跟Assetmanager.

![image.png](https://upload-images.jianshu.io/upload_images/1460468-36092001e62c767c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

而资源路径等等也都被初始化了，资源加载的时候，其实也是用这些路径来找文件的。

![image.png](https://upload-images.jianshu.io/upload_images/1460468-ae1e9c05dc015a25.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

    /**
     * Retrieve the string value associated with a particular resource
     * identifier for the current configuration / skin.
     */
    /*package*/ final CharSequence getResourceText(int ident) {
        synchronized (this) {
            TypedValue tmpValue = mValue;
            int block = loadResourceValue(ident, (short) 0, tmpValue, true);
            if (block >= 0) {
                if (tmpValue.type == TypedValue.TYPE_STRING) {
                    return mStringBlocks[block].get(tmpValue.data);
                }
                return tmpValue.coerceToString();
            }
        }
        return null;
    }

loadResourceValue是native函数：

	static jint android_content_AssetManager_loadResourceValue(JNIEnv* env, jobject clazz,
	                                                           jint ident,
	                                                           jshort density,
	                                                           jobject outValue,
	                                                           jboolean resolve)
	{
	    if (outValue == NULL) {
	         jniThrowNullPointerException(env, "outValue");
	         return 0;
	    }
	    AssetManager* am = assetManagerForJavaObject(env, clazz);
	    if (am == NULL) {
	        return 0;
	    }
	    const ResTable& res(am->getResources());
	
	    Res_value value;
	    ResTable_config config;
	    uint32_t typeSpecFlags;
	    ssize_t block = res.getResource(ident, &value, false, density, &typeSpecFlags, &config);
	    if (kThrowOnBadId) {
	        if (block == BAD_INDEX) {
	            jniThrowException(env, "java/lang/IllegalStateException", "Bad resource!");
	            return 0;
	        }
	    }
	    uint32_t ref = ident;
	    if (resolve) {
	        block = res.resolveReference(&value, block, &ref, &typeSpecFlags, &config);
	        if (kThrowOnBadId) {
	            if (block == BAD_INDEX) {
	                jniThrowException(env, "java/lang/IllegalStateException", "Bad resource!");
	                return 0;
	            }
	        }
	    }
	    if (block >= 0) {
	        return copyValue(env, outValue, &res, value, ref, block, typeSpecFlags, &config);
	    }
	
	    return static_cast<jint>(block);
	}

ResTable中去找，ResTable单例

	const ResTable* AssetManager::getResTable(bool required) const
	{
	    ResTable* rt = mResources;
	    if (rt) {
	        return rt;
	    }
	
	    // Iterate through all asset packages, collecting resources from each.
	
	    AutoMutex _l(mLock);
	
	    if (mResources != NULL) {
	        return mResources;
	    }
	
	    if (required) {
	        LOG_FATAL_IF(mAssetPaths.size() == 0, "No assets added to AssetManager");
	    }
	
	    if (mCacheMode != CACHE_OFF && !mCacheValid) {
	        const_cast<AssetManager*>(this)->loadFileNameCacheLocked();
	    }
	
	    mResources = new ResTable();
	    updateResourceParamsLocked();
	
	    bool onlyEmptyResources = true;
	    const size_t N = mAssetPaths.size();
	    for (size_t i=0; i<N; i++) {
	        bool empty = appendPathToResTable(mAssetPaths.itemAt(i));
	        onlyEmptyResources = onlyEmptyResources && empty;
	    }
	
	    if (required && onlyEmptyResources) {
	        ALOGW("Unable to find resources file resources.arsc");
	        delete mResources;
	        mResources = NULL;
	    }
	
	    return mResources;
	}

![image.png](https://upload-images.jianshu.io/upload_images/1460468-8af11439931838da.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# 为什么要新建AssetManager

系统的资源有自己的访问方式

    public static Resources getSystem() {
        synchronized (sSync) {
            Resources ret = mSystem;
            if (ret == null) {
                ret = new Resources();
                mSystem = ret;
            }

            return ret;
        }
    }

    private Resources() {
        mAssets = AssetManager.getSystem();
        // NOTE: Intentionally leaving this uninitialized (all values set
        // to zero), so that anyone who tries to do something that requires
        // metrics will get a very wrong value.
        mConfiguration.setToDefaults();
        mMetrics.setToDefaults();
        updateConfiguration(null, null);
        mAssets.ensureStringBlocks();
    }
}


# 参考文档

[](https://blog.csdn.net/luoshengyang/article/details/8806798)
