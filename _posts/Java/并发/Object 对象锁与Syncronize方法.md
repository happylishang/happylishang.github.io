很多面试会问Thread.sleep跟wait有什么区别，个人感觉其实这个问题有点牵强，面试官想得到的答案往往是，关于锁的释放，但是sleep跟wait本身的作用就不同，这似乎没什么可比性，Thread.sleep的主要作用是挂起线程多长时间，到了特定的时间后，会被重新唤起，除此之外，不做任何多余的事情，所以Thread.sleep的使用不受什么限制，任何时候，任何地方都可以，但是Thread.sleep由于本身没有锁的的概念，所以没什么锁的释放之类的，所以如果线程拿着锁睡眠了，会有可能造成一段时间死锁。相比之下wait做的事情就多了，使用wait的前提是首先获取了某个对象的锁，wait的时候，会重新释放锁。

####  为什么Object的wait会释放锁并且必须与synchronized 一起用

如果从表现上来说，因为我要释放，所以我肯定要先获取啊，当然这个是由结果倒退现象，不是设计的本意，  调用必须在获取了该object锁的线程中，为了防止lost wake up，notify也必须在synchronized中， 一般是为了某种条件满足才唤醒，两者均需要在锁种执行，这样的话，能保证同步，如果notify先执行，那么条件已经满足，如果obj.wait先执行，说明条件还不满足，后面满足后，notify就可以唤醒，当然如果obj.wait没有notify，那么就会一直睡眠。

一般而言有个  while(!condition)  来判断，因为条件可能有多种多样，唤醒的也不一定是哪个，有可能多个线程的条件不同，那么就不能直接执行下去，不满足的时候，挂起，唤起就一定满足吗，不一定，所以还要循环判断：

	synchronized(lock)  
	{  
	    while(!condition)  
	    {  
	        lock.wait();  
	    }  
	    doSomething();  
	}  

唤醒后，现成会先重新获取锁，之后才执行，notify也放在锁的作用域是什么目的呢？ notify本身也是多步骤的，wait也是多步骤的，如果notify不加锁，可能会发生锁解锁一半的时候，线程就被唤醒了，notify本身肯定不会再释放锁了，而是将线程unpark，如果unpark的时候，没锁的概念，那么估计乱套，应该是为了保证互斥操作的完整性。
 
	synchronized(lock)  
	{  
	   condition=true
	   lock.notify();  
	} 
 
 至于怎么通知，应该是通过设置对象的标志位之类的来实现
 
### ReenTrantLock也可以wait /notify 看到这种理念

        public final void signal() {
            if (!isHeldExclusively())
                throw new IllegalMonitorStateException();
            Node first = firstWaiter;
            if (first != null)
                doSignal(first);
        }
      
  isHeldExclusively主要是判断线程是否排他似的 持有了锁
        
        protected final boolean isHeldExclusively() {
            // While we must in general read state before owner,
            // we don't need to do so to check if current thread is owner
            return getExclusiveOwnerThread() == Thread.currentThread();
        }
        
        