List是个泛型，运行时检查？

        List a = new ArrayList();
        List<String> b;
        List<Integer> c = a;    运行错误
     // List<Integer> d = b;  编译错误 


####  ArrayList [基于数组的链表]初始容量与扩容

    private void ensureCapacityInternal(int minCapacity) {
        if (elementData == DEFAULTCAPACITY_EMPTY_ELEMENTDATA) {
            minCapacity = Math.max(DEFAULT_CAPACITY, minCapacity);
        }

        ensureExplicitCapacity(minCapacity);
    }

    private void ensureExplicitCapacity(int minCapacity) {
        modCount++;

        // overflow-conscious code
        if (minCapacity - elementData.length > 0)
            grow(minCapacity);
    }

    private void grow(int minCapacity) {
        // overflow-conscious code
        int oldCapacity = elementData.length;
        int newCapacity = oldCapacity + (oldCapacity >> 1);
        if (newCapacity - minCapacity < 0)
            newCapacity = minCapacity;
        if (newCapacity - MAX_ARRAY_SIZE > 0)
            newCapacity = hugeCapacity(minCapacity);
        // minCapacity is usually close to size, so this is a win:
        elementData = Arrays.copyOf(elementData, newCapacity);
    }
    
        /**
     * Default initial capacity.
     */
    private static final int DEFAULT_CAPACITY = 10;
    

* 默认构造函数是0，但是如果插入，就直接扩展到10，后续会根据size扩展，扩展容量为原来的1.5倍，
* 如果是自定容量，扩展的方式也是如此，只是初始的时候不一样，
* 另外ArrayList虽然可以扩容，但是它不会缩容，只会将对应位置的引用设置为null remove如果是整数，要记得封装，防止混淆remove方法Integer转换



#### LinkedList [双向链表] 基于前驱后继

    private static class Node<E> {
        E item;
        Node<E> next;
        Node<E> prev;

        Node(Node<E> prev, E element, Node<E> next) {
            this.item = element;
            this.next = next;
            this.prev = prev;
        }
    }
    
	    public class LinkedList<E>
	    extends AbstractSequentialList<E>
	    implements List<E>, Deque<E>, Cloneable, java.io.Serializable {
	    transient int size = 0;
	
	    /**
	     * Pointer to first node.
	     */
	    transient Node<E> first;
	
	    /**
	     * Pointer to last node.
	     */
	    transient Node<E> last;
	
	    /*
	    void dataStructureInvariants() {
	        assert (size == 0)
	            ? (first == null && last == null)
	            : (first.prev == null && last.next == null);
	    }
	    */
	
	    /**
	     * Constructs an empty list.
	     */
	    public LinkedList() {
	    }

LinkedList 节点具有前驱跟后继指针，并且具有first last指针，所以实现栈、队列非常方便。也是collection接口跟list接口的实现类。其次还实现了双端队列Deque接口，

Deque用作栈：

	push
	pop
	peek

Deque用作队列：

	add
	poll
	peek	


#### HashMap 