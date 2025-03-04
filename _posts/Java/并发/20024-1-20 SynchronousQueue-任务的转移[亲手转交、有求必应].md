SynchronousQueue: 生产出来的东西亲自交到消费者手里，否则就等，或者放弃，更像是一个等候站！ 线程陪着一起等待。
 
	public class SynchronousQueue<E> extends AbstractQueue<E> implements BlockingQueue<E>, Serializable {

SynchronousQueue继实现了BlockingQueue的几种方法：

add/offer/put都是往队列尾部添加元素 

* add：    不会阻塞，添加成功时返回true，不响应中断，当队列满时抛出IllegalStateException。
* offer：不会阻塞，添加成功时返回true，因队列已满导致添加失败时返回false，不响应中断。
* put：    会阻塞会响应中断, 等待队列空出来

take/poll取队列头部元素：

* take：会响应中断，会一直阻塞直到取得元素或当前线程中断。
* poll：会响应中断，会阻塞，阻塞时间参照方法里参数timeout.timeUnit，当阻塞时间到了还没取得元素会返回null
 
offer/poll配套，两者都可以选择等候时间，超时来了就返回，put/take则会一直等待，add的话会直接抛出异常，


### 简单的生产消费模型

SynchronousQueue可以看成是一个传球手，负责把生产者线程处理的数据直接传递给消费者线程。队列本身并不存储任何元素，非常适合传递性场景。

<!--公平用的是queue，非公平用的是stack，所以可能导致先进来的永远无法被消费-->

	public class SynchoronizedQueueTest {
	
	    static SynchronousQueue<String> queue = new SynchronousQueue<>();
	
	    public static void main(String[] args) {
	
	        new Thread(new Runnable() {
	            @Override
	            public void run() {
	                while (true) {
	                    System.out.println("生产 ");
	                    try {
	                        queue.put("  " + System.currentTimeMillis());
	                    } catch (InterruptedException e) {
	                        throw new RuntimeException(e);
	                    }
	                    try {
	                        Thread.sleep(1000);
	                    } catch (InterruptedException e) {
	                        throw new RuntimeException(e);
	                    }
	                }
	            }
	        }).start();
	
	        new Thread(new Runnable() {
	            @Override
	            public void run() {
	                while (true) {
	                    try {
	                        System.out.println("消费 " + queue.take());
	                    } catch (InterruptedException e) {
	                        throw new RuntimeException(e);
	                    }
	                }
	            }
	        }).start();
	    }
	}
	
	
	public SynchronousQueue(boolean fair) {
    transferer = fair ? new TransferQueue<E>() : new TransferStack<E>();
	}
	
	

* 对于生产者，如果阻塞的方式，则需要配对一个，唤起一个线程，如果非阻塞，那就是添加失败，总之生产的东西必须亲手交上去，要么死等，要么放弃。
* 对于消费者，吃一个唤醒一个，如果没有就等着生产者生产，并不是说不能多个，有多个生产了，等着消费，也可以有多个消费等生产，大概的意思就是不囤积。  

只有等待线程队列的概念，没有等待任务的概念，这个队列上挂的不是对象本身，而是线程的化身，用在线程池的时候

    public static ExecutorService newCachedThreadPool() {
        return new ThreadPoolExecutor(0, Integer.MAX_VALUE, 60L, TimeUnit.SECONDS, new SynchronousQueue());
        创建一个可缓存线程池，如果线程池长度超过处理需要，可灵活回收空闲线程，若无可回收，则新建线程。

    }
    

 如果有任务新来了，但是没消费者等，那就新建一个消费者，让他去消费，如果有消费者等着，直接给他。用的offer方法，失败了，就说明线程不够用，消费者都忙着呢。它的size永远返回0
  
    public int size() {
        return 0;
    }
  
  其实就是没有存储生产物的地方，只会任务转移，手把手转移，SynchronousQueue负责中转，任务的转移，线程池有没有任务在等，有就可以投喂成功，没有就加。
  
	      public boolean offer(E e, long timeout, TimeUnit unit) throws InterruptedException {
	        if (e == null) {
	            throw new NullPointerException();
	        } else if (this.transferer.transfer(e, true, unit.toNanos(timeout)) != null) {
	            return true;
	        } else if (!Thread.interrupted()) {
	            return false;
	        } else {
	            throw new InterruptedException();
	        }
	    }
	    
是否可以转交成功， ThreadPoolExecutor 的execute


	   public void execute(Runnable command) {
	        if (command == null) {
	            throw new NullPointerException();
	        } else {
	        <!--未达到核心数，直接创建-->
	            int c = this.ctl.get();
	            if (workerCountOf(c) < this.corePoolSize) {
	                if (this.addWorker(command, true)) {
	                    return;
	                }
	                c = this.ctl.get();
	            }
			<!--如果有线程等就给他 workQueue.offer，插入成功-->
	            if (isRunning(c) && this.workQueue.offer(command)) {
	                int recheck = this.ctl.get();
	                if (!isRunning(recheck) && this.remove(command)) {
	                    this.reject(command);
	                } else if (workerCountOf(recheck) == 0) {
	                    this.addWorker((Runnable)null, false);
	                }
	                	<!--如果队列插入失败 ，就增加一个线程-->
	            } else if (!this.addWorker(command, false)) {
	                this.reject(command);
	            }
	        }
	    }
对于SynchronousQueue，没有等待的消费者，	    this.workQueue.offer(command)就会失败，只会新建线程。[]()