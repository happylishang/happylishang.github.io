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


# 从最近的任务列表唤起，不走恢复流程

首先，APP端必须知道当前Activity的启动是不是在走恢复流程，Activity有一个onCreate方法，在ActivityThread新建Activity之后，会回调该函数，如果是从后台杀死恢复来的，回调onCreate的时候会传递一个非空的Bundle savedInstanceState给当前Activity，只要判断这个非空就能知道是否是恢复流程。

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
       }

知道恢复流程之后，如何处理呢？其实很简单，直接吊起闪屏页就可以了，不过这里有一点要注意的是，在启动闪屏页面的时候，必须要设置其IntentFlag：Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_CLEAR_TASK，这样做的理由是为了清理之前的场景，不然之前的ActivityRecord栈仍然保留在ActivityManagerService中，具体实现如下，放在BaseActivity中就可以：

    Intent intent = new Intent(this, SplashActivity.class);
    intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_CLEAR_TASK);
    startActivity(intent);

如果不设置会怎么样呢？举个例子，最常见的就是闪屏之后跳转主界面，主界面经常有router逻辑，并且其启动模式一般都是singleTask，处理一些推送，所以其onCreate跟onNewIntent都有相应的处理，如果不设置，在闪屏结束后，在startActivity启动主界面的时候，其实是先走恢复逻辑，然后走singleTask的onNewIntent逻辑，也就是说，onNewIntent跟onCreate是会同时调用的，也可能就会引发重复处理的逻辑，因此最好清理干净。

# 从推送唤起被杀APP时，如何走闪屏逻辑

对于推送消息的处理，其路由器一般放在MainActivity，并且在onCreate跟onNewIntent都有添加，如果APP存活的情况，可以直接跳转目标页面，如果APP被杀，这个时候，希望先跳转主界面，再跳转目标页面，在效果上来看就是，用户先看到目标页面，点击返回的时候再看到主界面，如果加上闪屏，希望达到的效果是先看到闪屏、点击返回看到目标页，再点击返回看到主页面。如果简单划分一下推送场景，可以看做一下三种

* 进程存活，Activity存活
* 进程存活，但是没有Activity存活
* 进程不存在（无论是否被杀）

其实后面两种完全可以看做一种，这个时候，都是要先start MainActivity，然后让MainActivity在其OnCreate中通过startActivityForResult启动SplashActivity，SplashActivity返回后，在start TargetActivity。下面的讨论都是针对后面两种，需要做的有两件事

* 一是：检测出后面两种场景，并且在唤起主界面的时候需要添加Intent.FLAG_ACTIVITY_CLEAR_TASK清理之前的现场
* 二是：在MainActivity的路由系统中，针对这两种场景要，先跳转闪屏，闪屏回来后，再跳转推送页

如何判断呢，后面两种场景其实只需要判断是否有Activity存活即可，也就是查查APP的topActivity是否为null，**注意不要去向AMS查询，而是在本地进程中查询**，可以通过反射查询ActivityThread的mActivities，也可以根据自己维护的Activity堆栈来判断，判断没有存活Activity的前提下，就跳转主页面去路由

    Intent intent = new Intent(this, MainActivity.class);
    intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_CLEAR_TASK);
    intent.setDate(跳转的Uri scheme)
    startActivity(intent);

在MainActivity的路由中，需要准确区分是否是推送跳转进来的，如果不是推送跳转进来，就不需要什么特殊处理，如果是推送跳转进来一定会携带跳转scheme数据，根据是否携带数据做区分即可，看一下MainActivity的代码：

  
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Uri uri= getIntent().getData();
        <!--只有在intent被设置了跳转数据的时候才去跳转，一般是推送就来，如果冷启动，是没有数据的-->
        if(uri!=null){
            SplashActivity.startActivityForResult(this,JUMP_TO_TARGET)
        }
    }
    <!--Intent.FLAG_ACTIVITY_CLEAR_TASK保证了onNewIntent被调用的时候，进程一定是正常活着的-->
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
            getIntent().setData(null);
        }
    }

    private void router(Uri uri) {

    }
    
通过上面两部分的处理，基本能够满足APP“死亡”的情况下，先跳转闪屏的需求。 
