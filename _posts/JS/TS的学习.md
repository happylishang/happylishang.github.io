#### declare关键字与d.ts文件

declare与d.ts文件是声明其他文件已经定义过的类，尤其是JavaScript中定义的，其中的函数不能有实现、变量不能初始化

	
	declare class TextInputAttribute extends CommonMethod<TextInputAttribute> {
	    /**
	     * Called when the input type is set.
	     * @since 7
	     */
	    type(value: InputType): TextInputAttribute;
	    /**
	     * Called when the color of the placeholder is set.
	     * @since 7
	     */
	    placeholderColor(value: ResourceColor): TextInputAttribute;
	    
	    }

还可以为JavaScript 引擎的原生对象添加属性和方法，可以使用declare global {}

	    export {};

			declare global {
			  interface String {
			    toSmallString(): string;
			  }
			}
					
			String.prototype.toSmallString = ():string => {
			  // 具体实现
			  return '';
			};
	    
参考	[declare 关键字](https://wangdoc.com/typescript/declare)    