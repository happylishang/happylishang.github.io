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

之后就可以了。如果一直连不上，尝试

	adb reverse tcp:8081 tcp:8081

## 构建bundle

	react-native bundle --platform android --dev false --entry-file index.js --bundle-output android/app/src/main/assets/index.android.bundle --assets-dest android/app/src/main/res
	
	
### * 三方demo异常

> Failed to construct transformer: Error: error:0308010C:digital envelope routines::unsupported in React native	

安装：

	brew install watchingman	
	
	
配置Linux / macOS
	
	export NODE_OPTIONS=--openssl-legacy-provider
	
### * Error: EMFILE: too many open files, watch at FSWatcher._handle.onchange

	安装watchMan
	
### 	直接使用ReactActivity

	    @Override
    public ReactNativeHost getReactNativeHost() {
        return new ReactNativeHost(this) {
            @Override
            public boolean getUseDeveloperSupport() {
                return BuildConfig.DEBUG;
            }

            @Override
            protected List<ReactPackage> getPackages() {
                return Collections.singletonList(new MainReactPackage());
            }

	//     这里index.js的入口
            @Override
            protected String getJSMainModuleName() {
                return "index";
            }
        };
    }
    

直接使用AppRegistry.registerComponent注册的模块
    
    class ArcReactActivity : ReactActivity() {
	//    这里显示是RN 注册的入口
	//    AppRegistry.registerComponent
	    override fun getMainComponentName(): String = "MyReactNativeApp"
	     override fun createReactActivityDelegate(): ReactActivityDelegate =
	        ReactActivityDelegate(this, mainComponentName)
	}

## react native业务形态，如何动态支持不同业务

index jsmodule怎么打包，下发


### yarn 还是 npm可以根据文件yarn.lock判断

* yarn android  
* npm run android



## react native语法

	const element = React.createElement(Text, null, "Hello World!")

等效

	const element = <Text>Hello World!</Text>
	
如何定义带参数组件
	
	 interface Props {
	  name: string
	}
	
	function Welcome(props: Props) {
	  return <Text>Hello {props.name}!</Text>
	}
	
	function App() {
	  return (
	    <View>
	      <Welcome name="Sara" />
	      <Welcome name="Cahal" />
	      <Welcome name="Edite" />
	    </View>
	  )
	}

如何更新状态 

	import React, { useState } from "react"
	import { View, Text, TextInput, Button, StyleSheet } from "react-native"
	import { withNavigationItem } from "react-native-navigation-hybrid"
	
	interface Props {
	  name: string
	}
	
	function Welcome(props: Props) {
	  return <Text style={styles.text}>Hello {props.name}!</Text>
	}
	
	function App() {
	  const [name, setName] = useState("Sara")
	  const [text, setText] = useState("")
	  return (
	    <View style={styles.container}>
	      <Welcome name={name} />
	      <TextInput value={text} onChangeText={setText} style={styles.input} />
	      <Button title="确定" onPress={() => setName(text)} />
	    </View>
	  )
	}

### babel.config.js   错误  一个demonpm install react-native-reanimated react-native-animatable —save // or yarn add react-native-reanimated react-native-animatable —save // and npm install @react-native-community/masked-view —save // or yarn add @react-native-community/masked-view —save // and npm install react-native-safe-area-context —save // or yarn add react-native-safe-area-context —save // and npm install @react-navigation/stack —save // or yarn add @react-navigation/stack —save // and npm install @react-navigation/native —save // or yarn add @react-navigation/native —save // and npm install @react-navigation/stack // or yarn add @react-navigation/stack —save // and npm install @react-navigation/stack // or yarn add @react-navigation/stack —save // and npm install @react-navigation/stack // or yarn add @react-navigation/stack —save // and npm installnpm install react-native-reanimated react-native-animatable —save // or yarn add react-native-reanimated react-native-animatable —save // and npm install @react-native-community/masked-view —save // or yarn add @react-native-community/masked-view —save // and npm install react-native-safe-area-context —save // or yarn add react-native-safe-area-context —save // and npm install @react-navigation/stack —save // or yarn add @react-navigation/stack —save // and npm install @react-navigation/native —save // or yarn add @react-navigation/native —save // and npm install @react-navigation/stack // or yarn add @react-navigation/stack —save // and npm install @react-navigation/stack // or yarn add @react-navigation/stack —save // and npm install @react-navigation/stack // or yarn add @react-navigation/stack —save // and npm install

	const path = require('path');
	const pak = require('../package.json');
	
	module.exports = {
	  presets: ['module:metro-react-native-babel-preset'],
	  overrides: [{
	    "plugins": [
	      ["@babel/plugin-transform-private-methods", {
	      "loose": true
	    }]
	    ]
	  }]
	};

## 	参考文档

[React Native 开发指南](https://todoit.tech/rn/framework.html#%E8%AE%A4%E8%AF%86-props)

