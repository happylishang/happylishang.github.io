Binder是Android跨进程通信的主要手段，一般而言，Client同步阻塞请求Service，直到Service提供完服务后才返回，不过，也有特殊的，比如请求用ONE_WAY方式，这种场景一般主要是用来通知，至于通知被谁消费，是否被消费压根不会关心。拿ContentService服务为例子，它是一个全局的通知中心，负责转发通知，而且，一般是群发，由于在转发的时候，ContentService被看做Client，如果这个时候采用普通的同步阻塞势必会造成通知的延时发送送，所以这里的Client采用了oneway，异步。

	interface IContentObserver
	{
	    /**
	     * This method is called when an update occurs to the cursor that is being
	     * observed. selfUpdate is true if the update was caused by a call to
	     * commit on the cursor that is being observed.
	     */
	    oneway void onChange(boolean selfUpdate, in Uri uri, int userId);
	}

不过这种机制可能也会影响Service的性能，比如**同一个线程中的Client**请求的服务是一个耗时操作的时候，通过oneway的方式发送请求的话，如果之前的请求还没被执行完，则Service不会启动新的线程去响应，该请求线程的所有操作都会被放到同一个Binder线程中依次执行，这样其实没有利用Binder机制的动态线程池，如果是多个线程中的Client并发请求，则还是会动态增加Binder线程的，大概这个是为了保证同一个线程中的Binder请求要依次执行吧，这种表现好像是反过来了，Client异步，而Service阻塞了。