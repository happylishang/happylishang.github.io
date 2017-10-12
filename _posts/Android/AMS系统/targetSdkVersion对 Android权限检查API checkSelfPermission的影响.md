
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
 
 可以看出6.0之后的手机，针对targetSdkVersion是否高于23做了不同处理，如果targetSdkVersion>=23支持动态权限管理，那就更新动态权限，并将其持久化到runtime-permission.xml中，并更新其granted值，如果targetSdkVersion<23 ,也即是不知道6.0的动态管理，那就只更新AppOps，这是4.3引入的老的动态权限管理模型，不过这里主要是将权限持久化到appops.xml中，不过对于其granted的值是没有做任何更新的，仅仅是更新了packages.xml中的flag，这个flag可以配合appops.xml标识是否被授权（对于targetSdkVersion<23的适用），以上就是为什么context checkSelfPermission会失效的原因，涉及代码很多，不一一列举，对于取消授权revokeRuntimePermissions函数，模型一样，不在赘述，那下面看第二个问题，如何检查targetSdkVersion<23 app 在6.0以上手机的权限呢？ Google给了一个兼容类PermissionChecker，这个类可以间接的都AppOpsService那一套逻辑，获取到权限是否被授予。
 
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
    
UidState可以看做每个应用对应的全下模型，这里的数据是有一部分是从appops.xml恢复回来，也有部分是在更新权限时候加进去的，这部分变化最终都要持久化到appops.xml中去，

* checkOp(String op, int uid, String packageName) 只读取不做记录，权限不通过会抛异常 
* noteOp(String op, int uid, String packageName) 和checkOp基本相同，但是在检验后会做记录。
* checkOpNoThrow(String op, int uid, String packageName) 和checkOp类似，但是权限错误，不会抛出SecurityException，而是返回AppOpsManager.MODE_ERRORED.
* noteOpNoThrow(String op, int uid, String packageName) 类似noteOp，但不会抛出SecurityException。
文件写到哪里去？ 
    
data/system/users/0/runtime-permissions.xml 不会记录targetSdkVersion<23的权限信息，这种情况下，app所有的权限信息都是持久化在data/system/packages.xml文件中，并且不支持更新，也就是说，如果用6.0的那种检查方法，那一直是赋予的，可以看到信息都在packages.xml中，并且这个文件中的权限赋予信息granted一直是true，这也就解释了为什么采用6.0的checkpermission会一直返回true，因为6.0以后，敏感权限与普通权限都是走的动态权限管理的那一套，如果targetSdkVersion<23那就认为所有的权限都是普通权限，默认全是true，而在这个时候所有的动态权限操作都是持久化到appops.xml文件中去的，也就是设置里面的看到的。

        <perms>
            <item name="android.permission.READ_EXTERNAL_STORAGE" granted="true" flags="0" />
            <item name="android.permission.READ_PHONE_STATE" granted="true" flags="8" />
            <item name="android.permission.WRITE_EXTERNAL_STORAGE" granted="true" flags="0" />
            <item name="android.permission.READ_CONTACTS" granted="true" flags="0" />
        </perms>
        
至于权限更新的时候，仍然走的不是6.0的那套逻辑，虽然很乱，但是不得不说，对于APP开发的兼容还是很好的，还是存在两套，只不过，另一套作为鸡肋的陪衬而已，没有完全割除干净，不过这里仍然有一个很神奇的地方，我们仍然可以用compileSdkVersion>23的进行编译，在targetSdkVersion=22的app里动态申请权限，不过走的就是Android原来的那套不太完善的4.3动态权限管理系统，而做的兼容处理可能就是flag，flags==0，表示是授权，其他是未授权。设置里面的东西，会持久化到packages.xml
 

 
###  6.0手机上运行时权限的判断  mAppSupportsRuntimePermissions 是否支持呢？
 
     public boolean areRuntimePermissionsGranted(String[] filterPermissions) {
        if (LocationUtils.isLocationGroupAndProvider(mName, mPackageInfo.packageName)) {
            return LocationUtils.isLocationEnabled(mContext);
        }
        final int permissionCount = mPermissions.size();
        for (int i = 0; i < permissionCount; i++) {
            Permission permission = mPermissions.valueAt(i);
            if (filterPermissions != null
                    && !ArrayUtils.contains(filterPermissions, permission.getName())) {
                continue;
            }
            if (mAppSupportsRuntimePermissions) {
                if (permission.isGranted()) {
                    return true;
                }
            } else if (permission.isGranted() && (permission.getAppOp() == null(没有操作过)
                    || permission.isAppOpAllowed())) {
                <!--如果targetSdkVersion<23-->
                return true;
            }
        }
        return false;
    }


持久化数据的恢复


    public static AppPermissionGroup create(Context context, PackageInfo packageInfo,
            PackageItemInfo groupInfo, List<PermissionInfo> permissionInfos,
            UserHandle userHandle) {

        AppPermissionGroup group = new AppPermissionGroup(context, packageInfo, groupInfo.name,
                groupInfo.packageName, groupInfo.loadLabel(context.getPackageManager()),
                loadGroupDescription(context, groupInfo), groupInfo.packageName, groupInfo.icon,
                userHandle);
        ...

        final int permissionCount = packageInfo.requestedPermissions.length;
        for (int i = 0; i < permissionCount; i++) {
            String requestedPermission = packageInfo.requestedPermissions[i];
         ...
            // 注意看这里的allowed，这里肯定是启动或者安装的时候进行加载的                     
            final boolean appOpAllowed = appOp != null
                    && context.getSystemService(AppOpsManager.class).checkOpNoThrow(appOp,
                    packageInfo.applicationInfo.uid, packageInfo.packageName)
                    == AppOpsManager.MODE_ALLOWED;
                    
          }

这里的数据，怎么处理的

    @Override
    public int checkOperation(int code, int uid, String packageName) {
        verifyIncomingUid(uid);
        verifyIncomingOp(code);
        synchronized (this) {
            if (isOpRestricted(uid, code, packageName)) {
                return AppOpsManager.MODE_IGNORED;
            }
            code = AppOpsManager.opToSwitch(code);
            UidState uidState = getUidStateLocked(uid, false);
            if (uidState != null && uidState.opModes != null) {
                final int uidMode = uidState.opModes.get(code);
                if (uidMode != AppOpsManager.MODE_ALLOWED) {
                    return uidMode;
                }
            }
            Op op = getOpLocked(code, uid, packageName, false);
            if (op == null) {
                return AppOpsManager.opToDefaultMode(code);
            }
            return op.mode;
        }
    }

# adb shell dumpsys appops 查看命令
    
    
###  持久化到appops.xml中，更新appops.xml滞后 

    static final long WRITE_DELAY = DEBUG ? 1000 : 30*60*1000;

30分钟才会去更新 ，内存中都是最新的 ，如果直接删除，然后意外重启，比如reboot bootloader，那么你的所有权限将会被清空，经过验证，是合理的，也就说，targetSdkVersion<23的情况下，Android6.0以上的手机，它的权限操作是持久化在appops.xml中的，如果还没来得及持久化，一般关机的时候，会持久化一次，如果异常关机，就会丢失，但是同runtime-permission分离，不过runtime-permission的持久化同这个是一致的，异常关机也会丢失，不信可以试验一下 
 
	 <pkg n="com.snail.labaffinity">
		<uid n="10084" p="false">
			<op n="0" r="1507791144179" />
			<op n="1" r="1507791144180" />
			<op n="4" r="1507791144178" />
			<op n="11" t="1507791150963" pu="0" />
			<op n="13" t="1507791144180" />
			<op n="26" r="1507791144180" />
			<op n="45" t="1507791154995" d="14561" />
			<op n="51" t="1507791144175" />
			<op n="59" t="1507789244941" r="1507791302296" pu="0" />
			<op n="60" t="1507789244941" r="1507791144177" pu="0" />
		</uid>
	</pkg>
   	    
# 解决方案
	    
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
    