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