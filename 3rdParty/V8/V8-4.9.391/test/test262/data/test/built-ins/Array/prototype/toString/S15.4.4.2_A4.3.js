// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of toString has the attribute ReadOnly
es5id: 15.4.4.2_A4.3
description: Checking if varying the length property fails
includes: [propertyHelper.js]
---*/

//CHECK#1
var x = Array.prototype.toString.length;
verifyNotWritable(Array.prototype.toString, "length", null, Infinity);
if (Array.prototype.toString.length !== x) {
  $ERROR('#1: x = Array.prototype.toString.length; Array.prototype.toString.length = Infinity; Array.prototype.toString.length === x. Actual: ' + (Array.prototype.toString.length));
}
