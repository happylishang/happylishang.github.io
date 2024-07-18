React语法

### useState

第一个是状态值，第二个是赋值函数，什么辣鸡玩意

	const [state, setState] = useState(initialState);
	
	
    import React, { useState } from 'react';
	
	function Bulbs() {
	<!--用了useState就说明，Bulbs有了状态能力 化身 有状态的函数组件-->
	  const [on, setOn] = useState(false);
	
	  const lightOn = () => setOn(true);
	  const lightOff = () => setOn(false);
	
	  return (
	    <>
	      <div className={on ? 'bulb-on' : 'bulb-off'} />
	      <button onClick={lightOn}>开灯</button>
	      <button onClick={lightOff}>关灯</button>
	    </>
	  );
	}


也可以用前一个值更新后一个

	// Increase a counter
	const [count, setCount] = useState(0);
	setCount(count => count + 1);
	
	setOn(on => !on)
	
	
延迟渲染，降低开销，而且防止每次都来一遍


	  const [value, setValue] = useState(function getInitialState() {
	    const object = JSON.parse(bigJsonData); // expensive operation
	    return object.initialValue;
	  });

在使用useState() Hook 时，必须遵循 Hook 的规则


* 仅顶层调用 Hook ：不能在循环，条件，嵌套函数等中调用useState()。
* 在多个useState()调用中，渲染之间的调用顺序必须相同。
* 仅从React 函数调用 Hook:必须仅在函数组件或自定义钩子内部调用useState()。
* useState是异步事件，不能及时获取到最新值
 
 
###  React框架的基本运行原理 

参考文档  [React框架的基本运行原理  ](https://www.cnblogs.com/zhou--fei/p/17778789.html)

React的本质是内部维护了一套虚拟DOM树，这个虚拟DOM树就是一棵js对象树，它和真实DOM树是一致的，一一对应的。
当某一个**组件的state**发生修改时，就会生成一个新的虚拟DOM，让它和旧的虚拟DOM通过**Diff算法**进行对比，生成一组差异对象。
然后**遍历差异对象，将修改更新到真实的DOM树上**。

* React的三大特性：JSX语法糖，虚拟DOM， Diff算法
* 虚拟DOM就是一个普通的 JavaScript 对象，包含了 tag、props、children 三个属性。
* 虚拟DOM1是为了提升浏览器的渲染性能
* 虚拟DOM2是跨平台

		三个重要点：
		Tag：参数一：标签名字符串
		props 参数二：属性对象
		children 参数三及其更多：子元素


JSX只是一个语法糖，内部还是要被编译成React.createElement。


###  React Hook 

* Hook本身单词意思是“钩子”，作用就是“勾住某些生命周期函数或某些数据状态，并进行某些关联触发调用”。
* Hooks只能运行在函数组件中，不能运行在类组件中。
* Hook只是React“增加”的概念和一些API，对原有React体系并没有任何破坏。

### useState ：勾住函数组件中自定义的变量

在React底层代码中，是通过自定义dispatcher，采用“发布订阅模式”实现的。

####  useEffect ： 勾住函数组件中某些生命周期函数

*  componentDidMount(组件被挂载完成后)
*  componentDidUpdate(组件重新渲染完成后)
*  componentWillUnmount(组件即将被卸载前)


