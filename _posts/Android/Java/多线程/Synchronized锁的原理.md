

## synchronized锁的使用

对象锁

	  public synchronized  void method(){}
	  
	   public void method() {
        	synchronized (object) {}
        }

类锁

	  public static synchronized  void method(){}
	  public void method() {
        	synchronized (XX.class) {}
        }

synchronized内置锁是一种对象锁,作用粒度是对象，作用在普通方法上产生的变化

	  public  void test2(){
	  }
  
	public  void  test3(){
		synchronized(this){}
	}
	  public static synchronized void test(){
	  }
	  
	  public synchronized void test(){
	  }

反编译后得到的代码 javap -verbose

<!--普通方法-->
	  public void test2();
	    descriptor: ()V
	    flags: ACC_PUBLIC
	    Code:
	      stack=0, locals=1, args_size=1
	         0: return
	      LineNumberTable:
	        line 6: 0

对于加在代码快的synchronized

	<!--代码快-->
	  public void test3();
	    descriptor: ()V
	    flags: ACC_PUBLIC
	    Code:
	      stack=2, locals=3, args_size=1
	         0: aload_0
	         1: dup
	         2: astore_1
	         3: monitorenter
	         4: aload_1
	         5: monitorexit
	         6: goto          14
	         9: astore_2
	        10: aload_1
	        11: monitorexit
	        12: aload_2
	        13: athrow
	        14: return
 
多了一些代码，尤其是monitorenter，monitorexit比较显眼，而方法中是通过方法访问标识符实现的
	        
	  public static synchronized void tests();
	    descriptor: ()V
	    flags: ACC_PUBLIC, ACC_STATIC, ACC_SYNCHRONIZED
	    Code:
	      stack=0, locals=0, args_size=0
	         0: return
	      LineNumberTable:
	        line 3: 0
	        
	  public synchronized void test();
	    descriptor: ()V
	    flags: ACC_PUBLIC, ACC_SYNCHRONIZED
	    Code:
	      stack=0, locals=1, args_size=1
	         0: return
	      LineNumberTable:
	        line 4: 0

	        
可以看到跟普通方法的区别 在  flags: ACC_PUBLIC, ACC_SYNCHRONIZED，多了一个ACC_SYNCHRONIZED标志。方法级别的同步是隐式的，作为方法调用的一部分，当调用一个ACC_SYNCHRONIZED标志的方法，线程也需要先获得monitor锁，然后开始执行方法，方法执行之后再释放monitor锁。如果在方法执行过程中，发生了异常，那么在异常被抛到方法外之前，监视器锁会被自动释放，**同步方法和同步代码块都是通过monitor来实现的**，对象与monitor一对一，线程可以占有或者释放monitor。


### synchronized锁升级及各种状态

synchronized早期完全属于悲观锁，而且完全是重量级锁，一旦牵扯锁竞争，就必定走线程的睡眠与唤醒，这里势必会走内核态与用户态的状态切换，开销非常大，可能睡眠唤醒的代价比代码执行的代价还要高，后期的JDK版本对synchronized进行了优化，有了一个 无锁-->偏向锁-->轻量级锁-->重量级锁的升级过程，除了重量级锁，其他的都不牵扯线程的睡眠唤醒，甚至都可以看做是无锁状态，这里的实现跟对象的头有很大关系，示意图如下

![](https://static001.geekbang.org/infoq/a8/a843ce0e58eff7844aa6970abddf5927.png)

简单看下各个阶段的表现跟原理。

#### 无锁跟偏向锁

其实个人感觉无锁跟偏向锁基本算是一个意思，作用也基本类似，默认偏向锁的开关是开启的，一个对象被创建后，MarkWord字段应该是无锁状态还是偏向锁状态，跟其创建的时机有一些关系，虚拟机启动前几秒创建的都是non-biasable的，
	
* 1  虚拟机启动就创建对象的MarkWord值


		OFF  SZ   TYPE DESCRIPTION               VALUE
		  0   8        (object header: mark)     0x0000000000000001 (non-biasable; age: 0)
		  8   4        (object header: class)    0xf80001e5
		 12   4        (object alignment gap)   
	 
	 
* 2 虚拟机启动1s后创建对象的MarkWord值

		OFF  SZ   TYPE DESCRIPTION               VALUE
		  0   8        (object header: mark)     0x0000000000000001 (non-biasable; age: 0)
		  8   4        (object header: class)    0xf80001e5
		 12   4        (object alignment gap)    
 
	
* 3 虚拟机启动3s后创建对象的MarkWord值

		OFF  SZ   TYPE DESCRIPTION               VALUE
		  0   8        (object header: mark)     0x0000000000000005 (biasable; age: 0)
		  8   4        (object header: class)    0xf80001e5
		 12   4        (object alignment gap)    
		 
* 4 虚拟机启动4s后创建对象的MarkWord值

		OFF  SZ   TYPE DESCRIPTION               VALUE
		  0   8        (object header: mark)     0x0000000000000005 (biasable; age: 0)
		  8   4        (object header: class)    0xf80001e5
		 12   4        (object alignment gap)    		 

可以看到挺神奇的表现，**对象的初始状态的MarkWord跟虚拟机的存活时间有关系**，启动前几秒的MarkWord是non-biasable，后面的都是biasable，不过这个并没有太大的影响，无锁跟偏向锁的作用基本是一样，两个状态个人认为基本可以等同。不过，在用synchronized获取对象锁后，表现是略不一样，non-biasable对象的锁会升级为轻量级锁，而biasable的会成为偏向锁状态biased，biasable状态的MarkWord前面会填充线程ID，只有填充色上线程ID，无锁与偏向锁的区别才能体现，没有填充线程ID的biasable与non-biasable是没啥区别，其次**可偏向与偏向的差别是是否设置了线程ID**。


### 偏向锁与轻量级锁：假设默认从偏向锁开始 

偏向锁在运行过程中偏向某个线程，线程获得锁之后，要再次获得锁时，**无需做任何同步 [比如CAS自旋]，CAS开销很低**，就可以再次执行同步代码。这是因为，**偏向锁退出同步块时，其实是没有任何操作的**，偏向锁标记依旧存在，线程ID依旧是当前线程，这就规避了频繁CAS设置，CAS复原。具体加锁、释放可简化为 

> *  如果第一次使用锁，则通过CAS设置为当前线程ID，并从biasable转换为biased
> *  执行同步代码区，执行结束后，不释放偏向锁。
> *  再次获取锁的时候会判断是不是当前线程，或者是不是初始状态，如果不是，则说明存在其他线程竞争
> *  在安全点挂起偏向锁线程，释放偏向锁，并膨胀为轻量级锁
> *  被阻塞在安全点的线程继续往下通过CAS竞争轻量级锁
> *  成功则继续执行，失败升级为重量级锁

* 5  biasable对象被synchronized获取对象锁之后，对象的MarkWord值会升级为biased偏向锁
 
		OFF  SZ   TYPE DESCRIPTION               VALUE
		  0   8        (object header: mark)     0x00007fdf6910d005 (biased: 0x0000001ff7da4434; epoch: 0; age: 0)
		  8   4        (object header: class)    0xf80001e5
		 12   4        (object alignment gap)    

偏向锁也会用到Lock Record，可重入性就是利用这个实现的，同时Lock Record也被用在锁升级的过程中，每次进入同步块的时候都会在栈中找到第一个可用（即栈中最高的）的Lock Record，将其obj字段指向锁对象，在偏向锁的获取中，每次进入同步块的时候都会在栈中找到第一个可用（即栈中最高的）的Lock Record，将其obj字段指向锁对象。每次解锁的时候都会把最低的Lock Record移除掉，所以可以通过遍历线程栈中的Lock Record来判断是否还在同步块中。存在竞争的时候，会根据是否在同步代码块决定是否直接撤销偏向锁，如果是在同步代码去则直接升级成轻量级锁，并设置给运行的线程，否则，先恢复成无锁状态后，再膨胀成轻量级锁，之后唤起之前被挂起的偏向锁线程，同时其他线程通过CAS+自旋争取轻量级锁。

non-biasable对象被synchronized利用自旋+CAS的方式来抢锁获取对象锁，之后MarkWord值会直接成为tink lock，这个不在这里讨论，因为它类似biasable，只不过在**退出同步代码快的时候，它需需要通过CAS释放锁**。
 
		OFF  SZ   TYPE DESCRIPTION               VALUE
		  0   8        (object header: mark)     0x000000030abbc9e8 (thin lock: 0x000000030abbc9e8)
		  8   4        (object header: class)    0xf80001e5
		 12   4        (object alignment gap)   

**获取偏向锁的线程不会主动释放偏向锁，即使是退出同步代码区**，在遇到其他线程尝试竞争锁时，偏向锁线程才会释放锁。如果升级后的轻量级锁，仍然竞争失败，则直接升级为fat lock ，偏向锁感觉是一次性的，升级为轻量级之后，就打破原来的模型，不会再恢复了，偏向与轻量级最大的区别是是否主动释放锁。

### 轻量级锁与重量级锁

轻量级锁假定的模型是线程交替使用资源，**只要没有同时申请锁，就不会升级为重量级锁**，它利用自旋代替内核态与用户态的状态切换，降低开销，
	 
* 6  tink lock如果**竞争失败**，MarkWord值会升级为fat lock	 

		OFF  SZ   TYPE DESCRIPTION               VALUE
		  0   8        (object header: mark)     0x00007ff14c824e8a (fat lock: 0x00007ff14c824e8a)
		  8   4        (object header: class)    0xf80001e5
		 12   4        (object alignment gap)  		  


![](https://static001.geekbang.org/infoq/81/815c3eccac3dd374194832e3369de89d.png)

轻量级锁的申请过程是：如果是偏向锁状态，则撤销偏向锁升级，或者直接升级为轻量级锁，如上面所述，不过上述在安全节点不需要CAS。如果本身是无锁状态，则升级+获取一体，首先在当前线程栈帧中建立一个Lock Record，用于拷贝锁对象的 Mark Word ，拷贝成功后，利用CAS 尝试将对象的 Mark Word 更新为新的 Lock Record 的指针，并将 Lock Record里的 owner 指针指向对象的 Mark Word，如果成功了，则这个线程就拥有了该对象的锁，并且暂时处于轻量级锁定状态，如果失败，则说明多个线程竞争锁，轻量级锁就要膨胀为重量级锁，锁标志的状态值变为 10 ，Mark Word中存储的就是指向重量级锁的指针，后面等待锁的线程也要进入阻塞状态，轻重的最大区别是是否借助系统资源，并涉及内核态及用户态的切换。

> tips ：对象的Lock Record 指向哪个线程的栈帧，哪个线程就拥有该轻量级锁。
 
### 轻量级锁的可重入

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly91cGxvYWQtaW1hZ2VzLmppYW5zaHUuaW8vdXBsb2FkX2ltYWdlcy8yNDkyMDgxLWE5MzI2OTU2MmJiZDEyMDQ?x-oss-process=image/format,png)

每次获取轻量级锁，都会新建一个Lock Record，但是只有最开始的Lock Record填充了锁对象的加锁前的mark word，Displaced Mark word有值，之后可重入的分配一个Displaced Mark word为null，因为没必要浪费资源存储无用的东西，最后的哪个退出锁的Displaced Mark word才有用，利用Lock Record列表实现可重入，其实偏向锁也是这么做的，只是偏向锁的Lock Record没有Displaced Mark word。

## monitorenter指令解析

InterpreterRuntime:: monitorenter
	
	IRT_ENTRY_NO_ASYNC(void, InterpreterRuntime::monitorenter(JavaThread* thread, BasicObjectLock* elem))
	#ifdef ASSERT
	  thread->last_frame().interpreter_frame_verify_monitor(elem);
	#endif
	  if (PrintBiasedLockingStatistics) {
	    Atomic::inc(BiasedLocking::slow_path_entry_count_addr());
	  }
	  Handle h_obj(thread, elem->obj());
	  assert(Universe::heap()->is_in_reserved_or_null(h_obj()),
	         "must be NULL or an object"); 
	//如果使用偏向锁，就进入Fast_enter，避免不必要的锁膨胀，如果不是偏向锁，就进入slow_enter,也就是锁升级
	  if (UseBiasedLocking) {
	    // Retry fast entry if bias is revoked to avoid unnecessary inflation
	    ObjectSynchronizer::fast_enter(h_obj, elem->lock(), true, CHECK);
	  } else {
	    ObjectSynchronizer::slow_enter(h_obj, elem->lock(), CHECK);
	  }
	  assert(Universe::heap()->is_in_reserved_or_null(elem->obj()),
	         "must be NULL or an object");
	#ifdef ASSERT
	  thread->last_frame().interpreter_frame_verify_monitor(elem);
	#endif
	IRT_END
	
先判断是否使用偏向锁，如果用的话，先进入fast_enter偏向锁逻辑，利用CAS走无锁编程的逻辑，否则走slow_enter。
 
	void ObjectSynchronizer::fast_enter(Handle obj, BasicLock* lock, bool attempt_rebias, TRAPS) {
	 if (UseBiasedLocking) {
	     //如果使用偏向锁，那么尝试偏向
	    if (!SafepointSynchronize::is_at_safepoint()) {
	        //如果线程不在安全点，那么就尝试BiasedLocking::revoke_and_rebias实现撤销并且重偏向
	      BiasedLocking::Condition cond = BiasedLocking::revoke_and_rebias(obj, attempt_rebias, THREAD);
	        //偏向成功，直接返回
	      if (cond == BiasedLocking::BIAS_REVOKED_AND_REBIASED) {
	        return;
	      }
	    } else {
	      assert(!attempt_rebias, "can not rebias toward VM thread");
	        //进入这里说明线程在安全点并且撤销偏向
	      BiasedLocking::revoke_at_safepoint(obj);
	    }
	    assert(!obj->mark()->has_bias_pattern(), "biases should be revoked by now");
	 }
	//使用兜底方案，slow_enter
	 slow_enter (obj, lock, THREAD) ;
	}

slow_enter，调用cmpxchg进行自旋（cmpxchg），如果成功则返回，说明获得轻量级锁；如果不成功，就进入锁膨胀
	 
		 void ObjectSynchronizer::slow_enter(Handle obj, BasicLock* lock, TRAPS) {
	    //获取上锁对象头部标记信息
	  markOop mark = obj->mark();
	  assert(!mark->has_bias_pattern(), "should not see bias pattern here");
	    //如果对象处于无锁状态
	  if (mark->is_neutral()) {
	    //将对象头部保存在lock对象中
	    lock->set_displaced_header(mark);
	    //通过cmpxchg进入自旋替换对象头为lock对象地址，如果替换成功则直接返回，表明获得了轻量级锁，不然继续自旋
	    if (mark == (markOop) Atomic::cmpxchg_ptr(lock, obj()->mark_addr(), mark)) {
	      TEVENT (slow_enter: release stacklock) ;
	      return ;
	    }
	    // 否则判断当前对象是否上锁，并且当前线程是否是锁的占有者，如果是markword的指针指向栈帧中的LR，则重入
	  } else
	  if (mark->has_locker() && THREAD->is_lock_owned((address)mark->locker())) {
	    assert(lock != mark->locker(), "must not re-lock the same lock");
	    assert(lock != (BasicLock*)obj->mark(), "don't relock with same BasicLock");
	    lock->set_displaced_header(NULL);
	    return;
	  }
	​
	#if 0
	  // The following optimization isn't particularly useful.
	  if (mark->has_monitor() && mark->monitor()->is_entered(THREAD)) {
	    lock->set_displaced_header (NULL) ;
	    return ;
	  }
	#endif
	​
	  // 代码执行到这里，说明有多个线程竞争轻量级锁，轻量级锁通过`inflate`进行膨胀升级为重量级锁
	  lock->set_displaced_header(markOopDesc::unused_mark());
	  ObjectSynchronizer::inflate(THREAD, obj())->enter(THREAD);
	}
 这里轻量级锁是通过BasicLock对象来实现的，在线程JVM栈中产生一个LR（lock Record）的栈桢，然后他们两个CAS竞争锁，成功的，就会在Markword中记录一个指针（62位），这个指针指向竞争成功的线程的LR，另外一个线程CAS自旋继续竞争，等到前面线程用完了，才进入。这就是自旋锁的由来。

	ObjectSynchronizer::inflate(THREAD, obj())->enter(THREAD);则是进行锁膨胀，升级为重量级锁。主要分为两部，其中inflate用于获取监视器monitor，enter用于抢占锁
	
	ObjectMonitor * ATTR ObjectSynchronizer::inflate (Thread * Self, oop object) {
	  // Inflate mutates the heap ...
	  // Relaxing assertion for bug 6320749.
	  assert (Universe::verify_in_progress() ||
	          !SafepointSynchronize::is_at_safepoint(), "invariant") ;
	​
	  for (;;) { //通过无意义的循环实现自旋操作
	      const markOop mark = object->mark() ;
	      assert (!mark->has_bias_pattern(), "invariant") ;
	​
	      if (mark->has_monitor()) {//has_monitor是markOop.hpp中的方法，如果为true表示当前锁已经是重量级锁了
	          ObjectMonitor * inf = mark->monitor() ;//获得重量级锁的对象监视器直接返回
	          assert (inf->header()->is_neutral(), "invariant");
	          assert (inf->object() == object, "invariant") ;
	          assert (ObjectSynchronizer::verify_objmon_isinpool(inf), "monitor is invalid");
	          return inf ;
	      }
	​
	      if (mark == markOopDesc::INFLATING()) {//膨胀等待，表示存在线程正在膨胀，通过continue进行下一轮的膨胀
	         TEVENT (Inflate: spin while INFLATING) ;
	         ReadStableMark(object) ;
	         continue ;
	      }
	​
	      if (mark->has_locker()) {//表示当前锁为轻量级锁，以下是轻量级锁的膨胀逻辑
	          ObjectMonitor * m = omAlloc (Self) ;//获取一个可用的ObjectMonitor
	          // Optimistically prepare the objectmonitor - anticipate successful CAS
	          // We do this before the CAS in order to minimize the length of time
	          // in which INFLATING appears in the mark.
	          m->Recycle();
	          m->_Responsible  = NULL ;
	          m->OwnerIsThread = 0 ;
	          m->_recursions   = 0 ;
	          m->_SpinDuration = ObjectMonitor::Knob_SpinLimit ;   // Consider: maintain by type/class
	          /**将object->mark_addr()和mark比较，如果这两个值相等，则将object->mark_addr()
	          改成markOopDesc::INFLATING()，相等返回是mark，不相等返回的是object->mark_addr()**/
	                     markOop cmp = (markOop) Atomic::cmpxchg_ptr (markOopDesc::INFLATING(), object->mark_addr(), mark) ;
	          if (cmp != mark) {//CAS失败
	             omRelease (Self, m, true) ;//释放监视器
	             continue ;       // 重试
	          }
	​
	          markOop dmw = mark->displaced_mark_helper() ;
	          assert (dmw->is_neutral(), "invariant") ;
	​
	          //CAS成功以后，设置ObjectMonitor相关属性
	          m->set_header(dmw) ;
	​
	​
	          m->set_owner(mark->locker());
	          m->set_object(object);
	          // TODO-FIXME: assert BasicLock->dhw != 0.
	​
	​
	          guarantee (object->mark() == markOopDesc::INFLATING(), "invariant") ;
	          object->release_set_mark(markOopDesc::encode(m));
	​
	​
	          if (ObjectMonitor::_sync_Inflations != NULL) ObjectMonitor::_sync_Inflations->inc() ;
	          TEVENT(Inflate: overwrite stacklock) ;
	          if (TraceMonitorInflation) {
	            if (object->is_instance()) {
	              ResourceMark rm;
	              tty->print_cr("Inflating object " INTPTR_FORMAT " , mark " INTPTR_FORMAT " , type %s",
	                (void *) object, (intptr_t) object->mark(),
	                object->klass()->external_name());
	            }
	          }
	          return m ; //返回ObjectMonitor
	      }
	      //如果是无锁状态
	      assert (mark->is_neutral(), "invariant");
	      ObjectMonitor * m = omAlloc (Self) ; ////获取一个可用的ObjectMonitor
	      //设置ObjectMonitor相关属性
	      m->Recycle();
	      m->set_header(mark);
	      m->set_owner(NULL);
	      m->set_object(object);
	      m->OwnerIsThread = 1 ;
	      m->_recursions   = 0 ;
	      m->_Responsible  = NULL ;
	      m->_SpinDuration = ObjectMonitor::Knob_SpinLimit ;       // consider: keep metastats by type/class
	      /**将object->mark_addr()和mark比较，如果这两个值相等，则将object->mark_addr()
	          改成markOopDesc::encode(m)，相等返回是mark，不相等返回的是object->mark_addr()**/
	      if (Atomic::cmpxchg_ptr (markOopDesc::encode(m), object->mark_addr(), mark) != mark) {
	          //CAS失败，说明出现了锁竞争，则释放监视器重行竞争锁
	          m->set_object (NULL) ;
	          m->set_owner  (NULL) ;
	          m->OwnerIsThread = 0 ;
	          m->Recycle() ;
	          omRelease (Self, m, true) ;
	          m = NULL ;
	          continue ;
	          // interference - the markword changed - just retry.
	          // The state-transitions are one-way, so there's no chance of
	          // live-lock -- "Inflated" is an absorbing state.
	      }
	​
	      if (ObjectMonitor::_sync_Inflations != NULL) ObjectMonitor::_sync_Inflations->inc() ;
	      TEVENT(Inflate: overwrite neutral) ;
	      if (TraceMonitorInflation) {
	        if (object->is_instance()) {
	          ResourceMark rm;
	          tty->print_cr("Inflating object " INTPTR_FORMAT " , mark " INTPTR_FORMAT " , type %s",
	            (void *) object, (intptr_t) object->mark(),
	            object->klass()->external_name());
	        }
	      }
	      return m ; //返回ObjectMonitor对象
	  }
	}
 
可以都看到返回值是ObjectMonitor，


###  ObjectMonitor对象锁的原理

重量级MarkWord锁标识位为10，指针指向的是 monitor 对象的起始地址，monitor对象可以与对象一起创建销毁，或者当线程试图获取对象锁时自动生成，一旦某个monitor被某个线程持有后，monitor便处于锁定状态，实现以是ObjectMonitor。ObjectMonitor的实现可以简单看下：

	//结构体如下
	ObjectMonitor::ObjectMonitor() {  
	  _header       = NULL;  
	  _count       = 0;  
	  _waiters      = 0,  
	  _recursions   = 0;       //线程重入次数
	  _object       = NULL;  
	  _owner        = NULL;    //拥有该monitor的线程
	  _WaitSet      = NULL;    //等待线程组成的双向循环链表，_WaitSet是第一个节点
	  _WaitSetLock  = 0 ;  
	  _Responsible  = NULL ;  
	  _succ         = NULL ;  
	  _cxq          = NULL ;    //多线程竞争锁进入时的单向链表
	  FreeNext      = NULL ;  
	  _EntryList    = NULL ;    //_owner从该双向循环链表中唤醒线程结点，_EntryList是第一个节点
	  _SpinFreq     = 0 ;  
	  _SpinClock    = 0 ;  
	  OwnerIsThread = 0 ;  
	} 

* 	监控区（Entry Set）：  锁已被其他线程获取，等待获取锁的线程就进入Monitor对象的监控区
* 	待授权区（Wait Set）：获取到锁，但是调用了wait方法进入待授权区[必须等待Notify重新进去监控区]

在锁已经被其它线程拥有的时候，请求锁的线程回进入了对象锁的entry set区域，一旦锁被释放，entryset区域的线程都会抢占锁，只能有任意的一个Thread能取得该锁，其他线程重新等待锁释放。如果调用wait方法，则线程进入Wait Set，等待Notify/notifyAll，线程先转移到wait set，等到锁释放，再竞争，而其enter函数


	void ATTR ObjectMonitor::enter(TRAPS) {
	  Thread * const Self = THREAD ;
	  void * cur ;
	  //通过CAS操作尝试把monitor的_owner字段设置为当前线程
	  cur = Atomic::cmpxchg_ptr (Self, &_owner, NULL) ;
	  //获取锁失败
	  if (cur == NULL) {
	     assert (_recursions == 0   , "invariant") ;
	     assert (_owner      == Self, "invariant") ;
	     return ;
	  }
	//如果之前的_owner指向该THREAD，那么该线程是重入，_recursions++
	  if (cur == Self) {
	     _recursions ++ ;
	     return ;
	  }
	//如果当前线程是第一次进入该monitor，设置_recursions为1，_owner为当前线程
	  if (Self->is_lock_owned ((address)cur)) {
	  <!--这里其他线程进不来-->
	    assert (_recursions == 0, "internal state error");
	    _recursions = 1 ;   //_recursions标记为1
	    _owner = Self ;     //设置owner
	    OwnerIsThread = 1 ;
	    return ;
	  }
	  
	  <!--否则竞争失败挂起自己-->
	  ...
	    jt->java_suspend_self();


主要是通过**CAS判断当前线程的指针和监视器的_owner比较替换**，如果成功了直接返回，如果失败了就判断当前线程是不是占用了监视器，如果是，则是重入的，次数加1，再开始竞争，竞争的方式有自旋竞争（TrySpin）和等待竞争(EnterI)。



### 为什么wait必须在syncronized中调用


wait是调用的某个锁的wait函数，为了保证能够执行不混乱， 必须在syncronized调用


### 为什么wait会释放锁

	  //1.调用ObjectSynchronizer::wait方法
	void ObjectSynchronizer::wait(Handle obj, jlong millis, TRAPS) {
	  /*省略 */
	  //2.获得Object的monitor对象(即内置锁)
	  ObjectMonitor* monitor = ObjectSynchronizer::inflate(THREAD, obj());
	  DTRACE_MONITOR_WAIT_PROBE(monitor, obj(), THREAD, millis);
	  //3.调用monitor的wait方法
	  monitor->wait(millis, true, THREAD);
	  /*省略*/
	}
	  //4.在wait方法中调用addWaiter方法
	  inline void ObjectMonitor::AddWaiter(ObjectWaiter* node) {
	  /*省略*/
	  if (_WaitSet == NULL) {
	    //_WaitSet为null，就初始化_waitSet
	    _WaitSet = node;
	    node->_prev = node;
	    node->_next = node;
	  } else {
	    //否则就尾插
	    ObjectWaiter* head = _WaitSet ;
	    ObjectWaiter* tail = head->_prev;
	    assert(tail->_next == head, "invariant check");
	    tail->_next = node;
	    head->_prev = node;
	    node->_next = head;
	    node->_prev = tail;
	  }
	}
	  //5.然后在ObjectMonitor::exit释放锁，接着 thread_ParkEvent->park  也就是wait

总结：通过object获得内置锁(objectMonitor)，通过内置锁将Thread封装成OjectWaiter对象，然后addWaiter将它插入以_waitSet为首结点的等待线程链表中去，最后释放锁。

notify方法的底层实现
	
	  //1.调用ObjectSynchronizer::notify方法
	    void ObjectSynchronizer::notify(Handle obj, TRAPS) {
	    /*省略*/
	    //2.调用ObjectSynchronizer::inflate方法
	    ObjectSynchronizer::inflate(THREAD, obj())->notify(THREAD);
	}
	    //3.通过inflate方法得到ObjectMonitor对象
	    ObjectMonitor * ATTR ObjectSynchronizer::inflate (Thread * Self, oop object) {
	    /*省略*/
	     if (mark->has_monitor()) {
	          ObjectMonitor * inf = mark->monitor() ;
	          assert (inf->header()->is_neutral(), "invariant");
	          assert (inf->object() == object, "invariant") ;
	          assert (ObjectSynchronizer::verify_objmon_isinpool(inf), "monitor is inva;lid");
	          return inf 
	      }
	    /*省略*/ 
	      }
	    //4.调用ObjectMonitor的notify方法
	    void ObjectMonitor::notify(TRAPS) {
	    /*省略*/
	    //5.调用DequeueWaiter方法移出_waiterSet第一个结点
	    ObjectWaiter * iterator = DequeueWaiter() ;
	    //6.后面省略是将上面DequeueWaiter尾插入_EntrySet的操作
	    /**省略*/
	  }
总结：通过object获得内置锁(objectMonitor)，调用内置锁的notify方法，通过_waitset结点移出等待链表中的首结点，将它置于_EntrySet中去，等待获取锁。注意：notifyAll根据policy不同可能移入_EntryList或者_cxq队列中，此处不详谈。

 
###  Monitorexit 与锁的释放

可释放的锁都会锁撤到non-biasable状态。轻量级锁、重量级锁使用完毕之后，都会释放，并恢复到non-biasable的无锁状态，偏向锁无法恢复。

 
### monitor监视器

montor到底是什么呢？我们接下来剥开Synchronized的第三层，monitor是什么？ 它可以理解为一种同步工具，或者说是同步机制，它通常被描述成一个对象。操作系统的管程是概念原理，ObjectMonitor是它的原理实现。



Mark Word : 用于存储对象自身的运行时数据，它是实现轻量级锁和偏向锁的关键。



可重入锁：又名递归锁，是指在同一个线程在外层方法获取锁的时候，再进入该线程的内层方法会自动获取锁（前提锁对象得是同一个对象或者class），不会因为之前已经获取过还没释放而阻塞。

 
简单来说在JVM中monitorenter和monitorexit字节码依赖于底层的操作系统的Mutex Lock来实现的，但是由于使用Mutex Lock需要将当前线程挂起并从用户态切换到内核态来执行

优化，只需要依靠一条CAS原子指令就可以完成锁的获取及释放。当存在锁竞争的情况下，执行CAS指令失败的线程将调用操作系统互斥锁进入到阻塞状态，当锁被释放的时候被唤醒(具体处理步骤下面详细讨论)。
 

轻量级锁(Lightweight Locking)：这种锁实现的背后基于这样一种假设，即在真实的情况下我们程序中的大部分同步代码一般都处于无锁竞争状态(即单线程执行环境)，在无锁竞争的情况下完全可以避免调用操作系统层面的重量级互斥锁，取而代之的是在monitorenter和monitorexit中只需要依靠一条CAS原子指令就可以完成锁的获取及释放。当存在锁竞争的情况下，执行CAS指令失败的线程将调用操作系统互斥锁进入到阻塞状态，当锁被释放的时候被唤醒(具体处理步骤下面详细讨论)。 偏向锁(Biased Locking)：是为了在无锁竞争的情况下避免在锁获取过程中执行不必要的CAS原子指令，因为CAS原子指令虽然相对于重量级锁来说开销比较小但还是存在非常可观的本地延迟。 适应性自旋(Adaptive Spinning)：当线程在获取轻量级锁的过程中执行CAS操作失败时，在进入与monitor相关联的操作系统重量级锁(mutex semaphore)前会进入忙等待(Spinning)然后再次尝试，当尝试一定的次数后如果仍然没有成功则调用与该monitor关联的semaphore(即互斥锁)进入到阻塞状态。



​自旋锁早在JDK1.4 中就引入了，只是当时默认时关闭的。在JDK 1.6后默认为开启状态。自旋锁本质上与阻塞并不相同，先不考虑其对多处理器的要求，如果锁占用的时间非常的短，那么自旋锁的性能会非常的好，相反，其会带来更多的性能开销(因为在线程自旋时，始终会占用CPU的时间片，如果锁占用的时间太长，那么自旋的线程会白白消耗掉CPU资源)。因此自旋等待的时间必须要有一定的限度，如果自旋超过了限定的次数仍然没有成功获取到锁，就应该使用传统的方式去挂起线程了，在JDK定义中，自旋锁默认的自旋次数为10次，用户可以使用参数-XX:PreBlockSpin来更改。

## synchronized与Lock的区别

1）Lock是一个接口，而synchronized是Java中的关键字，synchronized是内置的语言实现；
2）当synchronized块结束时，会自动释放锁，lock一般需要在finally中自己释放。synchronized在发生异常时，会自动释放线程占有的锁，因此不会导致死锁现象发生；而Lock在发生异常时，如果没有主动通过unLock()去释放锁，则很可能造成死锁现象，因此使用Lock时需要在finally块中释放锁；
3）lock等待锁过程中可以用interrupt来终端等待，而synchronized只能等待锁的释放，不能响应中断。
4）lock可以通过trylock来知道有没有获取锁，而synchronized不能； 
5）当synchronized块执行时，只能使用非公平锁，无法实现公平锁，而lock可以通过new ReentrantLock(true)设置为公平锁，从而在某些场景下提高效率。
6）LLock可以提高多个线程进行读操作的效率。（可以通过readwritelock实现读写分离）
7）synchronized 锁类型可重入 不可中断 非公平 而 lock 是： 可重入 可判断 可公平（两者皆可） 
在性能上来说，如果竞争资源不激烈，两者的性能是差不多的，而当竞争资源非常激烈时（即有大量线程同时竞争），此时Lock的性能要远远优于synchronized。所以说，在具体使用时要根据适当情况选择。 

## CAS的ABA问题


## AtomicInteger中volatile value作用

### volatile 可以保证可见性

：启动两个线程，一个线程修改static 变量，另一个线程读取该变量，看看volatile变量的作用

	public class VolatileTest {
	    final static int COUNT = 5;
	    static int value = 0;
	    public static void main(String[] args) {
	        new Thread(() -> {
	            int tmp = value;
	            while (tmp < COUNT) {
	            <!--读取并使用-->
	                if (value != tmp) {
	                    tmp = value;
	                    System.out.print("\n R "+value + " "+tmp);
	                }
	            }
	        }, "R").start();
	        new Thread(() -> {
	            while (value < COUNT) {
	                value ++;
	                System.out.print("\n W "+value);
	                try {
	                    TimeUnit.SECONDS.sleep(1);
	                } catch (Exception e) {
	                }
	            }
	        }, "W").start();
	    }
	}
	
输出可能是如下情况：	

	 W 1
	 R 1 1
	 W 2
	 W 3
	 W 4
	 W 5
	 结束
	 
可以看到写线程已经将静态变量value更新成了5，但是R线程中看到的value依旧是1，所以R线程可能就在那个地方死循环了，为value加上volatile之后呢？

	    static volatile  int value = 0;

之后输出会变成理想情况：

	 W 1
	 R 1 1
	 W 2
	 R 2 2
	 W 3
	 R 3 3
	 W 4
	 R 4 4
	 W 5
	 R 5 5
	Process finished with exit code 0

所以volatile修饰的变量其可见性会很时时，一个线程修改后，另一个线程会再次用的时候会立即可见。

### 防止指令重排

public class NoVisibility {
    private static boolean ready = false;
    private static int number = 0;

    private static class ReaderThread extends Thread {
        @Override
        public void run() {
            while (!ready) {
                Thread.yield(); //交出CPU让其它线程工作
            }
            System.out.println(number);
        }
    }

    public static void main(String[] args) {
        new ReaderThread().start();
        number = 42;
        ready = true;
    }
}

在单一线程中，只要重排序不会影响到程序的执行结果，那么就不能保证其中的操作一定按照程序写定的顺序执行，即使重排序可能会对其它线程产生明显的影响。


### synchroniz关键字也能保证可见性

即当ThreadA释放锁M时，它所写过的变量（比如，x和y，存在它工作内存中的）都会同步到主存中，而当ThreadB在申请同一个锁M时，ThreadB的工作内存会被设置为无效，然后ThreadB会重新从主存中加载它要访问的变量到它的工作内存中（这时x=1，y=1，是ThreadA中修改过的最新的值）。通过这样的方式来实现ThreadA到ThreadB的线程间的通信。

 


## 无锁编程与乐观锁

CAS：Compare and Swap，比较再交换，从硬件上说，是一条指令，执行两个动作，为什么要这样做：**因为有时候需要这样来达到及时更新，从而用于无锁编程。**，所以CAS背后其实透露的是一种无锁算法。



CAS到底是什么？是一种操作还是一种思想，还是一个指令

CAS算法


，。

无锁编程，即不使用锁的情况下实现多线程之间的变量同步，也就是在没有线程被阻塞的情况下实现变量的同步，所以也叫非阻塞同步（Non-blocking Synchronization）。

### 自旋

compareAndSwapInt本身是原子操作，不阻塞，本身没有自旋属性，需要外部添加do while才能达到自旋的作用






AtomicBoolean这些类只是提供原子操作的类，本身不算锁，而且本身的初衷是无锁编程，不存在GET，



	// Unsafe.java
	public final int getAndAddInt(Object o, long offset, int delta) {
	   int v;
	   do {
	       v = getIntVolatile(o, offset);
	   } while (!compareAndSwapInt(o, offset, v, v + delta));
	   return v;
	}
	 
	private static final Unsafe unsafe = Unsafe.getUnsafe();
    private static final long valueOffset;

    static {
      try {
        valueOffset = unsafe.objectFieldOffset
            (AtomicReference.class.getDeclaredField("value"));
      } catch (Exception ex) { throw new Error(ex); }
    }

    private volatile V value;

    /**
     * Creates a new AtomicReference with the given initial value.
     *
     * @param initialValue the initial value
     */
    public AtomicReference(V initialValue) {
        value = initialValue;
    }

    /**
     * Creates a new AtomicReference with null initial value.
     */
    public AtomicReference() {
    }
    
     
getAndAddInt()循环获取给定对象o中的偏移量处的值v，然后判断内存值是否等于v。如果相等则将内存值设置为 v + delta，否则返回false，继续循环进行重试，直到设置成功才能退出循环，并且将旧值返回。 整个“比较+更新”操作封装在compareAndSwapInt()中，在JNI里是借助于一个CPU指令完成的，属于原子操作，可以保证多个线程都能够看到同一个变量的修改值。CPU操作成功会里用MESI内存一致模型，让其同步，

后续JDK通过CPU的cmpxchg指令，去比较寄存器中的 A 和 内存中的值 V。如果相等，就把要写入的新值 B 存入内存中。如果不相等，就将内存值 V 赋值给寄存器中的值 A。然后通过Java代码中的while循环再次调用cmpxchg指令进行重试，直到设置成功为止。

 
    System.out.print("普通原子类无法解决ABA问题： ");
            System.out.println(atomicReference.compareAndSet("A", "C") + "\t" + atomicReference.get());
            System.out.print("版本号的原子类解决ABA问题： ");
 
#  synchronized锁原理
 
 Java早期版本中，synchronized属于重量级锁，效率低下，因为监视器锁（monitor）是依赖于底层的操作系统的Mutex Lock来实现的，而操作系统实现线程之间的切换时需要从用户态转换到核心态，这个状态之间的转换需要相对比较长的时间，时间成本相对较高，这也是为什么早期的synchronized效率低的原因。庆幸的是在Java 6之后Java官方对从JVM层面对synchronized较大优化，所以现在的synchronized锁效率也优化得很不错了，Java 6之后，为了减少获得锁和释放锁所带来的性能消耗，引入了轻量级锁和偏向锁，
 
 
 https://blog.csdn.net/yinwenjie/article/details/84922958
 
 
###  自旋

如果对于那些需要同步的简单的代码块，获取锁挂起操作消耗的时间比用户代码执行的时间还要长，这种同步策略显然非常糟糕的


自旋锁（spinlock）：是指当一个线程在获取锁的时候，如果锁已经被其它线程获取，那么该线程将循环等待，然后不断的判断锁是否能够被成功获取，直到获取到锁才会退出循环，获取锁的线程一直处于活跃状态，但是并没有执行任何有效的任务，使用这种锁会造成busy-waiting


升级顺序为 自旋锁->偏向锁->轻量级锁->重量级锁。 

乐观锁

乐观锁是一种乐观思想，即认为读多写少，遇到并发写的可能性低，每次去拿数据的时候都认为别人不会修改，所以不会上锁，但是在更新的时候会判断一下在此期间别人有没有去更新这个数据，采取在写时先读出当前版本号，然后加锁操作（比较跟上一次的版本号，如果一样则更新），如果失败则要重复读-比较-写的操作。

java中的乐观锁基本都是通过CAS操作实现的，CAS是一种更新的原子操作，比较当前值跟传入值是否一样，一样则更新，否则失败。

悲观锁
悲观锁是就是悲观思想，即认为写多，遇到并发写的可能性高，每次去拿数据的时候都认为别人会修改，所以每次在读写数据的时候都会上锁，这样别人想读写这个数据就会block直到拿到锁。java中的悲观锁就是Synchronized,AQS框架下的锁则是先尝试cas乐观锁去获取锁，获取不到，才会转换为悲观锁，如RetreenLock。
 

每个对象都与一个monitor 相关联。当且仅当拥有所有者时（被拥有），monitor才会被锁定。执行到monitorenter指令的线程，会尝试去获得对应的monitor，如下：
每个对象维护着一个记录着被锁次数的计数器, 对象未被锁定时，该计数器为0。线程进入monitor（执行monitorenter指令）时，会把计数器设置为1.
当同一个线程再次获得该对象的锁的时候，计数器再次自增.
当其他线程想获得该monitor的时候，就会阻塞，直到计数器为0才能成功。

monitor的拥有者线程才能执行 monitorexit指令。
线程执行monitorexit指令，就会让monitor的计数器减一。如果计数器为0，表明该线程不再拥有monitor。其他线程就允许尝试去获得该monitor了。


这两条会让对象在执行，使其锁计数器加1或者减1。每一个对象在同一时间只与一个monitor(锁)相关联，而一个monitor在同一时间只能被一个线程获得，一个对象在尝试获得与这个对象相关联的Monitor锁的所有权的时候，monitorenter指令会发生如下3中情况之一： monitor计数器为0，意味着目前还没有被获得，那这个线程就会立刻获得然后把锁计数器+1，一旦+1，别的线程再想获取，就需要等待 如果这个monitor已经拿到了这个锁的所有权，又重入了这把锁，那锁计数器就会累加，变成2，并且随着重入的次数，会一直累加 这把锁已经被别的线程获取了，等待锁释放。
 
 
 
### 参考文档



https://www.cnblogs.com/sunddenly/articles/15106247.html

https://www.cnblogs.com/hongdada/p/14513036.html

[monitorenter源码](https://www.cnblogs.com/gmt-hao/p/14139341.html)
[从 Monitorenter 源码看 Synchronized 锁优化的过程](https://juejin.cn/post/7104638789456232478)