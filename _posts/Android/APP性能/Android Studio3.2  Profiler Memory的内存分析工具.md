
## 内存使用统计问题  

整体内存的使用，看APP heap就可以了，

* Allocations：堆中的实例数。
* Shallow Size：此堆中所有实例的总大小（以字节为单位）。**其实算是比较真实的java堆内存**
* Retained Size：为此类的所有实例而保留的内存总大小（以字节为单位）。**会有重复统计的问题**

举个例子，创建一个List的场景，有一个ListItem40MClass类，自身占用40M内存，每个对象有个指向下一个ListItem40MClass对象的引用，从而构成List，


    class ListItem40MClass {
    
        byte[] content = new byte[1000 * 1000 * 40];
        ListItem40MClass() {
            for (int i = 0; i < content.length; i++) {
                content[i] = 1;
            }
        }

        @Override
        protected void finalize() throws Throwable {
            super.finalize();
        }

        ListItem40MClass next;
    }


    @OnClick(R.id.first)
    void first() {
        if (head == null) {
            head = new ListItem40MClass();
        } else {
            ListItem40MClass tmp = head;
            while (tmp.next != null) {
                tmp = tmp.next;
            }
            tmp.next = new ListItem40MClass();
        }
    }

我们创建三个这样的对象，并形成List，示意如下
	
	A1->next=A2
	A2->next=A3 
	A3->next= null

这个时候用Android Profiler查看内存，会看到如下效果：Retained Size统计要比实际3个ListItem40MClass类对象的大小大的多，如下图：

![281540022720_.pic_hd.jpg](https://upload-images.jianshu.io/upload_images/1460468-a563b20d9b852cc2.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到就总量而言Shallow Size基本能真是反应Java堆内存，而Retained Size却明显要高出不少， 因为Retained Size统计总内存的时候，基本不能避免重复统计的问题，比如：A对象有B对象的引用在计算总的对象大小的时候，一般会多出一个B，就像上图，有个3个约40M的int[]对象，占内存约120M,而每个ListItem40MClass对象至少会再统计一次40M，这里说的是至少，因为对象间可能还有其他关系。我们看下单个类的内存占用-Instance View

* Depth：从任意 GC 根到所选实例的最短 hop 数。
* Shallow Size：此实例的大小。
* Retained Size：此实例支配的内存大小（根据 dominator 树）。

可以看到Head本身的Retained Size是120M ，Head->next 是80M，最后一个ListItem40MClass对象是40M，因为每个对象的Retained Size除了包括自己的大小，还包括引用对象的大小，整个类的Retained Size大小累加起来就大了很多，所以如果想要看整体内存占用，看Shallow Size还是相对准确的，Retained Size可以用来大概反应哪种类占的内存比较多，仅仅是个示意，不过还是Retained Size比较常用，因为Shallow Size的大户一般都是String，数组，基本类型意义不大，如下。

![291540025853_.pic.jpg](https://upload-images.jianshu.io/upload_images/1460468-f1d8100edeecd85b.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
	
## FinalizerReference大小跟内存泄漏的关系


![image.png](https://upload-images.jianshu.io/upload_images/1460468-8791c7700db8e906.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

	