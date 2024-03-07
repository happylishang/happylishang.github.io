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