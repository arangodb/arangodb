// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of pop has the attribute ReadOnly
es5id: 15.4.4.6_A5.3
description: Checking if varying the length property fails
includes: [propertyHelper.js]
---*/

//CHECK#1
var x = Array.prototype.pop.length;
verifyNotWritable(Array.prototype.pop, "length", null, Infinity);
if (Array.prototype.pop.length !== x) {
  $ERROR('#1: x = Array.prototype.pop.length; Array.prototype.pop.length = Infinity; Array.prototype.pop.length === x. Actual: ' + (Array.prototype.pop.length));
}
