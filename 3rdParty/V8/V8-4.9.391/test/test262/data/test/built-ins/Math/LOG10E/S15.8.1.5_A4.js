// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Value Property LOG10E of the Math Object has the attribute ReadOnly
es5id: 15.8.1.5_A4
description: Checking if Math.LOG10E property has the attribute ReadOnly
includes: [propertyHelper.js]
---*/

// CHECK#1
var x = Math.LOG10E;
verifyNotWritable(Math, "LOG10E", null, 1);
if (Math.LOG10E !== x) {
  $ERROR('#1: Math.LOG10E hasn\'t ReadOnly: \'x = Math.LOG10E;Math.LOG10E = 1;Math.LOG10E === x\'');
}
