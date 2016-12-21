---
layout: default
title: PackageMangerService分析
category: JNI

---

### PackageMangerService分析

PackageMangerService的一些重要概念
	
	 final HashMap<String, PackageParser.Package> mPackages =  //以包名为key保存系统中所以安装的Package  
	            new HashMap<String, PackageParser.Package>();  
	                     
    final HashMap<String, FeatureInfo> mAvailableFeatures =//系统可用特性  
            new HashMap<String, FeatureInfo>();  
  
    // All available activities, for your resolving pleasure.  
    final ActivityIntentResolver mActivities =//系统所有的Activity  
            new ActivityIntentResolver();  
  
    // All available receivers, for your resolving pleasure.  
    final ActivityIntentResolver mReceivers =//系统所有的Receivers  
            new ActivityIntentResolver();  
  
    // All available services, for your resolving pleasure.  
    final ServiceIntentResolver mServices = new ServiceIntentResolver();//系统所有的Services  
  
    // Keys are String (provider class name), values are Provider.  
    final HashMap<ComponentName, PackageParser.Provider> mProvidersByComponent =//以ComponentName为key，系统所有的Provider  
            new HashMap<ComponentName, PackageParser.Provider>();  

	final HashMap<String, PackageParser.Provider> mProviders =//以Name为key，系统所有的Provider  