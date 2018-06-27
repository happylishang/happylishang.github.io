---
layout: post
title: "Android Bitmap变迁与原理解析（4.x-8.x）"
category: Android

---

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

在直接native内存分配，并且不释放，模拟代码如下：

	void increase(){
		 int size=1024*1024*100;
        char *Ptr = NULL;
        Ptr = (char *)malloc(size * sizeof(char));
        for(int i=0;i<size ;i++) {
          *(Ptr+i)=i%30;
        }
        for(int i=0;i<1024*1024 ;i++) {
           if(i%100==0)
          LOGI(" malloc  - %d" ,*(Ptr+i));
        }
	}

只malloc，不free，这种情况下Android6.0的内存增长如下： 
    
![image.png](https://upload-images.jianshu.io/upload_images/1460468-eb01a52b1bd07659.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

内存        | 大小         |  
--------------------|:------------------:| 
Total | 750m   |  
Java内存 | 1.9m |  
native内存 | 703M |
Graphics内存(一般是Fb对应的，App不需要考虑) | 44.1M |

Total内存750m，已经超过Nexus5 Android6.0 Dalvik虚拟机内存上限，但APP没有崩溃，可见native内存的增长并不会导致java虚拟机的OOM，在native层，oom的时机是到系统内存用尽的时候：

![屏幕快照 2018-05-17 下午7.44.53.png](https://upload-images.jianshu.io/upload_images/1460468-c6fa214a21590ace.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可见对于6.0的系统，一个APP也是能够耗尽系统所有内存的，下面来看下Bitmap内存分配原理，为什么8.0前后差别这么大。

# Bitmap内存分配原理



## 8.0之前Bitmap内存分配原理

其实，通过Bitmap的成员列表，就能看出一点眉目，Bitmap中有个byte[] mBuffer，其实就是用来存储像素数据的，很明显它位于java heap中

	public final class Bitmap implements Parcelable {
	    private static final String TAG = "Bitmap";
	     ...
	    private byte[] mBuffer;
	     ...
	    }
	    
    
接下来，通过手动创建Bitmap，进行分析：Bitmap.java

    public static Bitmap createBitmap(int width, int height, Config config) {
        return createBitmap(width, height, config, true);
    }

 ![屏幕快照 2018-05-22 上午11.06.00.png](https://upload-images.jianshu.io/upload_images/1460468-205620490e4829bb.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 
 Java层Bitmap的创建最终还是会走向native层：Bitmap.cpp
 
	 static jobject Bitmap_creator(JNIEnv* env, jobject, jintArray jColors,
	                               jint offset, jint stride, jint width, jint height,
	                               jint configHandle, jboolean isMutable) {
	     SkColorType colorType = GraphicsJNI::legacyBitmapConfigToColorType(configHandle);
	      ... 
	 
	     SkBitmap Bitmap;
	     Bitmap.setInfo(SkImageInfo::Make(width, height, colorType, kPremul_SkAlphaType));
	   		<!--关键点1 像素内存分配-->
	     Bitmap* nativeBitmap = GraphicsJNI::allocateJavaPixelRef(env, &Bitmap, NULL);
	     if (!nativeBitmap) {
	         return NULL;
	     }
	      ... 
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

这里只看关键点1，像素内存的分配：GraphicsJNI::allocateJavaPixelRef从这个函数名可以就可以看出，是在Java层分配，跟进去，也确实如此：


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
	
	    // we must respect the rowBytes value already set on the bitmap instead of
	    // attempting to compute our own.
	    const size_t rowBytes = bitmap->rowBytes();
	   <!--关键点1 ，创建Java层字节数据，作为数据存储单元-->
	    jbyteArray arrayObj = (jbyteArray) env->CallObjectMethod(gVMRuntime,
	                                                             gVMRuntime_newNonMovableArray,
	                                                             gByte_class, size);
	    if (env->ExceptionCheck() != 0) {
	        return NULL;
	    }
	    SkASSERT(arrayObj);
	    jbyte* addr = (jbyte*) env->CallLongMethod(gVMRuntime, gVMRuntime_addressOf, arrayObj);
	    if (env->ExceptionCheck() != 0) {
	        return NULL;
	    }
	    SkASSERT(addr);
	    android::Bitmap* wrapper = new android::Bitmap(env, arrayObj, (void*) addr,
	            info, rowBytes, ctable);
	    wrapper->getSkBitmap(bitmap);
	    // since we're already allocated, we lockPixels right away
	    // HeapAllocator behaves this way too
	    bitmap->lockPixels();
	
	    return wrapper;
	}
 
由于只关心内存分配，同样只看关键点1，这里其实就是在native层创建Java层byte[]，并将这个byte[]作为像素存储结构，之后再通过在native层构建Java Bitmap对象的方式，将生成的byte[]传递给Bitmap.java对象：

	jobject GraphicsJNI::createBitmap(JNIEnv* env, android::Bitmap* bitmap,
	        int bitmapCreateFlags, jbyteArray ninePatchChunk, jobject ninePatchInsets,
	        int density) {
	   	...<!--关键点1，构建java Bitmap对象，并设置byte[] mBuffer-->
	    jobject obj = env->NewObject(gBitmap_class, gBitmap_constructorMethodID,
	            reinterpret_cast<jlong>(bitmap), bitmap->javaByteArray(),
	            bitmap->width(), bitmap->height(), density, isMutable, isPremultiplied,
	            ninePatchChunk, ninePatchInsets);
	    hasException(env); // For the side effect of logging.
	    return obj;
	}

以上就是8.0之前的内存分配，其实4.4以及之前的更乱，下面再看下8.0之后的Bitmap是什么原理。

## 8.0之后Bitmap内存分配有什么新特点   
 
其实从8.0的Bitmap.java类也能看出区别，之前的  private byte[] mBuffer成员不见了，取而代之的是private final long mNativePtr，也就说，Bitmap.java只剩下一个壳了，具体如下：
	 
	public final class Bitmap implements Parcelable {
	    ...
	    // Convenience for JNI access
	    private final long mNativePtr;
	    ...
	 }
    
之前说过8.0之后的内存分配是在native，具体到代码是怎么样的表现呢？流程与8.0之前基本类似，区别在native分配时：
	
![屏幕快照 2018-05-22 下午1.55.15.png](https://upload-images.jianshu.io/upload_images/1460468-0f9a15b6f60ef53b.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
	
	static jobject Bitmap_creator(JNIEnv* env, jobject, jintArray jColors,
	                              jint offset, jint stride, jint width, jint height,
	                              jint configHandle, jboolean isMutable,
	                              jfloatArray xyzD50, jobject transferParameters) {
	    SkColorType colorType = GraphicsJNI::legacyBitmapConfigToColorType(configHandle);
	   	 ...
	   	 <!--关键点1 ，native层创建bitmap，并分配native内存-->
	    sk_sp<Bitmap> nativeBitmap = Bitmap::allocateHeapBitmap(&Bitmap);
	    if (!nativeBitmap) {
	        return NULL;
	    }
	    ...
	    return createBitmap(env, nativeBitmap.release(), getPremulBitmapCreateFlags(isMutable));
	}

看一下allocateHeapBitmap如何分配内存

	static sk_sp<Bitmap> allocateHeapBitmap(size_t size, const SkImageInfo& info, size_t rowBytes) {
	   	<!--关键点1 直接calloc分配内存-->
	    void* addr = calloc(size, 1);
	    if (!addr) {
	        return nullptr;
	    }
	 	<!--关键点2 创建native Bitmap-->
	    return sk_sp<Bitmap>(new Bitmap(addr, size, info, rowBytes));
	}
	
可以看出，8.0之后，Bitmap像素内存的分配是在native层直接调用calloc，所以其像素分配的是在native heap上， 这也是为什么8.0之后的Bitmap消耗内存可以无限增长，直到耗尽系统内存，也不会提示Java OOM的原因。

# 8.0之后的Bitmap内存回收机制

NativeAllocationRegistry是Android 8.0引入的一种辅助自动回收native内存的一种机制，当Java对象因为GC被回收后，NativeAllocationRegistry可以辅助回收Java对象所申请的native内存，拿Bitmap为例，入下：

    Bitmap(long nativeBitmap, int width, int height, int density,
            boolean isMutable, boolean requestPremultiplied,
            byte[] ninePatchChunk, NinePatch.InsetStruct ninePatchInsets) {
        ...
        mNativePtr = nativeBitmap;
        long nativeSize = NATIVE_ALLOCATION_SIZE + getAllocationByteCount();
        <!--辅助回收native内存-->
        NativeAllocationRegistry registry = new NativeAllocationRegistry(
            Bitmap.class.getClassLoader(), nativeGetNativeFinalizer(), nativeSize);
        registry.registerNativeAllocation(this, nativeBitmap);
       if (ResourcesImpl.TRACE_FOR_DETAILED_PRELOAD) {
            sPreloadTracingNumInstantiatedBitmaps++;
            sPreloadTracingTotalBitmapsSize += nativeSize;
        }
    }
  
当然这个功能也要Java虚拟机的支持，有机会再分析。
  
# Android 4.4之前其实Bitmap也可在native（伪）分配内存

其实在Android5.0之前，Bitmap也是可以在native分配内存的，一个典型的例子就是Fresco，Fresco为了提高5.0之前图片处理的性能，就很有效的利用了这个特性，不过由于不太成熟，在5.0之后废弃，直到8.0重新拾起来（新方案），与这个特性有关的两个属性是BitmapFactory.Options中的inPurgeable与inInputShareable，具体的不在分析。过期技术，等于垃圾，有兴趣，可以自行分析。
	  
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

* 8.0之前的Bitmap像素数据基本存储在Java heap
* 8.0之后的 Bitmap像素数据基本存储在native heap
* 4.4可以通过inInputShareable、inPurgeable让Bitmap的内存在native层分配（已废弃）
    
# 参考文档
 
[JNI java和c之间对象的传递](https://blog.csdn.net/lg707415323/article/details/7832252)           
[使用 Memory Profiler 查看 Java 堆和内存分配](https://developer.android.com/studio/profile/memory-profiler?hl=zh-CN)          
[
Android 内存详细分析](https://blog.csdn.net/hnulwt/article/details/44900811)         
[Managing Bitmap Memory](https://developer.android.com/topic/performance/graphics/manage-memory)     
[谈谈fresco的bitmap内存分配](https://blog.csdn.net/chiefhsing/article/details/53899242)       