### 配置

* react Native版本  0.70 yarn add react-native@0.70

如下跑的起来，中文官方的不靠谱，跑不起来

	  "dependencies": {
	    "react": "^18.3.1",
	    "react-dom": "^18.3.1",
	    "react-native": "0.70"
	  }

靠谱命令	yarn add react@latest react-dom@latest
	
	 👇️ with NPM
	npm install react@latest react-dom@latest
	👇️ ONLY If you use TypeScript
	
	npm install --save-dev @types/react@latest @types/react-dom@latest
	 
	👇️ with YARN
	yarn add react@latest react-dom@latest
	
	👇️ ONLY If you use TypeScript
	yarn add @types/react@latest @types/react-dom@latest --dev
	
用$ yarn add react@16.2.0 会提示 

	Uncaught Error: Cannot find module 'react/jsx-runtime'
	
	


## 工程代码配置

* android工程跟node_module、package.json同一级别
* 在你的 app 中 build.gradle 文件中添加 React Native 和 JSC 引擎依赖:


	    implementation "com.facebook.react:react-native:+" // From node_modules
	    implementation "org.webkit:android-jsc:+"
	    
*     在项目的 build.gradle 文件中为 React Native 和 JSC 引擎添加 maven 源的路径，必须写在 "allprojects" 代码块中

	allprojects {
	    repositories {
	        maven {
	            // All of React Native (JS, Android binaries) is installed from npm
	            url "$rootDir/../node_modules/react-native/android"
	        }
	        maven {
	            // Android JSC is installed from npm
	            url("$rootDir/../node_modules/jsc-android/dist")
	        }
	        ...
	    }
	    ...
	}

* 启用原生模块的自动链接

settings.gradle:

	 apply from: file("../node_modules/@react-native-community/cli-platform-android/native_modules.gradle"); applyNativeModulesSettingsGradle(settings)

接下来，在app/build.gradle的最底部添加以下内容:

	apply from: file("../../node_modules/@react-native-community/cli-platform-android/native_modules.gradle"); applyNativeModulesAppBuildGradle(project)

* index.js放在工程根目录

* $ yarn start 启动 Metro模拟器

之后就可以了。


## 构建bundle

	react-native bundle --platform android --dev false --entry-file index.js --bundle-output android/app/src/main/assets/index.android.bundle --assets-dest android/app/src/main/res
	
	
三方demo异常

> Failed to construct transformer: Error: error:0308010C:digital envelope routines::unsupported in React native	

安装：

	brew install watchingman	
	
	
配置Linux / macOS
	
	export NODE_OPTIONS=--openssl-legacy-provider

