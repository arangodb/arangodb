// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Value Property LN2 of the Math Object has the attribute ReadOnly
es5id: 15.8.1.3_A4
description: Checking if Math.LN2 property has the attribute DontDelete
includes: [propertyHelper.js]
---*/

// CHECK#1
var x = Math.LN2;
verifyNotWritable(Math, "LN2", null, 1);
if (Math.LN2 !== x) {
  $ERROR('#1: Math.LN2 hasn\'t ReadOnly: \'x = Math.LN2;Math.LN2 = 1;Math.LN2 === x\'');
}
