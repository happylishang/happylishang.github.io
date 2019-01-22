> 换肤的本质：动态更新UI资源

目前基本都是AppcompatActivity

    
![image.png](https://upload-images.jianshu.io/upload_images/1460468-b72993685e23d28b.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)   
   
![image.png](https://upload-images.jianshu.io/upload_images/1460468-cdc8872a0d4363c4.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

最终调用的其实都是setFactory2

![image.png](https://upload-images.jianshu.io/upload_images/1460468-1025e70bf28396b7.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

真正的实现类是Factory2Wrapper


![image.png](https://upload-images.jianshu.io/upload_images/1460468-08cbd8413b8535ab.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

AppCompatDelegateImplV7实现了LayoutInflaterFactory接口，setFactory2传递的参数是AppCompatDelegateImplV7自己：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-f1cefd131dd64483.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


![image.png](https://upload-images.jianshu.io/upload_images/1460468-81fbae2a70c6704a.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

![image.png](https://upload-images.jianshu.io/upload_images/1460468-8c62a964e8223388.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到，Google现在把很多逻辑都留给了兼容库，这样方便更新一些新的特性，这样是为什么推荐直接用AppCompatEditText，AppCompatTextView，因为就算你不写，底层也会帮你转成相应的AppCompatXXX。

# Application提供Activity的LifeCycle回调

加载资源后，统一apply，加载之前的先apply一遍，无效，加载后的，apply立刻生效，

不用非得重启生效，第一次下载后，直接apply也可以生效，只不过可能有个变化而已。

为什么Factory2不能合理创建View，它只能创建一些AppCompat类的基础View，其他的不行，所以经常null，最后调用的还是Layoutinflate的createViewFromTag，所以对于自定义的View，如果想要Hook，并达到换肤的目的，需要重写部分，Factory2就不是换肤的关键了，它只是一个Hook点，提供还如的关键入口与拦截，，Factory2主要