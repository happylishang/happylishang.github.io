* Android：WMS窗口管理+Activity（辅助） 其实有两套 
* iOS：View+VC 
* Flutter:View+VC(简)  更偏向iOS，

从Flutter看Google对于UI管理上的修改：Chrome是Google最厉害的工具。Flutter孵化与Chrome团队，

可能Google也感觉Android中Activity似乎有些多余，就单纯的界面显示而言，Activity完全是一个辅助工具，但是由于它参与了太多界面显示的东西，承担了太多非View界面的责任，导致WMS跟View自身功能的猥琐，AMS管理Activity，WMS管理窗口，但是WMS管理窗口的能力太低了，都AMS占用了，AMS管理四大组件，但就Activity而言，各种栈，各种恢复就比较麻烦。

Activity:窗口的管理与分组更加不好做，Activity是AMS管理，但是其代表的Token确实WMS中窗口分组的依据，Actiity的栈又存在多Task等类型，控制起来可能就更加不如意，导致AMS跟WMS合起来管理窗口，而且两者的分工也不是特别分明，弱化自大组件

## 布局的优缺点

Android布局性能越好，Android布局对于开发者的优势越明显，
iOS早起的自己计算高度的能力不错，但是硬件性能上来之后，这种优势会被缩小，虽然仍然有，但是没以前那么明显，Android越来越流畅就是个典型例子。