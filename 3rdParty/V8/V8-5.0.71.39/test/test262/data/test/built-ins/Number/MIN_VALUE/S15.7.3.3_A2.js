// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number.MIN_VALUE is ReadOnly
es5id: 15.7.3.3_A2
description: Checking if varying Number.MIN_VALUE fails
includes: [propertyHelper.js]
---*/

// CHECK#1
var x = Number.MIN_VALUE;
verifyNotWritable(Number, "MIN_VALUE", null, 1);
if (Number.MIN_VALUE !== x) {
  $ERROR('#1: x = Number.MIN_VALUE; Number.MIN_VALUE = 1; Number.MIN_VALUE === x');
}
