函数组件、类组件

	class Welcome extends React.Component{
	    render(){
	        return <h1> hello,{this.props.name}</h1>
	    }
	}
	 
	function App(){
	    return (
	        <div>
	            <Welcome name="Sara"/>
	            <Welcome nmae="Peng"/>
	        </div>
	    );
	}
	 
	export  {Welcome,App};

other.js

	import {App}  from './components/Com';
	 
	const element=<App/>;
	ReactDOM.render(element, document.getElementById('root'));


通过export方式导出，在导入时要加{ }，export default则不需要。使用export default命令，

	class Welcome extends React.Component{
	    render(){
	        return <h1> hello,{this.props.name}</h1>
	    }
	}
	 
	function App(){
	    return (
	        <div>
	            <Welcome name="Sara"/>
	            <Welcome nmae="Peng"/>
	        </div>
	    );
	}
	 
	export  default App;

other.js
	
	import App  from './components/Com';
	 
	const element=<App/>;
	ReactDOM.render(element, document.getElementById('root'));
	
	
匿名的	export default

在 React 项目中如何使用 TS  ：https://juejin.cn/post/7128929470618009608，各种文件语言的兼容。

Scss 与 Sass异同
Sass 和 Scss 其实就是同一种东西，我们平时都称之为 Sass（萨斯），两者之间不同之处主要有以下两点：
1.文件扩展名不同，Sass 是以“.sass”后缀为扩展名，而 Scss 是以“.scss”后缀为扩展名。
2.语法书写方式不同，Sass 是以严格的缩进式语法规则来书写，不带大括号 {} 和 分号 ；，而 Scss 的语法书写和我们的CSS 语法书写方式非常类似。

前端框架千千万，其实就是个三方库，轮子
