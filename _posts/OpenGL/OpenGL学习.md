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

* Local space (or Object space) 本地坐标，（原生坐标系）
* World space						世界左边，在世界中的样子，三维就是三维
* View space (or Eye space)	摄像机左边，应该是平行光投影
* Clip space						透视坐标，应该是透视投影
* Screen space

Those are all a different state at which our vertices will be transformed in before finally ending up as fragments.

![](https://learnopengl.com/img/getting-started/coordinate_systems.png)

# 参考文档

[](https://blog.csdn.net/lyx2007825/article/details/8792475)      
[Coordinate Systems ](https://learnopengl.com/Getting-started/Coordinate-Systems)