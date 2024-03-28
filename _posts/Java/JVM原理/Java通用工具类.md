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
    

* 默认构造函数是0，但是如果插入，就直接扩展到10，后续会根据size扩展，扩展容量为原来的**1.5**倍，
* 如果是自定容量，扩展的方式也是如此，只是初始的时候不一样，
* 另外ArrayList虽然可以扩容，但是它不会缩容，只会将对应位置的引用设置为null **remove如果是整数，要记得封装，防止混淆remove方法Integer转换**

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


### HashMap、LinkedList、ArrayList 如何遍历+删除【foreach】 用iterator

        List<String> list = new ArrayList<>();
        List<String> list2 = new LinkedList<>();

        for (int i = 0; i < 10; i++) {
            list.add(String.valueOf(i));
            list2.add(String.valueOf(i));
        }
        for (String item : list) {
            list.remove(item);
        }
 
        
这里ConcurrentModificationException被抛出，为什么呢，      for (String item : list)其实是java foreach写法，foreach 语句是一种特别的循环结构，仅用于遍历数组或实现了 Iterable 接口的集合（如 List，Set 等）。在编译之后，foreach 语句会被编译为使用迭代器的循环语句。上述foreach会被编译为

	  	for(int i = 0; i < 10; ++i) {
	            list.add(String.valueOf(i));
	            list2.add(String.valueOf(i));
	        }
	
	        Iterator var9 = list.iterator();
	        String item;
	        while(var9.hasNext()) {
	            item = (String)var9.next();
	            list.remove(item);
	        }
 

但是正确的写法应该是：

        Iterator<String> iterator = list.iterator();
        while(iterator.hasNext()) {
            iterator.next();
            iterator.remove();
        }
        
区别在于 iterator.remove()还是list.remove(item)，每次iterator都会构建一个Iterator，并调用都会        

	 private Itr() {
            this.expectedModCount = AbstractList.this.modCount;
        }
设置  this.expectedModCount = AbstractList.this.modCount，迭代器模式下，如果使用list的remove，会导致  this.expectedModCount = AbstractList.this.modCount;不同步，这样在第二次执行next函数时候

        public E next() {
            this.checkForComodification();
 		...
 		
this.checkForComodification会抛出异常

        final void checkForComodification() {
            if (ArrayList.this.modCount != this.expectedModCount) {
                throw new ConcurrentModificationException();
            }
        }

在迭代器的while循环中，要保证所有remove都是通过迭代器完成的迭代器模式下，似乎没有添加元素的操作，如果是添加，只能自己通过for + size动态修改来完成，当然删除也可以，但是不能forech

 
        int size = strList.size();
        for(int i=0;i<size;i++){
            String tmp = strList.get(i);
            if(i==0){
                strList.add(3,"newStr");
                size++;
            }
      }


>  * <p>The iterators returned by this class's {@code iterator} and
>  * {@code listIterator} methods are <i>fail-fast</i>: if the list is
>  * structurally modified at any time after the iterator is created, in
>  * any way except through the Iterator's own {@code remove} or
>  * {@code add} methods, the iterator will throw a {@link
>  * ConcurrentModificationException}.  Thus, in the face of concurrent
>  * modification, the iterator fails quickly and cleanly, rather than
>  * risking arbitrary, non-deterministic behavior at an undetermined
>  * time in the future.
 * 


 modCount == modifyCount


    /**
     * The number of times this list has been <i>structurally modified</i>.
     * Structural modifications are those that change the size of the
     * list, or otherwise perturb it in such a fashion that iterations in
     * progress may yield incorrect results.
     *
     * <p>This field is used by the iterator and list iterator implementation
     * returned by the {@code iterator} and {@code listIterator} methods.
     * If the value of this field changes unexpectedly, the iterator (or list
     * iterator) will throw a {@code ConcurrentModificationException} in
     * response to the {@code next}, {@code remove}, {@code previous},
     * {@code set} or {@code add} operations.  This provides
     * <i>fail-fast</i> behavior, rather than non-deterministic behavior in
     * the face of concurrent modification during iteration.
     *
     * <p><b>Use of this field by subclasses is optional.</b> If a subclass
     * wishes to provide fail-fast iterators (and list iterators), then it
     * merely has to increment this field in its {@code add(int, E)} and
     * {@code remove(int)} methods (and any other methods that it overrides
     * that result in structural modifications to the list).  A single call to
     * {@code add(int, E)} or {@code remove(int)} must add no more than
     * one to this field, or the iterators (and list iterators) will throw
     * bogus {@code ConcurrentModificationExceptions}.  If an implementation
     * does not wish to provide fail-fast iterators, this field may be
     * ignored.
     */
    protected transient int modCount = 0;



* 用迭代其模式，迭代器模式中的Iterator保证了正确的链接
        
	for (Iterator<Map.Entry<String, Integer>> it = myHashMap.entrySet().iterator(); it.hasNext();){
	    Map.Entry<String, Integer> item = it.next();
	    if ( xxx ) { it.remove(); }
	}
	
	