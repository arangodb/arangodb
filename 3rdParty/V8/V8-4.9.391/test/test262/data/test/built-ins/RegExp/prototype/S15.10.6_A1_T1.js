// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The value of the internal [[Prototype]] property of the RegExp prototype
    object is the Object prototype
es5id: 15.10.6_A1_T1
description: Checking Object.prototype.isPrototypeOf(RegExp.prototype)
---*/

//CHECK#1
if (Object.prototype.isPrototypeOf(RegExp.prototype) !== true) {
	$ERROR('#1: Object.prototype.isPrototypeOf(RegExp.prototype) === true');
}
