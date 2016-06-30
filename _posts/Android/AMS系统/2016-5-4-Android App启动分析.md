---
layout: post
title: "Android App启动分析"
description: "Java"
category: android开发

---

#### Android进程类型

* init进程
* init.rc中的Service进程
* zygote进程
* SystemServer及其他系统服务
* Android Application进程

#### 创建ActivityRecord

#### 创建ServiceRecord


#### pause stop上一个Activiyt，并保存 onsaveinstancestate 也许是跟上一个进程的交互

#### 创建新进程启动


    public static final ProcessStartResult start(final String processClass,
                                  final String niceName,
                                  int uid, int gid, int[] gids,
                                  int debugFlags, int mountExternal,
                                  int targetSdkVersion,
                                  String seInfo,
                                  String[] zygoteArgs) {
        try {
            return startViaZygote(processClass, niceName, uid, gid, gids,
                    debugFlags, mountExternal, targetSdkVersion, seInfo, zygoteArgs);
        } catch (ZygoteStartFailedEx ex) {
            Log.e(LOG_TAG,
                    "Starting VM process through Zygote failed");
            throw new RuntimeException(
                    "Starting VM process through Zygote failed", ex);
        }
    }
    

Appplication新进程的启动是通过AMS向Zygote发送请求进行，发送参数列表，返回pid，
	
	/**
	     * Sends an argument list to the zygote process, which starts a new child
	     * and returns the child's pid. Please note: the present implementation
	     * replaces newlines in the argument list with spaces.
	     * @param args argument list
	     * @return An object that describes the result of the attempt to start the process.
	     * @throws ZygoteStartFailedEx if process start failed for any reason
	     */
	    private static ProcessStartResult zygoteSendArgsAndGetResult(ArrayList<String> args)
	            throws ZygoteStartFailedEx {
	        openZygoteSocketIfNeeded();
	
	        try {
	            /**
	             * See com.android.internal.os.ZygoteInit.readArgumentList()
	             * Presently the wire format to the zygote process is:
	             * a) a count of arguments (argc, in essence)
	             * b) a number of newline-separated argument strings equal to count
	             *
	             * After the zygote process reads these it will write the pid of
	             * the child or -1 on failure, followed by boolean to
	             * indicate whether a wrapper process was used.
	             */
	
	            sZygoteWriter.write(Integer.toString(args.size()));
	            sZygoteWriter.newLine();
	
	            int sz = args.size();
	            for (int i = 0; i < sz; i++) {
	                String arg = args.get(i);
	                if (arg.indexOf('\n') >= 0) {
	                    throw new ZygoteStartFailedEx(
	                            "embedded newlines not allowed");
	                }
	                sZygoteWriter.write(arg);
	                sZygoteWriter.newLine();
	            }
	
	            sZygoteWriter.flush();
	
	            // Should there be a timeout on this?
	            ProcessStartResult result = new ProcessStartResult();
	            result.pid = sZygoteInputStream.readInt();
	            if (result.pid < 0) {
	                throw new ZygoteStartFailedEx("fork() failed");
	            }
	            result.usingWrapper = sZygoteInputStream.readBoolean();
	            return result;
	        } catch (IOException ex) {
	            try {
	                if (sZygoteSocket != null) {
	                    sZygoteSocket.close();
	                }
	            } catch (IOException ex2) {
	                // we're going to fail anyway
	                Log.e(LOG_TAG,"I/O exception on routine close", ex2);
	            }
	
	            sZygoteSocket = null;
	
	            throw new ZygoteStartFailedEx(ex);
	        }
	    }
    
#### ActivityManagerService作用（管理四大组件）

ActivityManagerService（AMS）是Android中最核心的服务，主要负责系统中四大组件的启动、切换、调度及应用进程的管理和调度等工作，其职责与操作系统中的进程管理和调度模块相类似，因此它在Android中非常重要。ActivityManagerService这些Java类没有继承Service，为什么？，尼玛，系统都没起来，AMS本来就是管理这些的，都没起来，都没有Service的概念，还谈什么继承Service，Service组件是什么，为了什么，不过是为了Java更好的定义服务，但是这种服务不是唯一的实现方式。只要实现了服务+Binder通信，就已经满足了Android Server的要求。
#### ActivityManagerServices框架（实现与使用）

框架图

![](http://wiki.jikexueyuan.com/project/deep-android-v2/images/chapter6/image001.png)



#### ActivityManagerService启动

Android系统大部分的Server服务都是由SystemServer进行启动的。AMS由SystemServer的ServerThread线程创建，代码如下：

	[-->SystemServer.java::ServerThread的run函数]

	
	public class SystemServer    
	{    
	    ......    
	  
	    native public static void init1(String[] args);    
	  
	    ......    
	  
	    public static void main(String[] args) {    
	        ......    
	  
	        init1(args);    
	  
	        ......    
	    }   
	  
	    public static final void init2() {    
	        Slog.i(TAG, "Entered the Android system server!");    
	        Thread thr = new ServerThread();    
	        thr.setName("android.server.ServerThread");    
	        thr.start();    
	    }
	    
	    class ServerThread extends Thread {
 
		    @Override
		    public void run() {
		   
	 
		        try {

		            context = ActivityManagerService.main(factoryTest);
	 
		            pm = PackageManagerService.main(context,
		                    factoryTest != SystemServer.FACTORY_TEST_OFF);
		
		

对应关系

* ApplicationThreadProxy（BinderProxy） -- ApplicationThreadNative --ApplicationThread（Binder）
* ActivityManagerProxy--ActivityManagerNative--ActivityManagerService（Binder）
* ActivityRecord（    final IApplicationToken.Stub appToken; // window manager token WMS 交互）

根据是否进程被创建，有时候，就算有Activityrecord 但是没有创建进程

	    final boolean resumeTopActivityLocked(ActivityRecord prev, Bundle options) {
    
   
 aPp端区分
 
       final HashMap<IBinder, ActivityClientRecord> mActivities
            = new HashMap<IBinder, ActivityClientRecord>();
            
     mActivities.put(r.token, r);  
     
         public final Activity getActivity(IBinder token) {
        return mActivities.get(token).activity;
    }
    
    
    Activity 内部启动Activity 看看启动模式，如果是同一个Task，那么就设置为同一个
    
    
#### 参考文档

[Android应用程序启动过程源代码分析](http://blog.csdn.net/luoshengyang/article/details/6689748)

[Android Framework架构浅析之【近期任务】](http://blog.csdn.net/lnb333666/article/details/7869465)

[Android Low Memory Killer介绍](http://mysuperbaby.iteye.com/blog/1397863)

[Android开发之InstanceState详解]( http://www.cnblogs.com/hanyonglu/archive/2012/03/28/2420515.html )

[对Android近期任务列表（Recent Applications）的简单分析](http://www.cnblogs.com/coding-way/archive/2013/06/05/3118732.html)

[ Android——内存管理-lowmemorykiller 机制](http://blog.csdn.net/jscese/article/details/47317765)  