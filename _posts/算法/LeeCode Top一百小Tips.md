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

# ✔	盛最多水的容器 ，主要是题目的理解 双指针或者单调栈

给定一个长度为 n 的整数数组 height 。有 n 条垂线，第 i 条线的两个端点是 (i, 0) 和 (i, height[i]) 。
单调栈，一定是某个为准，哪个边最高。一定是以某个高度为准。**双向，左边，右边，要么左边，要么右边**

dp[i] 表示i结尾最大值，