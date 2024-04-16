### 1、两数之和

给定一个整数数组 nums 和一个整数目标值 target，请你在该数组中找出 和为目标值 target 的那 两个 整数，并返回它们的数组下标。
你可以假设每种输入只会对应一个答案。但是，数组中同一个元素在答案里不能重复出现。

> 题解与考察点：考察HashMap，但是使用时候，注意一遍遍历搞定，先判断满足与否，再如HashMap，不考察频次，就不要用get(ket) > 0 ,用containKey来处理，value用来存储下标

	    public int[] twoSum(int[] nums, int target) {
	
	        HashMap<Integer, Integer> map = new HashMap<>();
	        for (int i = 0; i < nums.length; i++) {
	            if (map.containsKey(target - nums[i])) {
	                int[] ret = {i, map.get(target - nums[i])};
	                return ret;
	            } else {
	                map.put(nums[i], i);
	            }
	        }
	        return null;
	    }

### 2、两数相加：类似的还有大数相加、大数相乘，这里考察的是链表，也可以考察数组

 给你两个 非空 的链表，表示两个非负的整数。它们每位数字都是按照 逆序 的方式存储的，并且每个节点只能存储 一位 数字。
请你将两个数相加，并以相同形式返回一个表示和的链表。 你可以假设除了数字 0 之外，这两个数都不会以 0 开头。

> 题解与考察点  ：考察双指针，注意需要处理一方指针走完，另一方没走完的 情况，这种题目可以守住一方，一方结束再处理另一方， 再一方的时候，另一个是否走完也要兼顾，另外next的处理 current也要注意。不要怕麻烦，要定义变量，清晰比简洁重要

	 public ListNode addTwoNumbers(ListNode l1, ListNode l2) {
        ListNode ret = l1;
        int plus = 0;
        int v = 0;
        while (true) {
            v = plus + l1.val +( l2 == null ? 0 : l2.val);
            l1.val = v % 10;
            plus = v >= 10 ? 1 : 0;
            l2 = l2 == null ? null : l2.next;
            if (l1.next == null) {
                break;
            }
            l1 = l1.next;
        }
        if (l2 != null) {
            l1.next = l2;
            while (true) {
                v = plus + l2.val;
                l2.val = v % 10;
                plus = v >= 10 ? 1 : 0;
                l1 = l2;
                if (l2.next == null)
                    break;
                l2 = l2.next;
            }
        }
        if (plus > 0) l1.next = new ListNode(1);
        return ret;
    }
    
###     3 无重复字符串的最长子串：最值问题，dp

给定一个字符串 s ，请你找出其中不含有重复字符的 最长子串 的长度。

> 题解与考察点 看到最值问题，首先想到动态规划，**最长子串这种遍历的最经典**

	  public int lengthOfLongestSubstring(String s) {
	
	        if (s == null || s.isEmpty())
	            return 0;
		//   以谁为结尾的最长子串
	        int[] dp = new int[s.length()];
	        dp[0] = 1;
	        int max = 1;
	        for (int i = 1; i < s.length(); i++) {
	            int j;
	            char c = s.charAt(i);
	            for (j = 0; j < dp[i - 1]; j++) {
	                if (s.charAt(i - j - 1) == c) {
	                    break;
	                }
	            }
	            dp[i] = j + 1;
	            max = Math.max(max, dp[i]);
	        }
	        return max;
	    }
### 4 寻找两个正序数组的中位数	 算法的时间复杂度应该为 O(log (m+n))


给定两个大小分别为 m 和 n 的正序（从小到大）数组 nums1 和 nums2。请你找出并返回这两个正序数组的 中位数 。

> 题解，看到时间复杂度，就可以猜测二分查找， 

如果没有， 限制我们可以用 O(m+n) 的算法解决， 很简单，双指针，哪个往前走。
 
### 5  给你一个字符串 s，找到 s 中最长的回文子串。 最长子串，dp

如果字符串的反序与原始字符串相同，则该字符串称为回文字符串。动态规划 

	   public String longestPalindrome(String s) {
	        if (s == null || s.isEmpty())
	            return null;
	        int[] dp = new int[s.length()]; // i之前最长回文
	        dp[0] = 1;
	        String ret = "" + s.charAt(0);
	        for (int i = 1; i < s.length(); i++) {
	            String a = s.substring(Math.max(0, i - dp[i - 1] - 1), i + 1);
	            String b = s.substring(Math.max(0, i - dp[i - 1]), i + 1);
	            if (isR(a)) {
	                dp[i] = a.length();
	                ret = a;
	            } else if (isR(b)) {
	                dp[i] = b.length();
	                ret = b;
	            } else {
	                dp[i] = dp[i - 1];
	            }
	        }
	        return ret;
	    }
	
	//    是不是回文
	
	    boolean isR(String s) {
	        return new StringBuilder(s).reverse().toString().equals(s);
	    }
	    
	    
### ✔	正则表达式匹配	30.7% Hard：动态规划

给你一个字符串 s 和一个字符规律 p，请你来实现一个支持 '.' 和 '*' 的正则表达式匹配。

'.' 匹配任意单个字符
'*' 匹配零个或多个前面的那一个元素

# ✔	盛最多水的容器 ，主要是题目的理解 **双指针 **?


给定一个长度为 n 的整数数组 height 。有 n 条垂线，第 i 条线的两个端点是 (i, 0) 和 (i, height[i]) 。


其实用递归与分治的的思想考虑更加容易理解 ，或者说 逐步删减，要么用了它，要么就跟他没关系，就是剩余的

双指针，那个小，走动哪个

原理： 最远的两个，保留长的，最大的面积要么是当前短的构建的，要么是用长的跟剩余的构建的。而剩余的多大，每次都可以用当下最长的跟剩余的来比较。

	    public int maxArea(int[] height) {
	
	        //用递归考虑更加合理
	
	        if (height == null || height.length < 2) return 0;
	        int max = 0;
	        for (int i = 0, j = height.length - 1; i < j; ) {
	            max = Math.max(Math.min(height[i], height[j]) * (j - i), max);
	            if (height[i] <= height[j]) {
	                i++;
	            } else {
	                j--;
	            }
	        }
	        return max;
	    }


递归来解释更合理，但是不好写，**会超时**

	    public int maxArea(int[] height) {
	
	        //用递归考虑更加合理
	        if (height == null || height.length < 2) return 0;
	
	        return Math.max(Math.min(height[0], height[height.length - 1]) * (height.length - 1), maxArea(Arrays.copyOfRange(height,
	                height[0] > height[height.length - 1] ? 0 : 1, height[0] > height[height.length - 1] ? height.length - 1 : height.length)));
	    }
	 
##  ✔	[15]三数之和	37.8%	Medium 0.0%  双指针 

> 考察点：双指针，还有就是二分法，拆解问题 ，去重

子问题拆解


给你一个整数数组 nums ，判断是否存在三元组 [nums[i], nums[j], nums[k]] 满足 i != j、i != k 且 j != k ，同时还满足 nums[i] + nums[j] + nums[k] == 0 。 你返回所有和为 0 且不重复的三元组。

三数之和，分解可以了，包含第一个，不包含第一个，包含，同样的双指针 ？三数之和退化成两数之和，加去重

Arrays.asList(nums[j], nums[t])

 最值边界，是否满足，或者说，包含它的二分法是否成立，二分法。子问题拆解
 
 Leecode如果是返回是List，一定要返回空的List，而不是null
 
	 
	  //    顺序可以变，可以先排序，顺序不能变，就可以递归
	    public List<List<Integer>> threeSum(int[] nums) {
	// 递归，两数之和 +三数之和
	        List<List<Integer>> list = new ArrayList<>();
	        Arrays.sort(nums);
	        if (nums[0] > 0) return list;
	        if (nums[nums.length - 1] < 0) return list;
	        //  注意相等最多保留两个相等的
	
	        for (int i = 0; i < nums.length; i++) {
	            ArrayList<ArrayList<Integer>> tmp = new ArrayList<>();
	            int v = nums[i];
	            int remain = -v;
	            //        后面的一定是前面的子集
	            if (i > 0 && nums[i] == nums[i - 1]) continue;
	            if (nums[i] > 0) break;
	            for (int j = i + 1, t = nums.length - 1; j < t; ) {
	                if (nums[j] + nums[t] > remain) {
	                    t--;
	                } else if (nums[j] + nums[t] < remain) {
	                    j++;
	                } else {
	                    tmp.add(new ArrayList<>(Arrays.asList(nums[j], nums[t])));
	                    t--;
	                    //  必须只有一个可能
	                    while (j < nums.length - 1 && nums[j] == nums[j + 1]) {
	                        j++;
	                    }
	                    j++;
	                }
	            }
	            if (tmp.size() > 0) {
	                for (ArrayList<Integer> inte : tmp) {
	                    inte.add(v);
	                }
	                list.addAll(tmp);
	            }
	        }
	        return list;
	    }

## ✔	[17]电话号码的字母组合 


给定一个仅包含数字 2-9 的字符串，返回所有它能表示的字母组合。答案可以按 任意顺序 返回。

给出数字到字母的映射如下（与电话按键相同）。注意 1 不对应任何字母。


> 题解 这个很容易想到递归，也可以用循环 , 字符串的 扩张问题

	 public List<String> letterCombinations(String digits) {
	        List<String> list = new ArrayList<>();
	        if (digits == null || digits.length() == 0)
	            return list;
	        for (int i = 0; i < digits.length(); i++) {
	            if (list.isEmpty()) {
	                String p = getByC(digits.charAt(i));
	                for (int j = 0; j < p.length(); j++)
	                    list.add(String.valueOf(p.charAt(j)));
	            } else {
	                List<String> tmp = new ArrayList<>();
	                for (String item : list) {
	                    String p = getByC(digits.charAt(i));
	                    for (int j = 0; j < p.length(); j++)
	                        tmp.add(item + p.charAt(j));
	                }
	                list = tmp;
	            }
	        }
	
	        return list;
	    }
	
	    private String getByC(char c) {
	        switch (c) {
	            case '2':
	                return "abc";
	            case '3':
	                return "def";
	            case '4':
	                return "ghi";
	            case '5':
	                return "jkl";
	            case '6':
	                return "mno";
	            case '7':
	                return "pqrs";
	            case '8':
	                return "tuv";
	            case '9':
	                return "wzyx";
	            default:
	                return "";
	        }
	    }

## ✔	[19]删除链表的倒数第 N 个结点	47.8%	Medium	0.0%
给你一个链表，删除链表的倒数第 n 个结点，并且返回链表的头结点。

>  考察知识，链表，链表长度 这里需要注意的是倒数第几个，不一定求长度，只要把我好间隔短就可以。

	 public ListNode removeNthFromEnd(ListNode head, int n) {
	        ListNode tmp = head;
	        int count = 0;
	        if (n == 0 || head == null) return head;
	        ListNode lastN = null;
	        ListNode pre = head;
	        while (tmp != null) {
	            if (n == count + 1) {
	                lastN = head;
	            } else if (lastN != null) {
	                pre = lastN;
	                lastN = lastN.next;
	            }
	            count++;
	            tmp = tmp.next;
	        }
	        if (lastN == head)
	            return lastN.next;
	
	        if (pre != null) {
	            pre.next = lastN.next;
	        }
	        return head;
	    }
    
    
##     ✔	[20]有效的括号	43.9%	Easy	0.0%


给定一个只包括 '('，')'，'{'，'}'，'['，']' 的字符串 s ，判断字符串是否有效。

有效字符串需满足：

堆栈: switch的写法不要怕麻烦，要把每个的break写上，不然有问题

	public boolean isValid(String s) {
	
	        Stack<Character> stack = new Stack<>();
	        for (int i = 0; i < s.length(); i++) {
	            char v = s.charAt(i);
	            switch (v) {
	                case ')':
	                    if (stack.isEmpty() || stack.pop().charValue() != '(') return false;
	                    break;
	                case '}':
	                    if (stack.isEmpty() || stack.pop().charValue()  != '{') return false;
	                    break;
	                case ']':
	                    if (stack.isEmpty() || stack.pop().charValue()  != '[') return false;
	                    break;
	                default:
	                    stack.push(v);
	            }
	        }
	        return stack.isEmpty();
	    }
	    
	    
## ✔	[21]合并两个有序链表	66.4%	Easy	0.0%	    
将两个升序链表合并为一个新的 升序 链表并返回。新链表是通过拼接给定的两个链表的所有节点组成的。

> 典型的双指针，主要是边界处理 ,处理好 返回head current next
> 
	   public ListNode mergeTwoLists(ListNode list1, ListNode list2) {
	        if (list1 == null) return list2;
	        if (list2 == null) return list1;
	        ListNode ret = null, head = null;
	        while (list1 != null && list2 != null) {
	            if (list1.val > list2.val) {
	                if (ret == null) {
	                    ret = list2;
	                    head = ret;
	                    list2 = list2.next;
	                } else {
	                    ret.next = list2;
	                    list2 = list2.next;
	                    ret = ret.next;
	                }
	            } else {
	                if (ret == null) {
	                    ret = list1;
	                    head = ret;
	                    list1 = list1.next;
	                } else {
	                    ret.next = list1;
	                    list1 = list1.next;
	                    ret = ret.next;
	                }
	            }
	        }
	        if (list1 != null) {
	            ret.next = list1;
	        }
	        if (list2 != null) {
	            ret.next = list2;
	        }
	        return head;
	    }
	
## ✔	[22]括号生成	77.7%	Medium	0.0%

数字 n 代表生成括号的对数，请你设计一个函数，用于能够生成所有可能的并且 有效的 括号组合。

> 题解，虚拟的二叉树，回溯，剪枝，左半部分括号优先原则


	  public List<String> generateParenthesis(int n) {
        return generateParenthesis(n, n);
    }

    public List<String> generateParenthesis(int left, int right) {
        if (right < left) return null;
        ArrayList<String> list = new ArrayList<>();
        if (left == 0) {
            return Collections.singletonList(String.join("", Collections.nCopies(right, ")")));
        }
        List<String> leftL = generateParenthesis(left - 1, right);
        if (leftL != null && leftL.size() > 0) {
            for (String item : leftL) {
                list.add("(" + item);
            }
        }
        List<String> rightL = generateParenthesis(left, right - 1);
        if (rightL != null && rightL.size() > 0) {
            for (String item : rightL) {
                list.add(")" + item);
            }
        }
        return list;
    }
    
##     ✔	[23]合并 K 个升序链表	59.4%	Hard	0.0%

你一个链表数组，每个链表都已经按升序排列。 请你将所有链表合并到一个升序链表中，返回合并后的链表。

> 堆得属性，或者说优先队列

	 public ListNode mergeKLists(ListNode[] lists) {
	        //堆？
	        if (lists == null || lists.length == 0) return null;
	
	
	        PriorityQueue<ListNode> queue = new PriorityQueue<ListNode>(new Comparator<ListNode>() {
	            @Override
	            public int compare(ListNode listNode, ListNode t1) {
	                return listNode.val - t1.val;
	            }
	        });
	
	        for (ListNode item : lists) {
	            queue.add(item);
	        }
	        ListNode head = null;
	        ListNode current = null;
	        while (!queue.isEmpty()) {
	            if (head == null) {
	                current = queue.poll();
	                head = current;
	            } else {
	                current.next = queue.poll();
	                current = current.next;
	            }
	            if (current.next != null) {
	                queue.add(current.next);
	            }
	        }
	        return head;
	    }
	    
## 	  ✔	[31]下一个排列	39.1%	Medium	0.0%

整数数组的一个 排列 就是将其所有成员以序列或线性顺序排列。

例如，arr = [1,2,3] ，以下这些都可以视作 arr 的排列：[1,2,3]、[1,3,2]、[3,1,2]、[2,3,1] 。
整数数组的 下一个排列 是指其整数的下一个字典序更大的排列。更正式地，如果数组的所有排列根据其字典顺序从小到大排列在一个容器中，那么数组的 下一个排列 就是在这个有序容器中排在它后面的那个排列。如果不存在下一个更大的排列，那么这个数组必须重排为字典序最小的排列（即，其元素按升序排列）。

例如，arr = [1,2,3] 的下一个排列是 [1,3,2] 。
类似地，arr = [2,3,1] 的下一个排列是 [3,1,2] 。
而 arr = [3,2,1] 的下一个排列是 [1,2,3] ，因为 [3,2,1] 不存在一个字典序更大的排列。
给你一个整数数组 nums ，找出 nums 的下一个排列。

必须 原地 修改，只允许使用额外常数空间。


排列组合数 ,单纯就是数学

	
	public void nextPermutation(int[] nums) {
	
	        for (int i = nums.length - 1; i > 0; i--) {
	            if (nums[i] <= nums[i - 1]) {
	                if (i == 1) {
	                    Arrays.sort(nums);
	                }
	                continue;
	            } else {
	                // 找到了
	                for (int k = nums.length - 1; k >= i; k--) {
	                    if (nums[k] > nums[i - 1]) {
	                        int t = nums[k];
	                        nums[k] = nums[i - 1];
	                        nums[i - 1] = t;
	                        break;
	                    }
	                }
	                for (int p = i, j = nums.length - 1; p < j; p++, j--) {
	                    int t = nums[p];
	                    nums[p] = nums[j];
	                    nums[j] = t;
	                }
	                break;
	            }
	        }

# ✔	[32]最长有效括号	37.8%	Hard	0.0%


给你一个只包含 '(' 和 ')' 的字符串，找出最长有效（格式正确且连续）括号子串的长度。

> 题解 ：动态规划，以它为结尾的最长

	 //    动态规划 ？
	    public int longestValidParentheses(String s) {
	
	        if (s == null || s.length() == 0) return 0;
	        int[] dp = new int[s.length()];
	        dp[0] = 0;
	        int max = 0;
	        for (int i = 1; i < s.length(); i++) {
	            if (s.charAt(i) == '(') {
	                dp[i] = 0;
	            } else {
	                if (i - dp[i - 1] - 1 >= 0 && s.charAt(i - dp[i - 1] - 1) == '(') {
	                    dp[i] = dp[i - 1] + 2 + (i - dp[i - 1] - 1 > 0 ? dp[i - dp[i - 1] - 2] : 0);
	                } else dp[i] = 0;
	            }
	            max = Math.max(max, dp[i]);
	        }
	
	        return max;
	    }
	    
## ✔	[33]搜索旋转排序数组	44.2%	Medium	0.0%

整数数组 nums 按升序排列，数组中的值 互不相同 。

在传递给函数之前，nums 在预先未知的某个下标 k（0 <= k < nums.length）上进行了 旋转，使数组变为 [nums[k], nums[k+1], ..., nums[n-1], nums[0], nums[1], ..., nums[k-1]]（下标 从 0 开始 计数）。例如， [0,1,2,4,5,6,7] 在下标 3 处经旋转后可能变为 [4,5,6,7,0,1,2] 。

给你 旋转后 的数组 nums 和一个整数 target ，如果 nums 中存在这个目标值 target ，则返回它的下标，否则返回 -1 。

你必须设计一个时间复杂度为 O(log n) 的算法解决此问题。

> 题解：* O(log n)， 其实就是告诉我们使用二分法 

	 public int search(int[] nums, int target) {
	
	        if (nums == null || nums.length == 0) return -1;
	
	        int left = 0;
	        int right = nums.length - 1;
	        boolean inLeft = target >= nums[0];
	
	        //  也可能没旋转
	        while (left <= right) {
	            int middle = (left + right) / 2;
	            if (nums[middle] > target) {
	                if (inLeft) right = middle - 1;
	                else {
	                    if (nums[middle] >= nums[0])
	                        left = middle + 1;
	                    else right = middle - 1;
	                }
	            } else if (nums[middle] < target) {
	                if (inLeft) {
	                    if (nums[middle] >= nums[0])
	                        left = middle + 1;
	                    else
	                        right = middle - 1;
	                } else left = middle + 1;
	            } else {
	                return middle;
	            }
	        }
	        return -1;
	    }
	

## 	    ✔	[34]在排序数组中查找元素的第一个和最后一个位置	43.3%	Medium	0.0%

你必须设计并实现时间复杂度为 O(log n) 的算法解决此问题，二分法，找最左边的。

> 二分查找 ，区分左右边界的时候，需要注意，左边界，**left+right /2 ，又边界，需要，left+right+1 /2 **

	    public int[] searchRange(int[] nums, int target) {
	        if (nums == null || nums.length == 0) return new int[]{-1, -1};
	        int middle = 0;
	        int finA = -1, finB = -1;
	        for (int left = 0, right = nums.length - 1; left <= right; ) {
	            middle = (left + right) / 2;
	            if (nums[middle] >= target) {
	                if (right == middle) {
	                    break;
	                }
	                right = middle;
	            } else {
	                left = middle + 1;
	            }
	        }
	        if (nums[middle] == target)
	            finA = middle;

	        for (int left = 0, right = nums.length - 1; left <= right; ) {
	            // 找右侧，中间值要偏右，不能偏左
	            middle = (left + right + 1) / 2;
	            if (nums[middle] > target) {
	                right = middle - 1;
	            } else {
	                if (left == middle) {
	                    break;
	                }
	                left = middle;
	            }
	        }
	
	        if (nums[middle] == target)
	            finB = middle;
	
	        return new int[]{finA, finB};
	    }
	
	
## ✔	[48]旋转图像	76.0%	Medium	0.0%

给定一个 n × n 的二维矩阵 matrix 表示一个图像。请你将图像顺时针旋转 90 度。


> 存在公式 ，转换公式
[i][j] =[j][n-1-i] ，矩形，只有四步


        //    [i][j] [j][n-j-1]
        int n=matrix.length;
        for (int i = 0; i < matrix.length / 2; i++) {
            for (int j = i; j < matrix.length - i - 1; j++) {
                int tmp1 = matrix[j][n - 1 - i];
                matrix[j][n - 1 - i] = matrix[i][j];
                int tmp2 = matrix[n - 1 - i][n - 1 - j];
                matrix[n - 1 - i][n - 1 - j] = tmp1;
                tmp1 = matrix[n - 1 - j][i];
                matrix[n - 1 - j][i] = tmp2;
                matrix[i][j] = tmp1;
            }
        }
    }
    
##     不同的二叉搜素数  

动态规划


    public int numTrees(int n) {
        if (n == 0) return 0;
        if (n == 1) return 1;
        int sum = 0;
        int[] dp = new int[n + 1];
        dp[0] = 0;
        dp[1] = 1;
        for (int i = 2; i <= n; i++) {
            dp[i] = 0;
            for (int j = 1; j <= i; j++) {
                dp[i] += Math.max(1, dp[j - 1]) * Math.max(1, dp[i - j]);
            }

        }
        return dp[n];
    }
    
    
    
##     搜索二叉树的判断

> 找最左边，左右边

	 public boolean isValidBST(TreeNode root) {
	        if (root == null) return true;
	
	        if (root.left != null && findMax(root.left) >= root.val) {
	            return false;
	        }
	        if (root.right != null && findMin(root.right) <= root.val) {
	            return false;
	        }
	        return isValidBST(root.left) && isValidBST(root.right);
	    }
	
	    int findMax(TreeNode root) {
	        while (root != null) {
	            if (root.right != null) root = root.right;
	            else return root.val;
	        } return -1;
	    }
	
	    int findMin(TreeNode root) {
	        while (root != null) {
	            if (root.left != null) root = root.left;
	            else return root.val;
	        } return -1;
	    }