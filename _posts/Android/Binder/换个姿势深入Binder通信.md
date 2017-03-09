 



# Binder的执行线程与注册线程 

# Binder的查询

# Binder服务线程LOOP原理

# 唤起的时候，是唤起线程还是唤起进程队列

看看是不是返回，看看是不是有相互等待，因为Transaction栈是先处理上面的，如果存在相互等待，那么一定不会冲突

# 发送请求的的时候，阻塞在那个队列上，是自己线程的队列，还是进程，依据是什么


依据是，当前没有发送请求，并且是没有待处理任务，一般来说，这种情况是对Server端的，

猜测：比如多个请求，发送到进程队列上去了，这样多个线程能同时响应，也能处理快一些，

	// thread->transaction_stack  是否有请求压栈
	// 是否是当前线程没有需要处理的事情
	wait_for_proc_work = thread->transaction_stack == NULL &&
				list_empty(&thread->todo);
	if (thread->return_error != BR_OK && ptr < end) {
	 // $￥
	}
	// 表明线程即将进入等待状态。
	thread->looper |= BINDER_LOOPER_STATE_WAITING;
	// 就绪等待任务的空闲线程数加1。
	if (wait_for_proc_work)
		proc->ready_threads++;
	binder_unlock(__func__);
	trace_binder_wait_for_work(wait_for_proc_work,
				   !!thread->transaction_stack,
				   !list_empty(&thread->todo));
	if (wait_for_proc_work) {
		// 进程等待, 
		if (!(thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
					BINDER_LOOPER_STATE_ENTERED))) {
			binder_user_error("binder: %d:%d ERROR: Thread waiting "
				"for process work before calling BC_REGISTER_"
				"LOOPER or BC_ENTER_LOOPER (state %x)\n",
				proc->pid, thread->pid, thread->looper);
			wait_event_interruptible(binder_user_error_wait,
						 binder_stop_on_user_error < 2);
		}
		binder_set_nice(proc->default_priority);
		if (non_block) {
			if (!binder_has_proc_work(proc, thread))
				// 返回try again的提示。
				ret = -EAGAIN;
		} else
			// 当前task互斥等待在进程全局的等待队列中。 
			// 当前task互斥等待在进程全局的等待队列中。 
			// 多个线程互斥等待，防止重复处理请求
			 // 当前task互斥等待在进程全局的等待队列中。 
			// 多个线程互斥等待，防止重复处理请求
			// 何谓排他性的等待？有一些进程都在等待队列中，当唤醒的时候，
			// 内核是唤醒所有的进程。如果进程设置了排他性等待的标志，
			// 唤醒所有非排他性的进程和一个排他性进程。线程的排他性，其实都是线程，线程是内核调度的最小单位
			ret = wait_event_freezable_exclusive(proc->wait, binder_has_proc_work(proc, thread));
	} else {
		// 线程等待
		if (non_block) {
			if (!binder_has_thread_work(thread))
				ret = -EAGAIN;
		} else
		 /* 当前task等待在task自己的等待队列中(binder_thread.todo)，永远只有其自己。，只有自己*/
		 ret = wait_event_freezable(thread->wait, binder_has_thread_work(thread));
	}
	
**看看是不是有请求，或者有待处理的事情	再决定插入到哪里**


# Binder驱动中的四棵树

# 如何定向导弹

标记线程挂起，用哪个线程通信，那个才是Binder线程，并非所有的线程都是，只有open了驱动的进程，并且阻塞读取的线程，才算是Binder线程。

# APP层面有几个线程

至少Binder线程是Zygote分离时候就自带的

红黑树节点的产生过程

另一个要考虑的东西就是binder_proc里的那4棵树啦。前文在阐述binder_get_thread()时，已经看到过向threads树中添加节点的动作。那么其他3棵树的节点该如何添加呢？其实，秘密都在传输动作中。要知道，binder驱动在传输数据的时候，可不是仅仅简单地递送数据噢，它会分析被传输的数据，找出其中记录的binder对象，并生成相应的树节点。如果传输的是个binder实体对象，它不仅会在发起端对应的nodes树中添加一个binder_node节点，还会在目标端对应的refs_by_desc树、refs_by_node树中添加一个binder_ref节点，而且让binder_ref节点的node域指向binder_node节点

[](http://static.oschina.net/uploads/img/201308/15213415_Dm2n.png)

![](http://static.oschina.net/uploads/img/201308/15213415_Dm2n.png)

使用及添加时机不同，refs_by_desc主要是用在客户端使用的时候，refs_by_node主要是在getService添加的时候

基本上都会使用进程的队列，为什呢？，因为写数据之后，先返回一些响应，之后再用空的进行写，这个时候会阻塞在自己的进程队列中



	
	// 根据32位的uint32_t desc来查找
	
	static struct binder_ref *binder_get_ref(struct binder_proc *proc,
						 uint32_t desc)
	{
		struct rb_node *n = proc->refs_by_desc.rb_node;
		struct binder_ref *ref;
		while (n) {
			ref = rb_entry(n, struct binder_ref, rb_node_desc);
			if (desc < ref->desc)
				n = n->rb_left;
			else if (desc > ref->desc)
				n = n->rb_right;
			else
				return ref;
		}
		return NULL;
	}
	
	
	// 为何
	static struct binder_ref *binder_get_ref_for_node(struct binder_proc *proc,
							  struct binder_node *node)
	{
		struct rb_node *n;
		struct rb_node **p = &proc->refs_by_node.rb_node;
		struct rb_node *parent = NULL;
		struct binder_ref *ref, *new_ref;
		while (*p) {
			parent = *p;
			ref = rb_entry(parent, struct binder_ref, rb_node_node);
			if (node < ref->node)
				p = &(*p)->rb_left;
			else if (node > ref->node)
				p = &(*p)->rb_right;
			else
				return ref;
		}
	
		// binder_ref 可以在两棵树里面，但是，两棵树的查询方式不同，并且通过desc查询，不具备新建功能
	
		new_ref = kzalloc(sizeof(*ref), GFP_KERNEL);
		if (new_ref == NULL)
			return NULL;
		binder_stats_created(BINDER_STAT_REF);
		new_ref->debug_id = ++binder_last_id;
		new_ref->proc = proc;
		new_ref->node = node;
		rb_link_node(&new_ref->rb_node_node, parent, p);
		
		// 插入到proc->refs_by_node红黑树中去
	
		rb_insert_color(&new_ref->rb_node_node, &proc->refs_by_node);
		
		// 是不是ServiceManager的
		new_ref->desc = (node == binder_context_mgr_node) ? 0 : 1;
	
		// 分配Handle句柄，为了插入到refs_by_desc
		for (n = rb_first(&proc->refs_by_desc); n != NULL; n = rb_next(n)) {
			ref = rb_entry(n, struct binder_ref, rb_node_desc);
			if (ref->desc > new_ref->desc)
				break;
			new_ref->desc = ref->desc + 1;
		}
		// 插入到refs_by_desc红黑树中区
		p = &proc->refs_by_desc.rb_node;
		while (*p) {
			parent = *p;
			ref = rb_entry(parent, struct binder_ref, rb_node_desc);
			if (new_ref->desc < ref->desc)
				p = &(*p)->rb_left;
			else if (new_ref->desc > ref->desc)
				p = &(*p)->rb_right;
			else
				BUG();
		}
		rb_link_node(&new_ref->rb_node_desc, parent, p);
			// 插入到refs_by_desc红黑树中区
		rb_insert_color(&new_ref->rb_node_desc, &proc->refs_by_desc);
	
	
		if (node) {
			hlist_add_head(&new_ref->node_entry, &node->refs);
			binder_debug(BINDER_DEBUG_INTERNAL_REFS,
				     "binder: %d new ref %d desc %d for "
				     "node %d\n", proc->pid, new_ref->debug_id,
				     new_ref->desc, node->debug_id);
		} else {
			binder_debug(BINDER_DEBUG_INTERNAL_REFS,
				     "binder: %d new ref %d desc %d for "
				     "dead node\n", proc->pid, new_ref->debug_id,
				      new_ref->desc);
		}
		return new_ref;
	}



很多分析将Binder框架定义了四个角色：Server，Client，ServiceManager（以后简称SMgr）以及Binder驱动，其实这是容易将人引导到歧途，比如我们平时使用AIDL定义服务，并通信的时候，也许你觉得是注册到ServiceManager，其实不是，用户的Service是ActivityManagerService负责的。

其中Server，Client，SMgr运行于用户空间，驱动运行于内核空间。这四个角色的关系和互联网类似：Server是服务器，Client是客户终端，SMgr是域名服务器（DNS），驱动是路由器。


Bp flags = 0 单向

  BpBinder.h
      virtual status_t    transact(   uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);
                                    
                                    
     // TF_ONE_WAY == 1 
    // 并非单向，阻塞请求
    if ((flags & TF_ONE_WAY) == 0) {
        #if 0
        if (code == 4) { // relayout
            ALOGI(">>>>>> CALLING transaction 4");
        } else {
            ALOGI(">>>>>> CALLING transaction %d", code);
        }
        #endif
        if (reply) {
            err = waitForResponse(reply);
        } else {
            Parcel fakeReply;
            err = waitForResponse(&fakeReply);
        }
        #if 0
        if (code == 4) { // relayout
            ALOGI("<<<<<< RETURNING transaction 4");
        } else {
            ALOGI("<<<<<< RETURNING transaction %d", code);
        }
        #endif
        
        IF_LOG_TRANSACTIONS() {
            TextOutput::Bundle _b(alog);
            alog << "BR_REPLY thr " << (void*)pthread_self() << " / hand "
                << handle << ": ";
            if (reply) alog << indent << *reply << dedent << endl;
            else alog << "(none requested)" << endl;
        }
    } else {
        err = waitForResponse(NULL, NULL);
    }
    
    return err;
    
    
    
    
    
        // Is the read buffer empty?
    const bool needRead = mIn.dataPosition() >= mIn.dataSize();
    
    // We don't want to write anything if we are still reading
    // from data left in the input buffer and the caller
    // has requested to read the next data.

 

    // 如果正在读取数据，就不要再写请求，因为可能同时返回多个返回，一次性处理多个，先通知发完了，并且没有返回，

    const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;
    
    bwr.write_size = outAvail;
    bwr.write_buffer = (long unsigned int)mOut.data();

    // This is what we'll read.
    if (doReceive && needRead) {
        // 在这里把接受数据的size跟大小获取到，注意一次获取的大小，传递的大小，其实不用传递mIn，mOut，传递数据大小及地址就行
        bwr.read_size = mIn.dataCapacity();
        bwr.read_buffer = (long unsigned int)mIn.data();
    } else {
        bwr.read_size = 0;
        bwr.read_buffer = 0;
    }
    
    

(08) 在跳出while循环之后，会更新consumed的值。即，更新bwr.read_consumed的值。此时，由于写入了BR_NOOP和BR_TRANSACTION_COMPLETE两个指令，bwr.read_consumed=8。
    
说明： (01) 此时，因为在waitForResponse()中已经通过mIn.readInt32()读取了4个字节，因此mIn.dataPosition()=4，而mIn.dataSize()=8；因此，needRead=false。
(02) needRead=false，而doReceive=true；因此，outAvail=0。
最终，由于 bwr.write_size和bwr.read_size都为0，因此直接返回NO_ERROR。
再次回到waitForResponse()中，此时读出的cmd为BR_TRANSACTION_COMPLETE。此时，由于reply不为NULL，因此再次重新执行while循环，调用talkWithDriver()。
(01) 此时，已经读取了mIn中的全部数据，因此mIn.dataPosition()=8，而mIn.dataSize()=8；因此，needRead=true。
(02) outAvail=mOut.dataSize()，前面已经将mOut清空，因此outAvail=0。bwr初始化完毕之后，各个成员的值如下：


[参考文档](http://wangkuiwu.github.io/2014/09/05/BinderCommunication-AddService01/)
    
 
		
		
 
#  几个重要的结构体

binder_work等待处理的事件队列

 
binder_transaction    事件内容


为什么分开binder_transaction是复用的 binder_work是独立的，每个线程独有的这样处理比较好
    
    
#     binder主线程与其余Binder线程有什么不同

Binder系统中可分为3类binder线程：

Binder主线程：进程创建过程会调用startThreadPool()过程中再进入spawnPooledThread(true)，来创建Binder主线程。编号从1开始，也就是意味着binder主线程名为binder_1，并且主线程是不会退出的。
Binder普通线程：是由Binder Driver来根据是否有空闲的binder线程来决定是否创建binder线程，回调spawnPooledThread(false) ，isMain=false，该线程名格式为binder_x。
Binder其他线程：其他线程是指并没有调用spawnPooledThread方法，而是直接调用IPC.joinThreadPool()，将当前线程直接加入binder线程队列。例如： mediaserver和servicemanager的主线程都是binder线程，但system_server的主线程并非binder线程。


Binder的transaction有3种类型：

call: 发起进程的线程不一定是在Binder线程， 接收者只指向进程，并不确定会有哪个线程来处理，所以不指定线程；
reply: 发起者一定是binder线程，并且接收者线程便是上次call时的发起线程(该线程不一定是binder线程，可以是任意线程)。
async: 与call类型差不多，唯一不同的是async是oneway方式不需要回复，发起进程的线程不一定是在Binder线程， 接收者只指向进程，并不确定会有哪个线程来处理，所以不指定线程。


# Binder 线程的自动扩容 

	// Bn 会阻塞等待在这等待 Bp 的请求的到来
		static int binder_thread_read(struct binder_proc *proc,
		                  struct binder_thread *thread,
		                  void  __user *buffer, int size,
		                  signed long *consumed, int non_block)
		{
		    void __user *ptr = buffer + *consumed;
		    void __user *end = buffer + size;
		    int ret = 0;
		    int wait_for_proc_work;
		    if (*consumed == 0) {
		        if (put_user(BR_NOOP, (uint32_t __user *)ptr))
		            return -EFAULT;
		        ptr += sizeof(uint32_t);
		    }
		retry:
		    // transaction_stack == NULL 代表是第一次的 read（Bn 的阻塞read就是）
		    // Bn 的阻塞等待的 read todo list 也是空的
		    // 所以 Bn 的阻塞 read 这里的 wait_for_proc_work 是 true
		    wait_for_proc_work = thread->transaction_stack == NULL &&
		                list_empty(&thread->todo);
		    if (thread->return_error != BR_OK && ptr < end) {
		        if (thread->return_error2 != BR_OK) {
		            if (put_user(thread->return_error2, (uint32_t __user *)ptr))
		                return -EFAULT;
		            ptr += sizeof(uint32_t);
		            if (ptr == end)
		                goto done;
		            thread->return_error2 = BR_OK;
		        }
		        if (put_user(thread->return_error, (uint32_t __user *)ptr))
		            return -EFAULT;
		        ptr += sizeof(uint32_t);
		        thread->return_error = BR_OK;
		        goto done;
		    }
		    // 前面说了这个 looper 是当前线程的状态，
		    // 注意这里设置为 WAITING 了，表示正在等待
		    thread->looper |= BINDER_LOOPER_STATE_WAITING;
		    // Bn read 这里是 true，表示本进程空闲的进程数加1
		    if (wait_for_proc_work)
		        proc->ready_threads++;
		    mutex_unlock(&binder_lock);
		    if (wait_for_proc_work) {
		        // 这里检测 thread 是不是有下面这2个标志，这2个标志后面会说到。
		        // 还有注意前面设置那个 WAITTING 的是用 | 设置的，然后这里检测是用 &
		        // 然后看看这几个标志定义的值，会发现这里微妙的用法
		        if (!(thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
		                    BINDER_LOOPER_STATE_ENTERED))) {
		            binder_user_error("binder: %d:%d ERROR: Thread waiting "
		                "for process work before calling BC_REGISTER_"
		                "LOOPER or BC_ENTER_LOOPER (state %x)\n",
		                proc->pid, thread->pid, thread->looper);
		            wait_event_interruptible(binder_user_error_wait,
		                         binder_stop_on_user_error < 2);
		        }
		        binder_set_nice(proc->default_priority);
		        if (non_block) {
		            if (!binder_has_proc_work(proc, thread))
		                ret = -EAGAIN;
		        } else
		            // 这里就阻塞在这里，等 thread 的 todo list 不为空（Bp 请求）
		            ret = wait_event_interruptible_exclusive(proc->wait, binder_has_proc_work(proc, thread));
		    } else {
		        if (non_block) {
		            if (!binder_has_thread_work(thread))
		                ret = -EAGAIN;
		        } else
		            ret = wait_event_interruptible(thread->wait, binder_has_thread_work(thread));
		    }
		    mutex_lock(&binder_lock);
		    // 如果这个等待的线程被唤醒了（有 Bp 请求来了），
		    // 把这个进程空闲的线程数减1，
		    // 因为这个线程后面马上就要到用户空间去执行相关业务的函数了。
		    if (wait_for_proc_work)
		        proc->ready_threads--;
		    // 把线程的 WAITTING 标志去掉
		    thread->looper &= ~BINDER_LOOPER_STATE_WAITING;
		    // wait 出错的话，返回错误值
		    if (ret)
		        return ret;
		... ...
		done:
		    *consumed = ptr - buffer;
		    // 最后这里 requested_threads 表示发出请求要启动的线程数，
		    // ready_threads 表示空闲的线程数。
		    // 如果这2个加起来 == 0 就表示当前进程（服务进程）没有空闲的线程来处理请求，
		    // 并且还没请求去启动线程，所以需要启动一个新的线程来等待 Bp 的请求。
		    // requested_threads_started 表示本进程应请求启动的线程数，
		    // 这个不能超过 max_threads 设置的上限。
		    if (proc->requested_threads + proc->ready_threads == 0 &&
		        proc->requested_threads_started < proc->max_threads &&
		        (thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
		         BINDER_LOOPER_STATE_ENTERED)) /* the user-space code fails to */
		         /*spawn a new thread if we leave this out */) {
		        // 这里发 BR_SPAWN_LOOPER 到用户去创建新线程去了
		        // 然后把请求启动的线程数加1
		        proc->requested_threads++;
		        binder_debug(BINDER_DEBUG_THREADS,
		                 "binder: %d:%d BR_SPAWN_LOOPER\n",
		                 proc->pid, thread->pid);
		        if (put_user(BR_SPAWN_LOOPER, (uint32_t __user *)buffer))
		            return -EFAULT;
		    }
		    return 0;
		}

# 都在运行，唤醒空的怎么处理，累计处理，并且保持有等待的进程？？

其实链表的同步已经处理了


# 不同层次的对应关系以及流动方向

![](http://gityuan.com/images/binder/binder_start_service/binder_ipc_process.jpg)
![](http://gityuan.com/images/binder/binder_start_service/binder_transaction.jpg)
Binder客户端或者服务端向Binder Driver发送的命令都是以BC_开头,例如本文的BC_TRANSACTION和BC_REPLY, 所有Binder Driver向Binder客户端或者服务端发送的命令则都是以BR_开头, 例如本文中的BR_TRANSACTION和BR_REPLY.
只有当BC_TRANSACTION或者BC_REPLY时, 才调用binder_transaction()来处理事务. 并且都会回应调用者一个BINDER_WORK_TRANSACTION_COMPLETE事务, 经过binder_thread_read()会转变成BR_TRANSACTION_COMPLETE.
startService过程便是一个非oneway的过程, 那么oneway的通信过程如下所述.

## oneway

当收到BR_TRANSACTION_COMPLETE则程序返回,有人可能觉得好奇,为何oneway怎么还要等待回应消息? 我举个例子,你就明白了.

你(app进程)要给远方的家人(system_server进程)邮寄一封信(transaction), 你需要通过邮寄员(Binder Driver)来完成.整个过程如下:

你把信交给邮寄员(BC_TRANSACTION);
邮寄员收到信后, 填一张单子给你作为一份回执(BR_TRANSACTION_COMPLETE). 这样你才放心知道邮递员已确定接收信, 否则就这样走了,信到底有没有交到邮递员手里都不知道,这样的通信实在太让人不省心, 长时间收不到远方家人的回信, 无法得知是在路的中途信件丢失呢,还是压根就没有交到邮递员的手里. 所以说oneway也得知道信是投递状态是否成功.
邮递员利用交通工具(Binder Driver),将信交给了你的家人(BR_TRANSACTION);
当你收到回执(BR_TRANSACTION_COMPLETE)时心里也不期待家人回信, 那么这便是一次oneway的通信过程.

如果你希望家人回信, 那便是非oneway的过程,在上述步骤2后并不是直接返回,而是继续等待着收到家人的回信, 经历前3个步骤之后继续执行:

家人收到信后, 立马写了个回信交给邮递员BC_REPLY;
同样,邮递员要写一个回执(BR_TRANSACTION_COMPLETE)给你家人;
邮递员再次利用交通工具(Binder Driver), 将回信成功交到你的手上(BR_REPLY)
这便是一次完成的非oneway通信过程.

oneway与非oneway: 都是需要等待Binder Driver的回应消息BR_TRANSACTION_COMPLETE. 主要区别在于oneway的通信收到BR_TRANSACTION_COMPLETE则返回,而不会再等待BR_REPLY消息的到来. 另外，oneway的binder IPC则接收端无法获取对方的pid.





transact封装 业务层

	BinderDriverCommandProtocol {
	/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
	 BC_TRANSACTION = _IOW_BAD('c', 0, struct binder_transaction_data),
	 BC_REPLY = _IOW_BAD('c', 1, struct binder_transaction_data),
	 BC_ACQUIRE_RESULT = _IOW_BAD('c', 2, int),
	 BC_FREE_BUFFER = _IOW_BAD('c', 3, int),
	/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
	 BC_INCREFS = _IOW_BAD('c', 4, int),
	 BC_ACQUIRE = _IOW_BAD('c', 5, int),
	 BC_RELEASE = _IOW_BAD('c', 6, int),
	 BC_DECREFS = _IOW_BAD('c', 7, int),
	/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
	 BC_INCREFS_DONE = _IOW_BAD('c', 8, struct binder_ptr_cookie),
	 BC_ACQUIRE_DONE = _IOW_BAD('c', 9, struct binder_ptr_cookie),
	 BC_ATTEMPT_ACQUIRE = _IOW_BAD('c', 10, struct binder_pri_desc),
	 BC_REGISTER_LOOPER = _IO('c', 11),
	/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
	 BC_ENTER_LOOPER = _IO('c', 12),
	 BC_EXIT_LOOPER = _IO('c', 13),
	 BC_REQUEST_DEATH_NOTIFICATION = _IOW_BAD('c', 14, struct binder_ptr_cookie),
	 BC_CLEAR_DEATH_NOTIFICATION = _IOW_BAD('c', 15, struct binder_ptr_cookie),
	/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
	 BC_DEAD_BINDER_DONE = _IOW_BAD('c', 16, void *),
	};

	enum BinderDriverReturnProtocol {
	/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
	 BR_ERROR = _IOR_BAD('r', 0, int),
	 BR_OK = _IO('r', 1),
	 BR_TRANSACTION = _IOR_BAD('r', 2, struct binder_transaction_data),
	 BR_REPLY = _IOR_BAD('r', 3, struct binder_transaction_data),
	/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
	 BR_ACQUIRE_RESULT = _IOR_BAD('r', 4, int),
	 BR_DEAD_REPLY = _IO('r', 5),
	 BR_TRANSACTION_COMPLETE = _IO('r', 6),
	 BR_INCREFS = _IOR_BAD('r', 7, struct binder_ptr_cookie),
	/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
	 BR_ACQUIRE = _IOR_BAD('r', 8, struct binder_ptr_cookie),
	 BR_RELEASE = _IOR_BAD('r', 9, struct binder_ptr_cookie),
	 BR_DECREFS = _IOR_BAD('r', 10, struct binder_ptr_cookie),
	 BR_ATTEMPT_ACQUIRE = _IOR_BAD('r', 11, struct binder_pri_ptr_cookie),
	/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
	 BR_NOOP = _IO('r', 12),
	 BR_SPAWN_LOOPER = _IO('r', 13),
	 <!--没用过-->
	 BR_FINISHED = _IO('r', 14),
	 BR_DEAD_BINDER = _IOR_BAD('r', 15, void *),
	/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
	 BR_CLEAR_DEATH_NOTIFICATION_DONE = _IOR_BAD('r', 16, void *),
	 BR_FAILED_REPLY = _IO('r', 17),
	};


talkWithDirver封装

		#define BINDER_WRITE_READ   		_IOWR('b', 1, struct binder_write_read)
		#define	BINDER_SET_IDLE_TIMEOUT		_IOW('b', 3, int64_t)
		#define	BINDER_SET_MAX_THREADS		_IOW('b', 5, size_t)
		#define	BINDER_SET_IDLE_PRIORITY	_IOW('b', 6, int)
		#define	BINDER_SET_CONTEXT_MGR		_IOW('b', 7, int)
		#define	BINDER_THREAD_EXIT		_IOW('b', 8, int)
		#define BINDER_VERSION			_IOWR('b', 9, struct binder_version)

仅仅内核可见的业务层binder_work，封装事务队列，从一个进程转移到另一个进程

	struct binder_work {
		struct list_head entry;
		enum {
			BINDER_WORK_TRANSACTION = 1,
			BINDER_WORK_TRANSACTION_COMPLETE,
			BINDER_WORK_NODE,
			BINDER_WORK_DEAD_BINDER,
			BINDER_WORK_DEAD_BINDER_AND_CLEAR,
			BINDER_WORK_CLEAR_DEATH_NOTIFICATION,
		} type;
	};

[参考文档 http://gityuan.com/2016/09/04/binder-start-service/](http://gityuan.com/2016/09/04/binder-start-service/)

# 并非所有的进程 都能add_Service

 判断进程的uid是否有资格注册名称为name的服务
 
	int svc_can_register(unsigned uid, uint16_t *name)
	{
	    unsigned n;
	    
	    // 谁有权限add_service 0进程，或者 AID_SYSTEM进程
	    if ((uid == 0) || (uid == AID_SYSTEM))
	        return 1;
	
	    for (n = 0; n < sizeof(allowed) / sizeof(allowed[0]); n++)
	        if ((uid == allowed[n].uid) && str16eq(name, allowed[n].name))
	            return 1;
	
	    return 0;
	}
	

*  判断uid是否有资格注册名称为name的服务
*  如果用户是root用户或system用户，不用判断直接可以注册
*  所以，如果Server进程权限不够root和system，那么请记住要在allowed中添加相应的项。
	
	
		static struct {
		    unsigned uid;
		    const char *name;
		} allowed[] = {
		    { AID_MEDIA, "media.audio_flinger" },
		    { AID_MEDIA, "media.log" },
		    { AID_MEDIA, "media.player" },
		    { AID_MEDIA, "media.camera" },
		    { AID_MEDIA, "media.audio_policy" },
		    { AID_DRM,   "drm.drmManager" },
		    { AID_NFC,   "nfc" },
		    { AID_BLUETOOTH, "bluetooth" },
		    { AID_RADIO, "radio.phone" },
		    { AID_RADIO, "radio.sms" },
		    { AID_RADIO, "radio.phonesubinfo" },
		    { AID_RADIO, "radio.simphonebook" },
		/* TODO: remove after phone services are updated: */
		    { AID_RADIO, "phone" },
		    { AID_RADIO, "sip" },
		    { AID_RADIO, "isms" },
		    { AID_RADIO, "iphonesubinfo" },
		    { AID_RADIO, "simphonebook" },
		    { AID_MEDIA, "common_time.clock" },
		    { AID_MEDIA, "common_time.config" },
		    { AID_KEYSTORE, "android.security.keystore" },
		};

# 6.4 小规律

BC_TRANSACTION + BC_REPLY = BR_TRANSACTION_COMPLETE + BR_DEAD_REPLY + BR_FAILED_REPLY
Binder线程只有当本线程的thread->todo队列为空，并且thread->transaction_stack也为空，才会去处理当前进程的事务， 否则会继续处理或等待当前线程的todo队列事务。换句话说，就是只有当前线程的事务;

binder_thread_write: 添加成员到todo队列;
binder_thread_read: 消耗todo队列;

对于处于空闲可用的,或者Ready的binder线程是指停在binder_thread_read()的wait_event地方的Binder线程;
每一次BR_TRANSACTION或者BR_REPLY结束之后都会调用freeBuffer.


#  linux中的用户（UID）、组（GID）、进程（PID)

UserID 不同于UID，uid在安装以及系统启动的时候，就已经确定了

在 Linux 中，一个用户 UID 标示一个给定用户。Linux系统中的用户(UID)分为3类，即普通用户、根用户、系统用户。

      普通用户是指所有使用Linux系统的真实用户，这类用户可以使用用户名及密码登录系统。Linux有着极为详细的权限设置，所以一般来说普通用户只能在其家目录、系统临时目录或其他经过授权的目录中操作，以及操作属于该用户的文件。通常普通用户的UID大于500，因为在添加普通用户时，系统默认用户ID从500开始编号。
      根用户也就是root用户，它的ID是0，也被称为超级用户，root账户拥有对系统的完全控制权：可以修改、删除任何文件，运行任何命令。所以root用户也是系统里面最具危险性的用户，root用户甚至可以在系统正常运行时删除所有文件系统，造成无法挽回的灾难。所以一般情况下，使用root用户登录系统时需要十分小心。
      系统用户是指系统运行时必须有的用户，但并不是指真实的使用者。比如在RedHat或CentOS下运行网站服务时，需要使用系统用户apache来运行httpd进程，而运行MySQL数据库服务时，需要使用系统用户mysql来运行mysqld进程。在RedHat或CentOS下，系统用户的ID范围是1~499。下面给出的示例显示的是目前系统运行的进程，第一列是运行该进程的用户。

       组(GID)又是什么呢？事实上，在Linux下每个用户都至少属于一个组。举个例子：每个学生在学校使用学号来作为标识，而每个学生又都属于某一个班级，这里的学号就相当于UID，而班级就相当于GID。当然了，每个学生可能还会同时参加一些兴趣班，而每个兴趣班也是不同的组。也就是说，每个学生至少属于一个组，也可以同时属于多个组。在Linux下也是一样的道理
       
#  参考文档

[Android Binder 分析——通信模型](http://light3moon.com/2015/01/28/Android%20Binder%20%E5%88%86%E6%9E%90%E2%80%94%E2%80%94%E9%80%9A%E4%BF%A1%E6%A8%A1%E5%9E%8B/)            
[理解 Binder 线程池的管理](https://gold.xitu.io/entry/58197bd9128fe10055a4a51e)       
[Android Binder 分析——多线程支持](http://light3moon.com/2015/01/28/Android%20Binder%20%E5%88%86%E6%9E%90%E2%80%94%E2%80%94%E5%A4%9A%E7%BA%BF%E7%A8%8B%E6%94%AF%E6%8C%81/)      
[彻底理解Android Binder通信架构](http://gityuan.com/2016/09/04/binder-start-service/)         
[Android Binder机制の设计与实现4（Binder 协议）](http://blog.csdn.net/xujianqun/article/details/6677862)              
[Android四大组件与进程启动的关系](http://gityuan.com/2016/10/09/app-process-create-2/)          
[binder驱动-------之内存映射篇](http://blog.csdn.net/xiaojsj111/article/details/31422175)           