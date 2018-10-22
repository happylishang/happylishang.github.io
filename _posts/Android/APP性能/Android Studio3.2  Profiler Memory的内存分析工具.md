
## 内存使用统计问题  

整体内存的使用，看APP heap就可以了，

* Allocations：堆中的实例数。
* Shallow Size：此堆中所有实例的总大小（以字节为单位）。**其实算是比较真实的java堆内存**
* Retained Size：为此类的所有实例而保留的内存总大小（以字节为单位）。**会有重复统计的问题**

举个例子，创建一个List的场景，有一个ListItem40MClass类，自身占用40M内存，每个对象有个指向下一个ListItem40MClass对象的引用，从而构成List，


    class ListItem40MClass {
    
        byte[] content = new byte[1000 * 1000 * 40];
        ListItem40MClass() {
            for (int i = 0; i < content.length; i++) {
                content[i] = 1;
            }
        }

        @Override
        protected void finalize() throws Throwable {
            super.finalize();
        }

        ListItem40MClass next;
    }


    @OnClick(R.id.first)
    void first() {
        if (head == null) {
            head = new ListItem40MClass();
        } else {
            ListItem40MClass tmp = head;
            while (tmp.next != null) {
                tmp = tmp.next;
            }
            tmp.next = new ListItem40MClass();
        }
    }

我们创建三个这样的对象，并形成List，示意如下
	
	A1->next=A2
	A2->next=A3 
	A3->next= null

这个时候用Android Profiler查看内存，会看到如下效果：Retained Size统计要比实际3个ListItem40MClass类对象的大小大的多，如下图：

![281540022720_.pic_hd.jpg](https://upload-images.jianshu.io/upload_images/1460468-a563b20d9b852cc2.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到就总量而言Shallow Size基本能真是反应Java堆内存，而Retained Size却明显要高出不少， 因为Retained Size统计总内存的时候，基本不能避免重复统计的问题，比如：A对象有B对象的引用在计算总的对象大小的时候，一般会多出一个B，就像上图，有个3个约40M的int[]对象，占内存约120M,而每个ListItem40MClass对象至少会再统计一次40M，这里说的是至少，因为对象间可能还有其他关系。我们看下单个类的内存占用-Instance View

* Depth：从任意 GC 根到所选实例的最短 hop 数。
* Shallow Size：此实例的大小。
* Retained Size：此实例支配的内存大小（根据 dominator 树）。

可以看到Head本身的Retained Size是120M ，Head->next 是80M，最后一个ListItem40MClass对象是40M，因为每个对象的Retained Size除了包括自己的大小，还包括引用对象的大小，整个类的Retained Size大小累加起来就大了很多，所以如果想要看整体内存占用，看Shallow Size还是相对准确的，Retained Size可以用来大概反应哪种类占的内存比较多，仅仅是个示意，不过还是Retained Size比较常用，因为Shallow Size的大户一般都是String，数组，基本类型意义不大，如下。

![291540025853_.pic.jpg](https://upload-images.jianshu.io/upload_images/1460468-f1d8100edeecd85b.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
	
## FinalizerReference大小跟内存使用及内存泄漏的关系

之前说Retained Size是此实例支配的内存大小，其实在Retained Size的统计上游很多限制，比如Depth：从任意 GC 根到所选实例的最短hop数，一个对象的Retained Size只会统计Depth比自己打的引用，而不会统计小的，这个可能是为了避免重复统计而引入的，但是其实Retained Size在整体上是免不了重复统计的问题，所以才会右下图的情况：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-8791c7700db8e906.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

FinalizerReference中refrent的对象的retain size是40M，但是没有被计算到FinalizerReference的retain size中去，而且就图表而言FinalizerReference的意义其实不大，FinalizerReference对象本身占用的内存不大，其次FinalizerReference的	retain size统计的可以说是FinalizerReference的重复累加的和，并不代表其引用对象的大小，仅仅是ReferenceQueue<Object> queue中ReferenceQueue的累加，

	public final class FinalizerReference<T> extends Reference<T> {
	    // This queue contains those objects eligible for finalization.
	    public static final ReferenceQueue<Object> queue = new ReferenceQueue<Object>();
	
	    // Guards the list (not the queue).
	    private static final Object LIST_LOCK = new Object();
	
	    // This list contains a FinalizerReference for every finalizable object in the heap.
	    // Objects in this list may or may not be eligible for finalization yet.
	    private static FinalizerReference<?> head = null;
	
	    // The links used to construct the list.
	    private FinalizerReference<?> prev;
	    private FinalizerReference<?> next;
	
	    // When the GC wants something finalized, it moves it from the 'referent' field to
	    // the 'zombie' field instead.
	    private T zombie;
	
	    public FinalizerReference(T r, ReferenceQueue<? super T> q) {
	        super(r, q);
	    }
	
	    @Override public T get() {
	        return zombie;
	    }
	
	    @Override public void clear() {
	        zombie = null;
	    }
	
	    public static void add(Object referent) {
	        FinalizerReference<?> reference = new FinalizerReference<Object>(referent, queue);
	        synchronized (LIST_LOCK) {
	            reference.prev = null;
	            reference.next = head;
	            if (head != null) {
	                head.prev = reference;
	            }
	            head = reference;
	        }
	    }
	
	    public static void remove(FinalizerReference<?> reference) {
	        synchronized (LIST_LOCK) {
	            FinalizerReference<?> next = reference.next;
	            FinalizerReference<?> prev = reference.prev;
	            reference.next = null;
	            reference.prev = null;
	            if (prev != null) {
	                prev.next = next;
	            } else {
	                head = next;
	            }
	            if (next != null) {
	                next.prev = prev;
	            }
	        }
	    }
    ...
	}
 
 
 并且每个FinalizerReference retain size 都是其next+ FinalizerReference的shallowsize，反应的并不是其refrent对象内存的大小，如下：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-97be22e531d4dcc3.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 
 因此FinalizerReference越大只能说明需要执行finalize的对象越多，并且对象是通过强引用被持有，等待Deamon线程回收。可以通过该下代码试验下：
 
	 class ListItem40MClass {
	        byte[] content = new byte[5];
	
	        ListItem40MClass() {
	            for (int i = 0; i < content.length; i += 1000) {
	                content[i] = 1;
	            }
	        }
	
	        @Override
	        protected void finalize() throws Throwable {
	            super.finalize();
	            LogUtils.v("finalize ListItem40MClass");
	        }
	
	        ListItem40MClass next;
	    }
	
	
	    @OnClick(R.id.first)
	    void first() {
	        if (head == null) {
	            head = new ListItem40MClass();
	        } else {
	            for (int i = 0; i < 1000; i++) {
	                ListItem40MClass tmp = head;
	                while (tmp.next != null) {
	                    tmp = tmp.next;
	                }
	                tmp.next = new ListItem40MClass();
	            }
	        }
	    }
    
   多次点击后，需要finalize的对象指向上升，而FinalizerReference却会指数上升。
   
   ![image.png](https://upload-images.jianshu.io/upload_images/1460468-a118ea13d63a20d6.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

而且同之前40M的对比下，明显上一个内存占用更多，但是其实FinalizerReference的retain size却更小。再来理解FinalizerReference跟内存泄漏的关系就比价好理解了，回收线程没执行，实现了finalize方法的对象一直没有被释放，或者很迟才被释放，这个时候其实就算是泄漏了。

## 到底如何看Profile的Memory图

* 其一，看整体Java内存使用看shallowsize就可以了
*  想要看哪些对象占用内存较多，可以看Retained Size，不过看Retained Size的时候，要注意过滤一些无用的比如  FinalizerReference，基本类型如：数组对象

比如下图：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-1b8f948f4a78ebe5.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

其Java用的
