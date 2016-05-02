// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of decodeURIComponent has the attribute ReadOnly
es5id: 15.1.3.2_A5.3
description: Checking if varying the length property fails
includes: [propertyHelper.js]
---*/

//CHECK#1
var x = decodeURIComponent.length;
verifyNotWritable(decodeURIComponent, "length", null, Infinity);
if (decodeURIComponent.length !== x) {
  $ERROR('#1: x = decodeURIComponent.length; decodeURIComponent.length = Infinity; decodeURIComponent.length === x. Actual: ' + (decodeURIComponent.length));
}
