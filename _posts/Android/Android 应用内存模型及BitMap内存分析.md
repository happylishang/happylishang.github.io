Android Bitmap深度解析

* Java跟native内存的问题（如何统计）
* 占内存问题（Java Native）
* 分配问题
* 大小问题
* Bitmap概念

#  Java跟native内存的问题

![Android内存管理](https://upload-images.jianshu.io/upload_images/1460468-cddac202d1fd0ed4.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


采用adb看下内存状况：dumpsys  meminfo com.xxx
	
	** MEMINFO in pid 30401 [com.xxx] **
	                   Pss  Private  Private  Swapped     Heap     Heap     Heap
	                 Total    Dirty    Clean    Dirty     Size    Alloc     Free
	                ------   ------   ------   ------   ------   ------   ------
	  Native Heap    35976    35932        0        0    54272    43545    10726
	  Dalvik Heap    72093    72008        0        0    94227    78252    15975
	 Dalvik Other     3916     3916        0        0                           
	        Stack     2384     2384        0        0                           
	       Ashmem      492      136        0        0                           
	      Gfx dev    69140    69140        0        0                           
	    Other dev       12        0       12        0                           
	     .so mmap     1256      328      344        0                           
	    .apk mmap       43        0        0        0                           
	    .dex mmap    35088    35048       20        0                           
	    .oat mmap      591        0       32        0                           
	    .art mmap     2989     2816        4        0                           
	   Other mmap       66       12       20        0                           
	   EGL mtrack    44940    44940        0        0                           
	      Unknown     9848     9848        0        0                           
	        TOTAL   278834   276508      432        0   148499   121797    26701
	 
	 App Summary
	                       Pss(KB)
	                        ------
	           Java Heap:    74828
	         Native Heap:    35932
	                Code:    35772
	               Stack:     2384
	            Graphics:   114080
	       Private Other:    13944
	              System:     1894
	 
	               TOTAL:   278834      TOTAL SWAP (KB):        0
	 
	 Objects
	               Views:      854         ViewRootImpl:        2
	         AppContexts:        5           Activities:        2
	              Assets:        4        AssetManagers:        3
	       Local Binders:       49        Proxy Binders:       39
	       Parcel memory:       30         Parcel count:      121
	    Death Recipients:        5      OpenSSL Sockets:        1
	 
	 SQL
	         MEMORY_USED:      282
	  PAGECACHE_OVERFLOW:       87          MALLOC_SIZE:       62
	        
* Java：从 Java 或 Kotlin 代码分配的对象内存。
* Native：从 C 或 C++ 代码分配的对象内存。native层的 so 中调用malloc或new创建的内存  
* 即使您的应用中不使用 C++，您也可能会看到此处使用的一些原生内存，因为 Android 框架使用原生内存代表您处理各种任务，如处理图像资源和其他图形时，即使您编写的代码采用 Java 或 Kotlin 语言。
* Graphics：图形缓冲区队列向屏幕显示像素（包括 GL 表面、GL 纹理等等）所使用的内存。 （请注意，这是与 CPU 共享的内存，不是 GPU 专用内存。） OpenGL和SurfaceFlinger相关内存，若应用没有直接调用OpenGL，则可以确定这部分内存是由Android Framework操控的，可以忽略。（当然对于游戏类应用，这里肯定是优化重点。）
* Stack： 您的应用中的原生堆栈和 Java 堆栈使用的内存。 这通常与您的应用运行多少线程有关。 
* Code：您的应用用于处理代码和资源（如 dex 字节码、已优化或已编译的 dex 码、.so 库和字体）的内存。 
* Other：您的应用使用的系统不确定如何分类的内存。 
* Allocated：您的应用分配的 Java/Kotlin 对象数。 它没有计入 C 或 C++ 中分配的对象。


Android采用的Dalvik或者Art虚拟机，在内存管理上，分为两种内存，

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
        bitmap = createBitmap(neww, newh, newConfig, source.hasAlpha());
       ...
       return bitmap;
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
	 
	     SkBitmap bitmap;
	     bitmap.setInfo(SkImageInfo::Make(width, height, colorType, kPremul_SkAlphaType));
	   <!--内存分配-->
	     Bitmap* nativeBitmap = GraphicsJNI::allocateJavaPixelRef(env, &bitmap, NULL);
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
  
	 android::Bitmap* GraphicsJNI::allocateJavaPixelRef(JNIEnv* env, SkBitmap* bitmap,
	                                              SkColorTable* ctable) {
	                                              
	     const SkImageInfo& info = bitmap->info();
	     if (info.fColorType == kUnknown_SkColorType) {
	         doThrowIAE(env, "unknown bitmap configuration");
	         return NULL;
	     }
	 
	     size_t size;
	     if (!computeAllocationSize(*bitmap, &size)) {
	         return NULL;
	     }
	
	     const size_t rowBytes = bitmap->rowBytes();
	     <!--在Java层创建bitmap需要的Byte数组 jbyteArray-->
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
	     wrapper->getSkBitmap(bitmap);
	     bitmap->lockPixels();
	     return wrapper;
	 }

6 创建Java Bitmap对象 
 
	 jobject GraphicsJNI::createBitmap(JNIEnv* env, android::Bitmap* bitmap,
	         int bitmapCreateFlags, jbyteArray ninePatchChunk, jobject ninePatchInsets,
	         int density) {
	     bool isMutable = bitmapCreateFlags & kBitmapCreateFlag_Mutable;
	     bool isPremultiplied = bitmapCreateFlags & kBitmapCreateFlag_Premultiplied;
	     // The caller needs to have already set the alpha type properly, so the
	     // native SkBitmap stays in sync with the Java Bitmap.
	     assert_premultiplied(bitmap->info(), isPremultiplied);
	 
	     jobject obj = env->NewObject(gBitmap_class, gBitmap_constructorMethodID,
	             reinterpret_cast<jlong>(bitmap), bitmap->javaByteArray(),
	             bitmap->width(), bitmap->height(), density, isMutable, isPremultiplied,
	             ninePatchChunk, ninePatchInsets);
	     hasException(env); // For the side effect of logging.
	     return obj;
	 } 
 
你会发现，BitMap在新版本上，内存是直接以Java层Byte数组的方式进行分配的，

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
	
	    SkBitmap bitmap;
	    sk_sp<SkColorSpace> colorSpace;
	
	    if (colorType != kN32_SkColorType || xyzD50 == nullptr || transferParameters == nullptr) {
	        colorSpace = GraphicsJNI::colorSpaceForType(colorType);
	    } else {
	        SkColorSpaceTransferFn p = GraphicsJNI::getNativeTransferParameters(env, transferParameters);
	        SkMatrix44 xyzMatrix = GraphicsJNI::getNativeXYZMatrix(env, xyzD50);
	        colorSpace = SkColorSpace::MakeRGB(p, xyzMatrix);
	    }
	
	    bitmap.setInfo(SkImageInfo::Make(width, height, colorType, kPremul_SkAlphaType, colorSpace));
	
	    sk_sp<Bitmap> nativeBitmap = Bitmap::allocateHeapBitmap(&bitmap);
	    if (!nativeBitmap) {
	        return NULL;
	    }
	
	    if (jColors != NULL) {
	        GraphicsJNI::SetPixels(env, jColors, offset, stride, 0, 0, width, height, bitmap);
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

8.0之后图片浏览的时候，基本增加的都是native的内存

# 参考文档
 
[JNI java和c之间对象的传递](https://blog.csdn.net/lg707415323/article/details/7832252)           
[使用 Memory Profiler 查看 Java 堆和内存分配](https://developer.android.com/studio/profile/memory-profiler?hl=zh-CN)          
[
Android 内存详细分析](https://blog.csdn.net/hnulwt/article/details/44900811)         