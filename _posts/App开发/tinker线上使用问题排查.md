2020.7.29上线首次问题修复，修复问题位于组单页OrderCommoditiesActivity，修复类代码位于OrderCommoditiesPresenter.java



    private void getInitData() {
        Intent intent = target.getIntent();
        if (intent == null) {
            return;
        }
        orderModel = OneOffMemoryCenter.INSTANCE.get(ConstantsSC.SHOPPINGCART_INTENT_COMPOSED_ORDER_MODEL);
        mOrderCart = OneOffMemoryCenter.INSTANCE.get(ConstantsSC.KEY_SELECT_ID_LIST);

        isBuyNow = RouterParamParser.getBooleanRouterParam(intent, ConstantsSC.SHOPPINGCART_INTENT_IS_BUY_NOW_KEY, false);
        mOrderId = RouterParamParser.getLongRouterParam(intent, ConstantsSC.KEY_ORDER_ID, -1);// 如果没有orderId，就给-1
        mExtraItemServiceJson = RouterParamParser.getStringRouterParam(intent, ConstantsSC.KEY_EXTRA_SERVICE, null);
        target.setLeaveDialogData(orderModel != null ? orderModel.getFirstOrderRefundInfo() : null);
    }

上线后，部分用户出现native crash，反应到堆栈，仅仅有libc.so堆栈出现

	
	已崩溃：Thread #1
	SIGABRT 0x0000000000007ad7
	libc.so
	(缺少)
	(缺少)
	libart.so
	(缺少)
	libart.so
	(缺少)
	(缺少)
	(缺少)
	(缺少)
	libart.so
	(缺少)
	(缺少)
	(缺少)
	
有与Firebase SDK问题，native堆栈日志统计不全，很难定位问题，目前还未能找到手机复现。



	