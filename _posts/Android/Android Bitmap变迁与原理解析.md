


App开发不可避免的要和图片打交道，由于其占用内存非常大，管理不当很容易导致内存不足，最后OOM，图片的背后其实是Bitmap，它是Android中最能吃内存的对象之一，也是很多OOM的元凶，不过，在不同的Android版本中，Bitmap或多或少都存在差异，尤其是在其内存分配上，了解其中的不用跟原理能更好的指导图片管。先看Google官方文档的说明：

>On Android 2.3.3 (API level 10) and lower, the backing pixel data for a Bitmap is stored in native memory. It is separate from the Bitmap itself, which is stored in the Dalvik heap. The pixel data in native memory is not released in a predictable manner, potentially causing an application to briefly exceed its memory limits and crash. From Android 3.0 (API level 11) through Android 7.1 (API level 25), the pixel data is stored on the Dalvik heap along with the associated Bitmap. In Android 8.0 (API level 26), and higher, the Bitmap pixel data is stored in the native heap.

大意就是： 2.3之前的像素存储需要的内存是在native上分配的，并且生命周期不被Bitmap控制，需要用户自己回收。  2.3-7.1之间，Bitmap的像素存储在Dalvik的Java堆上，当然，4.4之前的甚至能在匿名共享内存上分配（Fresco采用），而8.0之后的像素内存又重新回到native上去分配，**不需要用户主动回收**，8.0之后图像资源的管理更加优秀，极大降低了OOM。Android 2.3.3已经属于过期技术，不再分析，本文主要看4.x之后的手机系统。


# Android 8.0前后Bitmap内存增长曲线直观对比

Bitmap内存分配一个很大的分水岭是在Android 8.0，可以用一段代码来模拟器Bitmap无限增长，最终OOM，或者Crash退出。通过在不同版本上的表现，期待对Bitmap内存分配有一个直观的了解，示例代码如下：
   
	   @onClick(R.id.increase)
	  	   void increase{
	  		 Map<String, Bitmap> map = new HashMap<>();
  			 for(int i=0 ; i<10;i++){
			   Bitmap bitmap = BitmapFactory.decodeResource(getResources(), 						R.mipmap.green);
  			    map.put("" + System.currentTimeMillis(), bitmap);
   				}
		    }


##  Nexus5 Android 6.0的表现


不断的解析图片，并持有Bitmap引用，会导致内存不断上升，通过Android Profiler工具简单看一下上图内存分配状况，在某一个点内存分配情况如下：

![1526644329066.jpg](https://upload-images.jianshu.io/upload_images/1460468-dc8a60c3f9724595.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

简单总结下内存占比

内存        | 大小         |  
--------------------|:------------------:| 
Total | 211M   |  
Java内存 | 157.2M  |  
native内存 | 3.7M  |
Bitmap内存 | 145.9M（152663617 byte） |
Graphics内存(一般是Fb对应的，App不需要考虑) | 45.1M（152663617 byte） |

从上表可以看到绝大数内存都是由Bitmap，并且位于虚拟机的heap中，其实是因为在6.0中，bitmap的像素数据都是以byte的数组的形式存在java 虚拟机的heap中。内存无限增大，知道OOM崩溃的时候，内存状况入下

 ![1526641659822.jpg](https://upload-images.jianshu.io/upload_images/1460468-c396929c4b54f134.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

内存        | 大小         |  
--------------------|:------------------:| 
Total | 546.2M   |  
Java内存 | 496.8M  |  
native内存 | 3.3M  |
Graphics内存(一般是Fb对应的，App不需要考虑) | 45.1M |

可见，增长的一直是Java堆中的内存，也就是Bitmap在Dalvik栈中分配的内存，等到Dalvik达到虚拟机内存上限的时候，在Dalvik会抛出OOM异常：
 
![1526641743077.jpg](https://upload-images.jianshu.io/upload_images/1460468-d215949b81b47944.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可见，对于Android6.0，Bitmap的内存分配基本都在Java层。然后，再看一下Android 8.0的Bitmap分配。

##  Nexus6p Android 8.0 的表现

>In Android 8.0 (API level 26), and higher, the Bitmap pixel data is stored in the native heap.

从官方文档中我们知道，Android8.0之后最大的改进就是Bitmap内存分配的位置：从Java堆转移到了native堆栈，直观分配图如下
 
![61526525051_.pic.jpg](https://upload-images.jianshu.io/upload_images/1460468-6a3fe361dab421b4.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

内存        | 大小         |  
--------------------|:------------------:| 
Total | 1.2G   |  
Java内存 | 0G  |  
native内存 | 1.1G |
Graphics内存(一般是Fb对应的，App不需要考虑) | 0.1G |

很明显，Bitmap内存的增加基本都在native层，随着Bitmap内存占用的无限增长，App最终无法从系统分配到内存，最后会导致崩溃，看一下崩溃的时候内存占用：

![51526524893_.pic.jpg](https://upload-images.jianshu.io/upload_images/1460468-d74219d2777e1f3c.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

内存        | 大小         |  
--------------------|:------------------:| 
Total | 1.9G   |  
Java内存 | 0G  |  
native内存 | 1.9G |
Graphics内存(一般是Fb对应的，App不需要考虑) | 0.1G |

可见一个APP内存的占用惊人的达到了1.9G，并且几乎全是native内存，这个其实就是Google在8.0做的最大的一个优化，我们知道Java虚拟机一般是有一个上限，但是由于Android同时能运行多个APP，这个上限一般不会太高，拿nexus6p而言，一般是如下配置

	dalvik.vm.heapstartsize=8m
	dalvik.vm.heapgrowthlimit=192m
	dalvik.vm.heapsize=512m
	dalvik.vm.heaptargetutilization=0.75
	dalvik.vm.heapminfree=512k
	dalvik.vm.heapmaxfree=8m

如果没有在AndroidManifest中启用largeheap，那么Java 堆内存达到192M的时候就会崩溃，对于现在动辄4G的手机而言，存在严重的资源浪费，ios的一个APP几乎能用近所有的可用内存（除去系统开支），8.0之后，Android也向这个方向靠拢，最好的下手对象就是Bitmap，因为它是耗内存大户。到图片内存被转移到native之后，一个APP的图片处理不仅能使用系统
绝大多数内存，还能降低Java层内存使用，减少OOM风险。不过，内存无限增长的情况下，也会导致APP崩溃，但是这种崩溃已经不是OOM崩溃了，Java虚拟机也不会捕获，按道理说，应该属于linux的OOM了。从崩溃时候的Log就能看得出与Android6.0的区别：

![1526641932348.jpg](https://upload-images.jianshu.io/upload_images/1460468-ce3c5a83fe03a259.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可见，这个时候崩溃并不为Java虚拟机控制，直接进程死掉，不会有Crash弹框。其实如果在Android6.0的手机上，在native分配内存，也会达到相同的效果，也就是说**native的内存不影响java虚拟机的**OOM。

## Android 6.0模拟native内存OOM

![image.png](https://upload-images.jianshu.io/upload_images/1460468-eb01a52b1bd07659.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

在native层会一直使用到系统内存用尽的时候

![屏幕快照 2018-05-17 下午7.44.53.png](https://upload-images.jianshu.io/upload_images/1460468-c6fa214a21590ace.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


# 8.0之后的Bitmap内存分配原理

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
  
  
## Android 4.4之前其实Bitmap也可以做到在native分配内存

其实在Android5.0之前，Bitmap也是可以在native分配内存的，一个典型的例子就是Fresco，Fresco为了提高5.0之前图片处理的性能很有效的利用了这个特性，不过由于不太成熟，在5.0之后废弃，直到8.0重新拾起来，与这个特性有关的两个属性是BitmapFactory.Options中的inPurgeable与inInputShareable，具体的不在分析，过期技术，等于垃圾，有兴趣，可以自行分析。
	  
	 		 /**
	         * @deprecated As of {@link android.os.Build.VERSION_CODES#LOLLIPOP}, this is
	         * ignored.
	         *
	         * In {@link android.os.Build.VERSION_CODES#KITKAT} and below, if this
	         * is set to true, then the resulting bitmap will allocate its
	         * pixels such that they can be purged if the system needs to reclaim
	         * memory. In that instance, when the pixels need to be accessed again
	         * (e.g. the bitmap is drawn, getPixels() is called), they will be
	         * automatically re-decoded.
	         *
	         * <p>For the re-decode to happen, the bitmap must have access to the
	         * encoded data, either by sharing a reference to the input
	         * or by making a copy of it. This distinction is controlled by
	         * inInputShareable. If this is true, then the bitmap may keep a shallow
	         * reference to the input. If this is false, then the bitmap will
	         * explicitly make a copy of the input data, and keep that. Even if
	         * sharing is allowed, the implementation may still decide to make a
	         * deep copy of the input data.</p >
	         *
	         * <p>While inPurgeable can help avoid big Dalvik heap allocations (from
	         * API level 11 onward), it sacrifices performance predictability since any
	         * image that the view system tries to draw may incur a decode delay which
	         * can lead to dropped frames. Therefore, most apps should avoid using
	         * inPurgeable to allow for a fast and fluid UI. To minimize Dalvik heap
	         * allocations use the {@link #inBitmap} flag instead.</p >
	         *
	         * <p class="note"><strong>Note:</strong> This flag is ignored when used
	         * with {@link #decodeResource(Resources, int,
	         * android.graphics.BitmapFactory.Options)} or {@link #decodeFile(String,
	         * android.graphics.BitmapFactory.Options)}.</p >
	         */
	        @Deprecated
	        public boolean inPurgeable;
	
	        /**
	         * @deprecated As of {@link android.os.Build.VERSION_CODES#LOLLIPOP}, this is
	         * ignored.
	         *
	         * In {@link android.os.Build.VERSION_CODES#KITKAT} and below, this
	         * field works in conjuction with inPurgeable. If inPurgeable is false,
	         * then this field is ignored. If inPurgeable is true, then this field
	         * determines whether the bitmap can share a reference to the input
	         * data (inputstream, array, etc.) or if it must make a deep copy.
	         */
	        @Deprecated
	        public boolean inInputShareable;
        
        
# 总结
    
# 参考文档
 
[JNI java和c之间对象的传递](https://blog.csdn.net/lg707415323/article/details/7832252)           
[使用 Memory Profiler 查看 Java 堆和内存分配](https://developer.android.com/studio/profile/memory-profiler?hl=zh-CN)          
[
Android 内存详细分析](https://blog.csdn.net/hnulwt/article/details/44900811)         
[Managing Bitmap Memory](https://developer.android.com/topic/performance/graphics/manage-memory)     
[谈谈fresco的bitmap内存分配](https://blog.csdn.net/chiefhsing/article/details/53899242)       