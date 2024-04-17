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
### 4 寻找两个正序数组的中位数	 算法的时间复杂度应该为 O(log (m+n))   左边占一半 。。


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
	    
	    
### ✔	正则表达式匹配	30.7% Hard：动态规划 不太容易理解

给你一个字符串 s 和一个字符规律 p，请你来实现一个支持 '.' 和 '*' 的正则表达式匹配。

'.' 匹配任意单个字符
'*' 匹配零个或多个前面的那一个元素
'.' 和 '*'
s 只包含从 a-z 的小写字母，p是模式 才包含'.' 和 '*'


要好好理解正则，跟零个的意思，零个、一个、多个 

	   public boolean isMatch(String s, String p) {
	
	        boolean[][] dp = new boolean[s.length() + 1][p.length() + 1];
	        dp[0][0] = true;
	
	        for (int i = 1; i <= p.length(); i++) {
	            dp[0][i] = ((i - 2 >= 0 && dp[0][i - 2]) || dp[0][i - 1]) && (p.charAt(i - 1) == '*');
	        }
	        for (int i = 1; i <= s.length(); i++) {
	            dp[i][0] = false;
	        }
	
	        for (int i = 1; i <= s.length(); i++) {
	            for (int j = 1; j <= p.length(); j++) {
	                if (p.charAt(j - 1) == '.') {
	                    dp[i][j] = dp[i - 1][j - 1];
	                } else if (p.charAt(j - 1) == '*') {
	//                    0个或者多个前面的字符
	                    dp[i][j] = (j - 2 >= 0 && dp[i][j - 2])
	                            || dp[i][j - 1]
	                            || (dp[i - 1][j] && (s.charAt(i - 1) == p.charAt(j - 2)
	                            || p.charAt(j - 2) == '.')); //注意多个的条件要表述清楚，多个的时候，是怎么样的 
	                } else {
	                    dp[i][j] = s.charAt(i - 1) == p.charAt(j - 1) && dp[i - 1][j - 1];
	                }
	            }
	        }
	        return dp[s.length()][p.length()];
	    }
	    

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

### 组合总数 回溯

包含的跟不包含

	public static List<List<Integer>> combinationSum(int[] candidates, int target) {
	        List<List<Integer>> list = new ArrayList<>();
	        int count = 1;
	        if(candidates==null || candidates.length==0) return list;
	        while (target >= count * candidates[0]) {
	            if (target == count * candidates[0]) {
	                Integer[] ar = new Integer[count];
	                Arrays.fill(ar, candidates[0]);
	                list.add(Arrays.asList(ar));
	            } else {
	                List<List<Integer>> tmp = combinationSum(Arrays.copyOfRange(candidates, 1, candidates.length), target - count * candidates[0]);
	                Integer[] ar = new Integer[count];
	                Arrays.fill(ar, candidates[0]);
	                List<Integer> tL = Arrays.asList(ar);
	                for (List<Integer> item : tmp) {
	                    ArrayList<Integer> c = new ArrayList<>(item);
	                    c.addAll(tL);
	                    list.add(c);
	                }
	            }
	            count++;
	        }
	        List<List<Integer>> tmp2 = combinationSum(Arrays.copyOfRange(candidates, 1, candidates.length), target);
	        if (!tmp2.isEmpty()) {
	            list.addAll(tmp2);
	        }
	        return list;
	    }

## ✔	[42]接雨水	63.4%	Hard	0.0%

考察的单调栈 ，左右两侧最大值中的最小

	public int trap(int[] height) {
	        Stack<Integer> stack = new Stack<>();
	        int[] left = new int[height.length];
	        int[] right = new int[height.length];
	        stack.push(height[0]);
	        left[0] = height[0];
	        for (int i = 1; i < height.length - 1; i++) {
	            if (height[i] > stack.peek()) {
	                while (!stack.isEmpty() && stack.peek() < height[i]) {
	                    stack.pop();
	                }
	                if (stack.isEmpty())
	                    stack.push(height[i]);
	            }
	            left[i] = stack.peek();
	        }
	        stack.clear();
	        right[height.length - 1] = height[height.length - 1];
	        stack.push(height[height.length - 1]);
	        for (int i = height.length - 2; i > 0; i--) {
	            if (height[i] > stack.peek()) {
	                while (!stack.isEmpty() && stack.peek() < height[i]) {
	                    stack.pop();
	                }
	                if (stack.isEmpty())
	                    stack.push(height[i]);
	            }
	            right[i] = stack.peek();
	        }
	        int ret = 0;
	        for (int i = 1; i < height.length - 1; i++) {
	            ret += Math.min(left[i], right[i]) - height[i];
	        }
	
	        return ret;
	    }

 

	给定一个不含重复数字的数组 nums ，返回其 所有可能的全排列 。你可以 按任意顺序 返回答案。
	
 
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
 
    
## ✔	[46]全排列	79.1%	Medium	0.0%

回溯

	public List<List<Integer>> permute(int[] nums) {
		        //    回溯
		        List<Integer> list = new ArrayList<>();
		        for (int item : nums) {
		            list.add(item);
		        }
		        return permute1(list);
		    }
		
		    public List<List<Integer>> permute1(List<Integer> input) {
		        //    回溯
		        List<List<Integer>> list = new ArrayList<>();
		
		        if (input == null || input.size() == 0) {
		            return list;
		        }
		
		        for (int i = 0; i < input.size(); i++) {
		            ArrayList<Integer> tmp = (new ArrayList<Integer>(input));
		            tmp.remove(i);
		            List<List<Integer>> list2 = permute1(tmp);
		            if (list2.size() == 0) {
		                list.add(Collections.singletonList(input.get(i)));
		            } else {
		                for (List<Integer> item : list2) {
		                    ArrayList<Integer> integers = new ArrayList<>(item);
		                    integers.add(0, input.get(i));
		                    list.add(integers);
		                }
		            }
		        }
		        return list;
		    }
		    
	插入法  找到前面所有的，后面的插入，前面的每个排列，插入后，都有多个，擦，这个想法简单多了
	
## 	 ✔	[49]字母异位词分组	68.0%	Medium	0.0%

给你一个字符串数组，请你将 字母异位词 组合在一起。可以按任意顺序返回结果列表。

> 不考虑去重，就用list，考虑就用set
	
	 public List<List<String>> groupAnagrams(String[] strs) {
	
	        HashMap<String, List<String>> hashMap = new HashMap<>();
	
	        for (int i = 0; i < strs.length; i++) {
	            char[] p = strs[i].toCharArray();
	            Arrays.sort(p);
	            String s = new String(p);
	            if (hashMap.containsKey(s)) {
	                hashMap.get(s).add(strs[i]);
	            } else {
	                ArrayList<String> strings = new ArrayList<>();
	                strings.add(strs[i]);
	                hashMap.put(s, strings);
	            }
	        }
	
	        List<List<String>> ret = new ArrayList<>();
	        for (Map.Entry<String, List<String>> stringHashSetEntry : hashMap.entrySet()) {
	            ret.add(stringHashSetEntry.getValue());
	        }
	        return ret;
	    }
	 
### ✔	[53]最大子数组和	55.3%	Medium	0.0% :

给你一个整数数组 nums ，请你找出一个具有最大和的连续子数组（子数组最少包含一个元素），返回其最大和。
子数组 是数组中的一个连续部分。

> 连续、最大、子区间 都是动态规划的字眼，用动态规划

    public int maxSubArray(int[] nums) {
        //动态规划
        int[] dp = new int[nums.length];
        //  以A为结尾的最大连续

        dp[0] = nums[0];
        int max = dp[0];
        for (int i = 1; i < nums.length; i++) {
            dp[i] = dp[i - 1] > 0 ? dp[i - 1] + nums[i] : nums[i];
            max = Math.max(dp[i], max);
        }
        return max;
    }
    
    
##     ✔	[55]跳跃游戏	43.3%	Medium  贪心 	0.0%

给你一个非负整数数组 nums ，你最初位于数组的 第一个下标 。数组中的每个元素代表你在该位置可以跳跃的最大长度。


> 题解：找到第一个比他跳的远的，找不到，就有问题 ** 贪心算法**

	 public boolean canJump(int[] nums) {
	        // 触达的最长距离
	        if (nums.length == 1) return true;
	        int i = 0;
	        while (i < nums.length) {
	            int nextI = i + nums[i];
	            if (nextI == i) return false;
	            if (nextI >= nums.length - 1)
	                return true;
	            for (int j = i + 1; j <= nextI; j++) {
	                if (j + nums[j] > nextI) {
	                    i = j;
	                    nextI = 0;
	                    break;
	                }
	            }
	            if (nextI > 0) return false;
	        }
	        return true;
	    }
	    
### 	    ✔	[56]合并区间	49.9%	Medium  数组排序，无论一维数组，还是二维数组。	0.0%

以数组 intervals 表示若干个区间的集合，其中单个区间为 intervals[i] = [starti, endi] 。请你合并所有重叠的区间，并返回 一个不重叠的区间数组，该数组需恰好覆盖输入中的所有区间 。

> 题解 :先排序，再合并
> 

    开发技巧，List写法
    
        List<int[]> merged = new ArrayList<int[]>();

    排序的写法   Arrays.sort Comparator

	    public int[][] merge(int[][] intervals) {
	
	        //  数组先排序
	
	        Arrays.sort(intervals, new Comparator<int[]>() {
	            @Override
	            public int compare(int[] ints, int[] t1) {
	                return ints[0]-t1[0];
	            }
	        });
	        int[][] ret = new int[intervals.length][];
	        ret[0] = intervals[0];
	        int current = 0;
	        for (int i = 1; i < intervals.length; i++) {
	            if (intervals[i][0] > ret[current][1]) {
	                ret[current + 1] = intervals[i];
	                current++;
	            } else {
	                ret[current][1] = Math.max(intervals[i][1], ret[current][1]);
	            }
	        }
	        int[][] p = new int[current + 1][];
	        for (int i = 0; i <= current; i++)
	            p[i] = ret[i];
	
	        return p;
	    }
    
###     ✔	[62]不同路径	68.1%	Medium	0.0%
    
 一个机器人位于一个 m x n 网格的左上角 （起始点在下图中标记为 “Start” ）。

机器人每次只能向下或者向右移动一步。机器人试图达到网格的右下角（在下图中标记为 “Finish” ）。

问总共有多少条不同的路径？ 最简单的动态规划


    public int uniquePaths(int m, int n) {
        int[][] dp = new int[m][n];
        dp[0][0] = 1;
        for (int i = 0; i < m; i++) {
            dp[i][0] = 1;
        }
        for (int i = 0; i < n; i++) {
            dp[0][i] = 1;
        }
        for (int i = 1; i < m; i++)
            for (int j = 1; j < n; j++) {
                dp[i][j] = dp[i - 1][j] + dp[i][j - 1];
            }
        return dp[m-1][n-1];
    }
    
    
###     ✔	[64]最小路径和	70.2%	Medium  dp	0.0%

给定一个包含非负整数的 m x n 网格 grid ，请找出一条从左上角到右下角的路径，使得路径上的数字总和为最小。


> 跟上面类似，只是加了权

    public int minPathSum(int[][] grid) {
        int m = grid.length;
        int n = grid[0].length;
        int[][] dp = new int[m][n];
        dp[0][0] = grid[0][0];
        for (int i = 1; i < m; i++) {
            dp[i][0] = dp[i - 1][0] + grid[i][0];
        }
        for (int i = 1; i < n; i++) {
            dp[0][i] = dp[0][i - 1] + grid[0][i];
        }
        for (int i = 1; i < m; i++)
            for (int j = 1; j < n; j++) {
                dp[i][j] = Math.min(dp[i - 1][j], dp[i][j - 1]) + grid[i][j];
            }

        return dp[m - 1][n - 1];
    }
    
##     ✔	[70]爬楼梯	54.5%	Easy	0.0%

假设你正在爬楼梯。需要 n 阶你才能到达楼顶。

每次你可以爬 1 或 2 个台阶。你有多少种不同的方法可以爬到楼顶呢？

动态规划？ 斐波那契?

    public int climbStairs(int n) {

        if (n == 1) return 1;
        if (n == 2) return 2;
        int before = 1, after = 2;
        for (int i = 3; i <= n; i++) {
            int tmp = before;
            before = after;
            after = before + tmp;
        }
        return after;
    }
    
##     ✔	[72]编辑距离	62.8%	Medium  经典动态规划  对于结尾的操作放在最后	0.0%


**对于结尾的操作放在最后**，参考跳格子


给你两个单词 word1 和 word2， 请返回将 word1 转换成 word2 所使用的最少操作数 。

你可以对一个单词进行如下三种操作：

插入一个字符
删除一个字符
替换一个字符


**1 逆序性质等价**  处理相等的时候，

不等的时候  **2 结尾等价**

 **先后处理顺序不影响结果，所以对于**最后一个字符**的处理放在最后，对第一个的结尾操作处理有三种，删除，替换  、插入。**
  
 其实就是从三个中找到最小的，最后不满足，在结尾的处理一定分三种：固定一个串 
  
*   如果是替换：那么一定是最后一个跟最后一个字符，替换成P最后一个，前面的都是从 i-1 替换成 j-1的代价
*   如果是删除：那么一定是从是从,  i-1到j的代价，最后一个的操作
*   如果是插入：对于上面的插入， 插入后 i+1=j 那其实等效于i变成j-1，逆序等价性质。
  
 顶住对于结尾的处理，动态规划，只要覆盖所有可能性就可以，然后计算最终的值，也比较像回溯。
 
    
> 题解 以及理解 很容易写出来，但是不容易理解，紧紧握住最后一步操作在最后【先后顺序不影响结果】，一共三种情况，
>  从后往前，跟从前往后是一样的，得出的结论是一样的，**从直观上理解，先砍掉一致的地方，不影响后面的不一致的匹配**

 
	 public int minDistance(String word1, String word2) {
	        if (word1 == null || word1.length() == 0) return word2 == null ? 0 : word2.length();
	        if (word2 == null || word2.length() == 0) return word1.length();
	
	        int[][] dp = new int[word1.length() + 1][word2.length() + 1];
	//        i j,i 跟j 匹配需要的编辑距离
	        dp[0][0] = 0;
	        for (int i = 1; i <= word1.length(); i++) {
	            dp[i][0] = i;
	        }
	        for (int i = 1; i <= word2.length(); i++) {
	            dp[0][i] = i;
	        }
	
	        // 转换成对最后一个操作等价，这个怎么证明的
	        for (int i = 1; i <= word1.length(); i++) {
	            for (int j = 1; j <= word2.length(); j++) {
	                //  id的长度从小到大 ， 最后一步是插入、删除、还是替换，对最后一个操作
	                if (word1.charAt(i - 1) == word2.charAt(j - 1)) {
	                    dp[i][j] = dp[i - 1][j - 1];
	                } else {
	                    dp[i][j] = Math.min(dp[i - 1][j - 1], Math.min(dp[i][j - 1], dp[i - 1][j])) + 1;
	                }
	            }
	        }
	
	        return dp[word1.length()][word2.length()];
	    }


### ✔	[75]颜色分类	61.1%	Medium	0.0%

给定一个包含红色、白色和蓝色、共 n 个元素的数组 nums ，原地对它们进行排序，使得相同颜色的元素相邻，并按照红色、白色、蓝色顺序排列。

    public void sortColors(int[] nums) {
//        双指针

        int rIndex = -1;
        int wIndex = -1;
        for (int i = 0; i < nums.length; i++) {
            switch (nums[i]) {
                case 0:
                    if (wIndex >= 0) {
                        nums[i] = nums[wIndex];
                        if (rIndex >= 0) {
                            nums[wIndex] = 1;
                            wIndex++;
                            nums[rIndex] = 0;
                            rIndex++;
                        } else {
                            nums[wIndex] = 0;
                            wIndex++;
                        }
                    } else if (rIndex >= 0) {
                        nums[i] = nums[rIndex];
                        nums[rIndex] = 0;
                        rIndex++;
                    }
                    break;
                case 1:
                    if (wIndex >= 0) {
                        nums[i] = nums[wIndex];
                        nums[wIndex] = 1;
                        if (rIndex < 0) {
                            rIndex = wIndex;
                        }
                        wIndex++;
                    } else {
                        if (rIndex < 0) {
                            rIndex = i;
                        }
                    }
                    break;
                case 2:
                    if (wIndex < 0)
                        wIndex = i;
                    break;
                default:
                    break;
            }
        }
    }

## ✔	[76]最小覆盖子串	45.6  z%	Hard	0.0%

给你一个字符串 s 、一个字符串 t 。返回 s 中涵盖 t 所有字符的最小子串。如果 s 中不存在涵盖 t 所有字符的子串，则返回空字符串 "" 。

> 题解  纯哈希表
  
 
	  public String minWindow(String t, String s) {
	//  找到第一个中的所有字符
	        //  找到第一个中的所有字符
	        HashMap<Character, Integer> set = new HashMap<>();
	        for (int i = 0; i < s.length(); i++) {
	            set.put(s.charAt(i), set.getOrDefault(s.charAt(i), 0) + 1);
	        }
	        int[] dp = new int[t.length()];
	        int valideBegin = -1;
	        HashMap<Character, Integer> hashMap = new HashMap<>();
	        boolean sati = false;
	        for (int i = 0; i < t.length(); i++) {
	            if (set.containsKey(t.charAt(i))) {
	                if (valideBegin == -1)
	                    valideBegin = i;
	                hashMap.put(t.charAt(i), hashMap.getOrDefault(t.charAt(i), 0) + 1);
	                while (!set.containsKey(t.charAt(valideBegin))
	                        || hashMap.getOrDefault(t.charAt(valideBegin), 0) > set.getOrDefault(t.charAt(valideBegin), 0)) {
	                    if (hashMap.getOrDefault(t.charAt(valideBegin), 0) > set.getOrDefault(t.charAt(valideBegin), 0)) {
	                        hashMap.put(t.charAt(valideBegin), hashMap.get(t.charAt(valideBegin)) - 1);
	                    }
	                    valideBegin++;
	                }
	                if (hashMap.size() == set.size()) {
	                    int count = 0;
	                    if (!sati) {
	                        for (Character item : set.keySet()) {
	                            if (hashMap.getOrDefault(item, 0) >= set.get(item))
	                                count++;
	                        }
	                        if (count == set.size()) sati = true;
	                    }
	                    dp[i] = sati ? i - valideBegin + 1 : 0;
	                }
	            } else {
	                dp[i] = 0;
	            }
	        }
	        int index = -1;
	        for (int i = 0; i < t.length(); i++) {
	            if (dp[i] > 0) {
	                if (index < 0) index = i;
	                else index = dp[i] < dp[index] ? i : index;
	            }
	        }
	
	        return index >= 0 ? t.substring(index - dp[index] + 1, index + 1) : "";
	    }

## ✔	[78]子集	81.3%	Medium	0.0%

给你一个整数数组 nums ，数组中的元素 互不相同 。返回该数组所有可能的子集（幂集）

> 回溯,注意对空list的处理 

    public List<List<Integer>> subsets(int[] nums) {
        List<List<Integer>> list = new ArrayList<>();

        for (int i = 0; i < nums.length; i++) {
            if (list.size() == 0) {
                list.add(Collections.singletonList(nums[i]));
                list.add(new ArrayList<>());
            } else {
                List<List<Integer>> tmp = new ArrayList<>();
                for (List<Integer> item : list) {
                    List<Integer> list1 = new ArrayList<>(item);
                    list1.add(nums[i]);
                    tmp.add(list1);
                }
                list.addAll(tmp);
            }
        }

        return list;
    }
    
    
####     ?	[79]单词搜索	46.8%	Medium	0.0%

给定一个 m x n 二维字符网格 board 和一个字符串单词 word 。如果 word 存在于网格中，返回 true ；否则，返回 false 。

> 考察点 回溯 深度优先遍历 visit，岛屿问题

	  public boolean exist(char[][] board, String word) {
	        for (int i = 0; i < board.length; i++) {
	            for (int j = 0; j < board[0].length; j++) {
	                boolean[][] visit = new boolean[board.length][board[0].length];
	                if (exist(board, word, visit, 0, i, j))
	                    return true;
	            }
	        }
	        return false;
	    }
	
	    public boolean exist(char[][] board, String word, boolean[][] visit, int start, int i, int j) {
	
	        if (start >= word.length())
	            return true;
	
	        if (board[i][j] == word.charAt(start)) {
	            if (start == word.length() - 1)
	                return true;
	
	            visit[i][j] = true;
	            boolean ret = false;
	
	            if (i + 1 < board.length && !visit[i + 1][j]) {
	                ret = exist(board, word, visit, start + 1, i + 1, j);
	                if (ret) return true;
	                visit[i + 1][j] = false;
	            }
	
	            if (j + 1 < board[0].length && !visit[i][j + 1]) {
	                ret = exist(board, word, visit, start + 1, i, j + 1);
	                if (ret) return true;
	                visit[i][j + 1] = false;
	            }
	
	            if (i - 1 >= 0 && !visit[i - 1][j]) {
	                ret = exist(board, word, visit, start + 1, i - 1, j);
	                if (ret) return true;
	                visit[i - 1][j] = false;
	            }
	
	            if (j - 1 >= 0 && !visit[i][j - 1]) {
	                ret = exist(board, word, visit, start + 1, i, j - 1);
	                if (ret) return true;
	                visit[i][j - 1] = false;
	            }
	            visit[i][j] = false;
	            return false;
	        } else {
	            return false;
	        }
	    }


## ✔	[84]柱状图中最大的矩形	45.6%	Hard	0.0%

单调栈 


public int largestRectangleArea(int[] heights) {

        // 左边比他大的值
        Stack<Integer> stack = new Stack<>();
        stack.push(0);
        int[] left = new int[heights.length];
        left[0] = 0;
        for (int i = 1; i < heights.length; i++) {
            //  找到左边第一个比他小的
            if (heights[i] > heights[stack.peek()]) {
                left[i] = i;
            } else {
                while (!stack.isEmpty() && heights[stack.peek()] >= heights[i])
                    stack.pop();
                if (!stack.isEmpty())
                    left[i] = stack.peek() + 1;
                else left[i] = 0;
            }
            stack.push(i);
        }
        stack.clear();
        stack.push(heights.length - 1);
        int[] right = new int[heights.length];
        right[heights.length - 1] = heights.length - 1;
        //   找右边第一个比他小的
        for (int i = heights.length - 2; i >= 0; i--) {
            if (heights[i] > heights[stack.peek()]) {
                right[i] = i;
            } else {
                while (!stack.isEmpty() && heights[stack.peek()] >= heights[i])
                    stack.pop();
                if (!stack.isEmpty()) right[i] = stack.peek() - 1;
                else right[i] = heights.length - 1;
            }
            stack.push(i);
        }
        int max = heights[0];
        for (int i = 0; i < heights.length; i++) {
            max = Math.max(max, heights[i] * (right[i] - left[i] + 1));
        }
        return max;
    }
    
##    ✔	[94]二叉树的中序遍历	76.7%	Easy	0.0% 

左，中 右  

深度到底，出，如果右边是null，继续出，如果不是null，新数入栈，新回合
    
	 public List<Integer> inorderTraversal(TreeNode root) {
	        List<Integer> list = new ArrayList<>();
	        if(root ==null)return list;
	        Stack<TreeNode> stack = new Stack<>();
	        stack.push(root);
	        while (!stack.isEmpty()) {
	            if (stack.peek().left != null) {
	                stack.push(stack.peek().left);
	            } else {
	                // 一直往回归
	                while (!stack.isEmpty()) {
	                    TreeNode node = stack.pop();
	                    list.add(node.val);
	                    if (node.right != null) {
	                        // 新树插入
	                        stack.push(node.right);
	                        break;
	                    }
	                }
	            }
	        }
	        return list;
	

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
 
## ✔	[101]对称二叉树	60.1%	Easy	0.0%

> 递归，或者用队列


	public boolean isSymmetric(TreeNode root) {
	
	        if (root == null) return true;
	        Deque<TreeNode> queue = new LinkedList<>();
	        queue.add(root);
	        while (!queue.isEmpty()) {
	            TreeNode node = queue.poll();
	            if (!queue.isEmpty()) {
	                TreeNode node2 = queue.poll();
	                if (node2.val != node.val)
	                    return false;
	                if (node.left != null && node2.right != null) {
	                    queue.add(node.left);
	                    queue.add(node2.right);
	                } else if (node.left == null && node2.right == null) {
	                } else {
	                    return false;
	                }
	
	                if (node.right != null && node2.left != null) {
	                    queue.add(node.right);
	                    queue.add(node2.left);
	                } else if (node.right == null && node2.left == null) {
	                } else {
	                    return false;
	                }
	
	            } else {
	                if (node == root && node.left == null && node.right == null) {
	                    return true;
	                }
	                if (node == root && node.left != null && node.right != null) {
	                    queue.add(node.left);
	                    queue.add(node.right);
	                } else
	                    return false;
	            }
	        }
	        return true;
	    }
	    
	    
 递归 擦这种巧劲
	    
	    
	    
    public boolean isSymmetric(TreeNode root) {

        if (root == null) return true;

        return isSymmetric(root.left, root.right);

    }

    boolean isSymmetric(TreeNode left, TreeNode right) {

        if (left == null && right == null) return true;

        if (left != null && right != null) {
            return left.val == right.val && isSymmetric(left.left, right.right) && isSymmetric(left.right, right.left);
        }

        return false;
    }
    
    
##     ✔	[102]二叉树的层序遍历	67.1%	Medium	0.0%


> 题解  借助层的数量
 
	 public List<List<Integer>> levelOrder(TreeNode root) {
	        List<List<Integer>> list = new ArrayList<>();
	
	        if (root == null) return list;
	
	        Deque<TreeNode> deque = new LinkedList<>();
	
	        deque.add(root);
	        int size = 1;
	        List<Integer> tmp = new ArrayList<>();
	        while (!deque.isEmpty()) {
	            TreeNode node = deque.poll();
	            tmp.add(node.val);
	            size--;
	            if (node.left != null) {
	                deque.add(node.left);
	            }
	            if (node.right != null) {
	                deque.add(node.right);
	            }
	            if (size == 0) {
	                list.add(tmp);
	                tmp = new ArrayList<>();
	                size = deque.size();
	            }
	        }
	        return list;
	    }
	    
### > ✔	[104]二叉树的最大深度	77.5%	Easy	0.0%

keyi	    
Ke	    
>     用递归
    
    public int maxDepth(TreeNode root) {
        if (root == null) return 0;

        return 1 + Math.max(maxDepth(root.left), maxDepth(root.right));
    }
    
    
    
##     ✔	[105]从前序与中序遍历序列构造二叉树	71.6%	Medium	0.0%

只有利用中序才能区分谁左，谁右
    
        public TreeNode buildTree(int[] preorder, int[] inorder) {

        if (preorder == null || preorder.length == 0) return null;
        if (preorder.length == 1) return new TreeNode(preorder[0]);
        TreeNode root = new TreeNode(preorder[0]);
        int index = 0;
        while (index < preorder.length && preorder[0] != inorder[index])
            index++;
        root.left = buildTree(Arrays.copyOfRange(preorder, 1, index + 1), Arrays.copyOfRange(inorder, 0, index));
        root.right = buildTree(Arrays.copyOfRange(preorder, index + 1, preorder.length), Arrays.copyOfRange(inorder, index + 1, preorder.length));
        return root;
    }