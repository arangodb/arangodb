// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If pattern is an object R whose [[Class]] property is "RegExp" and flags
    is undefined, then return R unchanged
es5id: 15.10.3.1_A1_T4
description: R is new RegExp() and instance is RegExp(R, void 0)
---*/

var __re = RegExp();
var __instance = RegExp(__re, void 0);
__re.indicator = 1;

//CHECK#1
if (__instance.indicator !== 1) {
	$ERROR('#1: __re = RegExp(); __instance = RegExp(__re, void 0); __re.indicator = 1; __instance.indicator === 1. Actual: ' + (__instance.indicator));
}
