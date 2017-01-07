$(document).ready(function(){

  //Get URL Parameters
  function getUrlParam(name) {
      var re = /<meta.*charset=([^"]+).*?>/i;
      var charset = document.documentElement.innerHTML.match(re)[1];
      var reg = new RegExp("(^|&)" + name + "=([^&]*)(&|$)");
      var r = window.location.search.substr(1).match(reg);
      if (r != null) {
        //console.log(unescape(r[2]));
        //console.log(decodeURIComponent(r[2]));
        if (charset == "utf-8") {
          // alert("document charset utf-8");
          return decodeURIComponent(r[2]);
        }
        else {
          // alert("document charset gbk");
          return unescape(r[2]);
        }
      }
      return null;
  }

  function isInArray(item, array) {
    for (var i = 0; i < array.length; i++) {
      if (array[i] == item) {
        return true;
      }
    }
    return false;
  }

  var g_category = getUrlParam('category');
  //console.log(g_category);

  var g_posts = [];     // 总条目
  var g_pageSize = pageSize;  // 每页显示条目

  //TimeAgo
  var showTimeAgo = function() {
    $("span.time").each(function(){
        $(this).text( $.timeago($(this).attr('date-time')) );
    });
  }

  //Activative
  var showActivedCategory = function(category) {
    if (category == null || category == "" || category == "All") {
      $("li[data='All']").addClass("active");
    } else {
      //console.log(category);
      $("li[data='" + category + "']").addClass("active");
    }
  }

  //Duoshuo
  var showDuoshuoData = function() {
    $.Duoshuo.settings = { shortName: duoshuoShortName };
    $(".post-data").duoshuo();
  }

  /** return the category-filtered posts */
  var getPostsWithCategory = function(data, category) {
    if (category == null || category == "" || category == "All") {
      return data.posts;
    }
    var tmp = data; // this is a reference, if need copy use $.extend
    for (var i = 0; i < tmp.posts.length; i++) {
      if (! isInArray(category, tmp.posts[i].categories)) {
        tmp.posts.splice(i, 1);
        i--;  //while delete, the index should not increase
      }
    }
    return tmp.posts;
  }

  /** sort by desc */
  var sortPostsByPin = function(posts) {
    posts.sort(function(post1, post2) {
      return post2.pin - post1.pin;
    });
    return posts;
  }

  /** return the pagination-filtered posts (num >= 1) */
  var getPostsByPageNum = function(posts, num) {
    if (num <= 0) {
      return posts;
    }
    return posts.slice((num-1)*g_pageSize, num*g_pageSize);
  }

  /** the final render */
  var loadPosts = function(posts) {
    // console.log(posts);
    // baiduTemplateData should be an object
    var baiduTemplateData = { posts: posts };
    var text = baidu.template('post-list', baiduTemplateData);
    //console.log(text);
    $(".article-list").html(text);
    $("#middle-panel").css("margin-top", $("#top-menu").height()+20);
    showTimeAgo();
    showActivedCategory(g_category);
    showDuoshuoData();
  }

  // Main
  $.ajax({
    type: "get",
    url: postfile,
    dataType: "json",
    success: function (data) {
      g_posts = getPostsWithCategory(data, g_category);
      g_posts = sortPostsByPin(g_posts);

      $.jqPaginator('#paginator', {
        totalCounts: g_posts.length,
        pageSize: g_pageSize,
        visiblePages: g_posts.length/g_pageSize >= 10 ? 10 : Math.ceil(g_posts.length/g_pageSize),  // 向上取整
        currentPage: 1,
        onPageChange: function (num, type) {
          // catch exception cause it would stop jqPaginator
          try {
            loadPosts(getPostsByPageNum(g_posts, num));
          } catch (e) {
            console.error(e);
          }
          // scrollTop
          $("html, body").stop().animate({
              scrollTop: 0
          }, 100);
        }
      });
    },
    error: function (XMLHttpRequest, textStatus, errorThrown) {
      alert("Postfile的JSON格式化错误" + errorThrown);
    }
  });



/* Handle Window Scroll Event */
  var WindowScrollDown = function(top) {
    if (top > 80) {
      $("#top-menu").fadeOut(50);
    }
  }
  var WindowScrollUp = function(top) {
    $("#top-menu").fadeIn(50);
  }

/* Event Listening */

  $(".article-list").ready(function(){
    //showTimeAgo();
  });

  var g_top_pos = 0;
  $(window).scroll(function(event){
    var top = $(window).scrollTop();
    //scroll down
    if (top > g_top_pos) {
      //console.log("down");
      WindowScrollDown(top);
    }
    //scroll up
    else {
      //console.log("up");
      WindowScrollUp(top);
    }
    g_top_pos = top;
  });

});
