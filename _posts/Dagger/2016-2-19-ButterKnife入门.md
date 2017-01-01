---
layout: default
title: ButterKnife入门本
categories: [android,ButterKnife]

---
 
####   ButterKnife是一个专注于Android系统的View注入框架

	class ExampleActivity extends Activity {
	  TextView title;
	  TextView subtitle;
	  TextView footer;
	
	  @Override public void onCreate(Bundle savedInstanceState) {
	    super.onCreate(savedInstanceState);
	    setContentView(R.layout.simple_activity);
	    title = (TextView) findViewById(R.id.title);
	    subtitle = (TextView) findViewById(R.id.subtitle);
	    footer = (TextView) findViewById(R.id.footer);
	
	    // TODO Use views...
	  }
	}


	// 带有 Button 参数
	@OnClick(R.id.submit)
	public void sayHi(Button button) {
	  button.setText("Hello!");
	}
	 
	// 不带参数
	@OnClick(R.id.submit)
	public void submit() {
	  // TODO submit data to server...
	}
	 
	// 同时注入多个 View 事件
	@OnClick({ R.id.door1, R.id.door2, R.id.door3 })
	public void pickDoor(DoorView door) {
	  if (door.hasPrizeBehind()) {
	    Toast.makeText(this, "You win!", LENGTH_SHORT).show();
	  } else {
	    Toast.makeText(this, "Try again", LENGTH_SHORT).show();
	  }
	}


#### ProGuard

Butter Knife使用动态生成的代码，这可能使ProGuard认为这些代码是无用的。为了避免这些代码被混淆，你可以添加如下代码到你的ProGuard中：

	-keep class butterknife.** { *; }
	-dontwarn butterknife.internal.**
	-keep class **$$ViewBinder { *; }
	
	-keepclasseswithmembernames class * {
	    @butterknife.* <fields>;
	}
	
	-keepclasseswithmembernames class * {
	    @butterknife.* <methods>;
	}

####  如果上面不行，试试下面
	
	-keep class butterknife.** { *; }
	-dontwarn butterknife.internal.**
	-keep class **$$ViewBinder { *; }
	-dontwarn butterknife.internal.**
	-keep class **$$ViewInjector { *; }
	-keepnames class * { @butterknife.InjectView *;}


#### 参考文档
	
[ButterKnife--View注入框架](http://stormzhang.com/openandroid/android/2014/01/12/android-butterknife/)	

[Butter Knife 源码解析](https://mp.weixin.qq.com/s?__biz=MzA4MjU5NTY0NA==&mid=404147665&idx=1&sn=a16153b2a658db64ab80926cd3b76447&scene=1&srcid=0316uiFozajuenpaPdddoL2F&key=710a5d99946419d9e1debace429380f18dd76186706a6593a596cb1903db25c663b6d1c228c066fa428a6b67ef51eb55&ascene=0&uin=Mjc3OTU3Nzk1&devicetype=iMac+MacBookPro9%2C2+OSX+OSX+10.10.3+build%2814D136%29&version=11020201&pass_ticket=e3qL7YcbmknxduKwWiyzQxJoeiIW7hRFdqBaO206p868fDQqQ7UIiIsPe%2FiSY23E)