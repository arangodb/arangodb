// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Array.prototype property has the attribute ReadOnly
es5id: 15.4.3.1_A4
description: Checking if varying the Array.prototype property fails
includes: [propertyHelper.js]
---*/

//CHECK#1
var x = Array.prototype;
verifyNotWritable(Array, "prototype", null, 1);
if (Array.prototype !== x) {
	$ERROR('#1: x = Array.prototype; Array.prototype = 1; Array.prototype === x. Actual: ' + (Array.prototype));
}
