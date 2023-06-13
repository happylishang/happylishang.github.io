## BlockingQueue的定义

BlockingQueue的定义：A Queue that additionally supports operations that wait for the queue to become non-empty when retrieving an element, and wait for space to become available in the queue when storing an element. 即BlockingQueue是一个支持阻塞的队列，如果添加时已满，或者获取是为空，都支持阻塞等待，在Java并发包中BlockingQueue是一个接口，并且满足线程安全，任何实现该接口的类都应该满足该要求，并提供上述的阻塞功能。Java中实现该接口的类有：

* ArrayBlockingQueue	一个由数组结构组成的有界阻塞队列。
* SynchronousQueue	一个不存储元素的阻塞队列，即直接提交给线程不保持它们
* LinkedBlockingQueue	一个由链表结构组成的可选有界阻塞队列。
* PriorityBlockingQueue	一个支持优先级排序的无界阻塞队列。
* DelayQueue	一个使用优先级队列实现的无界阻塞队列，只有在延迟期满时才能从中提取元素。
* LinkedTransferQueue	一个由链表结构组成的无界阻塞队列。与SynchronousQueue类似，还含有非阻塞方法。

可以根据具体的实现来看看BlockingQueue家族，先看下BlockingQueue几个常用抽象方法：

* add 若超出了度列的长度会直接抛出IllegalStateException异常
* remove:若队列为空，抛出NoSuchElementException
* offer方法在添加元素时，如果发现队列已满无法添加的话，会直接返回false。
* poll: 若队列为空，返回null。

就表现来看似乎是offer优于add，如果移除最早的元素，poll优于remove，LinkBlockQueue、SyncBlockQueue都是继承AbstractQueue，它们的add与remove也继承AbstractQueue，其实offer/poll的封装

    public boolean add(E e) {
        if (offer(e))
            return true;
        else
            throw new IllegalStateException("Queue full");
    }
    
    public E remove() {
    E x = poll();
    if (x != null)
        return x;
    else
        throw new NoSuchElementException();
}

* put：添加元素的时候发现队列已满，则阻塞等待，等待有位置
* take:获取元素时，若队列为空，发生阻塞，等待有元素。

put跟take都牵扯阻塞，即线程的睡眠与唤醒。不同的实现类做法可能不同。

* drainTo批量获取并清空数据，也是要保证线程安全的


### ArrayBlockingQueue实现

ArrayBlockingQueue是用数组实现的BlockingQueue，是一个有界阻塞队列

	  /**
	     * Creates an {@code ArrayBlockingQueue} with the given (fixed)
	     * capacity and the specified access policy.
	     *
	     * @param capacity the capacity of this queue
	     * @param fair if {@code true} then queue accesses for threads blocked
	     *        on insertion or removal, are processed in FIFO order;
	     *        if {@code false} the access order is unspecified.
	     * @throws IllegalArgumentException if {@code capacity < 1}
	     */
	    public ArrayBlockingQueue(int capacity, boolean fair) {
	        if (capacity <= 0)
	            throw new IllegalArgumentException();
	         <!--对象数组-->
	        this.items = new Object[capacity];
	        lock = new ReentrantLock(fair);
	        notEmpty = lock.newCondition();
	        notFull =  lock.newCondition();
	    }

ArrayBlockingQueue一般都是使用

    public ArrayBlockingQueue(int capacity) {
        this(capacity, false);
    }

所以其实使用的是非公平锁ReentrantLock(false)，并且根据capacity，构建相应容量的数组。同时为了构建生产者-消费者模型还创建了notEmpty跟notFull两个条件变量，直接看下它如何利用锁添加元素的offer

    public boolean offer(E e) {
        checkNotNull(e);
        final ReentrantLock lock = this.lock;
        lock.lock();
        try {
            if (count == items.length)
                return false;
            else {
                enqueue(e);
                return true;
            }
        } finally {
            lock.unlock();
        }
    }

利用lock.lock获取锁，如果达到容量，直接返回false否则，利用enqueue插入，offer是非阻塞的操作，ReentrantLock在这里只起互斥锁的作用，

    private void enqueue(E x) {
        // assert lock.getHoldCount() == 1;
        // assert items[putIndex] == null;
        final Object[] items = this.items;
        items[putIndex] = x;
        if (++putIndex == items.length)
            putIndex = 0;
        count++;
        notEmpty.signal();
    }

插入成功后利用notEmpty.signal()通知可能在等待的消费者，再看一下可能会阻塞的put函数


    /**
     * Inserts the specified element at the tail of this queue, waiting
     * for space to become available if the queue is full.
     *
     * @throws InterruptedException {@inheritDoc}
     * @throws NullPointerException {@inheritDoc}
     */
    public void put(E e) throws InterruptedException {
        checkNotNull(e);
        final ReentrantLock lock = this.lock;
        lock.lockInterruptibly();
        try {
            while (count == items.length)
                notFull.await();
            enqueue(e);
        } finally {
            lock.unlock();
        }
    }
    
可以看到，这里采用的是notFull.await，如果队列满了，则利用notFull.await等待，同时将锁释放，相对应的看下消费：


    public E poll() {
        final ReentrantLock lock = this.lock;
        lock.lock();
        try {
            return (count == 0) ? null : dequeue();
        } finally {
            lock.unlock();
        }
    }

    public E take() throws InterruptedException {
        final ReentrantLock lock = this.lock;
        lock.lockInterruptibly();
        try {
            while (count == 0)
                notEmpty.await();
            return dequeue();
        } finally {
            lock.unlock();
        }
    }
    
    
    /**
     * Extracts element at current take position, advances, and signals.
     * Call only when holding lock.
     */
    private E dequeue() {
        // assert lock.getHoldCount() == 1;
        // assert items[takeIndex] != null;
        final Object[] items = this.items;
        @SuppressWarnings("unchecked")
        E x = (E) items[takeIndex];
        items[takeIndex] = null;
        if (++takeIndex == items.length)
            takeIndex = 0;
        count--;
        if (itrs != null)
            itrs.elementDequeued();
        notFull.signal();
        return x;
    }


poll是非阻塞的似的，而take是阻塞的，如果缓冲池是空，则阻塞等待notEmpty.await，正对应了上述的notEmpty.signal，消费过后，队列就不再满，发出notFull.signal()信号，通知可以有新的元素插入，对应put函数里的notFull.wait。可以看到通过ReenTrantLokc跟其ConditionObject完成了ArrayBlockingQueue的生产消费模型，ReenTrantLokc是互斥的核心，ConditionObject是同步的核心。

### LinkedBlockingQueue	一个由链表结构组成的可选有界阻塞队列 

同ArrayBlockingQueue最大的不同是LinkedBlockingQueue采用的是链表作为缓冲区数据结构，同时它采用了两个锁，一个写锁，一个读锁，读写操作互斥分离，一定程度上能提高执行效率。
	
	  /**
	     * Head of linked list.
	     * Invariant: head.item == null
	     */
	    transient Node<E> head;
	
	    /**
	     * Tail of linked list.
	     * Invariant: last.next == null
	     */
	    private transient Node<E> last;
	
	    /** Lock held by take, poll, etc */
	    private final ReentrantLock takeLock = new ReentrantLock();
	
	    /** Wait queue for waiting takes */
	    private final Condition notEmpty = takeLock.newCondition();
	
	    /** Lock held by put, offer, etc */
	    private final ReentrantLock putLock = new ReentrantLock();
	
	    /** Wait queue for waiting puts */
	    private final Condition notFull = putLock.newCondition(); //Condition跟谁一起用在阻塞上，就用谁获取


LinkBlockQueue可以设置容量，如果不设置，就是默认Integer.MAX_VALUE，构造之初同时搞定链表的初始化

    public LinkedBlockingQueue(int capacity) {
        if (capacity <= 0) throw new IllegalArgumentException();
        this.capacity = capacity;
        last = head = new Node<E>(null);
    }

先看看offer操作是如何使用写锁进行同步的，

	
	  /**
	     * Inserts the specified element at the tail of this queue if it is
	     * possible to do so immediately without exceeding the queue's capacity,
	     * returning {@code true} upon success and {@code false} if this queue
	     * is full.
	     * When using a capacity-restricted queue, this method is generally
	     * preferable to method {@link BlockingQueue#add add}, which can fail to
	     * insert an element only by throwing an exception.
	     *
	     * @throws NullPointerException if the specified element is null
	     */
	    public boolean offer(E e) {
	        if (e == null) throw new NullPointerException();
	        final AtomicInteger count = this.count;
	        if (count.get() == capacity)
	            return false;
	        int c = -1;
	        Node<E> node = new Node<E>(e);
	        <!--获取写锁-->
	        final ReentrantLock putLock = this.putLock;
	        putLock.lock();
	        try {
	            if (count.get() < capacity) {
	                enqueue(node);
	                <!--？ 都已经互斥了为什么还要这样 其他地方有用count 该操作保证用count的时候值正确，且更新正确，理论上多线程的地方用到同一变量都应该这样，可能防止变量的异常使用吧-->
	                c = count.getAndIncrement();
	                <!--这里是为了什么？方便直接唤起另一个线程，因为到这里容量可能已经变化了，如果有其他的写等待，则可以唤醒提高效率，利用notFull.signal随机唤起等待池中的一个线程进入锁池-->
	                if (c + 1 < capacity)
	                <!--不满-->
	                    notFull.signal();
	            }
	        } finally {
	            putLock.unlock();
	        }
	        if (c == 0)
	        <!--非空，写入了，可以来去了-->
	            signalNotEmpty();
	        return c >= 0;
	    }
	    
	        /**
     * Links node at end of queue.
     *
     * @param node the node  先进先出 enqueue在队尾rear插入元素
     */
    private void enqueue(Node<E> node) { 
    	 <!--队尾插入元素-->
        // assert putLock.isHeldByCurrentThread();
        // assert last.next == null;
        last = last.next = node;
    }

流程很清楚，如果没有超过用量，就入队，由于入队操作的锁只有一把，所以一定可以保证入队没问题，入队后，其实容量不一定跟入队前对应起来，因为出队的操作可以同步进行了，入队，修改last的next指针，而出队，head后移即可，如下

    public E poll() {
        final AtomicInteger count = this.count;
        if (count.get() == 0)
            return null;
        E x = null;
        int c = -1;
        final ReentrantLock takeLock = this.takeLock;
        takeLock.lock();
        try {
            if (count.get() > 0) {
                x = dequeue();
                c = count.getAndDecrement();
                <!--可以唤起一个一个等待池的线程，进入锁池子，非null-->
                if (c > 1)
                <!--还是非空，可能是为了进一步提高效率->
                    notEmpty.signal();
            }
        } finally {
            takeLock.unlock();
        }
        <!--如果开始是满的，那么拿掉一个后，就有位置了，而且，只有一个锁可以take，所以可以通知放进来一个-->
        if (c == capacity)
        <!--不满-->
            signalNotFull();
        return x;
    }

    /**
     * Removes a node from head of queue.
     *
     * @return the node
     */
    private E dequeue() {
        // assert takeLock.isHeldByCurrentThread();
        // assert head.item == null;
        Node<E> h = head;
        Node<E> first = h.next;
        h.next = h; // help GC
        head = first;
        E x = first.item;
        first.item = null;
        return x;
    }
    
可以看到，poll出队，需要容量不为0，如果为零则直接返回，如果容量不为零，取最早的那个，并且head后移，如果去之前是满的，则可以唤起写线程，否则说明没必要唤起，本来就没阻塞，看看LinkBlockQueue的put/take阻塞方法的实现
	
	
	  /**
	     * Inserts the specified element at the tail of this queue, waiting if
	     * necessary for space to become available.
	     *
	     * @throws InterruptedException {@inheritDoc}
	     * @throws NullPointerException {@inheritDoc}
	     */
	    public void put(E e) throws InterruptedException {
	        if (e == null) throw new NullPointerException();
	        // Note: convention in all put/take/etc is to preset local var
	        // holding count negative to indicate failure unless set.
	        int c = -1;
	        Node<E> node = new Node<E>(e);
	        final ReentrantLock putLock = this.putLock;
	        final AtomicInteger count = this.count;
	        putLock.lockInterruptibly();
	        try {
	            /*
	             * Note that count is used in wait guard even though it is
	             * not protected by lock. This works because count can
	             * only decrease at this point (all other puts are shut
	             * out by lock), and we (or some other waiting put) are
	             * signalled if it ever changes from capacity. Similarly
	             * for all other uses of count in other wait guards.
	             */
	            while (count.get() == capacity) {
	                notFull.await();
	            }
	            enqueue(node);
	            c = count.getAndIncrement();
	            if (c + 1 < capacity)
	                notFull.signal();
	        } finally {
	            putLock.unlock();
	        }
	        if (c == 0)
	            signalNotEmpty();
	    }
	
同offer方法不同的是，这里也会检查容量，如果熔炼满则通过notFull.await阻塞等待，相应的take方法也是如此，只不过take方法用的是另一个锁，两个锁相互独立，而且得益于链表一个操作头，一个操作尾，写跟读可以完全并行，提高了Queue的效率，为什么两个锁还能保持队列的安全？

* 1、一个操作的是头、一个操作的是尾部，操作的对象一般不会冲突
* 2、最多是一对一的take+put
* 3：take操作以count.get()>0为前提，**必须有一个插入成功了才会take**，只有自己一个线程会getAndDecrement，保证了Head的原子性，在take期间不会有人操作Head以及Head后的一个元素。
* 4：AtomicInteger count ，原子操作性保证了count值的准确性 ，count值更新时候不会出现混乱、覆盖
* 5 ：初始化的时候 last = head = new Node(null) count =0 保证了take/put不会撞头，理论上说AtomicInteger的更新逻辑里应该有CAS可能还有自旋的存在。
* 6： put操作的是last，在put的时候take永远动不到last,两者之间必定有一个有效数据，否则take不运行

put/take锁保证了同一时刻最多只有一个take与一个put，那么要处理的就是这两个是否存在安全问题，如果两者完全岔开肯定没问题，如果同时操作在put的同时take会如何
    
    public void put(E e) throws InterruptedException {
   		 ...

            enqueue(node);
            c = count.getAndIncrement();
            if (c + 1 < capacity)
                notFull.signal();
                
enqueue后更新count.getAndIncrement，如果在更新前take，由于count.get还是旧的，数量一定还是0，

	    public E take() throws InterruptedException {
	        final E x;
	        final int c;
	        final AtomicInteger count = this.count;
	        final ReentrantLock takeLock = this.takeLock;
	        takeLock.lockInterruptibly();
	        try {
	            while (count.get() == 0) {
	                notEmpty.await();
	            }
	            x = dequeue();
	            c = count.getAndDecrement();
	            if (c > 1)
	                notEmpty.signal();
	        }
	        
take就会阻塞等c = count.getAndIncrement();完成，之后  signalNotEmpty();即使恰好被用了，signalNotEmpty也不会有什么问题。
    

### PriorityBlockingQueue	一个支持优先级排序的无界阻塞队列

PriorityBlockingQueue比较显著的特点就是支持优先级，An unbounded {@linkplain BlockingQueue blocking queue} that uses the same ordering rules as class {@link PriorityQueue} and supplies blocking retrieval operations，PriorityBlockingQueue内部的实现是依赖数组实现的一个二叉堆定义为无界，则一定存在数组的增长问题。

    /**
     * Creates a {@code PriorityBlockingQueue} with the specified initial
     * capacity that orders its elements according to the specified
     * comparator.
     *
     * @param initialCapacity the initial capacity for this priority queue
     * @param  comparator the comparator that will be used to order this
     *         priority queue.  If {@code null}, the {@linkplain Comparable
     *         natural ordering} of the elements will be used.
     * @throws IllegalArgumentException if {@code initialCapacity} is less
     *         than 1
     */
    public PriorityBlockingQueue(int initialCapacity,
                                 Comparator<? super E> comparator) {
        if (initialCapacity < 1)
            throw new IllegalArgumentException();
           <!-- 一个独占锁，控制同时只有一个线程在入队和出队-->
        this.lock = new ReentrantLock();
        <!--读线程的唤起  只有一个等待队列notEmpty-->
        this.notEmpty = lock.newCondition();
        this.comparator = comparator;
        this.queue = new Object[initialCapacity];
    }


由于是无界的，入队一定成功，所以一些函数都可以归为offer，包括put，

    public void put(E e) {
        offer(e); // never need to block
    }

    public boolean offer(E e) {
        if (e == null)
            throw new NullPointerException();
        final ReentrantLock lock = this.lock;
        lock.lock();
        int n, cap;
        Object[] array;
        while ((n = size) >= (cap = (array = queue).length))
            tryGrow(array, cap);
        try {
            Comparator<? super E> cmp = comparator;
            <!--是否设置比较器，如果没有用默认的-->
            if (cmp == null)
                siftUpComparable(n, e, array);
            else
                siftUpUsingComparator(n, e, array, cmp);
            size = n + 1;
            notEmpty.signal();
        } finally {
            lock.unlock();
        }
        return true;
    }

利用ReetrantLock处理互斥访问，tryGrow负责判断是不是容量是不是满了，如果满了则进行扩展，扩展的速度跟当前容量有关，越小扩展越快，扩展之后，就是插入元素，插入元素的时候，根据是否设置比较器选择如何插入，默认插入的元素都是Comparable，根据二叉堆的插入规则，选择合适的位置插入数据，之后利用notEmpty.signal将等待消费的线程唤醒。

    private static <T> void siftUpComparable(int k, T x, Object[] array) {
        Comparable<? super T> key = (Comparable<? super T>) x;
        while (k > 0) {
            int parent = (k - 1) >>> 1;
            Object e = array[parent];
            if (key.compareTo((T) e) >= 0)
                break;
            array[k] = e;
            k = parent;
        }
        array[k] = key;
    }

这里的消费可以看出优先级的概念，找到优先级最高的元素。

    private E dequeue() {
        int n = size - 1;
        if (n < 0)
            return null;
        else {
            Object[] array = queue;
            E result = (E) array[0];
            E x = (E) array[n];
            array[n] = null;
            Comparator<? super E> cmp = comparator;
            if (cmp == null)
                siftDownComparable(0, x, array, n);
            else
                siftDownUsingComparator(0, x, array, n, cmp);
            size = n;
            return result;
        }
    }
    


### SynchronousQueue	一个不存储元素的阻塞队列，直接提交给消费线程

SynchronousQueue不存储元素，所以内部所有集合相关的操作都没有意义，A blocking queue in which each insert operation must wait for a corresponding remove operation by another thread, and vice versa. A synchronous queue does not have any internal capacity, not even a capacity of one

    public SynchronousQueue(boolean fair) {
        transferer = fair ? new TransferQueue<E>() : new TransferStack<E>();
    }

使用SynchronousQueue最显著的特点是，在插入时候，如果没有读线程在等待，则一直等待到右线程释放，当然如果通过offer操作来，是直接返回，算提交失败，SynchronousQueue没有存储的数据结构。

### DelayQueue

DelayQueue是一个无界有序的BlockingQueue，用于放置实现了Delayed接口的对象，对象只能在到期时才能从队列中取走，没怎么用过

## 总结

* BlockingQueue很适合生产者消费者模型
* LinkBlockQueue有两个锁
* PriorityBlockQueue支持优先级
* SynchronousQueue没有存储随到随消费
* BlockingQueue的核心ReetrantLock