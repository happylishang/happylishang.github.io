![](https://rebooters.github.io/2020/01/04/Gradle-Transform-ASM-%E6%8E%A2%E7%B4%A2/transforms.png)

正确、高效的进行文件目录、jar 文件的解压、class 文件 IO 流的处理，保证在这个过程中不丢失文件和错误的写入
高效的找到要插桩的结点，过滤掉无效的 class
支持增量编译



参考文档

https://rebooters.github.io/2020/01/04/Gradle-Transform-ASM-%E6%8E%A2%E7%B4%A2/


> Task :app:transformClassesWithTinkerAppInfoTransformForDebug
 src des ImmutableDirectoryInput{name=7f36b517340e16432edb0d229a83b768aa4febc1, 
 
 
 file=/Users/personal/prj/yanxuan/YanXuan/app/build/intermediates/transforms/AutoRegisterTransformer/debug/331, contentTypes=CLASSES, scopes=PROJECT, changedFiles={}} -- 
 
  /Users/personal/prj/yanxuan/YanXuan/app/build/intermediates/transforms/TinkerAppInfoTransform/debug/1661


### 自定义插件名称


### 发布到mavenCenter

发布aar与发布jar配置不同

参考文档 https://juejin.cn/post/6932485276124233735


If buildscript itself needs something to run, use classpath.

If your project needs something to run, use compile.

The compile configuration is created by the Java plugin. The classpath configuration is commonly seen in the buildScript {} block where one needs to declare dependencies for the build.gradle, itself (for plugins, perhaps).

If your build script needs to use external libraries, you can add them to the script’s classpath in the build script itself. You do this using the buildscript() method, passing in a closure which declares the build script classpath.

This is the same way you declare, for example, the Java compilation classpath. You can use any of the dependency types described in Dependency Types, except project dependencies.

Having declared the build script classpath, you can use the classes in your build script as you would any other classes on the classpath.


### 插件调试

https://blog.csdn.net/ZYJWR/article/details/113129586

https://juejin.cn/post/6948626628637360135