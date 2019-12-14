---
layout: post
title: "被后台杀死后，Android应用如何重新走闪屏逻辑"
category: Android

---


Android应用运行在后台的时候，经常被系统的LowMemoryKiller杀掉，当用户再次点击icon或者从最近的任务列表启动的时候，进程会被重建，并且恢复被杀之前的现场。什么意思呢？假如APP在被杀之前的Activity堆栈是这样的，A<B<C，C位于最上层

![后台杀死与恢复的堆栈.jpg](http://upload-images.jianshu.io/upload_images/1460468-8c243747e13282a9.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

APP被后台杀死后，APP端进程被销毁了，也就不存在什么Activity了，也就没有什么Activity堆栈，不过AMS的却是被保留了下来：

![后台杀死与恢复的堆栈-杀后.jpg](http://upload-images.jianshu.io/upload_images/1460468-8ea4dad7492920d4.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

当用户再次启动APP时候会怎么样呢？这个时候，首先看到其实C，而不是栈底部的A，也就是说往往被杀死后，恢复看到的第一个界面是用户最后见到的那个界面。

![后台杀死与恢复的堆栈-恢复.jpg](http://upload-images.jianshu.io/upload_images/1460468-4973b41696019836.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

而用户点击返回，看到的就是上一个界面B，其次是A

![后台杀死与恢复的堆栈-恢复b.jpg](http://upload-images.jianshu.io/upload_images/1460468-4d3d1f43cfa82d29.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

之所以这样是因为APP端Activity的创建其实都是由AMS管理的，AMS严格维护这APP端Activity对应的ActivityRecord栈，可以看做当前APP的场景，不过，APP端Activity的销毁同AMS端ActivityRecord的销毁并不一定是同步的，最明显的就是后台杀死这种场景。Android为了能够让用户无感知后台杀死，就做了这种恢复逻辑，不过，在开发中，这种逻辑带了的问题确实多种多样，甚至有些产品就不希望走恢复流程，本文就说说如何避免走恢复流程。结合常见的开发场景，这里分为两种，一种是针对推送唤起APP，一种是针对从最近任务列表唤起APP（或者icon）。



## 正常启动

从最近任务列表跟Laucher是有区别的，区别在与根Intent 会影响从最近的任务列表的表现


# 从最近的任务列表或者Lancher如何重新走闪屏

> 前提：对于后台杀死的状况，不考虑路由。

首先，APP端必须知道当前Activity的启动是不是在走恢复流程，Activity有一个onCreate方法，在ActivityThread新建Activity之后，会回调该函数，如果是从后台杀死恢复来的，回调onCreate的时候会传递一个非空的Bundle savedInstanceState给当前Activity，只要判断这个非空就能知道是否是恢复流程。

## 做法一 所有Activity一视同仁

	public abstract class BaseActivity<T extends BasePresenter>
	        extends AppCompatActivity
	        {
	    @Override
	    protected void onCreate(Bundle savedInstanceState) {
        if (savedInstanceState != null) {
            savedInstanceState.clear();
            <!--重新走闪屏逻辑-->
            SplashActivity.startClearLastTask(this);
            isValidActivity = false;
        }
        super.onCreate(savedInstanceState);
        
        }
	}

知道恢复流程之后，如何处理呢？其实很简单，直接吊起闪屏页就可以了，不过这里有一点要注意的是，在启动闪屏页面的时候，必须要设置其IntentFlag：Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_CLEAR_TASK，这样做的理由是为了清理之前的场景，不然之前的ActivityRecord栈仍然保留在ActivityManagerService中，返回上一页可能会返回到被杀死前的界面（除非有特殊需求，一般模拟全新冷启动）：

    public static void startClearLastTask(Activity restoreActivity) {
        Intent intent = new Intent(restoreActivity, SplashActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
        restoreActivity.startActivity(intent);
        restoreActivity.finish();
    }

设置Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK主要是为了获取一个干净的新环境。当然如果你不设置也是有不设置的做法的。比如单独将首页路由界面过滤出来，对于首页的不finish。但是对于首页打开的闪屏页就不应该再start MainActivity了，不然会接着调用onNewIntent，不是多余吗。对于MainActivity而言，被杀死的场景无需处理route逻辑。

	public class MainActivity extends BaseActivity {
		<!--为存活处理-->
	    @Override
	    protected void onCreate(Bundle savedInstanceState) {
	        super.onCreate(savedInstanceState);
	        if (savedInstanceState == null) {
	            route(getIntent());
	        }
	    }
		
		<!--存活处理-->    
	    @Override
	    public void onNewIntent(Intent intent) {
	        super.onNewIntent(intent);
	        route(getIntent());
	      }
      
 
 如果MainActivity是通过推送或者其他方式直接启动的，那么intent中必定设置了数据，这种情况就需要先打开闪屏页，然后由闪屏页Finish后，再跳转推送目标页面。

      
## 做法二 单独处理MainActivity（路由Activity）
	
 
	public class BaseActivity extends AppCompatActivity {
		    @Override
		    protected void onCreate(Bundle savedInstanceState) {
		
		        LogUtil.v("lishang", "onCreate --" + this);
		        // savedInstanceState 非空就说明被杀了
		        if (savedInstanceState != null) {
		            savedInstanceState.clear();
		            if (this instanceof MainPageActivity) {
		                SplashActivity.startFormRestoreMainActivity(this);
		            } else {
		                SplashActivity.startClearLastTask(this);
		            }
		            isValidActivity = false;
		        }
		        //  推送或者其他的界面借助MainPage路由跳板，需要额外处理
		        super.onCreate(savedInstanceState);
		       }
		       }

做的特殊处理就是对于后台杀死的时候是只有MainActivity存活的情况下，则允许MainActivity走恢复流程，只不过再onCreate中先走一遍闪屏，同时由于后台杀死不走路由，那么这个情况下我们是可以做到少启动一次MainActivity。

	    // 杀死走恢复流程时候，主动截断
	    public static void startFormRestoreMainActivity(Activity restoreActivity) {
	        Intent intent = new Intent(restoreActivity, SplashActivity.class);
	        <!--标识从其他页面启动的，这样就不需要主动startMainActivity-->
	        intent.putExtra("FromResoreMainPage",true);
	        restoreActivity.startActivity(intent);
	 	    }

这样也能达到预期效果，同时也不会创建两个MainActivity，如果从闪屏页跳转其他界面，则需要额外处理，不过要特别注意的是：MainActivity在恢复的时候会先走完onCreate跟onResume可能会降低闪屏页的可见速度
 

# 从推送或者DeepLink唤起被杀APP时，如何走闪屏逻辑

**首先思考个问题：推送如何路由？**，可以通过启动一个Service，然后在我们APP中优雅的处理路由跳转，这算是个不错的方案。因为存在冷启动任然需要启动首页场景，一般而言推送跟DeepLink会统一通过MainActivity路由。


> 发送一个可以启动Service的通知

    void notify() {
        final NotificationCompat.Builder builder = new NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID);
        builder.setContentIntent(PendingIntent.getService(this, (int) System.currentTimeMillis(),
                new Intent(this,  PushService.class),
                PendingIntent.FLAG_UPDATE_CURRENT))
                .setContentText("content")
                .setContentTitle("title")
                .setContentInfo("content")
                .setWhen(System.currentTimeMillis()) 
                .setShowWhen(true)
                .setSmallIcon(R.mipmap.ic_launcher)
                .setAutoCancel(true) 
                .setOngoing(false); 

        final NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(NOTIFICATION_CHANNEL_ID,
                    "Channel human readable title",
                    NotificationManager.IMPORTANCE_DEFAULT);
            if (nm != null) {
                nm.createNotificationChannel(channel);
            }
        }        
         nm.notify((int) System.currentTimeMillis(), builder.build());
      
    }

> 在PushService中处理路由


	public class PushService extends Service {
	    @Nullable
	    @Override
	    public IBinder onBind(Intent intent) {
	        return null;
	    }
	
	    @Override
	    public int onStartCommand(Intent intent, int flags, int startId) {
	
	        if(hasLiveActivity){
	        		Intent intent=getRouterActivityIntent();
	        		intent.addFlags(  Intent.FLAG_ACTIVITY_NEW_TASK);
	        	  startActivity(intent)
	        }else{
	        		Intent intent=getMainActivityIntent();
	        	     intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
	        	   startActivity(intent)
	        }

	        return START_STICKY  ;
	    }
	}

对于推送，其实不需要关心Activity恢复的问题，因为在Service里面直接启动的话，可以通过Intent.FLAG_ACTIVITY_CLEAR_TASK避免所有的恢复。对于路由的处理分两种：在MainActivity存活的情况下，可以直接跳转target页面；如果不存在则需要唤起MainActivity，并由其作为路由跳板，因为只有这么处理，关闭推送target界面后才能返回主界面。这里**只看MainActivity不存活的情况**：推送分三步：

1. 跳转MainActivity
2. startActivityForResult打开闪屏页
3. 跳转推送目标页面

  
如何判断呢，正产流程下，启动后，rootActivity就是MainActivity，其实只需要判断是否有Activity存活即可，也就是查查APP的topActivity是否为null，**注意不要去向AMS查询，而是在本地进程中查询**，因为AMS在后台杀敌的场景下是有堆栈保存的。可以通过反射查询ActivityThread的mActivities，也可以根据自己维护的Activity堆栈来判断，判断没有存活Activity的前提下，就跳转主页面去路由。 在MainActivity的路由中，需要准确区分是否是推送跳转进来的，如果不是推送跳转进来，就不需要什么特殊处理，如果是推送跳转进来一定会携带跳转scheme数据，可以根据是否携带数据做区分下，当然也有其他处理方式，如果是MainActivity不存活，路由启动的场景，则需要打开闪屏。看一下MainActivity的代码：

  
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
   
    }
    
    @Override
    protected void onNewIntent(Intent intent) {
        Uri uri= intent.getData();
        intent.setData(null);
        router(uri);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if(requestCode==JUMP_TO_TARGET && requestCode == RESULT_OK){
            router(getIntent().getData());
            <!--闪屏消失后，需要跳转target页面-->
            getIntent().setData(null);
        }
    }

    private void router(Uri uri) {
    
     Uri uri= getIntent().getData();
        <!--只有在intent被设置了跳转数据的时候才去跳转，一般是推送就来，如果冷启动，是没有数据的-->
        if(uri!=null){
            SplashActivity.startActivityForResult(this,JUMP_TO_TARGET)
        }
    }
    
通过上面两部分的处理，基本能够满足APP“死亡”的情况下，先跳转闪屏的需求。 
