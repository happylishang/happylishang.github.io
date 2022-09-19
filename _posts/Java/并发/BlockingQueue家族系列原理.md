## BlockingQueue的定义

BlockingQueue的定义：A Queue that additionally supports operations that wait for the queue to become non-empty when retrieving an element, and wait for space to become available in the queue when storing an element. 即BlockingQueue是一个支持阻塞的队列，如果添加时已满，或者获取是为空，都支持阻塞等待，在Java并发包中BlockingQueue是一个接口，并且满足线程安全，任何实现该接口的类都应该满足该要求，并提供上述的阻塞功能。Java中实现该接口的类有：

* ArrayBlockingQueue	一个由数组结构组成的有界阻塞队列。
* SynchronousQueue	一个不存储元素的阻塞队列，即直接提交给线程不保持它们
* PriorityBlockingQueue	一个支持优先级排序的无界阻塞队列。
* LinkedBlockingQueue	一个由链表结构组成的可选有界阻塞队列。
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

所以其实使用的是非公平锁ReentrantLock(false)，并且根据capacity构建出现数组容量。



    /** Lock held by take, poll, etc */
    private final ReentrantLock takeLock = new ReentrantLock();

    /** Wait queue for waiting takes */
    private final Condition notEmpty = takeLock.newCondition();

    /** Lock held by put, offer, etc */
    private final ReentrantLock putLock = new ReentrantLock();

两个ReentrantLock可重入非公平锁