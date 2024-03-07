Thread

	public class Thread implements Runnable {
	
Thread实现了Runnable接口，本身是一个可运行的对象，在此基础上扩展了自己运行的能力


### Thread类中 interrupt() 、interrupted() 、isInterrupted() 

interrupt()函数的作用是设置一个中断标志位，并不是立刻结束中断，只有在线程受到阻塞时才抛出一个中断信号，以方便退出，具体来说就是：被设置的线程调用**Object.wait, Thread.join和Thread.sleep**三种方法之一时候，会抛出InterruptException，提早结束线程。

    public void interrupt() {
        <!--设置标志位，调用native方法-->
        this.interrupted = true;
        this.interrupt0();
    }

isInterrupted就是简单的判断是否被调用了interrupt，而interrupted() 是判断当前线程是否被设置了中断标志，

	 public static boolean interrupted() {
	        Thread t = currentThread();
	        boolean interrupted = t.interrupted;
	        if (interrupted) {
	            t.interrupted = false;
	            clearInterruptEvent();
	        }
	
	        return interrupted;
	    }
    
所以 interrupted() 被设计成了静态函数，只针对**interrupted()调用发生时处于的线程**，比如在main线程调用 interrupted()就是判断main线程是否被设置了中断标志，然后这个函数会将标志位清理掉。

>   interrupted()比较适合让线程自己处理是否结束，主要是判断是否有线程需要让自己暂停：注意他是个静态函数

	class Task implements Runnable{   
	    private double d = 0.0;   
	    public void run() {   
	      //检查程序是否被设置中断
	         while (!Thread.interrupted()) {   
	            System.out.println("I am running!");   
	            for (int i = 0; i < 900000; i++) {   
	                d = d + (Math.PI + Math.E) / d;   
	            }   
	        }   
	        System.out.println("ATask.run() interrupted!");   
	       // todo<!--后续需要做的事情，-->
	    }   
	}  

可以让一个任务中断下，后续如何处理可以自己控制，执行一次之后，已经清理掉了，后面的处理就比较灵活，我接受到了中断，我是继续执行，还是结束看条件。

###   join与yield()函数

A线程需要等待B线程执行完成之后再结束，就要用到 join() 方法了，比如A线程需要B、C的计算逻辑一起执行完，那就需要用到join。


* Main线程是个非守护线程，不能设置成守护线程，Main线程结束，其他线程一样可以正常运行。 
* 进程是资源分配的基本单位，线程是CPU调度的基本单位，对于CPU来说，其实并不存在java的主线程和子线程之分，都只是个普通的线程。进程的资源是线程共享的，只要进程还在，线程就可以正常执行，换句话说线程是强依赖于进程的。
* Main线程结束，其他线程也可以立刻结束，前提是：**仅当这些子线程都被设置为守护线程**。
	
	    public static void main(String[] args) throws InterruptedException {
	
	        Thread task=new Thread(() -> {
	            System.out.println("start child");
	            try {
	                Thread.sleep(2000);
	            } catch (InterruptedException e) {
	                throw new RuntimeException(e);
	            }
	            System.out.println("end child");
	        });
	        task.setDaemon(true); //主线程结束，可以立即结束
	        task.start();
	        task.join();            //主线程等子线程结束再继续执行
	        System.out.println("end main");
	    }

 yield() 方法只是提出申请释放CPU资源，至于能否成功释放由JVM决定， yield() 只是用来建议让当前线程暂停，放弃CPU使用权，加入到等候区，注意只会唤起同等或者更高优先级的线程，低优先级不受影响。
 
###  setPriority与setDaemon
    
*     public static int MIN_PRIORITY:它是线程的最大优先级，它的值是1。
*     public static int NORM_PRIORITY:这是线程的普通优先级，它的值是5。
*     public static int MAX_PRIORITY:它是线程的最小优先级，它的值是10。
 
线程的优先级一共十级：

    public final void setPriority(int newPriority) {
        this.checkAccess();
        if (newPriority <= 10 && newPriority >= 1) {
            ThreadGroup g;
            if ((g = this.getThreadGroup()) != null) {
                if (newPriority > g.getMaxPriority()) {
                    newPriority = g.getMaxPriority();
                }
               this.setPriority0(this.priority = newPriority);
            }
       } else {
            throw new IllegalArgumentException();
        }
    }

setDaemon理论上比setPriority设置的优先级都低，主要是守护，等所有的都没了，就退出虚拟机。
