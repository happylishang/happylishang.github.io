---
layout: post
title: "Android Context意义与实现"
description: "Android"
categories: [Android]

---

关于Context，首先看一下官方的说法：

Interface to global information about an application environment. This is an abstract class whose implementation is provided by the Android system. It allows access to application-specific resources and classes, as well as up-calls for application-level operations such as launching activities, broadcasting and receiving intents, etc

	   Resources getTopLevelResources(String resDir, String[] splitResDirs,
	            String[] overlayDirs, String[] libDirs, int displayId,
	            Configuration overrideConfiguration, CompatibilityInfo compatInfo) {
	        final float scale = compatInfo.applicationScale;
	        Configuration overrideConfigCopy = (overrideConfiguration != null)
	                ? new Configuration(overrideConfiguration) : null;
	        ResourcesKey key = new ResourcesKey(resDir, displayId, overrideConfigCopy, scale);
	        Resources r;
	        synchronized (this) {
	            // Resources is app scale dependent.
	            if (DEBUG) Slog.w(TAG, "getTopLevelResources: " + resDir + " / " + scale);
	
	            WeakReference<Resources> wr = mActiveResources.get(key);
	            r = wr != null ? wr.get() : null;
	            //if (r != null) Log.i(TAG, "isUpToDate " + resDir + ": " + r.getAssets().isUpToDate());
	            if (r != null && r.getAssets().isUpToDate()) {
	                if (DEBUG) Slog.w(TAG, "Returning cached resources " + r + " " + resDir
	                        + ": appScale=" + r.getCompatibilityInfo().applicationScale
	                        + " key=" + key + " overrideConfig=" + overrideConfiguration);
	                return r;
	            }
	        }
	
	        //if (r != null) {
	        //    Log.w(TAG, "Throwing away out-of-date resources!!!! "
	        //            + r + " " + resDir);
	        //}
	
	        AssetManager assets = new AssetManager();
	        // resDir can be null if the 'android' package is creating a new Resources object.
	        // This is fine, since each AssetManager automatically loads the 'android' package
	        // already.
	        if (resDir != null) {
	            if (assets.addAssetPath(resDir) == 0) {
	                return null;
	            }
	        }
	
	        if (splitResDirs != null) {
	            for (String splitResDir : splitResDirs) {
	                if (assets.addAssetPath(splitResDir) == 0) {
	                    return null;
	                }
	            }
	        }
	
	        if (overlayDirs != null) {
	            for (String idmapPath : overlayDirs) {
	                assets.addOverlayPath(idmapPath);
	            }
	        }
	
	        if (libDirs != null) {
	            for (String libDir : libDirs) {
	                if (libDir.endsWith(".apk")) {
	                    // Avoid opening files we know do not have resources,
	                    // like code-only .jar files.
	                    if (assets.addAssetPath(libDir) == 0) {
	                        Log.w(TAG, "Asset path '" + libDir +
	                                "' does not exist or contains no resources.");
	                    }
	                }
	            }
	        }
	
	        //Log.i(TAG, "Resource: key=" + key + ", display metrics=" + metrics);
	        DisplayMetrics dm = getDisplayMetricsLocked(displayId);
	        Configuration config;
	        final boolean isDefaultDisplay = (displayId == Display.DEFAULT_DISPLAY);
	        final boolean hasOverrideConfig = key.hasOverrideConfiguration();
	        if (!isDefaultDisplay || hasOverrideConfig) {
	            config = new Configuration(getConfiguration());
	            if (!isDefaultDisplay) {
	                applyNonDefaultDisplayMetricsToConfigurationLocked(dm, config);
	            }
	            if (hasOverrideConfig) {
	                config.updateFrom(key.mOverrideConfiguration);
	                if (DEBUG) Slog.v(TAG, "Applied overrideConfig=" + key.mOverrideConfiguration);
	            }
	        } else {
	            config = getConfiguration();
	        }
	        r = new Resources(assets, dm, config, compatInfo);
	        if (DEBUG) Slog.i(TAG, "Created app resources " + resDir + " " + r + ": "
	                + r.getConfiguration() + " appScale=" + r.getCompatibilityInfo().applicationScale);
	
	        synchronized (this) {
	            WeakReference<Resources> wr = mActiveResources.get(key);
	            Resources existing = wr != null ? wr.get() : null;
	            if (existing != null && existing.getAssets().isUpToDate()) {
	                // Someone else already created the resources while we were
	                // unlocked; go ahead and use theirs.
	                r.getAssets().close();
	                return existing;
	            }
	
	            // XXX need to remove entries when weak references go away
	            mActiveResources.put(key, new WeakReference<>(r));
	            if (DEBUG) Slog.v(TAG, "mActiveResources.size()=" + mActiveResources.size());
	            return r;
	        }
	    }


如何获取资源

	    @ColorInt
	    public int getColor(@ColorRes int id, @Nullable Theme theme) throws NotFoundException {
	        TypedValue value;
	        synchronized (mAccessLock) {
	            value = mTmpValue;
	            if (value == null) {
	                value = new TypedValue();
	            }
	            getValue(id, value, true);
	            if (value.type >= TypedValue.TYPE_FIRST_INT
	                    && value.type <= TypedValue.TYPE_LAST_INT) {
	                mTmpValue = value;
	                return value.data;
	            } else if (value.type != TypedValue.TYPE_STRING) {
	                throw new NotFoundException(
	                        "Resource ID #0x" + Integer.toHexString(id) + " type #0x"
	                                + Integer.toHexString(value.type) + " is not valid");
	            }
	            mTmpValue = null;
	        }
	        final ColorStateList csl = loadColorStateList(value, id, theme);
	        synchronized (mAccessLock) {
	            if (mTmpValue == null) {
	                mTmpValue = value;
	            }
	        }
	
	        return csl.getDefaultColor();
	    }
	    
	        public void getValue(@AnyRes int id, TypedValue outValue, boolean resolveRefs)
            throws NotFoundException {
        boolean found = mAssets.getResourceValue(id, 0, outValue, resolveRefs);
        if (found) {
            return;
        }
        throw new NotFoundException("Resource ID #0x"
                                    + Integer.toHexString(id));
    }
    
 继续调用
   
	   final boolean getResourceValue(int ident, int density, TypedValue outValue,   boolean resolveRefs)
	    {
	        int block = loadResourceValue(ident, (short) density, outValue, resolveRefs);
	        if (block >= 0) {
	            if (outValue.type != TypedValue.TYPE_STRING) {
	                return true;
	            }
	            outValue.string = mStringBlocks[block].get(outValue.data);
	            return true;
	        }
	        return false;
	    }
	    
	    
 loadResourceValue是native函数
 
	 
	static jint android_content_AssetManager_loadResourceValue(JNIEnv* env, jobject clazz,
	                                                           jint ident,
	                                                           jshort density,
	                                                           jobject outValue,
	                                                           jboolean resolve)
	{
	    if (outValue == NULL) {
	         jniThrowNullPointerException(env, "outValue");
	         return NULL;
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
	#if THROW_ON_BAD_ID
	    if (block == BAD_INDEX) {
	        jniThrowException(env, "java/lang/IllegalStateException", "Bad resource!");
	        return 0;
	    }
	#endif
	    uint32_t ref = ident;
	    if (resolve) {
	        block = res.resolveReference(&value, block, &ref, &typeSpecFlags, &config);
	#if THROW_ON_BAD_ID
	        if (block == BAD_INDEX) {
	            jniThrowException(env, "java/lang/IllegalStateException", "Bad resource!");
	            return 0;
	        }
	#endif
	    }
	    return block >= 0 ? copyValue(env, outValue, &res, value, ref, block, typeSpecFlags, &config) : block;
	}
	
	
	
		
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
	
	    if (mCacheMode != CACHE_OFF && !mCacheValid)
	        const_cast<AssetManager*>(this)->loadFileNameCacheLocked();
	
	    const size_t N = mAssetPaths.size();
	    for (size_t i=0; i<N; i++) {
	        Asset* ass = NULL;
	        ResTable* sharedRes = NULL;
	        bool shared = true;
	        const asset_path& ap = mAssetPaths.itemAt(i);
	        MY_TRACE_BEGIN(ap.path.string());
	        Asset* idmap = openIdmapLocked(ap);
	        ALOGV("Looking for resource asset in '%s'\n", ap.path.string());
	        if (ap.type != kFileTypeDirectory) {
	            if (i == 0) {
	                // The first item is typically the framework resources,
	                // which we want to avoid parsing every time.
	                sharedRes = const_cast<AssetManager*>(this)->
	                    mZipSet.getZipResourceTable(ap.path);
	            }
	            if (sharedRes == NULL) {
	                ass = const_cast<AssetManager*>(this)->
	                    mZipSet.getZipResourceTableAsset(ap.path);
	                if (ass == NULL) {
	                    ALOGV("loading resource table %s\n", ap.path.string());
	                    ass = const_cast<AssetManager*>(this)->
	                        openNonAssetInPathLocked("resources.arsc",
	                                                 Asset::ACCESS_BUFFER,
	                                                 ap);
	                    if (ass != NULL && ass != kExcludedAsset) {
	                        ass = const_cast<AssetManager*>(this)->
	                            mZipSet.setZipResourceTableAsset(ap.path, ass);
	                    }
	                }
	                
	                if (i == 0 && ass != NULL) {
	                    // If this is the first resource table in the asset
	                    // manager, then we are going to cache it so that we
	                    // can quickly copy it out for others.
	                    ALOGV("Creating shared resources for %s", ap.path.string());
	                    sharedRes = new ResTable();
	                    sharedRes->add(ass, (void*)(i+1), false, idmap);
	                    sharedRes = const_cast<AssetManager*>(this)->
	                        mZipSet.setZipResourceTable(ap.path, sharedRes);
	                }
	            }
	        } else {
	            ALOGV("loading resource table %s\n", ap.path.string());
	            Asset* ass = const_cast<AssetManager*>(this)->
	                openNonAssetInPathLocked("resources.arsc",
	                                         Asset::ACCESS_BUFFER,
	                                         ap);
	            shared = false;
	        }
	        if ((ass != NULL || sharedRes != NULL) && ass != kExcludedAsset) {
	            if (rt == NULL) {
	                mResources = rt = new ResTable();
	                updateResourceParamsLocked();
	            }
	            ALOGV("Installing resource asset %p in to table %p\n", ass, mResources);
	            if (sharedRes != NULL) {
	                ALOGV("Copying existing resources for %s", ap.path.string());
	                rt->add(sharedRes);
	            } else {
	                ALOGV("Parsing resources for %s", ap.path.string());
	                rt->add(ass, (void*)(i+1), !shared, idmap);
	            }
	
	            if (!shared) {
	                delete ass;
	            }
	        }
	        if (idmap != NULL) {
	            delete idmap;
	        }
	        MY_TRACE_END();
	    }
	
	    if (required && !rt) ALOGW("Unable to find resources file resources.arsc");
	    if (!rt) {
	        mResources = rt = new ResTable();
	    }
	    return rt;
	}

资源加载的话，会将全部资源加载？
