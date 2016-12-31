---
layout: post
title: "TodoLis"
categories: [TodoLis]

---

#### JS

#### MeasureSpec

#### Dagger

#### HotFix MutiDex

#### DynamicLoader

#### Chromium

#### 进程模型 Application oncreate多次

#### UncaughtExceptionHandler

#### 打包

#### SPDY Protocol 

#### 数据结构

#### 解耦

#### 外部存储与缓存

#### PackageClassLoader

#### Fragment的后台杀死重建 DialogFragment 为何会显示两个，

#### Fragment Activity getActivty =null  原理

#### 序列化 parcel 原理 ，数据与map的不同时存在 

#### 几种不同的存储位置 ，缓存清理，context.getApplicationContext()

#### 返回键 Activity退出源码分析 （isfinishing 后台会配置）

#### Service后台进程，前台进程区别，进程的几种状态

#### recycleView动画

#### 4.3回收机制跟5.1回收机制区别

			dafasdf

### RecycleView 

decoration
自适应 scroolview
流畅 ，动效


	  adapter = new EmailInputAdapter(context, "");
	        dataSetObserver = new DataSetObserver() {
	            @Override
	            public void onChanged() {
	                super.onChanged();
	                if (adapter.getCount() > 4) {
	                    accountEdit.setDropDownHeight(ResourcesUtil.getDimenPxSize(R.dimen.login_account_dropdown_height));
	                } else {
	                    accountEdit.setDropDownHeight(ResourcesUtil.getDimenPxSize(R.dimen.login_account_dropdown_single_height) * adapter.getCount());
	                }
	            }
	        };
	        adapter.registerDataSetObserver(dataSetObserver);
	        