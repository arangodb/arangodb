// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Since Error prototype object is not function it has no [[Construct]] method
es5id: 15.11.4_A4
description: Checking if creating "new Error.prototype" fails
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
try {
	var __instance = new Error.prototype;
	$ERROR('#1: "var __instance = new Error.prototype" lead to throwing exception');
} catch (e) {
    if (e instanceof Test262Error) throw e;
}
//
//////////////////////////////////////////////////////////////////////////////
