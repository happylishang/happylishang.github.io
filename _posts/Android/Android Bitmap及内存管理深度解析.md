Android Bitmap深度解析

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
	        

* Java跟native内存的问题（如何统计）
* 占内存问题（Java Native）
* 分配问题
* 大小问题
* Bitmap概念

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


 
5 
  
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

# 参考文档
 
[JNI java和c之间对象的传递](https://blog.csdn.net/lg707415323/article/details/7832252)