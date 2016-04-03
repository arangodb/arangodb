// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If pattern is an object R whose [[Class]] property is "RegExp" and flags
    is undefined, then return R unchanged
es5id: 15.10.3.1_A1_T1
description: R is /x/i and instance is RegExp(R)
---*/

var __re = /x/i;
var __instance = RegExp(__re);
__re.indicator = 1;

//CHECK#1
if (__instance.indicator !== 1) {
	$ERROR('#1: __re = /x/i; __instance = RegExp(__re); __re.indicator = 1; __instance.indicator === 1. Actual: ' + (__instance.indicator));
}
