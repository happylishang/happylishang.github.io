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