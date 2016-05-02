// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of join has the attribute ReadOnly
es5id: 15.4.4.5_A6.3
description: Checking if varying the length property fails
includes: [propertyHelper.js]
---*/

//CHECK#1
var x = Array.prototype.join.length;
verifyNotWritable(Array.prototype.join, "length", null, Infinity);
if (Array.prototype.join.length !== x) {
  $ERROR('#1: x = Array.prototype.join.length; Array.prototype.join.length = Infinity; Array.prototype.join.length === x. Actual: ' + (Array.prototype.join.length));
}
