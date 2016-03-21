// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of shift has the attribute ReadOnly
es5id: 15.4.4.9_A5.3
description: Checking if varying the length property fails
includes: [propertyHelper.js]
---*/

//CHECK#1
var x = Array.prototype.shift.length;
verifyNotWritable(Array.prototype.shift, "length", null, Infinity);
if (Array.prototype.shift.length !== x) {
  $ERROR('#1: x = Array.prototype.shift.length; Array.prototype.shift.length = Infinity; Array.prototype.shift.length === x. Actual: ' + (Array.prototype.shift.length));
}
