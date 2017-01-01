---
layout: post
title: "Hanlder的用法与注意事项"
description: "android"
category: android
tags: [android]

---

## Java中的线程安全与非线程安全

### 什么是线程安全？  

关于进程与线程：进程是数据分配的最小单位，线程是任务调度的最小单位，多线程肯定会涉及数据的共享，数据共享的时候，就涉及线程同步的问题，同步是为了防止多个线程访问一个数据对象时，对数据造成的破坏。
    
### 1、StringBuilder与StringBuilder的区别

* Stringbuffer是线程安全的，采用了synchronized，不允许同时进行写操作


		public synchronized StringBuffer append(String string) {
			append0(string);
			return this;
		}
    
* StringBuilder是非线程安全的

    	public StringBuilder append(String str) {
    		append0(str);
    		return this;
    	}


### 2、ArrayList和Vector有什么区别
* ArrayList是非线程安全的
 	
		@Override public boolean add(E object) {
	       Object[] a = array;
	       int s = size;
	       if (s == a.length) {
	           Object[] newArray = new Object[s +
	                   (s < (MIN_CAPACITY_INCREMENT / 2) ?
	                    MIN_CAPACITY_INCREMENT : s >> 1)];
	           System.arraycopy(a, 0, newArray, 0, s);
	           array = a = newArray;
	       }
	       a[s] = object;
	       size = s + 1;
	       modCount++;
	       return true;
	       }

* Vector是线程安全的；

	    @Override
	    public synchronized boolean add(E object) {
	        if (elementCount == elementData.length) {
	            growByOne();
	        }
	        elementData[elementCount++] = object;
	        modCount++;
	        return true;
	    }

### 3、HashMap与HashTable的区别

* HashMap是非线程安全的

	    @Override public V put(K key, V value) {
	        if (key == null) {
	            return putValueForNullKey(value);
	        }

	        int hash = secondaryHash(key.hashCode());
	        HashMapEntry<K, V>[] tab = table;
	        int index = hash & (tab.length - 1);
	        for (HashMapEntry<K, V> e = tab[index]; e != null; e = e.next) {
	            if (e.hash == hash && key.equals(e.key)) {
	                preModify(e);
	                V oldValue = e.value;
	                e.value = value;
	                return oldValue;
	            }
	        }
	
	        // No entry for (non-null) key is present; create one
	        modCount++;
	        if (size++ > threshold) {
	            tab = doubleCapacity();
	            index = hash & (tab.length - 1);
	        }
	        addNewEntry(key, value, hash, index);
	        return null;
		    }
* HashTable是线程安全的

		public synchronized V put(K key, V value) {
	        if (key == null) {
	            throw new NullPointerException("key == null");
	        } else if (value == null) {
	            throw new NullPointerException("value == null");
	        }
	        int hash = secondaryHash(key.hashCode());
	        HashtableEntry<K, V>[] tab = table;
	        int index = hash & (tab.length - 1);
	        HashtableEntry<K, V> first = tab[index];
	        for (HashtableEntry<K, V> e = first; e != null; e = e.next) {
	            if (e.hash == hash && key.equals(e.key)) {
	                V oldValue = e.value;
	                e.value = value;
	                return oldValue;
	            }
	        }
	        modCount++;
	        if (size++ > threshold) {
	            rehash();  // Does nothing!!
	            tab = doubleCapacity();
	            index = hash & (tab.length - 1);
	            first = tab[index];
	        }
	        tab[index] = new HashtableEntry<K, V>(key, value, hash, first);
	        return null;
	    }