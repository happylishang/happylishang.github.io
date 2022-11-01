## HasMap 

散列函数（英语：Hash function）又称散列算法、哈希函数，是一种从任何一种数据中创建小的数字“指纹”的方法，比较常见的有MDx系列(MD5等)、SHA-xxx系列(SHA-256等)，散列表是散列函数的一个主要应用，使用散列表能够快速的按照关键字查找数据记录，HasMap的实现便是散列表。

![](https://segmentfault.com/img/bV6P4Z?w=1636&h=742)

哈希表 + 链表

### 初始化与插入流程

* part1：特殊key值处理，key为null；key为null的存储位置，都统一放在下标为0的bucket，即：table[0]位置的链表
* part2：计算table中目标bucket的下标；
* part3：指定目标bucket，遍历Entry结点链表，若找到key相同的Entry结点，则做替换；
* part4：若未找到目标Entry结点，则新增一个Entry结点。

第一次插入才会分配并创建Table，比如第一次put操作发现还未初始化，则resize第一次扩容，其实也是初始化

    final V putVal(int hash, K key, V value, boolean onlyIfAbsent,
                   boolean evict) {
        Node<K,V>[] tab; Node<K,V> p; int n, i;
        if ((tab = table) == null || (n = tab.length) == 0)
            n = (tab = resize()).length;
            

第一次resize会创建初始的Hashtable，容量一般默认配置

        if (newThr == 0) {
            float ft = (float)newCap * loadFactor;
            newThr = (newCap < MAXIMUM_CAPACITY && ft < (float)MAXIMUM_CAPACITY ?
                      (int)ft : Integer.MAX_VALUE);
        }
        @SuppressWarnings({"rawtypes","unchecked"})
        Node<K,V>[] newTab = (Node<K,V>[])new Node[newCap];
        table = newTab;

有了初始Hashtable就可以计算插入k-v的bucket下标，使用key的hashCode作为算式的输入，得到了hash值，然后利用hash值与table容量做“与”运算

	static int indexFor(int h, int length) {
	    return h & (length-1);
	}
	
并在对应的位置插入Entry，同时便利查找ke，有则覆盖，无则添加。

###  扩容

扩容后大小是扩容前的2倍；数据搬迁，从旧table迁到扩容后的新table。 为避免碰撞过多，先决策是否需要对每个Entry链表结点重新hash，然后根据hash值计算得到bucket下标，然后使用头插法做结点迁移。

## LinkHashMap


有

## ConcurrentModificationException


## ConcurrentHashMap

