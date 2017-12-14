Android 6.0没有权限的时候请求服务会崩溃，为什么呢？而targetSdkVersion<23的时候为什么不会崩溃

 
# 权限检查的时机

* 匿名打开Activity的时候，检查action对应的Permmission

检查权限目标组件需要的权限，比如拍照：ACTIVITY_RESTRICTION_PERMISSION


比如原生的拍照的Activity需要相机权限，如果没有，targetSdkVersion <23的时候，就不会启动该Activity，如果是targetSdkVersion>=23则会抛出异常，并可能崩溃

    // Activity actions an app cannot start if it uses a permission which is not granted.
    private static final ArrayMap<String, String> ACTION_TO_RUNTIME_PERMISSION =
            new ArrayMap<>();
    static {
        ACTION_TO_RUNTIME_PERMISSION.put(MediaStore.ACTION_IMAGE_CAPTURE,
                Manifest.permission.CAMERA);
        ACTION_TO_RUNTIME_PERMISSION.put(MediaStore.ACTION_VIDEO_CAPTURE,
                Manifest.permission.CAMERA);
        ACTION_TO_RUNTIME_PERMISSION.put(Intent.ACTION_CALL,
                Manifest.permission.CALL_PHONE);
    }


    private int getActionRestrictionForCallingPackage(String action,
            String callingPackage, int callingPid, int callingUid) {
        if (action == null) {
            return ACTIVITY_RESTRICTION_NONE;
        }

        String permission = ACTION_TO_RUNTIME_PERMISSION.get(action);
        if (permission == null) {
            return ACTIVITY_RESTRICTION_NONE;
        }

        final PackageInfo packageInfo;
        try {
            packageInfo = mService.mContext.getPackageManager()
                    .getPackageInfo(callingPackage, PackageManager.GET_PERMISSIONS);
        } catch (PackageManager.NameNotFoundException e) {
            Slog.i(TAG, "Cannot find package info for " + callingPackage);
            return ACTIVITY_RESTRICTION_NONE;
        }

        if (!ArrayUtils.contains(packageInfo.requestedPermissions, permission)) {
            return ACTIVITY_RESTRICTION_NONE;
        }

        // 6.0用这个,6.0一下以下
        if (mService.checkPermission(permission, callingPid, callingUid) ==
                PackageManager.PERMISSION_DENIED) {
            return ACTIVITY_RESTRICTION_PERMISSION;
        }
        // < 23 动态模型
        final int opCode = AppOpsManager.permissionToOpCode(permission);
        if (opCode == AppOpsManager.OP_NONE) {
            return ACTIVITY_RESTRICTION_NONE;
        }

        if (mService.mAppOpsService.noteOperation(opCode, callingUid,
                callingPackage) != AppOpsManager.MODE_ALLOWED) {
            return ACTIVITY_RESTRICTION_APPOP;
        }

        return ACTIVITY_RESTRICTION_NONE;
    }

**抛出异常，AMS帮助的鉴权**	
	
	e: Shutting down VM
	12-13 13:13:47.415  9040  9040 E AndroidRuntime: FATAL EXCEPTION: main
	12-13 13:13:47.415  9040  9040 E AndroidRuntime: Process: com.snail.labaffinity, PID: 9040
	12-13 13:13:47.415  9040  9040 E AndroidRuntime: java.lang.SecurityException: Permission Denial: starting Intent { act=android.media.action.IMAGE_CAPTURE cmp=com.google.android.GoogleCamera/com.android.camera.activity.CaptureActivity } from ProcessRecord{8012826 9040:com.snail.labaffinity/u0a142} (pid=9040, uid=10142) with revoked permission android.permission.CAMERA
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at android.os.Parcel.readException(Parcel.java:1620)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at android.os.Parcel.readException(Parcel.java:1573)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at android.app.ActivityManagerProxy.startActivity(ActivityManagerNative.java:2659)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at android.app.Instrumentation.execStartActivity(Instrumentation.java:1507)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at android.app.Activity.startActivityForResult(Activity.java:3930)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at android.app.Activity.startActivityForResult(Activity.java:3890)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at android.support.v4.app.FragmentActivity.startActivityForResult(FragmentActivity.java:843)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at android.app.Activity.startActivity(Activity.java:4213)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at android.app.Activity.startActivity(Activity.java:4181)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at com.snail.labaffinity.activity.MainActivity.third(MainActivity.java:52)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at com.snail.labaffinity.activity.MainActivity$$ViewBinder$3.doClick(MainActivity$$ViewBinder.java:41)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at butterknife.internal.DebouncingOnClickListener.onClick(DebouncingOnClickListener.java:22)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at android.view.View.performClick(View.java:5204)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at android.view.View$PerformClick.run(View.java:21153)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at android.os.Handler.handleCallback(Handler.java:739)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at android.os.Handler.dispatchMessage(Handler.java:95)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at android.os.Looper.loop(Looper.java:148)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at android.app.ActivityThread.main(ActivityThread.java:5417)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at java.lang.reflect.Method.invoke(Native Method)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at com.android.internal.os.ZygoteInit$MethodAndArgsCaller.run(ZygoteInit.java:726)
	12-13 13:13:47.415  9040  9040 E AndroidRuntime:        at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:616)
	12-13 13:13:47.417   798  3148 W ActivityManager:   Force finishing activity com.snail.labaffinity/.activity.MainActivity
	12-13 13:13:47.422   289   916 D audio_hw_primary: select_devices: out_snd_device(2: speaker) in_snd_device(0: none)
	12-13 13:13:47.422   289   916 D msm8974_platform: platform_send_audio_calibration: sending audio calibration for snd_device(2) acdb_id(15)
	12-13 13:13:47.422   289   916 D audio_hw_primary: enable_snd_device: snd_device(2: speaker)
	12-13 13:13:47.424   289   916 D audio_hw_primary: enable_audio_route: apply and update mixer path: low-latency-playback
	12-13 13:13:47.484   289   916 D AudioFlinger: mixer(0xb4140000) throttle end: throttle time(10)


* 请求服务，服务本身检查，将结果返回客户端检查

比如Camera的open函数   ，Camera connect ，可能就会直接崩溃
	
	 /** used by Camera#open, Camera#open(int) */
    Camera(int cameraId) {
        int err = cameraInitNormal(cameraId);
        if (checkInitErrors(err)) {
            switch(err) {
                case EACCESS:
                    throw new RuntimeException("Fail to connect to camera service");
                case ENODEV:
                    throw new RuntimeException("Camera initialization failed");
                default:
                    // Should never hit this.
                    throw new RuntimeException("Unknown camera error");
            }
        }
    }
    

	
	// connect to camera service
	static jint android_hardware_Camera_native_setup(JNIEnv *env, jobject thiz,
	    jobject weak_this, jint cameraId, jint halVersion, jstring clientPackageName)
	{
	    // Convert jstring to String16
	    const char16_t *rawClientName = reinterpret_cast<const char16_t*>(
	        env->GetStringChars(clientPackageName, NULL));
	    jsize rawClientNameLen = env->GetStringLength(clientPackageName);
	    String16 clientName(rawClientName, rawClientNameLen);
	    env->ReleaseStringChars(clientPackageName,
	                            reinterpret_cast<const jchar*>(rawClientName));
	
	    sp<Camera> camera;
	    if (halVersion == CAMERA_HAL_API_VERSION_NORMAL_CONNECT) {
	        // Default path: hal version is don't care, do normal camera connect.
	        camera = Camera::connect(cameraId, clientName,
	                Camera::USE_CALLING_UID);
	    } else {
	        jint status = Camera::connectLegacy(cameraId, halVersion, clientName,
	                Camera::USE_CALLING_UID, camera);
	        if (status != NO_ERROR) {
	            return status;
	        }
	    }
	
	    if (camera == NULL) {
	        return -EACCES;
	    }
	
	    // make sure camera hardware is alive
	    if (camera->getStatus() != NO_ERROR) {
	        return NO_INIT;
	    }
	
	    jclass clazz = env->GetObjectClass(thiz);
	    if (clazz == NULL) {
	        // This should never happen
	        jniThrowRuntimeException(env, "Can't find android/hardware/Camera");
	        return INVALID_OPERATION;
	    }
	
	    // We use a weak reference so the Camera object can be garbage collected.
	    // The reference is only used as a proxy for callbacks.
	    sp<JNICameraContext> context = new JNICameraContext(env, weak_this, clazz, camera);
	    context->incStrong((void*)android_hardware_Camera_native_setup);
	    camera->setListener(context);
	
	    // save context in opaque field
	    env->SetLongField(thiz, fields.context, (jlong)context.get());
	    return NO_ERROR;
	}	      
	
	
	
还是会检验权限？至于怎么检测，可以APP端，也可以服务端，看把控


	template <typename TCam, typename TCamTraits>
	sp<TCam> CameraBase<TCam, TCamTraits>::connect(int cameraId,
	                                               const String16& clientPackageName,
	                                               int clientUid)
	{
	    ALOGV("%s: connect", __FUNCTION__);
	    sp<TCam> c = new TCam(cameraId);
	    sp<TCamCallbacks> cl = c;
	    status_t status = NO_ERROR;
	    const sp<ICameraService>& cs = getCameraService();
	
	    if (cs != 0) {
	        TCamConnectService fnConnectService = TCamTraits::fnConnectService;
	        status = (cs.get()->*fnConnectService)(cl, cameraId, clientPackageName, clientUid,
	                                             /*out*/ c->mCamera);
	    }
	    if (status == OK && c->mCamera != 0) {
	        IInterface::asBinder(c->mCamera)->linkToDeath(c);
	        c->mStatus = NO_ERROR;
	    } else {
	        ALOGW("An error occurred while connecting to camera: %d", cameraId);
	        c.clear();
	    }
	    return c;
	}

	 status_t CameraService::onTransact(uint32_t code, const Parcel& data, Parcel* reply,
        uint32_t flags) {

    const int pid = getCallingPid();
    const int selfPid = getpid();

    // Permission checks
    switch (code) {
        case BnCameraService::CONNECT:
        case BnCameraService::CONNECT_DEVICE:
        case BnCameraService::CONNECT_LEGACY: {
            if (pid != selfPid) {
                // we're called from a different process, do the real check
                // 权限检测
                if (!checkCallingPermission(
                        String16("android.permission.CAMERA"))) {
                    const int uid = getCallingUid();
                    ALOGE("Permission Denial: "
                         "can't use the camera pid=%d, uid=%d", pid, uid);
                    return PERMISSION_DENIED;
                }
            }
            break;
        }
        
        
        
native层的PermissionService是个什么鬼


# 存储权限问题

