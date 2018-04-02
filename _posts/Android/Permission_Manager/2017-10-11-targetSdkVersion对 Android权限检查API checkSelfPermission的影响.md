---
layout: post
title: "targetSdkVersion对 Android权限检查API checkSelfPermission的影响"
category: android
 

---

Android6.0之后，权限分为install时的权限跟运行时权限，如果我们的targetSdkVersion>=23，install权限同runtime权限是分开的，app也要针对6.0已经做适配，没什么大问题，无论运行在旧版本还是6.0之后的手机上都ok，这也是Google推荐的适配方案。但是如果targetSdkVersion < 23 ,在6.0之后的手机上就会遇到一些问题，因为在这种情况下默认权限是全部授予的，但是可能会被用户手动取消，而Context的checkSelfPermission权限检查接口也会失效，因为这个API接口6.0之后用的是runtime-permission的模型，而targetSdkVersion < 23 时候，app只有intalled的权限，其granted值一直是true，也可以看做是全部是授权了的，就算在设置里面取消授权也不会影响installed权限的granted，而Context的checkSelfPermission的接口却是用granted这个值作为授权与否的参考，所以如果用这个接口，那得到的一定是授权了，是不准确的，如下：targetSdkVersion < 23的时候，package信息中的权限包含app申请的全部权限,

    <package name="com.snail.labaffinity" codePath="/data/app/com.snail.labaffinity-1" nativeLibraryPath="/data/app/com.snail.labaffinity-1/lib" publicFlags="944291398" privateFlags="0" ft="15f0f58e548" it="15f0f58e548" ut="15f0f58e548" version="1" userId="10084">
        <perms>
            <item name="android.permission.ACCESS_FINE_LOCATION" granted="true" flags="0" />
            <item name="android.permission.INTERNET" granted="true" flags="0" />
            <item name="android.permission.READ_EXTERNAL_STORAGE" granted="true" flags="0" />
            <item name="android.permission.ACCESS_COARSE_LOCATION" granted="true" flags="0" />
            <item name="android.permission.READ_PHONE_STATE" granted="true" flags="0" />
            <item name="android.permission.CALL_PHONE" granted="true" flags="0" />
            <item name="android.permission.CAMERA" granted="true" flags="0" />
            <item name="android.permission.WRITE_EXTERNAL_STORAGE" granted="true" flags="0" />
            <item name="android.permission.READ_CONTACTS" granted="true" flags="0" />
        </perms>
        <proper-signing-keyset identifier="18" />
    </package>
 
这种情况下，该做法就会引发问题，先从源码看一下为什么targetSdkVersion < 23 Context 的 checkSelfPermission方法失效，之后再看下在targetSdkVersion < 23 的时候，如何判断6.0的手机是否被授权。
    
# 为什么targetSdkVersion < 23 Context 的 checkSelfPermission失效

跟踪一下源码发现Context 的 checkSelfPermission最终会调用ContextImp的checkPermission，最终调用

    @Override
    public int checkPermission(String permission, int pid, int uid) {
        if (permission == null) {
            throw new IllegalArgumentException("permission is null");
        }

        try {
            return ActivityManagerNative.getDefault().checkPermission(
                    permission, pid, uid);
        } catch (RemoteException e) {
            return PackageManager.PERMISSION_DENIED;
        }
    }
    
最终请求ActivityManagerService的checkPermission，经过预处理跟中转最后会调用PackageManagerService的checkUidPermission

    @Override
    public int checkUidPermission(String permName, int uid) {
        final int userId = UserHandle.getUserId(uid);
        synchronized (mPackages) {
        <!--查询权限-->
            Object obj = mSettings.getUserIdLPr(UserHandle.getAppId(uid));
            if (obj != null) {
                final SettingBase ps = (SettingBase) obj;
                final PermissionsState permissionsState = ps.getPermissionsState();
                <!--检验授权-->
                if (permissionsState.hasPermission(permName, userId)) {
                    return PackageManager.PERMISSION_GRANTED;
                }
                if (Manifest.permission.ACCESS_COARSE_LOCATION.equals(permName) && permissionsState
                        .hasPermission(Manifest.permission.ACCESS_FINE_LOCATION, userId)) {
                    return PackageManager.PERMISSION_GRANTED;
                }
            } ...        }

        return PackageManager.PERMISSION_DENIED;
    }

PackageManagerService会从mSettings全局变量中获取权限，然后进一步验证权限是否被授予

    public boolean hasPermission(String name, int userId) {
        enforceValidUserId(userId);

        if (mPermissions == null) {
            return false;
        }

        PermissionData permissionData = mPermissions.get(name);
        return permissionData != null && permissionData.isGranted(userId);
    }

这里的检查点只有两点，第一个是是否有这个权限，第二是是否是Granted，对于targetSdkVersion<23的所有的权限都在packages.xml中，grante一直是true，无法被跟新，为什么无法被更新呢？看一下6.0之后的授权与取消授权的函数，首先看一个变量mAppSupportsRuntimePermissions

        mAppSupportsRuntimePermissions = packageInfo.applicationInfo
                .targetSdkVersion > Build.VERSION_CODES.LOLLIPOP_MR1;
        mAppOps = context.getSystemService(AppOpsManager.class);
        
mAppSupportsRuntimePermissions定义在AppPermissionGroup中，6.0之后权限都是分组的，对于targetSdkVersion<23的APP来说，很明显是不支持动态权限管理的，那么授权跟取消授权函数就很不一样如下： 授权函数
    
    public boolean grantRuntimePermissions(boolean fixedByTheUser, String[] filterPermissions) {
        final int uid = mPackageInfo.applicationInfo.uid;

        for (Permission permission : mPermissions.values()) {
            if (filterPermissions != null
                    && !ArrayUtils.contains(filterPermissions, permission.getName())) {
                continue;
            }

            <!--关键点1 如果支持，也即是targetSdkVersion>23那走6.0动态权限管理那一套-->
            if (mAppSupportsRuntimePermissions) {
                // Do not touch permissions fixed by the system.
                if (permission.isSystemFixed()) {
                    return false;
                }
               // Ensure the permission app op enabled before the permission grant.
                if (permission.hasAppOp() && !permission.isAppOpAllowed()) {
                    permission.setAppOpAllowed(true);
                    mAppOps.setUidMode(permission.getAppOp(), uid, AppOpsManager.MODE_ALLOWED);
                }
               // Grant the permission if needed.
                if (!permission.isGranted()) {
                    permission.setGranted(true);
                    <!--关键点2更新其runtime-permission.xml 中granted值-->
                    mPackageManager.grantRuntimePermission(mPackageInfo.packageName,
                            permission.getName(), mUserHandle);
                }
				...
            } else {
                if (!permission.isGranted()) {
                    continue;
                }

                int killUid = -1;
                int mask = 0;
                if (permission.hasAppOp()) {
                    if (!permission.isAppOpAllowed()) {
                        permission.setAppOpAllowed(true);
                        <!--关键点3 设置为AppOpsManager.MODE_ALLOWED-->
                        mAppOps.setUidMode(permission.getAppOp(), uid, AppOpsManager.MODE_ALLOWED);
                        killUid = uid;
                    }
                }
					<!--关键点4 更新其PermissionFlags-->
                if (mask != 0) {
                    mPackageManager.updatePermissionFlags(permission.getName(),
                            mPackageInfo.packageName, mask, 0, mUserHandle);
                }
            }
        }
       return true;
    }
 
 可以看出6.0之后的手机，针对targetSdkVersion是否高于23做了不同处理，如果targetSdkVersion>=23支持动态权限管理，那就更新动态权限，并将其持久化到runtime-permission.xml中，并更新其granted值，如果targetSdkVersion<23 ,也即是不知道6.0的动态管理，那就只更新AppOps，这是4.3引入的老的动态权限管理模型，不过这里主要是将权限持久化到appops.xml中，不过对于其granted的值是没有做任何更新的，仅仅是更新了packages.xml中的flag，这个flag可以配合appops.xml标识是否被授权（对于targetSdkVersion<23的适用），以上就是为什么context checkSelfPermission会失效的原因，涉及代码很多，不一一列举，对于取消授权revokeRuntimePermissions函数，模型一样，不在赘述，那下面看第二个问题，如何检查targetSdkVersion<23 app 在6.0以上手机的权限呢？ Google给了一个兼容类PermissionChecker，这个类可以间接使用AppOpsService那一套逻辑，获取到权限是否被授予。
 
# targetSdkVersion < 23 的时候，如何判断6.0的手机是否被授权 

targetSdkVersion < 23的时候，6.0权限检查API失效了，不过通过上面的分析指导，在设置中权限的操作仍然会被存储内存及持久化到appops.xml文件中，这里就是走的AppOpsService那一套，AppOpsService可以看做6.0为了兼容老APP而保留的一个附加的权限管理模型，在6.0之后的系统中，可以看做runtime权限管理的补充，其实AppOpsService这套在4.3就推出了，不过不太灵活，基本没啥作用，之前只用到了通知管理。看一下Google提供的一个兼容类PermissionChecker如何做的:

	public static int checkPermission(@NonNull Context context, @NonNull String permission,
	            int pid, int uid, String packageName) {
	        <!--对于targetSdkVersion < 23 一定是true-->
	        if (context.checkPermission(permission, pid, uid) == PackageManager.PERMISSION_DENIED) {
	            return PERMISSION_DENIED;
	        }
		        String op = AppOpsManagerCompat.permissionToOp(permission);
	        <!--看看这个权限是不是能够操作，动态授权与取消授权  如果不能，说明权限一直有-->
	        if (op == null) {
	            return PERMISSION_GRANTED;
	        }
	       <!--如果能够取消授权，就看现在是不是处于权限被允许的状态，如果不是，那就是用户主动关闭了权限-->
	        if (AppOpsManagerCompat.noteProxyOp(context, op, packageName)
	                != AppOpsManagerCompat.MODE_ALLOWED) {
	            return PERMISSION_DENIED_APP_OP;
	        }
		  return PERMISSION_GRANTED;
	    }

对于6.0之后的手机AppOpsManagerCompat.noteProxyOp会调用AppOpsManager23的noteProxyOp，

    private static class AppOpsManagerImpl {
        public String permissionToOp(String permission) {
            return null;
        }

        public int noteOp(Context context, String op, int uid, String packageName) {
            return MODE_IGNORED;
        }

        public int noteProxyOp(Context context, String op, String proxiedPackageName) {
            return MODE_IGNORED;
        }
    }

    private static class AppOpsManager23 extends AppOpsManagerImpl {
        @Override
        public String permissionToOp(String permission) {
            return AppOpsManagerCompat23.permissionToOp(permission);
        }

        @Override
        public int noteOp(Context context, String op, int uid, String packageName) {
            return AppOpsManagerCompat23.noteOp(context, op, uid, packageName);
        }

        @Override
        public int noteProxyOp(Context context, String op, String proxiedPackageName) {
            return AppOpsManagerCompat23.noteProxyOp(context, op, proxiedPackageName);
        }
    }
    
上面的是6.0之前对应的API，下面的是6.0及其之后对应的接口，AppOpsManagerCompat23.noteProxyOp会进一步调用AppOpsManager的noteProxyOp向AppOpsService发送请求

    public static int noteProxyOp(Context context, String op, String proxiedPackageName) {
        AppOpsManager appOpsManager = context.getSystemService(AppOpsManager.class);
        return appOpsManager.noteProxyOp(op, proxiedPackageName);
    }

最后看一下AppOpsService如何检查权限

    private int noteOperationUnchecked(int code, int uid, String packageName,
            int proxyUid, String proxyPackageName) {
        synchronized (this) {
            Ops ops = getOpsLocked(uid, packageName, true);
            Op op = getOpLocked(ops, code, true);
            if (isOpRestricted(uid, code, packageName)) {
                return AppOpsManager.MODE_IGNORED;
            }
            op.duration = 0;
            final int switchCode = AppOpsManager.opToSwitch(code);
            UidState uidState = ops.uidState;
            if (uidState.opModes != null) {
                final int uidMode = uidState.opModes.get(switchCode);
                    op.rejectTime = System.currentTimeMillis();
                    return uidMode;
                }
            }
            final Op switchOp = switchCode != code ? getOpLocked(ops, switchCode, true) : op;
            if (switchOp.mode != AppOpsManager.MODE_ALLOWED) {
                op.rejectTime = System.currentTimeMillis();
                return switchOp.mode;
            }
            op.time = System.currentTimeMillis();
            op.rejectTime = 0;
            op.proxyUid = proxyUid;
            op.proxyPackageName = proxyPackageName;
            return AppOpsManager.MODE_ALLOWED;
        }
    }
    
UidState可以看做每个应用对应的权限模型，这里的数据是有一部分是从appops.xml恢复回来，也有部分是在更新权限时候加进去的，这部分变化最终都要持久化到appops.xml中去，不过持久化比较滞后，一般要等到手机更新权限后30分钟才会持久化到appops.xml中，这里的数据一般是在启动的时候被恢复重建，在启动ActivityManagerService服务的时候，会在其构造函数总启动AppOpsService服务:

    public ActivityManagerService(Context systemContext) {
    ...
        mAppOpsService = new AppOpsService(new File(systemDir, "appops.xml"), mHandler);
    ...}    

在AppOpsService的构造函数中会将持久化到appops.xml中的权限信息恢复出来，并存到内存中去，

    public AppOpsService(File storagePath, Handler handler) {
        mFile = new AtomicFile(storagePath);
        mHandler = handler;
        // 新建的时候就会读取
        readState();
    }

readState就是将持久化的UidState数据给重新读取出来，如下mFile其实就是appops.xml的文件对象

    void readState() {
        synchronized (mFile) {
            synchronized (this) {
                FileInputStream stream;
                try {
                    stream = mFile.openRead();
                } catch (FileNotFoundException e) {
                }
                boolean success = false;
                mUidStates.clear();
                try {
                    XmlPullParser parser = Xml.newPullParser();
                    parser.setInput(stream, StandardCharsets.UTF_8.name());
                    int type;
                    int outerDepth = parser.getDepth();
                    while ((type = parser.next()) != XmlPullParser.END_DOCUMENT
                            && (type != XmlPullParser.END_TAG || parser.getDepth() > outerDepth)) {
                        if (type == XmlPullParser.END_TAG || type == XmlPullParser.TEXT) {
                            continue;
                        }
                        String tagName = parser.getName();
                        if (tagName.equals("pkg")) {
                            readPackage(parser);
                        } else if (tagName.equals("uid")) {
                            readUidOps(parser);
                        } else {
                            XmlUtils.skipCurrentTag(parser);
                        }
                    }
                    success = true;
                ...}
                
读取之后，当用户操作权限的时候，也会随机的更新这里的标记，只看下targetSdkVersion<23的，

	   public boolean grantRuntimePermissions(boolean fixedByTheUser, String[] filterPermissions) {
	        final int uid = mPackageInfo.applicationInfo.uid;
	
	        for (Permission permission : mPermissions.values()) {
	            if (filterPermissions != null
	                    && !ArrayUtils.contains(filterPermissions, permission.getName())) {
	                continue;
	            }
	            <!--关键点1 如果支持，也即是targetSdkVersion>23那走6.0动态权限管理那一套-->
	            if (mAppSupportsRuntimePermissions) {
					...
					同样会使用 mAppOps.setUidMode更新 AppopsServie管理系统
					  if (permission.hasAppOp() && !permission.isAppOpAllowed()) {
                    permission.setAppOpAllowed(true);
                    mAppOps.setUidMode(permission.getAppOp(), uid, AppOpsManager.MODE_ALLOWED);
                }
                
	            } else {
	                if (!permission.isGranted()) {
	                    continue;
	                }
	                int killUid = -1;
	                int mask = 0;
	                if (permission.hasAppOp()) {
	                    if (!permission.isAppOpAllowed()) {
	                        permission.setAppOpAllowed(true);
	                        <!--关键点3 设置为AppOpsManager.MODE_ALLOWED-->
	                        mAppOps.setUidMode(permission.getAppOp(), uid, AppOpsManager.MODE_ALLOWED);
	                        killUid = uid;
	                    }
	                }
	                if (mask != 0) {
	                    mPackageManager.updatePermissionFlags(permission.getName(),
	                            mPackageInfo.packageName, mask, 0, mUserHandle);
	                }
	            }
	        }
	       return true;
	    }
	    
拿授权的场景来说，其实关键就是 mAppOps.setUidMode(permission.getAppOp(), uid, AppOpsManager.MODE_ALLOWED)函数，这个函数会更新AppOpsService中对于权限的标记，并将权限是否授予的信息持久化到appops.xml及packages.xml，不同版本可能有差别，有可能需要appops.xml跟packages.xml配合才能确定是否授予权限，具体没深究，有兴趣可以自行分析。

    @Override
    public void setUidMode(int code, int uid, int mode) {
        if (Binder.getCallingPid() != Process.myPid()) {
            mContext.enforcePermission(android.Manifest.permission.UPDATE_APP_OPS_STATS,
                    Binder.getCallingPid(), Binder.getCallingUid(), null);
        }
        verifyIncomingOp(code);
        code = AppOpsManager.opToSwitch(code);

        synchronized (this) {
            final int defaultMode = AppOpsManager.opToDefaultMode(code);
           <!--更新操作权限-->
            UidState uidState = getUidStateLocked(uid, false);
            if (uidState == null) {
                if (mode == defaultMode) {
                    return;
                }
                uidState = new UidState(uid);
                uidState.opModes = new SparseIntArray();
                uidState.opModes.put(code, mode);
                mUidStates.put(uid, uidState);
                scheduleWriteLocked();
            } else if (uidState.opModes == null) {
                if (mode != defaultMode) {
                    uidState.opModes = new SparseIntArray();
                    uidState.opModes.put(code, mode);
                    scheduleWriteLocked();
                }
            } else {
                if (uidState.opModes.get(code) == mode) {
                    return;
                }
                if (mode == defaultMode) {
                    uidState.opModes.delete(code);
                    if (uidState.opModes.size() <= 0) {
                        uidState.opModes = null;
                    }
                } else {
                    uidState.opModes.put(code, mode);
                }
                <!--持久化到appops.xml-->
                scheduleWriteLocked();
            }
        }
      ...
    }
    
这里有一点注意：scheduleWriteLocked并不是立即执行写操作，而是比更新内存滞后，一般滞后30分钟

    static final long WRITE_DELAY = DEBUG ? 1000 : 30*60*1000;

30分钟才会去更新 ，不过内存中都是最新的 ，如果直接删除appops.xml，然后意外重启，比如adb reboot bootloader，那么你的所有AppOpsService权限标记将会被清空，经过验证，是符合预期的，也就说，targetSdkVersion<23的情况下，Android6.0以上的手机，它的权限操作是持久化在appops.xml中的，一般关机的时候，会持久化一次，如果还没来得及持久化，异常关机，就会丢失，这点同runtime-permission类似，异常关机也会丢失，不信可以试验一下 。

# 对于targetSdkVersion<23检查6.0权限情况的解决方案
	
针对targetSdkVersion做如下兼容即可
	    
	public boolean selfPermissionGranted(Context context, String permission) {

		boolean ret = true;
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
			if (targetSdkVersion >= Build.VERSION_CODES.M) {
				ret = context.checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED;
			} else {
		      ret = PermissionChecker.checkSelfPermission(context, permission) == PermissionChecker.PERMISSION_GRANTED;
			}
		}
		return ret;
	}	    

# 在targetSdkVersion>=23的时候，对于 SDK>=23的机器如何检测权限


targetSdkVersion>=23系统已经提供了比较合理的检测手段，PermisionChecker的checkPermission就可以，不过，这里需要注意的是，AppOpsService对于targetSdkVersion>=23的时候就不能用了，这里可能是Android的一个bug，当targetSdkVersion>=23而SDK_Version>=23的，对于AppOpsService，权限的授予跟撤销不是配对的，如下，先简单看下授权：


	   public boolean grantRuntimePermissions(boolean fixedByTheUser, String[] filterPermissions) {
	        final int uid = mPackageInfo.applicationInfo.uid;

	        for (Permission permission : mPermissions.values()) {

	            if (mAppSupportsRuntimePermissions) {
	 	  					<!--关键点1 同时更新runtim-permission及Appops-->
	                 if (permission.hasAppOp() && !permission.isAppOpAllowed()) {
	                    permission.setAppOpAllowed(true);
	                    mAppOps.setUidMode(permission.getAppOp(), uid, AppOpsManager.MODE_ALLOWED);
	                }
	                if (!permission.isGranted()) {
	                    permission.setGranted(true);
	                    mPackageManager.grantRuntimePermission(mPackageInfo.packageName,
	                            permission.getName(), mUserHandle);
	                }
	            } else {
	                if (!permission.isGranted()) {
	                    continue;
	                }
	
	                int killUid = -1;
	                int mask = 0;
 					<!--关键点2 更新Appops-->

	                if (permission.hasAppOp()) {
	                    if (!permission.isAppOpAllowed()) {
	                        permission.setAppOpAllowed(true);
	                        // Enable the app op.
	                        mAppOps.setUidMode(permission.getAppOp(), uid, AppOpsManager.MODE_ALLOWED);
	                        killUid = uid;
	                    }
	              ...
	            }
	        }
	
	        return true;
	    }
	    
可见，对于6.0的系统，无论targetSdkVersion是否>=23，在授权的时候，都会更新appops.xml，那取消授权呢？
    
	public boolean revokeRuntimePermissions(boolean fixedByTheUser, String[] filterPermissions) {
	        final int uid = mPackageInfo.applicationInfo.uid;
	        for (Permission permission : mPermissions.values()) {
	            ...
	            if (mAppSupportsRuntimePermissions) {
	                if (permission.isSystemFixed()) {
	                    return false;
	                }
	
	                // Revoke the permission if needed.
	                if (permission.isGranted()) {
	                    permission.setGranted(false);
	                    mPackageManager.revokeRuntimePermission(mPackageInfo.packageName,
	                            permission.getName(), mUserHandle);
	                }
	                <!--关键点1 这里没有使用mAppOps.setUidMode更新appops.xml文件->
	                
	            } else {
	                // Legacy apps cannot have a non-granted permission but just in case.
	                if (!permission.isGranted()) {
	                    continue;
	                }
	
	                int mask = 0;
	                int flags = 0;
	                int killUid = -1;
	                if (permission.hasAppOp()) {
	                    if (permission.isAppOpAllowed()) {
                       <!--关键点2 这里使用mAppOps.setUidMode更新appops.xml文件->
	                        mAppOps.setUidMode(permission.getAppOp(), uid, AppOpsManager.MODE_IGNORED);
	                        killUid = uid;
	                    }
                      ...
	            }
	        }
	
	        return true;
	    }

看关键点1 ，如果targetSdkVersion>=23在取消授权的时候，是不会更新appops.xml的，只有在targetSdkVersion<23的时候，才会向关键点2，撤销授权。也就是说对于targetSdkVersion>=23的时候，不要用AppOpsManager了。

# 对于6.0以下的手机权限如何检测

对于Android6.0以下的手机，不需要关心targetVersion。先说个自己验证的结果：**基本没法检测，同时也不需要检测**，就算检测出来也没有多大意义，因为，触发时机是在真正的调用服务时候。对于4.3到6.0之前的国产ROM，虽然采用AppopsManagerService，但是并未按照Google的模型对所有权限进行适配，在这个模型下，也就适配了两个权限，(悬浮窗还不一定有)

* 通知权限            public static final int OP_POST_NOTIFICATION = 11;
* 悬浮窗权限          public static final int OP_SYSTEM_ALERT_WINDOW = 24; 

Google发行版的APPOpsService，基本是把整个鉴权逻辑给屏蔽了，通过CM的源码，课对这部分代码窥探一斑，如果整个权限都采用4.3权限管理模型，在拒绝一项权限的时候，这个操作会被持久化到appops.xml中去，但是具体看下去，其实并不是如此，这种机制只对以上两个权限生效：

		<pkg n="com.xxx">
		<uid n="10988">
		<!--关键点1-->
		<op n="11" m="1" t="1513145979969" r="1521550658067" />
		<op n="12" t="1521550651593" />
		<op n="29" t="1521550682769" />

		<pkg n="com.wandoujia.phoenix2.usbproxy">
		<uid n="10969">
		<op n="4" t="1517279031173" />
		 <!--关键点2-->
		<op n="11" m="1" t="1510889291834" r="1517279030708" />
		<op n="14" t="1517293452801" />
		<!--关键点3-->
		<op n="24" m="1" />
		<op n="40" t="1513599239364" d="600011" />

国产rom中，假如你拒绝授权位置权限，按照AppOpsService模型，该操作应该被持久化到appops.xml中去，但是，结果并非如此，也就是说，对于其他权限，国产ROM应该是自己糊弄了一套持久管理，持久化Android系统API无法访问的地方，仅仅为自身ROM可见。appops.xml真正被系统使用时从Android6.0开始，其实Android6.0是有两套权限管理的，这其实很混乱，不知道Google怎么想的，不过6.0似乎也有漏洞：**权限的授予跟回收权限好像并不配对**。

那么这就带来了一个问题，在Android4.3到Android6.0之间的版本，并没有同一个API来检测是否获取了某种权限，因为你动态更新的权限并未持久化到appops.xml中去。对于Android6.0之前的ROM，虽然不能检测，但完全可以直接用服务，不会崩溃，因为如果真需要鉴权，它的鉴权时机其实是在服务使用的时候。AppopsManager在6.0之前，只能用来检测通知，可能还有悬浮窗。


# 总结

Android6.0系统其实支持两种动态管理，runtime-permission及被阉割的AppOpsService，当targetSdkVersion>23的时候，采用rumtime-permission，当 targetSdkVersion<23的时候，两者兼有，其实targetSdkVersion<23的时候，仍然可以动态申请6.0的权限，前提是你要采用23之后的compileSdkVersion，只有这样才能用相应的API，不过还是推荐升级targetSdkVersion，这才是正道。对于Android6.0以下的手机，除了通知（可能还有悬浮窗），其他权限基本都没有系统的检测手段，无论Context的checkPermission还是AppopsManager的checkOp，基本都是对Android6.0之后才有效。