## BlockingQueue的定义

BlockingQueue的定义：A Queue that additionally supports operations that wait for the queue to become non-empty when retrieving an element, and wait for space to become available in the queue when storing an element. 即BlockingQueue是一个支持阻塞的队列，如果添加时已满，或者获取是为空，都支持阻塞等待，在Java并发包中BlockingQueue是一个接口，并且满足线程安全，任何实现该接口的类都应该满足该要求，并提供上述的阻塞功能。Java中实现该接口的类有：

* ArrayBlockingQueue	一个由数组结构组成的有界阻塞队列。
* SynchronousQueue	一个不存储元素的阻塞队列，即直接提交给线程不保持它们
* LinkedBlockingQueue	一个由链表结构组成的可选有界阻塞队列。
* PriorityBlockingQueue	一个支持优先级排序的无界阻塞队列。
* DelayQueue	一个使用优先级队列实现的无界阻塞队列，只有在延迟期满时才能从中提取元素。
* LinkedTransferQueue	一个由链表结构组成的无界阻塞队列。与SynchronousQueue类似，还含有非阻塞方法。
* TransferQueue  LinkedTransferQueue 当生产者利用transfer()方法发送消息给消费者时，生产者将一直被阻塞，直到消息被使用为止

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

### SynchronousQueue	一个不存储元素的阻塞队列，直接提交给消费线程

SynchronousQueue不存储元素，所以内部所有集合相关的操作都没有意义，A blocking queue in which each insert operation must wait for a corresponding remove operation by another thread, and vice versa. A synchronous queue does not have any internal capacity, not even a capacity of one

    public SynchronousQueue(boolean fair) {
        transferer = fair ? new TransferQueue<E>() : new TransferStack<E>();
    }

使用SynchronousQueue的目的就是保证“对于提交的任务，如果有空闲线程，则使用空闲线程来处理；否则新建一个线程来处理任务


### LinkedBlockingQueue	一个由链表结构组成的可选有界阻塞队列 

    /** Lock held by take, poll, etc */
    private final ReentrantLock takeLock = new ReentrantLock();

    /** Wait queue for waiting takes */
    private final Condition notEmpty = takeLock.newCondition();

    /** Lock held by put, offer, etc */
    private final ReentrantLock putLock = new ReentrantLock();

两个ReentrantLock可重入非公平锁