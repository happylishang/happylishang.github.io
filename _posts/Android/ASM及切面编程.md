AspectJ、ASM 等等，它们的相同之处在于输入输出都是 Class 文件，并且，它们都是 在 Java 文件编译成 .class 文件之后，生成 Dalvik 字节码之前执行。

看下一个函数的ASM代码

	{
	            methodVisitor = classWriter.visitMethod(ACC_PUBLIC, "onCreate", "()V", null, null);
	            methodVisitor.visitCode();
	            methodVisitor.visitVarInsn(ALOAD, 0);
	            methodVisitor.visitMethodInsn(INVOKESPECIAL, "android/app/Application", "onCreate", "()V", false);
	            methodVisitor.visitVarInsn(ALOAD, 0);
	            methodVisitor.visitFieldInsn(PUTSTATIC, "com/snail/labaffinity/app/LabApplication", "sApplication", "Landroid/app/Application;");
	            methodVisitor.visitVarInsn(ALOAD, 0);
	            methodVisitor.visitMethodInsn(INVOKESTATIC, "cn/campusapp/router/Router", "initBrowserRouter", "(Landroid/content/Context;)V", false);
	            methodVisitor.visitVarInsn(ALOAD, 0);
	            methodVisitor.visitMethodInsn(INVOKEVIRTUAL, "com/snail/labaffinity/app/LabApplication", "getApplicationContext", "()Landroid/content/Context;", false);
	            methodVisitor.visitMethodInsn(INVOKESTATIC, "cn/campusapp/router/Router", "initActivityRouter", "(Landroid/content/Context;)V", false);
	            methodVisitor.visitLdcInsn("LabApplication create");
	            methodVisitor.visitMethodInsn(INVOKESTATIC, "com/snail/labaffinity/utils/LogUtils", "v", "(Ljava/lang/String;)V", false);
	            methodVisitor.visitTypeInsn(NEW, "android/os/HandlerThread");
	            methodVisitor.visitInsn(DUP);
	            methodVisitor.visitLdcInsn("d");
	            methodVisitor.visitMethodInsn(INVOKESPECIAL, "android/os/HandlerThread", "<init>", "(Ljava/lang/String;)V", false);
	            methodVisitor.visitVarInsn(ASTORE, 1);
	            methodVisitor.visitFieldInsn(GETSTATIC, "com/snail/labaffinity/app/SingleTonHandlerThread", "INSTANCE", "Lcom/snail/labaffinity/app/SingleTonHandlerThread;");
	            methodVisitor.visitMethodInsn(INVOKEVIRTUAL, "com/snail/labaffinity/app/SingleTonHandlerThread", "getHandlerThread", "()Landroid/os/HandlerThread;", false);
	            methodVisitor.visitVarInsn(ASTORE, 2);
	            methodVisitor.visitInsn(RETURN);
	            methodVisitor.visitMaxs(3, 3);
	            methodVisitor.visitEnd();
	        }
	        
先获取methodVisitor，然后 methodVisitor.visitCode ，中间调用各种函数，最后methodVisitor.visitEnd()。

### ClassVisitor对应的是函数的调用，字段的处理等



### * 如何处理构造函数的替换比如单利  ：MethodVisito对应的是每一条指令的调用 

拦截new 与赋值之间的所有操作
	
	   class SingleTonMethodVisitor(mv: MethodVisitor) : MethodVisitor(ASM7, mv) {
	
	        //  方法调用前的调用 这里的面试往里面插入调用的，ASM处理，真的调用在运行时候，这里ASM写入字结码
	
	        private var startAsm = false
	        override fun visitTypeInsn(opcode: Int, type: String?) {
	            if (startAsm || opcode == NEW && type == "android/os/HandlerThread") {
	                startAsm = true
	            } else
	                super.visitTypeInsn(opcode, type)
	        }
	
	        override fun visitInsn(opcode: Int) {
	            if (!startAsm)
	                super.visitInsn(opcode)
	        }
	
	        override fun visitLdcInsn(value: Any?) {
	            if (!startAsm)
	                super.visitLdcInsn(value)
	        }
	
	        override fun visitParameter(name: String?, access: Int) {
	            if (!startAsm)
	                super.visitParameter(name, access)
	        }
	
	
	        override fun visitParameterAnnotation(
	            parameter: Int,
	            descriptor: String?,
	            visible: Boolean,
	        ): AnnotationVisitor {
	            return super.visitParameterAnnotation(parameter, descriptor, visible)
	        }
	
	        override fun visitAttribute(attribute: Attribute?) {
	            if (!startAsm)
	                super.visitAttribute(attribute)
	        }
	
	        override fun visitCode() {
	            if (!startAsm)
	                super.visitCode()
	        }
	
	        override fun visitFrame(
	            type: Int,
	            numLocal: Int,
	            local: Array<out Any>?,
	            numStack: Int,
	            stack: Array<out Any>?,
	        ) {
	            if (!startAsm)
	                super.visitFrame(type, numLocal, local, numStack, stack)
	        }
	
	        override fun visitIntInsn(opcode: Int, operand: Int) {
	            if (!startAsm)
	                super.visitIntInsn(opcode, operand)
	        }
	
	        override fun visitVarInsn(opcode: Int, varIndex: Int) {
	            if (!startAsm)
	                super.visitVarInsn(opcode, varIndex)
	        }
	
	        override fun visitFieldInsn(
	            opcode: Int,
	            owner: String?,
	            name: String?,
	            descriptor: String?,
	        ) {
	            if (!startAsm)
	                super.visitFieldInsn(opcode, owner, name, descriptor)
	        }
	
	        override fun visitInvokeDynamicInsn(
	            name: String?,
	            descriptor: String?,
	            bootstrapMethodHandle: Handle?,
	            vararg bootstrapMethodArguments: Any?,
	        ) {
	            if (!startAsm)
	                super.visitInvokeDynamicInsn(
	                    name,
	                    descriptor,
	                    bootstrapMethodHandle,
	                    *bootstrapMethodArguments
	                )
	        }
	
	        override fun visitJumpInsn(opcode: Int, label: Label?) {
	            if (!startAsm) super.visitJumpInsn(opcode, label)
	        }
	
	        override fun visitLabel(label: Label?) {
	            if (!startAsm) super.visitLabel(label)
	        }
	
	        override fun visitIincInsn(varIndex: Int, increment: Int) {
	            if (!startAsm) super.visitIincInsn(varIndex, increment)
	        }
	
	        override fun visitTableSwitchInsn(min: Int, max: Int, dflt: Label?, vararg labels: Label?) {
	            if (!startAsm) super.visitTableSwitchInsn(min, max, dflt, *labels)
	        }
	
	        override fun visitLookupSwitchInsn(
	            dflt: Label?,
	            keys: IntArray?,
	            labels: Array<out Label>?,
	        ) {
	            if (!startAsm) super.visitLookupSwitchInsn(dflt, keys, labels)
	        }
	
	        override fun visitMultiANewArrayInsn(descriptor: String?, numDimensions: Int) {
	            if (!startAsm) super.visitMultiANewArrayInsn(descriptor, numDimensions)
	        }
	
	        override fun visitInsnAnnotation(
	            typeRef: Int,
	            typePath: TypePath?,
	            descriptor: String?,
	            visible: Boolean,
	        ): AnnotationVisitor {
	            return super.visitInsnAnnotation(typeRef, typePath, descriptor, visible)
	        }
	
	        override fun visitTryCatchBlock(
	            start: Label?,
	            end: Label?,
	            handler: Label?,
	            type: String?,
	        ) {
	            if (!startAsm) super.visitTryCatchBlock(start, end, handler, type)
	        }
	
	        override fun visitTryCatchAnnotation(
	            typeRef: Int,
	            typePath: TypePath?,
	            descriptor: String?,
	            visible: Boolean,
	        ): AnnotationVisitor {
	            return super.visitTryCatchAnnotation(typeRef, typePath, descriptor, visible)
	        }
	
	        override fun visitLocalVariable(
	            name: String?,
	            descriptor: String?,
	            signature: String?,
	            start: Label?,
	            end: Label?,
	            index: Int,
	        ) {
	            if (!startAsm) super.visitLocalVariable(name, descriptor, signature, start, end, index)
	        }
	
	        override fun visitLocalVariableAnnotation(
	            typeRef: Int,
	            typePath: TypePath?,
	            start: Array<out Label>?,
	            end: Array<out Label>?,
	            index: IntArray?,
	            descriptor: String?,
	            visible: Boolean,
	        ): AnnotationVisitor {
	            return super.visitLocalVariableAnnotation(
	                typeRef,
	                typePath,
	                start,
	                end,
	                index,
	                descriptor,
	                visible
	            )
	        }
	
	        override fun visitLineNumber(line: Int, start: Label?) {
	            if (!startAsm)
	                super.visitLineNumber(line, start)
	        }
	
	        override fun visitMaxs(maxStack: Int, maxLocals: Int) {
	            if (!startAsm)
	                super.visitMaxs(maxStack, maxLocals)
	        }
	
	        override fun visitEnd() {
	            super.visitEnd()
	        }
	
	        override fun visitMethodInsn(
	            opcode: Int,
	            owner: String?,
	            name: String?,
	            descriptor: String?,
	            isInterface: Boolean,
	        ) {
	
	            if (opcode == INVOKESPECIAL && owner == "android/os/HandlerThread" && name == "<init>") {
	                println("$owner $name $descriptor")
	                mv.visitFieldInsn(GETSTATIC, "com/snail/labaffinity/app/SingleTonHandlerThread", "INSTANCE", "Lcom/snail/labaffinity/app/SingleTonHandlerThread;");
	                mv.visitMethodInsn(INVOKEVIRTUAL, "com/snail/labaffinity/app/SingleTonHandlerThread", "getHandlerThread", "()Landroid/os/HandlerThread;", false);
	                startAsm = false
	            } else
	                super.visitMethodInsn(opcode, owner, name, descriptor, isInterface)
	
	        }
	    }