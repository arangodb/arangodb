// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The value of the internal [[Prototype]] property of the Error prototype object is the Object prototype
    object(15.2.3.1)
es5id: 15.11.4_A1
description: Get Error.prototype and compare with Object.prototype
---*/

//////////////////////////////////////////////////////////////////////////////
// CHECK#1
if (!Object.prototype.isPrototypeOf(Error.prototype)) {
	$ERROR('#1: Object.prototype.isPrototypeOf(Error.prototype) return true. Actual: '+Object.prototype.isPrototypeOf(Error.prototype));
}
//
//////////////////////////////////////////////////////////////////////////////
