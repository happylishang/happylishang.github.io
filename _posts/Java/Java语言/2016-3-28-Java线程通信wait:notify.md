---
layout: post
title: Java线程通信
description: "Java"
category: Java


---


  
# wait()与notify()/notifyAll()

调用sleep()和yield()的时候对象的锁并没有被释放，而调用wait()将释放锁，这样另一个任务（线程）可以获得当前对象的锁，从而进入它的synchronized方法中。并可以通过notify()/notifyAll()，或者时间到期，从wait()中恢复执行。只能在同步控制方法或同步块中调用wait()、notify()和notifyAll()。如果在非同步的方法里调用这些方法，在运行时会抛出IllegalMonitorStateException异常。

#  对哪个对象wait，就必须在对哪个对象访问的时候，采用syncronize关键字,synchronizedsuit哪部分加锁

    @OnClick(R.id.first)
    void first() {
        if (count++ % 2 == 0) {
            synchronized (MainActivity.this) {
                MainActivity.this.notifyAll();
            }
        }
        Observable.create(new Observable.OnSubscribe<Object>() {
            @Override
            public void call(Subscriber<? super Object> subscriber) {
//                loadImg();
                try {

                    synchronized (MainActivity.this) {
                        MainActivity.this.wait();
                        LogUtils.v("hello");
                    }

                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }).subscribeOn(Schedulers.io()).subscribe();
    }
    
# notify和notifyAll区别

notify和notifyAll都是把某个对象上休息区内的线程唤醒,notify只能唤醒一个,但究竟是哪一个不能确定,而notifyAll则唤醒这个对象上的休息室中所有的线程.

# synchronized的含义其实是获取某个对象的锁 

为什么notify(), wait()等函数定义在Object中，而不是Thread中
Object中的wait(), notify()等函数，和synchronized一样，会对“对象的同步锁”进行操作。
wait()会使“当前线程”等待，因为线程进入等待状态，所以线程应该释放它锁持有的“同步锁”，否则其它线程获取不到该“同步锁”而无法运行！
OK，线程调用wait()之后，会释放它锁持有的“同步锁”；而且，根据前面的介绍，我们知道：等待线程可以被notify()或notifyAll()唤醒。现在，请思考一个问题：notify()是依据什么唤醒等待线程的？或者说，wait()等待线程和notify()之间是通过什么关联起来的？答案是：依据“对象的同步锁”。
负责唤醒等待线程的那个线程(我们称为“唤醒线程”)，它只有在获取“该对象的同步锁”(这里的同步锁必须和等待线程的同步锁是同一个)，并且调用notify()或notifyAll()方法之后，才能唤醒等待线程。虽然，等待线程被唤醒；但是，它不能立刻执行，因为唤醒线程还持有“该对象的同步锁”。必须等到唤醒线程释放了“对象的同步锁”之后，等待线程才能获取到“对象的同步锁”进而继续运行。
总之，notify(), wait()依赖于“同步锁”，而“同步锁”是对象锁持有，并且每个对象有且仅有一个！这就是为什么notify(), wait()等函数定义在Object类，而不是Thread类中的原因。