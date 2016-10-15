---
layout: default
title: Android M权限适配    
categories: [android,OOM]

---

#Android M权限适配

# TargetSdkVersion的意义

Runtime Permissions 将权限大致划分为敏感权限和普通权限： PHONE，SMS ，LOCATION ，CONTACTS 之类的隐私属于敏感权限，而NFC，INTERNET，BLUETOOTH 之类的属于普通权限。如果app需要获取敏感权限时需要在运行时通过代码请求权限，让用户手动同意。

1、只有设备 && TargetSdkVersion >= 23 才生效，否则还是是采用安装时获取权限的方式。（向下兼容，向上适配）
2、Manifest 中还是需要定义权限
 

	Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
	Uri uri = Uri.fromParts("package", getPackageName(), null);
	intent.setData(uri);
	startActivityForResult(intent, REQUEST_PERMISSION_SETTING);
	
	
# google easy permmision 比较靠谱

#参考文档
 [Android M 新的运行时权限开发者需要知道的一切](http://jijiaxin89.com/2015/08/30/Android-s-Runtime-Permission/)      