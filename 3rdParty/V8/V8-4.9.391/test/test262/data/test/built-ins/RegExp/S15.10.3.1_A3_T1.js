// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If pattern and flags are defined, then
    call the RegExp constructor (15.10.4.1), passing it the pattern and flags arguments and return the object constructed by that constructor
es5id: 15.10.3.1_A3_T1
description: R is "d+" and instance is RegExp(R,"i")
---*/

var __re = "d+";
var __instance = RegExp(__re, "i");

//CHECK#1
if (__instance.constructor !== RegExp) {
	$ERROR('#1: __re = "d+"; __instance = RegExp(__re, "i"); __instance.constructor === RegExp. Actual: ' + (__instance.constructor));
}

//CHECK#2
if (__instance.source !== __re) {
	$ERROR('#2: __re = "d+"; __instance = RegExp(__re, "i"); __instance.source === __re. Actual: '+ (__instance.source));
}
