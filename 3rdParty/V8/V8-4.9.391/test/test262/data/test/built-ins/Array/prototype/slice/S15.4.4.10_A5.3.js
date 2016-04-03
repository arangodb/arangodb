// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of slice has the attribute ReadOnly
es5id: 15.4.4.10_A5.3
description: Checking if varying the length property fails
includes: [propertyHelper.js]
---*/

//CHECK#1
var x = Array.prototype.slice.length;
verifyNotWritable(Array.prototype.slice, "length", null, Infinity);
if (Array.prototype.slice.length !== x) {
  $ERROR('#1: x = Array.prototype.slice.length; Array.prototype.slice.length = Infinity; Array.prototypeslice.length === x. Actual: ' + (Array.prototypeslice.length));
}
