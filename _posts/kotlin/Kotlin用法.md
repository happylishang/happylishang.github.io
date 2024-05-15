### kotlin中的同步互斥

kotlin不提供wait、notify、notifyAll，但是可以通过RetreentLock来处理，

		class KotlinBlockQueue<T> constructor(private val capacity: Int) {
	
		    private val lock = ReentrantLock()
		    private val notEmpty = lock.newCondition()
		    private val notFull = lock.newCondition()
		    private val items = mutableListOf<T>()
		
		    fun put(item: T) {
		        lock.withLock {
		            while (items.size == capacity) {
		                notFull.await()
		            }
		            items.add(item)
		            notEmpty.signal()
		        }
		    }
		
		    fun take(): T {
		        lock.withLock {
		            while (items.isEmpty()) {
		                notEmpty.await()
		            }
		            val item = items.removeAt(0)
		            notFull.signal()
		            return item
		        }
		    }
		}

如此可以实现生产者跟消费者模式，ReentrantLock的Condition是个不错的选择。如果只是处理互斥，当然也可以使用 synchronized 关键字，不过kotlin自己也提供了Mutex对象，不过kotlin自身没有现成的概念，转而是协程，Mutex主要是用在协程，多协程同步.



## kotlin下的单利synchronized用法

* 双重锁校验：懒汉模式

	    companion object {
	        @Volatile
	        private var instance: KotlinMutex? = null
	
	        fun getInstance(): KotlinMutex {
	            return instance ?: synchronized(this) {
	                instance ?: KotlinMutex().also { instance = it }
	            }
	        }
	    }
    
* 饿汉模式

		object KotlinMutex



    