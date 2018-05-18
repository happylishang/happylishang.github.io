
App开发不可避免的要和图片打交道，由于其占用内存非常大，管理不当很容易导致内存不足，最后OOM，图片的背后其实是Bitmap，它是Android中最能吃内存的对象之一，也是很多OOM的元凶，不过，在不同的Android版本中，Bitmap或多或少都存在差异，尤其是在其内存分配上，了解其中的不用跟原理能更好的指导图片管。先看Google官方文档的说明：

>On Android 2.3.3 (API level 10) and lower, the backing pixel data for a Bitmap is stored in native memory. It is separate from the Bitmap itself, which is stored in the Dalvik heap. The pixel data in native memory is not released in a predictable manner, potentially causing an application to briefly exceed its memory limits and crash. From Android 3.0 (API level 11) through Android 7.1 (API level 25), the pixel data is stored on the Dalvik heap along with the associated Bitmap. In Android 8.0 (API level 26), and higher, the Bitmap pixel data is stored in the native heap.

大意就是： 2.3之前的像素存储需要的内存是在native上分配的，并且生命周期不被Bitmap控制，需要用户自己回收。  2.3-7.1之间，Bitmap的像素存储在Dalvik的Java堆上，当然，4.4之前的甚至能在匿名共享内存上分配（Fresco采用），而8.0之后的像素内存又重新回到native上去分配，**不过不需要用户主动回收**，8.0之后图像资源的管理更加优秀，极大降低了OOM。

![21526521448_.pic.jpg](https://upload-images.jianshu.io/upload_images/1460468-6650ea0137bff13a.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![51526524893_.pic.jpg](https://upload-images.jianshu.io/upload_images/1460468-d74219d2777e1f3c.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![61526525051_.pic.jpg](https://upload-images.jianshu.io/upload_images/1460468-6a3fe361dab421b4.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# 2.3.3 及以前的主动recycle（不需要考虑，过期技术等于垃圾）

Manage Memory on Android 2.3.3 and Lower
On Android 2.3.3 (API level 10) and lower, using recycle() is recommended. If you're displaying large amounts of Bitmap data in your app, you're likely to run into OutOfMemoryError errors. The recycle() method allows an app to reclaim memory as soon as possible.

Caution: You should use recycle() only when you are sure that the Bitmap is no longer being used. If you call recycle() and later attempt to draw the Bitmap, you will get the error: "Canvas: trying to use a recycled Bitmap".


* Java跟native内存的问题（如何统计）
* 占内存问题（Java Native）
* 分配问题
* 大小问题
* Bitmap概念


# Android6.0 Nexus5 native内存的增加并不会导致OOM对于8.0也是如此

![image.png](https://upload-images.jianshu.io/upload_images/1460468-eb01a52b1bd07659.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

在native层会一直使用到系统内存用尽的时候

![屏幕快照 2018-05-17 下午7.44.53.png](https://upload-images.jianshu.io/upload_images/1460468-c6fa214a21590ace.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


# 4.4之前的Bitmap

匿名贡献内存，native，并且不占用本地内存

# 8.0之前Java内存

# 8.0之后native内存管理

#  Java跟native内存的问题

1

    public static Bitmap createBitmap(Bitmap source, int x, int y, int width, int height,
            Matrix m, boolean filter) {

        ...
        Config newConfig = Config.ARGB_8888;
        final Config config = source.getConfig();
        // GIF files generate null configs, assume ARGB_8888
        if (config != null) {
            switch (config) {
                case RGB_565:
                    newConfig = Config.RGB_565;
                    break;
                case ALPHA_8:
                    newConfig = Config.ALPHA_8;
                    break;
                case ARGB_4444:
                case ARGB_8888:
                default:
                    newConfig = Config.ARGB_8888;
                    break;
            }
        }

       ...
        Bitmap = createBitmap(neww, newh, newConfig, source.hasAlpha());
       ...
       return Bitmap;
    }
    

2

    
	  private static Bitmap createBitmap(DisplayMetrics display, int width, int height,
	             Config config, boolean hasAlpha) {
	         if (width <= 0 || height <= 0) {
	             throw new IllegalArgumentException("width and height must be > 0");
	         }
	         Bitmap bm = nativeCreate(null, 0, width, width, height, config.nativeInt, true);
	         if (display != null) {
	             bm.mDensity = display.densityDpi;
	         }
	         if (config == Config.ARGB_8888 && !hasAlpha) {
	             nativeErase(bm.mNativeBitmap, 0xff000000);
	             nativeSetHasAlpha(bm.mNativeBitmap, hasAlpha);
	         } else {
	 
	         }
	         return bm;
	     }
     
3

	 static JNINativeMethod gBitmapMethods[] = {
	     {   "nativeCreate",             "([IIIIIIZ)Landroid/graphics/Bitmap;",
	         (void*)Bitmap_creator },     
         
         
4
 
	 static jobject Bitmap_creator(JNIEnv* env, jobject, jintArray jColors,
	                               jint offset, jint stride, jint width, jint height,
	                               jint configHandle, jboolean isMutable) {
	     SkColorType colorType = GraphicsJNI::legacyBitmapConfigToColorType(configHandle);
	      ... 
	 
	     SkBitmap Bitmap;
	     Bitmap.setInfo(SkImageInfo::Make(width, height, colorType, kPremul_SkAlphaType));
	   <!--内存分配-->
	     Bitmap* nativeBitmap = GraphicsJNI::allocateJavaPixelRef(env, &Bitmap, NULL);
	     if (!nativeBitmap) {
	         return NULL;
	     }
	      ... 
	      <!--创建Bitmap-->
	     return GraphicsJNI::createBitmap(env, nativeBitmap,
	             getPremulBitmapCreateFlags(isMutable));
	 }


 
# 5  8.0之前跟之后有区别  

8.0之前内存是在Java层分配的的内存分配
  
	 android::Bitmap* GraphicsJNI::allocateJavaPixelRef(JNIEnv* env, SkBitmap* Bitmap,
	                                              SkColorTable* ctable) {
	                                              
	     const SkImageInfo& info = Bitmap->info();
	     if (info.fColorType == kUnknown_SkColorType) {
	         doThrowIAE(env, "unknown Bitmap configuration");
	         return NULL;
	     }
	 
	     size_t size;
	     if (!computeAllocationSize(*Bitmap, &size)) {
	         return NULL;
	     }
	
	     const size_t rowBytes = Bitmap->rowBytes();
	     <!--在Java层创建Bitmap需要的Byte数组 jbyteArray-->
	     jbyteArray arrayObj = (jbyteArray) env->CallObjectMethod(gVMRuntime,
	                                                              gVMRuntime_newNonMovableArray,
	                                                              gByte_class, size);
	     if (env->ExceptionCheck() != 0) {
	         return NULL;
	     }
	     <!--获取分配地址-->
	     jbyte* addr = (jbyte*) env->CallLongMethod(gVMRuntime, gVMRuntime_addressOf, arrayObj);
	     ...
	     <!--创建Bitmap-->
	     android::Bitmap* wrapper = new android::Bitmap(env, arrayObj, (void*) addr,
	             info, rowBytes, ctable);
	     wrapper->getSkBitmap(Bitmap);
	     Bitmap->lockPixels();
	     return wrapper;
	 }

6 创建Java Bitmap对象 
 
	 jobject GraphicsJNI::createBitmap(JNIEnv* env, android::Bitmap* Bitmap,
	         int BitmapCreateFlags, jbyteArray ninePatchChunk, jobject ninePatchInsets,
	         int density) {
	     bool isMutable = BitmapCreateFlags & kBitmapCreateFlag_Mutable;
	     bool isPremultiplied = BitmapCreateFlags & kBitmapCreateFlag_Premultiplied;
	     // The caller needs to have already set the alpha type properly, so the
	     // native SkBitmap stays in sync with the Java Bitmap.
	     assert_premultiplied(Bitmap->info(), isPremultiplied);
	 
	     jobject obj = env->NewObject(gBitmap_class, gBitmap_constructorMethodID,
	             reinterpret_cast<jlong>(Bitmap), Bitmap->javaByteArray(),
	             Bitmap->width(), Bitmap->height(), density, isMutable, isPremultiplied,
	             ninePatchChunk, ninePatchInsets);
	     hasException(env); // For the side effect of logging.
	     return obj;
	 } 
 
你会发现，Bitmap在新版本上，内存是直接以Java层Byte数组的方式进行分配的，

# 8.0之后的内存分配是在native
	
	
	static jobject Bitmap_creator(JNIEnv* env, jobject, jintArray jColors,
	                              jint offset, jint stride, jint width, jint height,
	                              jint configHandle, jboolean isMutable,
	                              jfloatArray xyzD50, jobject transferParameters) {
	    SkColorType colorType = GraphicsJNI::legacyBitmapConfigToColorType(configHandle);
	    if (NULL != jColors) {
	        size_t n = env->GetArrayLength(jColors);
	        if (n < SkAbs32(stride) * (size_t)height) {
	            doThrowAIOOBE(env);
	            return NULL;
	        }
	    }
	
	    // ARGB_4444 is a deprecated format, convert automatically to 8888
	    if (colorType == kARGB_4444_SkColorType) {
	        colorType = kN32_SkColorType;
	    }
	
	    SkBitmap Bitmap;
	    sk_sp<SkColorSpace> colorSpace;
	
	    if (colorType != kN32_SkColorType || xyzD50 == nullptr || transferParameters == nullptr) {
	        colorSpace = GraphicsJNI::colorSpaceForType(colorType);
	    } else {
	        SkColorSpaceTransferFn p = GraphicsJNI::getNativeTransferParameters(env, transferParameters);
	        SkMatrix44 xyzMatrix = GraphicsJNI::getNativeXYZMatrix(env, xyzD50);
	        colorSpace = SkColorSpace::MakeRGB(p, xyzMatrix);
	    }
	
	    Bitmap.setInfo(SkImageInfo::Make(width, height, colorType, kPremul_SkAlphaType, colorSpace));
	
	    sk_sp<Bitmap> nativeBitmap = Bitmap::allocateHeapBitmap(&Bitmap);
	    if (!nativeBitmap) {
	        return NULL;
	    }
	
	    if (jColors != NULL) {
	        GraphicsJNI::SetPixels(env, jColors, offset, stride, 0, 0, width, height, Bitmap);
	    }
	
	    return createBitmap(env, nativeBitmap.release(), getPremulBitmapCreateFlags(isMutable));
	}

	static sk_sp<Bitmap> allocateHeapBitmap(size_t size, const SkImageInfo& info, size_t rowBytes) {
	    void* addr = calloc(size, 1);
	    if (!addr) {
	        return nullptr;
	    }
	    return sk_sp<Bitmap>(new Bitmap(addr, size, info, rowBytes));
	}

8.0之后图片浏览的时候，基本增加的都是native的内存,

	dalvik.vm.heapstartsize=8m
	dalvik.vm.heapgrowthlimit=192m
	dalvik.vm.heapsize=512m
	dalvik.vm.heaptargetutilization=0.75
	dalvik.vm.heapminfree=512k
	dalvik.vm.heapmaxfree=8m

![Android内存管理](https://upload-images.jianshu.io/upload_images/1460468-cddac202d1fd0ed4.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

看看OOM曲线，其实并不受native内存影响，native当然有限制，但是这个限制跟oom没关系

![屏幕快照 2018-05-17 下午1.41.24.png](https://upload-images.jianshu.io/upload_images/1460468-6ae5d4b7c7c29c78.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![屏幕快照 2018-05-17 下午1.43.54.png](https://upload-images.jianshu.io/upload_images/1460468-9a52b0e512863231.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)



# 8.0之后的回收机制

    Bitmap(long nativeBitmap, int width, int height, int density,
            boolean isMutable, boolean requestPremultiplied,
            byte[] ninePatchChunk, NinePatch.InsetStruct ninePatchInsets) {
        if (nativeBitmap == 0) {
            throw new RuntimeException("internal error: native bitmap is 0");
        }

        mWidth = width;
        mHeight = height;
        mIsMutable = isMutable;
        mRequestPremultiplied = requestPremultiplied;

        mNinePatchChunk = ninePatchChunk;
        mNinePatchInsets = ninePatchInsets;
        if (density >= 0) {
            mDensity = density;
        }

        mNativePtr = nativeBitmap;
        long nativeSize = NATIVE_ALLOCATION_SIZE + getAllocationByteCount();
        NativeAllocationRegistry registry = new NativeAllocationRegistry(
            Bitmap.class.getClassLoader(), nativeGetNativeFinalizer(), nativeSize);
        registry.registerNativeAllocation(this, nativeBitmap);

        if (ResourcesImpl.TRACE_FOR_DETAILED_PRELOAD) {
            sPreloadTracingNumInstantiatedBitmaps++;
            sPreloadTracingTotalBitmapsSize += nativeSize;
        }
    }
    
  NativeAllocationRegistry保证了native内存在gc的时候自动释放，好牛逼
  
    
# 参考文档
 
[JNI java和c之间对象的传递](https://blog.csdn.net/lg707415323/article/details/7832252)           
[使用 Memory Profiler 查看 Java 堆和内存分配](https://developer.android.com/studio/profile/memory-profiler?hl=zh-CN)          
[
Android 内存详细分析](https://blog.csdn.net/hnulwt/article/details/44900811)         
[Managing Bitmap Memory](https://developer.android.com/topic/performance/graphics/manage-memory)