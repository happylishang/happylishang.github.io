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
        
区别在于 iterator.remove()还是list.remove(item)，每次iterator都会构建一个Iterator，并调用都会  ，迭代器必须保证修改次数统一

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

在迭代器的while循环中，要保证所有remove都是通过迭代器完成的迭代器模式下，似乎没有添加元素的操作，如果是添加，只能自己通过for + size动态修改来完成，当然删除也可以，但是不能forech,手动同步size修改。

 
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
		
		
## 二叉树

* 节点的高度，自底向上看

节点的高(heigh)是一片树叶的最长路径，树叶的高都是0，一棵树的高为它的根的高

* 节点的深度，自顶向下看

任意节点深度(depth)为从根到n的唯一的路径的长，根的深度为0，一棵树的深度等于它的最深的树叶的深度;该深度总是等于这棵树的高

* 先序遍历(preordertraversal)：想要列出目录中所有文件的名字

时间复杂度，O（n）


listAll(intdepth)
		printName();
		if（isDirectory))
		foreachfilecinthisdirectory
			c.listAll(depth+1);

*后续遍历：计算被该树所有文件占用的磁盘区块的总数。

		publicintsize(file){
			intsize=0
			if(fileisdir)
				for(itmeindir)
					size+=size(item)
			elsesize=file.size;	
			returnsize
		}
		
* 二叉查找树

左子树的数，全部小于父节点的值，右子树的值全部大于父节点的值，并且，子树也同样满足这样的要求

当选及树时，我们也不明显地画出nul1链，因为具有N个节点的每一棵二叉树都将需要N+1个null链，为什么？

**N个节点的树，全部链接需要2N个链接，但是N个节点都连起来了，需要N-1个链接，所以还有N+1个是NULL。**，N个点的二叉树，必有N+1个NULL


* 中序遍历

表达式树：根据树构建表达式容易，根据表达式能构建树吗？（二元操作符）

(a+(b*c))+(((d*e)+f)*g)


后续遍历的表达式树可以重新构建二叉树，仅仅是二叉，毕竟表达式必须二叉。


* 重构二叉树

必须要有个中序遍历才能构建，先序跟后续不能构建二叉树，因为先序后续加起来也无法区分单个孩子的情况，单个孩子，左右都不影响中序跟后续的输出。

###二叉查找树:目的是用来查找--核心思想递归o(logn)

删除操作比较麻烦：一般是用**右子树的最小的数据**(很容易找到)代替该节点，并递归地删除那个节点(现在它是空的)。因为右子树中的最小的节点不可能有左儿子，所以第二次remove要容易。

	insert(r,n)
		if(n.v==r.v)returnr;
		if(n.v<r.v)	r.left=insert(r.l,n)returnr;
		else		r.right=insert(r.l,n)returnr;

	remove(r,n)
		if(n.v>r.v)if(r.left==null)returnr;r.left=remove(r.left,n)returnr;
		if(n.v<r.v)if(r.right==null)returnr;r.right=remove(r.right,n)returnr;
		//相等
		if(r.left==null)returnr.right;
		if(r.right==null)returnr.left;
		//找右侧最小，删除掉
		p=findMin(r.right);	remove(r.right,p)	p.right=r.right;p.left=r.left;returnp;
		
		
	findMin(r)
		n=r;
	while(n.left!=null){
		n=n.left
	}	
	returnn
		
		
但是删除算法有助于使得左子树比右子树深度深，在删除操作中，我们可以通过随机选取右子树的最小元素或左子树的最大元素来代替被删除的元素以消除这种不平衡问题。


###平衡查找树：AVL(Adelson-velskii和Landis)树是带有平衡条件(balancecondition)的二叉查找树

一棵AVL树是其每个节点的左子树和右子树的高度最多差1的二叉查找树。一个AVL树的高度最多为1.410g(N+2)-1.328，但是实际上的高度只略大于1ogN		
AVL数的旋转：单旋转、双旋转，旋转主要记住一个东西：逆反，拨乱反正，哪里不正反哪里

*左左插入，右旋
*右右插入，左旋
*左右插入，先左旋，再右旋
*右左插入，先右旋，再左旋

**最后一步对于首个出现问题的点一定是反的，才能调整高度**，或者说，左右，右左需要先转换成左左，右右，转换后才可以。

*平衡二叉树插入：最多也只需要调整2次
*平衡二叉树的问题在于删除处理，删除节点可能失衡，导致需要从删除节点的父节点开始，不断回溯到根节点，如果平衡二叉树很高的话则需要判断多个节点。

平衡二叉树为了避免高度的重复计算，需要为每个Node添加高度，NULL的高度是-1 

		height(n)  
			
			return n==null ? -1 : n.height 

		insert(r,n)
		
			if(r==null)  n.height=0; return n;
			if(n<r.v)     r.left = insert(r.left,n) 
                  if(n>r.v) 	    r.right=  insert(r.right,n)
               	<!--直接调整，看看是不是需要-->
		      return balance(r ); //调整后，节点可能变
		
		调整最多两次
		
		 balance(r){
		 	ret 
		 	heightL =high(r.left)
		 	heightR =high(r.right)
		 	if( heightL-heightR ==0 || heightL-heightR==1 || heightL-heightR==-1 ) return r ;
		      if( heightL-heightR ==2)   
		      		if(high(r.left.left)>high(r.left.right))  
		      			 ret = singleRotateWithLeftChild( r ) 
		      		else 
		      			 ret = doubleRotateWithLeftChild( r ) 
		 	if( heightL-heightR ==-2)   
		 		 if(high(r.right.left)>high(r.right.right))  
		      			 ret = doubleRotateWithRightChild
		 		else
		 			  ret = singleRotateWithRightChild( r ) 
		 			 
	            ret.height = Math.max(height(ret.right) ,height(ret.left)) +1 
				
				return ret;
		 }
		
		 singleRotateWithLeftChild( r ) 
		 	
		 	p= r.left; 
		 	r.left = p.right;
		 	p.right = r;   r.height = Math.max(r.left ,r.right) +1 
		 	p.height = Math.max(r, p.left) +1 
		 	return p
		 
		高度获取:

		high(r){
			
			if(r==null)return0
			returnMath.max(high(r.left),high(r.right))+1
		}

二叉树的插入删除，注意返回值需要续接上parent。

		 remove(r,n)
		 
		 	if(n.v<r.v)  r.left = remove(r.left,n) ret =r;
		 	else if(n.v<r.v) r.right = remove(r.right,n)  ret =r;
		 	else if(n.left==null) return n.right
		 	else if(n.right ==null) return n.left;
		 	<!--完成值的替换，不用替换节点 也行-->
		 	<!--else { p = findMin(n.right)  p.left = n.left p.right=remove(n.right,p) ,ret = p}-->
		 	else { p = findMin(n.right)  ret = n n.v =p.v  ret.right =remove(n.right,p.v)}
		 	balance (ret )
			return ret;
			
* 满二叉树：节点占满 
* 完全二叉树：叶子结点同一层，靠左排列


### 分层遍历：例子：磁盘访问次数减小到 一个非常小的常数 

AVL树平均将使用大约25 次磁盘访问，需要的时间是4 秒。
我们想要把磁盘访问次数减小到 一个非常小的常数，比如了或4 ;而且我们愿意写 一个复杂 的 程 序 来 做 这 件 事 ，因 为 在 合 理 情 况 下机 器 指 令 基 本 上 是 不 占 时 间 的 。 由 于 典 型 的 A V L 树 接 近 到最优的高度，因此应该清楚的是，二叉查找树是不可行的。使用二叉查找树我们不能行进到 低于1og N。解法直觉上看是简单的:如果有更多的分支，那么就有更少的高度。这样，31 个节 点的理想二叉树(perfect binarytree)有5层，而31个节点的5叉树则只有3层，如图4-59所示。 一 棵 山 叉 查 找 树 ( M - a r r y s c a r c h t r e e ) 可 以 有 以 路 分 支 。 随 着 分 支 增 加 ， 树 的 深 度 在 减 少 。 一棵 完 全 二叉 树 ( c o m p l e t e b i n a r y t r e e )的 高 度 大 约 为 1 0 g 2 N ， 而 一棵 完 全 以 叉 树 ( c o m p l e t e M - a r y t r e e ) 的 高 度 大约 是 10 g u N 。


![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/33339004c5af4ca09baa262d7221fea9~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=1288&h=350&s=126657&e=png&b=fefefe)

阶 为 M 的 B 树 是 一棵 具 有 下 列 特 性 的 树

1. 数据项存储在树叶上。
2. 非叶节点存储直到1 - 1个关键宇以指示搜索的方向;关键宇i代表子树i+1中的最小 048 的关键宇。
3. 树的根或者是 一片树叶，或者其儿子数在2和似之间。
4. 除根外，所有非树叶节点的儿子数任「M/ 2 和从之问。
5. 所有的树叶都在相同的深度上并有「L/ 21和L之间个数据项，厶的确定稍后描述。


## Li st 容器即ArrayList 和LinkedtList 用于查找效率很低

Collections API提供了两个附加容器set 和Map


### 	collection 与map

Map本身不提供迭代器，set提供，map.entrySet()，就可以通过迭代器处理事情，

map.entrySet().iterator()

散列是一种用于以常数平均时间执行**插人、删除和 查找**的技术


>   HashMap： 链表数组

Hash冲突，如何查找，Key的hash值相同，还要对比Key本身是否相同。
	
	
散列函数：数组+index的方式 


* 分离链接法：解决冲突的第一种方法通常叫作分离链接法(separate chaining)  **SeparateChainingHashTable**

  其做法是将散列到同一个 值的所有元素保留到 一个表中。为执行 一次查找，我们使用散列函数来确定究竟遍历哪个链表。 然后我们再在被确定的链表中执行一次查找。为执行insert ，我们 检查相应的链表看看该元素是否已经处在适当的位置( 如果允许插入重复元，那么通常要留出 一个额外的域，这个域当出现匹配事件时增 s 1)。如果这个元素是个新元素，那么它将被插人到链表的前端，这不 6 仅因为方便，还因为常常发生这样的事实:新近插人的元素最有可能 。
不久又被访问	。就像 二叉查找树只对那些是compar abl e 的对象工作一样，本章中的散列表只对遵守确定 协议的那些对象工作。在Java中这样的对象必须提供适当eguals 方法和返回 一个int 型量 的hashcode 方法

* 分离链式散列表 数组+链表，链表数组。插入，先找到链表，然后插入该链表

		public void insert ( AnyType × )
			List<AnyType> whichList = theLists [ myhash ( x ) ]; 20
			i f （!whichList.contains × )  {
				whichList.add( x );
				i f ++currentSize > theLists. length )
				<!--再哈希-->
				rehash ( );

* 线性探测法
* 平方探测法
* 双散列
* 再 散 列

### HashMap 的原理

  HashMap允许空键空值么：允许，空key的hashcode看做0 只能一个key
 
    static final int hash(Object key) {
        int h;
        return key == null ? 0 : (h = key.hashCode()) ^ h >>> 16;
    }

####  影响HashMap性能的重要参数

初始容量：创建哈希表(数组)时桶的数量，默认为 16
负载因子：哈希表在其容量自动增加之前可以达到多满的一种尺度，默认为 0.75
	
      newCap = 16; //默认初始容量
        newThr = 12;//默认初始阈值  
        
 注意这个newThr是哈希表的扩容，不是链表的扩容，链表/红黑树，那只能叫转换，算不上扩容。链表插入红黑树转换
 
 ![](https://img-blog.csdnimg.cn/20200618150149962.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzM3MTQxNzcz,size_16,color_FFFFFF,t_70#pic_center)

获取

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9pbWdlZHUubGFnb3UuY29tLzE1ODAzMzItMjAxOTA4MTYyMDA1MzA1NTAtODg5MTcwODExLnBuZw?x-oss-process=image/format,png#pic_center)


####  HashMap 的底层数组长度为何总是2的n次方
 
 为何高效，碰撞少，计算快，位与操作替代取模操作。
 
* HashMap根据用户传入的初始化容量，利用无符号**右移和按位或运算等方式计算出第一个大于该数的2的幂**。使数据分布均匀，减少碰撞
* 当length为2的n次方时，**h&(length - 1) 就相当于对length取模**，而且在速度、效率上比直接取模要快得多。

 
这里我觉得可以用逆向思维来解释这个问题，我们计算桶的位置完全可以使用h % length，如果这个length是随便设定值的话当然也可以，但是如果你对它进行研究，设计一个合理的值得话，那么将对HashMap的性能发生翻天覆地的变化。

没错，JDK源码作者就发现了，那就是当length为2的N次方的时候，那么，为什么这么说呢？

第一：当length为2的N次方的时候，h & (length-1) = h % length 数组下标计算快，位运算比取模运算效率高。

为什么&效率更高呢？因为位运算直接对内存数据进行操作，不需要转成十进制，所以位运算要比取模运算的效率更高

第二：当length为2的N次方的时候，数据分布均匀，减少冲突


#### * 为什么0.75

		感觉是经验值，太大，后期冲突多，太小，浪费空间，效率低。

####  1.8中做了哪些优化优化？

* 		数组+链表改成了数组+链表或红黑树
* 		链表的插入方式从头插法改成了尾插法
* 		在插入时，1.7先判断是否需要扩容，再插入，1.8先进行插入，插入完成再判断是否需要扩容
* 		扩容的时候1.7需要对原数组中的元素进行重新hash定位在新数组的位置，1.8采用更简单的判断逻辑，位置不变或索引+旧容量大小；

扩容的时候，如何决定决定原链表中的低高归属，迁移，链表一定不会有变化，链表数量受限制，不是树

	                   Node<K, V> loHead = null;
	                        Node<K, V> loTail = null;
	                        Node<K, V> hiHead = null;
	                        Node<K, V> hiTail = null;
	
	                        Node next;
	                        do {
	                            next = e.next;
	                            <!--最高位，10000取与-->
	                            if ((e.hash & oldCap) == 0) {
	                                if (loTail == null) {
	                                    loHead = e;
	                                } else {
	                                    loTail.next = e;
	                                }
	
	                                loTail = e;
	                            } else {
	                                if (hiTail == null) {
	                                    hiHead = e;
	                                } else {
	                                    hiTail.next = e;
	                                }
	
	                                hiTail = e;
	                            }
	
	                            e = next;
	                        } while(next != null);
	
	                        if (loTail != null) {
	                            loTail.next = null;
	                            newTab[j] = loHead;
	                        }
	
	                        if (hiTail != null) {
	                            hiTail.next = null;
	                            newTab[j + oldCap] = hiHead;
	                        }
	                        
 红黑树的拆分
	                        
	final void split(HashMap<K, V> map, Node<K, V>[] tab, int index, int bit) {
	            TreeNode<K, V> loHead = null;
	            TreeNode<K, V> loTail = null;
	            TreeNode<K, V> hiHead = null;
	            TreeNode<K, V> hiTail = null;
	            int lc = 0;
	            int hc = 0;
		
		<!--现决定table index 高低划分-->
	            TreeNode next;
	            for(TreeNode<K, V> e = this; e != null; e = next) {
	                next = (TreeNode)e.next;
	                e.next = null;
	                if ((e.hash & bit) == 0) {
	                    if ((e.prev = loTail) == null) {
	                        loHead = e;
	                    } else {
	                        loTail.next = e;
	                    }
	
	                    loTail = e;
	                    ++lc;
	                } else {
	                    if ((e.prev = hiTail) == null) {
	                        hiHead = e;
	                    } else {
	                        hiTail.next = e;
	                    }
	
	                    hiTail = e;
	                    ++hc;
	                }
	            }
			<!--为低链表构建 非树 或者红黑树-->
	            if (loHead != null) {
	                if (lc <= 6) {
	                    tab[index] = loHead.untreeify(map);
	                } else {
	                    tab[index] = loHead;
	                    if (hiHead != null) {
	                        loHead.treeify(tab);
	                    }
	                }
	            }
		 <!--为高链表构建 非树 或者红黑树-->
	            if (hiHead != null) {
	                if (hc <= 6) {
	                    tab[index + bit] = hiHead.untreeify(map);
	                } else {
	                    tab[index + bit] = hiHead;
	                    if (loHead != null) {
	                        hiHead.treeify(tab);
	                    }
	                }
	            } }	
	            
split缩减到6 才转为链表，但是如果增长，要扩展到8才能转红黑树。到8 ，不是大于8

 
## 红黑树：平衡二叉查找树的变体 ：插入简单，删除复杂

红黑树是具有下列着色性质的二叉查找树:

1. 每 一个节点或者着成红色，或者着成黑色。
2. 根是黑色的。
3. 如果一个节点是红色的，那么它的子节点必须是黑色的。
4. 从一个节点到 一个nul 1 引用的每 一条路径必须包含相同数目的黑色节点。

从根到叶子的最长的可能路径不多于最短的可能路径的两倍长

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/1bb7f6cb83be4724a44ee1f084f0d701~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=1686&h=820&s=332897&e=png&b=ffffff)

### 红黑树如何保证自平衡：红黑树能自平衡，它靠的是什么？

参考文档： [https://www.cnblogs.com/chuonye/p/11236136.html](https://www.cnblogs.com/chuonye/p/11236136.html)

* 三种操作：左旋、右旋和变色。

* 插入节点红色：如果插入在黑色节点下，无需更改

插入的新节点通常都是红色节点，除了作为根节点。当插入的节点为红色的时候，大多数情况不违反红黑树的任何规则；而插入黑色节点，必然会导致一条路径上多了一个黑色节点，这是很难调整的；红色节点虽然可能导致红红相连的情况，但是这种情况可以通过颜色调换和旋转来调整；  **没有必要改变插入的红，插入必红，改变必定失衡，不能依靠这个来重新平衡**，只能改变父节点颜色。

*  插入节点往上回溯（R-R）的情景，爷爷节点黑色，一直找到**曾爷爷是黑色，或者找到爷爷是根节点【曾爷爷是NULL】**，这个时候就可以不改变高度，                                           

### 插入：递归（R-R调整） 叔叔变黑，爷爷变红，不断的爷爷变红，

 
插入的集中场景

* N 是根结点，即红黑树的第一个结点
* N 的父结点（P）为黑色
* P 是红色的（不是根结点），它的兄弟结点 U 也是红色的
* P 为红色，而 U 为黑色/或者null null也是黑色节点】

表现：根节点要先染红，再变黑才能增加高度 。 红-黑-高

* 	  根节点的另一侧孩子是红色，并且回溯倒root 也是红色的时候，才会增加黑色的路径长度，
*    黑色节点的叔叔、或者NULL节点的叔叔，才会旋转
*    旋转需要注意左旋，右旋，双旋转  **旋转就可以搞定的，基本就不需要递归了**
*    旋转搞定，就不需要递归，


![WechatIMG6.jpg](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b307bc9cd81547d0a8516de5c6a37f6d~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=1163&h=1653&s=107292&e=jpg&b=b4b7b1)

根节点要先染红，再变黑才能增加高度 ，必要条件之一是根节点的另一侧孩子必须是红色，其次一直回溯到顶。

### 删除  ：补齐所有的NIL节点都是黑色，就理解递归了，是一方的权值少了1，递归找齐，

参考文档 [红黑树删除节点——这一篇就够了](https://blog.csdn.net/qq_40843865/article/details/102498310)

真正删除的是右子树的最小值：也就是说，**它最多有一个叶子节点**，其次可以分两种情况：红色与黑色，、。

* 如果是root直接删除
* 如果是红色节点，那么直接删除，这个红色节点一定没有子节点
* 如果是黑色，有一个非空红色孩子，变色
*  如果黑色，没有孩子，兄弟是红色，直接旋转+变色
*  如果黑色，没有孩子，兄弟也是黑色，并且兄弟有红色孩子，旋转+变色可以搞定
*  如果黑色，没有孩子，兄弟也是黑色，并且兄弟没有红色孩子，兄弟变红，加上溯到爷爷，少了一个黑色，此时最复杂回溯，但是不一定需要到顶。

![](https://img-blog.csdnimg.cn/20191011193343281.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQwODQzODY1,size_16,color_FFFFFF,t_70)

上溯


调整红黑的理由是一边比另一边多，一定是另一边多，因为删除了黑色，那么一定少了



# 参考文档

[红黑树这个数据结构，让你又爱又恨？看了这篇，妥妥的征服它 ](https://www.cnblogs.com/chuonye/p/11236136.html)
[精  红黑树删除节点——这一篇就够了  ](https://blog.csdn.net/qq_40843865/article/details/102498310)