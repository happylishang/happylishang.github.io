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
    
    
##     ✔	[114]二叉树展开为链表	73.6%	Medium	0.0%




## 买卖股票时机

多次 找峰值

> 
	    public int maxProfit(int[] prices) {
	        int value = 0;
	        int buy = -1;
	        int sell = -1;
	
	//        只能买一次,卖一次
	        for (int i = 1; i < prices.length; i++) {
	            if (prices[i] > prices[i - 1]) {
	                if (buy == -1) {
	                    buy = prices[i - 1];
	                }
	            } else if (prices[i] < prices[i - 1]) {
	                if (buy > 0) {
	                    value += prices[i - 1] - buy;
	                    buy = -1;
	                }
	            }
	        }
	        return value;
	    }
	    
	    
> 如果只有一次 找i前边的最小值。

	    public int maxProfit(int[] prices) {
	        int value = 0;
	        int lowest = -1;
	        lowest = prices[0];
	//        只能买一次,卖一次
	        for (int i = 1; i < prices.length; i++) {
	            value = Math.max(value,prices[i]-lowest);
	            lowest = Math.min(prices[i], lowest);
	        }
	        return value;
	    }
	    
## 	  ✔	[114]二叉树展开为链表	73.6%  前序 中序 后续	Medium	0.0%

  
		      public void flatten(TreeNode root) {
	        if (root == null) return;
	
	        TreeNode pre = root;
	
	        while (pre != null) {
	            if (pre.right != null) {
	                if (pre.left != null) {
	                    findLeftLast(pre.left).right = pre.right;
	                    pre.right = pre.left;
	                    pre.left = null;
	                }
	            } else {
	                pre.right = pre.left;
	                pre.left = null;
	            }
	            pre = pre.right;
	        }
	    }
	
	    TreeNode findLeftLast(TreeNode root) {
	//        注意while
	        while (root != null && root.right != null)
	            root = root.right;
	        return root;
	    }
	    
 我们不再是打印根节点，而是利用一个全局变量 pre 修改树的指针，一般要通过后续，因为，先序不能获得后处理的Node，要按照顺序
	     
	    
		    private TreeNode pre = null;
		
			public void flatten(TreeNode root) {
		    if (root == null)
		        return;
		        <!--后续可以安排 左右顺序-->
		    flatten(root.right);
		    flatten(root.left);
		    root.right = pre;//先被处理的一定在前面，后续不会再处理root的孩子
		    root.left = null;
		    pre = root;
		}

 

## 	    ✔	[124]二叉树中的最大路径和	45.5%	Hard	0.0%

> 题解  二分，包含A的左侧大，右侧大，
	    
		     public int maxPathSum(TreeNode root) {
	        if (root == null)
	            return 0;
	        if (root.left == null && root.right == null)
	            return root.val;
	        if (root.left == null) {
	            return Math.max(root.val + findMaxR(root), maxPathSum(root.right));
	        } else if (root.right == null) {
	            return Math.max(findMaxL(root) + root.val, maxPathSum(root.left));
	        } else {
	            return Math.max(findMaxL(root) + root.val + findMaxR(root), Math.max(maxPathSum(root.left), maxPathSum(root.right)));
	        }
	    }
	
	    int findMaxL(TreeNode root) {
	        // 包含root的最大
	        if (root == null || root.left == null) return 0;
	        return Math.max(0, root.left.val + Math.max(findMaxL(root.left), findMaxR(root.left)));
	
	    }
	
	    int findMaxR(TreeNode root) {
	        if (root == null || root.right == null) return 0;
	        return Math.max(0, root.right.val + Math.max(findMaxL(root.right), findMaxR(root.right)));
	    }
	    
**树的递归，经常用全局变量，表示pre   **  


## ✔	[128]最长连续序列	51.9%	Medium	0.0%最、连续序列  动态规划


> 哈希表 左右区间扩展，中间不处理

	  public int longestConsecutive(int[] nums) {
	        if (nums == null || nums.length == 0) return 0;
	        HashMap<Integer, Integer> hashMap = new HashMap<>();
	        // 并查集？
	        int max = 0;
	        for (int i = 0; i < nums.length; i++) {
	            if (!hashMap.containsKey(nums[i])) {
	                int size = 0;
	                if (hashMap.containsKey(nums[i] - 1) && hashMap.containsKey(nums[i] + 1)) {
	                    size += hashMap.get(nums[i] - 1);
	                    size += hashMap.get(nums[i] + 1);
	                    size++;
	                    hashMap.put(nums[i], size);
	                    hashMap.put(nums[i] + hashMap.get(nums[i] + 1), size);
	                    hashMap.put(nums[i] - hashMap.get(nums[i] - 1), size);
	                } else if (hashMap.containsKey(nums[i] - 1)) {
	                    size += hashMap.get(nums[i] - 1);
	                    size++;
	                    hashMap.put(nums[i], size);
	                    hashMap.put(nums[i] - hashMap.get(nums[i] - 1), size);
	                } else if (hashMap.containsKey(nums[i] + 1)) {
	                    size += hashMap.get(nums[i] + 1);
	                    size++;
	                    hashMap.put(nums[i], size);
	                    hashMap.put(nums[i] + hashMap.get(nums[i] + 1), size);
	                } else {
	                    size++;
	                    hashMap.put(nums[i], 1);
	                }
	                max = Math.max(max, size);
	            }
	        }
	
	        return max;
	    }
	    
	    


### ✔	[136]只出现一次的数字	73.5%	Easy	0.0%

> 题解 两次，可以疑惑运算


	    public int singleNumber(int[] nums) {
	//        两次
	//        位运算
	        int ret = 0;
	        for (int i = 0; i < nums.length; i++) {
	            ret ^= nums[i];
	        }
	        return ret;
	    }
	    





## ✔	[141]环形链表	52.3%	Easy  入口判断	0.0%

> 判断链表入口：快慢指针

	
	    public boolean hasCycle(ListNode head) {
        if (head == null || head.next == null || head.next.next == null)
            return false;
        ListNode slow = head.next;
        ListNode fast = slow.next;
        while (slow != fast) {
            if (fast.next == null || fast.next.next == null)
                return false;
            fast = fast.next.next;
            slow = slow.next;
        }
        return true;
    }
    
### 	    ✔	[142]环形链表 II	58.8%	Medium  	0.0%

找入口  给定一个链表的头节点 head ，返回链表开始入环的第一个节点。 如果链表无环，则返回 null。

> 题解 ：找到合并除，然后慢指针一个从头开始一定碰上[慢指针不可能跑一圈]

证明 2(a+b)= a+n(b+c)+b   a=(n-1)(b+c) + c 


	    public ListNode detectCycle(ListNode head) {
	        if (head == null || head.next == null || head.next.next == null)
	            return null;
	        ListNode slow = head.next;
	        ListNode fast = slow.next;
	        while (slow != fast) {
	            if (fast.next == null || fast.next.next == null)
	                return null;
	            fast = fast.next.next;
	            slow = slow.next;
	        }
	        ListNode start = head;
	        while (start != slow) {
	            slow = slow.next;
	            start = start.next;
	        }
	        return start;
	    }

注意快慢指针的写法，可以主动让其先前进一次，条件可改



## ?	[139]单词拆分	55.4%	Medium  拆分 匹配，动态规划	0.0%

> <!--题解-->动态规划

	 public boolean wordBreak(String s, List<String> wordDict) {
	        boolean ret = false;
	        boolean[][] dp = new boolean[s.length()][wordDict.size()];
	
	        dp[0][0] = s.substring(0, 1).equals(wordDict.get(0));
	        for (int i = 1; i < s.length(); i++) {
	            dp[i][0] = s.substring(0, i).equals(wordDict.get(0));
	        }
	        for (int i = 1; i < wordDict.size(); i++) {
	            dp[0][i] = dp[0][i - 1] || s.substring(0, 1).equals(wordDict.get(i));
	        }
	
	        for (int i = 1; i < s.length(); i++) {
	            String sub = s.substring(0, i + 1);
	            for (int j = 1; j < wordDict.size(); j++) {
	                dp[i][j] = false;
	                for (int k = 0; k <= j; k++) {
	//                    边界
	                    dp[i][j] = sub.equals(wordDict.get(k)) || (sub.endsWith(wordDict.get(k)) && dp[i - wordDict.get(k).length()][j]);
	                    if (dp[i][j]) break;
	                }
	            }
	        }
	
	        return dp[s.length() - 1][wordDict.size() - 1];
	    }
	    
二维的动态规划有些浪费，为什么浪费，因为是可以重复，可以重复的的不作为一个维度，全部走一遍
	    
	     //    时间超过限制
    public boolean wordBreak(String s, List<String> wordDict) {
        boolean ret = false;
        boolean[] dp = new boolean[s.length()];
        for (int i = 0; i < wordDict.size(); i++) {
            dp[0] = s.substring(0, 1).equals(wordDict.get(i));
            if (dp[0]) break;
        }

        for (int i = 1; i < s.length(); i++) {
            String sub = s.substring(0, i + 1);
            dp[i] = false;
            for (int j = 0; j < wordDict.size(); j++) {
                dp[i] = sub.equals(wordDict.get(j)) || (sub.endsWith(wordDict.get(j)) && dp[i - wordDict.get(j).length()]);
                if (dp[i]) break;
            }
        }
        return dp[s.length() - 1];
    }
    
    
##     ✔	[146]LRU 缓存	53.6%	Medium	0.0%

请你设计并实现一个满足 LRU (最近最少使用) 缓存 约束的数据结构。
实现 LRUCache 类：
LRUCache(int capacity) 以 正整数 作为容量 capacity 初始化 LRU 缓存
int get(int key) 如果关键字 key 存在于缓存中，则返回关键字的值，否则返回 -1 。
void put(int key, int value) 如果关键字 key 已经存在，则变更其数据值 value ；如果不存在，则向缓存中插入该组 key-value 。如果插入操作导致关键字数量超过 capacity ，则应该 逐出 最久未使用的关键字。
函数 get 和 put 必须以 O(1) 的平均时间复杂度运行。

>  题解 ： get 和 put  O(1)的时间复杂度，get必须用hashMap ，而且要又先后，放前面，那么必须用双向链表，超过了尾部删除。  put很简单放在头部 
 

没规定删除的时间复杂度， 主要是细心 


	class LRUCache {
	
	    Node head;
	    Node tail;
	    int capacity = 0;
	    HashMap<Integer, Node> hashMap = new HashMap<>();
	
	    public LRUCache(int capacity) {
	        this.capacity = capacity;
	    }
	
	    public int get(int key) {
	        if (hashMap.containsKey(key)) {
	            Node node = hashMap.get(key);
	            if (node == tail) {
	                tail = node.pre == null ? node : node.pre;
	            }
	            if (node != head) {
	                node.pre.next = node.next;
	                if (node.next != null) {
	                    node.next.pre = node.pre;
	                }
	                node.next = head;
	                head.pre = node;
	                node.pre = null;
	                head = node;
	            }
	            return node.value;
	        }
	        return -1;
	    }
	
	    public void put(int key, int value) {
	        if (hashMap.containsKey(key)) {
	            get(key);
	            Node node = hashMap.get(key);
	            node.value = value;
	        } else {
	            Node node = new Node(key, value);
	            hashMap.put(key, node);
	            node.next = head;
	            if (head != null)
	                head.pre = node;
	            node.pre = null;
	            head = node;
	            if (tail == null) tail = node;
	            if (hashMap.size() > capacity) {
	                hashMap.remove(new Integer(tail.key));
	                tail.pre.next = null;
	                tail = tail.pre;
	            }
	        }
	
	    }
	
	    static class Node {
	        public Node(int key, int value) {
	            this.key = key;
	            this.value = value;
	        }
	
	        public int key;
	        public int value;
	        public Node pre;
	        public Node next;
	    }
	}

## ✔	[148]排序链表	65.6%	Medium	0.0%
 
	  public ListNode sortList(ListNode head) {
	        if (head == null || head.next == null) return head;
	        //    归并排序？ 二分
	        ListNode current = head;
	        int size = 0;
	        while (current != null) {
	            size++;
	            current = current.next;
	        }
	
	        current = head;
	        int half = 0;
	        while (current != null) {
	            if (half == (size - 1) / 2) {
	                ListNode node = current.next;
	                current.next = null;
	                return merge(sortList(head), sortList(node));
	            }
	            half++;
	            current = current.next;
	        }
	
	        return null;
	
	    }
	//    合并两个有序链表
	
	    ListNode merge(ListNode left, ListNode right) {
	        ListNode head = null;
	        ListNode cur = null;
	        while (left != null && right != null) {
	            if (head == null) {
	                head = left.val > right.val ? right : left;
	                if (left.val > right.val)
	                    right = right.next;
	                else left = left.next;
	                cur = head;
	            } else {
	                cur.next = left.val > right.val ? right : left;
	                if (left.val > right.val)
	                    right = right.next;
	                else left = left.next;
	                cur = cur.next;
	            }
	        }
	        cur.next = left == null ? right : left;
	        return head;
	    }
	 
	 
二分，查找一般的链表，可以用快慢指针。
	
	
	  public ListNode sortList(ListNode head) {
        if (head == null || head.next == null) return head;
        //    归并排序？ 二分 快慢指针
        ListNode fast = head;
        ListNode slow = head;
        while (fast != null) {
            if (fast.next == null || fast.next.next == null) {
                fast = slow.next;
                slow.next = null;
                return merge(sortList(head), sortList(fast));
            }
            slow = slow.next;
            fast = fast.next.next;
        }
        return null;
    }
//    合并两个有序链表

    ListNode merge(ListNode left, ListNode right) {
        ListNode head = null;
        ListNode cur = null;
        while (left != null && right != null) {
            if (head == null) {
                head = left.val > right.val ? right : left;
                if (left.val > right.val)
                    right = right.next;
                else left = left.next;
                cur = head;
            } else {
                cur.next = left.val > right.val ? right : left;
                if (left.val > right.val)
                    right = right.next;
                else left = left.next;
                cur = cur.next;
            }
        }
        cur.next = left == null ? right : left;
        return head;
    }
    
    
##     ✔	[152]乘积最大子数组	43.2%	Medium	0.0%

动态规划 

给你一个整数数组 nums ，请你找出数组中乘积最大的非空连续子数组（该子数组中至少包含一个数字），并返回该子数组所对应的乘积。


### 最小栈


	class MinStack {
	
	    Stack<Integer> stack = new Stack<>();
	    Stack<Integer> stackMini = new Stack<>();
	
	    public MinStack() {
	
	    }
	
	    public void push(int val) {
	        stack.push(val);
	        if (!stackMini.isEmpty() && val > stackMini.peek()) {
	            stackMini.push(stackMini.peek());
	        } else stackMini.push(val);
	    }
	
	    public void pop() {
	        stack.pop();
	        stackMini.pop();
	    }
	
	    public int top() {
	        return stack.peek();
	    }
	
	    public int getMin() {
	        return stackMini.peek();
	    }
	}


## ✔	[152]乘积最大子数组	43.2%	Medium	0.0%

> 题解 找到第一个负值，后面为负值 就除法他
	
	public int maxProduct(int[] nums) {
	        if (nums.length == 1) return nums[0];
	
	        int minN = 1;
	        int max = nums[0];
	        int total = 1;
	        int nIndex = -1;
	        for (int i = 0; i < nums.length; i++) {
	            if (nums[i] == 0) {
	                minN = 1;
	                total = 1;
	                max = Math.max(max, 0);
	                nIndex = -1;
	            } else {
	                total *= nums[i];
	                if (total < 0 && minN == 1) {
	                    minN = total;
	                    nIndex = i;
	                }
	
	                max = total > 0 ? Math.max(max, total) : Math.max(max, nIndex == i ? total : total / minN);
	            }
	        }
	        return max;
	    }
	    
	    

## ✔	[160]相交链表	64.8%	Easy	0.0%

set 看看谁在里面

    public ListNode getIntersectionNode(ListNode headA, ListNode headB) {

//        集合

        HashSet<ListNode> set = new HashSet<>();

        while (headA != null || headB != null) {
            if (set.contains(headA))
                return headA;
            if (headA != null)
                set.add(headA);
            if (set.contains(headB))
                return headB;
            if (headB != null)
                set.add(headB);
            if (headA != null)
                headA = headA.next;
            if (headB != null)
                headB = headB.next;
        }
        return null;
    }
    
    
####     ✔	[169]多数元素	66.3%	Easy	0.0%


给定一个大小为 n 的数组 nums ，返回其中的多数元素。多数元素是指在数组中出现次数 大于 ⌊ n/2 ⌋ 的元素。

> 消消乐的思想，双指针
> 
	public int majorityElement(int[] nums) {
	        if (nums.length == 1) return nums[0];
	        //消消乐
	        int start = 0;
	        int ret;
	        for (int i = 0; i < nums.length - 1; ) {
	            if (nums[i] != nums[i + 1]) {
	                if (start == i) {
	                    i += 2;
	                } else {
	                    nums[i + 1] = nums[start];
	                    nums[i] = nums[start + 1];
	                    i++;
	                }
	                start += 2;
	            } else {
	                i++;
	            }
	        }
	        return nums[nums.length - 1];
	    }
	    

## 打家劫舍：打劫最多，跟之前打劫的值有关系

> 动态规划， 固定最后一步，并且，可以从前面的 推断出来后面的。

你是一个专业的小偷，计划偷窃沿街的房屋。每间房内都藏有一定的现金，影响你偷窃的唯一制约因素就是相邻的房屋装有相互连通的防盗系统，如果两间相邻的房屋在同一晚上被小偷闯入，系统会自动报警。

	
	    public int rob(int[] nums) {
	//        后面的依赖前面的结果
	        int[] dp = new int[nums.length];
	        dp[0] = nums[0];
	        int max = dp[0];
	        for (int i = 1; i < nums.length; i++) {
	            dp[i] = Math.max(dp[i - 1], (i - 2 >= 0 ? dp[i - 2] : 0) + nums[i]);
	            max = Math.max(dp[i], max);
	        }
	        return max;
	    }
	    
	    
## 	    ✔	[206]反转链表	74.3%	Easy	0.0%

> 主要弄清楚 pre current ı

    public ListNode reverseList(ListNode head) {

        if (head == null || head.next == null) return head;
        ListNode current = head;
        ListNode pre = null;

        while (current != null) {
            ListNode tmp = current.next;
            current.next = pre;
            pre = current;
            current = tmp;
        }

        return pre;

    }
    
###     ✔	[200]岛屿数量	60.4%	Medium	0.0%
    
>     题解 ：深度优先+记忆 
> 
	     public int numIslands(char[][] grid) {

        boolean[][] visit = new boolean[grid.length][grid[0].length];

        for (int i = 0; i < grid.length; i++) {
            for (int j = 0; j < grid[0].length; j++) {
                if (grid[i][j] == '1') {
                    visit[i][j] = true;
                    dfs(grid, i, j, visit);
                }
            }
        }

        int max = 0;
        for (int i = 0; i < grid.length; i++) {
            for (int j = 0; j < grid[0].length; j++) {
                max += grid[i][j] - '0';
            }
        }
        return max;
    }

    void dfs(char[][] grid, int i, int j, boolean[][] visit) {
        if (i + 1 < grid.length) {
            if (grid[i + 1][j] == '1' && !visit[i + 1][j]) {
                visit[i + 1][j] = true;
                grid[i + 1][j] = '0';
                dfs(grid, i + 1, j, visit);
            }
        }

        if (j + 1 < grid[0].length) {
            if (grid[i][j + 1] == '1' && !visit[i][j + 1]) {
                visit[i][j + 1] = true;
                grid[i][j + 1] = '0';
                dfs(grid, i, j + 1, visit);
            }
        }

        if (i - 1 >= 0) {
            if (grid[i - 1][j] == '1' && !visit[i - 1][j]) {
                visit[i - 1][j] = true;
                grid[i - 1][j] = '0';
                dfs(grid, i - 1, j, visit);
            }
        }

        if (j - 1 >= 0) {
            if (grid[i][j - 1] == '1' && !visit[i][j - 1]) {
                visit[i][j - 1] = true;
                grid[i][j - 1] = '0';
                dfs(grid, i, j - 1, visit);
            }
        }
    }
    
    
### 	   ✔	[207]课程表	53.9%	Medium	0.0% 
 
 
>  题解考察点 邻接表  图 入度与出度
	 
	   public boolean canFinish(int numCourses, int[][] prerequisites) {
	        //  邻接表 ，有向图
	
	        if (numCourses <= 1 || prerequisites == null || prerequisites.length == 0) {
	            return true;
	        }
	
	        ArrayList<Integer>[] list = new ArrayList[numCourses];
	        int[] indegree = new int[numCourses];
	        ArrayList<Integer> zeros = new ArrayList<>();
	
	        for (int i = 0; i < prerequisites.length; i++) {
	            indegree[prerequisites[i][1]]++;
	            if (list[prerequisites[i][0]] == null) {
	                list[prerequisites[i][0]] = new ArrayList<>();
	            }
	            list[prerequisites[i][0]].add(prerequisites[i][1]);
	        }
	
	        for (int i = 0; i < numCourses; i++) {
	            if (indegree[i] == 0 && list[i] != null && !list[i].isEmpty()) {
	                zeros.add(i);
	            }
	        }
	
	        if (zeros.isEmpty()) return false;
	
	        while (!zeros.isEmpty()) {
	            int p = zeros.remove(0);
	            if (!list[p].isEmpty()) {
	                ArrayList<Integer> list1 = list[p];
	                for (Integer integer : list1) {
	                    indegree[integer]--;
	                    if (indegree[integer] == 0 && list[integer] != null && !list[integer].isEmpty()) {
	                        zeros.add(integer);
	                    }
	                }
	            }
	        }
	
	        for (int i = 0; i < indegree.length; i++) {
	            if (indegree[i] > 0)
	                return false;
	        }
	
	        return true;
	    }
	
### 	✔	[208]实现 Trie (前缀树)	72.0%	Medium	0.0%
	    
> 	  题解：
> 
	    Trie（发音类似 "try"）或者说 前缀树 是一种树形数据结构，用于高效地存储和检索字符串数据集中的键。这一数据结构有相当多的应用情景，例如自动补完和拼写检查
	    
	    
	    
### > 	    ✔	[215]数组中的第K个最大元素	61.5%	Medium	0.0%

> 题解
> 
> 堆 topK 
> 
	     
	      public int findKthLargest(int[] nums, int k) {
	
	        if (nums == null || nums.length == 0 || nums.length < k) return -1;
	
	        PriorityQueue<Integer> priorityQueue = new PriorityQueue<>(k, new Comparator<Integer>() {
	            @Override
	            public int compare(Integer integer, Integer t1) {
	                return integer - t1;
	            }
	        });
	
	        for (int i = 0; i < nums.length; i++) {
	            if (priorityQueue.size() < k) {
	                priorityQueue.add(nums[i]);
	            } else {
	                if (nums[i] > priorityQueue.peek()) {
	                    priorityQueue.poll();
	                    priorityQueue.add(nums[i]);
	                }
	            }
	        }
	        return priorityQueue.peek();
	    }
	    
	    
### 	     	[221]最大正方形	50.3%	Medium	0.0%

> 动态规划  还是数学知识

    public int maximalSquare(char[][] matrix) {

//        dp[i][j] 以ij 结尾的最大边长

        if (matrix == null || matrix.length == 0) return 0;

        int[][] dp = new int[matrix.length][matrix[0].length];

        dp[0][0] = matrix[0][0] - '0';
        int max = dp[0][0];
        for (int i = 1; i < matrix.length; i++) {
            dp[i][0] = matrix[i][0] - '0';
            max = Math.max(max, dp[i][0]);
        }

        for (int i = 1; i < matrix[0].length; i++) {
            dp[0][i] = matrix[0][i] - '0';
            max = Math.max(max, dp[0][i]);
        }

        for (int i = 1; i < matrix.length; i++) {
            for (int j = 1; j < matrix[0].length; j++) {
                if (matrix[i][j] == '0') {
                    dp[i][j] = 0;
                } else {
                    dp[i][j] = Math.min(dp[i][j - 1], Math.min(dp[i - 1][j - 1], dp[i - 1][j])) + 1;
                }
                max = Math.max(max, dp[i][j]);
            }
        }
        return max * max;
    }
    
    
    
##     ✔	[226]翻转二叉树	80.3%	Easy	0.0%


> 递归

	  public TreeNode invertTree(TreeNode root) {
	        if (root == null) return null;
	        TreeNode left = root.left;
	        TreeNode right = root.right;
	        root.left = right;
	        root.right = left;
	        invertTree(left);
	        invertTree(right);
	        return root;
	    }
	    
	    
## 	    ✔	[234]回文链表	54.3%	Easy	0.0%

给你一个单链表的头节点 head ，请你判断该链表是否为回文链表。如果是，返回 true ；否则，返回 false 。

> 题解
> 

进阶：你能否用 O(n) 时间复杂度和 O(1) 空间复杂度解决此题 ：快慢指针 ？

快慢指针 

	 public boolean isPalindrome(ListNode head) {
	        if (head == null || head.next == null) return true;
	
	        ListNode fast = head;
	        ListNode slow = head;
	        ListNode pre = null;
	
	        while (slow != null && fast != null) {
	
	            if (fast.next == null) {
	                fast = slow.next;
	                slow = pre;
	                break;
	            }
	            if (fast.next.next == null) {
	                fast = slow.next;
	                slow.next = pre;
	                break;
	            }
	            fast = fast.next.next;
	            ListNode tmp = slow.next;
	            slow.next = pre;
	            pre = slow;
	            slow = tmp;
	        }
	
	        while (slow != null && fast != null && slow.val == fast.val) {
	            slow = slow.next;
	            fast = fast.next;
	        }
	        return slow == fast && slow == null;
	    }
		    
### 		    ✔	[236]二叉树的最近公共祖先	71.0%	Medium	0.0%

 给定一个二叉树, 找到该树中两个指定节点的最近公共祖先。
 
> 要么左边，要么右边，要么上边
 
	 
	 class Solution {
	    public TreeNode lowestCommonAncestor(TreeNode root, TreeNode p, TreeNode q) {
	        if(root == null || root == p || root == q) return root;
	        TreeNode left = lowestCommonAncestor(root.left, p, q);
	        TreeNode right = lowestCommonAncestor(root.right, p, q);
	        if(left == null) return right;
	        if(right == null) return left;
	        return root;
	    }
	}
	 
### ✔	[238]除自身以外数组的乘积	75.4%	Medium	0.0%


zu
	
> 	左边的乘积 与右边的乘积

		public int[] productExceptSelf(int[] nums) {
		
		
		        //  找0 排除零
		        int[] ret = new int[nums.length];
		        int[] dpA = new int[nums.length];
		        int[] dpB = new int[nums.length];
		        dpA[0] = 1;
		        dpB[nums.length - 1] = 1;
		        for (int i = 1; i < nums.length; i++) {
		            dpA[i] = dpA[i - 1] * nums[i - 1];
		        }
		
		        for (int i = nums.length - 2; i >= 0; i--) {
		            dpB[i] = dpB[i + 1] * nums[i + 1];
		        }
		
		        ret[nums.length - 1] = dpA[nums.length - 1];
		        ret[0] = dpB[0];
		        for (int i = 1; i < nums.length - 1; i++) {
		            ret[i] = dpA[i] * dpB[i];
		        }
		        return ret;
		    }


### ✔	[739]每日温度	68.7%	Medium	0.0%


给定一个整数数组 temperatures ，表示每天的温度，返回一个数组 answer ，其中 answer[i] 是指对于第 i 天，下一个更高温度出现在几天后。如果气温在这之后都不会升高，请在该位置用 0 来代替。

v
>  单调栈
	
    public int[] dailyTemperatures(int[] temperatures) {

        int[] ret = new int[temperatures.length];
        Stack<Integer> stack = new Stack<>();
        ret[temperatures.length - 1] = 0;
        stack.push(temperatures.length - 1);
        for (int i = temperatures.length - 2; i >= 0; i--) {

            while (!stack.isEmpty() && temperatures[stack.peek()] <= temperatures[i]) {
                stack.pop();
            }
            if (stack.isEmpty()) {
                ret[i] = 0;
            } else {
                ret[i] = stack.peek() - i;
            }
            stack.push(i);
        }
        return ret;
    }
    
###     ✔	[647]回文子串	67.3%	Medium	0.0%

	给你一个字符串 s ，请你统计并返回这个字符串中 回文子串 的数目。
	
> 	动态规划

	    public int countSubstrings(String s) {
	        //动态规划？
	        boolean[][] dp = new boolean[s.length()][s.length()];
	//        下标 i-j是不是回文
	        int count = 0;
	        for (int i = 0; i < s.length(); i++) {
	            for (int j = 0; j <= i; j++) {
	                dp[j][i] = s.charAt(i) == s.charAt(j) && (i == j || i == j + 1 || dp[j + 1][i - 1]);
	                count += dp[j][i] ? 1 : 0;
	            }
	        }
	        return count;
	    }
	    
## 	    ###     ✔	[647]回文子串	67.3%	Medium	0.0%

马拉车算法


### ✔	[617]合并二叉树	79.3%	Easy 


> 
>      题解：递归
  
        public TreeNode mergeTrees(TreeNode root1, TreeNode root2) {
        if (root1 == null) return root2;

        root1.val = root1.val + (root2  == null ? 0 : root2.val);

        root1.left = mergeTrees(root1.left, root2 == null ? null : root2.left);
        root1.right = mergeTrees(root1.right, root2 == null ? null : root2.right);

        return root1;
    }
  
###   ✔	[581]最短无序连续子数组	42.2%	Medium	0.0%

给你一个整数数组 nums ，你需要找出一个 连续子数组 ，如果对这个子数组进行升序排序，那么整个数组都会变为升序排序。
    
>     题解 ：直观看就是动态规划 ，但是好像也可以单调栈

	O(N)
	
	public int findUnsortedSubarray(int[] nums) {
	
	        if (nums.length == 1) return 0;
	        Stack<Integer> stack = new Stack<>();
	        stack.push(0);
	        int max = nums[0];
	        int start = -2;
	        int end = 0;
	        for (int i = 1; i < nums.length; i++) {
	            if (nums[i] < max) {
	                end = i;
	            }
	            max = Math.max(max, nums[i]);
	            if (nums[i] < nums[stack.peek()]) {
	                while (!stack.isEmpty() && nums[i] < nums[stack.peek()]) {
	                    stack.pop();
	                }
	                //  找到开头
	                if (stack.isEmpty()) {
	                    start = -1;
	                } else {
	                    start = start == -2 ? stack.peek() : Math.min(stack.peek(), start);
	                }
	            }
	            stack.push(i);
	        }
	
	        return end > 0 ? end - start : 0;
	
	    }
	    
    
排序 +

	    public int findUnsortedSubarray(int[] nums) {
	
	        int[] copy = Arrays.copyOf(nums, nums.length);
	        Arrays.sort(nums);
	
	        int start = -1;
	        int end = -1;
	        for (int i = 0; i < nums.length; i++) {
	            if (nums[i] != copy[i]) {
	                if (start == -1) {
	                    start = i;
	                }
	
	                end = i;
	            }
	        }
	        return end > 0 ? end - start + 1 : 0;
	    }

findUnsortedSubarray  双指针

	  public int findUnsortedSubarray(int[] nums) {
	//        双指针
	        int end = 0;
	        int start = 0;
	        int tmp = nums[0];
	        for (int i = 0; i < nums.length; i++) {
	            if (nums[i] < tmp){
	                end = i;
	            }else{
	                tmp = nums[i];
	            }
	        }
	        tmp = nums[nums.length - 1];
	        for (int i = nums.length - 1; i >= 0; i--) {
	            if (nums[i] > tmp){
	                start = i;
	            }else{
	                tmp = nums[i];
	            }
	        }
	
	        return end > 0 ? end - start + 1 : 0;
	    }

#### > ✔	[560]和为 K 的子数组	44.1%	Medium	0.0%
 
 
 给你一个整数数组 nums 和一个整数 k ，请你统计并返回 该数组中和为 k 的子数组的个数 。
> >  
> 题解

回溯？ 


 
####  ✔	[621]任务调度器	60.1%	Medium	0.0%
 
	  public int leastInterval(char[] tasks, int n) {
	        if (tasks.length == 1) {
	            return 1;
	        }
	        HashMap<Character, Integer> hashMap = new HashMap<>();
	        int max = 0;
	        for (int i = 0; i < tasks.length; i++) {
	            int v = hashMap.getOrDefault(tasks[i], 0) + 1;
	            hashMap.put(tasks[i], v);
	            max = Math.max(max, v);
	        }
	        PriorityQueue<Integer> priorityQueue = new PriorityQueue<>(new Comparator<Integer>() {
	            @Override
	            public int compare(Integer integer, Integer t1) {
	                return t1 - integer;
	            }
	        });
	        int maxCount = 0;
	        for (Integer integer : hashMap.values()) {
	            priorityQueue.add(integer);
	            if (integer == max)
	                maxCount++;
	        }
	
	        int ret = 0;
	        int[] tmp = new int[n + 1];
	        while (!priorityQueue.isEmpty()) {
	            if (priorityQueue.size() >= n + 1) {
	                for (int i = 0; i < n + 1; i++) {
	                    tmp[i] = priorityQueue.poll();
	                    ret++;
	                }
	                for (int i = 0; i < n + 1; i++) {
	                    if (tmp[i] - 1 > 0)
	                        priorityQueue.add(tmp[i] - 1);
	                }
	            } else {
	
	                int cycle = priorityQueue.size() > 0 ? priorityQueue.peek() : 0;
	                return ret + (cycle > 1 ? (cycle - 1) * (n + 1) + maxCount : priorityQueue.size());
	            }
	        }
	
	        return ret;
	    }
	    
####  二叉树的直径	 

可以用深度，但是不好理解



###  ✔	[538]把二叉搜索树转换为累加树	77.3%	Medium	0.0%

> 题解，转换比较拗口

	   public TreeNode convertBST(TreeNode root) {
	        if (root == null) return root;
	        convertBST(root.right);
	        root.val += pre;
	        pre = root.val;
	        convertBST(root.left);
	        return root;
	    }
	    
	    
## 	  ✔	[494]目标和	48.3%	Medium	0.0%

> 回溯


    public int findTargetSumWays(int[] nums, int target) {
        if (nums.length == 1) return nums[0] == target || nums[0] == -target ? (target == 0 ? 2 : 1) : 0;
        return findTargetSumWays(nums, target, 0);

    }

    public int findTargetSumWays(int[] nums, int target, int start) {

        if (start == nums.length - 1)
            return nums[nums.length - 1] == target || nums[nums.length - 1] == -target ? (target == 0 ? 2 : 1) : 0;

        return findTargetSumWays(nums, target - nums[start], start + 1) + findTargetSumWays(nums, target + nums[start], start + 1);

    }

### 汉明距离

两个整数之间的 汉明距离 指的是这两个数字对应二进制位不同的位置的数目。

给你两个整数 x 和 y，计算并返回它们之间的汉明距离。

    public int hammingDistance(int x, int y) {
        x = x ^ y;
        int p = 0;
        while (x != 0) {
            p += x & 1;
            x = x >> 1;
        }
        return p;
    }
    
##     ✔	[448]找到所有数组中消失的数字	65.8%	Easy	0.0%

给你一个含 n 个整数的数组 nums ，其中 nums[i] 在区间 [1, n] 内。请你找出所有在 [1, n] 范围内但没有出现在 nums 中的数字，并以数组的形式返回结果。

跳动表

    public List<Integer> findDisappearedNumbers(int[] nums) {
        List<Integer> list = new ArrayList<>();
        for (int i = 0; i < nums.length; i++) {
            int tmp = nums[i];
            while (nums[tmp - 1] != tmp) {
                int next = nums[tmp - 1];
                nums[tmp - 1] = tmp;
                tmp = next;
            }
        }

        for (int i = 0; i < nums.length; i++) {
            if (nums[i] != i + 1)
                list.add(i + 1);
        }
        return list;
    }
    
    
##     ✔	[438]找到字符串中所有字母异位词	53.5%	Medium	0.0%

给定两个字符串 s 和 p，找到 s 中所有 p 的 异位词 的子串，返回这些子串的起始索引。不考虑答案输出的顺序。

> 题解  哈希表，如果是Interge需要注意 -128 到127 才能==，否则不行
> 
 
	  public List<Integer> findAnagrams(String s, String p) {
	//  哈希表
	        List<Integer> list = new ArrayList<>();
	        if (p.length() > s.length()) return list;
	        HashMap<Character, Integer> hashMap = new HashMap<>();
	        HashMap<Character, Integer> bHashMap = new HashMap<>(p.length());
	
	        for (int i = 0; i < p.length(); i++) {
	            bHashMap.put(p.charAt(i), bHashMap.getOrDefault(p.charAt(i), 0) + 1);
	        }
	
	        int satisfyCount = 0;
	        int l = p.length();
	
	        for (int i = 0; i < s.length(); i++) {
	            char v = s.charAt(i);
	            if (i >= l) {
	                char before = s.charAt(i - l);
	                if (bHashMap.containsKey(before) && bHashMap.get(before).equals(hashMap.getOrDefault(before, 0))) {
	                    satisfyCount--;
	                }
	                hashMap.put(before, hashMap.get(before) - 1);
	
	                if (bHashMap.containsKey(before) && bHashMap.get(before).equals(hashMap.getOrDefault(before, 0))) {
	                    satisfyCount++;
	                }
	            }
	
	            if (bHashMap.containsKey(v) && bHashMap.get(v).equals(hashMap.getOrDefault(v, 0))) {
	                satisfyCount--;
	            }
	            hashMap.put(v, hashMap.getOrDefault(v, 0) + 1);
	
	            if (bHashMap.containsKey(v) && bHashMap.get(v).equals(hashMap.get(v))) {
	                satisfyCount++;
	                if (satisfyCount == bHashMap.size()) {
	                    list.add(i - l + 1);
	                }
	            }
	        }
	        return list;
	    }
	    
## 	    ✔	[437]路径总和 III	47.9%	Medium	0.0%


> 其实很简单，回溯，+递归穿插 奇怪的思维方式 
> 
	 public int pathSum(TreeNode root, long targetSum) {
	        if (root == null) return 0;
	
	        return pathSumB(root, targetSum)
	                + pathSum(root.left, targetSum)
	                + pathSum(root.right, targetSum);
	    }
	
	
	    public int pathSumB(TreeNode root, long targetSum) {
	
	        int ret = 0;
	
	        if (targetSum == root.val) {
	            ret++;
	        }
	        if (root.left != null) {
	            ret += pathSumB(root.left, targetSum - root.val);
	        }
	        if (root.right != null) {
	            ret += pathSumB(root.right, targetSum - root.val);
	        }
	        return ret;
	    }
	    
	    
## 	    ✔	[416]分割等和子集	52.4%	Medium	0.0%

给你一个 只包含正整数 的 非空 数组 nums 。请你判断是否可以将这个数组分割成两个子集，使得两个子集的元素和相等。

> 子集问题，分割问题，二维的动态规划。


	 public boolean canPartition(int[] nums) {
	        if (nums.length < 2) return false;
	        int sum = 0;
	        for (int i = 0; i < nums.length; i++) {
	            sum += nums[i];
	        }
	        if (sum % 2 != 0) return false;
	
	        int half = sum / 2;
	        boolean[][] dp = new boolean[nums.length][half + 1];
	
	        dp[0][0] = true;
	
	        for (int i = 1; i < nums.length; i++) {
	            dp[i][0] = false;
	        }
	        //下标i 及i以前是否可以组成 j
	        for (int i = 1; i <= half; i++) {
	            dp[0][i] = nums[0] == i;
	        }
	
	        //  todo 可以降低空间使用
	        for (int i = 1; i < nums.length; i++) {
	            for (int j = 1; j <= half; j++) {
	                dp[i][j] = (j - nums[i] >= 0 && dp[i - 1][j - nums[i]]) || dp[i - 1][j];
	                if (dp[i][half])
	                    return true;
	            }
	        }
	
	        return false;
	    }
	    
## 	✔	[406]根据身高重建队列	76.4%	Medium	0.0%    
	    
  假设有打乱顺序的一群人站成一个队列，数组 people 表示队列中一些人的属性（不一定按顺序）。每个 people[i] = [hi, ki] 表示第 i 个人的身高为 hi ，前面 正好 有 ki 个身高大于或等于 hi 的人。
  
>   排序 插入 ，有些不明所以
	    
	 public static int[][] reconstructQueue(int[][] people) {

        ArrayList<int[]> list = new ArrayList<>();
        Arrays.sort(people, new Comparator<int[]>() {
            @Override
            public int compare(int[] ints, int[] t1) {
                return ints[0] - t1[0];
            }
        });

        while (list.size() < people.length) {
            for (int i = 0; i < people.length; i++) {
                for (int j = 0; j < people.length; j++) {
                    if (people[j][1] == i) {
                        int count = 0;
                        int index = 0;
                        for (int k = 0; k < list.size(); k++) {
                            index++;
                            if (people[j][0] <= list.get(k)[0]) {
                                count++;
                                if (count == i + 1) {
                                    index = k;
                                    break;
                                }
                            }
                        }
                        if (i == 0)
                            list.add(people[j]);
                        else {
                            list.add(index, people[j]);
                        }
                    }
                }
            }
        }

        return list.toArray(new int[people.length][]);
    }
    



## ✔	[338]比特位计数	78.7%	Easy	0.0%

给你一个整数 n ，对于 0 <= i <= n 中的每个 i ，计算其二进制表示中 1 的个数 ，返回一个长度为 n + 1 的数组 ans 作为答案。

> 动态规划 
> 

    public int[] countBits(int n) {
        int[] dp = new int[n + 1];
        dp[0] = 0;
        for (int i = 1; i <= n; i++) {
            dp[i] = i % 2 == 0 ? dp[i / 2] : (dp[i / 2] + 1);
        }

        return dp;
    }
    

 
 
##      	[337]打家劫舍 III	61.6%	Medium	0.0%


小偷又发现了一个新的可行窃的地区。这个地区只有一个入口，我们称之为 root 。

除了 root 之外，每栋房子有且只有一个“父“房子与之相连。一番侦察之后，聪明的小偷意识到“这个地方的所有房屋的排列类似于一棵二叉树”。 如果 两个直接相连的房子在同一天晚上被打劫 ，房屋将自动报警。

给定二叉树的 root 。返回 在不触动警报的情况下 ，小偷能够盗取的最高金额 。


>  递归时间超限
> 
    public int rob(TreeNode root) {
        if (root == null) return 0;
        return Math.max(root.val + (root.left != null ? (rob(root.left.left) + rob(root.left.right)) : 0
                ) + (root.right != null ? (rob(root.right.left) + rob(root.right.right)) : 0)
                , rob(root.left) + rob(root.right));
    }
    
    
    
>   用哈希表 全局变量存储 动态规划也是 不一定非得二维数组，哈希表也是可以的
> 
 
	  
	   HashMap<TreeNode, Integer> hashMap = new HashMap<TreeNode, Integer>();
	
	    public int rob(TreeNode root) {
	        if (root == null) return 0;
	        int left = 0;
	        int right = 0;
	        if (root.left != null) {
	            left = rob(root.left);
	        }
	        if (root.right != null) {
	            right = rob(root.right);
	        }
	        int t = root.left == null ? 0 : ((root.left.left == null ? 0 : hashMap.get(root.left.left))
	                + (root.left.right == null ? 0 : hashMap.get(root.left.right)));
	
	
	        t += root.right == null ? 0 : ((root.right.left == null ? 0 : hashMap.get(root.right.left))
	                + (root.right.right == null ? 0 : hashMap.get(root.right.right)));
	
	        hashMap.put(root, Math.max(left + right, root.val + t));
	        return hashMap.get(root);
	    }
	    
	    
## 	    ✔	[347]前 K 个高频元素	63.6%	Medium	0.0%

给你一个整数数组 nums 和一个整数 k ，请你返回其中出现频率前 k 高的元素。你可以按 任意顺序 返回答案。

> PripriotyQueue也可以直接class 比如HashMap的entry


	 public static int[] topKFrequent(int[] nums, int k) {
	
	        HashMap<Integer, Integer> hashMap = new HashMap<>();
	        PriorityQueue<Map.Entry<Integer, Integer>> priorityQueue = new PriorityQueue<>(new Comparator<Map.Entry<Integer, Integer>>() {
	            @Override
	            public int compare(Map.Entry<Integer, Integer> integerIntegerEntry, Map.Entry<Integer, Integer> t1) {
	                return integerIntegerEntry.getValue() - t1.getValue();
	            }
	        });
	
	        for (int i = 0; i < nums.length; i++) {
	            hashMap.put(nums[i], hashMap.getOrDefault(nums[i], 0) + 1);
	        }
	
	        for (Map.Entry<Integer, Integer> entry : hashMap.entrySet()) {
	
	            if (priorityQueue.size() < k) {
	                priorityQueue.add(entry);
	            } else if (entry.getValue() > priorityQueue.peek().getValue()) {
	                priorityQueue.poll();
	                priorityQueue.add(entry);
	            }
	        }
	        int[] ret = new int[k];
	        int i = 0;
	
	      while (!priorityQueue.isEmpty())
	            ret[i++] = priorityQueue.poll().getKey();
	        return ret;
	
	    }
	    
## 	    ✔	[394]字符串解码	57.5%	Medium	0.0%


给定一个经过编码的字符串，返回它解码后的字符串。

编码规则为: k[encoded_string]，表示其中方括号内部的 encoded_string 正好重复 k 次。注意 k 保证为正整数。

> 题解 栈

	 public String decodeString(String s) {
	        //  栈
	
	        Stack<Character> stack = new Stack<>();
	        StringBuilder stringBuilder = new StringBuilder();
	        StringBuilder tmp = new StringBuilder();
	        StringBuilder count = new StringBuilder();
	        for (int i = 0; i < s.length(); i++) {
	            if (s.charAt(i) == ']') {
	                char v = 0;
	                tmp = new StringBuilder();
	                count = new StringBuilder();
	                while ((v = stack.pop()) != '[') {
	                    tmp.append(v);
	                }
	
	                while (!stack.isEmpty() && (v = stack.peek()) >= '0' && stack.peek() <= '9') {
	                    stack.pop();
	                    count.append(v);
	                }
	                int p = Integer.parseInt(count.reverse().toString());
	                char[] c = String.join("", Collections.nCopies(p, tmp.reverse().toString())).toCharArray();
	
	                for (char item : c) {
	                    stack.push(item);
	                }
	            } else {
	                stack.push(s.charAt(i));
	            }
	        }
	        while (!stack.isEmpty())
	            stringBuilder.append(stack.pop());
	        return stringBuilder.reverse().toString();
	
	    }



## ✔	[322]零钱兑换	48.1%	Medium	0.0%


给你一个整数数组 coins ，表示不同面额的硬币；以及一个整数 amount ，表示总金额。

计算并返回可以凑成总金额所需的 最少的硬币个数 。如果没有任何一种硬币组合能组成总金额，返回 -1 。-1

> 背包问题，动态规划

	    public int coinChange(int[] coins, int amount) {
	        if (amount == 0) return 0;
	        int[][] dp = new int[coins.length][amount + 1];
	//        dp[i][j] 小标i之前 凑j的数量
	        Arrays.sort(coins);
	        dp[0][0] = 0;
	        for (int i = 1; i < coins.length; i++) {
	            dp[i][0] = 0;
	        }
	        int min = -1;
	
	        for (int i = 1; i <= amount; i++) {
	            dp[0][i] = i % coins[0] == 0 ? i / coins[0] : -1;
	        }
	
	        min = dp[0][amount];
	
	        for (int i = 1; i < coins.length; i++) {
	            for (int j = 1; j <= amount; j++) {
	                dp[i][j] = -1;
	                if (j % coins[i] == 0) {
	                    dp[i][j] = j / coins[i];
	                } else {
	                    int count = j / coins[i];
	                    dp[i][j] = dp[i - 1][j];
	                    while (count > 0) {
	                        int re = dp[i - 1][j - count * coins[i]];
	                        if (re >= 0) {
	                            if (dp[i][j] == -1) dp[i][j] = re + count;
	                            else {
	                                dp[i][j] = Math.min(dp[i][j], re + count);
	                            }
	                        }
	                        count--;
	                    }
	                }
	                if (dp[i][amount] > 0) {
	                    min = min < 0 ? dp[i][amount] : Math.min(min, dp[i][amount]);
	                }
	            }
	        }
	        return min;
	    }
	




    
##  ✔	[239]滑动窗口最大值	48.9%	Hard	0.0%
 
给你一个整数数组 nums，有一个大小为 k 的滑动窗口从数组的最左侧移动到数组的最右侧。你只可以看到在滑动窗口内的 k 个数字。滑动窗口每次只向右移动一位。

 
 给你一个用字符数组 tasks 表示的 CPU 需要执行的任务列表，用字母 A 到 Z 表示，以及一个冷却时间 n。每个周期或时间间隔允许完成一项任务。任务可以按任何顺序完成，但有一个限制：两个 相同种类 的任务之间必须有长度为 n 的冷却时间。
 
 
>  还有单调队列  哈哈哈哈哈 ，除了单调栈，还有单调队列 
> 
 

    //  单调栈 试试
    public int[] maxSlidingWindow(int[] nums, int k) {

        if (nums == null || nums.length == 1) return nums;
        if (k == 1) return nums;

        Deque<Integer> deque = new LinkedList<>();
        deque.add(0);
        int[] ret = new int[nums.length - k + 1];
        // 单个
        for (int i = 1; i < nums.length; i++) {

            while (!deque.isEmpty() && nums[deque.peekLast()] <= nums[i]) {
                deque.pollLast();
            }
            while (!deque.isEmpty() && deque.peekFirst() < i - k + 1) {
                deque.pollFirst();
            }

            if (i >= k - 1) {
                if (deque.isEmpty()) {
                    ret[i - k + 1] = nums[i];
                } else {
                    ret[i - k + 1] = nums[deque.peekFirst()];
                }
            }
            deque.addLast(i);
        }
        return ret;
    }
    
    
##      	[279]完全平方数	66.8%	Medium	0.0%

给你一个整数 n ，返回 和为 n 的完全平方数的最少数量 。
     	
    
>  找到平方数，转换为找零钱
> 动态规划
>
> 
   
     public int numSquares(int n) {
        //    转换找零钱
        //    小于10000
        int[] arr = new int[(int) Math.sqrt(n) + 1];
        for (int i = 0; i < arr.length; i++) {
            arr[i] = (i + 1) * (1 + i);
        }

        return coinChange(arr, n);

    }

    public int coinChange(int[] coins, int amount) {
        if (amount == 0) return 0;
        int[][] dp = new int[coins.length][amount + 1];
		//       dp[i][j] 小标i之前 凑j的数量
        dp[0][0] = 0;
        for (int i = 1; i < coins.length; i++) {
            dp[i][0] = 0;
        }
        int min = -1;

        for (int i = 1; i <= amount; i++) {
            dp[0][i] = i % coins[0] == 0 ? i / coins[0] : -1;
        }

        min = dp[0][amount];

        for (int i = 1; i < coins.length; i++) {
            for (int j = 1; j <= amount; j++) {
                dp[i][j] = -1;
                if (j % coins[i] == 0) {
                    dp[i][j] = j / coins[i];
                } else {
                    int count = j / coins[i];
                    dp[i][j] = dp[i - 1][j];
                    while (count > 0) {
                        int re = dp[i - 1][j - count * coins[i]];
                        if (re >= 0) {
                            if (dp[i][j] == -1) dp[i][j] = re + count;
                            else {
                                dp[i][j] = Math.min(dp[i][j], re + count);
                            }
                        }
                        count--;
                    }
                }
                if (dp[i][amount] > 0) {
                    min = min < 0 ? dp[i][amount] : Math.min(min, dp[i][amount]);
                }
            }
        }
        return min;
    }
    
    
##      	[240]搜索二维矩阵 II	53.7%	Medium	0.0%

编写一个高效的算法来搜索 m x n 矩阵 matrix 中的一个目标值 target每行的元素从左到右升序排列。每列的元素从上到下升序排列。

> 缩减，右边缩减
	
	    public boolean searchMatrix(int[][] matrix, int target) {
	
	        int startR = 0, startC = 0, endR = matrix.length - 1, endC = matrix[0].length - 1;
	
	        while ((endC >= 0 && startR < matrix.length) && (startR < endR || (startC < endC))) {
	            if (matrix[startR][endC] > target) {
	                endC--;
	            } else if (matrix[startR][endC] == target) {
	                return true;
	            } else {
	                startR++;
	            }
	        }
	
	        return startR < matrix.length && endC >= 0 && matrix[startR][endC] == target;
	    }
	    
	    
## 	    ✔	[297]二叉树的序列化与反序列化	59.2%	Hard	0.0%


> 层序遍历  用队列
> 

	public class Codec {
	
	    //  分层遍历
	    // Encodes a tree to a single string.
	    public String serialize(TreeNode root) {
	        if (root == null) return null;
	        Deque<TreeNode> deque = new LinkedList<>();
	        LinkedList<String> list = new LinkedList<>();
	        deque.add(root);
	        while (!deque.isEmpty()) {
	            TreeNode tmp = deque.pop();
	            if (tmp == null) {
	                list.add("null");
	            } else {
	                list.add(String.valueOf(tmp.val));
	                deque.add(tmp.left);
	                deque.add(tmp.right);
	            }
	        }
	        while (!list.isEmpty() && list.getLast().equals("null") ) {
	            list.removeLast();
	        }
	        return Arrays.toString(list.toArray()).replace(" ", "");
	    }
	
	    // Decodes your encoded data to tree.
	    //  计数
	
	    public TreeNode deserialize(String data) {
	
	        if (data == null || data.length() <= 2)
	            return null;
	        String s = data.substring(1, data.length() - 1);
	        String[] values = s.split(",");
	        Deque<TreeNode> deque = new LinkedList<>();
	        TreeNode node = new TreeNode(Integer.parseInt(values[0]));
	        deque.add(node);
	        int index = 0;
	        while (!deque.isEmpty()) {
	            TreeNode tmp = deque.pop();
	            if (index >= values.length - 1)
	                break;
	            index++;
	            tmp.left = values[index].equals("null") ? null : new TreeNode(Integer.parseInt(values[index]));
	            if (tmp.left != null) deque.add(tmp.left);
	            if (index >= values.length - 1)
	                break;
	            index++;
	            tmp.right = values[index].equals("null") ? null : new TreeNode(Integer.parseInt(values[index]));
	            if (tmp.right != null) deque.add(tmp.right);
	        }
	        return node;
	    }
	}




##  	[287]寻找重复数	64.3%	Medium	0.0%

给定一个包含 n + 1 个整数的数组 nums ，其数字都在 [1, n] 范围内（包括 1 和 n），可知至少存在一个重复的整数。

>  
>  普通超时
>   
	  public int findDuplicate(int[] nums) {
	        if (nums.length < 2)
	            return -1;
	
	        for (int i = 0; i < nums.length; i++) {
	            int count = 0;
	            for (int j = i; j < nums.length; j++) {
	                if (nums[i] == nums[j]) count++;
	                if (count >= 2) return nums[i];
	            }
	        }
	        return -1;
	    }

弄了半天跟 [448]找到所有数组中消失的数字  




**都跟num[i]交换，逮着一个交换**

	    
    public int findDuplicate(int[] nums) {
        if (nums.length < 2)
            return -1;

        for (int i = 0; i < nums.length; i++) {
            // 跟nums[i]交换
            while (nums[nums[i] - 1] != nums[i]) {
                int t = nums[nums[i] - 1];
                nums[nums[i] - 1] = nums[i];
                nums[i] = t;
            }
        }
        for (int i = 0; i < nums.length; i++) {
            if (nums[i] != i + 1) return nums[i];
        }

        return -1;
    }
	    
	    
## 	    ✔	[300]最长递增子序列	55.7%	Medium	0.0%

给你一个整数数组 nums ，找到其中最长严格递增子序列的长度。

子序列 是由数组派生而来的序列，删除（或不删除）数组中的元素而不改变其余元素的顺序。例如，[3,6,2,7] 是数组 [0,3,1,6,2,2,7] 的子序列。



> 动态规划  用前面的，不一定用一次。
 
 
     public int lengthOfLIS(int[] nums) {
        int[] dp = new int[nums.length];
        dp[0] = 1;
        int max = 1;
        for (int i = 1; i < nums.length; i++) {
            for (int j = i - 1; j >= 0; j--) {
                if (nums[i] > nums[j]) {
                    dp[i] = Math.max(dp[j], dp[i]);
                }
            }
            dp[i] += 1;
            max = Math.max(max, dp[i]);
        }
        return max;
    }
    
##     ✔	[309]买卖股票的最佳时机含冷冻期	64.7%	Medium	0.0%

> 动态规划，大不了全部遍历

        public int maxProfit(int[] prices) {
        int dp[] = new int[prices.length];
        int max = 0;
        dp[0] = 0;
        for (int i = 1; i < prices.length; i++) {
            if (prices[i] <= prices[i - 1]) {
                dp[i] = dp[i - 1];
            } else {
                for (int j = i - 1; j >= 0; j--) {
                    if (prices[j] < prices[i]) {
                        dp[i] = Math.max(dp[i], (j - 2 >= 0 ? dp[j - 2] : 0) + prices[i] - prices[j]);
                    }
                }
            }
            dp[i] = Math.max(dp[i - 1], dp[i]);
            max = Math.max(dp[i], max);
        }
        return max;
    }

	    
## 	     	[312]戳气球	70.0%	Hard	0.0%
	    

有 n 个气球，编号为0 到 n - 1，每个气球上都标有一个数字，这些数字存在数组 nums 中。



    
##      	[399]除法求值	58.6%	Medium	0.0%


给你一个变量对数组 equations 和一个实数值数组 values 作为已知条件，其中 equations[i] = [Ai, Bi] 和 values[i] 共同表示等式 Ai / Bi = values[i] 。每个 Ai 或 Bi 是一个表示单个变量的字符串。
    


	    
	    