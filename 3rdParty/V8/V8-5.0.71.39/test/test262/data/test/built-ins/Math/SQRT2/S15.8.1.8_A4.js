// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Value Property SQRT2 of the Math Object has the attribute ReadOnly
es5id: 15.8.1.8_A4
description: Checking if Math.SQRT2 property has the attribute ReadOnly
includes: [propertyHelper.js]
---*/

// CHECK#1
var x = Math.SQRT2;
verifyNotWritable(Math, "SQRT2", null, 1);
if (Math.SQRT2 !== x) {
  $ERROR('#1: Math.SQRT2 hasn\'t ReadOnly: \'x = Math.SQRT2;Math.SQRT2 = 1;Math.SQRT2 === x\'');
}
