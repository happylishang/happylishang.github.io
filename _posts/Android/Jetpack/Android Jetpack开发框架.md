# Jetpack就是个开发框架，或者说是堆积起来的一堆库，

![jetpack_donut.png](https://upload-images.jianshu.io/upload_images/1460468-c5bf67043ef1b721.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

#  LifeCircles+LiveData + ViewModel+DataBinding

##   Lifecycle

初衷：让组件感受到View界面的声明周期（Activity 、Fragment等）

*  LifecycleOwner
*  LifecycleObserver

lifecycle-aware components

	public class SupportActivity extends Activity implements LifecycleOwner {
	
	    private LifecycleRegistry mLifecycleRegistry = new LifecycleRegistry(this);
	...
	
	    @Override
    public Lifecycle getLifecycle() {
        return mLifecycleRegistry;
    }
    
	
	public interface LifecycleOwner {
    /**
     * Returns the Lifecycle of the provider.
     *
     * @return The lifecycle of the provider.
     */
    @NonNull
    Lifecycle getLifecycle();
	}

	public class Fragment implements ComponentCallbacks, OnCreateContextMenuListener, LifecycleOwner,


add之后，就能够得到通知，跟我们代码中的presenter使用类似，这个也就是一个封装，其实我们底层封装好，效果也是一样的。


实例化一个ReportFragment
    
        @Override
    @SuppressWarnings("RestrictedApi")
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ReportFragment.injectIfNeededIn(this);
    }
    
    public class ReportFragment extends Fragment {
    private static final String REPORT_FRAGMENT_TAG = "android.arch.lifecycle"
            + ".LifecycleDispatcher.report_fragment_tag";

    public static void injectIfNeededIn(Activity activity) {
        // ProcessLifecycleOwner should always correctly work and some activities may not extend
        // FragmentActivity from support lib, so we use framework fragments for activities
        android.app.FragmentManager manager = activity.getFragmentManager();
        if (manager.findFragmentByTag(REPORT_FRAGMENT_TAG) == null) {
            manager.beginTransaction().add(new ReportFragment(), REPORT_FRAGMENT_TAG).commit();
            // Hopefully, we are the first to make a transaction.
            manager.executePendingTransactions();
        }
    }


编译时候，注入一个provider，而且每个进程一个，就是辅助注册回调

	<provider
	        android:name="android.arch.lifecycle.LifecycleRuntimeTrojanProvider"
	        android:authorities="${applicationId}.lifecycle-trojan"
	        android:exported="false"
	        android:multiprocess="true" />

 
## LiveData

Live+Data

>LiveData is an observable data holder class. Unlike a regular observable, LiveData is lifecycle-aware, meaning it respects the lifecycle of other app components, such as activities, fragments, or services. This awareness ensures LiveData only updates app component observers that are in an active lifecycle state.


LiveData想要跟谁谁的声明周期，就只需要observe那个对象就可以，其实就是往具有生命周期的控件中添加一个观察者。

    @MainThread
    public void observe(@NonNull LifecycleOwner owner, @NonNull Observer<T> observer) {
        if (owner.getLifecycle().getCurrentState() == DESTROYED) {
            // ignore
            return;
        }
        LifecycleBoundObserver wrapper = new LifecycleBoundObserver(owner, observer);
        ...
        owner.getLifecycle().addObserver(wrapper);
    }
    
数据可以被观察者订阅，能够感知组件（Fragment、Activity、Service）的生命周期，在resume之后，数据可以被重新发送，并通知，有个暂存机制，用来处理FragmentDialog的弹出挺合适，好像也没多大用，数据驱动UI，这里是全局刷新吗？ 如果不是要嵌套跟我们目前的详情有些类似、

## ViewModel

data的集合就是model，针对特定UI，ViewModel是单利的，

Activity Fragment等

因此，可以很好的处理数据共享，但是Activity不借助这个，也可以处理，就是要自己添加回调

这个框架的特点就是，帮助你全局封装好回调，当然，我们在业务初期，可以在底层自己封装好，jetpack的好处是将回调隐形的封装好，代码统一，这个还比较有用，比较麻烦：我们的Model都是Json格式，如何跟ViewModel对应，也就是数据驱动UI的更新？databinding ？ 但是网络获取的数据一般是固定不变的，似乎没必要时时的数据驱动UI，ViewModel的生命周期长，不要让ViewModel持有Activity或者Fragment，会导致内存泄漏。后端返回的VO适合用作一个字段，但是如何全局规划呢？

场景：同一个Activity中有多个Fragment，每个Fragment中的操作都会影响Activity UI，这个时候Fragment复用Activity的ViewModel就非常合适。



## room   

jetpack的这一套，要全部配合起来使用，才能说效率提升。比如room配合livedata，配合注解


	@Dao
	interface GardenPlantingDao {
	    @Query("SELECT * FROM garden_plantings")
	    fun getGardenPlantings(): LiveData<List<GardenPlanting>>
	
	    @Query("SELECT * FROM garden_plantings WHERE id = :gardenPlantingId")
	    fun getGardenPlanting(gardenPlantingId: Long): LiveData<GardenPlanting>
	
	    @Query("SELECT * FROM garden_plantings WHERE plant_id = :plantId")
	    fun getGardenPlantingForPlant(plantId: String): LiveData<GardenPlanting>
	
	    /**
	     * This query will tell Room to query both the [Plant] and [GardenPlanting] tables and handle
	     * the object mapping.
	     */
	    @Transaction
	    @Query("SELECT * FROM plants")
	    fun getPlantAndGardenPlantings(): LiveData<List<PlantAndGardenPlantings>>
	
	    @Insert
	    fun insertGardenPlanting(gardenPlanting: GardenPlanting): Long
	}


## databinding

在布局中让View跟model中的数据绑定，代码中注入model，databinding用起来感觉挺乱，


## navigationg+paging是新的UI组件

就业务需求上来说，并没有必要仰慕这个空间，更新迭代的速度太快。

## workmanager  处理异步（自己封装的好也没必要非要用这个）