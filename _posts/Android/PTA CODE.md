#### Middium
 
* 7-19Call to your teacher 并查集

 从实验室出来后，你忽然发现你居然把自己的电脑落在了实验室里，但是实验室的老师已经把大门锁上了。

更糟的是，你没有那个老师的电话号码。你开始给你知道的所有人打电话，询问他们有没有老师的电话，如果没有，他们也会问自己的同学来询问电话号码。

那么，你能联系到老师并且拿到电脑吗？

输入样例1:
在这里给出一组输入。例如：

5 5
1 3
2 3
3 4
2 4
4 5
输出样例1:
在这里给出相应的输出。例如：

Yes

	// 并查集，或者找路径
	import java.util.*;
	public class Main{
	
	    public static void main(String[] oooo){
	            Scanner sc=new Scanner(System.in);
	        int[] arr=Arrays.stream(sc.nextLine().split(" ")).
	            mapToInt(Integer::parseInt).toArray();
	        int n = arr[0];
	        int k1 = arr[1];
	        int[] father=new  int[n];
	        
	              for(int i=0;i<n;i++)father[i]=i;
	        for(int i=0;i<k1;i++){
	        int[] arr1=Arrays.stream(sc.nextLine().split(" ")).
	          mapToInt(Integer::parseInt).toArray();
	        int a = arr1[0];
	        int b = arr1[1];
	      
	        if(a!=n){
	            // 合并 如果存在老师的联通集合，那么一定是老师是顶部
	            uni(a-1,b-1,father);
	            }
	        }
	
	        // 奇葩啊，奇葩，过了 ==n-1的就行了，虽然还没理解
	        if(find(0,father)==find(n-1,father)){
	            System.out.print("Yes");
	        }else{
	           System.out.print("No");
	        }
	    }
	    static int find(int x,int[] father){
	        if(x!=father[x]){
	            return find(father[x],father);
	        }
	        return x;
	    }
	    static void uni(int x,int y,int[] father){
	          father[find(x,father)]= find(y,father);
	    }
	}

#### 7-20 根据数字的补数排序 ：sort以及 Math.log(n)/Math.log(2)

整数的二进制表示取反（0 变 1 ，1 变 0）后，再转换为十进制表示，可以得到这个整数的补数。

例如，整数 5 的二进制表示是 "101" （没有前导零位），取反后得到 "010" ，再转回十进制表示得到补数 2 。

给你一个整数数组 arr 。请你将数组中的元素按照其补数升序排序。如果补数相同，则按照原数值大小升序排列。

请你返回排序后的数组。

提示：

1 <= arr.length <= 500

0 <= arr[i] <= 10^4

输入格式:
整数数组arr，以",”分隔字符串的形式作为输入

输出格式:
排好序的整数数组，以",”分隔字符串的形式作为输出

输入样例:
原始数组arr：

5,10,4,2
输出样例:
排序后的arr：

2,5,4,10

	import java.util.*;
	
	public class Main{
	
	    public static void main(String [] aaa){
	        Scanner sc=new Scanner(System.in);
	        int[] inputs=Arrays.stream(sc.nextLine().split(","))
	            .mapToInt(Integer::parseInt).toArray();
	        Pair[] pairs=new Pair[inputs.length];
	        for(int i=0;i<inputs.length;i++)
	        {
	            if( inputs[i]==0)  pairs[i]=new Pair(inputs[i],1);
	            else  if( inputs[i]==1) pairs[i]=new Pair(inputs[i],0);
	            else {
	                // pow(2,x)
	                pairs[i]=new Pair(inputs[i],inputs[i]^((int)Math.pow(2,lon(inputs[i])+1)-1));
	            }
	            
	        }
	        Arrays.sort(pairs,new Comparator<Pair>(){
	
	            
	            public int compare(Pair a,Pair b){
	                return a.second==b.second?(a.first-b.first):a.second-b.second;
	            }
	        });
	        for(int i=0;i<inputs.length;i++){
	             System.out.print(pairs[i].first);
	            if(i!=inputs.length-1)
	                           System.out.print(",");
	        }
	       
	    }
	    public static int lon(int x){
	        // 数学 logn/log2
	        return (int)(Math.log(x)/ Math.log(2));
	    }
	
	    static class Pair{
	        public int first;
	          public  int second;
	        public Pair(int a,int b){
	            first=a;
	            second=b;
	        }
	    }
	}
	
* 	7-21 连续数列

给定一个整数数组，找出总和最大的连续数列，并返回总和。


进阶：如果你已经实现复杂度为 O(n) 的解法，尝试使用更为精妙的分治法求解。


输入格式:
数组nums

输出格式:
连续子数组的最大和

输入样例:
在这里给出一组输入。例如：

-2,1,-3,4,-1,2,1,-5,4
输出样例:
在这里给出相应的输出。例如：

6

last 最大

// 动态规划，轻松拿下，或者就用last
		
		import java.util.*;
	
	public class Main{
	
	    public static void main(String [] aaa){
	        Scanner sc=new Scanner(System.in);
	        int[] inputs=Arrays.stream(sc.nextLine().split(","))
	            .mapToInt(Integer::parseInt).toArray();
	    
	        int max= inputs[0];
	        int last=  inputs[0];
	        for(int i=1;i<inputs.length;i++){
	            if(last>0){
	                last=last+inputs[i];
	            }else{
	                last=inputs[i];
	            }
	            max=Math.max(max,last);
	        }
	        System.out.print(max);
	    }
	}
