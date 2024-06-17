![](https://rebooters.github.io/2020/01/04/Gradle-Transform-ASM-%E6%8E%A2%E7%B4%A2/transforms.png)

正确、高效的进行文件目录、jar 文件的解压、class 文件 IO 流的处理，保证在这个过程中不丢失文件和错误的写入
高效的找到要插桩的结点，过滤掉无效的 class
支持增量编译

插件配置

创建Java或者kotlin的库 插件库配置

	plugins {
	    id 'java-library'
	//    版本要一致，而且必须明确声明，为什么呢？
	    id 'org.jetbrains.kotlin.jvm'
	    //   每个插件实现都要加入如下配置
	    id 'groovy'
	    id 'maven-publish'
	//    id 'java-gradle-plugin'
	}
	
	repositories {
	    google()
	    mavenCentral()
	
	}
	dependencies {
	    implementation gradleApi()
       implementation  'io.github.happylishang:abstracttransform:1.0.4'
		 
		api("com.android.tools.build:gradle-api:7.0.4")
		api("com.android.tools.build:gradle:7.0.4")
		api 'org.ow2.asm:asm:9.2'
		api 'org.ow2.asm:asm-commons:9.1'
		api 'commons-io:commons-io:2.6'
		api 'commons-codec:commons-codec:1.11'
	}
	

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


apply plugin: 'maven-publish'
apply plugin: 'signing'

//每次修改这个文件即可 替换PUBLISH_ARTIFACT_ID即可

//每次publish之后有延迟


		task sourcesJar(type: Jar) {
		    classifier = 'sources'
		    from sourceSets.main.getAllSource()
		}
		task javadocJar(type: Jar, dependsOn: javadoc) {
		    classifier "javadoc"
		    from javadoc.destinationDir
		}
		java {
		    sourceCompatibility = JavaVersion.VERSION_1_8
		    targetCompatibility = JavaVersion.VERSION_1_8
		}
		ext {
		    PUBLISH_GROUP_ID = 'io.github.happylishang'
		    PUBLISH_ARTIFACT_ID = 'abstracttransform'
		    PUBLISH_VERSION = '1.0.4'
		}
		
		ext["signing.keyId"] = ''
		ext["signing.password"] = ''
		ext["signing.secretKeyRingFile"] = ''
		ext["ossrhUsername"] = ''
		ext["ossrhPassword"] = ''
		
		File secretPropsFile = project.rootProject.file('local.properties')
		
		if (secretPropsFile.exists()) {
		    println "Found secret props file, loading props"
		    Properties p = new Properties()
		    p.load(new FileInputStream(secretPropsFile))
		    p.each { name, value ->
		        ext[name] = value
		    }
		} else {
		    println "No props file, loading env vars"
		}
		publishing {
		    publications {
		        mavenJava(MavenPublication) {
		
		
		
		
		            // The coordinates of the library, being set from variables that
		            // we'll set up in a moment
		            groupId PUBLISH_GROUP_ID
		            artifactId PUBLISH_ARTIFACT_ID
		            version PUBLISH_VERSION
		
		            artifact sourcesJar
		            artifact javadocJar
		//           jar包配置要添加下面的
		            from components.java
		            // Self-explanatory metadata for the most part
		            pom {
		                name = PUBLISH_ARTIFACT_ID
		                description = 'Activity start for result util'
		                // If your project has a dedicated site, use its URL here
		                url = 'https://github.com/happylishang/CacheEmulatorChecker'
		                licenses {
		                    license {
		                        //协议类型，一般默认Apache License2.0的话不用改：
		                        name = 'The Apache License, Version 2.0'
		                        url = 'http://www.apache.org/licenses/LICENSE-2.0.txt'
		                    }
		                }
		                developers {
		                    developer {
		                        id = 'BookSnail'
		                        name = 'BookSnail'
		                        email = 'happylishang@163.com'
		                    }
		                }
		                // Version control info, if you're using GitHub, follow the format as seen here
		                scm {
		                    //修改成你的Git地址：
		                    connection = 'https://github.com/happylishang/CacheEmulatorChecker.git'
		                    developerConnection = 'https://github.com/happylishang/CacheEmulatorChecker.git'
		                    url = 'https://github.com/happylishang/CacheEmulatorChecker'
		                }
		                // A slightly hacky fix so that your POM will include any transitive dependencies
		                // that your library builds upon
		//                jar包配置不需要下面的
		//                withXml {
		//
		//                    def dependenciesNode = asNode().appendNode('dependencies')
		//
		//                    project.configurations.implementation.allDependencies.each {
		//                        if (it.name != 'unspecified') {
		//                            def dependencyNode = dependenciesNode.appendNode('dependency')
		//                            dependencyNode.appendNode('groupId', it.group)
		//                            dependencyNode.appendNode('artifactId', it.name)
		//                            dependencyNode.appendNode('version', it.version)
		//                        }
		//                    }
		//
		//                }
		            }
		        }
		    }
		    repositories {
		        // The repository to publish to, Sonatype/MavenCentral
		        maven {
		            // This is an arbitrary name, you may also use "mavencentral" or
		            // any other name that's descriptive for you
		            name = "mavencentral"
		
		            def releasesRepoUrl = "https://s01.oss.sonatype.org/service/local/staging/deploy/maven2/"
		            def snapshotsRepoUrl = "https://s01.oss.sonatype.org/content/repositories/snapshots/"
		            // You only need this if you want to publish snapshots, otherwise just set the URL
		            // to the release repo directly
		            url = version.endsWith('SNAPSHOT') ? snapshotsRepoUrl : releasesRepoUrl
		
		            // The username and password we've fetched earlier
		            credentials {
		                username ossrhUsername
		                password ossrhPassword
		            }
		        }
		    }
		}
		signing {
		    sign publishing.publications
	}

local.properties配置mavenCenter账号

	sdk.dir=/Users/personal/Library/Android/sdk
	signing.keyId=CBAF123D
	signing.password=lisxxx11
	ossrhUsername=BookSnail
	signing.secretKeyRingFile=/Users/personal/secring.gpg
	ossrhPassword=bVkpVqtZ\#RAtwY2
 	


If buildscript itself needs something to run, use classpath.

If your project needs something to run, use compile.

The compile configuration is created by the Java plugin. The classpath configuration is commonly seen in the buildScript {} block where one needs to declare dependencies for the build.gradle, itself (for plugins, perhaps).

If your build script needs to use external libraries, you can add them to the script’s classpath in the build script itself. You do this using the buildscript() method, passing in a closure which declares the build script classpath.

This is the same way you declare, for example, the Java compilation classpath. You can use any of the dependency types described in Dependency Types, except project dependencies.

Having declared the build script classpath, you can use the classes in your build script as you would any other classes on the classpath.


### 插件调试

https://blog.csdn.net/ZYJWR/article/details/113129586

https://juejin.cn/post/6948626628637360135


# gradle7.0之后有变化


## 写插件注意事项

	plugins {
	//    有判断APP条件的插件，需要放在最前面
	    id("com.android.application")
	    id("com.netease.yanxuan.htrouterautoregister")

	}

否则isApp判断有问题

	class HTRouterAutoRegisterPlugin : Plugin<Project> {
    override fun apply(project: Project) {
        System.out.println("HTRouterAutoRegisterPlugin apply")
        val isApp = project.plugins.hasPlugin("com.android.application")
        if (isApp) {
            System.out.println("HTRouterAutoRegisterPlugin 2")
            val android = project.extensions.getByType(com.android.build.gradle.AppExtension::class.java)
            val transformImpl = AutoRegisterASMTransformer(project)
            android.registerTransform(transformImpl)
        }
    }
}

### pluginManagement中要配置repositories，否则build.gradle中找不到库，远老是不对


	 pluginManagement {
	//    似乎是在这里设置依赖比较靠谱，在里面设置 依赖的代码原，无效，老是自动jcenter
	    repositories {
	        google()
	        mavenCentral()
	    }
	    // 插件引入方式 这里应该是可以用给多个插件，
	    plugins {
	        id 'org.jetbrains.kotlin.android' version "1.8.10"
	    }

	    includeBuild("htrouterautorigister")
	} 

## 在线开发的关键
buildbuild.gradle与build.gradle.kt写法一致

	gradlePlugin {
	    plugins {
	        create("htrouterautorigister") {
	            // 在 app 模块须要经过 id 引用这个插件
	            id = "com.netease.yanxuan.htrouterautoregister"
	            // 实现这个插件的类的路径
	            implementationClass = "com.netease.htrouterautorigister.HTRouterAutoRegisterPlugin"
	        }
	    }
	}


## build.gradle.kts写法与build.gradle写法均可

settings.gradle要配置   id 'org.jetbrains.kotlin.android' version "1.8.10" 否则提醒找不到

	pluginManagement {
	//    似乎是在这里设置依赖比较靠谱，在里面设置 依赖的代码原，无效，老是自动jcenter
	    repositories {
	        google()
	        mavenCentral()
	    }
	    plugins {
	        id 'org.jetbrains.kotlin.android' version "1.8.10"
	    }
	    includeBuild("htrouterautorigister")
	}

build.gradle


	plugins {
	    id 'maven-publish'
	//    自动引入gradleAPI
	    id('java-gradle-plugin')
	    id 'org.jetbrains.kotlin.jvm' version('1.8.10')
	}
	//需要加，否则找不到com.android.tools.build:gradle:7.4.2"
	repositories {
	    google()
	    mavenCentral()
	}
	dependencies {
	//   自动引入gradleAPI   id('java-gradle-plugin')
	//    implementation("com.android.tools.build:gradle-api:7.4.2")
	    implementation("com.android.tools.build:gradle:7.4.2")
	    implementation 'org.javassist:javassist:3.20.0-GA'
	//    一些公共的要自己引入，不能依赖build中的，不靠谱
	    implementation 'commons-codec:commons-codec:1.11'
	//    commons-io
	    implementation 'commons-io:commons-io:2.6'
	    implementation("org.ow2.asm:asm:9.3")
	    implementation("org.ow2.asm:asm-commons:9.3")

	}


	task sourcesJar(type: Jar) {
	    classifier = 'sources'
	    from sourceSets.main.allSource
	}

	artifacts {
	    archives sourcesJar
	}


	publishing {
	    publications {
	        mavenJava(MavenPublication) {
	            from components.java
	            artifact sourcesJar
	            groupId = "com.netease.yanxuan"
	            artifactId = "htrouterautoregister"
	            version = "1.0.45"
	        }
	    }
	    repositories {

	        maven {
	//            url = uri("http://repo.mail.netease.com/artifactory/libs-release-local")
	            url = uri("$rootDir/../plugins")

	//            credentials {
	//                username = "user"
	//                password = "artifactory@163"
	//            }
	        }
	    }
	}

	gradlePlugin {
	    plugins {
	        create("htrouterautorigister") {
	            // 在 app 模块须要经过 id 引用这个插件
	            id = "com.netease.yanxuan.htrouterautoregister"
	            // 实现这个插件的类的路径
	            implementationClass = "com.netease.htrouterautorigister.HTRouterAutoRegisterPlugin"
	        }
	    }
	}

> * build.gradle.kt写法

settings.gradle要配置 写法一致

	pluginManagement {
	//    似乎是在这里设置依赖比较靠谱，在里面设置 依赖的代码原，无效，老是自动jcenter
	    repositories {
	        google()
	        mavenCentral()
	    }
	    // 插件引入方式
	    plugins {
	        id 'org.jetbrains.kotlin.android' version "1.8.10"
	    }

	    includeBuild("htrouterautorigister")
	}

build.grade.kt



	// buildscript {
	//     repositories {
	//         google()
	//         mavenCentral()
	//     }
	 //这种写法，只能用在一个里边，不能用在多个里边
	//     dependencies {
	//         // 由于使用的 Kotlin 须要须要添加 Kotlin 插件，这里使用最新的kotlin插件
	//         classpath("org.jetbrains.kotlin:kotlin-gradle-plugin:1.8.10")
	//     }
	// }

	plugins {
	    `kotlin-dsl`
	    `java-gradle-plugin`
	    `maven-publish`
	}

	repositories {
	    google()
	    mavenCentral()
	}

	dependencies {
	    // 自动引入gradleAPI   id('java-gradle-plugin')
	    // implementation("com.android.tools.build:gradle-api:7.4.2")

	    implementation("org.javassist:javassist:3.20.0-GA")
	    //    一些公共的要自己引入，不能依赖build中的，不靠谱
	    implementation("commons-codec:commons-codec:1.11")
	    //    commons-io
	    implementation("commons-io:commons-io:2.6")
	    implementation("org.ow2.asm:asm:9.3")
	    implementation("org.ow2.asm:asm-commons:9.3")
	    // com.android.build.gradle.AppExtension用的到
	    implementation("com.android.tools.build:gradle:7.4.2")
	}
	kotlinDslPluginOptions {
	    experimentalWarning.set(false)
	}
	gradlePlugin {
	    plugins {
	        create("htrouterautorigister") {
	            // 在 app 模块须要经过 id 引用这个插件
	            id = "com.netease.yanxuan.htrouterautoregister"
	            // 实现这个插件的类的路径
	            implementationClass = "com.netease.htrouterautorigister.HTRouterAutoRegisterPlugin"
	        }
	    }
	}

	publishing {
	    publications {
	        create<MavenPublication>("maven") {
	            groupId = "org.gradle.sample"
	            artifactId = "library"
	            version = "1.1"

	            from(components["java"])
	        }
	    }
	    repositories {

	        maven {
	//            url = uri("http://repo.mail.netease.com/artifactory/libs-release-local")
	            url = uri("$rootDir/../plugins")

	//            credentials {
	//                username = "user"
	//                password = "artifactory@163"
	//            }
	        }
	    }
	}
	
## 	group不设置会提醒Invalid publication 'pluginMaven'，奇葩

	//不设置会提醒 Invalid publication 'pluginMaven': groupId cannot be empty.
group='com.n '

### 	结构 ：无需settings.gradle.kts，只是当做普通的模块，进入草includeBuild中就行
	
		htrouterautorigister
		.gradle
		build
		src
		.gitignore
		build.gradle
		
假如到构建体系，同时自己是一个java插件库

### 最保守的方式，当做普通仓库，引入处理，不过要每次都发布一下，不能在线调试

## gradlePlugin必须加，group  version 必须加

	
	group = 'com.snail'
	version = "1.0.0"

	//发布的时候，还是必须要有的

	gradlePlugin {
	    plugins {
	        autorigister {
	            // 在 app 模块须要经过 id 引用这个插件
	            id = "com.snail.autorigister"
	            // 实现这个插件的类的路径
	            implementationClass = "com.snail.autorigister.AutoRegisterPlugin"
	        }
	        autorigister2 {
	            // 在 app 模块须要经过 id 引用这个插件
	            id = "com.snail.autorigister2"
	            // 实现这个插件的类的路径
	            implementationClass = "com.snail.autorigister.AutoRegisterPlugin2"
	        }
	    }
	}

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c1de90e408a9465caf1e40c1886e2cc9~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=964&h=1328&s=238253&e=png&b=2c2e31)
		
### 插件dubug

<!--如何debug-->

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f9789c21951a482d8a334b7ceda1ce0b~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=2622&h=1030&s=490121&e=png&b=242629)

### ASM 切面编程扫描全部class，可以用TransformTask

            androidComponents.onVariants { variant ->
                val taskProvider = project.tasks.register(
                    "${variant.name}TransformRouterTask", TransformRouterTask2::class.java
                )
                variant.artifacts.forScope(ScopedArtifacts.Scope.ALL).use(taskProvider)
                    .toTransform(
                        type = ScopedArtifact.CLASSES,
                        inputJars = TransformRouterTask2::allJars,
                        inputDirectories = TransformRouterTask2::allDirectories,
                        into = TransformRouterTask2::output
                    )
                    
参考路由，

	    /**
	     * 遍历并修改目标class，task中定义了输出，需要将所有的都写入草jar，不能过滤，其他的修改后写入，否则会有问题 jar校验问题
	     */
	    private fun transformJar(inputJar: File, jarOutput: JarOutputStream) {
	        val jarFile = JarFile(inputJar)
	        jarFile.entries().iterator().forEach { jarEntry ->
	            if (jarEntry.name.equals("com/ RouterInitializer.class")) {
	                jarOutput.putNextEntry(JarEntry(jarEntry.name))
	                asmTransform(jarFile.getInputStream(jarEntry)).inputStream().use {
	                    it.copyTo(jarOutput)
	                }
	                jarOutput.closeEntry()
	            }
	        }
	        jarFile.close()
	    }
	
参考来写就好了	                    