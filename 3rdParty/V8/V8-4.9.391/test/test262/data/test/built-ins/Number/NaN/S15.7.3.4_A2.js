// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number.NaN is ReadOnly
es5id: 15.7.3.4_A2
description: Checking if varying Number.NaN fails
includes: [propertyHelper.js]
---*/

// CHECK#1
verifyNotWritable(Number, "NaN", null, 1);
if (isNaN(Number.NaN) !== true) {
  $ERROR('#1: Number.NaN = 1; Number.NaN === Not-a-Number');
}
