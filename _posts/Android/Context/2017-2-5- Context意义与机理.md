---
layout: post
title: "Context意义与机理"
description: "Android"
categories: [Android]

---
 
#  Activity是Context ？ Context是什么?? 做什么的就是什么


/**
 * Interface to global information about an application environment.  This is
 * an abstract class whose implementation is provided by
 * the Android system.  It
 * allows access to application-specific resources and classes, as well as
 * up-calls for application-level operations such as launching activities,
 * broadcasting and receiving intents, etc.
 */
 
 场景：前台界面+后台服务+Provider+BroadCast 不同的功能 就是不同的场景