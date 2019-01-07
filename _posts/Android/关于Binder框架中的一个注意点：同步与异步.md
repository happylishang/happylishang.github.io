interface IContentObserver
{
    /**
     * This method is called when an update occurs to the cursor that is being
     * observed. selfUpdate is true if the update was caused by a call to
     * commit on the cursor that is being observed.
     */
     contentService 用的是oneway
    oneway void onChange(boolean selfUpdate, in Uri uri, int userId);
}


Binder.clearCallingIdentity();


 Binder.getCallingPid()为什么会是0呢？因为本地Service中使用的时候，不牵扯到调用跟新CallId的机制