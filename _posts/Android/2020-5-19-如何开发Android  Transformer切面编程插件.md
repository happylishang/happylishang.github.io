---
layout: post
title: "如何开发Android  Transformer切面编程插件"
description: "Java"
category: Android

---

* 工具Android Studio
* 语言 kotlin
* 知识点 ASM编程 Or javaAssit

## 第一步 ：在线调试版本buildSrc

开发过程，基于Android Studio工具，可以直接通过buildSrc实现一套本地编程版的插件，目录如下

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/1810fe79081a4702bc27333439eeb206~tplv-k3u1fbpfcp-watermark.image)

这种情况下，修改，调试比较方便。推荐使用kotlin+asm，javaassist有坑，尤其在window上



## 第二步 ：可发布版本，buildSrc实现完毕，就可以编写发布版本



![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/37556deab6914db5a871cc27da87bd93~tplv-k3u1fbpfcp-watermark.image)

基本上拷贝第一步的代码，让后upload即可

## 核心的ASM处理代码范例


jar文件修改流程如下\

* 读取原jar
* ASM修改Class
* 写到新jar
* 删除旧jar

核心代码参看如下，很少需要修改代码

    fun asmInsertMethod(originFile: File?) {

        val optJar = File(originFile?.parent, originFile?.name + ".opt")
        if (optJar.exists())
            optJar.delete()
        val jarFile = JarFile(originFile)
        val enumeration = jarFile.entries()
        val jarOutputStream = JarOutputStream(FileOutputStream(optJar))

        while (enumeration.hasMoreElements()) {
            val jarEntry = enumeration.nextElement()
            val entryName = jarEntry.getName()
            val zipEntry = ZipEntry(entryName)
            val inputStream = jarFile.getInputStream(jarEntry)
            //插桩class
            if (entryName.endsWith("xxx.class")) {
                //class文件处理
                jarOutputStream.putNextEntry(zipEntry)
                val classReader = ClassReader(IOUtils.toByteArray(inputStream))
                val classWriter = ClassWriter(classReader, ClassWriter.COMPUTE_MAXS)
                val cv = RegisterClassVisitor(Opcodes.ASM5, classWriter,tableList)
                classReader.accept(cv, EXPAND_FRAMES)
                val code = classWriter.toByteArray()
                jarOutputStream.write(code)
            } else {
                jarOutputStream.putNextEntry(zipEntry)
                jarOutputStream.write(IOUtils.toByteArray(inputStream))
            }
            jarOutputStream.closeEntry()
        }
        //结束
        jarOutputStream.close()
        jarFile.close()
        if (originFile?.exists() == true) {
            Files.delete(originFile.toPath())
        }
        optJar.renameTo(originFile)
    }
      
      
 RegisterClassVisitor继承ClassVisitor，是Class修改的核心，如果要修改方法，则继续继承MethodVisitor，在classvisit中找到对应方法，然后修改，
 
         override fun visitCode() {

            tablists?.forEach {

                System.out.println("RegisterClassVisitor : visitMethod : " +  it.first );

                mv.visitMethodInsn(
                        Opcodes.INVOKESTATIC,
                        it.first,
                        "pageRouterGroup",
                        "()Ljava/util/Map;",
                        false
                )

                mv.visitMethodInsn(
                        Opcodes.INVOKESTATIC,
                        it.first,
                        "methodRouters",
                        "()Ljava/util/List;",
                        false
                )

                mv.visitMethodInsn(
                        Opcodes.INVOKESTATIC,
                        it.first,
                        "interceptors",
                        "()Ljava/util/List;",
                        false
                )

                mv.visitMethodInsn(
                        Opcodes.INVOKESTATIC,
                        "com/netease/hearttouch/router/HTRouterManager",
                        "init",
                        "(Ljava/util/Map;Ljava/util/List;Ljava/util/List;)V",
                        false
                )

                super.visitCode()

            }
        }

        override fun visitInsn(opcode: Int) {
            super.visitInsn(opcode)
        }
        
  其中类似     mv.visitMethodInsn(XXX）中的代码是不需要自己编码的，依赖ASM ByteCode类的插件，反编译class文件即可        
  
  ![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7865ee65c1114c429b6cb4683db127d2~tplv-k3u1fbpfcp-watermark.image)
  
 然后，照葫芦画瓢填充代码即可。
  
  