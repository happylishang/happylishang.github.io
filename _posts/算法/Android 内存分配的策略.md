## 启动虚拟机，分配初始内存，及加载初始配置

核心参数的意义	
	
	struct HeapSource {
	    /* Target ideal heap utilization ratio; range 1..HEAP_UTILIZATION_MAX
	     */
	     <!--合理的利用率-->
	    size_t targetUtilization;
	    /* The starting heap size.
	     */
	    size_t startSize;
	    /* The largest that the heap source as a whole is allowed to grow.
	     */
	    size_t maximumSize;
	    /*
	     * The largest size we permit the heap to grow.  This value allows
	     * the user to limit the heap growth below the maximum size.  This
	     * is a work around until we can dynamically set the maximum size.
	     * This value can range between the starting size and the maximum
	     * size but should never be set below the current footprint of the
	     * heap.
	     */
	     <!--可以配置的大小，每个应用都可配置在start跟maximumSize之间-->
	    size_t growthLimit;
	    /* The desired max size of the heap source as a whole.
	     */
	     <!--基于利用率计算出来的-->
	    size_t idealSize;
	    /* The maximum number of bytes allowed to be allocated from the
	     * active heap before a GC is forced.  This is used to "shrink" the
	     * heap in lieu of actual compaction.
	     */
	     <!--触发GC所需要的上限-->
	    size_t softLimit;
	    /* Minimum number of free bytes. Used with the target utilization when
	     * setting the softLimit. Never allows less bytes than this to be free
	     * when the heap size is below the maximum size or growth limit.
	     */
	     <!--设置softLimit所参考的数据，不能低于此-->
	    size_t minFree;
	    /* Maximum number of free bytes. Used with the target utilization when
	     * setting the softLimit. Never allows more bytes than this to be free
	     * when the heap size is below the maximum size or growth limit.
	     */
	         <!--设置softLimit所参考的数据，不能高于此-->
	    size_t maxFree;
	    /* The heaps; heaps[0] is always the active heap,
	     * which new objects should be allocated from.
	     */
	    Heap heaps[HEAP_SOURCE_MAX_HEAP_COUNT];
	    /* The current number of heaps.
	     */
	    size_t numHeaps;
	    /* True if zygote mode was active when the HeapSource was created.
	     */
	    bool sawZygote;
	    /*
	     * The base address of the virtual memory reservation.
	     */
	    char *heapBase;
	    /*
	     * The length in bytes of the virtual memory reservation.
	     */
	    size_t heapLength;
	    /*
	     * The live object bitmap.
	     */
	    HeapBitmap liveBits;
	    /*
	     * The mark bitmap.
	     */
	    HeapBitmap markBits;
	    /*
	     * Native allocations.
	     */
	    int32_t nativeBytesAllocated;
	    size_t nativeFootprintGCWatermark;
	    size_t nativeFootprintLimit;
	    bool nativeNeedToRunFinalization;
	    /*
	     * State for the GC daemon.
	     */
	    bool hasGcThread;
	    pthread_t gcThread;
	    bool gcThreadShutdown;
	    pthread_mutex_t gcThreadMutex;
	    pthread_cond_t gcThreadCond;
	    bool gcThreadTrimNeeded;
	};

启动配置初始化，一开始softLimit不设置限制，所以一开始分配内存不会因为超出softLimit触发GC，但是由于申请的初始内存不够用了，分配失败后，会触发GC，从而，在GC的时候重新调整softLimit，softLimit是GC时候调整，并用于下次的GC判断，
	
	GcHeap* dvmHeapSourceStartup(size_t startSize, size_t maximumSize,
	                             size_t growthLimit)
	{
	     <!--确保设置的startSize maximumSize growthLimit有效-->
	    if (!(startSize <= growthLimit && growthLimit <= maximumSize)) {
	        ALOGE("Bad heap size parameters (start=%zd, max=%zd, limit=%zd)",
	             startSize, maximumSize, growthLimit);
	        return NULL;
	    }
	    /*
	     * Allocate a contiguous region of virtual memory to subdivided
	     * among the heaps managed by the garbage collector.
	     */
	    length = ALIGN_UP_TO_PAGE_SIZE(maximumSize);
	    base = dvmAllocRegion(length, PROT_NONE, gDvm.zygote ? "dalvik-zygote" : "dalvik-heap");
	    if (base == NULL) {
	        return NULL;
	    }
	    <!--基础对象类的内存分配-->
	    /* Create an unlocked dlmalloc mspace to use as
	     * a heap source.
	     */
	    msp = createMspace(base, kInitialMorecoreStart, startSize);
	    if (msp == NULL) {
	        goto fail;
	    }
	    gcHeap = (GcHeap *)calloc(1, sizeof(*gcHeap));
	    if (gcHeap == NULL) {
	        LOGE_HEAP("Can't allocate heap descriptor");
	        goto fail;
	    }
	    hs = (HeapSource *)calloc(1, sizeof(*hs));
	    if (hs == NULL) {
	        LOGE_HEAP("Can't allocate heap source");
	        free(gcHeap);
	        goto fail;
	    }
	    <!--初始化内存管理配置-->
	    hs->targetUtilization = gDvm.heapTargetUtilization * HEAP_UTILIZATION_MAX;
	    <!--调整的依据-->
	    hs->minFree = gDvm.heapMinFree;
	    hs->maxFree = gDvm.heapMaxFree;
	    hs->startSize = startSize;
	    hs->maximumSize = maximumSize;
	    hs->growthLimit = growthLimit;
	    hs->idealSize = startSize;
	    <!--调整依据-->
	    hs->softLimit = SIZE_MAX;    // no soft limit at first
	    hs->numHeaps = 0;
	    hs->sawZygote = gDvm.zygote;
	    hs->nativeBytesAllocated = 0;
	    hs->nativeFootprintGCWatermark = startSize;
	    hs->nativeFootprintLimit = startSize * 2;
	    hs->nativeNeedToRunFinalization = false;
	    hs->hasGcThread = false;
	    hs->heapBase = (char *)base;
	    hs->heapLength = length;
	    if (hs->maxFree > hs->maximumSize) {
	      hs->maxFree = hs->maximumSize;
	    }
	    if (hs->minFree < CONCURRENT_START) {
	      hs->minFree = CONCURRENT_START;
	    } else if (hs->minFree > hs->maxFree) {
	      hs->minFree = hs->maxFree;
	    }
	    if (!addInitialHeap(hs, msp, growthLimit)) {
	        LOGE_HEAP("Can't add initial heap");
	        goto fail;
	    }
	    if (!dvmHeapBitmapInit(&hs->liveBits, base, length, "dalvik-bitmap-1")) {
	        LOGE_HEAP("Can't create liveBits");
	        goto fail;
	    }
	    if (!dvmHeapBitmapInit(&hs->markBits, base, length, "dalvik-bitmap-2")) {
	        LOGE_HEAP("Can't create markBits");
	        dvmHeapBitmapDelete(&hs->liveBits);
	        goto fail;
	    }
	    if (!allocMarkStack(&gcHeap->markContext.stack, hs->maximumSize)) {
	        ALOGE("Can't create markStack");
	        dvmHeapBitmapDelete(&hs->markBits);
	        dvmHeapBitmapDelete(&hs->liveBits);
	        goto fail;
	    }
	    gcHeap->markContext.bitmap = &hs->markBits;
	    <!--赋值，后续GC 分配都用的到-->
	    gcHeap->heapSource = hs;
	    gHs = hs;
	    return gcHeap;
	fail:
	    munmap(base, length);
	    return NULL;
	}
	

申请 mspace_calloc，也可能会失败。
	
	 void* mspace_calloc(mspace msp, size_t n_elements, size_t elem_size) {
	  void* mem;
	  size_t req = 0;
	  mstate ms = (mstate)msp;
	  if (!ok_magic(ms)) {
	    USAGE_ERROR_ACTION(ms,ms);
	    return 0;
	  }
	  if (n_elements != 0) {
	    req = n_elements * elem_size;
	    if (((n_elements | elem_size) & ~(size_t)0xffff) &&
	        (req / n_elements != elem_size))
	      req = MAX_SIZE_T; /* force downstream failure on overflow */
	  }
	  mem = internal_malloc(ms, req);
	  if (mem != 0 && calloc_must_clear(mem2chunk(mem)))
	    memset(mem, 0, req);
	  return mem;
	}

