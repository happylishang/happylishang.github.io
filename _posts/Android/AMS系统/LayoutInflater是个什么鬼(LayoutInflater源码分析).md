---
layout: default
title: LayoutInflater是个什么鬼
image: http://upload-images.jianshu.io/upload_images/1460468-7f7ec81bf66a8680.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240

---


LayoutInflater其实是一个布局渲染工具，其本质就只是一个工具，说白了**LayoutInflater的作用就是根据xml布局文件构建View树**，自定义View的时候经常用到，常用的做法如下：

        View tmpView= LayoutInflater.from(context).inflate(R.layout.content,container,false);

首先通过LayoutInflater.from静态函数获得一个LayoutInflater实例，其实是是个PhoneLayoutInflater对象，跟踪源码看一下：

    public static LayoutInflater from(Context context) {
        LayoutInflater LayoutInflater =
                (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        if (LayoutInflater == null) {
            throw new AssertionError("LayoutInflater not found.");
        }
        return LayoutInflater;
    }
    

# LayoutInflater服务是什么
    
这里的context.getSystemService可以直接去ContextImpl中找，其中，LAYOUT_INFLATER_SERVICE服务跟AMS、WMS等服务不同，它完全是APP端自己虚拟的一个服务，主要作用是：在本地，为调用者创建PhoneLayoutInflater工具对象，ContextImpl在注册这个“服务”的时候，将工作委托给PolicyManager，利用其makeNewLayoutInflater构建LayoutInflater

    registerService(LAYOUT_INFLATER_SERVICE, new ServiceFetcher() {
        public Object createService(ContextImpl ctx) {
            return PolicyManager.makeNewLayoutInflater(ctx.getOuterContext());
        }});
        
    public static LayoutInflater makeNewLayoutInflater(Context context) {
        return sPolicy.makeNewLayoutInflater(context);
    }        

而PolicyManager进一步调用com.android.internal.policy.impl.Policy对象的makeNewLayoutInflater构建PhoneLayoutInflater。

    private static final String POLICY_IMPL_CLASS_NAME =
    "com.android.internal.policy.impl.Policy";
    
    public LayoutInflater makeNewLayoutInflater(Context context) {
        return new PhoneLayoutInflater(context);
    }   
    
也就是说，这里获取的服务严格来说其实就是一个本地工具对象PhoneLayoutInflater，接下来看看，这个PhoneLayoutInflater如何创建View树的呢？

![获取inflate服务.png](http://upload-images.jianshu.io/upload_images/1460468-7f7ec81bf66a8680.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# LayoutInflater如何构建View树

先从直观理解一下LayoutInflater的工作原理，LayoutInflater如何根据布局文件的id构建View树呢？有以下几个方面

*  解析xml获取xml信息（应该有缓存，因为这些数据是静态不变的）
* 根据xml中的tag标签逐层构建View （通过反射创建View）
* 递归构建其中的子View，并将子View添加到父ViewGroup中

LayoutInflater源码中也确实是按照上面的流程来构建View的，只是添加了些特殊标签的处理逻辑，比如merge、include、stubview等，下面简单跟踪下源码：

    public View inflate(int resource, ViewGroup root, boolean attachToRoot) {
        XmlResourceParser parser = getContext().getResources().getLayout(resource);
        try {
            return inflate(parser, root, attachToRoot);
        } finally {
            parser.close();
        }
    }    
    
XmlResourceParser其实就包含了xml文件信息的一个对象，之后通过XmlResourceParser将tag的信息取出，递归创建View，具体XmlResourceParser对象的创建如下，

    public XmlResourceParser getLayout(int id) throws NotFoundException {
        return loadXmlResourceParser(id, "layout");
    }   
    
注意这里解析的xml文件是layout，
    
      XmlResourceParser loadXmlResourceParser(int id, String type)
            throws NotFoundException {
        synchronized (mAccessLock) {
            TypedValue value = mTmpValue;
            <!--获取一个TypedValue-->
            if (value == null) {
                mTmpValue = value = new TypedValue();
            }
            <!--利用id 查询layout，并填充TypedValue-->
            getValue(id, value, true);
            <!--根据布局文件的路径，返回解析xml文件-->
            if (value.type == TypedValue.TYPE_STRING) {
                return loadXmlResourceParser(value.string.toString(), id,
                        value.assetCookie, type);
            }
        }
    }    
    
TypedValue是与xml定义的资源对应的值，xml是固定的，非动态的，因此只需要一份，所以可以有缓存机制，看一下getValue如何获取对应xml资源：

    public void getValue(int id, TypedValue outValue, boolean resolveRefs)
            throws NotFoundException {
        boolean found = mAssets.getResourceValue(id, 0, outValue, resolveRefs);
    }
     
这里牵扯到Android的资源管理内容，mAssets是一个AssetManager对象，
 
    final boolean getResourceValue(int ident,int density, TypedValue outValue, boolean resolveRefs) {
       <!--加载资源-->
        int block = loadResourceValue(ident, (short) density, outValue, resolveRefs);
        if (block >= 0) {
            if (outValue.type != TypedValue.TYPE_STRING) {
                return true;
            }
            outValue.string = mStringBlocks[block].get(outValue.data);
            return true; }  return false;  }

AssetManager进而会通过native函数加载xml文件信息，
	
	static jint android_content_AssetManager_loadResourceValue(JNIEnv* env, jobject clazz, jint ident,jshort density,jobject outValue,jboolean resolve){
	    ...<!--获取native AssetManager对象-->
	    AssetManager* am = assetManagerForJavaObject(env, clazz);
	    <!--获取ResTable资源表，这里应该有缓存 不能每次都弄一次吧？ 所有资源的唯一表吗？-->
	    const ResTable& res(am->getResources());
	    Res_value value;
	    ResTable_config config;
	    uint32_t typeSpecFlags;
	    <!--通过ResTable获取资源-->
	    ssize_t block = res.getResource(ident, &value, false, density, &typeSpecFlags, &config);
       ...
	    uint32_t ref = ident;
	    if (resolve) {
	    <!--是否需要二次解析资源-->
	        block = res.resolveReference(&value, block, &ref, &typeSpecFlags, &config);
        ...
	    }
	    return block >= 0 ? copyValue(env, outValue, &res, value, ref, block, typeSpecFlags, &config) : block;
	}

以上代码就是如何获取资源的， 其中res.getResource并不是是每次都加载一遍，第一次加载后就能获得单利ResTable，后面用的都是这个缓存，只不过ResTable不会缓存全部资源，对于布局、图像资源等，缓存的都是引用，所以，如果是真实资源的引用话，还需要通过res.resolveReference来解析真正的资源。资源加载不是这里重点，重点是LayoutInflater如何创建View树，只简单看一下资源加载：
	
	const ResTable* AssetManager::getResTable(bool required) const{
	
		<!--缓存 ResTable，如果非空直接返回-->
	    ResTable* rt = mResources;
	    if (rt) {  return rt;   }
       ...<!--多个apk的话，会有多个-->
	    const size_t N = mAssetPaths.size();
	    for (size_t i=0; i<N; i++) {
	        Asset* ass = NULL;
	        ResTable* sharedRes = NULL;
	        bool shared = true;
	        <!--找到Asset的路径-->
	        const asset_path& ap = mAssetPaths.itemAt(i);
	        Asset* idmap = openIdmapLocked(ap);
	        <!--这里的路径一般都不是目录-->
	        if (ap.type != kFileTypeDirectory) {   
	        		if (i == 0) {
	            	  <!--第一个一般是框架层的系统资源，用的较多，不想每次都解析，需要缓存-->
	                sharedRes = const_cast<AssetManager*>(this)->mZipSet.getZipResourceTable(ap.path);
	            }
	            if (sharedRes == NULL) {
	                ass = const_cast<AssetManager*>(this)->mZipSet.getZipResourceTableAsset(ap.path);
	                if (ass == NULL) {
	                <!--打开resources.arsc文件-->
	                    ass = const_cast<AssetManager*>(this)->openNonAssetInPathLocked("resources.arsc",  Asset::ACCESS_BUFFER,  ap);
	                    if (ass != NULL && ass != kExcludedAsset) {
	                        ass = const_cast<AssetManager*>(this)->mZipSet.setZipResourceTableAsset(ap.path, ass);
	                    }}
	                if (i == 0 && ass != NULL) {
	                    <!--缓存第一个asset-->
	                    sharedRes = new ResTable();
	                    sharedRes->add(ass, (void*)(i+1), false, idmap);
	                    sharedRes = const_cast<AssetManager*>(this)->mZipSet.setZipResourceTable(ap.path, sharedRes);
	                } } } 
	        ...	        
	        if ((ass != NULL || sharedRes != NULL) && ass != kExcludedAsset) {
	            if (rt == NULL) {
	                mResources = rt = new ResTable();
	                updateResourceParamsLocked();
	            }
	            if (sharedRes != NULL) {
	                rt->add(sharedRes);
	            } else {
	                rt->add(ass, (void*)(i+1), !shared, idmap);
	            }  }  .. }
	    return rt;
	}

简而言之：通过上面的操作，完成了resources.arsc文件的解析，获得了一个**ResTable**对象，该对象包含了应用程序的全部资源信息（动态加载的先不考虑），之后，就可以通过ResTable的getResource来获得指定资源，而对于xml布局文件，这里获得的就是一个引用，需要res.resolveReference二次解析，之后就得到了id对应的资源项。这里的xml布局文件对应的资源项的值是一个字符串，其实是一个布局文件路径，它指向一个经过编译的二进制格式保存的Xml资源文件。有了这个Xml资源文件的路径之后，会再次通过loadXmlResourceParser来对该Xml资源文件进行解析，从而得到布局文件解析对象XmlResourceParser。

       XmlResourceParser loadXmlResourceParser(String file, int id,
            int assetCookie, String type) throws NotFoundException {
        if (id != 0) {
            try {...
            		  <!--解析xml文件-->
                    XmlBlock block = mAssets.openXmlBlockAsset(assetCookie, file);
                    if (block != null) {
                        int pos = mLastCachedXmlBlockIndex+1;
                        if (pos >= num) pos = 0;
                        mLastCachedXmlBlockIndex = pos;
                        XmlBlock oldBlock = mCachedXmlBlocks[pos];
                        if (oldBlock != null) {
                            oldBlock.close();
                        }
                        <!--缓存-->
                        mCachedXmlBlockIds[pos] = id;
                        mCachedXmlBlocks[pos] = block;
                        <!--返回-->
                        return block.newParser();
             ...
            
 
通过上一步，返回一个 XmlResourceParser对象，对外而言，XmlResourceParser是这样一个对象：它包含解析后xml布局信息，通过它，可以获得xml中各种标签的信息，甚至你可以简化的看做是一个包含xml格式字符串的缓存对象。到这里，就获取了XmlResourceParser ，也可以说，到这里就知道了id对应的xml文件到底包含了什么View，那么下一步就是根据这份缓存来实例化各种View:
            
    public View inflate(XmlPullParser parser, ViewGroup root, boolean attachToRoot) {
        synchronized (mConstructorArgs) {
            final AttributeSet attrs = Xml.asAttributeSet(parser);
            Context lastContext = (Context)mConstructorArgs[0];
            mConstructorArgs[0] = mContext;
            View result = root;
            try {
                int type;
                final String name = parser.getName();
                <!--Merge标签的根布局不能直接用LayoutInflater进行inflate-->
                if (TAG_MERGE.equals(name)) {
                    if (root == null || !attachToRoot) {
                        throw new InflateException("<merge /> can be used only with a valid "
                                + "ViewGroup root and attachToRoot=true");
                    }
                   rInflate(parser, root, attrs, false);
                } else {
                    View temp;
                    if (TAG_1995.equals(name)) {
                        temp = new BlinkLayout(mContext, attrs);
                    } else {
                    <!--利用tag创建View-->
                        temp = createViewFromTag(root, name, attrs);
                    }
                    ViewGroup.LayoutParams params = null;
                    if (root != null) {
                        <!--是否有container来辅助，或者添加到container中，或者辅助生成布局参数-->
                        params = root.generateLayoutParams(attrs);
                        if (!attachToRoot) {
                            temp.setLayoutParams(params);
                        }
                    }
                    <!--如果有必要，递归生成子View，并添加到temp容器中-->
                    rInflate(parser, temp, attrs, true);
						<!--是否需要添加到root的container容器总-->
                    if (root != null && attachToRoot) {
                        root.addView(temp, params);
                    }
                    <!--如果不添加root中，返回结果就是infate出的根布局View，否则就是root根布局-->
                    if (root == null || !attachToRoot) {
                        result = temp;
                    }
                }

            } ...
            return result; 
            }}
   
inflate的主要作用是生成layout的跟布局文件，并且根据参数看看是否需要添加container容器中，之后根据需求调用rInflate递归生成子View。

	    void rInflate(XmlPullParser parser, View parent, final AttributeSet attrs,
	            boolean finishInflate) throws XmlPullParserException, IOException {
	        final int depth = parser.getDepth();
	        int type;
	        <!--递归解析-->
	        while (((type = parser.next()) != XmlPullParser.END_TAG ||
	                parser.getDepth() > depth) && type != XmlPullParser.END_DOCUMENT) {
	            if (type != XmlPullParser.START_TAG) {
	                continue;
	            }
	            final String name = parser.getName();
	            if (TAG_REQUEST_FOCUS.equals(name)) {
	                parseRequestFocus(parser, parent);
	            } else if (TAG_INCLUDE.equals(name)) {
	                // inclue标签，不能用在getDepth() == 0
	                if (parser.getDepth() == 0) {
	                    throw new InflateException("<include /> cannot be the root element");
	                }
	                parseInclude(parser, parent, attrs);
	            } else if (TAG_MERGE.equals(name)) {
	               <!--merge标签必须是布局的根元素，因此merge使用方式一定是被inclue-->
	                throw new InflateException("<merge /> must be the root element");
	            } else if (TAG_1995.equals(name)) {
	                final View view = new BlinkLayout(mContext, attrs);
	                final ViewGroup viewGroup = (ViewGroup) parent;
	                final ViewGroup.LayoutParams params = viewGroup.generateLayoutParams(attrs);
	                rInflate(parser, view, attrs, true);
	                viewGroup.addView(view, params);                
	            } else {
	                <!--创建View，如果有必要，接着递归-->
	                final View view = createViewFromTag(parent, name, attrs);
	                final ViewGroup viewGroup = (ViewGroup) parent;
	                final ViewGroup.LayoutParams params = viewGroup.generateLayoutParams(attrs);
	                rInflate(parser, view, attrs, true);
	                <!--添加View-->
	                viewGroup.addView(view, params);
	            }
	        }
	        if (finishInflate) parent.onFinishInflate();
	    }
    
rInflate主要作用是开启递归遍历，生成View树，createViewFromTag的主要作用是利用反射生成View对象，以上就是LayoutInflater的简易分析。
        
# 总结

LayoutInflater其实就是一个**工具类**，虽然是通过服务方式获取的PhoneLayoutInflater对象，但是它本身算不上服务，也不会牵扯到Binder通信。**LayoutInflater的主要作用就是根据xml文件，通过反射的方式，递归生成View树**。