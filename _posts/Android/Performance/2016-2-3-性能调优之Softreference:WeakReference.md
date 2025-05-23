---

layout: post
title: Android GPU呈现模式原理及卡顿掉帧分析
category: Android

---


性能调优之软/弱引用

### 1 .SoftReference软引用

  对于一些本地图片处理，比如图片浏览，可以采用弱引用的方式，如果内存吃紧，就会自动释放，如果不吃紧就不释放，符合图片浏览的需求,如果一个对象只具有软引用，则内存空间足够，垃圾回收器就不会回收它；如果内存空间不足了，就会回收这些对象的内存。只要垃圾回收器没有回收它，该对象就可以被程序使用。软引用可用来实现内存敏感的高速缓存（下文给出示例）。软引用可以和一个引用队列（ReferenceQueue）联合使用，如果软引用所引用的对象被垃圾回收器回收，Java虚拟机就会把这个软引用加入到与之关联的引用队列中。用法如下：注意这里的软引用队列跟普通的不同，这里的是一个待清理的队列，不是用来使用的，每次都要根据该队列进行清理
    
```

	    public class CacheMap <K, V>{
		private Map<K, Entry<K, V>> mCache = new ConcurrentHashMap<K, Entry<K, V>>();
		private ReferenceQueue<V> mQueue = new ReferenceQueue<V>();
		
		private static class Entry<K, V> extends SoftReference<V> {
			K mKey;
			
			public Entry(K key, V value, ReferenceQueue<V> queue) {
				super(value, queue);
				mKey = key;
			}
		}
		
		//这其实是清除软引用，软引用被放到队列中
		private void cleanUpWeakMap() {
	        Entry<K, V> entry = (Entry<K, V>) mQueue.poll();
	        while (entry != null) {
	        	mCache.remove(entry.mKey);
	            entry = (Entry<K, V>) mQueue.poll();
	        }
	    }
	
	    public boolean containsKey(K key) {
	        cleanUpWeakMap();
	        return mCache.containsKey(key);
	    }
	
	    public V put(K key, V value) {
	        cleanUpWeakMap();
	        Entry<K, V> entry = mCache.put(
	                key, new Entry<K, V>(key, value, mQueue));
	        return entry == null ? null : entry.get();
	    }
	
	    public V get(K key) {
	        cleanUpWeakMap();
	        Entry<K, V> entry = mCache.get(key);
	        return entry == null ? null : entry.get();
	    }
	
	    public void clear() {
	        mCache.clear();
	    }
	    
	    public int size(){
	    	return mCache.size();
	    }
	}

```
##### 避免在Android内部使用软引用

> A reference that is cleared when its referent is not strongly reachable and there is memory pressure.

> **Avoid Soft References for Caching**

 In practice, soft references are inefficient for caching. The runtime doesn't have enough information on which references to clear and which to keep. Most fatally, it doesn't know what to do when given the choice between clearing a soft reference and growing the heap.
The lack of information on the value to your application of each reference limits the usefulness of soft references. References that are cleared too early cause unnecessary work; those that are cleared too late waste memory.

 Most applications should use an android.util.LruCache instead of soft references. LruCache has an effective eviction policy and lets the user tune how much memory is allotted.

> **Garbage Collection of Soft References**

When the garbage collector encounters an object obj that is softly-reachable, the following happens:
A set refs of references is determined. refs contains the following elements:
All soft references pointing to obj.
All soft references pointing to objects from which obj is strongly reachable.
All references in refs are atomically cleared.
At the same time or some time in the future, all references in refs will be enqueued with their corresponding reference queues, if any.
The system may delay clearing and enqueueing soft references, yet all SoftReferences pointing to softly reachable objects will be cleared before the runtime throws an OutOfMemoryError.
Unlike a WeakReference, a SoftReference will not be cleared and enqueued until the runtime must reclaim memory to satisfy an allocation.


### 2 .弱引用（WeakReference）

弱引用与软引用的区别在于：只具有弱引用的对象拥有更短暂的生命周期。在垃圾回收器线程扫描它所管辖的内存区域的过程中，一旦发现了只具有弱引用的对象，不管当前内存空间足够与否，都会回收它的内存。不过，由于垃圾回收器是一个优先级很低的线程，因此不一定会很快发现那些只具有弱引用的对象。

弱引用可以和一个引用队列（ReferenceQueue）联合使用，如果弱引用所引用的对象被垃圾回收，Java虚拟机就会把这个弱引用加入到与之关联的引用队列中。

### 3 .虚引用（PhantomReference）

虚引用必须和引用队列(ReferenceQueue)联合使用，程序可以通过判断引用队列中是否已经加入了虚引用，来了解被引用的对象是否将要进行垃圾回收。如果程序发现某个虚引用已经被加入到引用队列，那么就可以在所引用的对象的内存被回收之前采取必要的行动。




> public class PhantomReference<T>extends Reference<T>

Phantom reference objects, which are enqueued after the collector determines that their referents may otherwise be reclaimed. Phantom references are most often used for scheduling pre-mortem cleanup actions in a more flexible way than is possible with the Java finalization mechanism.
If the garbage collector determines at a certain point in time that the referent of a phantom reference is phantom reachable, then at that time or at some later time it will enqueue the reference.

In order to ensure that a reclaimable object remains so, the referent of a phantom reference may not be retrieved: The get method of a phantom reference always returns null.

Unlike soft and weak references, phantom references are not automatically cleared by the garbage collector as they are enqueued. An object that is reachable via phantom references will remain so until all such references are cleared or themselves become unreachable.


虚引用不影响对象的生命周期，但是影响对象的GC，虚引用主要用来跟踪对象被垃圾回收，**只有虚引用本身也变得不可达【不是虚引用指向的对象】，虚引用所指向的对象才会被垃圾回收器GC回收。**


	public final class Daemons {
	    private static final int NANOS_PER_MILLI = 1000 * 1000;
	    private static final int NANOS_PER_SECOND = NANOS_PER_MILLI * 1000;
	    private static final long MAX_FINALIZE_NANOS = 10L * NANOS_PER_SECOND;
	
	    public static void start() {
	        ReferenceQueueDaemon.INSTANCE.start();
	        FinalizerDaemon.INSTANCE.start();
	        FinalizerWatchdogDaemon.INSTANCE.start();
	        HeapTaskDaemon.INSTANCE.start();
	    }
	
	    public static void startPostZygoteFork() {
	        ReferenceQueueDaemon.INSTANCE.startPostZygoteFork();
	        FinalizerDaemon.INSTANCE.startPostZygoteFork();
	        FinalizerWatchdogDaemon.INSTANCE.startPostZygoteFork();
	        HeapTaskDaemon.INSTANCE.startPostZygoteFork();
	    }
	    

清理的时机，自定义清理策略

    private boolean enqueueLocked(Reference<? extends T> r) {
        // Verify the reference has not already been enqueued.
        if (r.queueNext != null) {
            return false;
        }

        if (r instanceof Cleaner) {
            // If this reference is a Cleaner, then simply invoke the clean method instead
            // of enqueueing it in the queue. Cleaners are associated with dummy queues that
            // are never polled and objects are never enqueued on them.
            Cleaner cl = (sun.misc.Cleaner) r;
            <!--此处支持自定义一些清理逻辑-->
            cl.clean();

            // Update queueNext to indicate that the reference has been
            // enqueued, but is now removed from the queue.
            r.queueNext = sQueueNextUnenqueued;
            return true;
        }

        if (tail == null) {
            head = r;
        } else {
            tail.queueNext = r;
        }
        tail = r;
        tail.queueNext = r;
        return true;
    }	    