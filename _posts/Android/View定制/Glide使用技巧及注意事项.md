---
layout: default
title: Glide使用技巧及注意事项
categories: [Java,RxJava]

---


# 去除白边

ImageView自己要注意填充方式，Glide只负责bitmap的裁减

# 九宫格布局注意ItemDocration

# 动效注意时长

# 自定义内存缓存与磁盘缓存

# 自定义全展开recyclerview

        public int getDecoratedMeasuredWidth(View child) {
            final Rect insets = ((LayoutParams) child.getLayoutParams()).mDecorInsets;
            return child.getMeasuredWidth() + insets.left + insets.right;
        }
        
# Glide限制不能再ImageView上设置Tag Fresco做法不同

为何Fresco没有tag限制，因为fresco全部采用的是非原生imageView，自己封装了Tag类型的实现，并没有覆盖原来的Tag

# Glide优点：能自适应，不用区分gif，fresco必须手动指定宽高，来源等，相比之下Glide使用更加简单，但是Fresco效率较高，尤其是小图，渲染速度快

# Glide加载Gif注意使用.diskCacheStrategy(DiskCacheStrategy.SOURCE)