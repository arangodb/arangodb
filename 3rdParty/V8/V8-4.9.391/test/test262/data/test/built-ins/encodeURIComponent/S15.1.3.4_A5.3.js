// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of encodeURIComponent has the attribute ReadOnly
es5id: 15.1.3.4_A5.3
description: Checking if varying the length property fails
includes: [propertyHelper.js]
---*/

//CHECK#1
var x = encodeURIComponent.length;
verifyNotWritable(encodeURIComponent, "length", null, Infinity);
if (encodeURIComponent.length !== x) {
  $ERROR('#1: x = encodeURIComponent.length; encodeURIComponent.length = Infinity; encodeURIComponent.length === x. Actual: ' + (encodeURIComponent.length));
}
