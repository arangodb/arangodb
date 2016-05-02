// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of toLocaleString has the attribute ReadOnly
es5id: 15.4.4.3_A4.3
description: Checking if varying the length property fails
includes: [propertyHelper.js]
---*/

//CHECK#1
var x = Array.prototype.toLocaleString.length;
verifyNotWritable(Array.prototype.toLocaleString, "length", null, Infinity);
if (Array.prototype.toLocaleString.length !== x) {
  $ERROR('#1: x = Array.prototype.toLocaleString.length; Array.prototype.toLocaleString.length = Infinity; Array.prototype.toLocaleString.length === x. Actual: ' + (Array.prototype.toLocaleString.length));
}
