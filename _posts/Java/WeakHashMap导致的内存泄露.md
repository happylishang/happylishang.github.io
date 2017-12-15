---
layout: post
title: "WeakHashMap导致的内存泄露"
description: "Java"
category: android
tags: [Binder]

---

#### WeakHashMap原理分析


WeakHashMap与其他 Map 最主要的不同之处在于其 key 是弱引用类型，注意是key是弱引用类型，而不是Value，每次put与get都会更新，如果key被回收了，那么value会被主动清楚，但是如果value显示的引用了key，那就会导致key无法释放。
		
		
		/**
	  * The entries in this hash table extend WeakReference, using its main ref
	  * field as the key.
	  */
	 private static class Entry<K,V> extends WeakReference<Object> implements Map.Entry<K,V> {
	     V value;
	     int hash;
	     Entry<K,V> next;
	     /**
	      * Creates new entry.
	      */
	     Entry(Object key, V value,
	           ReferenceQueue<Object> queue,
	           int hash, Entry<K,V> next) {
	         //这里把key传给了父类WeakReference，说明key为弱引用（没有显式的 this.key = key）
	         //所有如果key只有通过弱引用访问时，key会被 GC 清理掉
	         //同时该key所代表的Entry会进入queue中，等待被处理
	         //还可以看到value为强引用（有显式的 this.value = value ），但这并不影响
	         //后面可以看到WeakHashMap.expungeStaleEntries方法是如何清理value的
	         super(key, queue);
	         this.value = value;
	         this.hash  = hash;
	         this.next  = next;
	     }
	     @SuppressWarnings("unchecked")
	     //在获取key时需要unmaskNull，因为对于null的key，是用WeakHashMap的内部成员属性来表示的
	     public K getKey() {
	         return (K) WeakHashMap.unmaskNull(get());
	     }
	     public V getValue() {
	         return value;
	     }
	     public V setValue(V newValue) {
	         V oldValue = value;
	         value = newValue;
	         return oldValue;
	     }
	     public boolean equals(Object o) {
	         if (!(o instanceof Map.Entry))
	             return false;
	         Map.Entry<?,?> e = (Map.Entry<?,?>)o;
	         K k1 = getKey();
	         Object k2 = e.getKey();
	         if (k1 == k2 || (k1 != null && k1.equals(k2))) {
	             V v1 = getValue();
	             Object v2 = e.getValue();
	             if (v1 == v2 || (v1 != null && v1.equals(v2)))
	                 return true;
	         }
	         return false;
	     }
	     public int hashCode() {
	         K k = getKey();
	         V v = getValue();
	         return ((k==null ? 0 : k.hashCode()) ^
	                 (v==null ? 0 : v.hashCode()));
	     }
	     public String toString() {
	         return getKey() + "=" + getValue();
	     }
	 }
 
 
	/**
	 * Reference queue for cleared WeakEntries
	 */
	// 所有Entry在构造时都传入该queue
	private final ReferenceQueue<Object> queue = new ReferenceQueue<>();
	/**
	 * Expunges stale entries from the table.
	 */
	 
	private void expungeStaleEntries() {
	    for (Object x; (x = queue.poll()) != null; ) {
	        synchronized (queue) {
	            // e 为要清理的对象
	            @SuppressWarnings("unchecked")
	                Entry<K,V> e = (Entry<K,V>) x;
	            int i = indexFor(e.hash, table.length);
	            Entry<K,V> prev = table[i];
	            Entry<K,V> p = prev;
	            // while 循环遍历冲突链
	            while (p != null) {
	                Entry<K,V> next = p.next;
	                if (p == e) {
	                    if (prev == e)
	                        table[i] = next;
	                    else
	                        prev.next = next;
	                        
	                    // Must not null out e.next;
	                    // stale entries may be in use by a HashIterator
	                    // 可以看到这里把value赋值为null，来帮助 GC 回收强引用的value
	                    e.value = null; // Help GC
	                    size--;
	                    break;
	                }
	                prev = p;
	                p = next;
	            }
	        }
	    }
	}
参考文档 

<http://blog.csdn.net/lyfi01/article/details/6415726>

<http://liujiacai.net/blog/2015/09/27/java-weakhashmap/>
<http://www.cnblogs.com/skywang12345/p/3311092.html>

####  在Value里面引用Key

> WeakHashMap Implementation note: 

出现场景

出现原因：

The value objects in a WeakHashMap are held by ordinary strong references. Thus care should be taken to ensure that value objects do not strongly refer to their own keys, either directly or indirectly, since that will prevent the keys from being discarded. 

Note that a value object may refer indirectly to its key via the WeakHashMap itself; that is, a value object may strongly refer to some other key object whose associated value object, in turn, strongly refers to the key of the first value object.

解决方案：

One way to deal with this is to wrap values themselves within WeakReferences before inserting, as in: m.put(key, new WeakReference(value)), and then unwrapping upon each get.