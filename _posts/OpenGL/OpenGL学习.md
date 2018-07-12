OpenGl学习

# 概念

OpenGL是一个标准API接口，实现由具体的厂商来负责，OpenGL规范严格规定了每个函数该如何执行，以及它们的输出值。至于内部具体每个函数是如何实现(Implement)的，将由OpenGL库的开发者自行决定（注：这里开发者是指编写OpenGL库的人）。

# 运行机制

OpenGL自身是一个巨大的状态机(State Machine)：一系列的变量描述OpenGL此刻应当如何运行。OpenGL的状态通常被称为OpenGL上下文(Context)。我们通常使用如下途径去更改OpenGL状态：设置选项，操作缓冲。最后，我们使用当前OpenGL上下文来渲染。**OpenGL本质上是个大状态机**

Graphics Pipeline，大多译为管线，实际上指的是一堆原始图形数据途经一个输送管道，期间经过各种变化处理最终出现在屏幕的过程，图形渲染管线接受一组3D坐标，然后把它们转变为你屏幕上的有色2D像素输出。图形渲染管线可以被划分为几个阶段，每个阶段将会把前一个阶段的输出作为输入。所有这些阶段都是高度专门化的（它们都有一个特定的函数），并且很容易并行执行。正是由于它们具有并行执行的特性，当今大多数显卡都有成千上万的小处理核心，它们在GPU上为每一个（渲染管线）阶段运行各自的小程序，从而在图形渲染管线中快速处理你的数据。这些小程序叫做着色器(Shader)。

有些着色器允许开发者自己配置，这就允许我们用自己写的着色器来替换默认的。这样我们就可以更细致地控制图形渲染管线中的特定部分了，而且因为它们运行在GPU上，所以它们可以给我们节约宝贵的CPU时间。OpenGL着色器是用OpenGL着色器语言(OpenGL Shading Language, GLSL)写成的，在下一节中我们再花更多时间研究它。

# GLSL

# Android OpenGL 入门

伴随着GLSurfaceView的使用及学习

EGLContext如何创建EGLImpl

	public abstract class EGLContext
	{
	    private static final EGL EGL_INSTANCE = new com.google.android.gles_jni.EGLImpl();
	    
	    public static EGL getEGL() {
	        return EGL_INSTANCE;
	    }
	
	    public abstract GL getGL();
	}


# OpenGL渲染文字（纹理）

其实先生成一个纹理，之后利用纹理处理，FreeType要做的事就是加载TrueType字体并为每一个字形生成位图和几个度量值。我们可以取出它生成的位图作为字形的纹理，将这些度量值用作字形纹理的位置、偏移等描述。 

找到对应的纹理左边，贴图合成即可。

      单位矩阵就是对角线上都是1，其余元素皆为0的矩阵。



当您调用glLoadIdentity()之后，您实际上将当前点移到了屏幕中心：类似于一个复位操作

*       1.X坐标轴从左至右，Y坐标轴从下至上，Z坐标轴从里至外。
*       2.OpenGL屏幕中心的坐标值是X和Y轴上的0.0f点。
*       3. 中心左面的坐标值是负值，右面是正值。
           移向屏幕顶端是正值，移向屏幕底端是负值。
           移入屏幕深处是负值，移出屏幕则是正值。
           
           
gl. glMatrixMode(GL10.GL_PROJECTION);用来指定操作矩阵，有多个变换矩阵，矩阵要Union，图形 * 世界 * 视图
           
应当说明的是，用一个单位矩阵来替换当前矩阵的做法并非在任何场合下都可以使用。例如，已经进行了3次矩阵变换，而现在打算将当前矩阵恢复到第二次变换后的状态时，该方法将失效。此时可用glPushMatrix()命令将每次变换前的矩阵压入矩阵堆栈，在进行完新矩阵中的各种操作后，再利用glPopMatrix()命令将栈顶的矩阵弹出矩阵堆栈，成为当前矩阵。

![](http://www.songho.ca/opengl/files/gl_transform02.png)

OpenGL操作的编程矩阵，并不是点本身，而是其变换矩阵。

# OpenGL坐标系

* Local space (or Object space) 本地坐标，（原坐标系,仅仅对自己有意义，比如一个球，两个球，都有自己的000，001，点面）

>Local space is the coordinate space that is local to your object, i.e. where your object begins in. Imagine that you've created your cube in a modeling software package (like Blender). The origin of your cube is probably at (0,0,0) even though your cube might end up at a different location in your final application. Probably all the models you've created all have (0,0,0) as their initial position. All the vertices of your model are therefore in local space: they are all local to your object.


* World space						世界左边，在世界中的样子，三维就是三维 （两个球，分别的相对位置，世界坐标系，相对世界的000点） 将球放到世界中，进入三维空间

>	The model matrix is a transformation matrix that translates, scales and/or rotates your object to place it in the world at a location/orientation they belong to. Think of it as transforming a house by scaling it down (it was a bit too large in local space), translating it to a suburbia town and rotating it a bit to the left on the y-axis so that it neatly fits with the neighboring houses. You could think of the matrix in the previous tutorial to position the container all over the scene as a sort of model matrix as well; we transformed the local coordinates of the container to some different place in the scene/world.

* View space (or Eye space)	摄像机(眼镜)，  就是从不同的视角看过去，但是这里没有牵扯到透射的概念

>The view space is what people usually refer to as the camera of OpenGL (it is sometimes also known as the camera space or eye space). The view space is the result of transforming your world-space coordinates to coordinates that are in front of the user's view.


* Clip space						裁剪

>At the end of each vertex shader run, OpenGL expects the coordinates to be within a specific range and any coordinate that falls outside this range is clipped. Coordinates that are clipped are discarded, so the remaining coordinates will end up as fragments visible on your screen. This is also where clip space gets its name from.


* Screen space

Those are all a different state at which our vertices will be transformed in before finally ending up as fragments.

![](https://learnopengl.com/img/getting-started/coordinate_systems.png)


	Vclip=Mprojection⋅Mview⋅Mmodel⋅Vlocal
	
Camera坐标系还是平行的，没有经过投射处理，机器没有肉眼的概念，Camera其实就是换个角度的问题，而坐标系依然是平行坐标系，没有近大远小的问题：

![屏幕快照 2018-07-11 下午3.31.42.png](https://upload-images.jianshu.io/upload_images/1460468-1de606405c48f70c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# OpenGL文字渲染

也就是说，当直接或间接调用Canvas.drawText()函数的时候，OpenGL 渲染器不会收到你发送的参数，而是收到一串数字、符号标识，还有x/y 坐标集合。



# 参考文档

[](https://blog.csdn.net/lyx2007825/article/details/8792475)      
[Coordinate Systems ](https://learnopengl.com/Getting-started/Coordinate-Systems)


View视图坐标系的理解，或者说Camera坐标系

其实就是个坐标系旋转：不牵扯到透射，为什么这么说，坐标系及坐标本身并没发生近大远小，依旧是平行的坐标系，只是改变了位置跟角度，比如


世界坐标系中的（0，0，0），转换到视图坐标系后是（a,b,c）,世界坐标系中（1，1，1）转换到视图坐标系周是（a+1,b+1,c+1）相对位置不变