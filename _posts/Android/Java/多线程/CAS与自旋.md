CAS到底是什么？是一种操作还是一种思想，还是一个指令

CAS算法


CAS：Compare and Swap，即比较再交换，是一种有名的无锁算法。

无锁编程，即不使用锁的情况下实现多线程之间的变量同步，也就是在没有线程被阻塞的情况下实现变量的同步，所以也叫非阻塞同步（Non-blocking Synchronization）。





compareAndSwapInt本身是原子操作，不阻塞，本身没有自旋属性，需要外部添加do while才能达到自旋的作用